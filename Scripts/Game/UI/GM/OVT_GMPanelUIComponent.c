//------------------------------------------------------------------------------------------------
//! Root component of UI/Layouts/GM/GMPanel.layout - the read-only Overthrow readout docked above the
//! Game Master mode menu in the editor's bottom-left stack.
//!
//! LIFETIME IS ENGINE-SCOPED. The panel is created by the modded SCR_EditModeEditorUIComponent when
//! EDIT mode activates and destroyed with the mode's widget tree when it deactivates
//! (SCR_MenuLayoutEditorComponent.EOnEditorPostActivate :87 / EOnEditorPostDeactivate :109). This
//! component therefore asks nothing about whether the editor is open; it only caches its widgets on
//! attach and drops them on detach.
//!
//! EVERY ROW IS PRE-AUTHORED. Values are written with SetText into named widgets that already exist
//! in the layout. Nothing is created, destroyed or shown/hidden per frame, so the one-second
//! countdown tick added in Phase 2 costs three SetText calls and no re-layout.
//!
//! This component sends and receives nothing; new data goes on gm-state's fan.
//! No remote-procedure call, no replicated property and no replication receiver may ever appear in
//! this file - a new number belongs on OVT_GMRequestComponent's snapshot, which is gated by
//! gm-state's authorization check. (The rule is spelled out rather than quoted so the Definition of
//! Done's grep gate stays clean.)
//!
//! THE SEAM IS RESOLVED PER RENDER, NEVER CACHED. OVT_ControllerComponent<T>.Get() is null on a
//! dedicated server, before ownership assignment and when the prefab block is missing
//! (OVT_ControllerComponent.c:22-29), and ownership can be assigned after this panel attaches. A
//! cached null would therefore be permanent; a fresh lookup costs one component search per second.
//!
//! THE DETAIL SEAM IS EMPTY BY DESIGN. GetInstance(), GetDetailSlot(), ShowDetail() and ClearDetail()
//! hand hud-icons an authored, empty, collapsed container and nothing more. This panel owns the slot's
//! existence and lifecycle; whatever goes in it - its layout, its data and its refresh - belongs to the
//! caller, and no assumption about it is made here (D5).
//------------------------------------------------------------------------------------------------
class OVT_GMPanelUIComponent : SCR_ScriptedWidgetComponent
{
	//! How often the two countdown strings and the QRF row are redrawn, in milliseconds. The store's
	//! countdown readers extrapolate locally and clamp at zero, so this rate is free of the poll.
	protected static const int TICK_INTERVAL_MS = 1000;

	//! Overthrow's accent, used for a countdown that will not fire.
	protected static ref Color s_ColorSuppressed = new Color(0.761, 0.392, 0.08, 1);

	//! Shown in a locally-sourced row whose manager has not resolved yet. Matches the layout's
	//! authored placeholder, so an unresolved manager looks like "not yet", not like zero.
	protected static const string PLACEHOLDER = "-";

	//! The live panel, or null when EDIT mode is not active. Exposed to hud-icons via GetInstance();
	//! it must never be cached across frames by a caller.
	protected static OVT_GMPanelUIComponent s_Instance;

	//! Campaign-wide numbers from the gm-state store. Hidden until a snapshot lands (D6).
	protected Widget m_wCampaignSection;

	//! "Waiting for campaign data" line, shown in place of m_wCampaignSection (D6).
	protected Widget m_wNoDataLabel;

	//! Locally replicated numbers (resistance funds, QRF). Always visible - they are known without a
	//! snapshot, so blanking them alongside the campaign block would be its own lie (D6).
	protected Widget m_wLocalSection;

	//! Empty, collapsed container whose content is owned by hud-icons (D5). Exposed through
	//! GetDetailSlot(); this component never puts anything in it.
	protected Widget m_wDetailSection;

