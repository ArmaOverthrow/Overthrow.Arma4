class OVT_VehicleManagerComponentClass: OVT_RplOwnerManagerComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Callback class for vehicle upgrade transfers
class OVT_VehicleUpgradeCallback : OVT_StorageProgressCallback
{
	protected OVT_VehicleManagerComponent m_Component;
	
	void OVT_VehicleUpgradeCallback(OVT_VehicleManagerComponent component)
	{
		m_Component = component;
	}
	
	override void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation)
	{
		// No progress tracking needed for vehicle upgrades
	}
	
	override void OnComplete(int itemsTransferred, int itemsSkipped)
	{
		if (m_Component)
			m_Component.OnUpgradeTransferComplete(itemsTransferred, itemsSkipped);
	}
	
	override void OnError(string errorMessage)
	{
		if (m_Component)
			m_Component.OnUpgradeTransferError(errorMessage);
	}
};

class OVT_VehicleManagerComponent: OVT_RplOwnerManagerComponent
{	

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Players starting cars", params: "et", category: "Vehicles")]
	ref array<ResourceName> m_pStartingCarPrefabs;
	
	[Attribute()]
	ref SCR_EntityCatalogMultiList m_CivilianVehicleEntityCatalog;
		
	ref array<EntityID> m_aAllVehicleShops;	
	
	ref array<ref EntityID> m_aVehicles;
	
	OVT_RealEstateManagerComponent m_RealEstate;
		
	static OVT_VehicleManagerComponent s_Instance;	
	
	protected ref array<EntityID> m_aParkingSearch;
	protected ref array<EntityID> m_aFoundPlayerVehicles;
	protected bool m_bSpotBlockedByVehicle;
	
	// Vehicle upgrade tracking
	protected IEntity m_pUpgradeOldVehicle;
	protected ref OVT_VehicleUpgradeCallback m_UpgradeCallback;
	
	// Player vehicle despawn/respawn system for performance
	protected ref map<string, ref array<string>> m_mPlayerVehicleIds; // Player UID -> Vehicle persistent IDs
	protected ref map<string, float> m_mOfflinePlayerTimers; // Player UID -> Timer
	protected ref map<string, EntityID> m_mSpawnedVehicles; // Vehicle persistent ID -> Entity ID

	//------------------------------------------------------------------------------------------------
	//! Vehicle persistent id -> everything needed to put that vehicle back WITHOUT its stored record.
	//!
	//! WHY THIS EXISTS. Until 2026-08-04 the ownership registry above lived only in memory, so a server
	//! restart forgot which vehicles belonged to whom. A vehicle despawned because its owner was offline
	//! (ReleaseVehicle, 60 s after they leave) is not in the world when the save is taken, so it cannot
	//! come back by itself either - nothing knew its id to ask for it, and a player's car was simply
	//! gone after every restart. This map is what the save carries.
	//!
	//! IT IS ALSO THE FALLBACK. Asking for the stored record is still the preferred path and brings back
	//! fuel, damage and cargo. When the record cannot be resolved, prefab + transform + owner + lock is
	//! enough to rebuild the vehicle where it was parked, losing its contents but not the vehicle. That
	//! is legitimate precisely BECAUSE this record exists: Overthrow wrote down that this player owned
	//! this prefab at this spot, so rebuilding is restoring a fact, not minting a car from nothing.
	//!
	//! Shares OVT_PersistedPlayerVehicle with the serializer rather than duplicating the shape - there is
	//! no other live representation of a vehicle registration for it to disagree with.
	protected ref map<string, ref OVT_PersistedPlayerVehicle> m_mVehicleRecords;

	//! Despawn time for locked vehicles when player is offline (10 minutes)
	static const float OFFLINE_VEHICLE_DESPAWN_TIME = 60.0;

	//! Vehicle ids whose stored instance is being fetched from the persistence system right now.
	//!
	//! PersistenceSystem.RequestSpawn() is ASYNCHRONOUS, so between the request and its callback a
	//! vehicle has neither an instance in the world nor a request that anything can see. Without this
	//! list the "already in world" guard in RespawnPlayerVehicles() would let a second call start a
	//! second request for the same vehicle. Same role, and the same ordering rules, as
	//! OVT_RecruitManagerComponent.m_aPendingBodySpawns.
	protected ref array<string> m_aPendingVehicleSpawns;

	//! The persistence collection player vehicles are stored in, resolved once and cached.
	//!
	//! Held without `ref` on purpose: the collection is owned by the persistence system and is only
	//! obtainable from it (PersistenceCollection is sealed with a private constructor). This mirrors
	//! SCR_SpawnLogic.m_CharacterCollection exactly.
	protected PersistenceCollection m_VehicleCollection;

	//! Why the last respawn attempt did not produce a vehicle. Empty after a clean one, which is what
	//! makes it usable as a single "did that work?" question - the same contract as
	//! OVT_PersistenceManagerComponent.GetLastReapplyDiagnostic().
	protected string m_sLastRespawnDiagnostic;

	//! Name of the collection player vehicles are stored in.
	//!
	//! VERIFIED, not assumed. Overthrow overrides vanilla's Prefabs/Vehicles/Core/Wheeled_Base.et and
	//! Helicopter_Base.et, which are exactly the prefabs vanilla's Car {64C6B4937723DA61} and Helicopter
	//! {64EE8D74EB8192BA} configurations match (Configuration/Vehicle/Car.conf:2-6, Helicopter.conf).
	//! Vanilla Common.conf:189-197 puts both configurations - and the Tracked one - in collection
	//! {64C6B493421C8948}, whose Name is "Vehicle" (Common.conf:11-14). Overthrow's Vehicles group
	//! overrides the same two GUIDs only to append OVT_PlayerOwnerComponentSerializer; it changes
	//! neither the collection nor SelfSpawn.
	static const string VEHICLE_COLLECTION = "Vehicle";

	//! How long to wait for a spawn request to answer before giving up on it (ms).
	static const int VEHICLE_SPAWN_TIMEOUT_MS = 15000;

	//! Drift beyond which InitialVehicleCleanup() puts a restored vehicle back at its recorded pose.
	//! Generous enough that suspension settle on flat ground never trips either bound; a fall off a
	//! ramp or a depenetration kick is metres and tens of degrees away from both.
	static const float POSE_DRIFT_TOLERANCE_M = 0.5;
	static const float POSE_DRIFT_TOLERANCE_DEG = 5;

