//------------------------------------------------------------------------------------------------
//! The non-modal tutorial presentation: a HUD overlay that shows one entry and then gets out of
//! the way.
//!
//! It is deliberately the DUMBEST of the two surfaces. Every decision about whether an entry may
//! be shown at all has already been made by OVT_TutorialComponent's pipeline (seen store, tips
//! flag, queue, gate) before m_OnShowTutorial fires; this class only draws the thing, counts down,
//! and reports back. Its whole contract with the pipeline is two calls:
//!
//!   in : OVT_TutorialComponent.m_OnShowTutorial(OVT_TutorialEntryConfig)
//!   out: OVT_TutorialComponent.NotifyDismissed(entryId)
//!
//! NotifyDismissed("") is a real and load-bearing case, not a mistake: it hands the pipeline back
//! ("nothing is on screen any more") WITHOUT recording the entry as seen. Every path that ends a
//! popup for a reason that is not the player having read it uses that form.
//!
//! GENUINELY NON-MODAL (plan item F3). This class registers no input listener, activates no
//! ActionContext and adds no keybinding. The one SCR_InputButtonComponent in the layout is bound
//! to the EXISTING OverthrowMainMenu action, which is live in OverthrowGeneralContext every frame
//! whether this popup is on screen or not - so the popup cannot steal movement, aim or fire, and
//! cannot change what any input already did. See the R3 note on OnMoreActivated().
//------------------------------------------------------------------------------------------------
class OVT_TutorialInfo : SCR_InfoDisplay
{
	//! How long a popup stays up before it retires itself, in milliseconds. Matches the duration
	//! the legacy #OVT-IntroHint used, so the pacing is one players have already been taught.
	static const int AUTO_DISMISS_MS = 20000;

	//! Cadence of the countdown bar and the "has a menu opened?" poll, in milliseconds.
	//!
	//! Driven from the call queue rather than UpdateValues() on purpose: several Overthrow contexts
	//! open with m_bHideHUDOnShow, and a hidden HUD is exactly the moment this popup most needs to
	//! notice and retire itself.
	static const int TICK_MS = 100;

	//! Retry cadence for resolving the tutorial component, in milliseconds.
	static const int SUBSCRIBE_RETRY_MS = 1000;

	//! Hard ceiling on those retries. OVT_OverthrowController is registered by an async
	//! RpcDo_NotifyOwnerAssignment, so the component legitimately does not exist for the first
	//! seconds of a session; after a minute it never will, and a timer that retries forever is a
	//! leak rather than a fallback.
	static const int SUBSCRIBE_MAX_ATTEMPTS = 60;

	//! Root of everything this display draws. Hidden whenever no entry is on screen.
	protected Widget m_wFrame;

	//! Entry title line.
	protected TextWidget m_wTitle;

	//! First (and, for a non-modal entry, only) page of body text.
	protected RichTextWidget m_wBody;

	//! Optional per-page illustration. Hidden when the page declares no image.
	protected ImageWidget m_wImage;

	//! The one input prompt on the popup.
	protected Widget m_wMoreButton;

	//! Handler on m_wMoreButton. Subscribed in OnStartDraw, removed in OnStopDraw.
	protected SCR_InputButtonComponent m_MoreAction;

	//! Visual countdown to the auto-dismiss.
	protected SCR_WLibProgressBarComponent m_Timer;

	//! The component whose invoker this display is subscribed to. Held so OnStopDraw can remove
	//! itself from the SAME invoker it inserted into, even if the local controller has changed.
	protected OVT_TutorialComponent m_Tutorials;

	//! Id of the entry currently on screen. "" when nothing is showing.
	protected string m_sEntryId;

	//! The entry currently on screen. Held so the escalation prompt can hand the whole entry to the
	//! modal surface, which needs the pages and the field-manual key, not just the id.
	protected OVT_TutorialEntryConfig m_Entry;

	//! Milliseconds left before the auto-dismiss.
	protected int m_iRemainingMs;

	//! How many times the subscribe retry has run.
	protected int m_iSubscribeAttempts;

	//! True while the TICK_MS timer is registered on the call queue.
	protected bool m_bTicking;

	//! True while the subscribe retry timer is registered on the call queue.
	protected bool m_bSubscribeRetryRunning;

