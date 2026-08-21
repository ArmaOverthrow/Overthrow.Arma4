//------------------------------------------------------------------------------------------------
//! Suppresses the in-world action menu while an Overthrow dialog or UI context owns the screen.
//!
//! THE SYMPTOM. Vanilla's SCR_ActionMenuInteractionDisplay - the floating list of actions on the
//! object you are looking at - draws ON TOP of an Overthrow screen opened from one of those very
//! actions, its text overlapping ours. It does not steal input, it is just painted over everything.
//!
//! WHY IT IS ON TOP. That display is declared with m_eLayer ALWAYS_TOP in
//! DefaultPlayerController.et:202, and that layer lives under SCR_HUDManagerComponent.m_wRootTop, a
//! workspace widget created with SetZOrder(100) expressly so it draws above everything else - the
//! base game says so in a comment on SCR_HUDManagerComponent.c:226 ("even above MenuManager
//! layouts"). So it beats a MenuManager dialog AND anything OVT_UIContext puts on the workspace.
//!
//! WHY IT IS STILL SHOWING. SCR_InteractionHandlerComponent.GetCanInteractScript() is the base
//! game's own "no interactions while UI is up" test, and OnPostFrame() feeds the display an EMPTY
//! action list when it answers no, which fades the display out. But that test (line 272 of the
//! vanilla file) reads:
//!
//!     if (menuManager && menuManager.IsAnyMenuOpen())
//!
//! IsAnyMenuOpen() and IsAnyDialogOpen() are two DISJOINT engine queries, and the base game's own
//! idiom for "is any UI up" is both of them ORed together - see game.c:667 and game.c:679. The
//! interaction handler checks only the first. A dialog therefore does not suppress the action menu,
//! which is a vanilla bug and is exactly Overthrow's case: every in-world action that opens a
//! screen here is an OVT_DialogUserAction, i.e. SCR_ConfigurableDialogUi ->
//! MenuManager.OpenDialog(). (OVT_DialogUserAction.CloseInteractionMenu() is an older attempt at
//! this: a single HideDisplay() at dialog-creation time, which the handler undoes on the next frame
//! the moment DisplayUpdate() decides it should be shown again.)
//!
//! Overthrow's OVT_UIContext screens are a second, separate blind spot: they are not MenuManager
//! menus at all, so neither query sees them.
//!
//! THE FIX is to answer the same question the base game meant to ask - for dialogs, and for our
//! contexts. Everything downstream is vanilla's: empty list, Update() returns false, the display
//! fades itself out and fades back when the screen closes. It also stops a new action being started
//! from behind an open screen, because DoProcessInteraction() consults the same method before
//! user.DoStartObjectAction() - the same protection vanilla menus already get.
//!
//! Belt and braces: OnPostFrame() also hides the display's root widget outright, re-asserted each
//! frame. The empty-list fade is reached through several conditions (m_bIsPerforming, a null
//! selected action, the display's own m_bShown bookkeeping) and any one of them can keep the widget
//! on screen. We restore only what we hid, so a display vanilla wanted hidden stays hidden.
//!
//! Asks IsAnyContextBlocking() rather than a per-context IsActive() because OVT_PlaceContext and
//! OVT_BuildContext close their menu before they start driving a ghost - they own the screen for
//! the whole of placement with m_bIsActive false. That method is the one place that knows.
modded class SCR_InteractionHandlerComponent
{
	//! True while we are holding the display's root widget hidden. Guards the restore so we never
	//! force-show a display vanilla had hidden for its own reasons.
	protected bool m_bOvtDisplayHidden;

	//------------------------------------------------------------------------------------------------
	//! \param[in] controlledEntity The character the local player is controlling.
	//! \return True while a dialog or an Overthrow context has something on screen.
	protected bool IsOverthrowUIBlocking(IEntity controlledEntity)
	{
		// The half vanilla forgot. Covers every SCR_ConfigurableDialogUi, ours and the base game's.
		MenuManager menuManager = GetGame().GetMenuManager();
		if (menuManager && menuManager.IsAnyDialogOpen())
			return true;

		if (!controlledEntity)
			return false;

		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(controlledEntity.FindComponent(OVT_UIManagerComponent));
		if (!ui)
			return false;

		return ui.IsAnyContextBlocking();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] controlledEntity The character the local player is controlling.
	//! \return False while a dialog or an Overthrow context is on screen, otherwise vanilla's answer.
	override protected bool GetCanInteractScript(IEntity controlledEntity)
	{
		if (!super.GetCanInteractScript(controlledEntity))
			return false;

		return !IsOverthrowUIBlocking(controlledEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs after vanilla has decided what the display should show, and overrules it.
	override protected void OnPostFrame(IEntity owner, IEntity controlledEntity, float timeSlice)
	{
		super.OnPostFrame(owner, controlledEntity, timeSlice);

		if (!m_pDisplay)
			return;

		Widget root = m_pDisplay.GetRootWidget();
		if (!root)
			return;

		if (IsOverthrowUIBlocking(controlledEntity))
		{
			if (root.IsVisible())
			{
				root.SetVisible(false);
				m_bOvtDisplayHidden = true;
			}

			return;
		}

		if (m_bOvtDisplayHidden)
		{
			m_bOvtDisplayHidden = false;
			root.SetVisible(true);
		}
	}
}
