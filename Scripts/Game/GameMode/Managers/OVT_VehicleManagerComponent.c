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
		m_aParkingSearch.Clear();
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
		IEntity ent = OVT_Global.SpawnEntityPrefabMatrix(prefab, mat);
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
			SCR_EntityHelper.DeleteEntityAndChildren(m_pUpgradeOldVehicle);
			m_pUpgradeOldVehicle = null;
		}
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
			if (vehicle && IsVehicleLocked(vehicle))
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
	//! Takes an offline player's locked vehicles out of the world without losing them.
	//!
	//! PUBLIC because it is one half of a lifecycle whose other half (RespawnPlayerVehicles) is public
	//! too, and because the automated round-trip case has to be able to drive the disconnect flow's own
	//! method rather than a re-implementation of it. Server-side; a client has no persistence system and
	//! every call below becomes a no-op there.
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

			// Only despawn locked vehicles
			if (!IsVehicleLocked(vehicle))
				continue;

			ReleaseVehicle(vehicleId, vehicle);
			despawnedCount++;
		}

		Print(string.Format("[Overthrow] Despawned %1 locked vehicles for offline player: %2", despawnedCount, playerPersistentId));
	}

	//------------------------------------------------------------------------------------------------
	//! Writes a vehicle's record, releases tracking WITHOUT dropping that record, and removes it from
	//! the world.
	//!
	//! THE POINT OF THE ORDER. Save() writes the vehicle - transform, fuel, damage and inventory
	//! included, through the serializers vanilla's Vehicle.conf already binds - into storage; only then
	//! is tracking released with the data KEPT (StopTracking(entity, removeData: false)). Deleting a
	//! tracked entity without that last step drops its stored record too, and the vehicle is gone for
	//! good.
	//!
	//! This is vanilla's own "despawn but do not forget" idiom - SCR_SpawnLogic.c:107-108 and :215-216
	//! do exactly this to a disconnecting player's controller and character, and
	//! OVT_RecruitManagerComponent.ReleaseRecruitBody() to a recruit's body.
	//!
	//! The REGISTRATION IS DELIBERATELY KEPT. m_mPlayerVehicleIds is what RespawnPlayerVehicles() reads
	//! to know what to ask for; dropping the id here would make the vehicle unrecoverable in this
	//! session. Only the live-instance mapping goes.
	//! \param[in] vehicleId The vehicle's persistent id, which stays registered under its owner.
	//! \param[in] vehicle The instance to write, release and delete.
	protected void ReleaseVehicle(string vehicleId, notnull IEntity vehicle)
	{
		// LAST CHANCE to read the transform and lock state off the live instance. After the delete below
		// the only remaining description of this vehicle is its registration, and that is what a rebuild
		// uses if the stored record cannot be read back next session.
		string ownerUid;
		if (m_mVehicleRecords.Contains(vehicleId) && m_mVehicleRecords[vehicleId])
			ownerUid = m_mVehicleRecords[vehicleId].ownerUid;

		CaptureVehicleRecord(ownerUid, vehicleId, vehicle);

		OVT_PersistenceTracking.Save(vehicle);
		OVT_PersistenceTracking.Untrack(vehicle, true);

		// Remove from spawned tracking
		m_mSpawnedVehicles.Remove(vehicleId);
		m_aVehicles.RemoveItem(vehicle.GetID());

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
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
			// Already standing in the world - nothing to fetch
			if (FindVehicleEntity(vehicleId))
				continue;

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

		// The owner may have left again while the request was in flight. Putting a vehicle in the world
		// for somebody who is not there is precisely what the offline despawn undoes, so undo it now:
		// save-and-release keeps it, with its cargo, for the next time they return.
		if (!IsPlayerOnline(playerPersistentId))
		{
			Print("[Overthrow] Owner of vehicle " + vehicleId + " left before it arrived - releasing it again");
			ReleaseVehicle(vehicleId, vehicle);
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
	//! \return True when a replacement was built and registered.
	protected bool RebuildVehicleFromRecord(string playerPersistentId, string vehicleId)
	{
		if (!m_mVehicleRecords.Contains(vehicleId))
			return false;

		OVT_PersistedPlayerVehicle record = m_mVehicleRecords[vehicleId];
		if (!record || record.prefab.IsEmpty() || record.position == vector.Zero)
			return false;

		// Copied out before the registration is dropped below - the record lives in the map being
		// modified, and reading through it afterwards would depend on how long it outlives its owner.
		ResourceName prefab = record.prefab;
		bool wasLocked = record.locked;

		vector mat[4];
		Math3D.AnglesToMatrix(record.angles, mat);
		mat[3] = record.position;

		IEntity vehicle = OVT_Global.SpawnEntityPrefabMatrix(prefab, mat);
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
	//! Initial cleanup of offline player vehicles after server restart
	protected void InitialVehicleCleanup()
	{
		Print("[Overthrow] Starting initial vehicle cleanup for offline players...");

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

			// Check if player is currently online by comparing persistent identity ids
			bool isOnline = IsPlayerOnline(ownerUid);
			
			// Only despawn locked vehicles of offline players
			if (!isOnline && ownerComp.IsLocked())
			{
				// Register vehicle first (this indexes it under its owner)
				RegisterPlayerVehicle(ownerUid, vehicle);

				string persistentId = OVT_PersistenceTracking.GetPersistentId(vehicle);
				if (!persistentId.IsEmpty())
				{
					ReleaseVehicle(persistentId, vehicle);
					despawnedCount++;

					Print(string.Format("[Overthrow] Despawned offline player vehicle: %1 (owner: %2)", persistentId, ownerUid));
				}
			}
			else if (isOnline)
			{
				// Player is online, register vehicle for management
				RegisterPlayerVehicle(ownerUid, vehicle);
			}
		}
		
		Print(string.Format("[Overthrow] Initial vehicle cleanup complete. Despawned %1 offline player vehicles.", despawnedCount));
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