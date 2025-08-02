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
	
	//! Despawn time for locked vehicles when player is offline (10 minutes)
	static const float OFFLINE_VEHICLE_DESPAWN_TIME = 60.0;
	
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
		
		// Delayed initialization to despawn offline player vehicles after EPF loads everything
		GetGame().GetCallqueue().CallLater(InitialVehicleCleanup, 5000, false);
	}
	
	void SpawnStartingCar(IEntity home, string playerId)
	{		
		vector mat[4];
		
		int i = s_AIRandomGenerator.RandInt(0, m_pStartingCarPrefabs.Count()-1);
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
			OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(veh);
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
			OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(ent);
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
		
		// Get EPF persistence component
		EPF_PersistenceComponent persistenceComp = EPF_PersistenceComponent.Cast(
			vehicle.FindComponent(EPF_PersistenceComponent)
		);
		
		if (!persistenceComp)
		{
			Print("[Overthrow] Vehicle missing EPF_PersistenceComponent, cannot register for management");
			return;
		}
		
		// Use existing persistent ID from EPF
		string persistentId = persistenceComp.GetPersistentId();
		if (persistentId.IsEmpty())
		{
			Print("[Overthrow] Vehicle has no persistent ID, cannot register for management");
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
		
		// Note: EPF handles auto-saving, no manual save needed
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
	//! Despawn locked vehicles for an offline player
	protected void DespawnPlayerLockedVehicles(string playerPersistentId)
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
			
			// Pause EPF tracking before despawn to keep it for auto-spawning
			EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
				vehicle.FindComponent(EPF_PersistenceComponent)
			);
			
			if (persistence)
			{
				persistence.Save(); // Save current state
				persistence.PauseTracking(); // Pause tracking to keep for auto-spawn
				Print(string.Format("[Overthrow] Paused EPF tracking for vehicle: %1", vehicleId));
			}
			
			// Remove from spawned tracking
			m_mSpawnedVehicles.Remove(vehicleId);
			m_aVehicles.RemoveItem(vehicle.GetID());
			
			// Delete from world (EPF will auto-spawn on restart)
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			despawnedCount++;
		}
		
		Print(string.Format("[Overthrow] Despawned %1 locked vehicles for offline player: %2", despawnedCount, playerPersistentId));
	}
	
	
	//------------------------------------------------------------------------------------------------
	//! Save all vehicles for a player via EPF
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
			
			// Force save via EPF
			EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
				vehicle.FindComponent(EPF_PersistenceComponent)
			);
			
			if (persistence)
			{
				persistence.Save();
				savedCount++;
			}
		}
		
		if (savedCount > 0)
			Print(string.Format("[Overthrow] Saved %1 vehicles for player: %2", savedCount, playerPersistentId));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Respawn paused vehicles for a connected player
	protected void RespawnPlayerVehicles(string playerPersistentId)
	{
		if (!m_mPlayerVehicleIds.Contains(playerPersistentId))
			return;
		
		array<string> vehicleIds = m_mPlayerVehicleIds[playerPersistentId];
		int respawnedCount = 0;
		
		foreach (string vehicleId : vehicleIds)
		{
			// Check if already spawned
			IEntity existingVehicle = FindVehicleEntity(vehicleId);
			if (existingVehicle)
				continue;
			
			// Load vehicle from EPF using the proper method
			EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
			if (persistenceManager)
			{
				Print(string.Format("[Overthrow] Loading vehicle from EPF: %1", vehicleId));
				
				// Load the vehicle entity from EPF save data
				IEntity vehicleEntity = EPF_PersistentWorldEntityLoader.Load(EPF_VehicleSaveData, vehicleId);
				if (vehicleEntity)
				{
					// Resume EPF tracking for the respawned vehicle
					EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
						vehicleEntity.FindComponent(EPF_PersistenceComponent)
					);
					
					if (persistence)
					{
						persistence.ResumeTracking();
					}
					
					// Track spawned vehicle
					m_mSpawnedVehicles[vehicleId] = vehicleEntity.GetID();
					m_aVehicles.Insert(vehicleEntity.GetID());
					respawnedCount++;
					
					Print(string.Format("[Overthrow] Successfully respawned vehicle: %1 for player: %2", vehicleId, playerPersistentId));
				}
				else
				{
					Print(string.Format("[Overthrow] Failed to load vehicle from EPF: %1", vehicleId));
				}
			}
		}
		
		if (respawnedCount > 0)
			Print(string.Format("[Overthrow] Respawned %1 vehicles for player: %2", respawnedCount, playerPersistentId));
	}

	//------------------------------------------------------------------------------------------------
	//! Find vehicle entity by persistent ID
	protected IEntity FindVehicleEntity(string vehicleId)
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
	//! Initial cleanup of offline player vehicles after server restart
	protected void InitialVehicleCleanup()
	{
		Print("[Overthrow] Starting initial vehicle cleanup for offline players...");
		
		// Find all player-owned vehicles in the world
		m_aFoundPlayerVehicles.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999, null, FilterPlayerOwnedVehicles, EQueryEntitiesFlags.ALL);
		
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return;
		
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
			
			// Check if player is currently online
			OVT_PlayerData playerData = playerManager.GetPlayer(ownerUid);
			bool isOnline = playerData && !playerData.IsOffline();
			
			// Only despawn locked vehicles of offline players
			if (!isOnline && ownerComp.IsLocked())
			{
				// Register vehicle first (this sets up EPF persistence correctly)
				RegisterPlayerVehicle(ownerUid, vehicle);
				
				// Get persistent ID for tracking
				EPF_PersistenceComponent persistenceComp = EPF_PersistenceComponent.Cast(
					vehicle.FindComponent(EPF_PersistenceComponent)
				);
				
				if (persistenceComp)
				{
					string persistentId = persistenceComp.GetPersistentId();
					
					// Save current state and pause EPF tracking for auto-spawn
					persistenceComp.Save();
					persistenceComp.PauseTracking(); // Keep for EPF auto-spawn on restart
					
					// Remove from tracking
					m_mSpawnedVehicles.Remove(persistentId);
					m_aVehicles.RemoveItem(vehicle.GetID());
					
					// Delete from world (EPF will auto-spawn on restart)
					SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
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