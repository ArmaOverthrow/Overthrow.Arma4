//! Vehicle spawning module for deployments
//! Handles spawning vehicles with crew/passengers
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_VehicleSpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(desc: "Vehicle type name from faction registry")]
	string m_sVehicleType;
	
	[Attribute(defvalue: "1", desc: "Number of vehicles to spawn")]
	int m_iVehicleCount;
	
	[Attribute(desc: "Infantry group type to use as crew/passengers")]
	string m_sCrewGroupType;
	
	// Vehicle crew assignment happens automatically based on spawned group members
	
	[Attribute(defvalue: "50", desc: "Spawn radius around deployment position")]
	float m_fSpawnRadius;
	
	[Attribute(defvalue: "50", desc: "Resource cost per vehicle")]
	int m_iCostPerVehicle;
	
	[Attribute(defvalue: "true", desc: "Allow reinforcement when vehicles are destroyed")]
	bool m_bAllowReinforcement;
	
	[Attribute(defvalue: "25", desc: "Reinforcement cost per vehicle")]
	int m_iReinforcementCost;
	
	protected ref array<Vehicle> m_aSpawnedVehicles;
	protected ref array<SCR_AIGroup> m_aSpawnedGroups;
	protected ref map<ref EntityID, ref EntityID> m_mPendingCrewAssignments; // GroupID -> VehicleID mappings for pending crew assignments
	protected int m_iSpawnedCount;
	
	//------------------------------------------------------------------------------------------------
	void OVT_VehicleSpawningDeploymentModule()
	{
		m_aSpawnedVehicles = new array<Vehicle>;
		m_aSpawnedGroups = new array<SCR_AIGroup>;
		m_mPendingCrewAssignments = new map<ref EntityID, ref EntityID>;
		m_iSpawnedCount = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override int GetResourceCost()
	{
		return m_iVehicleCount * m_iCostPerVehicle;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		
		if (!m_ParentDeployment)
			return;
		
		// Check if vehicles have been eliminated previously
		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
		{
			Print(string.Format("Vehicles for deployment '%1' were previously eliminated and will not respawn", m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
			return;
		}
			
		SpawnVehicles();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();
		
		Print(string.Format("[Overthrow] Vehicle patrol despawning near %1", OVT_Global.GetTowns().GetNearestTownName(m_ParentDeployment.GetOwner().GetOrigin())), LogLevel.NORMAL);
		
		// Clean up spawned vehicles and groups
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle)
			{
				//Only clean up if near point of departure, otherwise they are likely dead or the vehicle was stolen/destroyed
				float distance = vector.Distance(vehicle.GetOrigin(), m_ParentDeployment.GetOwner().GetOrigin());
				if(distance > 40) continue;
				
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			}
		}
		
		foreach (SCR_AIGroup group : m_aSpawnedGroups)
		{
			if (group)
			{
				//Only clean up if near point of departure				
				float distance = vector.Distance(group.GetOrigin(), m_ParentDeployment.GetOwner().GetOrigin());
				if(distance > 40) continue;
				
				OVT_EntitySpawningAPI.CleanupGroup(group);
			}
		}
		
		m_aSpawnedVehicles.Clear();
		m_aSpawnedGroups.Clear();
		m_iSpawnedCount = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		// Check for destroyed vehicles
		CheckVehicleStatus();
	}
	
	//------------------------------------------------------------------------------------------------
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;
		
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle)
				entities.Insert(vehicle);
		}
		
		foreach (SCR_AIGroup group : m_aSpawnedGroups)
		{
			if (group)
				entities.Insert(group);
		}
		
		return entities;
	}
	
	array<SCR_AIGroup> GetSpawnedGroups()
	{
		return m_aSpawnedGroups;
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_VehicleSpawningDeploymentModule clone = new OVT_VehicleSpawningDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_sVehicleType = m_sVehicleType;
		clone.m_iVehicleCount = m_iVehicleCount;
		clone.m_sCrewGroupType = m_sCrewGroupType;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerVehicle = m_iCostPerVehicle;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void SpawnVehicles()
	{
		if (!m_ParentDeployment || m_sVehicleType.IsEmpty())
			return;
			
		vector deploymentPos = m_ParentDeployment.GetPosition();
		int factionIndex = m_ParentDeployment.GetControllingFaction();
		
		// Get vehicle prefab from faction registry
		ResourceName vehiclePrefab = GetVehiclePrefabFromFaction(factionIndex);
		if (vehiclePrefab.IsEmpty())
		{
			Print(string.Format("Failed to get vehicle prefab for type '%1' from faction %2", m_sVehicleType, factionIndex), LogLevel.ERROR);
			return;
		}
		
		// Get crew group prefab if needed
		ResourceName crewPrefab = "";
		if (!m_sCrewGroupType.IsEmpty())
		{
			crewPrefab = GetGroupPrefabFromFaction(factionIndex);
			if (crewPrefab.IsEmpty())
			{
				Print(string.Format("Failed to get crew group prefab for type '%1' from faction %2", m_sCrewGroupType, factionIndex), LogLevel.WARNING);
			}
		}
		
		for (int i = 0; i < m_iVehicleCount; i++)
		{
			vector spawnPos = GetRandomSpawnPosition(deploymentPos);
			
			//Move deployment to this position, so we know when to clean it up
			m_ParentDeployment.GetOwner().SetOrigin(spawnPos);
			
			// Spawn vehicle
			Vehicle vehicle = Vehicle.Cast(SpawnEntity(vehiclePrefab, spawnPos));
			if (!vehicle)
			{
				Print(string.Format("Failed to spawn vehicle %1 (%2)", i + 1, m_sVehicleType), LogLevel.ERROR);
				continue;
			}
			
			m_aSpawnedVehicles.Insert(vehicle);
			m_iSpawnedCount++;
			
			// Spawn crew if we have a crew prefab
			if (!crewPrefab.IsEmpty())
			{
				SCR_AIGroup crewGroup = SpawnCrewForVehicle(vehicle, crewPrefab, factionIndex);
				if (crewGroup)
					m_aSpawnedGroups.Insert(crewGroup);
			}
			
			Print(string.Format("Spawned vehicle %1 (%2) at %3", i + 1, m_sVehicleType, spawnPos.ToString()), LogLevel.VERBOSE);
		}
		
		Print(string.Format("[Overthrow] Vehicle spawning complete: %1/%2 vehicles spawned for type '%3' near %4", 
			m_aSpawnedVehicles.Count(), m_iVehicleCount, m_sVehicleType, 
			OVT_Global.GetTowns().GetNearestTownName(deploymentPos)), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected SCR_AIGroup SpawnCrewForVehicle(Vehicle vehicle, ResourceName crewPrefab, int factionIndex)
	{
		if (!vehicle || crewPrefab.IsEmpty())
			return null;
			
		// Spawn the crew group near the vehicle
		vector vehiclePos = vehicle.GetOrigin();
		vector spawnPos = vehiclePos + Vector(5, 0, 0); // Spawn crew 5m away
		
		SCR_AIGroup crewGroup = OVT_EntitySpawningAPI.SpawnInfantryGroup(
			crewPrefab, 
			spawnPos, 
			"0 0 0", 
			factionIndex
		);
		
		if (!crewGroup)
		{
			Print("Failed to spawn crew group", LogLevel.ERROR);
			return null;
		}
		
		// Store the pending assignment - crew will be assigned when group is initialized
		m_mPendingCrewAssignments.Insert(crewGroup.GetID(), vehicle.GetID());
		
		// Subscribe to group initialization to assign crew when agents are spawned
		crewGroup.GetOnInit().Insert(OnCrewGroupInitialized);
		
		return crewGroup;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnCrewGroupInitialized(SCR_AIGroup group)
	{
		if (!group)
			return;
			
		// Find the vehicle for this group
		EntityID groupID = group.GetID();
		EntityID vehicleID = m_mPendingCrewAssignments.Get(groupID);
		
		if (!vehicleID)
		{
			Print("No pending vehicle assignment found for group", LogLevel.WARNING);
			return;
		}
		
		Vehicle vehicle = Vehicle.Cast(GetGame().GetWorld().FindEntityByID(vehicleID));
		if (!vehicle)
		{
			Print("Vehicle no longer exists for crew assignment", LogLevel.WARNING);
			m_mPendingCrewAssignments.Remove(groupID);
			return;
		}
		
		// Get vehicle compartment manager
		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);
		
		if (!compartmentManager)
		{
			Print("Vehicle has no compartment manager", LogLevel.ERROR);
			m_mPendingCrewAssignments.Remove(groupID);
			return;
		}
		
		// Get crew members
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		
		if (agents.IsEmpty())
		{
			Print("No agents found in crew group", LogLevel.WARNING);
			m_mPendingCrewAssignments.Remove(groupID);
			return;
		}
		
		int agentIndex = 0;
		
		// Assign driver first (pilot compartment)
		if (agentIndex < agents.Count())
		{
			if (FillCompartment(compartmentManager, agents[agentIndex], ECompartmentType.PILOT))
			{
				Print(string.Format("Assigned agent %1 as driver", agentIndex), LogLevel.VERBOSE);
				agentIndex++;
			}
		}
		
		// Assign turret gunners next
		while (agentIndex < agents.Count())
		{
			if (FillCompartment(compartmentManager, agents[agentIndex], ECompartmentType.TURRET))
			{
				Print(string.Format("Assigned agent %1 to turret", agentIndex), LogLevel.VERBOSE);
				agentIndex++;
			}
			else
			{
				break; // No more turret positions available
			}
		}
		
		// Fill remaining seats with cargo (passengers)
		while (agentIndex < agents.Count())
		{
			if (FillCompartment(compartmentManager, agents[agentIndex], ECompartmentType.CARGO))
			{
				Print(string.Format("Assigned agent %1 as passenger", agentIndex), LogLevel.VERBOSE);
				agentIndex++;
			}
			else
			{
				break; // No more cargo positions available
			}
		}
		
		// Remove from pending assignments
		m_mPendingCrewAssignments.Remove(groupID);
		
		Print(string.Format("Crew assignment complete: %1/%2 agents assigned to vehicle", 
			agentIndex, agents.Count()), LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool FillCompartment(SCR_BaseCompartmentManagerComponent compartmentManager, AIAgent agent, ECompartmentType type)
	{
		if (!agent || !agent.GetControlledEntity())
			return false;
			
		IEntity character = agent.GetControlledEntity();
		
		// Get the character's compartment access component
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;
		
		// Get the vehicle entity from the compartment manager
		IEntity vehicle = compartmentManager.GetOwner();
		if (!vehicle)
			return false;
		
		// Move the character into the vehicle with the specified compartment type
		return access.MoveInVehicle(vehicle, type);
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector GetRandomSpawnPosition(vector center)
	{	
		return OVT_Global.FindNearestRoad(center);
	}
	
	//------------------------------------------------------------------------------------------------
	override protected bool CheckIfUnitsEliminated()
	{
		// Clean up destroyed vehicles first
		for (int i = m_aSpawnedVehicles.Count() - 1; i >= 0; i--)
		{
			Vehicle vehicle = m_aSpawnedVehicles[i];
			if (!vehicle || !IsVehicleOperational(vehicle))
			{
				m_aSpawnedVehicles.Remove(i);
			}
		}
		
		// Clean up dead groups
		for (int i = m_aSpawnedGroups.Count() - 1; i >= 0; i--)
		{
			SCR_AIGroup group = m_aSpawnedGroups[i];
			if (!group || group.GetAgentsCount() == 0)
			{
				m_aSpawnedGroups.Remove(i);
			}
		}
		
		// Return true if all vehicles have been eliminated (spawned some but none operational)
		return m_aSpawnedVehicles.IsEmpty() && m_iSpawnedCount > 0;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CheckVehicleStatus()
	{
		// Use the base class elimination checking
		bool wasEliminated = m_bSpawnedUnitsEliminated;
		bool isNowEliminated = CheckIfUnitsEliminated();
		
		// Update elimination state if changed
		if (!wasEliminated && isNowEliminated)
		{
			m_bSpawnedUnitsEliminated = true;
			Print(string.Format("All vehicles for deployment '%1' have been eliminated", m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
			
			// Check if ALL spawning modules in this deployment have been eliminated
			if (m_ParentDeployment)
				m_ParentDeployment.CheckAllSpawningModulesEliminated();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsVehicleOperational(Vehicle vehicle)
	{
		if (!vehicle)
			return false;
			
		// Check if vehicle is destroyed
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
		if (damageManager && damageManager.IsDestroyed())
			return false;
					
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Faction vehicle registry integration
	//------------------------------------------------------------------------------------------------
	protected ResourceName GetVehiclePrefabFromFaction(int factionIndex)
	{
		// Get the OVT_Faction directly by index
		OVT_Faction ovtFaction = OVT_Global.GetFactions().GetOverthrowFactionByIndex(factionIndex);
		if (!ovtFaction)
		{
			Print(string.Format("Failed to get OVT_Faction for index %1", factionIndex), LogLevel.ERROR);
			return "";
		}
		
		// Initialize vehicle registry if needed
		ovtFaction.InitializeVehicleRegistry();
		
		// Get the vehicle prefab from the registry
		ResourceName vehiclePrefab = ovtFaction.GetVehiclePrefabByName(m_sVehicleType);
		if (vehiclePrefab.IsEmpty())
		{
			Print(string.Format("Vehicle type '%1' not found in faction '%2' registry", m_sVehicleType, ovtFaction.GetFactionKey()), LogLevel.WARNING);
		}
		
		return vehiclePrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	protected ResourceName GetGroupPrefabFromFaction(int factionIndex)
	{
		// Get the OVT_Faction directly by index
		OVT_Faction ovtFaction = OVT_Global.GetFactions().GetOverthrowFactionByIndex(factionIndex);
		if (!ovtFaction)
		{
			Print(string.Format("Failed to get OVT_Faction for index %1", factionIndex), LogLevel.ERROR);
			return "";
		}
		
		// Initialize group registry if needed
		ovtFaction.InitializeGroupRegistry();
		
		// Get the group prefab from the registry
		ResourceName groupPrefab = ovtFaction.GetGroupPrefabByName(m_sCrewGroupType);
		if (groupPrefab.IsEmpty())
		{
			Print(string.Format("Group type '%1' not found in faction '%2' registry", m_sCrewGroupType, ovtFaction.GetFactionKey()), LogLevel.WARNING);
		}
		
		return groupPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	// Status methods for behavior modules
	//------------------------------------------------------------------------------------------------
	int GetOperationalVehicleCount()
	{
		int count = 0;
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle && IsVehicleOperational(vehicle))
				count++;
		}
		return count;
	}
	
	//------------------------------------------------------------------------------------------------
	array<Vehicle> GetOperationalVehicles()
	{
		array<Vehicle> vehicles = new array<Vehicle>;
		foreach (Vehicle vehicle : m_aSpawnedVehicles)
		{
			if (vehicle && IsVehicleOperational(vehicle))
				vehicles.Insert(vehicle);
		}
		return vehicles;
	}
	
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Vehicle Module: %1", m_sModuleName));
		Print(string.Format("  Vehicles: %1/%2 operational", GetOperationalVehicleCount(), m_iVehicleCount));
		Print(string.Format("  Vehicle Type: %1", m_sVehicleType));
		Print(string.Format("  Crew Type: %1", m_sCrewGroupType));
		string reinforcementStatus = "Disabled";
		if (m_bAllowReinforcement)
			reinforcementStatus = "Enabled";
		Print(string.Format("  Reinforcement: %1", reinforcementStatus));
	}
}