class OVT_ResistanceFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_CampData : Managed
{
	[NonSerialized()]
	int id;
	
	string persistentId; // Unique persistent string ID for EPF
	string name;	
	vector location;
	string owner;
	bool isPrivate = false; // Default to public for collaboration
	
	[NonSerialized()]
	ref array<ref EntityID> garrisonEntities = {};
	
	ref array<ref ResourceName> garrison = {};
}

class OVT_FOBData : Managed
{
	[NonSerialized()]
	int id;
	
	string persistentId; // Unique persistent string ID for EPF
	string name;	
	vector location;
	string owner;
	bool isPriority = false; // Priority FOB for enhanced map visibility
	
	[NonSerialized()]
	ref array<ref EntityID> garrisonEntities = {};
	
	ref array<ref ResourceName> garrison = {};
}


class OVT_VehicleUpgrades : ScriptAndConfig
{
	[Attribute()]
	ResourceName m_pBasePrefab;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehicleUpgrade> m_aUpgrades;	
}

class OVT_VehicleUpgrade : ScriptAndConfig
{
	[Attribute()]
	ResourceName m_pUpgradePrefab;
	
	[Attribute(defvalue: "100", desc: "Cost (multiplied by difficulty)")]
	int m_iCost;
}

class OVT_ResistanceFactionManager: OVT_Component
{
	//! Buffer (m) added to baseCloseRange when validating FOB deployment near any base.
	//! Shared by the deploy action, the server-side check and the map's restricted-area overlay
	static const int FOB_DEPLOY_BASE_BUFFER = 50;
	//! Enforced FOB deployment exclusion radius (m) around any radio tower
	static const int FOB_DEPLOY_TOWER_RANGE = 70;

	[Attribute()]
	ResourceName m_rPlaceablesConfigFile;
	
	ref OVT_PlaceablesConfig m_PlaceablesConfig;
	
	[Attribute()]
	ResourceName m_rBuildablesConfigFile;
	
	ref OVT_BuildablesConfig m_BuildablesConfig;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehicleUpgrades> m_aVehicleUpgrades;
	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pHiredCivilianPrefab;
	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pMobileFOBPrefab;
	
	[Attribute("", UIWidgets.Object)]
	ResourceName m_pMobileFOBDeployedPrefab;
	
	ref array<ref OVT_CampData> m_Camps;
	ref array<ref OVT_FOBData> m_FOBs;
	
	OVT_PlayerManagerComponent m_Players;
	
	protected IEntity m_TempVehicle;
	
	// FOB operation tracking
	protected IEntity m_pCurrentUndeployedFOB;
	protected IEntity m_pCurrentMobileFOB;
	protected IEntity m_pCurrentDeploymentSource;
	protected IEntity m_pCurrentDeploymentTarget;
	// The transfer component the in-flight operation subscribed to; the completion handlers must
	// unsubscribe from this exact component (its owner is the player's controller, not this manager)
	protected OVT_ContainerTransferComponent m_CurrentDeploymentTransfer;
	protected OVT_ContainerTransferComponent m_CurrentCollectionTransfer;
	protected SCR_AIGroup m_TempGroup;
	
	ref ScriptInvoker m_OnPlace = new ScriptInvoker();
	ref ScriptInvoker m_OnBuild = new ScriptInvoker();
	
	// Camp cleanup search variables
	protected ref array<EntityID> m_aCampCleanupEntities;
	protected string m_sCampCleanupId;
	
	// FOB cleanup search variables
	protected ref array<IEntity> m_aFOBCleanupEntities;
	
	static OVT_ResistanceFactionManager s_Instance;
	
