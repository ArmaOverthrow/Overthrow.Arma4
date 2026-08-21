//------------------------------------------------------------------------------------------------
//! AN OPERATION: one thing the occupying faction may DO at an objective this interval.
//!
//! Every operation module in a phase is tried in the authored order once the phase's cadence has
//! elapsed, and THE FIRST ONE THAT ACTS CONSUMES THE CADENCE. That single rule is what reproduces the
//! shipped machine's `tower || harassment || sabotage` chain from authored data: three modules, in
//! that order, at most one operation per interval whichever kind it is.
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
}
