//! Reinforcement behavior module for deployments
//! Handles decision-making about when and how to reinforce spawned units
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ReinforcementBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;
	
	[Attribute(defvalue: "true", desc: "Enable reinforcement")]
	bool m_bEnableReinforcement;
	
	[Attribute(defvalue: "60000", desc: "Check interval in milliseconds")]
	float m_fCheckInterval;
	
	
	[Attribute(defvalue: "300000", desc: "Delay after deployment activation before reinforcement is allowed (milliseconds)")]
	float m_fInitialDelay;
	
	[Attribute(defvalue: "120000", desc: "Minimum time between reinforcement attempts (milliseconds)")]
	float m_fReinforcementCooldown;
	
	[Attribute(defvalue: "false", desc: "Require town size > 1 for reinforcement (villages don't get reinforced)")]
	bool m_bRequireTownSizeCheck;
	
	[Attribute(defvalue: "false", desc: "Delete deployment if conditions are no longer met")]
	bool m_bDeleteOnConditionFail;
	
	protected float m_fLastCheckTime;
	protected float m_fLastReinforcementTime;
	protected float m_fActivationTime;
	
	//------------------------------------------------------------------------------------------------
	void OVT_ReinforcementBehaviorDeploymentModule()
	{
		m_fLastCheckTime = 0;
		m_fLastReinforcementTime = 0;
		m_fActivationTime = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		m_fActivationTime = GetGame().GetWorld().GetWorldTime();
		m_fLastCheckTime = m_fActivationTime;
		m_fLastReinforcementTime = 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		if (!m_bEnableReinforcement)
			return;
		
		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		// Check if enough time has passed since last check
		if (currentTime - m_fLastCheckTime < m_fCheckInterval)
			return;
		
		m_fLastCheckTime = currentTime;
		
		// Check if initial delay has passed
		if (currentTime - m_fActivationTime < m_fInitialDelay)
			return;
		
		// Check if cooldown has passed since last reinforcement
		if (m_fLastReinforcementTime > 0 && currentTime - m_fLastReinforcementTime < m_fReinforcementCooldown)
			return;
		
		CheckReinforcement();
	}
	
	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ReinforcementBehaviorDeploymentModule clone = new OVT_ReinforcementBehaviorDeploymentModule();
		
		// Copy configuration
		clone.m_sModuleName = m_sModuleName;
		clone.m_bEnableReinforcement = m_bEnableReinforcement;
		clone.m_fCheckInterval = m_fCheckInterval;
		clone.m_fInitialDelay = m_fInitialDelay;
		clone.m_fReinforcementCooldown = m_fReinforcementCooldown;
		clone.m_bRequireTownSizeCheck = m_bRequireTownSizeCheck;
		clone.m_bDeleteOnConditionFail = m_bDeleteOnConditionFail;
		
		return clone;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CheckReinforcement()
	{
		if (!m_ParentDeployment)
			return;
				
		// Get all spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules =  m_ParentDeployment.GetSpawningModules();
		
		if (spawningModules.IsEmpty())
		{
			Print("Reinforcement behavior: No spawning modules found", LogLevel.WARNING);
			return;
		}
		
		// Check if reinforcement conditions are met
		if (!EvaluateReinforcementConditions())
		{
			if (m_bDeleteOnConditionFail)
			{
				Print("Reinforcement behavior: Conditions no longer met, requesting deployment deletion", LogLevel.NORMAL);
				RequestDeploymentDeletion();
			}
			return;
		}
		
		// Check each spawning module for reinforcement needs
		bool anyReinforced = false;
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (ShouldReinforceModule(spawningModule))
			{
				int missingUnits = GetMissingUnitsCount(spawningModule);
				if (missingUnits > 0 && CanAffordReinforcement(spawningModule, missingUnits))
				{
					if (TryReinforceModule(spawningModule, missingUnits))
					{
						anyReinforced = true;
					}
				}
			}
		}
		
		if (anyReinforced)
		{
			m_fLastReinforcementTime = GetGame().GetWorld().GetWorldTime();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool EvaluateReinforcementConditions()
	{		
		// Check town size requirement if enabled
		if (m_bRequireTownSizeCheck)
		{
			OVT_TownConditionalDeploymentModule townCondition = OVT_TownConditionalDeploymentModule.Cast(
				m_ParentDeployment.GetModule(OVT_TownConditionalDeploymentModule)
			);
			
			if (townCondition)
			{
				// This is a town deployment, check town size
				OVT_TownData nearestTown = townCondition.GetNearestTown();
				if (!nearestTown || nearestTown.size <= 1)
				{
					return false;
				}
			}
		}
		
		// Check all condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = m_ParentDeployment.GetConditionModules();
		
		foreach (OVT_BaseConditionDeploymentModule conditionModule : conditionModules)
		{
			if (!conditionModule.EvaluateCondition())
			{
				Print(string.Format("Reinforcement denied: Condition module %1 failed", conditionModule.Type().ToString()), LogLevel.VERBOSE);
				return false;
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool ShouldReinforceModule(OVT_BaseSpawningDeploymentModule spawningModule)
	{
		// Only reinforce if the spawning module is completely eliminated
		return spawningModule.AreSpawnedUnitsEliminated();
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetMissingUnitsCount(OVT_BaseSpawningDeploymentModule spawningModule)
	{
		// When reinforcing eliminated units, we need to reinforce the full capacity
		OVT_InfantrySpawningDeploymentModule infantryModule = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
		if (infantryModule)
		{
			return infantryModule.GetMaxGroupCount();
		}
		
		// For other spawning module types, add support here
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool CanAffordReinforcement(OVT_BaseSpawningDeploymentModule spawningModule, int unitsNeeded)
	{
		OVT_InfantrySpawningDeploymentModule infantryModule = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
		if (infantryModule)
		{
			return infantryModule.CanReinforce(unitsNeeded);
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool TryReinforceModule(OVT_BaseSpawningDeploymentModule spawningModule, int unitsNeeded)
	{
		OVT_InfantrySpawningDeploymentModule infantryModule = OVT_InfantrySpawningDeploymentModule.Cast(spawningModule);
		if (infantryModule)
		{
			bool success = infantryModule.Reinforce(unitsNeeded);
			if (success)
			{
				Print(string.Format("Reinforcement behavior: Successfully reinforced %1 with %2 groups", 
					infantryModule.Type().ToString(), unitsNeeded), LogLevel.NORMAL);
			}
			else
			{
				Print(string.Format("Reinforcement behavior: Failed to reinforce %1", 
					infantryModule.Type().ToString()), LogLevel.WARNING);
			}
			return success;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RequestDeploymentDeletion()
	{
		if (!m_ParentDeployment)
			return;
		
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
		{
			Print(string.Format("Requesting deletion of deployment %1 due to failed conditions", 
				m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
			manager.DeleteDeployment(m_ParentDeployment);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug methods
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Reinforcement Behavior Module: %1", m_sModuleName));
		
		string enabled = "No";
		if (m_bEnableReinforcement)
			enabled = "Yes";
		Print(string.Format("  Enabled: %1", enabled));
		Print(string.Format("  Check Interval: %1s", m_fCheckInterval));
		Print("  Reinforcement Trigger: Only when units eliminated");
		
		string townSizeCheck = "No";
		if (m_bRequireTownSizeCheck)
			townSizeCheck = "Yes";
		Print(string.Format("  Town Size Check: %1", townSizeCheck));
		
		string deleteOnFail = "No";
		if (m_bDeleteOnConditionFail)
			deleteOnFail = "Yes";
		Print(string.Format("  Delete on Fail: %1", deleteOnFail));
		
		float currentTime = GetGame().GetWorld().GetWorldTime();
		Print(string.Format("  Time since activation: %1s", currentTime - m_fActivationTime));
		
		if (m_fLastReinforcementTime > 0)
		{
			Print(string.Format("  Time since last reinforcement: %1s", currentTime - m_fLastReinforcementTime));
		}
		else
		{
			Print("  No reinforcements yet");
		}
	}
}