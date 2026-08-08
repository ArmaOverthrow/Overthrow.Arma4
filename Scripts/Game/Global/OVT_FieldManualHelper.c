//------------------------------------------------------------------------------------------------
//! The single call Overthrow code makes to open the Field Manual on a chosen page.
//!
//! Everything mechanical about deep-linking lives in the modded SCR_FieldManualUI
//! (Scripts/Game/UI/Modded/SCR_FieldManualUI.c). This class exists so that call sites - a tutorial
//! popup's "Learn more" button today, a main-menu shortcut or a user action tomorrow - depend on one
//! Overthrow-owned name instead of on a base-game class Overthrow has modded. If the seam ever has to
//! change shape (the rejected `modded enum EFieldManualEntryId` route, say), this is the only file
//! that has to know.
//!
//! CLIENT-ONLY, BY CONSTRUCTION. There is no menu manager on a dedicated server, so Open() answers
//! false there rather than erroring - the same shape every other UI-touching Overthrow helper uses.
//!
//! AN UNKNOWN KEY IS NOT AN ERROR (plan integration criterion I2). The manual opens on its front
//! page and a warning is logged naming the key. Tips are the lowest-stakes system in the mod; a
//! stale content link must never be able to break a session.
//------------------------------------------------------------------------------------------------
class OVT_FieldManualHelper
{
	//------------------------------------------------------------------------------------------------
	//! Opens the Field Manual on the entry whose m_sTitle is titleKey.
	//!
	//! The key is a localization key, not a display string and not an entry index: it is matched
	//! against SCR_FieldManualConfigEntry.m_sTitle exactly and case-sensitively. See plan decision D12
	//! and the id contract in docs/features/new-player-experience/tutorial-system/context.md.
	//!
	//! Callers do not need to check the return value. It is there so a UI can choose not to close
	//! itself when the manual refused to open - which is the one case where leaving the popup on
	//! screen is better than dismissing it into nothing.
	//! \param[in] titleKey The target entry's m_sTitle key, e.g. "#OVT-FieldManual_MainMenu_Title".
	//! An unknown key opens the front page and logs a warning; an empty key is refused outright.
	//! \return True when the Field Manual was opened, false when it could not be.
	static bool Open(string titleKey)
	{
		if (titleKey == "")
		{
			Print("[Overthrow.FieldManual] OVT_FieldManualHelper.Open() was called with an empty title key. Nothing was opened - a caller is asking for a page it has no id for.", LogLevel.WARNING);
			return false;
		}

		SCR_FieldManualUI ui = SCR_FieldManualUI.OVT_OpenByTitle(titleKey);
		if (!ui)
		{
			Print("[Overthrow.FieldManual] The Field Manual would not open for '" + titleKey + "'. Either there is no menu manager on this machine (a dedicated server has none) or ChimeraMenuPreset.FieldManualDialog refused to open over the current menu.", LogLevel.WARNING);
			return false;
		}

		return true;
	}
}
