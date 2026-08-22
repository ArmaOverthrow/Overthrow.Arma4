//------------------------------------------------------------------------------------------------
//! A CONDITION: one conjunct of "may this objective advance to the next phase?".
//!
//! Every condition module in a phase is asked once per director tick and the answers are AND'd
//! (OVT_ObjectivePlanRules.AllConditionsMet). All true advances; anything false leaves the phase
//! running. ⚠ AN EMPTY CONDITION SET ADVANCES NOTHING BY ITSELF but is not a wedge either - it means
//! the phase ends on a terminal operation or on the idle clock, and the registry's validator is what
//! refuses a phase that can do neither.
//!
//! ⚠ Evaluate() MUST BE SIDE-EFFECT FREE. It is asked every tick, it may be asked by a read-only
//! surface (the Game Master panel asks the equivalent question today through IsCounterAttackReady()),
//! and above all A PUBLIC READ MAY NEVER CHANGE PHASE - only the tick moves the machine. A condition
//! that spent resources, sent a deployment or re-armed a timer would put a transition somewhere other
//! than behind the tick's three early returns.
//!
//! ⚠ THE ONE EXCEPTION IS A LOG LATCH, and it is an exception because it changes nothing the machine
//! reads: a condition that reports "waiting for daylight" once rather than every in-game minute is
//! still side-effect free as far as every decision is concerned.
//!
//! See OVT_BaseObjectiveModule for the clone contract, which every concrete condition owes.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseObjectiveConditionModule : OVT_BaseObjectiveModule
{
	//------------------------------------------------------------------------------------------------
	//! Whether this conjunct is satisfied right now.
	//! \return True when the objective may advance as far as this condition is concerned. The base
	//!         answers FALSE, so a condition that forgot to override it blocks loudly in a test rather
	//!         than advancing a phase nobody gated.
	bool Evaluate()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this condition, WHILE IT IS FALSE, is blocking for a reason that is not the objective's
	//! fault - and therefore HOLDS the idle clock instead of spending it.
	//!
	//! 🔴 THE ONE SHIPPED USER IS THE DAYLIGHT WINDOW, AND THE DISTINCTION IT DRAWS IS D17'S. A
	//! condition that is false because the RAMP is not finished is the objective still being worked on
	//! and the clock must run - that is what the backstop is for. A condition that is false because the
	//! WORLD CLOCK says it is night is a wait the occupying faction can do nothing about: a gate met at
	//! 16:00 would otherwise spend the rest of the objective's patience waiting out the night and the
	//! objective would be ABANDONED FOR BEING DARK.
	//!
	//! ⚠ IT HOLDS THE IDLE CLOCK AND NOTHING ELSE. The operation cadence keeps running, every abort
	//! module keeps being asked, and every operation keeps being tried - so a forward base cut off at
	//! 22:00 still comes down at 22:00 and the garrison sender still runs. Freezing the whole phase was
	//! the ORIGINAL, over-broad reading of D17 and it was corrected by the author of the decision on
	//! 2026-08-19: it contradicts F7 outright and punishes a correct play.
	//!
	//! ⚠ IT IS ONLY CONSULTED ON A CONDITION THAT ANSWERED FALSE. A condition that is satisfied is not
	//! holding anything.
	//! eturn True when a false answer from this condition must not cost the objective a round. The
	//!         base answers false, so a condition that says nothing spends the clock as it always did.
	bool HoldsIdleClock()
	{
		return false;
	}
}
