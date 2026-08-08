//------------------------------------------------------------------------------------------------
//! Overthrow's open-the-Field-Manual-on-a-specific-page seam.
//!
//! WHY A MODDED CLASS AND NOT A SUBCLASS OR A HELPER. The base game can already open the manual on a
//! chosen page - SCR_FieldManualUI.Open(EFieldManualEntryId) at :859-866 - but it matches on
//! SCR_FieldManualConfigEntry.m_eId, a base-game enum with a fixed value set. There is no FindEntry,
//! no GetEntryById and no name lookup anywhere in the class; the only other walk of m_aAllEntries is
//! the searchbar's fuzzy ProcessSearch (:738-818), which is protected, localized, and populates the
//! tile grid rather than opening an entry. And the menu is instantiated by the engine from
//! ChimeraMenuPreset.FieldManualDialog, so a subclass would never be the object that gets created.
//! A modded class IS the class, which is what gives the two methods below access to the protected
//! m_aAllEntries (:18), m_bOpenedFromOutside (:34) and SetCurrentEntry (:323).
//!
//! WHY TITLE KEYS ARE THE LINK IDS (plan decision D12). The rejected alternative was
//! `modded enum EFieldManualEntryId`, which is legal - the base game mods its own enums - but needs
//! the enum PLUS a per-entry conf field PLUS a string-to-enum map to keep callers string-based.
//! SCR_FieldManualConfigEntry.m_sTitle is mod-owned, already unique, already stable, and already the
//! thing whoever authors a manual page types. Accepted cost, and it is a real one: the match is
//! EXACT and CASE-SENSITIVE, so renaming a page's title key breaks every link pointing at it. The
//! contract that freezes those keys is in
//! docs/features/new-player-experience/tutorial-system/context.md, and
//! OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve fails the build if an authored tutorial link
//! stops resolving.
//!
//! NOTHING VANILLA CHANGES. No override, no super call, no behaviour touched: opening the manual
//! from the main menu or the pause menu runs exactly the code it ran before. This file only adds two
//! methods nobody but Overthrow calls.
//!
//! Overthrow code should call OVT_FieldManualHelper.Open() rather than either method here.
//------------------------------------------------------------------------------------------------
modded class SCR_FieldManualUI
{
	//------------------------------------------------------------------------------------------------
	//! Navigates an ALREADY-OPEN manual to the entry whose m_sTitle is titleKey.
	//!
	//! Deliberately shaped like vanilla's OpenEntry (:412-434), including its fallback: an id that
	//! matches nothing leaves the manual on its front page rather than erroring or closing. An unknown
	//! link is a content mistake, not a crash (plan integration criterion I2).
	//!
	//! m_bOpenedFromOutside is set for the same reason OpenEntry sets it: it tells
	//! CloseMenuOrReadingPanel (:831-837) that Back should leave the manual entirely rather than drop
	//! the player onto a tile grid they never chose to visit.
	//!
	//! Only entries the UI can actually reach are matchable, because m_aAllEntries is what
	//! SetAllEntriesAndParents (:600-663) built: disabled entries, entries with no content, and
	//! anything nested more than two category levels deep are already gone from it. Linking to one of
	//! those therefore lands on the front page, which is the correct outcome - the page genuinely is
	//! not in the manual.
	//! \param[in] titleKey The target entry's m_sTitle localization key, e.g.
	//! "#OVT-FieldManual_MainMenu_Title". An empty string opens the front page without a warning.
	void OVT_OpenEntryByTitle(string titleKey)
	{
		if (titleKey == "")
		{
			SetCurrentEntry(null);
			return;
		}

		if (m_aAllEntries)
		{
			foreach (SCR_FieldManualConfigEntry entry : m_aAllEntries)
			{
				if (!entry)
					continue;

				if (entry.m_sTitle != titleKey)
					continue;

				m_bOpenedFromOutside = true;
				SetCurrentEntry(entry);
				return;
			}
		}

		Print("[Overthrow.FieldManual] No manual entry has the title key '" + titleKey + "' - opening the front page instead. Title keys are the link ids and are matched exactly and case-sensitively; check the key against the entry's m_sTitle in Configs/FieldManual/Categories/FM_Overthrow.conf.", LogLevel.WARNING);

		SetCurrentEntry(null);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the Field Manual and navigates it to one page, in one call.
	//!
	//! ORDERING IS LOAD-BEARING (plan task 7.5). The navigation happens AFTER OpenMenu returns and must
	//! never be moved into an override of OnMenuOpen. MenuManager.OpenMenu drives OnMenuOpen AND
	//! OnMenuShow before it returns, and OnMenuShow (:153-162) calls CloseReadingPanel() ->
	//! SetCurrentEntry(null). Anything navigated from inside OnMenuOpen is silently undone one call
	//! later, and the symptom - "the deep link opens the front page" - looks exactly like a wrong key.
	//! This is also why vanilla's own Open() is written this way.
	//!
	//! The one deliberate difference from vanilla's Open() (:859-866): the OpenMenu result is
	//! null-guarded. Vanilla dereferences it immediately, so anything that stops the dialog opening -
	//! a menu already refusing to be replaced, a missing preset - is a script error there. A tip's
	//! "Learn more" button must never be able to error a session.
	//! \param[in] titleKey The target entry's m_sTitle localization key.
	//! \return The opened menu, or null when the Field Manual could not be opened at all.
	static SCR_FieldManualUI OVT_OpenByTitle(string titleKey)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return null;

		SCR_FieldManualUI ui = SCR_FieldManualUI.Cast(menuManager.OpenMenu(ChimeraMenuPreset.FieldManualDialog));
		if (!ui)
			return null;

		ui.OVT_OpenEntryByTitle(titleKey);

		return ui;
	}
}
