//------------------------------------------------------------------------------------------------
//! The modal tutorial presentation: a focusable popup with real buttons and a multi-page sequence.
//!
//! The second of the two surfaces the tutorial pipeline can hand an entry to, and the one that
//! carries the full control set. OVT_TutorialInfo (the HUD overlay) takes single-page NONMODAL
//! entries; this takes everything else - MODAL entries, and multi-page entries regardless of what
//! they declare, because plan decision D10 makes a sequence modal by definition. IsModal() below is
//! the ONE copy of that rule and both surfaces call it, so an entry can never be shown twice or
//! dropped by the two of them disagreeing.
//!
//! Its contract with the pipeline is the same two calls the HUD overlay uses:
//!
//!   in : OVT_TutorialComponent.m_OnShowTutorial(OVT_TutorialEntryConfig)
//!   out: OVT_TutorialComponent.NotifyDismissed(entryId)
//!
//! and NotifyDismissed("") is the same load-bearing escape hatch: it hands the pipeline back without
//! recording the entry as seen, for every path that ends a popup for a reason that is not the player
//! having read it.
//!
//! Registered on the player prefab with NO m_sOpenAction (the OVT_ShopContext shape). It is opened
//! either by the pipeline, or from the Overthrow main menu's Tips entry after the HUD overlay's
//! prompt has escalated a tip into it.
//------------------------------------------------------------------------------------------------
class OVT_TutorialContext : OVT_UIContext
{
	//! Retry cadence for resolving the tutorial component, in milliseconds.
	static const int SUBSCRIBE_RETRY_MS = 1000;

	//! Hard ceiling on those retries. OVT_OverthrowController is registered by an async
	//! RpcDo_NotifyOwnerAssignment, so the component legitimately does not exist for the first
	//! seconds of a session; after a minute it never will, and a timer that retries forever is a
	//! leak rather than a fallback. Same shape and reasoning as OVT_TutorialInfo.
	static const int SUBSCRIBE_MAX_ATTEMPTS = 60;

	//! The entry this popup shows. Not a `ref`: the strong reference lives in the authored
	//! m_aEntries array on the game mode, exactly as OVT_ManageVehicleContext holds its
	//! OVT_VehicleUpgrade. Kept after the popup closes so the main menu's Tips entry can re-open it.
	protected OVT_TutorialEntryConfig m_Entry;

	//! Zero-based index of the page on screen.
	protected int m_iPageIndex;

	//! The component whose invoker this context is subscribed to. Held so the unsubscribe can target
	//! the SAME invoker that was inserted into, even if the local controller has changed since.
	protected OVT_TutorialComponent m_Tutorials;

	//! How many times the subscribe retry has run.
	protected int m_iSubscribeAttempts;

	//! True while the subscribe retry timer is registered on the call queue.
	protected bool m_bSubscribeRetryRunning;

	protected Widget m_wDismissButton;
	protected Widget m_wNextButton;
	protected Widget m_wBackButton;
	protected Widget m_wLearnMoreButton;
	protected Widget m_wDisableTipsButton;

	protected SCR_InputButtonComponent m_DismissAction;
	protected SCR_InputButtonComponent m_NextAction;
	protected SCR_InputButtonComponent m_BackAction;
	protected SCR_InputButtonComponent m_LearnMoreAction;
	protected SCR_InputButtonComponent m_DisableTipsAction;

	//-----------------------------------------------------------------------------------------------
	// THE D10 RULE - ONE COPY, BOTH SURFACES
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether an entry belongs to this surface rather than to the HUD overlay.
	//!
	//! The single implementation of plan decision D10. OVT_TutorialInfo calls this too: if the two
	//! surfaces ever applied different tests, an entry would be presented twice or by nobody, and
	//! "by nobody" jams the pipeline for the rest of the session.
	//! \param[in] entry The entry to classify. Null is not modal.
	//! \return True when the entry must be presented modally.
	static bool IsModal(OVT_TutorialEntryConfig entry)
	{
		if (!entry)
			return false;

		if (entry.m_ePresentation == OVT_TutorialPresentation.MODAL)
			return true;

		// A sequence is modal whatever it declares: paging a non-focusable HUD overlay would cost
		// two more gameplay keybindings for no benefit, and the only sequence consumer wants modal.
		if (entry.m_aPages && entry.m_aPages.Count() > 1)
			return true;

		return false;
	}

