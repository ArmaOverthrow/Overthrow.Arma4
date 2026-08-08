//------------------------------------------------------------------------------------------------
//! The one predicate that decides whether a queued tutorial popup may be shown RIGHT NOW.
//!
//! Every input is a plain bool, and producing those bools is somebody else's job. In particular
//! `blockingUiOpen` is computed by the component from three separate engine facts (any Overthrow
//! context active, any base-game menu on top, the map open); folding them into one argument is what
//! keeps this decision testable without a world, and what makes "which three things count as
//! blocking" a question with exactly one answer in one place.
//!
//! Conjunction of four clear conditions rather than an early-out chain, because the requirement is
//! stated as four independent vetoes and the Logic case asserts each one in isolation.
//------------------------------------------------------------------------------------------------
class OVT_TutorialGate
{
	//------------------------------------------------------------------------------------------------
	//! Whether a queued popup may be shown at this instant.
	//!
	//! Note what is NOT here: whether the entry has been seen. That is checked once on RECEIPT, not
	//! on every pump - an entry dropped for being seen never reaches the queue at all.
	//!
	//! \param[in] tipsDisabled The player's "Don't show tips again" setting.
	//! \param[in] alreadyShowing True while a tutorial popup of either presentation is on screen.
	//! \param[in] blockingUiOpen True while any Overthrow context, base-game menu or the map is open.
	//! \param[in] playerAlive True while the local player's character is alive.
	//! \return True only when tips are enabled, nothing is showing, no blocking UI is open and the
	//! player is alive.
	static bool CanShowNow(bool tipsDisabled, bool alreadyShowing, bool blockingUiOpen, bool playerAlive)
	{
		if (tipsDisabled)
			return false;

		if (alreadyShowing)
			return false;

		if (blockingUiOpen)
			return false;

		if (!playerAlive)
			return false;

		return true;
	}
}
