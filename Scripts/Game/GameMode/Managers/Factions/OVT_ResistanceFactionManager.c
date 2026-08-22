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

	//! Footprint (m) an undeploy collects placed containers from. The shipped number.
	static const float FOB_UNDEPLOY_COLLECT_RADIUS = 75;

	//! What GetStructureCost() answers for a live structure no config entry claims.
	//!
	//! ⚠ DELIBERATELY HUGE, NOT ZERO. Its only consumer orders structures cheapest-first, and an
	//! unknown price sorting FIRST would make anything a mod adds the first thing destroyed. Sorting
	//! last means the known, authored, cheap things go first and an unpriced object is only ever
	//! reached once everything else is gone - the conservative half of the mistake either way.
	static const int UNKNOWN_STRUCTURE_COST = 1000000;

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
	// Undeploy runs on the storage job engine, not on the container transfer component: it converts
	// every nearby container into the mobile FOB's LEDGER rather than moving entities between two
	// vanilla storages, so the completion handlers listen there instead.
	protected OVT_StorageRequestComponent m_CurrentCollectionTransfer;
	// The player whose transfer component drives the in-flight FOB operation; if they disconnect
	// mid-transfer the complete/error callbacks never fire, so the state must be recovered manually
	protected int m_iFOBOperationPlayerId = -1;
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

		// If the player driving an in-flight FOB operation disconnects, the transfer callbacks on
		// their controller never fire and the operation state would wedge FOB deploy/undeploy for
		// the rest of the session - recover it here (the invoker only fires on the server)
		if (m_Players)
			m_Players.m_OnPlayerDisconnected.Insert(OnPlayerDisconnectedFOBRecovery);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves camp and FOB ownership from one persistent id to another.
	//! See OVT_PlayerManagerComponent.TryAdoptNullIdentityRecords.
	//! \param[in] oldId Persistent id the camps/FOBs are currently keyed to.
	//! \param[in] newId Persistent id they should be keyed to.
	void RekeyPlayerPersistentId(string oldId, string newId)
	{
		if (oldId == newId || newId.IsEmpty())
			return;

		if (m_Camps)
		{
			foreach (OVT_CampData camp : m_Camps)
			{
				if (camp && camp.owner == oldId)
					camp.owner = newId;
			}
		}

		if (m_FOBs)
		{
			foreach (OVT_FOBData fob : m_FOBs)
			{
				if (fob && fob.owner == oldId)
					fob.owner = newId;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Recovers the shared FOB operation state if the initiating player disconnects mid-transfer
	protected void OnPlayerDisconnectedFOBRecovery(string playerPersistentId, int playerId)
	{
		if (m_iFOBOperationPlayerId == -1 || playerId != m_iFOBOperationPlayerId) return;

		if (m_pCurrentDeploymentSource || m_pCurrentDeploymentTarget)
		{
			OnFOBDeploymentError("Player disconnected");
		}
		else if (m_pCurrentUndeployedFOB || m_pCurrentMobileFOB)
		{
			OnFOBCollectionError("Player disconnected");
		}
		m_iFOBOperationPlayerId = -1;
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
	
	//! Lifecycle hook called by OVT_OverthrowGameMode.DoStartGame(). Nothing to start here today; kept
	//! because the game mode calls it unconditionally.
	void PostGameStart()
	{
	}	
	
	//------------------------------------------------------------------------------------------------
	//! Applies the persisted resistance state: player faction, camps and FOBs.
	//!
	//! Called from OVT_ResistanceManagerSerializer.Deserialize().
	//!
	//! SPAWNS NOTHING.
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
		// OVT_ResistanceRequestComponent.AddOfficer.
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
				// Client pre-validates, but state can diverge - tell the player why nothing
				// happened. Server-initiated calls (playerId -1) would broadcast to everyone.
				if (playerId > -1)
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
				if (playerId > -1)
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
		if (m_pCurrentDeploymentSource || m_pCurrentDeploymentTarget || m_pCurrentUndeployedFOB || m_pCurrentMobileFOB)
		{
			OVT_Global.GetNotify().SendTextNotification("FOBOperationInProgress", playerId);
			return;
		}

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
		m_iFOBOperationPlayerId = playerId;

		// Subscribe to completion event to handle cleanup
		transfer.m_OnOperationComplete.Insert(OnFOBDeploymentComplete);
		transfer.m_OnOperationError.Insert(OnFOBDeploymentError);

		// The truck's LEDGER goes across first, synchronously and with zero spawns. It has to happen
		// here: OnFOBDeploymentComplete deletes the truck, and an undeploy leaves everything it
		// collected in the ledger rather than in the vanilla inventory the transfer below moves.
		OVT_StorageUtils.MoveWholeLedger(entity, newveh);

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
		
		// Validate the initiating player and their storage engine BEFORE spawning anything -
		// falling through after the spawn leaves a duplicate truck and a still-registered FOB
		if (playerId == -1) return;
		OVT_OverthrowController controller = OVT_Global.GetPlayers().GetController(playerId);
		if (!controller) return;
		OVT_StorageRequestComponent storage = OVT_StorageRequestComponent.Cast(controller.FindComponent(OVT_StorageRequestComponent));
		if (!storage || storage.IsBusy()) return;

		// Only one FOB operation may be in flight - the operation state below is shared
		if (m_pCurrentDeploymentSource || m_pCurrentDeploymentTarget || m_pCurrentUndeployedFOB || m_pCurrentMobileFOB)
		{
			OVT_Global.GetNotify().SendTextNotification("FOBOperationInProgress", playerId);
			return;
		}

		OVT_VehicleManagerComponent vm = OVT_Global.GetVehicles();

		string ownerId = vm.GetOwnerID(entity);

		vector mat[4];
		entity.GetTransform(mat);

		IEntity newveh = vm.SpawnVehicleMatrix(m_pMobileFOBPrefab, mat, ownerId);
		if (!newveh) return;

		RplId mobileId = OVT_StorageUtils.GetHolderId(newveh);
		if (!mobileId.IsValid())
		{
			SCR_EntityHelper.DeleteEntityAndChildren(newveh);
			return;
		}

		// Deactivate physics immediately on the mobile FOB to prevent physics conflicts
		Physics physics = newveh.GetPhysics();
		if (physics)
		{
			physics.SetActive(ActiveState.INACTIVE);
		}

		OVT_Global.GetVehicles().m_aVehicles.RemoveItem(entity.GetID());

		// Clear any existing callbacks first
		storage.m_OnOperationComplete.Remove(OnFOBCollectionComplete);
		storage.m_OnOperationError.Remove(OnFOBCollectionError);

		// Subscribe to completion event to handle FOB cleanup
		storage.m_OnOperationComplete.Insert(OnFOBCollectionComplete);
		storage.m_OnOperationError.Insert(OnFOBCollectionError);

		// Store FOB entities for cleanup (using member variables)
		m_pCurrentUndeployedFOB = entity;
		m_pCurrentMobileFOB = newveh;
		m_CurrentCollectionTransfer = storage;
		m_iFOBOperationPlayerId = playerId;

		// The deployed FOB and every placed container around it are converted into the mobile FOB's
		// ledger. A refusal emits nothing, so the state latched above is unwound by hand - otherwise
		// FOB operations stay wedged for the rest of the session.
		if (!storage.StartCollectionJob(playerId, vehicle, mobileId, FOB_UNDEPLOY_COLLECT_RADIUS, "#OVT-Progress-UndeployingFOB"))
			OnFOBCollectionError("#OVT-Storage_Failed");
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
		
		IEntity entity = OVT_WorldUtils.SpawnEntityPrefabMatrix(res, mat);
		
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

		// Being seen placing an illegal item (propaganda) makes you wanted - same rule as looting
		if(placeable.m_bIllegal && runHandler)
		{
			IEntity placerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if(placerEntity)
			{
				OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(placerEntity.FindComponent(OVT_PlayerWantedComponent));
				if(wanted)
					wanted.OnIllegalActionSeen("WantedIllegalPlacement");
			}
		}

		economy.TakePlayerMoney(playerId, cost);

		// Immediate, not queued: the player is standing here and expects their squad to path around
		// the thing that just appeared. The null-guarded helper also protects everything below it -
		// the inline SCR_AIWorld cast this replaced had no guard, so a null AI world would have
		// VM-errored out of PlaceItem() before ownership stamping and OVT_PersistenceTracking.Track().
		OVT_NavmeshRebuild.RebuildNow(entity);

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
	
	//------------------------------------------------------------------------------------------------
	//! Build a structure from the buildables config. THE ONE DOOR every caller already uses; its
	//! signature is unchanged.
	//!
	//! A buildable that authors resource requirements and is being built BY A PLAYER puts a
	//! construction site down instead and charges the money there (D2). Everything else - every
	//! money-only buildable, and every server-initiated build at playerId -1 - goes straight to
	//! FinishBuild() exactly as it did before construction sites existed.
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \param[in] pos Where to put it.
	//! \param[in] angles Its yaw/pitch/roll.
	//! \param[in] playerId The builder, or -1 for a server-initiated build (free of money AND of
	//! resources, and never a site).
	//! \param[in] runHandler Whether the buildable's handler runs.
	//! \return The structure, the construction site, or null.
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

		// The server half of OVT_BuildContext.CanBuild's town branch, through the same pure predicate.
		// Player-initiated only, like every check in the block above: -1 is the server's own
		// "free and unvalidated" marker. Refuses silently, as the item-limit check does - the client
		// already named the reason before it ever sent this.
		if(playerId > -1 && !TownControlAllowsBuild(buildable, pos)) return null;

		// A player building something that costs resources gets a SITE, and pays for it now (D2).
		// playerId -1 never reaches here, so a server-initiated build is never a site.
		if(playerId > -1 && HasResourceRequirements(buildable))
			return PlaceConstructionSite(buildableIndex, prefabIndex, pos, angles, playerId);

		return FinishBuild(buildableIndex, prefabIndex, pos, angles, playerId, runHandler, true);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE SPAWN/REGISTER/HANDLER/NAVMESH/INVOKE/TRACK PATH, and therefore the one ordering (D13).
	//!
	//! ⚠ THE ORDER IN HERE HAS A BUG HISTORY. A failed handler deletes the structure and returns
	//! BEFORE any charge; m_OnBuild fires exactly once, when the building appears, so XP and the
	//! tutorial land then and not when a site is placed; Track() runs last, so a rejected build is
	//! never registered. Nothing was reordered when the site path was added - the only difference
	//! between the two entries is the charge flag.
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \param[in] pos Where to put it.
	//! \param[in] angles Its yaw/pitch/roll.
	//! \param[in] playerId The builder, or -1.
	//! \param[in] runHandler Whether the buildable's handler runs.
	//! \param[in] charge Whether the money is taken here. False when a construction site already
	//! charged it at placement.
	//! \return The structure, or null when the handler rejected it.
	protected IEntity FinishBuild(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId, bool runHandler, bool charge)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();

		if(buildableIndex < 0 || buildableIndex >= config.m_BuildablesConfig.m_aBuildables.Count()) return null;
		OVT_Buildable buildable = config.m_BuildablesConfig.m_aBuildables[buildableIndex];
		if(prefabIndex < 0 || prefabIndex >= buildable.m_aPrefabs.Count()) return null;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		int cost = m_Config.GetBuildableCost(buildable);

		ResourceName res = buildable.m_aPrefabs[prefabIndex];

		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;

		IEntity entity = OVT_WorldUtils.SpawnEntityPrefabMatrix(res, mat);

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
		if(charge)
			economy.TakePlayerMoney(playerId, cost);

		// Immediate - see the matching call in PlaceItem().
		OVT_NavmeshRebuild.RebuildNow(entity);

		m_OnBuild.Invoke(entity, buildable, playerId);

		// Include the built structure in save points - see the matching call in PlaceItem().
		OVT_PersistenceTracking.Track(entity);

		// AFTER Track(), and deliberately not an m_OnBuild subscriber: an exception inside an invoker
		// aborts the whole chain, and this one would run before the line above.
		RegisterBuiltWarehouse(entity, playerId);

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a freshly built warehouse with real estate, so it is indistinguishable from a
	//! purchased one (D14): owned by the builder, public, on the map, and openable.
	//!
	//! GATED ON THE REAL-ESTATE CONFIG, not on the buildable type. SetOwnerPersistentId() also writes
	//! ownership for the OWNER manager, so calling it for every built structure would put every garage
	//! and guard tower in the player's owned-property list. Only a building the real-estate config
	//! already calls a warehouse is registered.
	//!
	//! NOTHING IN OVT_RealEstateManagerComponent CHANGES for this - the whole path is its shipped
	//! purchase path, reached with a different owner.
	//! \param[in] entity The structure that was just built.
	//! \param[in] playerId The builder, or -1 for a server-initiated build (never registered - there
	//! is no owner to register it to).
	protected void RegisterBuiltWarehouse(IEntity entity, int playerId)
	{
		if(!entity || playerId <= -1) return;

		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if(!realEstate) return;

		OVT_RealEstateConfig config = realEstate.GetConfig(entity);
		if(!config || !config.m_IsWarehouse) return;

		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if(persId == "") return;

		// isPrivate is left at its default false, so the warehouse is PUBLIC (D14).
		realEstate.SetOwnerPersistentId(persId, entity);
	}

	//------------------------------------------------------------------------------------------------
	//! The server half of OVT_BuildContext.CanBuild's town branch.
	//!
	//! Mirrors the client exactly - the same nearest town, the same size-to-range table, the same pure
	//! predicate - so a position the client offered can never be refused here for a different reason.
	//! Villages are excluded (the client refuses a town-buildable in one outright, for a different
	//! reason this has no business restating), and so is every buildable that is not town-buildable.
	//! \param[in] buildable The config entry.
	//! \param[in] pos Where it was ordered.
	//! \return True unless the position sits inside a town the resistance does not hold.
	//! Mirrors OVT_BuildContext.CanBuild's base branch so both sides of the wire agree.
	//! \param[in] pos The requested build position.
	//! \return True when a resistance-held base is close enough to authorise the build.
	protected bool BaseAllowsBuild(vector pos)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if(!occupyingFaction) return false;

		OVT_BaseData base = occupyingFaction.GetNearestBase(pos);
		if(!base || base.IsOccupyingFaction()) return false;

		return vector.Distance(base.location, pos) < OVT_Global.GetConfig().m_Difficulty.baseRange;
	}

	protected bool TownControlAllowsBuild(OVT_Buildable buildable, vector pos)
	{
		if(!buildable || !buildable.m_bBuildInTown) return true;

		// A qualifying base wins outright, exactly as OVT_BuildContext.CanBuild's base branch returns
		// before its town branch. Without this the client offers a build the server refuses in silence.
		if(buildable.m_bBuildAtBase && BaseAllowsBuild(pos)) return true;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return true;

		OVT_TownData town = towns.GetNearestTown(pos);
		if(!town || town.size == 1) return true;

		int range = towns.m_iCityRange;
		if(town.size < 3) range = towns.m_iTownRange;

		return OVT_ResourceRules.TownControlAllowsBuild(town.faction, OVT_Global.GetConfig().GetPlayerFactionIndex(), vector.DistanceSq(town.location, pos), range * range);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a construction site where a building was ordered, and takes the money for it (D2).
	//!
	//! Money is charged HERE and refunded nowhere - removing a site refunds nothing, because no refund
	//! path exists anywhere in the mod. m_OnBuild does NOT fire: a site is not a building, so no XP is
	//! awarded and no tutorial step completes until it is finished.
	//! \param[in] buildableIndex Index into the buildables config.
	//! \param[in] prefabIndex Index into that buildable's prefab list.
	//! \param[in] pos Where the building was ordered.
	//! \param[in] angles The orientation it will be built at.
	//! \param[in] playerId The builder. Never -1 - BuildItem() gates on that.
	//! \return The site, or null when nothing was placed and nothing was charged.
	protected IEntity PlaceConstructionSite(int buildableIndex, int prefabIndex, vector pos, vector angles, int playerId)
	{
		OVT_ResistanceFactionManager config = OVT_Global.GetResistanceFaction();

		if(buildableIndex < 0 || buildableIndex >= config.m_BuildablesConfig.m_aBuildables.Count()) return null;
		OVT_Buildable buildable = config.m_BuildablesConfig.m_aBuildables[buildableIndex];
		if(prefabIndex < 0 || prefabIndex >= buildable.m_aPrefabs.Count()) return null;

		ResourceName sitePrefab = ResolveSitePrefab(buildable);
		if(sitePrefab == ResourceName.Empty)
		{
			Print("[Overthrow] A buildable authors resource requirements but there is no construction site prefab to place - neither the buildable's own m_SitePrefab nor the resource manager's generic one is wired. Nothing was built and nothing was charged.", LogLevel.ERROR);
			return null;
		}

		vector mat[4];
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;

		IEntity site = OVT_WorldUtils.SpawnEntityPrefabMatrix(sitePrefab, mat);
		if(!site) return null;

		OVT_ConstructionSiteComponent siteComp = OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(site);
		if(!siteComp)
		{
			Print(string.Format("[Overthrow] The construction site prefab '%1' carries no OVT_ConstructionSiteComponent, so it could never be finished. Nothing was built and nothing was charged.", sitePrefab), LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(site);
			return null;
		}

		siteComp.Initialize(buildableIndex, prefabIndex, angles, buildable.m_sTitle);

		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);

		// Ownership and base association are stamped here as well as in FinishBuild(), so the shipped
		// removal flow (owner-or-officer) works on a site the moment it exists.
		OVT_BuildableComponent buildableComp = OVT_ComponentFinder<OVT_BuildableComponent>.Find(site);
		if(buildableComp)
		{
			buildableComp.SetOwnerPersistentId(playerUid);

			string baseId;
			EOVTBaseType baseType;
			if(FindNearestBase(pos, baseId, baseType))
				buildableComp.SetAssociatedBase(baseId, baseType);
		}

		// The same figure BuildItem() checked the player could afford, in the same frame.
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		economy.TakePlayerMoney(playerId, m_Config.GetBuildableCost(buildable));

		OVT_NavmeshRebuild.RebuildNow(site);

		OVT_PersistenceTracking.Track(site);

		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER ONLY. Consumes the crate piles around a construction site and turns it into the building.
	//!
	//! The money was taken at placement (D2), so this re-enters FinishBuild() with charge false. The
	//! resources are taken BEFORE the site is destroyed and the building spawned, in the order §3.8
	//! specifies; Consume() is all-or-nothing, so a refusal leaves every pile exactly as it was.
	//! \param[in] site The construction site.
	//! \param[in] playerId The player finishing it. Credited with the build.
	//! \param[out] reason Localization key naming the refusal; "" on success.
	//! \return The finished building, or null.
	IEntity CompleteSite(IEntity site, int playerId, out string reason)
	{
		reason = "";

		if(!Replication.IsServer())
		{
			reason = "#OVT-Resource_Failed";
			return null;
		}

		OVT_ConstructionSiteComponent siteComp = OVT_ComponentFinder<OVT_ConstructionSiteComponent>.Find(site);
		if(!siteComp)
		{
			reason = "#OVT-Resource_NoSite";
			return null;
		}

		int buildableIndex = siteComp.GetBuildableIndex();
		int prefabIndex = siteComp.GetPrefabIndex();
		vector angles = siteComp.GetAngles();
		vector pos = site.GetOrigin();

		OVT_Buildable buildable = GetBuildableAt(buildableIndex);
		if(!buildable || prefabIndex < 0 || prefabIndex >= buildable.m_aPrefabs.Count())
		{
			// The site outlived a buildables.conf edit that moved or removed its entry.
			reason = "#OVT-Resource_NoSite";
			return null;
		}

		// A .conf entry that authors no requirements leaves the array NULL, and a standing site can
		// outlive the edit that emptied it - in which case the building now costs nothing and the site
		// completes for free, which is what the config says.
		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		if(buildable.m_aResourceRequirements)
			OVT_ResourceRequirements.ScaleForDifficulty(buildable.m_aResourceRequirements, need);

		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		OVT_ResourceRequirements.NearbyAvailability(pos, need, have);

		string shortId;
		if(!OVT_ResourceRules.IsSatisfied(need, have, shortId))
		{
			reason = "#OVT-Resource_NotEnough";
			return null;
		}

		if(!OVT_ResourceRequirements.Consume(pos, need))
		{
			reason = "#OVT-Resource_NotEnough";
			return null;
		}

		// Released before the delete so the save is not left holding a record for a site that no
		// longer exists; the building FinishBuild() spawns is tracked in its own right.
		OVT_PersistenceTracking.Untrack(site, false);
		DestroyPlacedItem(site);

		IEntity built = FinishBuild(buildableIndex, prefabIndex, pos, angles, playerId, true, false);
		if(!built)
		{
			reason = "#OVT-Resource_Failed";
			return null;
		}

		return built;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] buildable The config entry.
	//! \return True when it authors at least one non-empty resource requirement.
	bool HasResourceRequirements(OVT_Buildable buildable)
	{
		if(!buildable || !buildable.m_aResourceRequirements) return false;

		foreach(OVT_BuildableResourceRequirement requirement : buildable.m_aResourceRequirements)
		{
			if(requirement && requirement.m_sResourceId != "" && requirement.m_iQuantity > 0) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] buildableIndex Index into the buildables config.
	//! \return The entry, or null when the index is out of range.
	OVT_Buildable GetBuildableAt(int buildableIndex)
	{
		if(!m_BuildablesConfig || !m_BuildablesConfig.m_aBuildables) return null;
		if(buildableIndex < 0 || buildableIndex >= m_BuildablesConfig.m_aBuildables.Count()) return null;

		return m_BuildablesConfig.m_aBuildables[buildableIndex];
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] buildable The config entry.
	//! \return Its own site prefab, else the resource manager's generic one, else empty.
	protected ResourceName ResolveSitePrefab(OVT_Buildable buildable)
	{
		if(buildable && buildable.m_SitePrefab != ResourceName.Empty) return buildable.m_SitePrefab;

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if(!resources) return ResourceName.Empty;

		return resources.GetDefaultSitePrefab();
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

		DestroyPlacedItem(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE WAY A PLACED OR BUILT STRUCTURE LEAVES THE WORLD. Two lines, and their ORDER is the
	//! whole of it.
	//!
	//! ⚠ MECHANISM, NOT AUTHORIZATION. This method asks nobody's permission and is not reachable from
	//! any RPC. RemovePlacedItem() above is the PLAYER's door and keeps its owner-or-officer check
	//! exactly where it is; this is the door the server itself uses (base sabotage, camp teardown, FOB
	//! area cleanup), where there is no player to check. Adding a caller means deciding, at the CALL
	//! SITE, who is allowed to ask - do not "fix" this by giving it a playerId.
	//!
	//! ⚠ QUEUE BEFORE DELETE, NEVER AFTER, AND NEVER RebuildNow(). OVT_NavmeshRebuild.Queue() measures
	//! the entity's bounds at CALL TIME and issues the rebuild a second later, so the capture happens
	//! while the object still stands and the rebuild happens once it is gone. Reverse the two and there
	//! is nothing left to measure: the carve stays in the navmesh forever and the AI keeps refusing to
	//! walk through ground that is now empty. That is the whole reason this pair is a method rather
	//! than two lines each caller writes for itself - it used to be copied four times in this file.
	//! \param[in] entity The structure to remove. Null is a no-op.
	void DestroyPlacedItem(IEntity entity)
	{
		if(!entity) return;

		OVT_NavmeshRebuild.Queue(entity);
		DeleteComposition(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ SCR_EntityHelper.DeleteEntityAndChildren IS A MISNOMER. Its whole body is
	//! RplComponent.DeleteRplEntity(entity, false) (SCR_EntityHelper.c:177), which takes the ROOT out
	//! of replication and out of the world and leaves prefab-authored hierarchy children standing.
	//! Every composition structure therefore left its props behind: a finished construction site kept
	//! the Site_*.et scaffolding (planks, cargo containers) sitting inside the new building.
	//!
	//! Direct children only, each deleted through its own subtree - collecting the whole tree and
	//! deleting deepest-first would hand back handles already freed by an ancestor's delete.
	//! \param[in] root The structure to remove.
	protected void DeleteComposition(notnull IEntity root)
	{
		array<IEntity> children = new array<IEntity>();

		IEntity child = root.GetChildren();
		while(child)
		{
			children.Insert(child);
			child = child.GetSibling();
		}

		foreach(IEntity c : children)
		{
			if(!c) continue;

			// A character is never part of a structure's composition, and deleting one could be a player.
			if(ChimeraCharacter.Cast(c)) continue;

			// A replicated child must leave through replication; a plain prop has no RplComponent at
			// all, and DeleteRplEntity does nothing for it.
			if(RplComponent.Cast(c.FindComponent(RplComponent)))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(c);
				continue;
			}

			delete c;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(root);
	}

	//------------------------------------------------------------------------------------------------
	//! THE COST JOIN: the only place in the tree that gets from a LIVE structure back to the config
	//! entry it was built from, and therefore to a price.
	//!
	//! ⚠ IT JOINS ON THE PREFAB, NOT ON THE TYPE STRING, and that is a finding rather than a
	//! preference. OVT_PlaceableComponent.GetPlaceableType() / OVT_BuildableComponent.GetBuildableType()
	//! are authored on the prefab and DO NOT MATCH the config's m_sName for seven of the eight shipped
	//! buildables ("GuardTower" vs "Guard Tower", "Bunker" vs "Bunkers", "VehicleGarage" vs "Garage");
	//! only "Helipad" happens to line up. A join on the type string would silently price most of the
	//! game's structures at nothing. The prefab resource name is exact, needs no data to be re-authored,
	//! and is what BuildItem() spawned the thing from in the first place.
	//! \param[in] entity The live structure.
	//! \return The buildables-config entry that claims its prefab, or null.
	OVT_Buildable FindBuildableForEntity(IEntity entity)
	{
		if(!entity) return null;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(entity);
		if(prefab == ResourceName.Empty) return null;

		if(!m_BuildablesConfig || !m_BuildablesConfig.m_aBuildables) return null;

		foreach(OVT_Buildable buildable : m_BuildablesConfig.m_aBuildables)
		{
			if(!buildable || !buildable.m_aPrefabs) continue;
			if(buildable.m_aPrefabs.Contains(prefab)) return buildable;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The authored price of a live structure, buildable or placeable.
	//!
	//! ⚠ THE RAW AUTHORED m_iCost, NOT the difficulty-multiplied one. Sabotage orders structures by
	//! price and the multiplier is uniform, so applying it would change nothing except to add a config
	//! dependency to a pure lookup. A caller that wants what a PLAYER would pay must ask
	//! OVT_OverthrowConfigComponent.GetPlaceableCost/GetBuildableCost, or GetRepairCost() below.
	//!
	//! The buildable half of the join lives in FindBuildableForEntity() above - see its header for why
	//! the join is on the prefab and never on the type string.
	//! \param[in] entity The live structure.
	//! \return Its authored cost, or UNKNOWN_STRUCTURE_COST when no config entry claims its prefab.
	int GetStructureCost(IEntity entity)
	{
		if(!entity) return UNKNOWN_STRUCTURE_COST;

		OVT_Buildable buildable = FindBuildableForEntity(entity);
		if(buildable) return buildable.m_iCost;

		ResourceName prefab = OVT_PrefabUtils.GetPrefabName(entity);
		if(prefab == ResourceName.Empty) return UNKNOWN_STRUCTURE_COST;

		if(m_PlaceablesConfig && m_PlaceablesConfig.m_aPlaceables)
		{
			foreach(OVT_Placeable placeable : m_PlaceablesConfig.m_aPlaceables)
			{
				if(!placeable || !placeable.m_aPrefabs) continue;
				if(placeable.m_aPrefabs.Contains(prefab)) return placeable.m_iCost;
			}
		}

		return UNKNOWN_STRUCTURE_COST;
	}

	//------------------------------------------------------------------------------------------------
	//! What a player pays to put this ruined structure back.
	//!
	//! One expression, evaluated on both machines: the client draws this in the action label and greys
	//! the action out with it, and the server re-derives it here before taking the money. Only
	//! BUILDABLES are repairable - a placeable has no ruined phase to come back from - so an entity the
	//! buildables config does not claim is refused rather than priced.
	//! \param[in] entity The structure (the root; its destruction component may sit on a child).
	//! \return Dollars owed, or -1 when this structure cannot be priced for repair at all.
	int GetRepairCost(IEntity entity)
	{
		OVT_Buildable buildable = FindBuildableForEntity(entity);
		if(!buildable) return -1;

		if(!OVT_RepairPricing.IsRepairable(buildable.m_iCost)) return -1;

		OVT_OverthrowConfigComponent config = m_Config;
		if(!config) config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return -1;

		return OVT_RepairPricing.RepairCost(buildable.m_iCost, config.m_Difficulty.buildableCostMultiplier, config.m_Difficulty.repairCostMultiplier);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: repair a ruined structure, charging the asking player.
	//!
	//! CHARGE AFTER PERFORMING, NEVER BEFORE - the shape RpcAsk_RearmVehicle uses: check
	//! PlayerHasMoney, do the thing, then TakePlayerMoney. DoTakePlayerMoney clamps at zero, so the
	//! explicit funds check is mandatory rather than defensive.
	//!
	//! playerId == -1 MEANS SERVER-INITIATED AND FREE, the convention BuildItem() already uses. That is
	//! how the occupying faction's repair module and the admin command repair without a wallet.
	//! \param[in] entity The ruined structure.
	//! \param[in] playerId The paying player, or -1 for a free server-initiated repair.
	//! \return True when the structure was repaired.
	bool RepairStructure(IEntity entity, int playerId)
	{
		if(!entity) return false;
		if(!Replication.IsServer()) return false;

		if(!OVT_StructureDamage.IsRuined(entity)) return false;

		int cost = 0;
		if(playerId > -1)
		{
			cost = GetRepairCost(entity);
			if(cost < 0) return false;

			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if(!economy) return false;

			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			if(!economy.PlayerHasMoney(persId, cost))
			{
				OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId);
				return false;
			}

			if(!OVT_StructureDamage.Repair(entity)) return false;

			economy.TakePlayerMoney(playerId, cost);
			OVT_Global.GetNotify().SendTextNotification("RepairedStructure", playerId);

			return true;
		}

		return OVT_StructureDamage.Repair(entity);
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
				
		Rpc(RpcDo_RegisterCamp, pos, fob.name, fob.persistentId, persId, fob.isPrivate);
		OVT_Global.GetNotify().SendTextNotification("PlacedCamp",-1,OVT_Global.GetPlayers().GetPlayerName(playerId),OVT_Global.GetTowns().GetTownName(pos));
	}
	
	void RegisterFOB(IEntity ent, int playerId)
	{
		vector pos = ent.GetOrigin();

		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(persId);

		// Re-deploying where a record already lies (e.g. one orphaned by a lost FOB entity) must
		// reuse that record. Two records inside the 10 m wire-matching tolerance are
		// indistinguishable to every position-keyed RPC, draw two overlapping map icons, and the
		// loser of an undeploy's nearest-match removal becomes a permanent orphan marker (BUG-129).
		// Clients already hold the surviving record, so nothing is broadcast.
		foreach (OVT_FOBData existing : m_FOBs)
		{
			if (vector.Distance(existing.location, pos) < 10)
			{
				existing.owner = persId;
				OVT_Global.GetNotify().SendTextNotification("DeployedFOB",-1,OVT_Global.GetPlayers().GetPlayerName(playerId),OVT_Global.GetTowns().GetTownName(pos));
				return;
			}
		}

		OVT_FOBData fob = new OVT_FOBData;
		fob.persistentId = GenerateUniquePersistentId("FOB");
		fob.owner = persId;

		fob.location = pos;
		m_FOBs.Insert(fob);

		Rpc(RpcDo_RegisterFOB, pos, fob.name, persId, fob.persistentId);
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
	
	// The owner arrives as the persistent id string resolved ONCE on the server, exactly as the JIP
	// stream sends it. Re-deriving it here from the runtime playerId raced the player-id table's own
	// replication and could permanently record owner "" - which the map's private-camp filter then
	// hid from everyone, the owner included (BUG-177)
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterCamp(vector pos, string name, string persistentId, string ownerPersistentId, bool isPrivate)
	{
		OVT_CampData fob = new OVT_CampData;
		fob.location = pos;
		fob.name = name;
		fob.persistentId = persistentId;
		fob.owner = ownerPersistentId;
		fob.isPrivate = isPrivate;
		m_Camps.Insert(fob);

		OVT_PlayerData player = OVT_Global.GetPlayers().GetPlayer(ownerPersistentId);
		if(player)
		{
			player.camp = pos;
		}
	}

	// The owner arrives as the persistent id string resolved ONCE on the server, exactly as
	// RpcDo_RegisterCamp receives it - re-deriving it here from the runtime playerId raced the
	// player-id table's own replication and could permanently record owner "" (the BUG-177 defect,
	// left unfixed on the FOB path until BUG-192)
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterFOB(vector pos, string name, string ownerPersistentId, string persistentId)
	{
		OVT_FOBData fob = new OVT_FOBData;
		fob.location = pos;
		fob.name = name;
		fob.persistentId = persistentId;
		fob.owner = ownerPersistentId;
		m_FOBs.Insert(fob);
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
					DestroyPlacedItem(campEntity);
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
				// One call per object, so a whole camp's worth of carves is measured while the objects
				// still stand and re-issued as one merged batch once they are gone.
				DestroyPlacedItem(entity);
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
				DestroyPlacedItem(entity);
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

		// The deployed FOB is consumed by the undeploy - retire its registration before deleting
		// it, or its ghost gets rebuilt on the owner's next connect (BUG-129)
		OVT_Global.GetVehicles().UnregisterVehicle(m_pCurrentUndeployedFOB);
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
				OVT_Global.GetNotify().SendTextNotification("FOBUndeployed", playerId, 
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
		m_iFOBOperationPlayerId = -1;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when FOB container collection fails
	void OnFOBCollectionError(string errorMessage)
	{
		
		// Still delete deployed FOB to prevent it being stuck - retiring its registration too (BUG-129)
		if (m_pCurrentUndeployedFOB)
		{
			OVT_Global.GetVehicles().UnregisterVehicle(m_pCurrentUndeployedFOB);
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
					OVT_Global.GetNotify().SendTextNotification("FOBUndeployFailed", playerId,
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
		m_iFOBOperationPlayerId = -1;
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
		
		// The truck is consumed by the deploy - retire its registration BEFORE deleting it, or the
		// owner's next connect rebuilds it from the ghost registration at its last recorded
		// position, typically the shop it was bought at (BUG-129)
		OVT_Global.GetVehicles().UnregisterVehicle(m_pCurrentDeploymentSource);
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
		m_iFOBOperationPlayerId = -1;
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
		
		// Still delete the mobile FOB - and retire its registration with it (BUG-129)
		if (m_pCurrentDeploymentSource)
		{
			OVT_Global.GetVehicles().UnregisterVehicle(m_pCurrentDeploymentSource);
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
		m_iFOBOperationPlayerId = -1;
	}
	
}