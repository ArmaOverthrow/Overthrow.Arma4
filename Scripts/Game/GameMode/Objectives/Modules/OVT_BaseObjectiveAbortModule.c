//------------------------------------------------------------------------------------------------
//! AN ABORT: one reason to give this objective up and let the machine choose again.
//!
//! Every abort module in a phase is asked once per director tick, BEFORE the conditions and before
//! the operations, and the answers are OR'd (OVT_ObjectivePlanRules.AnyAbort) - the first that fires
//! wins and its reason is the one that reaches the log. ⚠ AN EMPTY ABORT SET NEVER ABORTS, which is
//! the opposite of an empty CONDITION set and both are right: a phase with no aborts must not abort, a
//! phase with no conditions must not be stuck.
//!
//! ⚠ THE REASON IS A SENTENCE, NOT A CODE. It is printed verbatim into the campaign log and is the
//! only thing a server owner has to explain why the occupying faction stopped attacking a place, so it
//! says what was expected and what happened instead - "the forward-base phase did nothing at all for
//! 240 in-game minutes and never reached the counter-attack gate", not "TIMEOUT".
//!
//! ⚠ BLACKLISTING IS THE ABORT'S DECISION, NOT THE RUNNER'S. A place that could not be built toward
//! sits out a selection round so the machine does not pick it again immediately and fail again; a
//! place whose battle merely resolved does NOT, because a resolved battle is not a failure of the
//! objective. Getting that backwards makes the campaign either churn or go quiet.
//!
//! ⚠ ShouldAbort() MUST BE SIDE-EFFECT FREE except for a log latch, for the same reason a condition
//! must be: the RESET is the runner's, behind the tick's three early returns.
//!
//! See OVT_BaseObjectiveModule for the clone contract, which every concrete abort owes.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseObjectiveAbortModule : OVT_BaseObjectiveModule
{
	//------------------------------------------------------------------------------------------------
	//! Whether this objective must be given up now.
	//! \param[out] reason A sentence for the campaign log, written only when this returns true.
	//! \param[out] blacklist Whether the objective's PLACE should sit out a selection round.
	//! \return True when the objective must be reset. The base answers false.
	bool ShouldAbort(out string reason, out bool blacklist)
	{
		reason = "";
		blacklist = false;

		return false;
	}
}
