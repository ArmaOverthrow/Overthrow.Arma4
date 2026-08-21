//------------------------------------------------------------------------------------------------
//! THE BACKSTOP, AS AN AUTHORED MODULE: give this objective up when it has gone its whole budget of
//! in-game minutes without making any progress at all.
//!
//! The port of the `if (TickObjectiveIdleClock(created)) ResetObjective(...)` line each hard-coded phase
//! handler ended with. ⚠ THE CLOCK ITSELF IS STILL THE RUNNER'S and stays there: it is the only thing in
//! the tick that has to know whether THIS tick created an operation, and "an operation is still walking"
//! and "the pool cannot cover the next one" are facts about the director's own bookkeeping rather than
//! about any one module. See OVT_ObjectiveDirectorComponent.TickObjectiveIdleClock() for the four
//! answers and for the play-test that turned a phase BUDGET into an IDLE clock.
//!
//! WHAT THIS MODULE OWNS is the two things that are doctrine: WHETHER a phase gives up at all, and WHAT
//! IT SAYS AND COSTS when it does.
//!
//! 🔴 A PHASE WITH NO IDLE ABORT AUTHORED CANNOT TIME OUT AT ALL. Its clock still runs down to zero and
//! the runner still asks the abort fold, but nothing answers - so the phase sits, forever, exactly as
//! it would have if it were making progress. Before the doctrine was authored data EVERY phase timed
//! out, because the give-up was hard-coded into each phase handler; the price of making it authorable
//! is that an author can now leave it out. That is a supported gesture for a TERMINAL phase (the
//! counter-attack ends by acting, not by waiting), and it is a mistake anywhere else. The registry's
//! validator does not catch it, deliberately: it cannot tell a terminal phase that wants no timeout
//! from a ramp phase that forgot one. An initialisation case pins the two shipped plans instead.
//!
//! ⚠ IT IS ASKED TWICE ON A TICK THAT REACHES THE END, AND THAT IS WHAT MAKES THE TIMING EXACT. The
//! runner folds the abort modules at the top of the tick, before anything is spent, and again
//! immediately after serving the idle clock - because the clock is served LAST, after the operations,
//! and a verdict read only at the top of the tick would abandon the objective one in-game minute late.
//! Asking twice is free precisely because ShouldAbort() is side-effect free by contract.
//!
//! ⚠ THE REASON IS A SENTENCE, NOT A CODE. It is printed verbatim into the campaign log and is the only
//! thing a server owner has to explain why the occupying faction stopped attacking a place, so it says
//! what was expected and what happened instead.
//!
//! ⚠ BLACKLISTING IS THIS MODULE'S DECISION, NOT THE RUNNER'S. A place that could not be worked on sits
//! out a selection round so the machine does not pick it again immediately and fail again; a place whose
//! battle merely resolved does NOT, because a resolved battle is not a failure of the objective.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_IdleForObjectiveAbort : OVT_BaseObjectiveAbortModule
{
	[Attribute(defvalue: "harassment", desc: "What this phase's work is called in the log line, e.g. 'harassment' or 'the forward-base phase'. The sentence reads '<this> did nothing at all for N in-game minutes and never reached <the goal below>'")]
	string m_sPhaseWork;

	[Attribute(defvalue: "the forward-base gate", desc: "What the phase was trying to reach and did not, e.g. 'the forward-base gate' or 'the counter-attack gate'. See the sentence above")]
	string m_sGoalNotReached;

	[Attribute(defvalue: "1", desc: "Whether the PLACE sits out a selection round when this fires. 1 for a phase that failed to make progress - picking it again immediately would fail the same way. 0 for an ending that is not a failure of the objective")]
	bool m_bBlacklist;

	//------------------------------------------------------------------------------------------------
	//! \param[out] reason The sentence for the campaign log, written only when this returns true.
	//! \param[out] blacklist Whether the objective's PLACE should sit out a selection round.
	//! \return True when the idle clock has run all the way down.
	override bool ShouldAbort(out string reason, out bool blacklist)
	{
		reason = "";
		blacklist = false;

		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		// ⚠ THE CLOCK IS THE RUNNER'S AND IS ONLY READ HERE. It is HELD - never decremented - while an
		// operation is in flight or the pool cannot cover the next one, so a zero here really does mean
		// "nothing has happened for the whole budget", not "the objective has been unlucky".
		if (objective.GetPhaseTicks() > 0)
			return false;

		OVT_ObjectiveDirectorComponent director = GetDirector();
		int budget = 0;
		if (director)
			budget = director.GetPhaseTimeoutTicks();

		reason = m_sPhaseWork + " did nothing at all for " + budget.ToString() + " in-game minutes and never reached " + m_sGoalNotReached;
		blacklist = m_bBlacklist;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the clock lives on the objective record, not in the bag.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_sPhaseWork or m_sGoalNotReached and the campaign log gets a
	//! half-written sentence for the one event a server owner most needs explained; drop m_bBlacklist and
	//! every clone reads false, so a place the machine has just failed at is immediately eligible again
	//! and the campaign churns on it.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_IdleForObjectiveAbort clone = new OVT_IdleForObjectiveAbort();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sPhaseWork = m_sPhaseWork;
		clone.m_sGoalNotReached = m_sGoalNotReached;
		clone.m_bBlacklist = m_bBlacklist;

		return clone;
	}
}
