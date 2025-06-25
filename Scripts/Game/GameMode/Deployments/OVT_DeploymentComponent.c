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
	
	static const int UPDATE_FREQUENCY = 1000; // 1 second
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
			
		m_aActiveModules = new array<ref OVT_BaseDeploymentModule>;
		m_vPosition = owner.GetOrigin();
		
		// Register with manager
		OVT_DeploymentManager manager = OVT_Global.GetDeploymentManager();
		if (manager)
			manager.RegisterDeployment(this);
	}
	
	//------------------------------------------------------------------------------------------------
	void InitializeDeployment(OVT_DeploymentConfig config, int factionIndex)
	{
		if (!config)
			return;
			
		m_DeploymentConfig = config;
		m_iControllingFaction = factionIndex;
		
		// Create module instances from config
		foreach (OVT_BaseDeploymentModule moduleTemplate : config.m_aModules)
		{
			OVT_BaseDeploymentModule module = OVT_BaseDeploymentModule.Cast(moduleTemplate.CloneModule());
			if (module)
			{
				AddModule(module);
			}
		}
		
		// Start update loop
		GetGame().GetCallqueue().CallLater(UpdateDeployment, UPDATE_FREQUENCY, true);
	}
	
	//------------------------------------------------------------------------------------------------
	void ActivateDeployment()
	{
		if (m_bActive)
			return;
			
		m_bActive = true;
		
		// Activate all modules
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
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
		
		// Deactivate all modules
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			module.Deactivate();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UpdateDeployment()
	{
		if (!Replication.IsServer())
			return;
			
		// Update all modules
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			module.Update(UPDATE_FREQUENCY);
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
		// Simple proximity check - will be replaced by virtualization system
		float activationRange = 1000; // Default 1km
		if (m_DeploymentConfig)
			activationRange = m_DeploymentConfig.m_fActivationRange;
			
		array<int> players = new array<int>;
		GetGame().GetPlayerManager().GetPlayers(players);
		
		foreach (int playerId : players)
		{
			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!player)
				continue;
				
			float distance = vector.Distance(player.GetOrigin(), m_vPosition);
			if (distance <= activationRange)
				return true;
		}
		
		return false;
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
		
		// Cleanup all modules
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			module.Cleanup();
		}
		
		m_aActiveModules.Clear();
		
		// Unregister from manager
		OVT_DeploymentManager manager = OVT_Global.GetDeploymentManager();
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
			
		// Check faction type
		if (!config.m_aAllowedFactionTypes.IsEmpty())
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			if (!factionManager)
				return false;
				
			Faction faction = factionManager.GetFactionByIndex(factionIndex);
			if (!faction)
				return false;
				
			string factionType = GetFactionType(faction);
			if (!config.m_aAllowedFactionTypes.Contains(factionType))
				return false;
		}
		
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
	protected static string GetFactionType(Faction faction)
	{
		// Determine faction type based on faction key
		string factionKey = faction.GetFactionKey();
		
		if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
			return "occupying";
		else if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
			return "resistance";
		else if (factionKey == OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey())
			return "supporting";
		
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	// Getters
	//------------------------------------------------------------------------------------------------
	int GetControllingFaction() { return m_iControllingFaction; }
	float GetThreatLevel() { return m_fThreatLevel; }
	int GetResourcesInvested() { return m_iResourcesInvested; }
	bool IsDeploymentActive() { return m_bActive; }
	vector GetPosition() { return m_vPosition; }
	OVT_DeploymentConfig GetConfig() { return m_DeploymentConfig; }
	
	//------------------------------------------------------------------------------------------------
	void SetThreatLevel(float threat) { m_fThreatLevel = threat; }
	void SetControllingFaction(int factionIndex) { m_iControllingFaction = factionIndex; }
}

// EPF Save Data
[EPF_ComponentSaveDataType(OVT_DeploymentComponent), BaseContainerProps()]
class OVT_DeploymentSaveDataClass : EPF_ComponentSaveDataClass
{
}

[EDF_DbName.Automatic()]
class OVT_DeploymentSaveData : EPF_ComponentSaveData
{
	int m_iControllingFaction;
	float m_fThreatLevel;
	int m_iResourcesInvested;
	bool m_bActive;
	string m_sDeploymentConfigName;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_DeploymentComponent deployment = OVT_DeploymentComponent.Cast(component);
		if (!deployment)
			return EPF_EReadResult.ERROR;
			
		m_iControllingFaction = deployment.GetControllingFaction();
		m_fThreatLevel = deployment.GetThreatLevel();
		m_iResourcesInvested = deployment.GetResourcesInvested();
		m_bActive = deployment.IsDeploymentActive();
		
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
			OVT_DeploymentManager manager = OVT_Global.GetDeploymentManager();
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
		
		if (m_bActive)
			deployment.ActivateDeployment();
			
		return EPF_EApplyResult.OK;
	}
}