	static OVT_VehicleManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_VehicleManagerComponent.Cast(pGameMode.FindComponent(OVT_VehicleManagerComponent));
		}

		return s_Instance;
	}
	
	void OVT_VehicleManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{		
		m_aAllVehicleShops = new array<EntityID>;		
		m_aVehicles = new array<ref EntityID>;
		
		// Initialize vehicle despawn/respawn system
		m_mPlayerVehicleIds = new map<string, ref array<string>>();
		m_mOfflinePlayerTimers = new map<string, float>();
		m_mSpawnedVehicles = new map<string, EntityID>();
		m_aFoundPlayerVehicles = new array<EntityID>();
		m_aPendingVehicleSpawns = new array<string>();
		m_mVehicleRecords = new map<string, ref OVT_PersistedPlayerVehicle>();
	}
	
	void Init(IEntity owner)
	{			
		m_RealEstate = OVT_Global.GetRealEstate();
		
		if(!Replication.IsServer())
			return;
		
		// Connect to player events for vehicle management
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (playerManager)
		{
			playerManager.m_OnPlayerConnected.Insert(OnPlayerConnected);
			playerManager.m_OnPlayerDisconnected.Insert(OnPlayerDisconnected);
		}
		
		// Start offline timer processing
		GetGame().GetCallqueue().CallLater(ProcessOfflineTimers, 1000, true);
		
		// Delayed initialization to despawn offline player vehicles, after persistence has restored the world
		GetGame().GetCallqueue().CallLater(InitialVehicleCleanup, 5000, false);
	}
	
	void SpawnStartingCar(IEntity home, string playerId)
	{		
		vector mat[4];
		
		int i = s_AIRandomGenerator.RandInt(0, m_pStartingCarPrefabs.Count());
		ResourceName prefab = m_pStartingCarPrefabs[i];
		
		//Find us a parking spot
		IEntity veh;
		
		if(GetParkingSpot(home, mat, OVT_ParkingType.PARKING_CAR, true))
		{
			
			veh = SpawnVehicleMatrix(prefab, mat, playerId);
			
		}else if(FindNearestKerbParking(home.GetOrigin(), 20, mat))
		{
			Print("Unable to find OVT_ParkingComponent in starting house prefab. Trying to spawn car next to a kerb.");
			veh = SpawnVehicleMatrix(prefab, mat, playerId);
			
		}else{
			Print("Failure to spawn player's starting car. Add OVT_ParkingComponent to all starting house prefabs in config");			
		}
		
		if(veh)
		{
			OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(veh);
			if(playerowner) playerowner.SetLocked(true);
		}
	}
	
	bool GetParkingSpot(IEntity building, out vector outMat[4], OVT_ParkingType type = OVT_ParkingType.PARKING_CAR, bool skipObstructionCheck = false)
	{
		OVT_ParkingComponent parking = OVT_ParkingComponent.Cast(building.FindComponent(OVT_ParkingComponent));
		if(!parking) return false;
		return parking.GetParkingSpot(outMat, type, skipObstructionCheck);
	}
	
	bool GetNearestParkingSpot(vector pos, out vector outMat[4], OVT_ParkingType type = OVT_ParkingType.PARKING_CAR)
	{
		m_aParkingSearch = new array<EntityID>();
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FilterParkingAddToArray, EQueryEntitiesFlags.ALL);
		
		if(m_aParkingSearch.Count() == 0) return false;
		
		return GetParkingSpot(GetGame().GetWorld().FindEntityByID(m_aParkingSearch[0]), outMat, type);
	}
	
	bool FindNearestKerbParking(vector pos, float range, out vector outMat[4])
	{
		// Allocate rather than Clear: this can be the first search of the session
		// (ambient vehicle placement), when GetNearestParkingSpot has never run
		m_aParkingSearch = new array<EntityID>();
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, null, FilterKerbAddToArray, EQueryEntitiesFlags.STATIC);
		
		if(m_aParkingSearch.Count() == 0) return false;
		
		float nearestDistance = range;
		IEntity nearest;
		
		foreach(EntityID id : m_aParkingSearch)
		{
			IEntity kerb = GetGame().GetWorld().FindEntityByID(id);
			float distance = vector.Distance(kerb.GetOrigin(), pos);
			if(distance < nearestDistance)
			{
				nearest = kerb;
				nearestDistance = distance;
			}
		}
		
		if(!nearest) return false;
		
		vector mat[4];
		
		nearest.GetTransform(mat);
			
		mat[3] = mat[3] + (mat[2] * 3);
		
		vector p = mat[3];
	
		vector angles = Math3D.MatrixToAngles(mat);
		angles[0] = angles[0] - 90;
		Math3D.AnglesToMatrix(angles, outMat);
		outMat[3] = p;
		
		return true;
		
	}
	
	bool FilterKerbAddToArray(IEntity entity)
	{
		if(entity.ClassName() == "StaticModelEntity"){
			VObject mesh = entity.GetVObject();
			
			if(mesh){
				string res = mesh.GetResourceName();
				if(res.IndexOf("Pavement_") > -1) m_aParkingSearch.Insert(entity.GetID());
				if(res.IndexOf("Kerb_") > -1) m_aParkingSearch.Insert(entity.GetID());				
			}
		}
		return false;
	}
	
	bool FilterParkingAddToArray(IEntity entity)
	{
		if(entity.FindComponent(OVT_ParkingComponent)){
			m_aParkingSearch.Insert(entity.GetID());
		}
		return false;
	}
	
	IEntity SpawnVehicle(ResourceName prefab, vector pos,  int ownerId = 0, float rotation = 0)
	{
		
	}
	
	IEntity SpawnVehicleNearestParking(ResourceName prefab, vector pos,  string ownerId = "")
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_ParkingType parkingType = OVT_ParkingType.PARKING_CAR;
		
		int id = economy.GetInventoryId(prefab);
		if(id > -1)
		{
			parkingType = economy.GetParkingType(id);
		}
		
		vector mat[4];		
		if(!GetNearestParkingSpot(pos, mat, parkingType))
		{
			if(!FindNearestKerbParking(pos, 30, mat))
			{				
				return null;
			}
		}
		return SpawnVehicleMatrix(prefab, mat, ownerId);
	}
	
	IEntity SpawnVehicleBehind(ResourceName prefab, IEntity entity, string ownerId="")
	{
		vector mat[4];
			
		entity.GetTransform(mat);
		mat[3] = mat[3] + (mat[2] * -5);
		vector pos = mat[3];
			
		vector angles = Math3D.MatrixToAngles(mat);
		angles[0] = angles[0] - 90;
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = pos;
		
		return SpawnVehicleMatrix(prefab, mat, ownerId);
	}
	
	IEntity SpawnVehicleMatrix(ResourceName prefab, vector mat[4], string ownerId = "")
	{		
		IEntity ent = OVT_WorldUtils.SpawnEntityPrefabMatrix(prefab, mat);
		if(!ent)
		{
			Print("Failure to spawn vehicle");
			return null;
		}
				
		if(ownerId != "") 
		{
			SetOwnerPersistentId(ownerId, ent);
			OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(ent);
			if(playerowner)
			{
				playerowner.SetPlayerOwner(ownerId);
			}
			
			// Register vehicle for despawn/respawn management
			RegisterPlayerVehicle(ownerId, ent);
		}
		
		m_aVehicles.Insert(ent.GetID());
		
		return ent;
	}
	
	void UpgradeVehicle(RplId vehicle, int id)
	{
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		
		ResourceName res = economy.GetResource(id);
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		
		string ownerId = GetOwnerID(entity);
		
		vector mat[4];
		entity.GetTransform(mat);
		
		IEntity newveh = SpawnVehicleMatrix(res, mat, ownerId);
		RplComponent newrpl = RplComponent.Cast(newveh.FindComponent(RplComponent));
		
		m_aVehicles.RemoveItem(entity.GetID());
		
		// Store old vehicle for deletion after transfer completes
		m_pUpgradeOldVehicle = entity;
		
		// Transfer storage from old vehicle to new vehicle using inventory manager
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
			false,      // skipWeaponsOnGround
			false,      // deleteEmptyContainers
			50,         // itemsPerBatch
			100,        // batchDelayMs (normal delay to prevent crashes)
			-1,         // searchRadius (not needed for direct transfer)
			1           // maxBatchesPerFrame (normal batching)
		);
		
		OVT_Global.GetInventory().TransferStorageByRplId(rpl.Id(), newrpl.Id(), config, GetUpgradeCallback());		
	}
	
	void RepairVehicle(RplId vehicle)
	{					
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(vehicle));
		if(!rpl) return;
		IEntity entity = rpl.GetEntity();
		
		SCR_VehicleDamageManagerComponent dmg = SCR_VehicleDamageManagerComponent.Cast(entity.FindComponent(SCR_VehicleDamageManagerComponent));
		if(dmg)
		{
			dmg.FullHeal();
			dmg.SetHealthScaled(dmg.GetMaxHealth());
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get or create the upgrade callback instance
	protected OVT_VehicleUpgradeCallback GetUpgradeCallback()
	{
		if (!m_UpgradeCallback)
			m_UpgradeCallback = new OVT_VehicleUpgradeCallback(this);
		return m_UpgradeCallback;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when vehicle upgrade transfer completes successfully
	void OnUpgradeTransferComplete(int itemsTransferred, int itemsSkipped)
	{
		if (m_pUpgradeOldVehicle)
		{
			UnregisterVehicle(m_pUpgradeOldVehicle);
			SCR_EntityHelper.DeleteEntityAndChildren(m_pUpgradeOldVehicle);
			m_pUpgradeOldVehicle = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called when vehicle upgrade transfer fails
	void OnUpgradeTransferError(string errorMessage)
	{
		Print(string.Format("Vehicle upgrade transfer failed: %1", errorMessage), LogLevel.ERROR);

		// Still delete old vehicle to prevent it being stuck
		if (m_pUpgradeOldVehicle)
		{
			UnregisterVehicle(m_pUpgradeOldVehicle);
			SCR_EntityHelper.DeleteEntityAndChildren(m_pUpgradeOldVehicle);
			m_pUpgradeOldVehicle = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Retires a vehicle that game logic is about to deliberately delete (FOB deploy/undeploy prefab
	//! swaps, vehicle upgrades). Call BEFORE SCR_EntityHelper.DeleteEntityAndChildren.
	//!
	//! WHY THIS MUST HAPPEN (BUG-129). A registration that outlives its deliberately-deleted vehicle
	//! is a ghost: the owner's next connect asks storage for the id, is answered NOT_FOUND (the
	//! stored record died with the entity), and RebuildVehicleFromRecord builds the vehicle AGAIN at
	//! the registration's last captured transform - which for a truck consumed by an FOB deploy is
	//! the shop parking spot it was bought at. The rebuilt dynamic body can then stand
	//! interpenetrating the static deployed FOB after a restart and wreck it.
	//!
	//! Removes the id from EVERY player's list, not just the record's owner: an ownership change
	//! registers a vehicle under its new owner without removing the old entry, so one id can appear
	//! in two lists.
	//! \param[in] vehicle The registered vehicle about to be deleted.
	void UnregisterVehicle(IEntity vehicle)
	{
		if (!vehicle)
			return;

		m_aVehicles.RemoveItem(vehicle.GetID());

		string persistentId = OVT_PersistenceTracking.GetPersistentId(vehicle);
		if (persistentId.IsEmpty())
			return;

		m_mSpawnedVehicles.Remove(persistentId);
		m_mVehicleRecords.Remove(persistentId);

		for (int i = 0; i < m_mPlayerVehicleIds.Count(); i++)
		{
			array<string> vehicleIds = m_mPlayerVehicleIds.GetElement(i);
			if (!vehicleIds)
				continue;
			int index = vehicleIds.Find(persistentId);
			if (index != -1)
				vehicleIds.Remove(index);
		}

		Print(string.Format("[Overthrow] Unregistered consumed vehicle %1", persistentId));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a vehicle is the mobile FOB truck or the deployed FOB.
	//!
	//! An FOB is shared resistance infrastructure, not personal property: it must remain in the
	//! world always, so every vehicle-lifecycle decision (offline reservation, registry rebuild)
	//! excludes it (BUG-129).
	//! \param[in] vehicle The vehicle to test.
	bool IsMobileFOB(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		return IsMobileFOBPrefab(OVT_Global.GetPrefabName(vehicle));
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab-name form of IsMobileFOB, for registry records whose instance no longer exists.
	//! \param[in] prefab The prefab resource to test.
	bool IsMobileFOBPrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return false;
		return prefab == resistance.m_pMobileFOBPrefab || IsDeployedFOBPrefab(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a vehicle is a DEPLOYED FOB specifically - not the drivable truck it came from.
	//!
	//! Deploying spawns the deployed prefab through SpawnVehicleMatrix carrying the truck's owner
	//! (OVT_ResistanceFactionManager.DeployFOB), so a deployed FOB is a registered owned vehicle and
	//! anything enumerating a player's vehicles will find it. It is a structure at that point: it has
	//! its own OVT_FOBData record and its own map marker, and the vehicle it used to be is gone. The
	//! narrower test exists because the two halves are not interchangeable - the undeployed truck IS
	//! an ordinary owned vehicle and must keep being treated as one.
	//! \param[in] vehicle The vehicle to test.
	bool IsDeployedFOB(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		return IsDeployedFOBPrefab(OVT_Global.GetPrefabName(vehicle));
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab-name form of IsDeployedFOB, for registry records whose instance no longer exists.
	//! \param[in] prefab The prefab resource to test.
	bool IsDeployedFOBPrefab(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (!resistance)
			return false;
		return prefab == resistance.m_pMobileFOBDeployedPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Register a player-owned vehicle for despawn/respawn management
	void RegisterPlayerVehicle(string playerUid, IEntity vehicle)
	{
		if (!vehicle || playerUid.IsEmpty())
			return;

		// Identity comes from the vanilla persistence system, which tracks vehicles through the
		// Car/Helicopter configs Overthrow.conf inherits. An untracked vehicle has no id and cannot be
		// managed - the same guard the old persistence component's id used to provide.
		string persistentId = OVT_PersistenceTracking.GetPersistentId(vehicle);
		if (persistentId.IsEmpty())
		{
			Print("[Overthrow] Vehicle is not tracked by persistence, cannot register for management");
			return;
		}

		// Register vehicle with player
		if (!m_mPlayerVehicleIds.Contains(playerUid))
			m_mPlayerVehicleIds[playerUid] = new array<string>();
		
		array<string> playerVehicles = m_mPlayerVehicleIds[playerUid];
		if (playerVehicles.Find(persistentId) == -1)
			playerVehicles.Insert(persistentId);
		
		// Track spawned vehicle
		m_mSpawnedVehicles[persistentId] = vehicle.GetID();

		// ...and write down everything needed to rebuild it if its stored record is ever unavailable.
		CaptureVehicleRecord(playerUid, persistentId, vehicle);
		
		// Subscribe to ownership changes for future registration updates
		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
			vehicle.FindComponent(OVT_PlayerOwnerComponent)
		);
		if (ownerComp)
		{
			ownerComp.GetOnOwnerChange().Insert(OnVehicleOwnershipChanged);
		}
		
		Print(string.Format("[Overthrow] Registered player vehicle %1 for %2", persistentId, playerUid));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle vehicle ownership changes - register vehicle for management
	void OnVehicleOwnershipChanged(OVT_PlayerOwnerComponent ownerComp)
	{
		if (!ownerComp)
			return;
		
		IEntity vehicle = ownerComp.GetOwner();
		string newOwnerUid = ownerComp.GetPlayerOwnerUid();
		
		if (!vehicle || newOwnerUid.IsEmpty())
			return;
		
		// Register vehicle under new owner
		RegisterPlayerVehicle(newOwnerUid, vehicle);
		
		Print(string.Format("[Overthrow] Vehicle ownership changed, registered for player: %1", newOwnerUid));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle player connection - respawn their paused vehicles
	protected void OnPlayerConnected(string playerPersistentId, int playerId)
	{
		// Cancel offline timer
		if (m_mOfflinePlayerTimers.Contains(playerPersistentId))
		{
			m_mOfflinePlayerTimers.Remove(playerPersistentId);
			Print(string.Format("[Overthrow] Player %1 reconnected, cancelled vehicle despawn timer", playerPersistentId));
		}
		
		// Respawn player's paused vehicles
		RespawnPlayerVehicles(playerPersistentId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle player disconnection - start offline timer for locked vehicles
	protected void OnPlayerDisconnected(string playerPersistentId, int playerId)
	{
		// Only start timer if player has locked vehicles
		if (HasLockedVehicles(playerPersistentId))
		{
			m_mOfflinePlayerTimers[playerPersistentId] = OFFLINE_VEHICLE_DESPAWN_TIME;
			Print(string.Format("[Overthrow] Player %1 disconnected, starting vehicle despawn timer", playerPersistentId));
		}

		// The despawn itself writes each vehicle's record before releasing it, so nothing extra is
		// needed here.
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if player has any locked vehicles
	protected bool HasLockedVehicles(string playerPersistentId)
	{
		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return false;
		
		array<string> vehicleIds = m_mPlayerVehicleIds[playerPersistentId];
		foreach (string vehicleId : vehicleIds)
		{
			IEntity vehicle = FindVehicleEntity(vehicleId);
			// FOBs are never reserved (BUG-129), so a player whose only locked vehicle is an FOB
			// needs no timer
			if (vehicle && IsVehicleLocked(vehicle) && !IsMobileFOB(vehicle))
				return true;
		}

		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if a vehicle is locked
	protected bool IsVehicleLocked(IEntity vehicle)
	{
		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
			vehicle.FindComponent(OVT_PlayerOwnerComponent)
		);
		
		return ownerComp && ownerComp.IsLocked();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Process offline timers for vehicle despawning
	protected void ProcessOfflineTimers()
	{
		if (m_mOfflinePlayerTimers.IsEmpty())
			return;
		
		array<string> toRemove = {};
		
		foreach (string playerPersistentId, float timer : m_mOfflinePlayerTimers)
		{
			timer -= 1.0;
			
			if (timer <= 0)
			{
				Print(string.Format("[Overthrow] Offline timer expired for player %1, despawning locked vehicles", playerPersistentId));
				DespawnPlayerLockedVehicles(playerPersistentId);
				toRemove.Insert(playerPersistentId);
			}
			else
			{
				m_mOfflinePlayerTimers[playerPersistentId] = timer;
			}
		}
		
		// Remove expired timers
		foreach (string playerPersistentId : toRemove)
		{
			m_mOfflinePlayerTimers.Remove(playerPersistentId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes an offline player's locked vehicles out of play without taking them out of the world.
	//!
	//! LOCK STATE IS THE GATE, AND IT IS AN OWNERSHIP RULE, NOT A PERFORMANCE ONE. An UNLOCKED vehicle
	//! belongs to the whole resistance and must stay usable while its owner is offline, so it is never
	//! touched here. Only a LOCKED vehicle - one its owner has explicitly reserved to themselves - is
	//! hidden. Lock state cannot change while the owner is offline (only the owner may lock or unlock),
	//! so this decision stays correct for the whole absence and needs no re-evaluation.
	//!
	//! PUBLIC because it is one half of a lifecycle whose other half (RespawnPlayerVehicles) is public
	//! too, and because the automated round-trip case has to be able to drive the disconnect flow's own
	//! method rather than a re-implementation of it.
	//! \param[in] playerPersistentId The player who has been offline long enough.
	void DespawnPlayerLockedVehicles(string playerPersistentId)
	{
		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return;

		array<string> vehicleIds = m_mPlayerVehicleIds[playerPersistentId];
		int despawnedCount = 0;

		foreach (string vehicleId : vehicleIds)
		{
			IEntity vehicle = FindVehicleEntity(vehicleId);
			if (!vehicle)
				continue;

			// Only hide locked vehicles - an unlocked one is public property
			if (!IsVehicleLocked(vehicle))
				continue;

			// An FOB stays in the world whatever its lock state - it is the resistance's shared
			// infrastructure, and hiding it reads as "my base vanished" to every other player (BUG-129)
			if (IsMobileFOB(vehicle))
				continue;

			if (ReserveVehicle(vehicleId, vehicle))
				despawnedCount++;
		}

		Print(string.Format("[Overthrow] Reserved %1 locked vehicles for offline player: %2", despawnedCount, playerPersistentId));
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a vehicle's record and hides it in place, leaving it alive and tracked.
	//!
	//! WHAT THIS USED TO DO, AND WHY IT COULD NOT WORK (BUG-086). It was Save() +
	//! StopTracking(removeData: false) + delete - vanilla's "despawn but do not forget" idiom - and the
	//! vehicle was asked back by id when its owner returned. The kept record is not durable: measured on
	//! a dedicated server 2026-08-05, a vehicle released at a disconnect was already unresolvable ten
	//! minutes later, in the same session, with no restart. The owner got the registry REBUILD instead,
	//! which is where "contents lost" came from. Cargo, fuel and damage now survive by construction,
	//! because the vehicle is never destroyed.
	//!
	//! THE RECORD IS STILL REFRESHED FIRST. OVT_PersistedPlayerVehicle (prefab, transform, owner, lock)
	//! is Overthrow's own last-resort description of the vehicle and is what rebuilds it if its stored
	//! record is ever genuinely unreadable. Reserving does not move the vehicle, so the transform
	//! captured here stays true for as long as the reservation stands.
	//!
	//! EVERY REGISTRY ENTRY IS KEPT, including m_mSpawnedVehicles: the instance is still there, so the
	//! map that points at it is still correct. That is what lets RespawnPlayerVehicles() recognise a
	//! reserved vehicle and simply un-hide it instead of asking storage for one.
	//! \param[in] vehicleId The vehicle's persistent id.
	//! \param[in] vehicle The instance to write and hide.
	//! \return True when the vehicle was hidden by this call; false when it already was.
	protected bool ReserveVehicle(string vehicleId, notnull IEntity vehicle)
	{
		if (OVT_PersistenceReservation.IsReserved(vehicle))
			return false;

		string ownerUid;
		if (m_mVehicleRecords.Contains(vehicleId) && m_mVehicleRecords[vehicleId])
			ownerUid = m_mVehicleRecords[vehicleId].ownerUid;

		CaptureVehicleRecord(ownerUid, vehicleId, vehicle);

		// Not a hand-off to storage any more, just insurance: it makes the record on disk match the
		// vehicle as it was parked even if the server dies before the next save point.
		OVT_PersistenceTracking.Save(vehicle);

		return OVT_PersistenceReservation.Reserve(vehicle);
	}

	
	//------------------------------------------------------------------------------------------------
	//! Writes the stored record for every managed vehicle of a player, without taking a save point.
	protected void SavePlayerVehicles(string playerPersistentId)
	{
		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return;

		array<string> vehicleIds = m_mPlayerVehicleIds[playerPersistentId];
		int savedCount = 0;

		foreach (string vehicleId : vehicleIds)
		{
			IEntity vehicle = FindVehicleEntity(vehicleId);
			if (!vehicle)
				continue;

			if (OVT_PersistenceTracking.Save(vehicle))
				savedCount++;
		}

		if (savedCount > 0)
			Print(string.Format("[Overthrow] Saved %1 vehicles for player: %2", savedCount, playerPersistentId));
	}

	//------------------------------------------------------------------------------------------------
	//! Brings a returning player's despawned vehicles back into the world. GitHub #143.
	//!
	//! WHEN A LOCKED VEHICLE EXISTS IS AN OWNER-PRESENCE QUESTION. It is saved and released
	//! OFFLINE_VEHICLE_DESPAWN_TIME after its owner leaves (DespawnPlayerLockedVehicles) and comes back
	//! when that owner returns - this is called from OnPlayerConnected. A player who reconnects must find
	//! their car where they parked it, with its fuel, damage and cargo, not an empty driveway.
	//!
	//! THE CLAIM THIS METHOD USED TO CARRY, AND WHY IT WAS WRONG. It was deliberately empty, on the
	//! grounds that vanilla persistence is save-point based and cannot round-trip a single instance
	//! inside a running session. That is not what the API does: PersistenceSystem.Save() writes ONE
	//! instance's record immediately ("data is transient until the storage is flushed, but the entity can
	//! safely be deleted already after this"), and PersistenceSystem.RequestSpawn() fetches records back
	//! by id - "any already available instances will instantly complete the callback with OK". Save +
	//! StopTracking(keepData) + RequestSpawn is therefore a complete in-session round trip, and it is the
	//! same one OVT_RecruitManagerComponent uses for recruit bodies.
	//!
	//! IT ALSO COVERS VEHICLES THAT WERE NEVER DESPAWNED IN THIS SESSION. A registered id with no live
	//! instance is asked for, whatever removed the instance; a vehicle that is still standing there is
	//! skipped.
	//!
	//! IDEMPOTENT: a vehicle already in the world, or one with a request in flight, is never asked for
	//! twice, so calling this repeatedly cannot mint duplicates.
	//!
	//! PUBLIC so the owner-return hook and the automated round-trip case drive the same entry point.
	//! \param[in] playerPersistentId The returning owner.
	void RespawnPlayerVehicles(string playerPersistentId)
	{
		m_sLastRespawnDiagnostic = "";

		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return;

		// Iterate a COPY. A request whose record is already in memory completes its callback from INSIDE
		// RequestSpawn(), and that callback can drop a registration - which would invalidate this
		// iteration if it walked the live array.
		array<string> registered = m_mPlayerVehicleIds[playerPersistentId];
		array<string> vehicleIds = {};
		foreach (string registeredId : registered)
		{
			vehicleIds.Insert(registeredId);
		}

		foreach (string vehicleId : vehicleIds)
		{
			IEntity live = FindVehicleEntity(vehicleId);
			if (live)
			{
				// THE NORMAL PATH SINCE BUG-086: the vehicle was never destroyed, only hidden, so
				// "bringing it back" is un-hiding it. Nothing is read, nothing is rebuilt, and its
				// cargo, fuel and damage are whatever they were when its owner parked it.
				if (OVT_PersistenceReservation.IsReserved(live))
				{
					OVT_PersistenceReservation.Release(live);
					Print("[Overthrow] Vehicle " + vehicleId + " was reserved - released it back to " + playerPersistentId + ", contents intact");
				}

				continue;
			}

			// A request for this vehicle has not answered yet
			if (m_aPendingVehicleSpawns && m_aPendingVehicleSpawns.Find(vehicleId) != -1)
			{
				Print("[Overthrow] Vehicle " + vehicleId + " already has a spawn request in flight");
				continue;
			}

			RequestPersistedVehicle(playerPersistentId, vehicleId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the persistence system to spawn back one stored vehicle by id.
	//!
	//! Vanilla's own model is SCR_SpawnLogic.c:368-374, which fetches a returning player's character from
	//! its collection the same way. The request is filtered to a single id, so the callback is invoked
	//! once.
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] vehicleId The registered persistent id to fetch.
	//! \return True when a request was sent and the callback now owns the outcome.
	protected bool RequestPersistedVehicle(string playerPersistentId, string vehicleId)
	{
		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			m_sLastRespawnDiagnostic = "there is no persistence system in this world, so no stored vehicle can be fetched";
			return false;
		}

		// Asking a system that is still loading would answer UNAVAILABLE; vanilla guards the same way
		// (SCR_SpawnLogic.c:302-307).
		if (persistence.GetState() != EPersistenceSystemState.ACTIVE)
		{
			m_sLastRespawnDiagnostic = "the persistence system is not active yet, so vehicle " + vehicleId + " cannot be fetched";
			return false;
		}

		PersistenceCollection collection = GetVehicleCollection(persistence);
		if (!collection)
		{
			m_sLastRespawnDiagnostic = "the loaded configuration has no '" + VEHICLE_COLLECTION + "' persistence collection";
			return false;
		}

		// Vanilla's own validation of a stored id before using it (SCR_VoiceoverSystemSerializer.c:90).
		// A null UUID still stringifies to a zero-filled value, so never compare, always ask.
		if (!UUID.IsUUID(vehicleId))
		{
			m_sLastRespawnDiagnostic = "registered vehicle id '" + vehicleId + "' is not an id the persistence system can resolve - dropping the registration";
			Print("[Overthrow] " + m_sLastRespawnDiagnostic, LogLevel.WARNING);
			DropVehicleRegistration(playerPersistentId, vehicleId);
			return false;
		}

		UUID storedId = vehicleId;

		// SAME FAST PATH AS THE PLAYER BODY, and for the same reason: the vehicle configuration
		// self-spawns, so after a restart the instance is usually already in the world and RequestSpawn
		// has nothing left to spawn - it answers NOT_FOUND. Vehicles normally dodge this because
		// InitialVehicleCleanup() releases an offline owner's vehicle back to storage 5 s after init, but
		// an owner who reconnects inside that window would otherwise be told NOT_FOUND and handed a
		// REBUILT duplicate standing next to their real car.
		//
		// FindVehicleEntity() cannot answer this: it reads m_mSpawnedVehicles, which is rebuilt from the
		// save without entity ids and therefore knows nothing about a self-spawned instance.
		Managed liveInstance = persistence.FindById(storedId);
		IEntity liveVehicle = IEntity.Cast(liveInstance);
		if (liveVehicle)
		{
			Print("[Overthrow] Vehicle " + vehicleId + " is already in the world - adopting it instead of requesting a spawn");
			AdoptRespawnedVehicle(playerPersistentId, vehicleId, liveVehicle);
			m_sLastRespawnDiagnostic = "";
			return true;
		}

		if (!m_aPendingVehicleSpawns)
			m_aPendingVehicleSpawns = new array<string>();

		// MUST be marked pending BEFORE the request is sent: an instance the system already has in memory
		// completes the callback IMMEDIATELY, i.e. from inside RequestSpawn(), and the callback's first
		// act is to consume this entry.
		if (m_aPendingVehicleSpawns.Find(vehicleId) == -1)
			m_aPendingVehicleSpawns.Insert(vehicleId);

		Print("[Overthrow] Requesting stored vehicle " + vehicleId + " for " + playerPersistentId);

		PersistenceSpawnRequest request();
		request.Collection = collection;
		request.Include = {storedId};

		Tuple2<string, string> spawnContext(vehicleId, playerPersistentId);
		PersistenceResultCallback callback(OnPlayerVehicleSpawned, spawnContext);
		persistence.RequestSpawn(request, callback);

		// Insurance only, and a no-op once the callback has answered. The engine documents the callback as
		// always firing, but a vehicle that never heard back would otherwise stay pending for the rest of
		// the session and never be asked for again.
		GetGame().GetCallqueue().CallLater(OnPlayerVehicleSpawnTimeout, VEHICLE_SPAWN_TIMEOUT_MS, false, vehicleId, playerPersistentId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! PersistenceResultCallback delegate for RequestPersistedVehicle().
	//!
	//! Arity must match PersistenceResultDelegate exactly (the model is SCR_SpawnLogic.c:378).
	//! \param[in] statusCode OK when the stored vehicle was found and instantiated.
	//! \param[in] result The spawned instance on OK; on failure it is the id that could not be fetched.
	//! \param[in] isLast True on the final result of the request (always, for a single-id request).
	//! \param[in] context Tuple2 of vehicle persistent id and owner persistent id.
	protected void OnPlayerVehicleSpawned(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		Tuple2<string, string> spawnContext = Tuple2<string, string>.Cast(context);
		if (!spawnContext)
			return;

		string vehicleId = spawnContext.param1;
		string playerPersistentId = spawnContext.param2;

		if (m_aPendingVehicleSpawns)
		{
			int pendingIndex = m_aPendingVehicleSpawns.Find(vehicleId);
			if (pendingIndex != -1)
				m_aPendingVehicleSpawns.Remove(pendingIndex);
		}

		IEntity vehicle;
		if (statusCode == EPersistenceStatusCode.OK)
			vehicle = IEntity.Cast(result);

		if (!vehicle)
		{
			// NOT_FOUND: the record is gone and re-asking will never change that. Measured on a
			// dedicated server 2026-08-04, this is the NORMAL answer after a restart for a vehicle that
			// was despawned while its owner was offline, so it is the case that has to be handled well
			// rather than merely logged.
			//
			// The registration carries prefab, position and lock state, so the vehicle is REBUILT there
			// - see RebuildVehicleFromRecord for why that is restoring a recorded fact and not minting a
			// car. Only if there is no usable registration either is the id finally dropped.
			//
			// ONLY this code drops the registration. Every other failure (BUSY, UNAVAILABLE,
			// READ_ERROR, ...) says something about THIS attempt, not about the record - and the
			// registration is what the next save carries, so dropping it on a transient error would
			// make the loss permanent while the vehicle's stored record still exists. Keep it and let
			// the next owner return ask again, exactly as the timeout path does.
			if (statusCode == EPersistenceStatusCode.NOT_FOUND)
			{
				if (RebuildVehicleFromRecord(playerPersistentId, vehicleId))
				{
					m_sLastRespawnDiagnostic = "";
					return;
				}

				m_sLastRespawnDiagnostic = string.Format("persistence answered %1 for vehicle %2 and no registration was left to rebuild it from - the registration was dropped",
					typename.EnumToString(EPersistenceStatusCode, statusCode), vehicleId);
				Print("[Overthrow] " + m_sLastRespawnDiagnostic, LogLevel.WARNING);
				DropVehicleRegistration(playerPersistentId, vehicleId);
				return;
			}

			m_sLastRespawnDiagnostic = string.Format("persistence answered %1 for vehicle %2 - the registration was kept for a later retry",
				typename.EnumToString(EPersistenceStatusCode, statusCode), vehicleId);
			Print("[Overthrow] " + m_sLastRespawnDiagnostic, LogLevel.WARNING);
			return;
		}

		// A late or duplicate answer that would be a SECOND instance for the same id. Never build one and
		// never delete either - one of them is the player's vehicle and this code cannot tell which.
		IEntity existing = FindVehicleEntity(vehicleId);
		if (existing && existing != vehicle)
		{
			m_sLastRespawnDiagnostic = "vehicle " + vehicleId + " was answered twice - the second instance was left alone";
			Print("[Overthrow] " + m_sLastRespawnDiagnostic, LogLevel.ERROR);
			return;
		}

		// The owner may have left again while the request was in flight. Putting a vehicle in play for
		// somebody who is not there is precisely what the offline despawn undoes, so undo it now - but
		// adopt it first, or the registries would still point at the instance that no longer exists.
		if (!IsPlayerOnline(playerPersistentId))
		{
			Print("[Overthrow] Owner of vehicle " + vehicleId + " left before it arrived - reserving it again");
			AdoptRespawnedVehicle(playerPersistentId, vehicleId, vehicle);
			ReserveVehicle(vehicleId, vehicle);
			return;
		}

		AdoptRespawnedVehicle(playerPersistentId, vehicleId, vehicle);

		m_sLastRespawnDiagnostic = "";

		Print("[Overthrow] Vehicle " + vehicleId + " restored for " + playerPersistentId + ", contents intact");
	}

	//------------------------------------------------------------------------------------------------
	//! Gives up on a spawn request that never answered.
	//!
	//! No-op in the normal case: the callback has already cleared the pending entry by the time this runs.
	//! The REGISTRATION IS KEPT - an unanswered request is not evidence that the record is gone, and the
	//! next owner return simply asks again.
	//! \param[in] vehicleId The vehicle whose request was sent.
	//! \param[in] playerPersistentId The owner the request was made for.
	protected void OnPlayerVehicleSpawnTimeout(string vehicleId, string playerPersistentId)
	{
		if (!m_aPendingVehicleSpawns)
			return;

		int pendingIndex = m_aPendingVehicleSpawns.Find(vehicleId);
		if (pendingIndex == -1)
			return;

		m_aPendingVehicleSpawns.Remove(pendingIndex);

		m_sLastRespawnDiagnostic = string.Format("no answer to the spawn request for vehicle %1 within %2 ms",
			vehicleId, VEHICLE_SPAWN_TIMEOUT_MS);
		Print("[Overthrow] " + m_sLastRespawnDiagnostic, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-links a restored vehicle instance to everything that tracked the one it replaces.
	//!
	//! WHAT COMES BACK BY ITSELF and what does not. Ownership and lock state are read out of the
	//! vehicle's own stored record by OVT_PlayerOwnerComponentSerializer, so this does NOT re-apply them -
	//! doing so would hide a serializer that had stopped running. What it must repair is the manager
	//! side: OVT_RplOwnerManagerComponent keys ownership by RplId, this is a NEW instance with a NEW
	//! RplId, and the old one's entry now points at nothing.
	//!
	//! The one exception is a record that carried no owner at all. An unowned vehicle standing in the
	//! world is free for anybody to take, so the registry - which knows whose vehicle this is - repairs
	//! it and says so. Lock state is never repaired: the manager has no record of it.
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] vehicleId The registered persistent id this instance came back under.
	//! \param[in] vehicle The restored instance.
	protected void AdoptRespawnedVehicle(string playerPersistentId, string vehicleId, notnull IEntity vehicle)
	{
		// The persistence system spawned this instance, so it normally arrives already tracked - ASK
		// before registering rather than re-registering blind, as AttachRecruitBody() does.
		if (!OVT_PersistenceTracking.IsTracked(vehicle))
			OVT_PersistenceTracking.Track(vehicle);

		// A vehicle reached through the FindById fast path is a RESERVED live instance and is still
		// hidden; one that came out of a genuine RequestSpawn is not. Release() is idempotent, so this
		// one call puts both in play. The caller re-reserves afterwards if the owner has left again.
		OVT_PersistenceReservation.Release(vehicle);

		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
			vehicle.FindComponent(OVT_PlayerOwnerComponent)
		);

		if (ownerComp)
		{
			if (ownerComp.GetPlayerOwnerUid().IsEmpty())
			{
				Print("[Overthrow] Restored vehicle " + vehicleId + " carried no owner - repairing it from the registry", LogLevel.WARNING);
				ownerComp.SetPlayerOwner(playerPersistentId);
			}

			// Future ownership changes must re-index it, exactly as a freshly spawned vehicle's do
			ownerComp.GetOnOwnerChange().Insert(OnVehicleOwnershipChanged);
		}

		// RplIds are per-instance, so the RplId-keyed ownership maps still point at the despawned one.
		// Guarded because SetOwnerPersistentId() dereferences the RplComponent without checking, and this
		// runs from an engine callback where a null would take the whole session down.
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			SetOwnerPersistentId(playerPersistentId, vehicle);
		else
			Print("[Overthrow] Restored vehicle " + vehicleId + " has no RplComponent - it cannot be re-linked to its owner in the ownership maps", LogLevel.ERROR);

		// Manager-side tracking, under the id the registry already holds. Done by hand rather than through
		// RegisterPlayerVehicle() so the registry key stays stable even in the pathological case where the
		// restored instance answers with a different identity.
		m_mSpawnedVehicles[vehicleId] = vehicle.GetID();

		// Remove-then-insert rather than a contains check, so re-adopting an instance (a duplicate answer
		// to the same request) cannot leave two entries behind.
		m_aVehicles.RemoveItem(vehicle.GetID());
		m_aVehicles.Insert(vehicle.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Writes down where a player's vehicle is and what it is, so it can be rebuilt without its record.
	//!
	//! Called at every registration and refreshed before every save and immediately before a despawn -
	//! the transform has to be the one it was last standing at, not the one it was bought at.
	//! \param[in] playerUid The owner.
	//! \param[in] persistentId The vehicle's persistence id, which is the key.
	//! \param[in] vehicle The live instance to read.
	protected void CaptureVehicleRecord(string playerUid, string persistentId, notnull IEntity vehicle)
	{
		if (persistentId.IsEmpty())
			return;

		OVT_PersistedPlayerVehicle record;
		if (m_mVehicleRecords.Contains(persistentId))
			record = m_mVehicleRecords[persistentId];

		if (!record)
		{
			record = new OVT_PersistedPlayerVehicle();
			m_mVehicleRecords[persistentId] = record;
		}

		record.persistentId = persistentId;

		// An ownership change re-registers under the new owner; an empty uid here would be a caller bug,
		// but silently blanking a known owner is worse than keeping the last good one.
		if (!playerUid.IsEmpty())
			record.ownerUid = playerUid;

		record.prefab = OVT_Global.GetPrefabName(vehicle);
		record.position = vehicle.GetOrigin();
		record.angles = vehicle.GetYawPitchRoll();

		OVT_PlayerOwnerComponent ownerComp = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if (ownerComp)
			record.locked = ownerComp.IsLocked();
	}

	//------------------------------------------------------------------------------------------------
	//! Refreshes the stored transform and lock state of every registered vehicle still in the world.
	//!
	//! Runs from OVT_OverthrowGameMode.PreShutdownPersist(), i.e. before every save, for the same reason
	//! recruit positions are synced there: the record is only as good as its last refresh, and a vehicle
	//! driven across the map since it was registered would otherwise be rebuilt where it was bought.
	void SyncVehicleRecords()
	{
		if (!Replication.IsServer() || !m_mVehicleRecords)
			return;

		for (int i = 0; i < m_mVehicleRecords.Count(); i++)
		{
			OVT_PersistedPlayerVehicle record = m_mVehicleRecords.GetElement(i);
			if (!record)
				continue;

			IEntity vehicle = FindVehicleEntity(record.persistentId);
			if (!vehicle)
				continue;

			CaptureVehicleRecord(record.ownerUid, record.persistentId, vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a restored vehicle back at its recorded pose when load-time physics moved it.
	//!
	//! WHY VEHICLES MOVE DURING A LOAD. A vehicle self-spawns from its stored record at its exact
	//! saved transform, but it wakes as a live dynamic body with no saved velocities - and whatever
	//! it was parked ON (a buildable maintenance ramp, a base composition) is a separately
	//! self-spawned record with no ordering guarantee between collections. A vehicle whose support
	//! is not in the world yet free-falls onto the terrain and settles rotated, and a support
	//! spawning into it kicks it with the depenetration impulse (the BUG-129 mechanism). On flat
	//! ground both effects are invisible, which is why only ramp-parked vehicles were reported.
	//!
	//! The record's pose is the truth to snap back to: SyncVehicleRecords() captured it from the
	//! standing vehicle immediately before the save was written. Snapping is safe at the call site,
	//! InitialVehicleCleanup(), because by then every self-spawned entity - support included -
	//! exists and has collision.
	//!
	//! MUST RUN BEFORE RegisterPlayerVehicle() FOR THE SAME VEHICLE. Registration recaptures the
	//! record from the live pose, which would overwrite the saved pose with the drifted one and
	//! make the drift permanent.
	//!
	//! Public for the test seam only; production's one call site is InitialVehicleCleanup().
	//! \param[in] vehicleId The vehicle's persistent id; empty tolerated (untracked, no record).
	//! \param[in] vehicle The live instance to check.
	void ReassertRecordedPose(string vehicleId, notnull IEntity vehicle)
	{
		if (vehicleId.IsEmpty() || !m_mVehicleRecords.Contains(vehicleId))
			return;

		OVT_PersistedPlayerVehicle record = m_mVehicleRecords[vehicleId];
		if (!record || record.position == vector.Zero)
			return;

		// An occupied vehicle is being used, not settling - its pose is the driver's business.
		Vehicle typed = Vehicle.Cast(vehicle);
		if (typed && typed.IsOccupied())
			return;

		float positionDrift = vector.Distance(vehicle.GetOrigin(), record.position);

		vector liveAngles = vehicle.GetYawPitchRoll();
		float angleDrift = 0;
		for (int i = 0; i < 3; i++)
		{
			float delta = Math.AbsFloat(liveAngles[i] - record.angles[i]);
			if (delta > 180)
				delta = 360 - delta;
			if (delta > angleDrift)
				angleDrift = delta;
		}

		if (positionDrift < POSE_DRIFT_TOLERANCE_M && angleDrift < POSE_DRIFT_TOLERANCE_DEG)
			return;

		vector mat[4];
		Math3D.AnglesToMatrix(record.angles, mat);
		mat[3] = record.position;

		// The vehicle can still be tumbling when this runs - kill the motion before the teleport
		// or it survives it.
		Physics physics = vehicle.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}

		vehicle.SetTransform(mat);

		// Wake physics at the corrected pose so the vehicle rests on its support instead of a stale
		// contact set - the OVT_FlipVehicleAction idiom.
		if (physics)
			physics.ApplyImpulse("0 -1 0");

		Print(string.Format("[Overthrow] Vehicle %1 drifted %2 m / %3 deg from its recorded pose during the load - snapped back",
			vehicleId, positionDrift, angleDrift));
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the in-memory registries from a save.
	//!
	//! Both maps are derived from the same records, so they cannot disagree after a load the way two
	//! independently persisted structures could. Idempotent: applying the same save twice replaces the
	//! records rather than accumulating them, which is what re-applying to a live session needs.
	//! \param[in] records Vehicle records read from the save, may be null.
	void ApplyPersistedVehicles(array<ref OVT_PersistedPlayerVehicle> records)
	{
		if (!records)
			return;

		foreach (OVT_PersistedPlayerVehicle record : records)
		{
			if (!record)
				continue;

			if (record.persistentId.IsEmpty() || record.ownerUid.IsEmpty())
				continue;

			m_mVehicleRecords[record.persistentId] = record;

			if (!m_mPlayerVehicleIds.Contains(record.ownerUid))
				m_mPlayerVehicleIds[record.ownerUid] = new array<string>();

			array<string> ownedIds = m_mPlayerVehicleIds[record.ownerUid];
			if (ownedIds.Find(record.persistentId) == -1)
				ownedIds.Insert(record.persistentId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every vehicle registration currently held, for the serializer to write.
	//! \return The live map, keyed by vehicle persistent id. Never null.
	map<string, ref OVT_PersistedPlayerVehicle> GetVehicleRecords()
	{
		return m_mVehicleRecords;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a replacement vehicle from its registration when the stored record cannot be resolved.
	//!
	//! THIS IS NOT "MINTING A CAR FROM NOTHING", which an earlier version of this file rightly refused
	//! to do. The difference is provenance: Overthrow itself wrote down that this player owned this
	//! prefab at this position with this lock state, and that registration is carried by the save. What
	//! is lost is what only the vehicle's own record held - fuel, damage and cargo. What is kept is the
	//! vehicle, its place and its ownership, which is the difference between a player finding their car
	//! where they parked it and finding an empty street.
	//! \param[in] playerPersistentId The owner asking for it back.
	//! \param[in] vehicleId The registration id, which no longer resolves in storage.
	//! \return True when a replacement was built and registered, or when the registration was
	//!         deliberately kept for a later attempt (blocked spot); false when the registration is
	//!         unusable and the caller should drop it.
	protected bool RebuildVehicleFromRecord(string playerPersistentId, string vehicleId)
	{
		if (!m_mVehicleRecords.Contains(vehicleId))
			return false;

		OVT_PersistedPlayerVehicle record = m_mVehicleRecords[vehicleId];
		if (!record || record.prefab.IsEmpty() || record.position == vector.Zero)
			return false;

		// Never rebuild an FOB from a registration (BUG-129). The deployed FOB self-spawns from its
		// own stored record, and a mobile-FOB-truck registration that storage cannot resolve is a
		// ghost left behind by a deploy that consumed the truck (saves written before the deploy
		// path retired registrations). Rebuilding one put a free truck at the shop it was bought at
		// - and, after a restart, a dynamic body inside the static deployed FOB, wrecking it.
		// Returning false drops the registration, which is the heal for those older saves.
		if (IsMobileFOBPrefab(record.prefab))
		{
			Print(string.Format("[Overthrow] Registration %1 is a mobile FOB (%2) - dropping it instead of rebuilding", vehicleId, record.prefab), LogLevel.WARNING);
			return false;
		}

		// Copied out before the registration is dropped below - the record lives in the map being
		// modified, and reading through it afterwards would depend on how long it outlives its owner.
		ResourceName prefab = record.prefab;
		bool wasLocked = record.locked;

		vector mat[4];
		Math3D.AnglesToMatrix(record.angles, mat);
		mat[3] = record.position;

		// A dynamic body spawned interpenetrating another vehicle takes the whole depenetration
		// impulse and wrecks one or both (BUG-129). If something is parked on the recorded spot,
		// rebuild at the nearest parking/kerb instead; failing that, keep the registration and let
		// the owner's next connect try again.
		if (IsSpotBlockedByVehicle(record.position))
		{
			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			OVT_ParkingType parkingType = OVT_ParkingType.PARKING_CAR;
			int invId = economy.GetInventoryId(prefab);
			if (invId > -1)
				parkingType = economy.GetParkingType(invId);

			vector clearMat[4];
			if (GetNearestParkingSpot(record.position, clearMat, parkingType) || FindNearestKerbParking(record.position, 30, clearMat))
			{
				for (int i = 0; i < 4; i++)
				{
					mat[i] = clearMat[i];
				}
			}
			else
			{
				Print(string.Format("[Overthrow] Recorded spot for vehicle %1 is blocked and no clear spot is nearby - keeping the registration for a later attempt", vehicleId), LogLevel.WARNING);
				return true;
			}
		}

		IEntity vehicle = OVT_WorldUtils.SpawnEntityPrefabMatrix(prefab, mat);
		if (!vehicle)
			return false;

		// The replacement is a NEW instance with a new persistence id, so the old registration is retired
		// and the vehicle re-registered under whatever id it is given now.
		DropVehicleRegistration(playerPersistentId, vehicleId);

		SetOwnerPersistentId(playerPersistentId, vehicle);

		OVT_PlayerOwnerComponent ownerComp = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if (ownerComp)
		{
			ownerComp.SetPlayerOwner(playerPersistentId);
			ownerComp.SetLocked(wasLocked);
		}

		m_aVehicles.Insert(vehicle.GetID());
		RegisterPlayerVehicle(playerPersistentId, vehicle);

		Print(string.Format("[Overthrow] Vehicle %1 could not be read back from storage - rebuilt %2 at its last parked position for %3 (contents lost)",
			vehicleId, prefab, playerPersistentId), LogLevel.WARNING);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets a vehicle id that can never be resolved again.
	//! \param[in] playerPersistentId The owner it is registered under.
	//! \param[in] vehicleId The id to forget.
	protected void DropVehicleRegistration(string playerPersistentId, string vehicleId)
	{
		m_mSpawnedVehicles.Remove(vehicleId);
		m_mVehicleRecords.Remove(vehicleId);

		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return;

		array<string> vehicleIds = m_mPlayerVehicleIds[playerPersistentId];
		int index = vehicleIds.Find(vehicleId);
		if (index != -1)
			vehicleIds.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! The persistence collection player vehicles are stored in, resolved once and kept.
	//!
	//! Cached the way vanilla caches it (SCR_SpawnLogic.SetupPersistenceCollections, :51-55) - the lookup
	//! is by name against the loaded configuration and does not change during a session.
	//! \param[in] persistence The live persistence system.
	//! \return The collection, or null when the loaded configuration does not contain it.
	protected PersistenceCollection GetVehicleCollection(notnull SCR_PersistenceSystem persistence)
	{
		if (!m_VehicleCollection)
		{
			m_VehicleCollection = persistence.FindCollection(VEHICLE_COLLECTION);

			if (!m_VehicleCollection)
				Print("[Overthrow] No '" + VEHICLE_COLLECTION + "' persistence collection - player vehicles cannot be spawned back", LogLevel.WARNING);
		}

		return m_VehicleCollection;
	}

	//------------------------------------------------------------------------------------------------
	//! The persistent ids of every vehicle registered to a player, live or despawned.
	//!
	//! A COPY, so a caller iterating it cannot be tripped by a registration being dropped underneath it.
	//! \param[in] playerPersistentId The owning player.
	//! \return The registered ids; an empty array when the player has none.
	array<string> GetPlayerVehicleIds(string playerPersistentId)
	{
		array<string> ids = {};

		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return ids;

		array<string> registered = m_mPlayerVehicleIds[playerPersistentId];
		foreach (string vehicleId : registered)
		{
			ids.Insert(vehicleId);
		}

		return ids;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a spawn request for a vehicle has been sent and has not answered yet.
	//! \param[in] vehicleId The registered persistent id.
	//! \return True while the request is in flight.
	bool IsVehicleRespawnPending(string vehicleId)
	{
		if (!m_aPendingVehicleSpawns)
			return false;

		return m_aPendingVehicleSpawns.Find(vehicleId) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the last RespawnPlayerVehicles() attempt did not produce a vehicle.
	//!
	//! Cleared when a respawn is requested and cleared again when one succeeds, so a non-empty value
	//! always means the most recent attempt failed and says how.
	//! \return The diagnostic, or an empty string.
	string GetLastVehicleRespawnDiagnostic()
	{
		return m_sLastRespawnDiagnostic;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether another vehicle is standing on a prospective spawn spot (BUG-129).
	//! \param[in] pos The spot to test.
	//! \return True when any Vehicle overlaps the spot.
	protected bool IsSpotBlockedByVehicle(vector pos)
	{
		m_bSpotBlockedByVehicle = false;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 3, null, FilterSpotBlockingVehicle, EQueryEntitiesFlags.ALL);
		return m_bSpotBlockedByVehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! Filter function for IsSpotBlockedByVehicle
	protected bool FilterSpotBlockingVehicle(IEntity entity)
	{
		if (Vehicle.Cast(entity))
			m_bSpotBlockedByVehicle = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Find vehicle entity by persistent ID
	IEntity FindVehicleEntity(string vehicleId)
	{
		if (!m_mSpawnedVehicles.Contains(vehicleId))
			return null;
		
		EntityID entityId = m_mSpawnedVehicles[vehicleId];
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(entityId);
		
		// Clean up stale mapping if entity no longer exists
		if (!vehicle)
			m_mSpawnedVehicles.Remove(vehicleId);
		
		return vehicle;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if a player is currently connected, by their persistent identity id
	protected bool IsPlayerOnline(string playerPersistentId)
	{
		array<int> connectedPlayers = {};
		GetGame().GetPlayerManager().GetPlayers(connectedPlayers);
		foreach (int playerId : connectedPlayers)
		{
			string uid = OVT_Global.GetPlayerUID(playerId);
			if (!uid.IsEmpty() && uid == playerPersistentId)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-indexes every player-owned vehicle after a world load, and re-hides the ones whose owner is
	//! offline and who are locked.
	//!
	//! WHY A LOADED WORLD NEEDS THIS. Neither the reservation (entity flags) nor m_mSpawnedVehicles
	//! survives a restart: vehicles self-spawn from their own stored records with their prefab's flags,
	//! i.e. visible and simulating, and the manager's live-instance map starts empty. This pass is what
	//! reconnects the two - it finds every player-owned vehicle in the world, registers it under its
	//! owner, and puts the locked ones belonging to absent players back to sleep.
	//!
	//! IT NO LONGER DESPAWNS ANYTHING. It used to save-and-release an offline owner's locked vehicles
	//! back into storage, which is the lossy path BUG-086 removed; the vehicle is now hidden in place
	//! with everything inside it.
	//!
	//! AN OFFLINE OWNER'S UNLOCKED VEHICLE IS REGISTERED AND LEFT ALONE. Registering it is a fix in its
	//! own right - the previous two-branch form (offline-and-locked, else online) matched neither, so an
	//! absent player's unlocked car fell out of manager tracking entirely and was never re-indexed after
	//! a restart. Leaving it visible is the rule the whole feature is gated on: unlocked means public,
	//! and the resistance keeps using it while its owner is away.
	protected void InitialVehicleCleanup()
	{
		Print("[Overthrow] Re-indexing player-owned vehicles after world load...");

		// Find all player-owned vehicles in the world
		m_aFoundPlayerVehicles.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999, null, FilterPlayerOwnedVehicles, EQueryEntitiesFlags.ALL);

		int despawnedCount = 0;

		foreach (EntityID vehicleId : m_aFoundPlayerVehicles)
		{
			IEntity vehicle = GetGame().GetWorld().FindEntityByID(vehicleId);
			if (!vehicle)
				continue;
			OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
				vehicle.FindComponent(OVT_PlayerOwnerComponent)
			);

			if (!ownerComp)
				continue;

			string ownerUid = ownerComp.GetPlayerOwnerUid();
			if (ownerUid.IsEmpty())
				continue;

			string persistentId = OVT_PersistenceTracking.GetPersistentId(vehicle);

			// Load-time physics can have moved this vehicle off its saved pose (fell off a not-yet-
			// spawned ramp, depenetration kick). Snap it back BEFORE RegisterPlayerVehicle(), whose
			// record recapture would otherwise make the drifted pose the recorded one.
			ReassertRecordedPose(persistentId, vehicle);

			// EVERY player-owned vehicle is indexed, whatever its owner is doing - see the method header
			// for the offline-and-unlocked case this used to drop on the floor.
			RegisterPlayerVehicle(ownerUid, vehicle);

			// The RplId-keyed ownership maps are memory-only and start empty after a load. Without
			// this repair GetOwnerID() answers "" for every pre-restart vehicle, so deploying a
			// pre-restart truck minted an UNOWNED FOB - unprotected from garbage collection and
			// invisible to this very re-index (BUG-129). Guarded: SetOwnerPersistentId dereferences
			// the RplComponent without checking.
			if (RplComponent.Cast(vehicle.FindComponent(RplComponent)))
				SetOwnerPersistentId(ownerUid, vehicle);

			// Check if player is currently online by comparing persistent identity ids
			bool isOnline = IsPlayerOnline(ownerUid);

			// Only LOCKED vehicles of OFFLINE players are hidden; unlocked ones stay public
			// property, and an FOB is never hidden at all (BUG-129)
			if (!isOnline && ownerComp.IsLocked() && !IsMobileFOB(vehicle))
			{
				if (!persistentId.IsEmpty() && ReserveVehicle(persistentId, vehicle))
				{
					despawnedCount++;

					Print(string.Format("[Overthrow] Reserved offline player vehicle: %1 (owner: %2)", persistentId, ownerUid));
				}
			}
			else if (isOnline)
			{
				// Their owner is here: anything a previous session hid is theirs to use again
				OVT_PersistenceReservation.Release(vehicle);
			}
		}

		Print(string.Format("[Overthrow] Vehicle re-indexing complete. Reserved %1 offline players' locked vehicles.", despawnedCount));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Filter function to find player-owned vehicles
	protected bool FilterPlayerOwnedVehicles(IEntity entity)
	{
		// Check if it's a vehicle with player owner component
		if (!Vehicle.Cast(entity))
			return false;
		
		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(
			entity.FindComponent(OVT_PlayerOwnerComponent)
		);
		
		if (ownerComp && !ownerComp.GetPlayerOwnerUid().IsEmpty())
		{
			m_aFoundPlayerVehicles.Insert(entity.GetID());
		}
		
		return false; // Always return false for QueryEntitiesBySphere callback pattern
	}
	
	
}