	//-----------------------------------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Caches every widget, subscribes to the pipeline and starts hidden.
	//! \param[in] owner The character this HUD belongs to.
	protected override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);

		if (!m_wRoot)
			return;

		CacheWidgets();

		// Hidden by default. The layout also ships with "Is Visible" 0 on the frame, but a HUD that
		// depended on an authored flag for correctness would fail silently the day someone toggles
		// it in the Workbench.
		HidePopup();

		m_iSubscribeAttempts = 0;
		StartSubscribeRetry();
		TrySubscribe();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes everything OnStartDraw added: both timers, the invoker subscription and the button
	//! handler. A leaked m_OnShowTutorial subscription would compound once per respawn.
	//! \param[in] owner The character this HUD belonged to.
	protected override event void OnStopDraw(IEntity owner)
	{
		// A popup that was on screen when the character went away was not read, so hand the
		// pipeline back WITHOUT marking the entry seen - it should get another chance after the
		// respawn. Clearing the flag matters either way: the pipeline shows nothing at all while it
		// believes something is still on screen.
		if (m_sEntryId != "")
			Release("");

		StopTick();
		StopSubscribeRetry();
		Unsubscribe();

		if (m_MoreAction)
			m_MoreAction.m_OnActivated.Remove(OnMoreActivated);

		m_MoreAction = null;
		m_wMoreButton = null;
		m_wFrame = null;
		m_wTitle = null;
		m_wBody = null;
		m_wImage = null;
		m_Timer = null;
		m_sEntryId = "";
		m_Entry = null;

		super.OnStopDraw(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves every widget this display touches. Each lookup is guarded so a layout that has
	//! drifted degrades to a missing element rather than a script error on every HUD frame (Q7).
	protected void CacheWidgets()
	{
		m_wFrame = m_wRoot.FindAnyWidget("TutorialFrame");
		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("TutorialTitle"));
		m_wBody = RichTextWidget.Cast(m_wRoot.FindAnyWidget("TutorialBody"));
		m_wImage = ImageWidget.Cast(m_wRoot.FindAnyWidget("TutorialImage"));

		Widget timerWidget = m_wRoot.FindAnyWidget("TutorialTimer");
		if (timerWidget)
			m_Timer = SCR_WLibProgressBarComponent.Cast(timerWidget.FindHandler(SCR_WLibProgressBarComponent));

		m_wMoreButton = m_wRoot.FindAnyWidget("TutorialMoreButton");
		if (m_wMoreButton)
		{
			m_MoreAction = SCR_InputButtonComponent.Cast(m_wMoreButton.FindHandler(SCR_InputButtonComponent));
			if (m_MoreAction)
				m_MoreAction.m_OnActivated.Insert(OnMoreActivated);
		}
	}

	//-----------------------------------------------------------------------------------------------
	// PIPELINE SUBSCRIPTION
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One attempt at subscribing to the local player's tutorial component.
	//!
	//! Retried rather than attempted once because the HUD starts drawing the moment the character
	//! is controlled, which can be before the owning controller has been registered. Giving up
	//! after one miss is the bug OVT_ProgressInfo has; a tip fired in the first seconds of a session
	//! would then have nowhere to be drawn for the whole session.
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
			m_Tutorials.m_OnShowTutorial.Insert(OnShowTutorial);
			StopSubscribeRetry();
			return;
		}

		m_iSubscribeAttempts++;

		// Silent give-up. A dedicated server, a spectator and a player who left during the countdown
		// all reach this path, so a log line here would be noise rather than diagnosis.
		if (m_iSubscribeAttempts >= SUBSCRIBE_MAX_ATTEMPTS)
			StopSubscribeRetry();
	}

	//------------------------------------------------------------------------------------------------
	//! Removes this display from the invoker it actually inserted into.
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
	//! Stops the subscribe retry timer. Idempotent, and safe from OnStopDraw.
	protected void StopSubscribeRetry()
	{
		if (!m_bSubscribeRetryRunning)
			return;

		m_bSubscribeRetryRunning = false;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(TrySubscribe);
	}

	//-----------------------------------------------------------------------------------------------
	// SHOWING
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The pipeline cleared an entry to be shown.
	//!
	//! Every branch here must end in a popup on screen, in a call to Release(), or in the MODAL
	//! surface taking the entry instead - because OVT_TutorialComponent.Pump() sets its "something is
	//! showing" flag BEFORE invoking and only a surface can clear it. A branch that just returns with
	//! nobody else on the hook would jam the queue for the session.
	//! \param[in] entry The entry to present. Null is treated as a failed presentation.
	protected void OnShowTutorial(OVT_TutorialEntryConfig entry)
	{
		if (!entry)
		{
			Release("");
			return;
		}

		int pageCount = 0;
		if (entry.m_aPages)
			pageCount = entry.m_aPages.Count();

		// OVT_TutorialContext is subscribed to this SAME invoker and takes MODAL entries plus every
		// multi-page entry (plan decision D10). Both surfaces ask OVT_TutorialContext.IsModal(), so
		// there is exactly one copy of that rule and the two cannot disagree.
		//
		// The decline path deliberately does NOT Release(): Release clears the pipeline's "something
		// is showing" flag, and clearing it out from under the modal that is about to open would let
		// the next pump push a second entry on top of it. Invoker order between the two surfaces is
		// not guaranteed, so this has to be safe in both orders - and it is, because the surface that
		// declines does nothing at all.
		//
		// The one case that must still release is a modal entry with no modal surface wired up: a
		// dropped tip is a non-event, a jammed pipeline costs the player every later tip.
		if (OVT_TutorialContext.IsModal(entry))
		{
			OVT_TutorialContext modal = FindModalContext();
			if (modal && modal.IsPipelineSubscribed())
				return;

			Release("");
			return;
		}

		// No layout, no popup - and specifically no false "seen" for an entry nobody could read.
		// An empty id is refused for the same reason plus a mechanical one: "" is this class's
		// marker for "nothing on screen", so an entry carrying it would stop its own countdown.
		if (!m_wFrame || pageCount < 1 || entry.m_sId == "")
		{
			Release("");
			return;
		}

		OVT_TutorialPage page = entry.m_aPages[0];
		if (!page)
		{
			Release("");
			return;
		}

		// Unreachable while the pipeline's gate holds (CanShowNow refuses a second entry while one
		// is showing), but if it ever stops holding, one consistent popup beats two overlapping
		// states. Deliberately does not touch the seen store: the displaced entry was not read.
		HidePopup();

		m_sEntryId = entry.m_sId;
		m_Entry = entry;

		ApplyTitle(entry.m_sTitle);
		ApplyBody(page.m_sBody);
		ApplyImage(page.m_sImage);

		m_iRemainingMs = AUTO_DISMISS_MS;
		if (m_Timer)
			m_Timer.SetValue(1, false);

		m_wFrame.SetVisible(true);

		StartTick();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] titleKey #OVT- localization key, or "" to hide the title line.
	protected void ApplyTitle(string titleKey)
	{
		if (!m_wTitle)
			return;

		if (titleKey == "")
		{
			m_wTitle.SetVisible(false);
			return;
		}

		m_wTitle.SetVisible(true);
		m_wTitle.SetText(titleKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] bodyKey #OVT- localization key, or "" to hide the body block.
	protected void ApplyBody(string bodyKey)
	{
		if (!m_wBody)
			return;

		if (bodyKey == "")
		{
			m_wBody.SetVisible(false);
			return;
		}

		m_wBody.SetVisible(true);
		m_wBody.SetText(bodyKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] image Texture resource for this page, or ResourceName.Empty for none.
	protected void ApplyImage(ResourceName image)
	{
		if (!m_wImage)
			return;

		if (image == ResourceName.Empty)
		{
			m_wImage.SetVisible(false);
			return;
		}

		m_wImage.LoadImageTexture(0, image);
		m_wImage.SetVisible(true);
	}

	//-----------------------------------------------------------------------------------------------
	// COUNTDOWN AND DISMISSAL
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One countdown step: retire the popup if a blocking UI opened, otherwise advance the bar.
	protected void Tick()
	{
		if (m_sEntryId == "")
		{
			StopTick();
			return;
		}

		// Plan item F4: the popup disappears the instant the player opens the map or any menu, and
		// is marked seen when it does. It has been read or deliberately walked away from; either
		// way, bringing it back over the menu the player just opened is the annoyance this whole
		// feature exists to remove.
		if (IsBlockingUiOpen())
		{
			Dismiss();
			return;
		}

		m_iRemainingMs -= TICK_MS;

		if (m_Timer)
		{
			// Assigned to a float FIRST: int / int is integer division in EnforceScript, so the
			// obvious one-liner would step straight from 1 to 0 and never draw a countdown.
			float remaining = m_iRemainingMs;
			float fraction = remaining / AUTO_DISMISS_MS;
			if (fraction < 0)
				fraction = 0;

			m_Timer.SetValue(fraction, false);
		}

		if (m_iRemainingMs <= 0)
			Dismiss();
	}

	//------------------------------------------------------------------------------------------------
	//! Retires the popup and records the entry as seen. Idempotent.
	protected void Dismiss()
	{
		string entryId = m_sEntryId;

		HidePopup();

		Release(entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! Hides the popup and stops the countdown, without touching the seen store.
	protected void HidePopup()
	{
		StopTick();

		m_sEntryId = "";
		m_Entry = null;

		if (m_wFrame)
			m_wFrame.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the pipeline this display is free again.
	//! \param[in] entryId Id to record as seen, or "" to release without recording anything.
	protected void Release(string entryId)
	{
		m_sEntryId = "";
		m_Entry = null;

		if (!m_Tutorials)
			return;

		m_Tutorials.NotifyDismissed(entryId);
	}

	//------------------------------------------------------------------------------------------------
	//! The modal presentation registered on this player, if any.
	//! \return OVT_TutorialContext, or null when the player prefab does not carry it.
	protected OVT_TutorialContext FindModalContext()
	{
		OVT_UIManagerComponent ui = GetLocalUIManager();
		if (!ui)
			return null;

		return OVT_TutorialContext.Cast(ui.GetContext(OVT_TutorialContext));
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the countdown timer. Idempotent.
	protected void StartTick()
	{
		if (m_bTicking)
			return;

		m_bTicking = true;

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the countdown timer. Idempotent, and safe from OnStopDraw.
	protected void StopTick()
	{
		if (!m_bTicking)
			return;

		m_bTicking = false;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
			queue.Remove(Tick);
	}

	//-----------------------------------------------------------------------------------------------
	// ESCALATION
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The player used the prompt on the popup.
	//!
	//! R3 FALLBACK, TAKEN DELIBERATELY, AND RE-CONFIRMED IN PHASE 6. The plan budgeted one new
	//! gameplay-context keybinding for escalating a popup into its modal form. There is no input for
	//! it. Phase 5 found that every gamepad input is bound in some context that can be live while a
	//! non-modal HUD popup is on screen; Phase 6 closed the place/build hole that was supposed to
	//! free shoulder_left (OVT_UIContext.IsBlockingPopups) and re-ran the survey inline-aware - and
	//! shoulder_left is still taken, by VONContext at Priority 110, which SCR_VONController.Update
	//! activates every frame the player is alive and conscious. The keyboard is no better: KC_T is
	//! VONDirect/VONDirectToggle, KC_K is GadgetCompass, KC_O is GadgetWatch. All six of those are
	//! declared INLINE inside their ActionContext blocks, which is why an ActionRefs-only survey
	//! reports them as unbound.
	//!
	//! So the prompt stays bound to the EXISTING OverthrowMainMenu action, which already has both a
	//! keyboard (KC_U) and a gamepad (pad_down) source and is already live every frame through
	//! OverthrowGeneralContext. Nothing is stolen: the main menu opens exactly as it always did.
	//!
	//! What this handler adds is the handover. The entry is pushed into OVT_TutorialContext before
	//! the popup lets go of it, so the Tips entry in the main menu that is opening right now has
	//! something to show. The pipeline is released with "" rather than with the id: the player asked
	//! to read MORE of this tip, so it has not been read and must not be recorded as seen. StopTick()
	//! inside HidePopup() cancels the auto-dismiss, so the countdown cannot fire behind the menu.
	//! \param[in] button The component that fired. Unused.
	//! \param[in] action Name of the action that fired. Unused.
	protected void OnMoreActivated(SCR_InputButtonComponent button, string action)
	{
		if (m_sEntryId == "")
			return;

		OVT_TutorialContext modal = FindModalContext();
		if (modal)
			modal.SetEntry(m_Entry);

		HidePopup();

		Release("");
	}

	//-----------------------------------------------------------------------------------------------
	// GATE INPUTS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether something is on screen that this popup must not sit under.
	//!
	//! Deliberately the same three facts, in the same order, that OVT_TutorialComponent folds into
	//! the pipeline gate. Duplicated rather than shared because the component's version is
	//! protected, and because the two answer different questions at different times: the gate
	//! decides whether a popup may START, this decides whether one must STOP.
	//!
	//! Placement and building count as blocking (OVT_UIContext.IsBlockingPopups), which is what
	//! retires an on-screen popup the moment the player starts positioning a ghost.
	//! \return True while a popup would be intrusive. An unresolvable fact reads as not blocking.
	protected bool IsBlockingUiOpen()
	{
		OVT_UIManagerComponent ui = GetLocalUIManager();
		if (ui && ui.IsAnyContextBlocking())
			return true;

		MenuManager menus = GetGame().GetMenuManager();
		if (menus && menus.GetTopMenu())
			return true;

		// NOT named `map`: EnforceScript reserves that as a type name and rejects the local.
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity && mapEntity.IsOpen())
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the local player's UI manager without OVT_Global.GetUI(), which dereferences the
	//! controlled entity unguarded and would throw between respawns.
	//! \return The UI manager, or null.
	protected OVT_UIManagerComponent GetLocalUIManager()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return null;

		return OVT_UIManagerComponent.Cast(player.FindComponent(OVT_UIManagerComponent));
	}

	//-----------------------------------------------------------------------------------------------
	// GETTERS
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return Id of the entry currently on screen, or "" when nothing is showing.
	string GetShownEntryId()
	{
		return m_sEntryId;
	}
}
