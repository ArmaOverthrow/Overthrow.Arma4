//------------------------------------------------------------------------------------------------
//! AN OPERATION THAT PUTS SOMETHING IN THE WORLD AND LEAVES IT THERE.
//!
//! Every other operation module buys men who do a job and finish. An ASSET operation builds a thing
//! that OUTLIVES the phase that built it: the forward operating base is raised in the forward-base
//! phase, is still standing all the way through the counter-attack, and is only taken down when the
//! objective ENDS - which can happen from any phase, on a tick where this module is not in the
//! running phase's module set at all.
//!
//! =============================================================================================
//! 🔴 THAT LIFETIME IS THE WHOLE REASON THIS CLASS EXISTS, AND IT IS THE ONE THING A PLAIN
//! OPERATION MODULE CANNOT EXPRESS.
//! =============================================================================================
//! A phase's modules are cloned on entry and dropped on exit (OVT_ObjectivePhase.CloneModules), so a
//! module is only reachable while its own phase is running. Three things about a standing asset have
//! to be answerable outside that window:
//!
//!   1. THE TEARDOWN. ResetObjective() is the ONE path that ends an objective and it is reached from
//!      every phase - a timeout during harassment, a battle resolving in the counter-attack phase, a
//!      player dismantling the flag. It has to be able to reach the doctrine that built the asset.
//!   2. THE SPEND CEILING. The director's one create-then-debit choke point asks "would this spend
//!      take the asset past its budget" for EVERY operation, including the ramp operations that carry
//!      on in a later phase, so the ceiling has to be readable from outside this module's phase.
//!   3. THE PLAYER-FACING RULES. The dismantle action asks whether the asset can be pulled down; that
//!      question is asked on a client, where no objective and no module set exist at all.
//!
//! So an asset module REGISTERS ITSELF WITH THE DIRECTOR on entry
//! (OVT_ObjectiveDirectorComponent.RegisterAssetModule) and the director holds it - by key, strongly -
//! until the objective's record is cleared. The registration is what makes the director's asset API
//! generic: it walks registered modules and never names one.
//!
//! ⚠ IT MUST KEEP WORKING AFTER Exit(). The base class nulls m_Objective when the phase ends, so
//! everything a registered module needs afterwards is cached on entry instead - see m_Asset and
//! m_OwnerDirector. Reading GetObjective() from a teardown is a null dereference waiting for the first
//! objective that ends in a later phase, which is every objective that reaches a battle.
//!
//! ⚠ THE CEILING IS A COUNTER, NEVER A WALLET (G5). GetCeiling()/GetSpent() describe money that has
//! ALREADY left the one deployment pool. Nothing here reserves, holds, refunds or moves a resource,
//! and a grep of this folder for the framework's pool-credit method is an acceptance criterion.
//!
//! See OVT_BaseObjectiveModule for the clone contract, which every concrete asset module owes - and
//! note that m_sAssetKey is one more attribute a CloneModule() has to carry by hand.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseObjectiveAssetModule : OVT_BaseObjectiveOperationModule
{
	[Attribute(defvalue: "fob", desc: "Which asset record this module owns. 'fob' is the forward operating base - the key IsAssetUp()/GetAssetPosition() answer for, and the key the save payload carries. A second asset is a new key, not a new record class")]
	string m_sAssetKey;

	//! The record this module writes. CACHED ON ENTRY AND DELIBERATELY NOT CLEARED BY OnExit(): a
	//! registered module has to be able to answer about its asset after its phase has ended. It is the
	//! objective's own record object, not a copy, so writing it here is what IsAssetUp() reads.
	protected ref OVT_ObjectiveAssetRecord m_Asset;

	//! The director this module registered with. Same lifetime rule as m_Asset, and for the same reason.
	protected OVT_ObjectiveDirectorComponent m_OwnerDirector;

	//------------------------------------------------------------------------------------------------
	//! \return The asset key this module owns.
	string GetAssetKey()
	{
		return m_sAssetKey;
	}

	//------------------------------------------------------------------------------------------------
	//! The record this module owns, valid after entry and after the phase has ended.
	//! \return The record, or null before the module was ever initialised.
	OVT_ObjectiveAssetRecord GetAssetRecord()
	{
		return m_Asset;
	}

	//------------------------------------------------------------------------------------------------
	//! Caches the record and the director, and registers this module as the asset's owner.
	//!
	//! ⚠ CALLED FROM THE CONCRETE MODULE'S OWN OnEnter(), NOT FROM Initialize(). A concrete module may
	//! have latching of its own to do first, and the registration is the last thing that should happen
	//! on entry rather than the first.
	protected void RegisterAsAssetOwner()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective)
			return;

		m_Asset = objective.GetAsset(m_sAssetKey);
		m_OwnerDirector = objective.GetDirector();

		if (m_OwnerDirector)
			m_OwnerDirector.RegisterAssetModule(m_sAssetKey, this);
	}

	//------------------------------------------------------------------------------------------------
	// THE THREE THINGS THE DIRECTOR ASKS A REGISTERED MODULE
	//------------------------------------------------------------------------------------------------

	//! Whether this asset's spend ceiling governs the director's spending right now.
	//!
	//! ⚠ ARMED IS NOT THE SAME AS STANDING. The forward base's ceiling arms the moment its own
	//! deployment is SENT, so the structure's own cost is inside the budget rather than outside it.
	//! \return True while every spend is counted against GetCeiling(). The base answers false.
	bool IsCeilingArmed()
	{
		return false;
	}

	//! The most that may be spent on this asset and everything sourced from it.
	//! \return The ceiling. Zero or below is read by OVT_ObjectivePhaseRules.WithinFOBCeiling() as
	//!         "refuse everything", which is what makes a misauthored cost fail closed.
	int GetCeiling()
	{
		return 0;
	}

	//! \return What has already been spent against that ceiling.
	int GetSpent()
	{
		if (!m_Asset)
			return 0;

		return m_Asset.spent;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts a spend that has ALREADY left the deployment pool against this asset's ceiling.
	//!
	//! ⚠ THIS MOVES NO MONEY. It is called after the pool has been debited, at the director's one
	//! choke point, and its only job is to stop the ramp spending past the ceiling.
	//! \param[in] cost What was spent. Non-positive is ignored.
	void CountSpend(int cost)
	{
		if (cost <= 0 || !m_Asset)
			return;

		m_Asset.spent = m_Asset.spent + cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Take this asset out of the world: its deployments, its structure and its runtime state.
	//!
	//! ⚠ CALLED FROM ResetObjective() AND FROM NOWHERE ELSE, on the one path that ends an objective,
	//! and it must be IDEMPOTENT and safe on an asset that was never raised - it is reached by every
	//! objective that never got out of harassment.
	//!
	//! ⚠ IT MUST NOT ASSUME ITS PHASE IS STILL RUNNING. See the class header.
	void TearDownAsset()
	{
	}
}
