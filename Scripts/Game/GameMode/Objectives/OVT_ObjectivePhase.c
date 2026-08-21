//------------------------------------------------------------------------------------------------
//! ONE PHASE OF A PLAN: a stable name, a module bag, a cadence and an anchor radius.
//!
//! Authored inside an OVT_ObjectiveConfig's m_aPhases[]. Index 0 is entered the moment the plan is
//! committed to; every later index is reached by the phase before it passing all of its condition
//! modules.
//!
//! ⚠ THE NAME IS THE PERSISTENCE KEY AND CHANGING ONE ABANDONS EVERY SAVE THAT NAMES IT. The save
//! payload carries the phase NAME, not an index and not an enum integer, which is what lets a plan
//! grow a phase in the middle without re-labelling every objective in every save on disk (D2). A
//! renamed phase is detected on load, logged loudly by name and the objective is abandoned and
//! re-selected - never silently adopted as whatever index happens to sit there now. From Phase 4 the
//! deployment-side OVT_ObjectiveConditionDeploymentModule names phases by this string too, so a
//! rename costs two edits, not one.
//!
//! ⚠ m_aModules IS AN ORDERED BAG AND THE AUTHORED ORDER IS THE EVALUATION ORDER. .conf files cannot
//! carry comments, so the contract is stated here instead and nowhere else is allowed to re-derive it:
//!   ABORT modules are OR'd - the first that fires ends the objective, and its own attributes say
//!     whether the place is blacklisted.
//!   CONDITION modules are AND'd - all true advances to the next phase. An empty set never advances
//!     on its own; such a phase must end on a terminal operation or on the idle clock, and the
//!     registry's validator refuses one that can do neither.
//!   OPERATION modules are tried in order once the cadence has elapsed, and THE FIRST ONE THAT ACTS
//!     CONSUMES THE CADENCE. That is what reproduces the shipped `tower || harassment || sabotage`
//!     chain: three modules in that order, at most one operation per interval. A phase whose operation
//!     spends nothing and is WAITING for something - the shipped battle phase - authors a cadence of
//!     zero and is therefore asked every in-game minute.
//! The three roles are read off the module's own type, so an author may interleave them freely; only
//! the order WITHIN a role is meaningful.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomTitleField("m_sPhaseName")]
class OVT_ObjectivePhase
{
	[Attribute(desc: "Stable name of this phase. THE PERSISTENCE KEY: it travels in the save payload and is named by deployment-side phase conditions, so renaming it abandons every saved objective sitting in it. Must be unique within the plan")]
	string m_sPhaseName;

	[Attribute(defvalue: "-1", desc: "In-game minutes between operations while in this phase. -1 = use the campaign's objectiveHarassmentIntervalMinutes difficulty setting (Easy 90, Normal 60, Hard 45, Extreme 30, Insane 20). ZERO IS LEGAL AND MEANS EVERY IN-GAME MINUTE - which is what a phase that spends nothing and is POLLING wants, and is why both shipped plans author it on their battle phase")]
	int m_iOperationCadence;

	[Attribute(defvalue: "-1", desc: "Radius (m) of the deployment-evaluator anchor while in this phase - how far from the objective routine spending is nudged. Both shipped plans author 600 m while they are still choosing, 1200 m once they are committed to a forward base and a battle. -1 = the director's DEFAULT_ANCHOR_RADIUS, which is the tight one")]
	float m_fAnchorRadius;

	[Attribute(defvalue: "-1", desc: "In-game minutes this phase may go WITHOUT PROGRESS before the objective is abandoned as wedged. An IDLE clock, not a phase budget. -1 = the director's own m_iPhaseTimeoutTicks, which is what every phase shared before plans existed")]
	int m_iIdleTimeoutTicks;

	[Attribute(desc: "Condition, operation and abort modules. THE AUTHORED ORDER IS THE EVALUATION ORDER - see the class header, because a .conf cannot carry a comment saying so")]
	ref array<ref OVT_BaseObjectiveModule> m_aModules;

	//------------------------------------------------------------------------------------------------
	void OVT_ObjectivePhase()
	{
		if (!m_aModules)
			m_aModules = new array<ref OVT_BaseObjectiveModule>();
	}

	//------------------------------------------------------------------------------------------------
	//! A fresh, independent copy of every module in this phase, in the authored order.
	//!
	//! ⚠ THE CONFIG'S OWN MODULE OBJECTS ARE TEMPLATES AND ARE NEVER RUN. A phase entered twice - by
	//! two objectives, or by the same objective in two campaigns in one session - would otherwise share
	//! one module object and one set of latches, and the second entry would inherit the first's state.
	//! This mirrors the deployment framework exactly: OVT_DeploymentComponent clones its config's
	//! modules on activation for the same reason.
	//!
	//! 🔴 CloneModule() COPIES ATTRIBUTES BY HAND, IS NOT CHAINED, AND SILENTLY DROPS WHAT IT FORGETS.
	//! A dropped line is not a compile error and not a runtime error: the clone simply runs with a zero
	//! where the author wrote a number. Every concrete module therefore repeats its parent's WHOLE
	//! attribute list and gets its own dedicated case in the initialisation tier - see
	//! OVT_BaseObjectiveModule's header for the pattern those cases follow.
	//! \param[out] runtime Receives the clones. Cleared first.
	//! \return True when at least one module was cloned.
	bool CloneModules(notnull array<ref OVT_BaseObjectiveModule> runtime)
	{
		runtime.Clear();

		if (!m_aModules)
			return false;

		foreach (OVT_BaseObjectiveModule module : m_aModules)
		{
			if (!module)
				continue;

			OVT_BaseObjectiveModule clone = module.CloneModule();
			if (!clone)
				continue;

			runtime.Insert(clone);
		}

		return !runtime.IsEmpty();
	}
}
