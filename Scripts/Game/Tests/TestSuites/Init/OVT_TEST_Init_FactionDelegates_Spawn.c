//------------------------------------------------------------------------------------------------
//! The faction manager spawns one faction delegate per faction (BUG-132).
//!
//! Faction friendly-relations NEVER replicate by themselves: SCR_Faction.InitFactionRelationships
//! is server-gated, SCR_FactionManager's own RplSave carries no relations, and the only channel
//! that ships the friendly matrix to clients (live and JIP) is the per-faction delegate entity's
//! SCR_EditableFactionComponent. The delegates are spawned by SCR_DelegateFactionManagerComponent
//! during the faction manager's EOnInit - so a faction-manager prefab that lacks that component
//! leaves every client with an empty relation table, where every faction is hostile to EVERYTHING
//! INCLUDING ITSELF. Observed in MP as "Hostile" blocking entry to any occupied vehicle.
//!
//! That is exactly how BUG-132 happened: OVT_OverthrowFactionManager.et was hand-authored instead
//! of derived from vanilla FactionManager_Base.et and silently lost the component (the same way it
//! lost its RplComponent - BUG-088). This case pins the repaired prefab at the seam the defect
//! lived at: the delegate manager must resolve, and it must hold exactly one delegate per faction.
//!
//! Init tier because all of it is true at world load - no campaign start involved, and the spawn
//! is synchronous inside the faction manager's init (OnFactionsInit runs before
//! InitFactionRelationships, SCR_FactionManager.c:428-442).
//!
//! PROVEN ABLE TO FAIL (2026-08-09): with the SCR_DelegateFactionManagerComponent block removed
//! from OVT_OverthrowFactionManager.et - the literal BUG-132 defect - the case fails with
//! "SCR_DelegateFactionManagerComponent.GetInstance() is null"; restored, it passes.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_FactionDelegates_Spawn : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			SetFailure("GetGame().GetFactionManager() is null at world load");
			return true;
		}

		int factionCount = factionManager.GetFactionsCount();
		if (factionCount < 1)
		{
			SetFailure("The faction manager holds no factions, so delegate coverage cannot be asserted");
			return true;
		}

		SCR_DelegateFactionManagerComponent delegates = SCR_DelegateFactionManagerComponent.GetInstance();
		if (!delegates)
		{
			SetFailure("SCR_DelegateFactionManagerComponent.GetInstance() is null - the faction manager prefab is missing the component, so faction relations never reach clients (BUG-132)");
			return true;
		}

		int delegateCount = delegates.GetFactionDelegateCount();
		if (delegateCount != factionCount)
		{
			SetFailure("%1 faction delegate(s) exist for %2 faction(s) - every uncovered faction's relations are invisible to clients (BUG-132)",
				delegateCount.ToString(), factionCount.ToString());
			return true;
		}

		PrintFormat("Faction delegates present: %1 delegate(s) for %2 faction(s)",
			delegateCount.ToString(), factionCount.ToString());
		return true;
	}
}
