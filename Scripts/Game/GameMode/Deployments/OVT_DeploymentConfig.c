// Location type flags for deployments
enum OVT_LocationTypeFlag
{
	TOWN = 1,
	BASE = 2,
	PORT = 4,
	AIRFIELD = 8,
	RADIO_TOWER = 16,
	CHECKPOINT = 32,
	OPEN_TERRAIN = 64
}

[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sDeploymentName")]
class OVT_DeploymentConfig : ScriptAndConfig
{
	[Attribute(desc: "Name of this deployment type")]
	string m_sDeploymentName;
	
	[Attribute(desc: "Modules that make up this deployment")]
	ref array<ref OVT_BaseDeploymentModule> m_aModules;
	
	[Attribute("1", UIWidgets.Flags, enums: ParamEnumArray.FromEnum(OVT_FactionTypeFlag))]
	OVT_FactionTypeFlag m_iAllowedFactionTypes;
	
	[Attribute("1", UIWidgets.Flags, enums: ParamEnumArray.FromEnum(OVT_LocationTypeFlag), desc: "Valid location types for this deployment")]
	OVT_LocationTypeFlag m_iAllowedLocationTypes;
	
	[Attribute(defvalue: "100", desc: "Base resource cost to create this deployment")]
	int m_iBaseCost;
	
	[Attribute(defvalue: "0", desc: "Minimum threat level required")]
	int m_iMinimumThreatLevel;
	
	[Attribute(defvalue: "5", desc: "Priority (1-20, lower = higher priority)")]
	int m_iPriority;
	
	[Attribute(defvalue: "1000", desc: "NO LONGER DRIVES GROUP LIFECYCLE - a deployment's groups are registered with the virtualization core and the engine materialises them by observer proximity. Retained so authored configs keep parsing")]
	float m_fActivationRange;

	[Attribute(defvalue: "1", desc: "NO LONGER DRIVES GROUP LIFECYCLE - every deployment activates once and stays active; its groups are spawned and despawned by the engine's observer proximity, never by this flag. Retained so authored configs keep parsing")]
	bool m_bEnableProximityActivation;
	
	[Attribute(defvalue: "-1", desc: "Resource allocation limit (-1 = no limit)")]
	int m_iResourceAllocation;
	
	[Attribute(defvalue: "100", desc: "Chance this deployment will be created (0-100, where 100 = 100% chance)")]
	float m_fChance;
	
	[Attribute(defvalue: "-1", desc: "Maximum number of active instances of this deployment type (-1 = no limit)")]
	int m_iMaxInstances;

	[Attribute(defvalue: "0", desc: "Seeded free of charge at campaign start at every eligible location (bypasses the resource pool, the player-count guard and the QRF guard; dedup and MaxInstances still apply)")]
	bool m_bFreeAtGameStart;

	[Attribute(defvalue: "0", desc: "Only a deliberate caller may create this - the objective director's own operations. The 30 s evaluator will never pick it as a candidate and the free-at-game-start pass will never seed it, whatever its location mask, priority or cost say. ForceCreateDeployment() is unaffected")]
	bool m_bDirectorOnly;

	//------------------------------------------------------------------------------------------------
	void OVT_DeploymentConfig()
	{
		if (!m_aModules)
			m_aModules = new array<ref OVT_BaseDeploymentModule>;
			
		// Set default to allow only occupying faction
		if (m_iAllowedFactionTypes == 0)
			m_iAllowedFactionTypes = 1;
	}
	
	//------------------------------------------------------------------------------------------------
	// Validation and utility methods
	//------------------------------------------------------------------------------------------------
	bool IsValidConfig()
	{
		if (m_sDeploymentName.IsEmpty())
			return false;
			
		if (!m_aModules || m_aModules.IsEmpty())
			return false;
			
		// Check that we have at least one spawning module
		bool hasSpawningModule = false;
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			if (OVT_BaseSpawningDeploymentModule.Cast(module))
			{
				hasSpawningModule = true;
				break;
			}
		}
		
		if (!hasSpawningModule)
		{
			Print(string.Format("Deployment config '%1' has no spawning modules", m_sDeploymentName), LogLevel.WARNING);
			return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Whether the ORDINARY EVALUATOR may pick this config out of the registry on its own initiative.
	//!
	//! 🔴 WHY THIS EXISTS AT ALL, AND WHY NEITHER EXISTING MASK COULD SAY IT (occupying/counter-attacks,
	//! 2026-08-19 play-test). The objective director's operations are registered configs like any other,
	//! because everything else in the framework - the cost model, the Game Master panel, the
	//! reinforcement rebuy, the save - reads a CONFIG. That made them candidates for
	//! FindBestDeploymentConfig(), and a play-test caught the consequence: an "Objective Harassment
	//! (Patrol)" deployment created at a town by the 30 s evaluator, with no director line above it. It
	//! stood at the flag doing nothing (it was not the director's operation, and its objective condition
	//! had never made it one) and the faction pool was charged for it outside the director's accounting,
	//! which is exactly the single-decision-maker property (G1) the director exists to hold.
	//!
	//! ⚠ NEITHER m_iAllowedFactionTypes NOR m_iAllowedLocationTypes CAN EXPRESS THIS. Every objective
	//! config is legitimately an OCCUPYING_FACTION config sent to a TOWN, a BASE or a RADIO_TOWER - that
	//! is where the director sends it - and a zero location mask means "no restrictions", not "nowhere".
	//! An explicitly named opt-out is the only honest way to say "a caller has to mean it".
	//!
	//! ⚠ IT IS NOT A BAN ON CREATION. ForceCreateDeployment() does not consult this and must not: it is
	//! the director's own door, and closing it would leave the ramp unable to send anything at all.
	//! \return True when the evaluator and the free-at-game-start pass may consider this config.
	bool IsSelectableByEvaluator()
	{
		return !m_bDirectorOnly;
	}

	//------------------------------------------------------------------------------------------------
	bool CanFactionUse(OVT_FactionTypeFlag factionType)
	{
		if (m_iAllowedFactionTypes == 7)
			return true; // No restrictions
			
		return (factionType & m_iAllowedFactionTypes) != 0;
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanUseLocationType(OVT_LocationTypeFlag locationType)
	{
		if (m_iAllowedLocationTypes == 0)
			return true; // No restrictions if not set
			
		return (locationType & m_iAllowedLocationTypes) != 0;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetTotalResourceCost(int difficultyMultiplier = 1)
	{
		int totalCost = m_iBaseCost * difficultyMultiplier;
		
		// Add module-specific costs
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			totalCost += module.GetResourceCost();
		}
		
		return totalCost;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseSpawningDeploymentModule> GetSpawningModules()
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = new array<OVT_BaseSpawningDeploymentModule>;
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			OVT_BaseSpawningDeploymentModule spawningModule = OVT_BaseSpawningDeploymentModule.Cast(module);
			if (spawningModule)
				spawningModules.Insert(spawningModule);
		}
		
		return spawningModules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseBehaviorDeploymentModule> GetBehaviorModules()
	{
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = new array<OVT_BaseBehaviorDeploymentModule>;
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			OVT_BaseBehaviorDeploymentModule behaviorModule = OVT_BaseBehaviorDeploymentModule.Cast(module);
			if (behaviorModule)
				behaviorModules.Insert(behaviorModule);
		}
		
		return behaviorModules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseConditionDeploymentModule> GetConditionModules()
	{
		array<OVT_BaseConditionDeploymentModule> conditionModules = new array<OVT_BaseConditionDeploymentModule>;
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(module);
			if (conditionModule)
				conditionModules.Insert(conditionModule);
		}
		
		return conditionModules;
	}
	
	//------------------------------------------------------------------------------------------------
	bool RequiresSlots()
	{
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			// Check if any spawning modules require specific slots
			OVT_BaseSpawningDeploymentModule spawningModule = OVT_BaseSpawningDeploymentModule.Cast(module);
			if (spawningModule)
			{
				// This would need to be implemented in derived classes
				// For now, assume no slot requirements
			}
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetRequiredSlotType()
	{
		// Return the most restrictive slot type required by any module
		// This would need to be implemented based on specific module requirements
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	// Debug and logging
	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Deployment Config: %1", m_sDeploymentName));
		Print(string.Format("  Base Cost: %1", m_iBaseCost));
		Print(string.Format("  Priority: %1", m_iPriority));
		Print(string.Format("  Min Threat: %1", m_iMinimumThreatLevel));
		Print(string.Format("  Modules: %1", m_aModules.Count()));

		if (m_bFreeAtGameStart)
			Print("  Free at game start: yes (seeded at every eligible location, nothing charged)");

		if (m_bDirectorOnly)
			Print("  Director only: yes (the evaluator will never pick this - only a deliberate ForceCreateDeployment caller)");
		
		foreach (OVT_BaseDeploymentModule module : m_aModules)
		{
			Print(string.Format("    - %1", module.Type().ToString()));
		}
		
		if (m_iAllowedFactionTypes != 0)
		{
			Print("  Allowed Factions:");
			if (m_iAllowedFactionTypes & OVT_FactionTypeFlag.OCCUPYING_FACTION)
				Print("    - Occupying");
			if (m_iAllowedFactionTypes & OVT_FactionTypeFlag.RESISTANCE_FACTION)
				Print("    - Resistance");
			if (m_iAllowedFactionTypes & OVT_FactionTypeFlag.SUPPORTING_FACTION)
				Print("    - Supporting");
		}
	}
}