//! Unified entity spawning API for deployment modules
//! Provides standardized methods for spawning various entity types used in deployments
class OVT_EntitySpawningAPI : Managed
{
	//------------------------------------------------------------------------------------------------
	// Vehicle spawning
	//------------------------------------------------------------------------------------------------
	static IEntity SpawnVehicle(ResourceName vehiclePrefab, vector position, vector orientation = "0 0 0", int factionIndex = -1)
	{
		if (!vehiclePrefab || vehiclePrefab.IsEmpty())
			return null;
			
		// Create spawn parameters
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		// Set transform matrix
		vector mat[4];
		Math3D.AnglesToMatrix(orientation, mat);
		mat[3] = position;
		spawnParams.Transform = mat;
		
		// Spawn the vehicle
		IEntity vehicle = GetGame().SpawnEntityPrefab(Resource.Load(vehiclePrefab), GetGame().GetWorld(), spawnParams);
		if (!vehicle)
		{
			Print(string.Format("Failed to spawn vehicle: %1", vehiclePrefab), LogLevel.ERROR);
			return null;
		}
		
		// Set faction if specified
		if (factionIndex >= 0)
		{
			SetEntityFaction(vehicle, factionIndex);
		}
		
		// Perform post-spawn setup
		PostSpawnVehicleSetup(vehicle, factionIndex);
		
		Print(string.Format("Spawned vehicle %1 at %2", vehiclePrefab, position.ToString()), LogLevel.VERBOSE);
		return vehicle;
	}
	
	//------------------------------------------------------------------------------------------------
	// Infantry group spawning
	//------------------------------------------------------------------------------------------------
	static SCR_AIGroup SpawnInfantryGroup(ResourceName groupPrefab, vector position, vector orientation = "0 0 0", int factionIndex = -1)
	{
		if (!groupPrefab || groupPrefab.IsEmpty())
			return null;
			
		// Create spawn parameters
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		// Set transform matrix
		vector mat[4];
		Math3D.AnglesToMatrix(orientation, mat);
		mat[3] = position;
		spawnParams.Transform = mat;
		
		// Spawn the group
		IEntity groupEntity = GetGame().SpawnEntityPrefab(Resource.Load(groupPrefab), GetGame().GetWorld(), spawnParams);
		if (!groupEntity)
		{
			Print(string.Format("Failed to spawn infantry group: %1", groupPrefab), LogLevel.ERROR);
			return null;
		}
		
		// Get the AI group component
		SCR_AIGroup group = SCR_AIGroup.Cast(groupEntity);
		if (!group)
		{
			Print(string.Format("Spawned entity is not an AI group: %1", groupPrefab), LogLevel.ERROR);
			delete groupEntity;
			return null;
		}
		
		// Set faction if specified
		if (factionIndex >= 0)
		{
			SetGroupFaction(group, factionIndex);
		}
		
		// Perform post-spawn setup
		PostSpawnGroupSetup(group, factionIndex);
		
		Print(string.Format("Spawned infantry group %1 at %2", groupPrefab, position.ToString()), LogLevel.VERBOSE);
		return group;
	}
	
	//------------------------------------------------------------------------------------------------
	// Individual soldier spawning
	//------------------------------------------------------------------------------------------------
	static IEntity SpawnSoldier(ResourceName soldierPrefab, vector position, vector orientation = "0 0 0", int factionIndex = -1)
	{
		if (!soldierPrefab || soldierPrefab.IsEmpty())
			return null;
			
		// Find a safe spawn position
		vector safePosition = OVT_Global.FindSafeSpawnPosition(position);
		
		// Create spawn parameters
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		// Set transform matrix
		vector mat[4];
		Math3D.AnglesToMatrix(orientation, mat);
		mat[3] = safePosition;
		spawnParams.Transform = mat;
		
		// Spawn the soldier
		IEntity soldier = GetGame().SpawnEntityPrefab(Resource.Load(soldierPrefab), GetGame().GetWorld(), spawnParams);
		if (!soldier)
		{
			Print(string.Format("Failed to spawn soldier: %1", soldierPrefab), LogLevel.ERROR);
			return null;
		}
		
		// Set faction if specified
		if (factionIndex >= 0)
		{
			SetEntityFaction(soldier, factionIndex);
		}
		
		// Perform post-spawn setup
		PostSpawnSoldierSetup(soldier, factionIndex);
		
		Print(string.Format("Spawned soldier %1 at %2", soldierPrefab, safePosition.ToString()), LogLevel.VERBOSE);
		return soldier;
	}
	