	protected TextWidget m_wValueThreat;
	protected TextWidget m_wValueOFResources;
	protected TextWidget m_wValueOFDeployment;
	protected TextWidget m_wValueDistribution;
	protected TextWidget m_wStatusDistribution;
	protected TextWidget m_wValuePayout;
	protected TextWidget m_wStatusPayout;
	protected TextWidget m_wValueFunds;
	protected TextWidget m_wValueQRF;

	//! The Status widgets' authored text colour, read once at attach so suppression can be undone
	//! without hardcoding a guess at what the layout specified.
	protected ref Color m_ColorStatusDefault;

	//! Whether Status_Distribution is currently drawn in the suppressed colour. Tracked so the tick
	//! only touches the colour at a state change.
	protected bool m_bDistributionSuppressedShown;

	//! Whether Status_Payout is currently drawn in the suppressed colour.
	protected bool m_bPayoutSuppressedShown;

	//------------------------------------------------------------------------------------------------
	//! Caches every named widget once, subscribes to the three data sources, renders immediately and
	//! starts the one-second tick.
	//!
	//! The immediate render matters: switching EDIT -> PHOTO -> EDIT rebuilds this panel while the
	//! seam keeps polling, so waiting for the next snapshot would leave a rebuilt panel empty for up
	//! to eight seconds with live data already in the store.
	//! \param[in] w Layout root, supplied by the widget system.
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		if (!m_wRoot)
			return;

		CacheWidgets();

		if (m_wStatusDistribution)
			m_ColorStatusDefault = m_wStatusDistribution.GetColor();

		s_Instance = this;

		Subscribe();

		RenderAll();