	//-----------------------------------------------------------------------------------------------
	// PIPELINE SUBSCRIPTION
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the pipeline as well as binding the base class's open/close actions.
	//!
	//! NOT PostInit() and NOT OnShow(). This context has to be listening while it is CLOSED - being
	//! opened is the whole point of the subscription - so OnShow is too late; and the invoker lives on
	//! OVT_OverthrowController, which outlives the character, so a PostInit subscription with no
	//! matching removal would leave one dead context per respawn wired to a live invoker. Register /
	//! Unregister are the symmetric pair the UI manager already drives on possession and on death.
	override void RegisterInputs()
	{
		super.RegisterInputs();

		m_iSubscribeAttempts = 0;
		StartSubscribeRetry();
		TrySubscribe();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what RegisterInputs added.
	override void UnregisterInputs()
	{
		super.UnregisterInputs();

		StopSubscribeRetry();
		Unsubscribe();
	}

	//------------------------------------------------------------------------------------------------
	//! One attempt at subscribing to the local player's tutorial component.
	protected void TrySubscribe()
	{
		if (m_Tutorials)
		{
			StopSubscribeRetry();
			return;
		}

		OVT_TutorialComponent tutorials = OVT_Global.GetTutorials();
		if (tutorials && tutorials.m_OnShowTutorial)
		{
			m_Tutorials = tutorials;
			// Remove first: Insert does not de-duplicate, and RegisterInputs can legitimately run
			// twice without an Unregister between (possession changes are driven from two places).
			m_Tutorials.m_OnShowTutorial.Remove(OnShowTutorial);
			m_Tutorials.m_OnShowTutorial.Insert(OnShowTutorial);
			StopSubscribeRetry();
			return;
		}

		m_iSubscribeAttempts++;

		// Silent give-up: a spectator and a player who left during the countdown both reach this.
		if (m_iSubscribeAttempts >= SUBSCRIBE_MAX_ATTEMPTS)
			StopSubscribeRetry();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes this context from the invoker it actually inserted into.
	protected void Unsubscribe()
	{
		if (!m_Tutorials)
			return;

		if (m_Tutorials.m_OnShowTutorial)
			m_Tutorials.m_OnShowTutorial.Remove(OnShowTutorial);

		m_Tutorials = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the subscribe retry timer. Idempotent.
	protected void StartSubscribeRetry()
	{
		if (m_bSubscribeRetryRunning)
			return;

		m_bSubscribeRetryRunning = true;

		GetGame().GetCallqueue().CallLater(TrySubscribe, SUBSCRIBE_RETRY_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the subscribe retry timer. Idempotent.
	protected void StopSubscribeRetry()
	{
		if (!m_bSubscribeRetryRunning)
			return;

		m_bSubscribeRetryRunning = false;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(TrySubscribe);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while this context is wired to the pipeline and can be opened by it.
	bool IsPipelineSubscribed()
	{
		return m_Tutorials != null;
	}

	//-----------------------------------------------------------------------------------------------
	// OPENING
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The pipeline cleared an entry to be shown.
	//!
	//! Every branch must end either with the popup on screen or with the pipeline released, because
	//! OVT_TutorialComponent.Pump() sets its "something is showing" flag BEFORE invoking and only a
	//! surface can clear it. A branch that just returns would jam the queue for the session - the one
	//! exception being an entry that belongs to the HUD overlay, which releases it instead.
	//! \param[in] entry The entry the pipeline released. Null is left to the HUD overlay.
	protected void OnShowTutorial(OVT_TutorialEntryConfig entry)
	{
		if (!IsModal(entry))
			return;

		SetEntry(entry);

		ShowLayout();

		// CanShowLayout() can legitimately refuse - most realistically because the player started
		// positioning a placement ghost between the gate check and this frame. Hand the pipeline back
		// WITHOUT marking the entry seen; m_Entry survives, so the main menu's Tips entry can still
		// open it, and the entry itself will come back next session.
		if (!m_bIsActive)
			ReleaseToPipeline("");
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the entry this popup will show. Call BEFORE ShowLayout(), never as an argument to it -
	//! ShowLayout is also an input-listener target and takes none.
	//! \param[in] entry The entry to present. Null clears the popup's contents.
	void SetEntry(OVT_TutorialEntryConfig entry)
	{
		m_Entry = entry;
		m_iPageIndex = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The entry this popup would show, or null when nothing has been handed to it yet.
	OVT_TutorialEntryConfig GetEntry()
	{
		return m_Entry;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when there is an entry to (re-)open. Drives the enabled state of the Overthrow
	//! main menu's Tips entry, so it is never a dead button.
	bool HasEntry()
	{
		return m_Entry != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Refuses to open with nothing to show, or while another Overthrow context owns the screen.
	//!
	//! The second test is what keeps this popup's shortcuts off the place/build rotate keys: placement
	//! runs with its menu closed (OVT_PlaceContext.IsBlockingPopups), so an IsActive()-based test
	//! would let the modal open on top of a ghost with OverthrowPlaceContext live underneath it.
	//! \return True when the popup may open.
	override bool CanShowLayout()
	{
		if (!m_Entry)
			return false;

		if (!m_Entry.m_aPages || m_Entry.m_aPages.IsEmpty())
			return false;

		// Already up: let the base class's own re-open path deal with it rather than refusing here.
		if (m_bIsActive)
			return true;

		if (m_UIManager && m_UIManager.IsAnyContextBlocking())
			return false;

		return true;
	}

	//-----------------------------------------------------------------------------------------------
	// LAYOUT WIRING
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Caches and wires every button, then draws the first page.
	override void OnShow()
	{
		m_wDismissButton = m_wRoot.FindAnyWidget("DismissButton");
		if (m_wDismissButton)
		{
			m_DismissAction = SCR_InputButtonComponent.Cast(m_wDismissButton.FindHandler(SCR_InputButtonComponent));
			if (m_DismissAction)
				m_DismissAction.m_OnActivated.Insert(Dismiss);
		}

		m_wNextButton = m_wRoot.FindAnyWidget("NextButton");
		if (m_wNextButton)
		{
			m_NextAction = SCR_InputButtonComponent.Cast(m_wNextButton.FindHandler(SCR_InputButtonComponent));
			if (m_NextAction)
				m_NextAction.m_OnActivated.Insert(NextPage);
		}

		m_wBackButton = m_wRoot.FindAnyWidget("BackButton");
		if (m_wBackButton)
		{
			m_BackAction = SCR_InputButtonComponent.Cast(m_wBackButton.FindHandler(SCR_InputButtonComponent));
			if (m_BackAction)
				m_BackAction.m_OnActivated.Insert(PreviousPage);
		}

		m_wLearnMoreButton = m_wRoot.FindAnyWidget("LearnMoreButton");
		if (m_wLearnMoreButton)
		{
			m_LearnMoreAction = SCR_InputButtonComponent.Cast(m_wLearnMoreButton.FindHandler(SCR_InputButtonComponent));
			if (m_LearnMoreAction)
				m_LearnMoreAction.m_OnActivated.Insert(LearnMore);
		}

		m_wDisableTipsButton = m_wRoot.FindAnyWidget("DisableTipsButton");
		if (m_wDisableTipsButton)
		{
			m_DisableTipsAction = SCR_InputButtonComponent.Cast(m_wDisableTipsButton.FindHandler(SCR_InputButtonComponent));
			if (m_DisableTipsAction)
				m_DisableTipsAction.m_OnActivated.Insert(DisableTips);
		}

		Refresh();

		FocusFirstButton();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes exactly what OnShow inserted, then settles up with the pipeline.
	//!
	//! Marking seen HERE rather than on the Dismiss button is deliberate: MenuBack / gamepad B close
	//! the popup through the base class's own close listener without ever reaching a button handler,
	//! so a button-side implementation would silently fail to record the most common dismissal.
	override void OnClose()
	{
		if (m_DismissAction) m_DismissAction.m_OnActivated.Remove(Dismiss);
		if (m_NextAction) m_NextAction.m_OnActivated.Remove(NextPage);
		if (m_BackAction) m_BackAction.m_OnActivated.Remove(PreviousPage);
		if (m_LearnMoreAction) m_LearnMoreAction.m_OnActivated.Remove(LearnMore);
		if (m_DisableTipsAction) m_DisableTipsAction.m_OnActivated.Remove(DisableTips);

		m_DismissAction = null;
		m_NextAction = null;
		m_BackAction = null;
		m_LearnMoreAction = null;
		m_DisableTipsAction = null;

		m_wDismissButton = null;
		m_wNextButton = null;
		m_wBackButton = null;
		m_wLearnMoreButton = null;
		m_wDisableTipsButton = null;

		string entryId = "";

		// A close the player did not ask for is not a reading. The UI manager force-closes every
		// context on death, and the welcome sequence dying with its owner should get another chance.
		if (m_Entry && IsLocalPlayerAlive())
			entryId = m_Entry.m_sId;

		ReleaseToPipeline(entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! Redraws everything from m_Entry and m_iPageIndex. Safe to call at any time.
	protected void Refresh()
	{
		if (!m_wRoot) return;
		if (!m_bIsActive) return;
		if (!m_Entry) return;

		int pageCount = 0;
		if (m_Entry.m_aPages)
			pageCount = m_Entry.m_aPages.Count();

		if (pageCount < 1) return;

		if (m_iPageIndex < 0)
			m_iPageIndex = 0;

		if (m_iPageIndex > pageCount - 1)
			m_iPageIndex = pageCount - 1;

		ApplyTitle();
		ApplyPage(m_Entry.m_aPages[m_iPageIndex]);
		ApplyPageIndicator(pageCount);
		RefreshButtons(pageCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws the entry title, falling back to a generic heading when the entry declares none.
	protected void ApplyTitle()
	{
		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget("TutorialTitle"));
		if (!title)
			return;

		if (m_Entry.m_sTitle == "")
		{
			title.SetText("#OVT-Tutorial_Title");
			return;
		}

		title.SetText(m_Entry.m_sTitle);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws one page's body and optional illustration.
	//! \param[in] page The page to draw. Null blanks the body rather than erroring.
	protected void ApplyPage(OVT_TutorialPage page)
	{
		RichTextWidget body = RichTextWidget.Cast(m_wRoot.FindAnyWidget("TutorialBody"));
		ImageWidget image = ImageWidget.Cast(m_wRoot.FindAnyWidget("TutorialImage"));

		if (body)
		{
			if (page)
			{
				body.SetText(page.m_sBody);
			}
			else
			{
				body.SetText("");
			}
		}

		if (!image)
			return;

		if (!page || page.m_sImage == ResourceName.Empty)
		{
			image.SetVisible(false);
			return;
		}

		image.LoadImageTexture(0, page.m_sImage);
		image.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws "n / total", or hides the counter entirely on a single-page entry.
	//! \param[in] pageCount How many pages the entry has.
	protected void ApplyPageIndicator(int pageCount)
	{
		TextWidget indicator = TextWidget.Cast(m_wRoot.FindAnyWidget("TutorialPageIndicator"));
		if (!indicator)
			return;

		if (pageCount < 2)
		{
			indicator.SetVisible(false);
			return;
		}

		indicator.SetVisible(true);
		indicator.SetTextFormat("#OVT-Tutorial_PageIndicator", m_iPageIndex + 1, pageCount);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows, hides and relabels the buttons for the current page.
	//!
	//! Visibility is not cosmetic here: SCR_InputButtonComponent.OnInput() refuses to fire for a
	//! widget that is not visible in its hierarchy, so hiding Back on page one is also what makes
	//! its shortcut do nothing there. That is the supported way to retire a shortcut contextually.
	//! \param[in] pageCount How many pages the entry has.
	protected void RefreshButtons(int pageCount)
	{
		bool isSequence = pageCount > 1;
		bool isLastPage = m_iPageIndex >= pageCount - 1;

		// Next/Back exist only for sequences. On a single-page tip the only control is Dismiss, which
		// is MenuBack - the key and the pad button every player tries first.
		if (m_wNextButton)
			m_wNextButton.SetVisible(isSequence);

		if (m_wBackButton)
			m_wBackButton.SetVisible(isSequence);

		// Back is present but dead on page one, so the row does not reflow as the player pages.
		if (m_BackAction)
			m_BackAction.SetEnabled(m_iPageIndex > 0);

		// On the last page Next stops meaning "forward" and becomes the finish action, so it says so.
		if (m_NextAction)
		{
			if (isLastPage)
			{
				m_NextAction.SetLabel("#OVT-Tutorial_Finish");
			}
			else
			{
				m_NextAction.SetLabel("#OVT-Tutorial_Next");
			}
		}

		if (m_wLearnMoreButton)
			m_wLearnMoreButton.SetVisible(m_Entry.m_sFieldManualTitleKey != "");
	}

	//------------------------------------------------------------------------------------------------
	//! Puts keyboard/gamepad focus somewhere useful so a pad player is not stranded with no selection.
	protected void FocusFirstButton()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (m_wNextButton && m_wNextButton.IsVisible())
		{
			workspace.SetFocusedWidget(m_wNextButton);
			return;
		}

		if (m_wDismissButton)
			workspace.SetFocusedWidget(m_wDismissButton);
	}

	//-----------------------------------------------------------------------------------------------
	// ACTIONS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Next page, or finish when there is no next page.
	//!
	//! Signature matches the codebase's established SCR_InputButtonComponent handler shape - the
	//! invoker calls (component, actionName) positionally and ScriptInvoker does not type-check.
	//! \param[in] src The component that fired. Unused.
	//! \param[in] value Unused.
	//! \param[in] reason Unused.
	void NextPage(Widget src = null, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if (!m_bIsActive) return;
		if (!m_Entry || !m_Entry.m_aPages) return;

		int pageCount = m_Entry.m_aPages.Count();

		if (m_iPageIndex >= pageCount - 1)
		{
			// Last page: Next is the finish action (plan item F6).
			CloseLayout();
			return;
		}

		m_iPageIndex++;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Previous page. Cannot page past the start.
	//! \param[in] src The component that fired. Unused.
	//! \param[in] value Unused.
	//! \param[in] reason Unused.
	void PreviousPage(Widget src = null, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if (!m_bIsActive) return;
		if (m_iPageIndex <= 0) return;

		m_iPageIndex--;

		Refresh();

		// Back is disabled on page one, and SCR_InputButtonComponent.SetEnabled disables the widget
		// itself - which can strand gamepad focus on a control that cannot respond. Hand focus to
		// Next at the exact moment that happens.
		if (m_iPageIndex == 0)
			FocusFirstButton();
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the popup and records the entry as read. Idempotent - CloseLayout guards itself, and
	//! MenuBack legitimately reaches both this handler and the base class's close listener.
	//! \param[in] src The component that fired. Unused.
	//! \param[in] value Unused.
	//! \param[in] reason Unused.
	void Dismiss(Widget src = null, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		CloseLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! Turns every future tip off for this machine, then closes.
	//!
	//! The flag is written straight through to the player's profile by
	//! OVT_TutorialComponent.SetTipsDisabled, which also empties the pending queue - tips queued under
	//! the old setting reappearing the moment the player turns tips back on would make the toggle look
	//! broken. Two-way by construction: writing false later really does clear the stored true.
	//! \param[in] src The component that fired. Unused.
	//! \param[in] value Unused.
	//! \param[in] reason Unused.
	void DisableTips(Widget src = null, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		OVT_TutorialComponent tutorials = m_Tutorials;
		if (!tutorials)
			tutorials = OVT_Global.GetTutorials();

		if (tutorials)
			tutorials.SetTipsDisabled(true);

		// The flag is written FIRST on purpose. GetGame().SaveUserSettings() is throttled and DROPS
		// rather than defers, so of the two flushes this method causes - the flag here, and this
		// entry's seen id from OnClose a moment later - only the first is guaranteed to reach disk.
		// Losing the seen id costs nothing: tips are off, and the accessor rewrites the whole record
		// on the next mutation. Losing the flag would silently un-disable tips on the next launch.
		CloseLayout();
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the Field Manual on the page this entry links to, and closes this popup behind it.
	//!
	//! THE POPUP MUST NOT SURVIVE THE JUMP, and not merely for tidiness. An OVT_UIContext re-activates
	//! its own action context every frame it is active (OVT_UIContext.EOnFrame:109-117), so leaving this
	//! one up would keep OverthrowTutorialMenuContext live underneath the Field Manual - and both bind
	//! MenuBack and MenuSelect. On a gamepad, B would close the tip instead of the manual's reading
	//! panel. Closing also marks the entry seen through OnClose, which is right: following a tip's link
	//! is the strongest possible evidence the player read it.
	//!
	//! The order is open-then-close, not close-then-open. Both leave the screen in the same state, but
	//! closing first would need a re-show on the failure path, and a re-show runs back through the
	//! pipeline handback OnClose just performed. Opening first has no such path: if the manual will not
	//! open - a dedicated server has no menu manager, and a menu can refuse to be replaced - the popup
	//! is left exactly as it was, with its Dismiss button intact, and OVT_FieldManualHelper has already
	//! logged why.
	//!
	//! An unknown key is not an error either: the manual opens on its front page and a warning names
	//! the key (plan integration criterion I2).
	//! \param[in] src The component that fired. Unused.
	//! \param[in] value Unused.
	//! \param[in] reason Unused.
	void LearnMore(Widget src = null, float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if (!m_Entry) return;

		string titleKey = m_Entry.m_sFieldManualTitleKey;
		if (titleKey == "") return;

		if (!OVT_FieldManualHelper.Open(titleKey))
			return;

		CloseLayout();
	}

	//-----------------------------------------------------------------------------------------------
	// PIPELINE HANDBACK
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Tells the pipeline this surface is free again.
	//! \param[in] entryId Id to record as seen, or "" to release without recording anything.
	protected void ReleaseToPipeline(string entryId)
	{
		OVT_TutorialComponent tutorials = m_Tutorials;
		if (!tutorials)
			tutorials = OVT_Global.GetTutorials();

		if (!tutorials)
			return;

		tutorials.NotifyDismissed(entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the owning character is alive. A close driven by death must not be counted
	//! as the player having read the tip.
	protected bool IsLocalPlayerAlive()
	{
		if (!m_Controller)
			return false;

		return m_Controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}
}
