//------------------------------------------------------------------------------------------------
//! The one predicate that decides whether a queued tutorial popup may be shown RIGHT NOW.
//!
//! Every input is a plain bool, and producing those bools is somebody else's job. In particular
//! `blockingUiOpen` is computed by the component from three separate engine facts (any Overthrow
//! context active, any base-game menu on top, the map open); folding them into one argument is what
//! keeps this decision testable without a world, and what makes "which three things count as
//! blocking" a question with exactly one answer in one place.
//!
//! Four independent vetoes, each asserted in isolation by the Logic case. Three of them are absolute.
//! The fourth, blockingUiOpen, is the only one an ENTRY can waive: an entry whose whole subject is the
//! screen the player is looking at (the map, the place menu, the real estate or skills screens)
//! declares m_bShowOverUI and is shown on top of it rather than after it closes.
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
	//! \param[in] entryShowsOverUi The pending entry's m_bShowOverUI, already reduced to false for
	//! anything MODAL by the caller. It WAIVES the blockingUiOpen veto and NOTHING else - a tip that
	//! may sit over the map still may not appear while the player is dead, while tips are off, or on
	//! top of another tip.
	//! \return True only when tips are enabled, nothing is showing, the player is alive, and either no
	//! blocking UI is open or this entry is one that belongs on top of it.
	static bool CanShowNow(bool tipsDisabled, bool alreadyShowing, bool blockingUiOpen, bool playerAlive, bool entryShowsOverUi = false)
	{
		if (tipsDisabled)
			return false;

		if (alreadyShowing)
			return false;

		if (blockingUiOpen && !entryShowsOverUi)
			return false;

		if (!playerAlive)
			return false;

		return true;
	}
}