		GetGame().GetCallqueue().CallLater(TickCountdowns, TICK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every reference this component holds.
	//!
	//! The ordering that matters is that nothing outlives m_wRoot: the tick and all three
	//! subscriptions go first, then the widget references. Hygiene template:
	//! SCR_BudgetEditorUIComponent.c :207 / :240.
	//! \param[in] w Layout root, supplied by the widget system.
	override void HandlerDeattached(Widget w)
	{
		GetGame().GetCallqueue().Remove(TickCountdowns);

		Unsubscribe();

		m_ColorStatusDefault = null;
		m_bDistributionSuppressedShown = false;
		m_bPayoutSuppressedShown = false;

		if (s_Instance == this)
			s_Instance = null;

		m_wCampaignSection = null;
		m_wNoDataLabel = null;
		m_wLocalSection = null;
		m_wDetailSection = null;

		m_wValueThreat = null;
		m_wValueOFResources = null;
		m_wValueOFDeployment = null;
		m_wValueDistribution = null;
		m_wStatusDistribution = null;
		m_wValuePayout = null;
		m_wStatusPayout = null;
		m_wValueFunds = null;
		m_wValueQRF = null;

		m_wRoot = null;

		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves every widget the panel writes to, once. A missing widget degrades that row to
	//! "never updated" rather than throwing on every render, so nothing here is fatal.
	protected void CacheWidgets()
	{
		m_wCampaignSection = m_wRoot.FindAnyWidget("CampaignSection");
		m_wNoDataLabel = m_wRoot.FindAnyWidget("NoDataLabel");
		m_wLocalSection = m_wRoot.FindAnyWidget("LocalSection");
		m_wDetailSection = m_wRoot.FindAnyWidget("DetailSection");

		m_wValueThreat = FindText("Value_Threat");
		m_wValueOFResources = FindText("Value_OFResources");
		m_wValueOFDeployment = FindText("Value_OFDeployment");
		m_wValueDistribution = FindText("Value_Distribution");
		m_wStatusDistribution = FindText("Status_Distribution");
		m_wValuePayout = FindText("Value_Payout");
		m_wStatusPayout = FindText("Status_Payout");
		m_wValueFunds = FindText("Value_Funds");
		m_wValueQRF = FindText("Value_QRF");
	}

	//------------------------------------------------------------------------------------------------
	//! Null-safe named TextWidget lookup under the panel root.
	//! \param[in] name Widget name authored in GMPanel.layout.
	//! \return The widget, or null if the layout no longer carries it.
	protected TextWidget FindText(string name)
	{
		if (!m_wRoot)
			return null;

		Widget w = m_wRoot.FindAnyWidget(name);
		if (!w)
		{
			Print("OVT_GMPanelUIComponent: GMPanel.layout has no widget named " + name, LogLevel.WARNING);
			return null;
		}

		return TextWidget.Cast(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Swaps the campaign block for the "waiting for campaign data" line. Reached before the first
	//! snapshot, on an unauthorized client, and after the store is cleared - all normal, none an
	//! error, which is why the copy reads as waiting rather than as broken (D6).
	protected void ShowNoData()
	{
		if (m_wCampaignSection)
			m_wCampaignSection.SetVisible(false);

		if (m_wNoDataLabel)
			m_wNoDataLabel.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Swaps the "waiting" line for the campaign block. Phase 2 calls this from RenderAll().
	protected void ShowCampaignData()
	{
		if (m_wNoDataLabel)
			m_wNoDataLabel.SetVisible(false);

		if (m_wCampaignSection)
			m_wCampaignSection.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	// DETAIL SEAM (owned by hud-icons - D5)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The live panel component, or null.
	//!
	//! MAY BE NULL, AND USUALLY IS. The panel exists only while the Game Master editor is in EDIT
	//! mode: the engine builds its widget tree on mode activation and destroys it on deactivation, and
	//! this static is set in HandlerAttached and nulled in HandlerDeattached. Every caller must
	//! null-check the result.
	//!
	//! NEVER CACHE THE INSTANCE ACROSS FRAMES. Switching EDIT -> PHOTO -> EDIT destroys this component
	//! and constructs a new one, so a stored reference goes stale without notice. Call this again each
	//! time the panel is needed; it is one static read.
	//! \return The attached panel component, or null when no panel is attached.
	static OVT_GMPanelUIComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! The container widget that detail content is parented into.
	//!
	//! This method NEVER CREATES ANYTHING. It returns the empty DetailSection container authored in
	//! GMPanel.layout and nothing else; building, filling and removing content is entirely the
	//! caller's, and the panel makes no assumption about what that content is.
	//!
	//! May return null while the panel is being torn down, or if the layout no longer carries a widget
	//! named DetailSection. Null-check before parenting into it.
	//! \return The detail container, or null when there is nothing to parent into.
	Widget GetDetailSlot()
	{
		return m_wDetailSection;
	}

	//------------------------------------------------------------------------------------------------
	//! Shows or hides the detail section.
	//!
	//! The section is authored hidden and occupies no vertical space in that state, so the panel is
	//! exactly its own height until a caller shows it. Visibility is all this does - it neither adds
	//! nor removes content, so hiding and re-showing preserves whatever was parented in.
	//!
	//! Safe to call when the slot is missing; it does nothing in that case.
	//! \param[in] show True to reveal the section, false to collapse it.
	void ShowDetail(bool show)
	{
		if (!m_wDetailSection)
			return;

		m_wDetailSection.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every child of the detail slot and hides the section.
	//!
	//! This is the whole teardown path: after it returns the slot is empty and collapsed, exactly as
	//! authored. The panel makes no assumption about what the removed widgets were, and it does not
	//! notify their owner - a caller holding references to content it parented in must drop them
	//! itself.
	//!
	//! Safe to call when the slot is missing or already empty.
	void ClearDetail()
	{
		if (!m_wDetailSection)
			return;

		// Read the sibling before unparenting, otherwise the walk loses the rest of the list.
		Widget child = m_wDetailSection.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}

		m_wDetailSection.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	// SUBSCRIPTIONS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Attaches to the two gm-state invokers and the economy's funds invoker.
	//!
	//! Both sources may be unavailable at attach time - the seam component is null on a dedicated
	//! server and before ownership assignment, and the economy manager may not have resolved - so
	//! each is guarded, and Unsubscribe() guards symmetrically.
	protected void Subscribe()
	{
		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (gm)
		{
			gm.GetOnSnapshotUpdated().Insert(OnSnapshot);
			gm.GetOnStateCleared().Insert(OnCleared);
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (economy)
			economy.m_OnResistanceMoneyChanged.Insert(OnFundsChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Detaches everything Subscribe() attached. Both invoker owners outlive this widget, so a missed
	//! removal here would wake a destroyed panel on every later snapshot.
	protected void Unsubscribe()
	{
		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (gm)
		{
			gm.GetOnSnapshotUpdated().Remove(OnSnapshot);
			gm.GetOnStateCleared().Remove(OnCleared);
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (economy)
			economy.m_OnResistanceMoneyChanged.Remove(OnFundsChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! A complete snapshot landed in the store. The invoker carries no payload; the store is read
	//! here, fresh.
	protected void OnSnapshot()
	{
		if (!m_wRoot)
			return;

		RenderAll();
	}

	//------------------------------------------------------------------------------------------------
	//! The editor closed and the store was emptied, so the campaign block must stop claiming numbers
	//! it no longer has. The locally-sourced rows are untouched - they never depended on a snapshot.
	protected void OnCleared()
	{
		if (!m_wRoot)
			return;

		ShowNoData();
	}

	//------------------------------------------------------------------------------------------------
	//! Resistance funds changed server-side. One SetText; nothing else on the panel depends on it.
	//! \param[in] amount The new resistance balance.
	protected void OnFundsChanged(int amount)
	{
		if (!m_wRoot || !m_wValueFunds)
			return;

		m_wValueFunds.SetText("$" + amount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	// RENDERING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Redraws every row. The locally-sourced rows are drawn first and unconditionally: they come
	//! from replicated managers and are known whether or not a snapshot has landed, so blanking them
	//! alongside the campaign block would be its own lie (D6).
	protected void RenderAll()
	{
		if (!m_wRoot)
			return;

		RenderFunds();
		RenderQRF();

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
		{
			ShowNoData();
			return;
		}

		OVT_GMCampaignState state = gm.GetState();
		if (!state || !state.HasData())
		{
			ShowNoData();
			return;
		}

		ShowCampaignData();

		if (m_wValueThreat)
			m_wValueThreat.SetText(OVT_GMPanelFormat.FormatThreat(state.m_fThreat));

		if (m_wValueOFResources)
			m_wValueOFResources.SetText(state.m_iOFResources.ToString());

		if (m_wValueOFDeployment)
			m_wValueOFDeployment.SetText(state.m_iOFDeploymentResources.ToString());

		// The distribution amount is occupying-faction resources, not money, so it carries no
		// currency mark; the payout is money and does.
		if (m_wValueDistribution)
			m_wValueDistribution.SetText(state.m_iDistributionAmount.ToString());

		if (m_wValuePayout)
			m_wValuePayout.SetText("$" + state.m_iPayoutAmount.ToString());

		RenderCountdowns(state);
	}

	//------------------------------------------------------------------------------------------------
	//! The one-second redraw. It writes text into three widgets that already exist and, at a
	//! suppression change, one colour - it creates, destroys, shows and hides nothing, which is what
	//! keeps a countdown that runs for as long as the editor is open free of layout work.
	//!
	//! The first line is a self-cancel guard: the editor tears the mode's widget tree down itself,
	//! and if that ever happens without HandlerDeattached firing, this tick would otherwise run
	//! forever against a dead widget.
	protected void TickCountdowns()
	{
		if (!m_wRoot)
		{
			GetGame().GetCallqueue().Remove(TickCountdowns);
			return;
		}

		RenderQRF();

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
			return;

		OVT_GMCampaignState state = gm.GetState();
		if (!state || !state.HasData())
			return;

		// With no data the campaign rows are hidden with their section; leaving them alone here is
		// deliberate, and is why this returns rather than writing a placeholder.
		RenderCountdowns(state);
	}

	//------------------------------------------------------------------------------------------------
	//! Draws both countdown status strings.
	//!
	//! The two clocks currently carry the same value by construction - both schedules fire on the
	//! same in-game six-hour marks - and they are rendered separately anyway, because the two wire
	//! fields exist so the schedules can diverge without a wire change.
	//! \param[in] state The store, already known to hold data.
	protected void RenderCountdowns(notnull OVT_GMCampaignState state)
	{
		m_bDistributionSuppressedShown = RenderStatus(m_wStatusDistribution, state.IsDistributionSuppressed(),
			"#OVT-GMPanel_SuppressedQRF", state.GetDistributionSecondsRemaining(), m_bDistributionSuppressedShown);

		m_bPayoutSuppressedShown = RenderStatus(m_wStatusPayout, state.IsPayoutSuppressed(),
			"#OVT-GMPanel_SuppressedNoPlayers", state.GetPayoutSecondsRemaining(), m_bPayoutSuppressedShown);
	}

	//------------------------------------------------------------------------------------------------
	//! One countdown status widget: either the clock, or the reason it will not fire in the accent
	//! colour. The amount beside it stays visible either way - a GM wants to know what would have
	//! landed.
	//! \param[in] widget The Status_* widget, may be null.
	//! \param[in] suppressed Whether the event will be skipped.
	//! \param[in] suppressedKey Localization key naming the reason.
	//! \param[in] seconds Seconds remaining, when not suppressed.
	//! \param[in] wasSuppressed What this widget was drawn as last time.
	//! \return The state just drawn, to be stored for the next call.
	protected bool RenderStatus(TextWidget widget, bool suppressed, string suppressedKey, float seconds, bool wasSuppressed)
	{
		if (!widget)
			return wasSuppressed;

		if (suppressed)
		{
			widget.SetTextFormat(suppressedKey);

			if (!wasSuppressed)
				widget.SetColor(s_ColorSuppressed);

			return true;
		}

		widget.SetText(OVT_GMPanelFormat.FormatCountdown(seconds));

		if (wasSuppressed && m_ColorStatusDefault)
			widget.SetColor(m_ColorStatusDefault);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The resistance balance, read straight from the economy manager. Between changes the invoker
	//! keeps this current; this is the initial draw and the redraw after a rebuild.
	protected void RenderFunds()
	{
		if (!m_wValueFunds)
			return;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			m_wValueFunds.SetText(PLACEHOLDER);
			return;
		}

		m_wValueFunds.SetText("$" + economy.GetResistanceMoney().ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! QRF status: "None" when nothing is running, otherwise points and the time left, plus the town
	//! name when the target is a town. A base-targeted QRF renders without a name because a base
	//! record carries none.
	//!
	//! The timer is broadcast rather than invoked, which is why this row is refreshed by the tick
	//! rather than by a subscription.
	protected void RenderQRF()
	{
		if (!m_wValueQRF)
			return;

		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if (!of || !of.m_bQRFActive)
		{
			m_wValueQRF.SetTextFormat("#OVT-GMPanel_QRFNone");
			return;
		}

		// The timer is milliseconds on the manager; the formatter takes seconds.
		float remaining = of.m_iQRFTimer;
		remaining = remaining / 1000;

		string countdown = OVT_GMPanelFormat.FormatCountdown(remaining);
		string townName = ResolveQRFTownName(of.m_iCurrentQRFTown);

		if (townName == string.Empty)
		{
			m_wValueQRF.SetTextFormat("#OVT-GMPanel_QRFActive", of.m_iQRFPoints, countdown);
			return;
		}

		m_wValueQRF.SetTextFormat("#OVT-GMPanel_QRFActiveTown", of.m_iQRFPoints, countdown, townName);
	}

	//------------------------------------------------------------------------------------------------
	//! The display name of a town-targeted QRF's target.
	//! \param[in] townId Town index, or -1 when the QRF targets a base.
	//! \return The town name, or an empty string when there is no town to name.
	protected string ResolveQRFTownName(int townId)
	{
		if (townId < 0)
			return string.Empty;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || !towns.m_Towns)
			return string.Empty;

		if (townId >= towns.m_Towns.Count())
			return string.Empty;

		return towns.GetTownName(townId);
	}
}