	//------------------------------------------------------------------------------------------------
	// Static object spawning (defenses, structures, etc.)
	//------------------------------------------------------------------------------------------------
	static IEntity SpawnStaticObject(ResourceName objectPrefab, vector position, vector orientation = "0 0 0", int factionIndex = -1)
	{
		if (!objectPrefab || objectPrefab.IsEmpty())
			return null;
			
		// Create spawn parameters
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		// Set transform matrix
		vector mat[4];
		Math3D.AnglesToMatrix(orientation, mat);
		mat[3] = position;
		spawnParams.Transform = mat;
		
		// Spawn the object
		IEntity obj = GetGame().SpawnEntityPrefab(Resource.Load(objectPrefab), GetGame().GetWorld(), spawnParams);
		if (!obj)
		{
			Print(string.Format("Failed to spawn static object: %1", objectPrefab), LogLevel.ERROR);
			return null;
		}
		
		// Set faction if specified
		if (factionIndex >= 0)
		{
			SetEntityFaction(obj, factionIndex);
		}
		
		Print(string.Format("Spawned static object %1 at %2", objectPrefab, position.ToString()), LogLevel.VERBOSE);
		return obj;
	}
	
	//------------------------------------------------------------------------------------------------
	// Batch spawning for multiple entities
	//------------------------------------------------------------------------------------------------
	static array<IEntity> SpawnEntityBatch(array<ResourceName> prefabs, array<vector> positions, array<vector> orientations = null, int factionIndex = -1)
	{
		array<IEntity> spawnedEntities = new array<IEntity>;
		
		if (!prefabs || !positions || prefabs.Count() != positions.Count())
		{
			Print("Invalid parameters for batch spawning", LogLevel.ERROR);
			return spawnedEntities;
		}
		
		// Check orientations array
		if (orientations && orientations.Count() != prefabs.Count())
		{
			Print("Orientations array size mismatch for batch spawning", LogLevel.WARNING);
			orientations = null;
		}
		
		for (int i = 0; i < prefabs.Count(); i++)
		{
			vector orientation = "0 0 0";
			if (orientations)
				orientation = orientations[i];
				
			IEntity entity = SpawnStaticObject(prefabs[i], positions[i], orientation, factionIndex);
			if (entity)
				spawnedEntities.Insert(entity);
		}
		
		Print(string.Format("Batch spawned %1/%2 entities", spawnedEntities.Count(), prefabs.Count()), LogLevel.NORMAL);
		return spawnedEntities;
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods
	//------------------------------------------------------------------------------------------------
	protected static void SetEntityFaction(IEntity entity, int factionIndex)
	{
		if (!entity || factionIndex < 0)
			return;
			
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;
			
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return;
			
		// Set faction for the entity
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
		if (factionComp)
		{
			factionComp.SetAffiliatedFaction(faction);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected static void SetGroupFaction(SCR_AIGroup group, int factionIndex)
	{
		if (!group || factionIndex < 0)
			return;
			
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return;
			
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return;
			
		// Set faction for the group
		group.SetFaction(faction);
		
		// Also set faction for all agents in the group
		array<AIAgent> agents = new array<AIAgent>;
		group.GetAgents(agents);
		
		foreach (AIAgent agent : agents)
		{
			IEntity soldier = agent.GetControlledEntity();
			if (soldier)
				SetEntityFaction(soldier, factionIndex);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected static void PostSpawnVehicleSetup(IEntity vehicle, int factionIndex)
	{
		if (!vehicle)
			return;
			
		// Add vehicle to faction vehicle tracking if needed
		OVT_VehicleManagerComponent vehicleManager = OVT_Global.GetVehicles();
		if (vehicleManager)
		{
			// Register vehicle for management (if applicable)
			// This would be specific to Overthrow's vehicle system
		}
		
		// Set initial fuel level
		SCR_FuelManagerComponent fuelManager = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
		if (fuelManager)
		{
			// Set to full fuel for deployed vehicles
			array<BaseFuelNode> fuelNodes = new array<BaseFuelNode>;
			fuelManager.GetFuelNodesList(fuelNodes);
			foreach (BaseFuelNode node : fuelNodes)
			{
				node.SetFuel(node.GetMaxFuel());
			}
		}
		
		// Initialize vehicle damage state (undamaged)
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
		if (damageManager)
		{
			// Vehicle starts undamaged
			damageManager.FullHeal();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected static void PostSpawnGroupSetup(SCR_AIGroup group, int factionIndex)
	{
		if (!group)
			return;
			
		// Set initial group behavior
		group.SetMaxMembers(8); // Standard squad size
		
		// Apply faction-specific loadouts if needed
		array<AIAgent> agents = new array<AIAgent>;
		group.GetAgents(agents);
		
		foreach (AIAgent agent : agents)
		{
			IEntity soldier = agent.GetControlledEntity();
			if (soldier)
				PostSpawnSoldierSetup(soldier, factionIndex);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected static void PostSpawnSoldierSetup(IEntity soldier, int factionIndex)
	{
		if (!soldier)
			return;
			
		// Heal soldier to full health
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(soldier.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damageManager)
		{
			damageManager.FullHeal();
		}
		
		// Randomize civilian clothes if this is a civilian
		// This would be faction-specific logic
		if (IsCivilianFaction(factionIndex))
		{
			AIAgent agent = AIAgent.Cast(soldier.FindComponent(AIAgent));
			if (agent)
				OVT_Global.RandomizeCivilianClothes(agent);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected static bool IsCivilianFaction(int factionIndex)
	{
		// Determine if this is a civilian faction
		// This is simplified - in practice you'd check against known faction types
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return false;
			
		Faction faction = factionManager.GetFactionByIndex(factionIndex);
		if (!faction)
			return false;
			
		string factionKey = faction.GetFactionKey();
		
		// Check if it's not one of the main military factions
		return (factionKey != OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey() &&
		        factionKey != OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey() &&
		        factionKey != OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey());
	}
	
	//------------------------------------------------------------------------------------------------
	// Safe cleanup methods
	//------------------------------------------------------------------------------------------------
	static void CleanupEntity(IEntity entity)
	{
		if (!entity)
			return;
			
		// Perform any necessary cleanup before deletion
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (group)
		{
			CleanupGroup(group);
		}
		
		// Delete the entity
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
	
	//------------------------------------------------------------------------------------------------
	static void CleanupGroup(SCR_AIGroup group)
	{
		if (!group)
			return;
			
		// Clear waypoints
		array<AIWaypoint> waypoints = new array<AIWaypoint>;
		group.GetWaypoints(waypoints);
		foreach (AIWaypoint waypoint : waypoints)
		{
			group.RemoveWaypoint(waypoint);
		}
		
		// Get all agents before deletion
		array<AIAgent> agents = new array<AIAgent>;
		group.GetAgents(agents);
		
		// Delete individual soldiers
		foreach (AIAgent agent : agents)
		{
			IEntity soldier = agent.GetControlledEntity();
			if (soldier)
				SCR_EntityHelper.DeleteEntityAndChildren(soldier);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	static void CleanupEntityArray(array<IEntity> entities)
	{
		if (!entities)
			return;
			
		foreach (IEntity entity : entities)
		{
			CleanupEntity(entity);
		}
		
		entities.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	// Validation methods
	//------------------------------------------------------------------------------------------------
	static bool ValidateSpawnPosition(vector position, float radius = 5.0)
	{
		// Check if position is suitable for spawning
		TraceParam param = new TraceParam();
		param.Start = position + Vector(0, 100, 0);
		param.End = position + Vector(0, -100, 0);
		param.Flags = TraceFlags.WORLD;
		
		float result = GetGame().GetWorld().TraceMove(param, null);
		if (result >= 1.0)
			return false; // No ground found
			
		// Check for entity collisions in the area
		array<IEntity> nearbyEntities = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(position, radius, null, FilterCollidingEntities, EQueryEntitiesFlags.ALL);
		
		return m_CollisionCheckPassed;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static bool m_CollisionCheckPassed = true;
	
	//------------------------------------------------------------------------------------------------
	protected static bool FilterCollidingEntities(IEntity entity)
	{
		// Check if entity would cause collision issues
		if (!entity)
			return false;
			
		// Skip terrain and non-solid entities
		Physics physics = entity.GetPhysics();
		if (!physics)
			return false;
			
		// If we find solid physics, mark collision check as failed
		m_CollisionCheckPassed = false;
		return false; // Stop searching
	}
}