	static OVT_ResistanceFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_ResistanceFactionManager.Cast(pGameMode.FindComponent(OVT_ResistanceFactionManager));
		}

		return s_Instance;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Players = OVT_Global.GetPlayers();
		
		if (SCR_Global.IsEditMode()) return;
		LoadConfigs();
	}
	
	void OVT_ResistanceFactionManager(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_Camps = new array<ref OVT_CampData>;
		m_FOBs = new array<ref OVT_FOBData>;
		m_aCampCleanupEntities = new array<EntityID>;
	}
	
	void Init(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(RegisterUpgrades, 0);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Generate a unique persistent ID for camps and FOBs
	protected string GenerateUniquePersistentId(string prefix)
	{
		string timeStamp = string.Format("%1", System.GetUnixTime());
		string randomPart = string.Format("%1", Math.RandomInt(10000, 99999));
		return string.Format("%1_%2_%3", prefix, timeStamp, randomPart);
	}
	
	protected void LoadConfigs()
	{
		Resource holder = BaseContainerTools.LoadContainer(m_rPlaceablesConfigFile);
		if (holder)		
		{
			OVT_PlaceablesConfig obj = OVT_PlaceablesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_PlaceablesConfig = obj;
			}
		}
		
		holder = BaseContainerTools.LoadContainer(m_rBuildablesConfigFile);
		if (holder)		
		{
			OVT_BuildablesConfig obj = OVT_BuildablesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_BuildablesConfig = obj;
			}
		}
	}
	
	void RegisterUpgrades()
	{
		//Register vehicle upgrade resources with the economy
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		foreach(OVT_VehicleUpgrades upgrades : m_aVehicleUpgrades)
		{
			foreach(OVT_VehicleUpgrade upgrade : upgrades.m_aUpgrades)
			{
				economy.RegisterResource(upgrade.m_pUpgradePrefab);				
			}
		}
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(SpawnGarrisons, 0);
	}	
	
	//------------------------------------------------------------------------------------------------
	//! Applies the persisted resistance state: player faction, camps and FOBs.
	//!
	//! Called from OVT_ResistanceManagerSerializer.Deserialize().
	//!
	//! SPAWNS NOTHING. Restored garrison prefab lists are replayed by SpawnGarrisons() during
	//! PostGameStart, the same path a fresh campaign start takes.
	//!
	//! NO RPC. Clients receive camps and FOBs through the manager's normal replication.
	//!
	//! IDEMPOTENT: records are matched by persistent id (or position, for saves written before ids
	//! existed) and updated in place, so re-applying the same data on a live session cannot duplicate
	//! a camp.
	//! \param[in] playerFactionKey Faction key the campaign is being fought for, may be empty.
	//! \param[in] camps Persisted camp records, may be null.
	//! \param[in] fobs Persisted FOB records, may be null.
	void ApplyPersistedResistance(string playerFactionKey, array<ref OVT_PersistedCamp> camps, array<ref OVT_PersistedFOB> fobs)
	{
		ApplyPersistedPlayerFaction(playerFactionKey);

		if (!m_Camps)
			m_Camps = new array<ref OVT_CampData>;

		if (!m_FOBs)
			m_FOBs = new array<ref OVT_FOBData>;

		if (camps)
		{
			foreach (OVT_PersistedCamp campRecord : camps)
			{
				if (!campRecord)
					continue;

				OVT_CampData camp = FindPersistedCamp(campRecord);
				if (!camp)
				{
					camp = new OVT_CampData();
					camp.persistentId = campRecord.persistentId;
					m_Camps.Insert(camp);
				}

				camp.name = campRecord.name;
				camp.location = campRecord.location;
				camp.owner = campRecord.owner;
				camp.isPrivate = campRecord.isPrivate;
				ApplyPersistedGarrison(camp.garrison, campRecord.garrison);
			}
		}

		if (fobs)
		{
			foreach (OVT_PersistedFOB fobRecord : fobs)
			{
				if (!fobRecord)
					continue;

				OVT_FOBData fob = FindPersistedFOB(fobRecord);
				if (!fob)
				{
					fob = new OVT_FOBData();
					fob.persistentId = fobRecord.persistentId;
					m_FOBs.Insert(fob);
				}

				fob.name = fobRecord.name;
				fob.location = fobRecord.location;
				fob.owner = fobRecord.owner;
				fob.isPriority = fobRecord.isPriority;
				ApplyPersistedGarrison(fob.garrison, fobRecord.garrison);
			}
		}

		// id is the array index everywhere it is used, so it is re-derived rather than stored.
		foreach (int campIndex, OVT_CampData liveCamp : m_Camps)
		{
			if (liveCamp)
				liveCamp.id = campIndex;
		}

		foreach (int fobIndex, OVT_FOBData liveFob : m_FOBs)
		{
			if (liveFob)
				liveFob.id = fobIndex;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Restores the faction the player is fighting for.
	//!
	//! Only ever applied when the save actually names one. EPF substituted a hardcoded "FIA" for an
	//! empty key, which could overwrite a perfectly valid live configuration with a guess.
	//! \param[in] playerFactionKey Saved faction key, may be empty.
	protected void ApplyPersistedPlayerFaction(string playerFactionKey)
	{
		if (playerFactionKey == "")
		{
			Print("[Overthrow] The save names no player faction - keeping the configured one", LogLevel.WARNING);
			return;
		}

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;

		Faction faction = factionManager.GetFactionByKey(playerFactionKey);
		if (!faction)
		{
			Print(string.Format("[Overthrow] Saved player faction '%1' no longer exists - keeping the configured one", playerFactionKey), LogLevel.WARNING);
			return;
		}

		config.m_sPlayerFaction = playerFactionKey;
		config.m_iPlayerFactionIndex = factionManager.GetFactionIndex(faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the live camp a save record belongs to, by persistent id or - for records written before
	//! ids existed - by position.
	//! \param[in] record The saved record.
	//! \return The live camp, or null when it has to be created.
	protected OVT_CampData FindPersistedCamp(notnull OVT_PersistedCamp record)
	{
		foreach (OVT_CampData camp : m_Camps)
		{
			if (!camp)
				continue;

			if (record.persistentId != "" && camp.persistentId == record.persistentId)
				return camp;

			if (record.persistentId == "" && camp.persistentId == "" && vector.Distance(camp.location, record.location) < 1)
				return camp;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! FOB equivalent of FindPersistedCamp().
	//! \param[in] record The saved record.
	//! \return The live FOB, or null when it has to be created.
	protected OVT_FOBData FindPersistedFOB(notnull OVT_PersistedFOB record)
	{
		foreach (OVT_FOBData fob : m_FOBs)
		{
			if (!fob)
				continue;

			if (record.persistentId != "" && fob.persistentId == record.persistentId)
				return fob;

			if (record.persistentId == "" && fob.persistentId == "" && vector.Distance(fob.location, record.location) < 1)
				return fob;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds a garrison prefab list from a save record.
	//! \param[in] target The live prefab list, may be null.
	//! \param[in] source The saved prefab list, may be null.
	protected void ApplyPersistedGarrison(array<ref ResourceName> target, array<ResourceName> source)
	{
		if (!target)
			return;

		target.Clear();

		if (!source)
			return;

		foreach (ResourceName prefab : source)
		{
			if (prefab.IsEmpty())
				continue;

			target.Insert(prefab);
		}
	}

	protected void SpawnGarrisons()
	{
		foreach(OVT_CampData fob : m_Camps)
		{
			foreach(ResourceName res : fob.garrison)
			{
				SCR_AIGroup group = SpawnGarrisonCamp(fob, res);
				if(!group)
				{
					Print(string.Format("[Overthrow] Failed to spawn camp garrison prefab '%1' - skipping", res), LogLevel.WARNING);
					continue;
				}
				fob.garrisonEntities.Insert(group.GetID());
			}
		}
		foreach(OVT_FOBData fob : m_FOBs)
		{
			foreach(ResourceName res : fob.garrison)
			{
				SCR_AIGroup group = SpawnGarrisonFOB(fob, res);
				if(!group)
				{
					Print(string.Format("[Overthrow] Failed to spawn FOB garrison prefab '%1' - skipping", res), LogLevel.WARNING);
					continue;
				}
				fob.garrisonEntities.Insert(group.GetID());
			}
		}
	}
	
	bool IsOfficer(int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return false;
		return player.isOfficer;
	}
	
	bool IsLocalPlayerOfficer()
	{
		return IsOfficer(SCR_PlayerController.GetLocalPlayerId());
	}
	
	void AddOfficer(int playerId)
	{
		// Server-only: the broadcast below is dropped when sent from a client, which would leave
		// the promotion applied on the caller's screen alone. Clients go through
		// OVT_PlayerCommsComponent.AddOfficer.
		if (!Replication.IsServer()) return;
		RpcDo_AddOfficer(playerId);
		Rpc(RpcDo_AddOfficer, playerId);
	}
	
	void DeployFOB(RplId vehicle, int playerId = -1)
	{		
		// SERVER-SIDE ONLY: FOB operations must happen on server
		if (!Replication.IsServer())
		{
			return;
		}
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		
		// Server-side validation: Check if too close to enemy bases
		vector fobPos = entity.GetOrigin();
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		// Check distance to ALL bases (occupying faction and resistance)
		foreach(OVT_BaseData base : occupyingFaction.m_Bases)
		{
			float distance = vector.Distance(base.location, fobPos);
			float restrictedDistance = config.m_Difficulty.baseCloseRange + FOB_DEPLOY_BASE_BUFFER;

			if(distance < restrictedDistance)
			{
				// Client pre-validates, but state can diverge - tell the player why nothing happened
				OVT_Global.GetNotify().SendTextNotification("TooCloseBase", playerId);
				return;
			}
		}

		// Check distance to ALL radio towers (occupying faction and resistance)
		foreach(OVT_RadioTowerData tower : occupyingFaction.m_RadioTowers)
		{
			float distance = vector.Distance(tower.location, fobPos);

			if(distance < FOB_DEPLOY_TOWER_RANGE)
			{
				OVT_Global.GetNotify().SendTextNotification("TooCloseToRadioTower", playerId);
				return;
			}
		}
		
		// Validate the initiating player and their transfer component BEFORE spawning anything -
		// falling through after the spawn leaves both vehicles (and duplicated cargo) in the world
		if (playerId == -1) return;
		OVT_OverthrowController controller = OVT_Global.GetPlayers().GetController(playerId);
		if (!controller) return;
		OVT_ContainerTransferComponent transfer = OVT_ContainerTransferComponent.Cast(controller.FindComponent(OVT_ContainerTransferComponent));
		if (!transfer || !transfer.IsAvailable()) return;

		// Only one FOB operation may be in flight - the operation state below is shared
		if (m_pCurrentDeploymentSource || m_pCurrentDeploymentTarget || m_pCurrentUndeployedFOB || m_pCurrentMobileFOB) return;

		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();

		string ownerId = vm.GetOwnerID(entity);

		vector mat[4];
		entity.GetTransform(mat);

		IEntity newveh = vm.SpawnVehicleMatrix(m_pMobileFOBDeployedPrefab, mat, ownerId);
		if (!newveh) return;

		// Clear any existing callbacks first
		transfer.m_OnOperationComplete.Remove(OnFOBCollectionComplete);
		transfer.m_OnOperationComplete.Remove(OnFOBDeploymentComplete);
		transfer.m_OnOperationError.Remove(OnFOBCollectionError);
		transfer.m_OnOperationError.Remove(OnFOBDeploymentError);

		// Store entities for cleanup after transfer
		m_pCurrentDeploymentSource = entity; // mobile FOB to be deleted
		m_pCurrentDeploymentTarget = newveh; // deployed FOB that was created
		m_CurrentDeploymentTransfer = transfer;

		// Subscribe to completion event to handle cleanup
		transfer.m_OnOperationComplete.Insert(OnFOBDeploymentComplete);
		transfer.m_OnOperationError.Insert(OnFOBDeploymentError);

		// Transfer items from mobile FOB to deployed FOB
		transfer.TransferStorage(entity, newveh, false);
	}
	
	void UndeployFOB(RplId vehicle, int playerId = -1)
	{		
		// SERVER-SIDE ONLY: FOB operations must happen on server
		if (!Replication.IsServer())
		{
			return;
		}
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		
		// Validate the initiating player and their transfer component BEFORE spawning anything -
		// falling through after the spawn leaves a duplicate truck and a still-registered FOB
		if (playerId == -1) return;
		OVT_OverthrowController controller = OVT_Global.GetPlayers().GetController(playerId);
		if (!controller) return;
		OVT_ContainerTransferComponent transfer = OVT_ContainerTransferComponent.Cast(controller.FindComponent(OVT_ContainerTransferComponent));
		if (!transfer || !transfer.IsAvailable()) return;

		// Only one FOB operation may be in flight - the operation state below is shared
		if (m_pCurrentDeploymentSource || m_pCurrentDeploymentTarget || m_pCurrentUndeployedFOB || m_pCurrentMobileFOB) return;

		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();

		string ownerId = vm.GetOwnerID(entity);

		vector mat[4];
		entity.GetTransform(mat);

		IEntity newveh = vm.SpawnVehicleMatrix(m_pMobileFOBPrefab, mat, ownerId);
		if (!newveh) return;

		// Deactivate physics immediately on the mobile FOB to prevent physics conflicts
		Physics physics = newveh.GetPhysics();
		if (physics)
		{
			physics.SetActive(ActiveState.INACTIVE);
		}

		OVT_Global.GetVehicles().m_aVehicles.RemoveItem(entity.GetID());

		// Clear any existing callbacks first
		transfer.m_OnOperationComplete.Remove(OnFOBCollectionComplete);
		transfer.m_OnOperationComplete.Remove(OnFOBDeploymentComplete);
		transfer.m_OnOperationError.Remove(OnFOBCollectionError);
		transfer.m_OnOperationError.Remove(OnFOBDeploymentError);

		// Subscribe to completion event to handle FOB cleanup
		transfer.m_OnOperationComplete.Insert(OnFOBCollectionComplete);
		transfer.m_OnOperationError.Insert(OnFOBCollectionError);

		// Store FOB entities for cleanup (using member variables)
		m_pCurrentUndeployedFOB = entity;
		m_pCurrentMobileFOB = newveh;
		m_CurrentCollectionTransfer = transfer;

		// Start container collection with the new progress system
		transfer.UndeployFOBWithCollection(entity, newveh);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set a FOB as priority (only one priority FOB allowed at a time)
	void SetPriorityFOB(IEntity fobEntity)
	{
		if (!fobEntity) return;
		
		vector fobPos = fobEntity.GetOrigin();
		
		// Find the FOB data for this entity
		OVT_FOBData targetFOB = null;
		foreach (OVT_FOBData fob : m_FOBs)
		{
			if (vector.Distance(fob.location, fobPos) < 10) // Close enough to be the same FOB
			{
				targetFOB = fob;
				break;
			}
		}
		
		if (!targetFOB) return;
		
		// Clear priority from all other FOBs
		foreach (OVT_FOBData fob : m_FOBs)
		{
			if (fob != targetFOB)
				fob.isPriority = false;
		}
		
		// Set this FOB as priority
		targetFOB.isPriority = true;
		
		// Notify clients about priority change
		Rpc(RpcDo_SetPriorityFOB, fobPos);
		
		// Notify players
		OVT_Global.GetNotify().SendTextNotification("PriorityFOBSet", -1, targetFOB.name);
	}
	
	IEntity PlaceItem(int placeableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler = true)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();

		// Guard client-supplied indices - out-of-range values are a remote VM-error vector
		if(placeableIndex < 0 || placeableIndex >= config.m_PlaceablesConfig.m_aPlaceables.Count()) return null;
		OVT_Placeable placeable = config.m_PlaceablesConfig.m_aPlaceables[placeableIndex];
		if(prefabIndex < 0 || prefabIndex >= placeable.m_aPrefabs.Count()) return null;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int cost = m_Config.GetPlaceableCost(placeable);

		// Server-side validation for player-initiated placement (-1 = server-initiated, free)
		if(playerId > -1)
		{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);

			// DoTakePlayerMoney clamps at zero, so an explicit funds check is required
			if(!economy.PlayerHasMoney(persId, cost))
			{
				OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
				return null;
			}

			// The place UI traces at most ~15m from the player - reject far-away positions
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if(playerEntity && vector.Distance(playerEntity.GetOrigin(), pos) > 50) return null;

			// Re-check item limits (the client menu check is advisory only)
			if(!placeable.m_bIgnoreLocation)
			{
				OVT_ItemLimitChecker limits = new OVT_ItemLimitChecker();
				string reason;
				if(!limits.CanPlaceItem(pos, persId, reason)) return null;
			}
		}

		ResourceName res = placeable.m_aPrefabs[prefabIndex];

		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		
		IEntity entity = OVT_Global.SpawnEntityPrefabMatrix(res, mat);
		
		// Check for OVT_PlaceableComponent and warn if missing
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		if (!placeableComp)
		{
			Print(string.Format("[Overthrow] WARNING: Placeable entity '%1' missing OVT_PlaceableComponent!", res), LogLevel.WARNING);
		}
		else
		{
			// Set ownership and association
			string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			placeableComp.SetOwnerPersistentId(playerUid);

			// Find nearest base/camp/FOB to associate with (if enabled)
			if (placeable.m_bAssociateWithNearest)
			{
				string baseId;
				EOVTBaseType baseType;
				if (FindNearestBase(pos, baseId, baseType))
				{
					placeableComp.SetAssociatedBase(baseId, baseType);
				}
			}
		}
		
		if(placeable.handler && runHandler)
		{
			if(!placeable.handler.OnPlace(entity, playerId))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				return null;
			}
		}
		
		economy.TakePlayerMoney(playerId, cost);
		
		SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		aiworld.RequestNavmeshRebuildEntity(entity);
		
		m_OnPlace.Invoke(entity, placeable, playerId);
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(entity);
		if(playerowner)
		{
			string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			playerowner.SetPlayerOwner(playerUid);
			playerowner.SetLocked(false);
		}

		// Include the placed object in save points. Placeables are spawned from prefabs Overthrow
		// mostly does not own, so they carry no native Persistence component and have to be
		// registered from script - see OVT_PersistenceTracking. Done last so an object a handler
		// rejected (deleted above) is never registered.
		OVT_PersistenceTracking.Track(entity);

		return entity;
	}
	
	IEntity BuildItem(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler = true)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();

		// Guard client-supplied indices - out-of-range values are a remote VM-error vector
		if(buildableIndex < 0 || buildableIndex >= config.m_BuildablesConfig.m_aBuildables.Count()) return null;
		OVT_Buildable buildable = config.m_BuildablesConfig.m_aBuildables[buildableIndex];
		if(prefabIndex < 0 || prefabIndex >= buildable.m_aPrefabs.Count()) return null;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int cost = m_Config.GetBuildableCost(buildable);

		// Server-side validation for player-initiated builds (-1 = server-initiated, free)
		if(playerId > -1)
		{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);

			// DoTakePlayerMoney clamps at zero, so an explicit funds check is required
			if(!economy.PlayerHasMoney(persId, cost))
			{
				OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
				return null;
			}

			// The build camera is clamped to 50m of the player - reject far-away positions
			IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if(playerEntity && vector.Distance(playerEntity.GetOrigin(), pos) > 250) return null;

			// Re-check item limits (the client menu check is advisory only)
			OVT_ItemLimitChecker limits = new OVT_ItemLimitChecker();
			string reason;
			if(!limits.CanBuildItem(pos, reason)) return null;
		}

		ResourceName res = buildable.m_aPrefabs[prefabIndex];

		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;

		IEntity entity = OVT_Global.SpawnEntityPrefabMatrix(res, mat);

		// Check for OVT_BuildableComponent and warn if missing
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (!buildableComp)
		{
			Print(string.Format("[Overthrow] WARNING: Buildable entity '%1' missing OVT_BuildableComponent!", res), LogLevel.WARNING);
		}
		else
		{
			// Set ownership and association
			string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			buildableComp.SetOwnerPersistentId(playerUid);

			// Find nearest base/camp/FOB to associate with
			string baseId;
			EOVTBaseType baseType;
			if (FindNearestBase(pos, baseId, baseType))
			{
				buildableComp.SetAssociatedBase(baseId, baseType);
			}
		}

		// A failed handler aborts the build before any charge - mirrors PlaceItem()
		if(buildable.handler && runHandler)
		{
			if(!buildable.handler.OnPlace(entity, playerId))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				return null;
			}
		}

		// Charge server-side - the client no longer pays via the generic money RPC
		economy.TakePlayerMoney(playerId, cost);

		SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		aiworld.RequestNavmeshRebuildEntity(entity);

		m_OnBuild.Invoke(entity, buildable, playerId);

		// Include the built structure in save points - see the matching call in PlaceItem().
		OVT_PersistenceTracking.Track(entity);

		return entity;
	}
	
	//! Remove a placed item from the world. Takes an RplId - EntityIDs are not valid across the network.
	void RemovePlacedItem(RplId entityId, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(entityId));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		if(!entity) return;
		
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		
		if(!placeableComp && !buildableComp) return;
		
		// Check permissions
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerUid);
		bool isOfficer = player && player.isOfficer;
		
		string ownerUid = "";
		if(placeableComp)
			ownerUid = placeableComp.GetOwnerPersistentId();
		else if(buildableComp)
			ownerUid = buildableComp.GetOwnerPersistentId();
		
		// Only allow removal if player is owner or officer
		if(ownerUid != playerUid && !isOfficer) return;
		
		// Delete the entity
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Resolves a garrison group prefab from the player faction, guarding the client-supplied index.
	protected ResourceName GetGarrisonPrefab(int prefabIndex)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetPlayerFaction();
		if(!faction) return ResourceName.Empty;
		if(prefabIndex < 0 || prefabIndex >= faction.m_aGroupPrefabSlots.Count()) return ResourceName.Empty;
		return faction.m_aGroupPrefabSlots[prefabIndex];
	}

	//------------------------------------------------------------------------------------------------
	//! Charges a player for an already-spawned garrison group. Server-side twin of the cost shown in
	//! the base/FOB menus: (recruit cost + 300 equipment placeholder) per soldier. A playerId of -1
	//! means a server-initiated (free) garrison.
	//! \return false when the player cannot afford it (the caller must delete the group).
	protected bool ChargeForGarrison(int playerId, SCR_AIGroup group)
	{
		if(playerId == -1) return true;

		int cost = (OVT_Global.GetConfig().m_Difficulty.baseRecruitCost + 300) * group.m_aUnitPrefabSlots.Count();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(!economy.PlayerHasMoney(persId, cost))
		{
			OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
			return false;
		}
		economy.DoTakePlayerMoney(playerId, cost);
		return true;
	}

	void AddGarrison(int baseId, int prefabIndex, bool takeSupporters = true, int playerId = -1)
	{
		array<ref OVT_BaseData> bases = OVT_Global.GetOccupyingFaction().m_Bases;
		if(baseId < 0 || baseId >= bases.Count()) return;
		OVT_BaseData base = bases[baseId];
		if(!base) return;

		ResourceName res = GetGarrisonPrefab(prefabIndex);
		if(res.IsEmpty()) return;

		SCR_AIGroup group = SpawnGarrison(base, res);
		if(!group) return;

		// Refuse before charging when the town cannot supply the units - TakeSupporters
		// silently no-ops when short, which used to hand out free garrisons (BUG-064)
		if(takeSupporters && !OVT_Global.GetTowns().NearestTownHasSupporters(base.location, group.m_aUnitPrefabSlots.Count()))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return;
		}

		if(!ChargeForGarrison(playerId, group))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return;
		}

		base.garrisonEntities.Insert(group.GetID());

		if(takeSupporters)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(base.location, group.m_aUnitPrefabSlots.Count());
		}
	}

	void AddGarrisonCamp(OVT_CampData fob, int prefabIndex, bool takeSupporters = true, int playerId = -1)
	{
		if(!fob) return;

		ResourceName res = GetGarrisonPrefab(prefabIndex);
		if(res.IsEmpty()) return;

		SCR_AIGroup group = SpawnGarrisonCamp(fob, res);
		if(!group) return;

		// Refuse before charging when the town cannot supply the units (BUG-064)
		if(takeSupporters && !OVT_Global.GetTowns().NearestTownHasSupporters(fob.location, group.m_aUnitPrefabSlots.Count()))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return;
		}

		if(!ChargeForGarrison(playerId, group))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return;
		}

		fob.garrisonEntities.Insert(group.GetID());

		if(takeSupporters)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(fob.location, group.m_aUnitPrefabSlots.Count());
		}
	}

	void AddGarrisonFOB(OVT_FOBData fob, int prefabIndex, bool takeSupporters = true, int playerId = -1)
	{
		if(!fob) return;

		ResourceName res = GetGarrisonPrefab(prefabIndex);
		if(res.IsEmpty()) return;

		SCR_AIGroup group = SpawnGarrisonFOB(fob, res);
		if(!group) return;

		// Refuse before charging when the town cannot supply the units (BUG-064)
		if(takeSupporters && !OVT_Global.GetTowns().NearestTownHasSupporters(fob.location, group.m_aUnitPrefabSlots.Count()))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return;
		}

		if(!ChargeForGarrison(playerId, group))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return;
		}

		fob.garrisonEntities.Insert(group.GetID());

		if(takeSupporters)
		{
			OVT_Global.GetTowns().TakeSupportersFromNearestTown(fob.location, group.m_aUnitPrefabSlots.Count());
		}
	}
	
	SCR_AIGroup SpawnGarrison(OVT_BaseData base, ResourceName res)
	{
		IEntity entity = OVT_Global.SpawnEntityPrefab(res, base.location);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if(!group) return null;
		AddPatrolWaypoints(group, base);
		return group;
	}

	SCR_AIGroup SpawnGarrisonCamp(OVT_CampData fob, ResourceName res)
	{
		IEntity entity = OVT_Global.SpawnEntityPrefab(res, fob.location + "1 0 0");
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if(!group) return null;

		AIWaypoint wp = OVT_Global.GetConfig().SpawnDefendWaypoint(fob.location);
		group.AddWaypoint(wp);

		return group;
	}

	SCR_AIGroup SpawnGarrisonFOB(OVT_FOBData fob, ResourceName res)
	{
		IEntity entity = OVT_Global.SpawnEntityPrefab(res, fob.location + "1 0 0");
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if(!group) return null;

		AIWaypoint wp = OVT_Global.GetConfig().SpawnDefendWaypoint(fob.location);
		group.AddWaypoint(wp);

		return group;
	}
	
	protected void AddPatrolWaypoints(SCR_AIGroup aigroup, OVT_BaseData base)
	{
		OVT_BaseControllerComponent controller = OVT_Global.GetOccupyingFaction().GetBase(base.entId);
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		if(controller.m_AllCloseSlots.Count() > 2)
		{
			AIWaypoint firstWP;
			for(int i=0; i< 3; i++)
			{
				IEntity randomSlot = GetGame().GetWorld().FindEntityByID(controller.m_AllCloseSlots.GetRandomElement());
				AIWaypoint wp = OVT_Global.GetConfig().SpawnPatrolWaypoint(randomSlot.GetOrigin());
				if(i==0) firstWP = wp;
				queueOfWaypoints.Insert(wp);
				
				AIWaypoint wait = OVT_Global.GetConfig().SpawnWaitWaypoint(randomSlot.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
				queueOfWaypoints.Insert(wait);
			}
			AIWaypointCycle cycle = AIWaypointCycle.Cast(OVT_Global.GetConfig().SpawnWaypoint(OVT_Global.GetConfig().m_pCycleWaypointPrefab, firstWP.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}
	}
	
	void RegisterCamp(IEntity ent, int playerId)
	{
		vector pos = ent.GetOrigin();	
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);	
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		OVT_CampData fob = new OVT_CampData;
		fob.persistentId = GenerateUniquePersistentId("CAMP");
		fob.owner = persId;
		if(player)
		{
			if(player.camp[0] != 0)
			{
				// Remove old camp using proper server method
				RemoveOldCamp(player.camp);
			}			
			player.camp = pos;
			fob.name = "#OVT-Place_Camp " + player.name;
		}
		
		fob.location = pos;
		m_Camps.Insert(fob);
				
		Rpc(RpcDo_RegisterCamp, pos, fob.name, playerId, fob.persistentId);
		OVT_Global.GetNotify().SendTextNotification("PlacedCamp",-1,OVT_Global.GetPlayers().GetPlayerName(playerId),OVT_Global.GetTowns().GetTownName(pos));
	}
	
	void RegisterFOB(IEntity ent, int playerId)
	{
		vector pos = ent.GetOrigin();	
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);	
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		OVT_FOBData fob = new OVT_FOBData;
		fob.persistentId = GenerateUniquePersistentId("FOB");
		fob.owner = persId;		
		
		fob.location = pos;
		m_FOBs.Insert(fob);
				
		Rpc(RpcDo_RegisterFOB, pos, fob.name, playerId, fob.persistentId);
		OVT_Global.GetNotify().SendTextNotification("DeployedFOB",-1,OVT_Global.GetPlayers().GetPlayerName(playerId),OVT_Global.GetTowns().GetTownName(pos));
	}

	void UnregisterFOB(vector pos)
	{
		RpcDo_RemoveFOB(pos);
		Rpc(RpcDo_RemoveFOB, pos);		
	}
	
	
	float DistanceToCamp(vector pos, string playerId)
	{
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(playerId);
		if(!player) return 99999;
		if(player.camp[0] == 0) return 99999;
		
		return vector.Distance(player.camp, pos);
	}
	
	vector GetNearestCamp(vector pos)
	{
		vector nearestBase;
		float nearest = -1;
		foreach(OVT_CampData fob : m_Camps)
		{
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob.location;
			}
		}
		return nearestBase;
	}
	
	OVT_CampData GetNearestCampData(vector pos)
	{
		OVT_CampData nearestBase;
		float nearest = -1;
		foreach(OVT_CampData fob : m_Camps)
		{
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob;
			}
		}
		return nearestBase;
	}
	
	vector GetNearestFOB(vector pos)
	{
		vector nearestBase;
		float nearest = -1;
		foreach(OVT_FOBData fob : m_FOBs)
		{
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob.location;
			}
		}
		return nearestBase;
	}
	
	OVT_FOBData GetNearestFOBData(vector pos)
	{
		OVT_FOBData nearestBase;
		float nearest = -1;
		foreach(OVT_FOBData fob : m_FOBs)
		{
			float distance = vector.Distance(fob.location, pos);
			if(nearest == -1 || distance < nearest){
				nearest = distance;
				nearestBase = fob;
			}
		}
		return nearestBase;
	}
	
	protected void MoveInGunner()
	{
		array<AIAgent> agents = {};
		m_TempGroup.GetAgents(agents);
		if(agents.Count() == 0) return;
		
		AIAgent dude = agents[0];
		IEntity ent = dude.GetControlledEntity();
		
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(ent.FindComponent(SCR_CompartmentAccessComponent));
		if(!access) return;
		
		access.MoveInVehicle(m_TempVehicle, ECompartmentType.TURRET);
	}		
	
	//RPC Methods	
	override bool RplSave(ScriptBitWriter writer)
	{				
		//Send JIP Camps
		writer.WriteInt(m_Camps.Count()); 
		for(int i=0; i<m_Camps.Count(); i++)
		{
			OVT_CampData camp = m_Camps[i];
			writer.WriteString(camp.persistentId);
			writer.WriteString(camp.name);
			writer.WriteVector(camp.location);
			writer.WriteString(camp.owner);
			writer.WriteBool(camp.isPrivate);
		}
		
		//Send JIP FOBs
		writer.WriteInt(m_FOBs.Count()); 
		for(int i=0; i<m_FOBs.Count(); i++)
		{
			OVT_FOBData fob = m_FOBs[i];
			writer.WriteString(fob.persistentId);
			writer.WriteString(fob.name);
			writer.WriteVector(fob.location);
			writer.WriteString(fob.owner);
			writer.WriteBool(fob.isPriority);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{						
		//Receive JIP Camps
		int length;
		string s;
		vector v;
		bool b;
		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{			
			OVT_CampData camp = new OVT_CampData;			
			if (!reader.ReadString(s)) return false;
			camp.persistentId = s;
			if (!reader.ReadString(s)) return false;
			camp.name = s;
			if (!reader.ReadVector(v)) return false;
			camp.location = v;
			if (!reader.ReadString(s)) return false;
			camp.owner = s;
			if (!reader.ReadBool(b)) return false;
			camp.isPrivate = b;
			m_Camps.Insert(camp);
		}
		
		//Receive JIP FOBs
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{			
			OVT_FOBData fob = new OVT_FOBData;			
			if (!reader.ReadString(s)) return false;
			fob.persistentId = s;
			if (!reader.ReadString(s)) return false;
			fob.name = s;
			if (!reader.ReadVector(v)) return false;
			fob.location = v;
			if (!reader.ReadString(s)) return false;
			fob.owner = s;
			if (!reader.ReadBool(b)) return false;
			fob.isPriority = b;
			m_FOBs.Insert(fob);
		}
		
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterCamp(vector pos, string name, int playerId, string persistentId)
	{		
		OVT_CampData fob = new OVT_CampData;
		fob.location = pos;
		fob.name = name;
		fob.persistentId = persistentId;
		m_Camps.Insert(fob);
		
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		fob.owner = persId;
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(player)
		{
			player.camp = pos;
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterFOB(vector pos, string name, int playerId, string persistentId)
	{		
		OVT_FOBData fob = new OVT_FOBData;
		fob.location = pos;
		fob.name = name;
		fob.persistentId = persistentId;
		m_FOBs.Insert(fob);

		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		fob.owner = persId;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveFOB(vector pos)
	{
		int index = -1;
		float nearestDistance = -1;
		foreach(int t, OVT_FOBData fob : m_FOBs)
		{
			// Tolerance match (like RpcDo_SetPriorityFOB): the record holds the deployed entity's
			// origin at registration time, while pos is the replacement truck's origin - physics
			// settling in between made exact equality leave permanent ghost records
			float distance = vector.Distance(fob.location, pos);
			if(distance < 10 && (nearestDistance == -1 || distance < nearestDistance))
			{
				index = t;
				nearestDistance = distance;
			}
		}
		if(index > -1)
		{
			m_FOBs.Remove(index);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetPriorityFOB(vector pos)
	{
		// Clear priority from all FOBs
		foreach (OVT_FOBData fob : m_FOBs)
		{
			fob.isPriority = false;
		}
		
		// Set priority for the FOB at this position
		foreach (OVT_FOBData fob : m_FOBs)
		{
			if (vector.Distance(fob.location, pos) < 10) // Close enough to be the same FOB
			{
				fob.isPriority = true;
				break;
			}
		}
	}
	
	void SetCampPrivacy(vector pos, bool isPrivate)
	{
		foreach(OVT_CampData camp : m_Camps)
		{
			if(camp.location == pos)
			{
				camp.isPrivate = isPrivate;
				break;
			}
		}
		Rpc(RpcDo_SetCampPrivacy, pos, isPrivate);
	}
	
	void RemoveCamp(RplId campEntityId, vector pos)
	{
		int index = -1;
		OVT_CampData campToRemove;
		foreach(int t, OVT_CampData camp : m_Camps)
		{
			if(camp.location == pos)
			{
				index = t;
				campToRemove = camp;
				break;
			}
		}
		if(index > -1)
		{
			// Clean up associated placeable and buildable objects
			CleanupCampObjects(campToRemove);
			
			// Delete the actual camp entity itself using RplId
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(campEntityId));
			if (rpl)
			{
				IEntity campEntity = rpl.GetEntity();
				if (campEntity)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(campEntity);
				}
			}
			
			m_Camps.Remove(index);
		}
		Rpc(RpcDo_RemoveCamp, pos);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RemoveCamp(vector pos)
	{		
		int index = -1;
		foreach(int t, OVT_CampData camp : m_Camps)
		{
			if(camp.location == pos)
			{
				index = t;
				break;
			}
		}
		if(index > -1)
		{
			m_Camps.Remove(index);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetCampPrivacy(vector pos, bool isPrivate)
	{		
		foreach(OVT_CampData camp : m_Camps)
		{
			if(camp.location == pos)
			{
				camp.isPrivate = isPrivate;
				break;
			}
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddOfficer(int playerId)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(IsOfficer(playerId)) return;
		
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);
		if(!player) return;
		
		player.isOfficer = true;
		if(playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NewOfficerYou", "", 10, true);
		}else{
			string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
			SCR_HintManagerComponent.GetInstance().ShowCustom(playerName + " #OVT-NewOfficer", "", 10, true);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove old camp when player places a new one (server-side only)
	protected void RemoveOldCamp(vector pos)
	{
		// Find camp data
		int index = -1;
		OVT_CampData campToRemove;
		foreach(int t, OVT_CampData camp : m_Camps)
		{
			if(camp.location == pos)
			{
				index = t;
				campToRemove = camp;
				break;
			}
		}
		
		if(index > -1)
		{
			// Clean up associated objects
			CleanupCampObjects(campToRemove);
			
			// Find and delete the camp entity using callback
			GetGame().GetWorld().QueryEntitiesBySphere(pos, 10, null, FindAndDeleteOldCamp, EQueryEntitiesFlags.ALL);
			
			// Remove from data and sync
			m_Camps.Remove(index);
			Rpc(RpcDo_RemoveCamp, pos);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback to find and delete old camp entity
	protected bool FindAndDeleteOldCamp(IEntity entity)
	{
		if (!entity) return false;
		
		ActionsManagerComponent actionsManager = ActionsManagerComponent.Cast(entity.FindComponent(ActionsManagerComponent));
		if (actionsManager)
		{
			array<BaseUserAction> actions = {};
			actionsManager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				if (OVT_ManageCampAction.Cast(action))
				{
					SCR_EntityHelper.DeleteEntityAndChildren(entity);
					return false;
				}
			}
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback function to filter entities during camp cleanup
	protected bool FilterCampCleanupEntities(IEntity entity)
	{
		if (!entity)
			return false;
			
		// Check for placeable component
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		if (placeableComp && placeableComp.BelongsTo(m_sCampCleanupId, EOVTBaseType.CAMP))
		{
			m_aCampCleanupEntities.Insert(entity.GetID());
			return false; // Continue searching
		}
		
		// Check for buildable component
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (buildableComp && buildableComp.BelongsTo(m_sCampCleanupId, EOVTBaseType.CAMP))
		{
			m_aCampCleanupEntities.Insert(entity.GetID());
		}
		
		return false; // Continue searching
	}
	
	//------------------------------------------------------------------------------------------------
	//! Clean up all placeable and buildable objects associated with a camp
	protected void CleanupCampObjects(OVT_CampData camp)
	{
		if (!camp)
			return;
			
		m_sCampCleanupId = camp.persistentId;
		m_aCampCleanupEntities.Clear();
		
		float searchRadius = 75; // Same as MAX_CAMP_PLACE_DIS from PlaceContext
		
		// Query entities around the camp location
		GetGame().GetWorld().QueryEntitiesBySphere(camp.location, searchRadius, null, FilterCampCleanupEntities, EQueryEntitiesFlags.ALL);
		
		// Delete all found entities
		foreach (EntityID entityId : m_aCampCleanupEntities)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(entityId);
			if (entity)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			}
		}
		
		m_aCampCleanupEntities.Clear();
		m_sCampCleanupId = "";
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find the nearest base/camp/FOB to a position and return its ID and type
	protected bool FindNearestBase(vector pos, out string baseId, out EOVTBaseType baseType)
	{
		float nearestDistance = -1;
		string nearestId = "";
		EOVTBaseType nearestType = EOVTBaseType.NONE;

		// Check camps
		OVT_CampData nearestCamp = GetNearestCampData(pos);
		if (nearestCamp)
		{
			float campDist = vector.Distance(nearestCamp.location, pos);
			if (nearestDistance == -1 || campDist < nearestDistance)
			{
				nearestDistance = campDist;
				nearestId = nearestCamp.persistentId;
				nearestType = EOVTBaseType.CAMP;
			}
		}

		// Check FOBs
		OVT_FOBData nearestFOB = GetNearestFOBData(pos);
		if (nearestFOB)
		{
			float fobDist = vector.Distance(nearestFOB.location, pos);
			if (nearestDistance == -1 || fobDist < nearestDistance)
			{
				nearestDistance = fobDist;
				nearestId = nearestFOB.persistentId;
				nearestType = EOVTBaseType.FOB;
			}
		}

		// Check bases using the existing method
		OVT_BaseData nearestBase = OVT_Global.GetOccupyingFaction().GetNearestBase(pos);
		if (nearestBase && !nearestBase.IsOccupyingFaction())
		{
			float baseDist = vector.Distance(nearestBase.location, pos);
			if (nearestDistance == -1 || baseDist < nearestDistance)
			{
				nearestDistance = baseDist;
				nearestId = nearestBase.id.ToString();
				nearestType = EOVTBaseType.BASE;
			}
		}

		if (nearestType != EOVTBaseType.NONE)
		{
			baseId = nearestId;
			baseType = nearestType;
			return true;
		}

		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Comprehensive cleanup of FOB area - removes all placed and built items
	//! \param centerPos Center position of FOB area
	//! \param radius Cleanup radius in meters
	void CleanupFOBArea(vector centerPos, float radius)
	{		
		// Clear and prepare the cleanup results array
		m_aFOBCleanupEntities = new array<IEntity>();
		
		// Find all entities in the FOB radius
		GetGame().GetWorld().QueryEntitiesBySphere(centerPos, radius, null, 
			FOBAreaCleanupCallback, EQueryEntitiesFlags.ALL);
		
		// Delete all found placeable/buildable items
		int deletedCount = 0;
		foreach (IEntity entity : m_aFOBCleanupEntities)
		{
			if (entity)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				deletedCount++;
			}
		}
				
		// Clear the cleanup array
		m_aFOBCleanupEntities = null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback for FOB area cleanup - identifies placeable and buildable items to remove
	//! \param entity Entity being checked
	//! \return Always returns false to continue searching
	protected bool FOBAreaCleanupCallback(IEntity entity)
	{
		if (!entity || !m_aFOBCleanupEntities) return false;
		
		// Check for placeable component (tents, equipment boxes, etc.)
		OVT_PlaceableComponent placeable = OVT_ComponentFinder<OVT_PlaceableComponent>.Find(entity);
		if (placeable)
		{
			m_aFOBCleanupEntities.Insert(entity);
			return false;
		}
		
		// Check for buildable component (guard towers, medical tents, etc.)
		OVT_BuildableComponent buildable = OVT_ComponentFinder<OVT_BuildableComponent>.Find(entity);
		if (buildable)
		{
			m_aFOBCleanupEntities.Insert(entity);
			return false;
		}
		
		// Could add more specific checks here for other types of items
		// that should be cleaned up when an FOB is undeployed
		
		return false; // Continue searching
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a display name for an entity for logging purposes
	//! \param entity Entity to get name for
	//! \return Display name or fallback
	protected string GetEntityDisplayName(IEntity entity)
	{
		if (!entity) return "Unknown";
		
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (prefabData)
		{
			ResourceName prefab = prefabData.GetPrefabName();
			if (!prefab.IsEmpty())
			{
				int lastSlash = prefab.LastIndexOf("/");
				if (lastSlash >= 0)
					return prefab.Substring(lastSlash + 1, prefab.Length() - lastSlash - 1);
				else
					return prefab;
			}
		}
		
		return "Entity";
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when FOB container collection completes successfully
	void OnFOBCollectionComplete(int itemsTransferred, int itemsSkipped)
	{
		
		if (!m_pCurrentUndeployedFOB || !m_pCurrentMobileFOB)
		{
			Print("ERROR: FOB entities are null during completion!");
			return;
		}
		
		// Clean up all placed/built items in FOB area
		vector fobPosition = m_pCurrentUndeployedFOB.GetOrigin();
		CleanupFOBArea(fobPosition, 75.0);
		
		// Delete the deployed FOB entity
		SCR_EntityHelper.DeleteEntityAndChildren(m_pCurrentUndeployedFOB);
		
		// Reactivate physics on the mobile FOB
		Physics physics = m_pCurrentMobileFOB.GetPhysics();
		if (physics)
		{
			physics.SetActive(ActiveState.ACTIVE);
		}
		
		// Send notification
		string ownerPersistentId = "";
		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();
		if (vm)
			ownerPersistentId = vm.GetOwnerID(m_pCurrentMobileFOB);
		
		if (!ownerPersistentId.IsEmpty())
		{
			int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(ownerPersistentId);
			if (playerId > 0)
			{
				OVT_Global.GetNotify().SendTextNotification("#OVT-FOBUndeployed", playerId, 
					itemsTransferred.ToString(), "3");
			}
		}
		
		// Unregister the FOB
		UnregisterFOB(m_pCurrentMobileFOB.GetOrigin());

		// Unsubscribe from the transfer component the operation actually subscribed to (it lives on
		// the player's controller, not on this manager's owner)
		if (m_CurrentCollectionTransfer)
		{
			m_CurrentCollectionTransfer.m_OnOperationComplete.Remove(OnFOBCollectionComplete);
			m_CurrentCollectionTransfer.m_OnOperationError.Remove(OnFOBCollectionError);
			m_CurrentCollectionTransfer = null;
		}

		// Clean up references
		m_pCurrentUndeployedFOB = null;
		m_pCurrentMobileFOB = null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when FOB container collection fails
	void OnFOBCollectionError(string errorMessage)
	{
		
		// Still delete deployed FOB to prevent it being stuck
		if (m_pCurrentUndeployedFOB)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_pCurrentUndeployedFOB);
		}
		
		// Send error notification
		if (m_pCurrentMobileFOB)
		{
			string ownerPersistentId = "";
			OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();
			if (vm)
				ownerPersistentId = vm.GetOwnerID(m_pCurrentMobileFOB);
			
			if (!ownerPersistentId.IsEmpty())
			{
				int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(ownerPersistentId);
				if (playerId > 0)
				{
					OVT_Global.GetNotify().SendTextNotification("#OVT-FOBUndeployFailed", playerId, 
						errorMessage);
				}
			}
		}
		
		// Unsubscribe from the transfer component the operation actually subscribed to
		if (m_CurrentCollectionTransfer)
		{
			m_CurrentCollectionTransfer.m_OnOperationComplete.Remove(OnFOBCollectionComplete);
			m_CurrentCollectionTransfer.m_OnOperationError.Remove(OnFOBCollectionError);
			m_CurrentCollectionTransfer = null;
		}

		// Clean up references
		m_pCurrentUndeployedFOB = null;
		m_pCurrentMobileFOB = null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when FOB deployment transfer completes successfully
	void OnFOBDeploymentComplete(int itemsTransferred, int itemsSkipped)
	{
		
		if (!m_pCurrentDeploymentSource || !m_pCurrentDeploymentTarget)
		{
			Print("ERROR: FOB deployment entities are null during completion!");
			return;
		}
		
		// Remove from vehicle manager and delete the mobile FOB entity after transfer
		OVT_Global.GetVehicles().m_aVehicles.RemoveItem(m_pCurrentDeploymentSource.GetID());
		SCR_EntityHelper.DeleteEntityAndChildren(m_pCurrentDeploymentSource);
		
		// Register the deployed FOB
		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();
		if (vm)
		{
			string ownerId = vm.GetOwnerID(m_pCurrentDeploymentTarget);
			int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(ownerId);
			RegisterFOB(m_pCurrentDeploymentTarget, playerId);
		}
		
		// Unsubscribe from the transfer component the operation actually subscribed to
		if (m_CurrentDeploymentTransfer)
		{
			m_CurrentDeploymentTransfer.m_OnOperationComplete.Remove(OnFOBDeploymentComplete);
			m_CurrentDeploymentTransfer.m_OnOperationError.Remove(OnFOBDeploymentError);
			m_CurrentDeploymentTransfer = null;
		}

		// Clean up references
		m_pCurrentDeploymentSource = null;
		m_pCurrentDeploymentTarget = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Called when FOB deployment transfer fails
	void OnFOBDeploymentError(string errorMessage)
	{
		Print(string.Format("FOB deployment transfer failed: %1", errorMessage), LogLevel.ERROR);
		
		// Still register the deployed FOB even if transfer failed
		if (m_pCurrentDeploymentTarget)
		{
			OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();
			if (vm)
			{
				string ownerId = vm.GetOwnerID(m_pCurrentDeploymentTarget);
				int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(ownerId);
				RegisterFOB(m_pCurrentDeploymentTarget, playerId);
			}
		}
		
		// Still delete the mobile FOB
		if (m_pCurrentDeploymentSource)
		{
			OVT_Global.GetVehicles().m_aVehicles.RemoveItem(m_pCurrentDeploymentSource.GetID());
			SCR_EntityHelper.DeleteEntityAndChildren(m_pCurrentDeploymentSource);
		}
		
		// Unsubscribe from the transfer component the operation actually subscribed to
		if (m_CurrentDeploymentTransfer)
		{
			m_CurrentDeploymentTransfer.m_OnOperationComplete.Remove(OnFOBDeploymentComplete);
			m_CurrentDeploymentTransfer.m_OnOperationError.Remove(OnFOBDeploymentError);
			m_CurrentDeploymentTransfer = null;
		}

		// Clean up references
		m_pCurrentDeploymentSource = null;
		m_pCurrentDeploymentTarget = null;
	}
	
}