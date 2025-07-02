[EntityEditorProps(category: "Overthrow/Deployments", description: "Deployment instance component")]
class OVT_DeploymentComponentClass : OVT_ComponentClass
{
}

class OVT_DeploymentComponent : OVT_Component
{
	[Attribute()]
	ref OVT_DeploymentConfig m_DeploymentConfig;
	
	protected ref array<ref OVT_BaseDeploymentModule> m_aActiveModules;
	protected int m_iControllingFaction;
	protected float m_fThreatLevel;
	protected int m_iResourcesInvested;
	protected bool m_bActive;
	protected vector m_vPosition;
	protected bool m_bSpawnedUnitsEliminated; // Flag to track if spawned units have been eliminated
	
	static const int UPDATE_FREQUENCY = 10000; // 10 seconds
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
			
		m_aActiveModules = new array<ref OVT_BaseDeploymentModule>;
		m_vPosition = owner.GetOrigin();
	}
	
	//------------------------------------------------------------------------------------------------
	void InitializeDeployment(OVT_DeploymentConfig config, int factionIndex)
	{
		if (!config)
			return;
			
		m_DeploymentConfig = config;
		m_iControllingFaction = factionIndex;
		
		// Register with manager
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
			manager.RegisterDeployment(this);
		
		// Create module instances from config
		foreach (OVT_BaseDeploymentModule moduleTemplate : config.m_aModules)
		{
			OVT_BaseDeploymentModule module = OVT_BaseDeploymentModule.Cast(moduleTemplate.CloneModule());
			if (module)
			{
				AddModule(module);
			}
		}
		
		// If this deployment was loaded with spawned units eliminated, set all spawning modules as eliminated
		if (m_bSpawnedUnitsEliminated)
		{
			array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
			foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
			{
				spawningModule.SetSpawnedUnitsEliminated(true);
				Print(string.Format("Set spawning module as eliminated on load for deployment '%1'", GetDeploymentName()), LogLevel.VERBOSE);
			}
		}
		
		// Start update loop
		float mul = s_AIRandomGenerator.RandFloatXY(0.8, 1.2);
		int frequency = (int)((float)UPDATE_FREQUENCY * mul); //Stagger these updates
		GetGame().GetCallqueue().CallLater(UpdateDeployment, frequency, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void ActivateDeployment()
	{
		if (m_bActive)
			return;
			
		m_bActive = true;
		
		// Activate modules in the correct order: spawning → behavior → condition
		
		// First: Activate spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			module.Activate();
		}
		
		// Second: Activate behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			module.Activate();
		}
		
		// Third: Activate condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			module.Activate();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void DeactivateDeployment()
	{
		if (!m_bActive)
			return;
			
		m_bActive = false;
		
		// Deactivate modules in reverse order: condition → behavior → spawning
		
		// First: Deactivate condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			module.Deactivate();
		}
		
		// Second: Deactivate behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			module.Deactivate();
		}
		
		// Third: Deactivate spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			module.Deactivate();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UpdateDeployment()
	{
		if (!Replication.IsServer())
			return;
			
		// Safety check to prevent crashes during cleanup
		if (!m_aActiveModules)
			return;
			
		// Update modules in order: spawning → behavior → condition
		
		// First: Update spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (module)
				module.Update(UPDATE_FREQUENCY);
		}
		
		// Second: Update behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			if (module)
				module.Update(UPDATE_FREQUENCY);
		}
		
		// Third: Update condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			if (module)
				module.Update(UPDATE_FREQUENCY);
		}
		
		if(!m_DeploymentConfig.m_bEnableProximityActivation)
		{
			if(!m_bActive)
			{
				ActivateDeployment();				
			}
			return;
		}
		
		// Check proximity for activation (will be replaced by virtualization system)
		bool shouldBeActive = IsPlayerInRange();
		
		if (m_bActive && !shouldBeActive)
		{
			DeactivateDeployment();
		}
		else if (!m_bActive && shouldBeActive)
		{
			ActivateDeployment();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool IsPlayerInRange()
	{
		return OVT_Global.PlayerInRange(GetOwner().GetOrigin(), OVT_Global.GetConfig().m_iMilitarySpawnDistance);
	}
	
	//------------------------------------------------------------------------------------------------
	void ReinforceDeployment(int additionalResources)
	{
		m_iResourcesInvested += additionalResources;
		
		// Distribute resources to spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		if (spawningModules.IsEmpty())
			return;
			
		int resourcesPerModule = additionalResources / spawningModules.Count();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			// TODO: Implement reinforcement logic
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void DestroyDeployment()
	{
		// Stop update loop
		GetGame().GetCallqueue().Remove(UpdateDeployment);
		
		// Cleanup modules in reverse order: condition → behavior → spawning
		
		// First: Cleanup condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			module.Cleanup();
		}
		
		// Second: Cleanup behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			module.Cleanup();
		}
		
		// Third: Cleanup spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			module.Cleanup();
		}
		
		m_aActiveModules.Clear();
		
		// Unregister from manager
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
			manager.UnregisterDeployment(this);
			
		// Delete the entity
		delete GetOwner();
	}
	
	//------------------------------------------------------------------------------------------------
	// Module management
	//------------------------------------------------------------------------------------------------
	void AddModule(OVT_BaseDeploymentModule module)
	{
		if (!module)
			return;
			
		m_aActiveModules.Insert(module);
		module.Initialize(this);
	}
	
	//------------------------------------------------------------------------------------------------
	void RemoveModule(typename moduleType)
	{
		for (int i = m_aActiveModules.Count() - 1; i >= 0; i--)
		{
			if (m_aActiveModules[i].Type() == moduleType)
			{
				m_aActiveModules[i].Cleanup();
				m_aActiveModules.Remove(i);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_BaseDeploymentModule GetModule(typename moduleType)
	{
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			if (module.Type() == moduleType)
				return module;
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	// Module type queries
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseConditionDeploymentModule> GetConditionModules()
	{
		array<OVT_BaseConditionDeploymentModule> modules = new array<OVT_BaseConditionDeploymentModule>;
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(module);
			if (conditionModule)
				modules.Insert(conditionModule);
		}
		return modules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseSpawningDeploymentModule> GetSpawningModules()
	{
		array<OVT_BaseSpawningDeploymentModule> modules = new array<OVT_BaseSpawningDeploymentModule>;
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			OVT_BaseSpawningDeploymentModule spawningModule = OVT_BaseSpawningDeploymentModule.Cast(module);
			if (spawningModule)
				modules.Insert(spawningModule);
		}
		return modules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseBehaviorDeploymentModule> GetBehaviorModules()
	{
		array<OVT_BaseBehaviorDeploymentModule> modules = new array<OVT_BaseBehaviorDeploymentModule>;
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			OVT_BaseBehaviorDeploymentModule behaviorModule = OVT_BaseBehaviorDeploymentModule.Cast(module);
			if (behaviorModule)
				modules.Insert(behaviorModule);
		}
		return modules;
	}
	
	//------------------------------------------------------------------------------------------------
	// Used by deployment manager to check if deployment should be created
	static bool CheckDeploymentConditions(OVT_DeploymentConfig config, vector position, int factionIndex, float threatLevel)
	{
		if (!config)
			return false;
			
		// Check threat level requirement
		if (config.m_iMinimumThreatLevel > 0 && threatLevel < config.m_iMinimumThreatLevel)
			return false;
		
		// Check module-specific conditions
		foreach (OVT_BaseDeploymentModule moduleTemplate : config.m_aModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(moduleTemplate);
			if (conditionModule && !conditionModule.EvaluateStaticCondition(position, factionIndex, threatLevel))
				return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static OVT_FactionType GetFactionType(Faction faction)
	{
		// Determine faction type based on faction key
		string factionKey = faction.GetFactionKey();
		
		if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
			return OVT_FactionType.OCCUPYING_FACTION;
		else if (factionKey == OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey())
			return OVT_FactionType.RESISTANCE_FACTION;
		else if (factionKey == OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey())
			return OVT_FactionType.SUPPORTING_FACTION;
		
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	// Getters
	//------------------------------------------------------------------------------------------------
	int GetControllingFaction() { return m_iControllingFaction; }
	float GetThreatLevel() { return m_fThreatLevel; }
	int GetResourcesInvested() { return m_iResourcesInvested; }
	bool IsDeploymentActive() { return m_bActive; }
	vector GetPosition() { return GetOwner().GetOrigin(); }
	OVT_DeploymentConfig GetConfig() { return m_DeploymentConfig; }
	string GetDeploymentName() 
	{ 
		if (m_DeploymentConfig)
			return m_DeploymentConfig.m_sDeploymentName;
		return "Unknown Deployment";
	}
	
	//------------------------------------------------------------------------------------------------
	void SetThreatLevel(float threat) { m_fThreatLevel = threat; }
	void SetControllingFaction(int factionIndex) { m_iControllingFaction = factionIndex; }
	
	//------------------------------------------------------------------------------------------------
	void CheckAllSpawningModulesEliminated()
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		if (spawningModules.IsEmpty())
		{
			// No spawning modules, so nothing to eliminate
			m_bSpawnedUnitsEliminated = false;
			return;
		}
		
		// Check if ALL spawning modules have been eliminated
		bool allEliminated = true;
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (!module.AreSpawnedUnitsEliminated())
			{
				allEliminated = false;
				break;
			}
		}
		
		bool previousState = m_bSpawnedUnitsEliminated;
		m_bSpawnedUnitsEliminated = allEliminated;
		
		// Log state change
		if (previousState != m_bSpawnedUnitsEliminated)
		{
			if (m_bSpawnedUnitsEliminated)
				Print(string.Format("All spawned units for deployment '%1' have been eliminated", GetDeploymentName()), LogLevel.NORMAL);
			else
				Print(string.Format("Spawned units for deployment '%1' are no longer eliminated (reinforcements successful)", GetDeploymentName()), LogLevel.NORMAL);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	bool GetSpawnedUnitsEliminated() { return m_bSpawnedUnitsEliminated; }
	void SetSpawnedUnitsEliminated(bool eliminated) { m_bSpawnedUnitsEliminated = eliminated; }
}

// EPF Save Data

[BaseContainerProps()]
class OVT_DeploymentSaveDataClass : EPF_ItemSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_DeploymentSaveData : EPF_ItemSaveData
{
};


[EPF_ComponentSaveDataType(OVT_DeploymentComponent), BaseContainerProps()]
class OVT_DeploymentComponentSaveDataClass : EPF_ComponentSaveDataClass
{
}

[EDF_DbName.Automatic()]
class OVT_DeploymentComponentSaveData : EPF_ComponentSaveData
{
	int m_iControllingFaction;
	float m_fThreatLevel;
	int m_iResourcesInvested;
	string m_sDeploymentConfigName;
	bool m_bSpawnedUnitsEliminated;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(component);
		if (!deployment)
			return EPF_EReadResult.ERROR;
			
		m_iControllingFaction = deployment.GetControllingFaction();
		m_fThreatLevel = deployment.GetThreatLevel();
		m_iResourcesInvested = deployment.GetResourcesInvested();
		m_bSpawnedUnitsEliminated = deployment.GetSpawnedUnitsEliminated();
		
		// Save config name for restoration
		if (deployment.GetConfig())
			m_sDeploymentConfigName = deployment.GetConfig().m_sDeploymentName;
			
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(component);
		if (!deployment)
			return EPF_EApplyResult.ERROR;
			
		// Restore config by looking it up in the deployment registry
		if (!m_sDeploymentConfigName.IsEmpty())
		{
			OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
			if (manager && manager.m_DeploymentRegistry)
			{
				OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(m_sDeploymentConfigName);
				if (config)
				{
					deployment.InitializeDeployment(config, m_iControllingFaction);
				}
			}
		}
		
		deployment.SetThreatLevel(m_fThreatLevel);
		deployment.SetControllingFaction(m_iControllingFaction);
		deployment.SetSpawnedUnitsEliminated(m_bSpawnedUnitsEliminated);
					
		return EPF_EApplyResult.OK;
	}
}