//------------------------------------------------------------------------------------------------
//! ONE BASE, THREE ROLES: the modder seam of the objective machine.
//!
//! Mirrors OVT_BaseDeploymentModule (Scripts/Game/GameMode/Deployments/Modules/OVT_BaseDeploymentModule.c)
//! one layer up - public wrapper, protected virtual hook, hand-written CloneModule() per concrete
//! class - because a modder who has written a deployment module should not have to learn a second
//! shape to write an objective module.
//!
//! THE THREE ROLES, AND WHAT THE RUNNER DOES WITH EACH:
//!   OVT_BaseObjectiveAbortModule      OR'd.  The first that fires ends the objective; its own
//!                                            attributes say whether the place is blacklisted.
//!   OVT_BaseObjectiveConditionModule  AND'd. All true advances to the next phase. AN EMPTY SET IS
//!                                            TRUE - see OVT_ObjectivePlanRules.AllConditionsMet().
//!   OVT_BaseObjectiveOperationModule  Tried in the authored order once the cadence has elapsed, and
//!                                            THE FIRST THAT ACTS CONSUMES THE CADENCE.
//! They are evaluated in that order within one tick, and a tick that advances a phase runs no
//! operation - which is what the shipped `if (gate) return;` did before there were modules.
//!
//! =====================================================================================
//! 🔴 CloneModule() COPIES ATTRIBUTES BY HAND, IS NOT CHAINED, AND SILENTLY DROPS WHAT IT FORGETS.
//! =====================================================================================
//! There is no reflective copy in this dialect and no override of CopyTo() anywhere in this tree - the
//! deployment base declares one and not a single concrete module uses it (C7), so the pattern that is
//! actually used is the one to mirror: every concrete class overrides CloneModule(), news up its own
//! type, and assigns EVERY attribute it and its parents declare.
//!
//! A DROPPED LINE IS NOT A COMPILE ERROR AND NOT A RUNTIME ERROR. The clone runs with a zero where the
//! author wrote a number: a radius of 0 finds nothing, a cadence of 0 fires every tick, a cap of 0
//! refuses everything. Nothing logs. So EVERY concrete module gets a DEDICATED initialisation-tier
//! case in the OVT_TEST_Init_TowerUnrestRecapture.c:214-259 shape - set every attribute on a template
//! to a DISTINCT non-default value, clone it, and assert each field individually with its own
//! SetFailure naming the field. A single "the clone differs" assertion is not enough, because the
//! whole point is to name which line was dropped.
//!
//! ⚠ MODULES DO NOT SERIALIZE (D9). Serialize/Deserialize are declared here because the requirements
//! ask for them and because a mod may need one, but the base is empty and NO SHIPPED MODULE OVERRIDES
//! THEM. All persisted module state is bag keys on the instance, written by the director's one
//! serializer: one save format, one version number, one place to append. A per-module serializer would
//! reproduce the dropped-line trap in the save layer, where there is no equivalent test.
//!
//! ⚠ THE INSTANCE BACK-REFERENCE IS WEAK. The instance owns the module; a strong reference back would
//! cycle and neither would ever be collected.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseObjectiveModule
{
	[Attribute(desc: "Name of this module. Shown as the entry's title in Workbench and used in log lines; it is NOT an identifier and nothing resolves it")]
	string m_sModuleName;

	//! The objective this module is running for. WEAK - see the class header. Set by Initialize().
	protected OVT_ObjectiveInstance m_Objective;

	//! Whether Initialize() has run. A module asked to act before its phase was entered answers "no"
	//! rather than dereferencing a null objective.
	protected bool m_bInitialized;

	//------------------------------------------------------------------------------------------------
	//! Binds the module to an objective and lets it latch whatever it needs on entry.
	//! \param[in] objective The objective whose phase is being entered.
	void Initialize(OVT_ObjectiveInstance objective)
	{
		m_Objective = objective;
		m_bInitialized = true;

		OnEnter();
	}

	//------------------------------------------------------------------------------------------------
	//! One step, for a module that wants one whatever its role answers. Called once per director tick,
	//! before the role method, and only while the phase is running.
	void Tick()
	{
		if (!m_bInitialized)
			return;

		OnTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the module its phase has ended, whether the objective advanced, aborted or was reset.
	void Exit()
	{
		if (!m_bInitialized)
			return;

		OnExit();

		m_bInitialized = false;
		m_Objective = null;
	}

	//------------------------------------------------------------------------------------------------
	// VIRTUAL HOOKS - override these, never the wrappers above
	//------------------------------------------------------------------------------------------------

	protected void OnEnter() {}
	protected void OnTick() {}
	protected void OnExit() {}

	//------------------------------------------------------------------------------------------------
	//! DECLARED, EMPTY, AND NOT OVERRIDDEN BY ANY SHIPPED MODULE (D9). See the class header.
	//! \param[in] objective The objective whose bag a module would write into.
	void Serialize(notnull OVT_ObjectiveInstance objective) {}

	//------------------------------------------------------------------------------------------------
	//! DECLARED, EMPTY, AND NOT OVERRIDDEN BY ANY SHIPPED MODULE (D9). See the class header.
	//! \param[in] objective The objective whose bag a module would read back.
	void Deserialize(notnull OVT_ObjectiveInstance objective) {}

	//------------------------------------------------------------------------------------------------
	//! A fresh, independent copy of this module.
	//!
	//! 🔴 OVERRIDE THIS IN EVERY CONCRETE CLASS AND COPY EVERY ATTRIBUTE BY HAND, INCLUDING THE ONES
	//! DECLARED BY PARENTS. It is not chained: a subclass that calls nothing of its parent's still has
	//! to assign m_sModuleName itself. See the class header for the case that catches a dropped line.
	//! \return The copy. The base answers null, because a module that forgot to override this must not
	//!         quietly run as an unconfigured base class.
	OVT_BaseObjectiveModule CloneModule()
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefix every bag key this module writes begins with, e.g. "fob" for the forward base.
	//!
	//! ⚠ IT IS A DECLARATION, NOT AN ENFORCEMENT. Nothing rejects a key outside the prefix; the point
	//! is that a reader of a save payload can tell which module owns a key, and that two modules cannot
	//! silently share a counter because they both happened to call it "successes".
	//! \return The prefix, without a trailing dot. An empty string means the module keeps no state.
	string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The objective this module is running for.
	//! \return The instance, or null before Initialize().
	protected OVT_ObjectiveInstance GetObjective()
	{
		return m_Objective;
	}

	//------------------------------------------------------------------------------------------------
	//! The director running this module's objective.
	//! \return The director, or null before Initialize().
	protected OVT_ObjectiveDirectorComponent GetDirector()
	{
		if (!m_Objective)
			return null;

		return m_Objective.GetDirector();
	}

	//! \return True once the module has been bound to an objective.
	bool IsInitialized() { return m_bInitialized; }
}
