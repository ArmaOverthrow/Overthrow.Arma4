//------------------------------------------------------------------------------------------------
//! AN OPERATION: one thing the occupying faction may DO at an objective this interval.
//!
//! Every operation module in a phase is tried once the phase's cadence has elapsed, and THE FIRST ONE
//! THAT ACTS CONSUMES THE CADENCE. That single rule is what reproduces the shipped machine's
//! `tower || harassment || sabotage` chain from authored data: a handful of modules, at most one
//! operation per interval whichever kind it is.
//!
//! ⚠ THE ORDER THEY ARE ASKED IN IS SHUFFLED EVERY CADENCE, and since 2026-08-21 that is the point -
//! see ShufflesFreely() for the author's reasoning and for why it does not contradict the plan's
//! "no jitter" rule. The AUTHORED order still decides two things: which operations are pinned ahead of
//! the shuffle, and the order among those pinned ones.
//!
//! ⚠ THE CADENCE IS ONLY RE-ARMED ON A SUCCESSFUL ACT, AND THAT IS DELIBERATE. Every refusal -
//! nothing to recapture, nothing to sabotage, the concurrency cap full, the pool short, a config
//! missing - leaves the countdown at zero so the NEXT tick asks again a minute later, instead of
//! waiting out another whole interval for a condition that may have cleared immediately. That retry is
//! also what makes an affordability hold cover a whole poverty spell rather than one tick in
//! forty-five.
//!
//! ⚠ EVERY SPEND STILL LEAVES THE ONE POOL EXACTLY ONCE, AT THE DIRECTOR'S ONE CHOKE POINT (G5). An
//! operation module never touches OVT_DeploymentManagerComponent's resource map, never credits
//! anything, and never holds money: it asks the director to create a deployment, and the director
//! creates it and debits for it in one place. A grep for the credit call under this folder is a
//! Definition-of-Done criterion, comments included.
//!
//! ⚠ IsTerminal() IS ABOUT THE PHASE, NOT ABOUT THE MODULE. A phase carrying a terminal operation is
//! one that can END the objective by acting - the counter-attack is the shipped example - and that is
//! what lets the validator accept a phase with no advance conditions.
//!
//! See OVT_BaseObjectiveModule for the clone contract, which every concrete operation owes.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseObjectiveOperationModule : OVT_BaseObjectiveModule
{
	//------------------------------------------------------------------------------------------------
	//! Do this operation's work, if there is any to do and it can be afforded.
	//! \return True when something was actually created and paid for - which is PROGRESS, re-arms the
	//!         idle clock, consumes the cadence and stops later operation modules being asked this tick.
	bool TryAct()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a phase carrying this operation can end the objective by acting.
	//! \return True for an operation that finishes the plan. The base answers false.
	bool IsTerminal()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! MAY THIS OPERATION BE ASKED OUT OF ITS AUTHORED POSITION?
	//!
	//! ==========================================================================================
	//! THE DIRECTOR'S NEXT MOVE IS DELIBERATELY UNPREDICTABLE (author, 2026-08-21).
	//! ==========================================================================================
	//! *"I would assume that the cadence after an FOB is up is just a little random and unpredictable,
	//! sometimes they garrison, sometimes they sabotage. We want some unpredictability about what the
	//! director does next otherwise the player learns his patterns and just follows a script to counter
	//! it."*
	//!
	//! Strict first-wins over a fixed authored order is a script: once a player has seen one forward-base
	//! phase they know the next move of every other one. So the director SHUFFLES the operations it
	//! offers each cadence - see OVT_ObjectiveDirectorComponent.BuildOperationOrder.
	//!
	//! ⚠ THIS IS NOT THE "no jitter" RULE BEING BROKEN, AND THE NEXT READER WILL THINK IT IS. That rule
	//! (implementation.md §4 Phase 3, D-series) is about OBJECTIVE SELECTION - which place gets picked -
	//! where randomness was explicitly rejected because a server owner has to be able to read why a
	//! target was chosen. This is OPERATION CHOICE WITHIN A PHASE, a different question, and the author
	//! asked for randomness in it by name. Do not "restore" determinism here by citing the selection
	//! rule.
	//!
	//! ⚠ WHY A DECLARATIVE FLAG AND NOT A CanAct()/WouldAct() PREDICATE. TryAct() PERFORMS the work, so
	//! "would you act" cannot be asked without splitting every operation into a test half and a do half -
	//! a contract change across every module, and a new way for one to disagree with itself. This asks a
	//! question no module can get wrong about itself: may my POSITION move? It says nothing about
	//! eligibility and cannot lie about it.
	//!
	//! \return True to be shuffled with its peers. FALSE PINS THIS OPERATION AHEAD OF THE SHUFFLE, in
	//!         authored order, and is for an operation whose position guards something the shuffle would
	//!         otherwise spend - see OVT_RaiseForwardBaseObjectiveOperation, the only override in the
	//!         tree. The base answers true, so an operation that says nothing is freely reordered.
	bool ShufflesFreely()
	{
		return true;
	}
}
