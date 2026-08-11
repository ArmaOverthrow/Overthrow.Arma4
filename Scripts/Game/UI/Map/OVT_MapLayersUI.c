//! Map UI component owning the Overthrow map layer filter panel.
//! Entered through vanilla's own map tool menu (the icon strip on the left of the
//! fullscreen map), so it costs no new keybinding: reaching the strip and opening the
//! panel on a gamepad is the already-bound MapToolMenuFocus action.
//!
//! WALKING THE ROWS INSIDE THE PANEL IS A DIFFERENT INPUT SURFACE AND IT IS NOT FREE.
//! The map menu preset carries exactly one ActionContext - vanilla's MapContext - and
//! MapContext declares no Menu* navigation action at all, so a pad has nothing bound to
//! move focus between the rows once the panel is up. OverthrowMapLayersContext exists for
//! that and only that: it re-references the Menu* actions that already exist, above
//! MapContext's priority, and is leased one frame at a time while the panel is open.
//! See ActivateRowNavigationContext.
//!
//! ============================================================================================
//! LAYOUT <-> CODE NAME CONTRACT - UI/Layouts/Map/Core/OVT_MapLayersPanel.layout
//! ============================================================================================
//!   "ToolFramesOverlay"  vanilla MapMenu.layout, resolved BY NAME and never overridden
//!   "ToolMenuContainer"  vanilla MapMenu.layout, first dock fallback
//!   "LayersPanel"        the panel layout's own root - held as m_wPanel, never looked up
//!   "PanelTitle"         BuildPanel - SetText
//!   "OverlaysHeader"     BuildPanel - SetText
//!   "MarkersHeader"      BuildPanel - SetText
//!   "OverlayRows"        BuildRows  - parent for the canvas-overlay and player rows
//!   "TypeRows"           BuildRows  - parent for the location-type rows
//!   "FocusProxy"         OnPanelBuilt - SCR_EventHandlerComponent.GetOnFocus -> FocusFirstRow
//!
//! Every one of these is null-guarded and ERROR-logs the name it could not find. FindAnyWidget
//! returning null is a silent no-op the compiler cannot see, and this epic lost two configured
//! features to exactly that.
//! ============================================================================================
class OVT_MapLayersUI : SCR_MapUIBaseComponent
{
	[Attribute("filters", UIWidgets.EditBox, desc: "Quad name inside SCR_MapToolMenuUI.s_sToolMenuIcons used for the layers tool menu entry")]
	protected string m_sToolMenuIcon;

	[Attribute("3", UIWidgets.EditBox, desc: "Tool menu sort priority. Ascending - lower values sort HIGHER on screen. Values <= 0 clamp to 0")]
	protected int m_iSortPriority;

	[Attribute("{6A85D1E000000010}UI/Layouts/Map/Core/OVT_MapLayersPanel.layout", UIWidgets.ResourceNamePicker, desc: "Layers panel layout", params: "layout")]
	protected ResourceName m_PanelLayout;

	[Attribute("{6A85D1E000000030}UI/Layouts/Map/Core/OVT_MapLayerRow.layout", UIWidgets.ResourceNamePicker, desc: "Layout for one filter row (icon, label, checkbox)", params: "layout")]
	protected ResourceName m_RowLayout;

	//! Vanilla MapMenu.layout dock target - the overlay the journal, task list and weather frames live in
	protected const string WIDGET_DOCK_OVERLAY = "ToolFramesOverlay";
	//! First fallback - FastTravelMapMenu.layout has no ToolFramesOverlay and parents its frames here
	protected const string WIDGET_DOCK_CONTAINER = "ToolMenuContainer";

	protected const string WIDGET_PANEL_TITLE = "PanelTitle";
	protected const string WIDGET_OVERLAYS_HEADER = "OverlaysHeader";
	protected const string WIDGET_MARKERS_HEADER = "MarkersHeader";
	protected const string WIDGET_OVERLAY_ROWS = "OverlayRows";
	protected const string WIDGET_TYPE_ROWS = "TypeRows";
	protected const string WIDGET_FOCUS_PROXY = "FocusProxy";

	protected const string LABEL_PANEL_TITLE = "#OVT-Map_Layers_Title";
	protected const string LABEL_OVERLAYS_HEADER = "#OVT-Map_Layers_Overlays";
	protected const string LABEL_MARKERS_HEADER = "#OVT-Map_Layers_Markers";
	protected const string LABEL_PLAYERS = "#OVT-Map_Layer_Players";

	//! Layer-namespaced id of the one hand-built row. OVT_MapPlayerLocation is a
	//! SCR_MapUIBaseComponent rather than an OVT_MapCanvasLayer, so it never appears in the
	//! compositor's layer list and cannot be enumerated with the others.
	protected const string KEY_PLAYERS = "players";

	//! Overthrow's row-navigation input context, declared in Configs/System/chimeraInputCommon.conf.
	//! It re-references MenuUp / MenuDown / MenuLeft / MenuRight / MenuSelect - actions that all
	//! already exist - and consumes no new key or pad button of its own.
	protected const string INPUT_CONTEXT_ROW_NAV = "OverthrowMapLayersContext";

	protected SCR_MapToolMenuUI m_ToolMenu;
	protected SCR_MapToolEntry m_ToolMenuEntry;
	protected bool m_bEntryRegistered;

	protected Widget m_wPanel;
	protected Widget m_wOverlayRows;
	protected Widget m_wTypeRows;

	protected SCR_EventHandlerComponent m_FocusProxyHandler;

	//! The rows currently on screen, in the order they were built. Weak refs - each row component is
	//! owned by its own widget, and a strong ref here would outlive the widget it belongs to.
	protected ref array<OVT_MapLayerRowComponent> m_aRows;

	//! Keys already claimed by a row in THIS build, so two config entries cannot quietly share one
	//! row and one preference.
	protected ref set<string> m_aBuiltKeys;

	//! One-shot latches for the three skip filters. The panel rebuilds on every open, so a
	//! per-occurrence warning would be log spam rather than a diagnosis, and none of the three
	//! conditions can change without a config edit.
	protected bool m_bWarnedEmptyLayerId;
	protected bool m_bWarnedEmptyDisplayName;
	protected bool m_bWarnedDuplicateKey;

	//! The per-profile record of which rows this player has hidden. Loaded once, on the first map
	//! open, and cached for the life of this component - the profile cannot change under a running
	//! client, so a reload per map open would re-read an answer that cannot have moved.
	//!
	//! It is ALWAYS allocated, even when nothing could be read from disk. A headless/console app or a
	//! missing settings module leaves an empty store, which reads as "everything visible" - the
	//! pre-feature map - and every toggle still works for the session. Applying a preference to the
	//! map and persisting it are deliberately separate operations for exactly this reason.
	protected ref OVT_MapLayerPrefsStore m_PrefsStore;

	//! Set by a toggle, cleared by a successful flush. It exists so the two flush points cannot spend
	//! the engine's SaveUserSettings throttle window on a session where nothing was toggled - the
	//! disable-map-UI invoker fires ClosePanel whenever any other exclusive tool is opened, panel open
	//! or not.
	protected bool m_bPrefsDirty;

	//! One-shot: armed by OnMapOpen, consumed by the next Update tick. See ApplyPreferences.
	protected bool m_bApplyOnNextTick;

	//! Whether the row-navigation context has already skipped its opening frame.
	//! Self-clearing - see ActivateRowNavigationContext.
	protected bool m_bNavContextArmed;

	//------------------------------------------------------------------------------------------------
	//! Register the tool menu entry.
	//! 🔴 The registration lives HERE and must never move to OnMapOpen.
	//! The two Init()s in play have opposite lifetimes: SCR_MapUIBaseComponent.Init()
	//! runs once per map config load, while OVT_MapLocationType.Init() - the Init()
	//! the rest of this epic deals with - runs on EVERY map open.
	//! SCR_MapToolMenuUI.m_aMenuEntries is only ever inserted into (there is no
	//! unregister API in the class) and OnMapOpen re-runs PopulateToolMenu, so
	//! registering from OnMapOpen would add a duplicate icon on every map open.
	override void Init()
	{
		if (m_bEntryRegistered)
			return;

		if (!m_MapEntity)
		{
			Print("[Overthrow] OVT_MapLayersUI: no map entity, layers tool menu entry not registered", LogLevel.ERROR);
			return;
		}

		m_ToolMenu = SCR_MapToolMenuUI.Cast(m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI));
		if (!m_ToolMenu)
		{
			Print("[Overthrow] OVT_MapLayersUI: SCR_MapToolMenuUI is not present in this map config, no layers entry can be shown", LogLevel.ERROR);
			return;
		}

		m_ToolMenuEntry = m_ToolMenu.RegisterToolMenuEntry(SCR_MapToolMenuUI.s_sToolMenuIcons, m_sToolMenuIcon, m_iSortPriority, m_bIsExclusive);
		if (!m_ToolMenuEntry)
		{
			Print("[Overthrow] OVT_MapLayersUI: RegisterToolMenuEntry returned null, no layers entry can be shown", LogLevel.ERROR);
			return;
		}

		m_bEntryRegistered = true;

		m_ToolMenuEntry.m_OnClick.Insert(TogglePanel);
		m_ToolMenuEntry.GetOnDisableMapUIInvoker().Insert(ClosePanel);

		// Cosmetic only - SCR_MapToolEntry.SetEnabled sets the border colour and nothing
		// consults m_bIsEnabled to block a click. Every vanilla registrant does this.
		m_ToolMenuEntry.SetEnabled(true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		EnsurePrefsLoaded();

		// First of the two applications. This one is what stops a layer the player has hidden from
		// being visible for a frame before it is switched off again.
		ApplyPreferences();

		// Second one, armed here and fired from the next Update tick. See ApplyPreferences for why
		// once is not enough.
		m_bApplyOnNextTick = true;

		BuildPanel();
	}

	//------------------------------------------------------------------------------------------------
	//! Per-frame work for an open map: renew the row-navigation input context for as long as the
	//! panel is open, and fire the armed second application of the stored preferences exactly once
	//! per map open.
	//! \param[in] timeSlice frame time, unused
	override void Update(float timeSlice)
	{
		ActivateRowNavigationContext();

		if (!m_bApplyOnNextTick)
			return;

		m_bApplyOnNextTick = false;

		ApplyPreferences();
	}

	//------------------------------------------------------------------------------------------------
	//! Renew the row-navigation input context, for exactly the frames the panel is open.
	//!
	//! WHY IT IS NEEDED. Vanilla's tool menu gets a pad to this panel for free, but it stops there:
	//! MapContext is the only ActionContext the map menu preset carries and it declares no Menu*
	//! navigation action, so nothing is bound to move focus down a list docked on the map. This is
	//! vanilla's own answer to vanilla's own problem, used twice - SCR_UITaskManagerComponent renews
	//! TaskListMapContext every frame the task list is up on the map, and SCR_MapMarkersUI renews
	//! MapMarkerEditContext from its own SCR_MapUIBaseComponent.Update override, which is the same
	//! call site as this one. OverthrowMapLayersContext is that shape, and its priority and flags are
	//! TaskListMapContext's.
	//!
	//! IT MUST NOT BE LIVE WHILE THE PANEL IS CLOSED. The context outranks MapContext, so for as
	//! long as it is up the D-pad and the left stick belong to the panel rather than to the map's
	//! own tool shortcuts and panning. Activation is a single-frame lease, so NOT calling this is
	//! the entire teardown: a closed panel, a map close and a destroyed component all stop renewing
	//! it within one frame, and none of them needs a special case.
	//!
	//! THE ONE-FRAME DELAY IS DELIBERATE AND IS COPIED FROM SCR_MapMarkersUI, which carries the same
	//! guard with the same reasoning. The panel is opened by a click on the tool menu entry, which
	//! on a pad is the A button, and this context puts MenuSelect on the A button. Going live in the
	//! frame the panel opened would offer the press that opened the panel to the row that has just
	//! taken focus. One frame of grace removes that whole class of double-fire. The flag clears
	//! itself whenever the panel is not visible, so every reopen gets its own grace frame.
	protected void ActivateRowNavigationContext()
	{
		if (!IsPanelVisible())
		{
			m_bNavContextArmed = false;
			return;
		}

		if (!m_bNavContextArmed)
		{
			m_bNavContextArmed = true;
			return;
		}

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.ActivateContext(INPUT_CONTEXT_ROW_NAV);
	}

	//------------------------------------------------------------------------------------------------
	//! Tear down exactly what one map session set up.
	//!
	//! The teardown lives here rather than in HandlerDeattached because this component is never added
	//! as a widget handler (m_bHookToRoot is false), so HandlerDeattached is never called for it -
	//! the same is true of vanilla's SCR_MapJournalUI, whose teardown is equally unreachable.
	//! The tool menu entry's own subscriptions are deliberately NOT removed here: the entry object
	//! survives a map close, OnMapOpen re-runs PopulateToolMenu and re-binds the button to the same
	//! invoker, so dropping them would leave a visible entry that does nothing on the next open.
	override void OnMapClose(MapConfiguration config)
	{
		super.OnMapClose(config);

		// Flush point 2 of 2. The map closing is the last moment this component is certain to hear
		// about, and it is far enough from a panel close in wall-clock terms that the engine's
		// SaveUserSettings throttle cannot swallow it as a duplicate of the first.
		FlushPrefs();

		// A map that closes before its first Update tick must not fire the armed application into the
		// next session, where the layer list has already been rebuilt from scratch.
		m_bApplyOnNextTick = false;

		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(FocusFirstRowDeferred);

		UnwireFocusProxy();

		ReleaseFocusInsidePanel();

		// The panel is recreated on every open, so it must not survive a close or the
		// next open would stack a second one into the same overlay.
		if (m_wPanel)
			m_wPanel.RemoveFromHierarchy();

		m_wPanel = null;
		m_wOverlayRows = null;
		m_wTypeRows = null;

		if (m_aRows)
			m_aRows.Clear();

		if (m_aBuiltKeys)
			m_aBuiltKeys.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Create the panel into the resolved dock parent, hidden.
	//! The vanilla map layout is resolved by name and never overridden - a same-GUID
	//! delta over a vanilla .layout is unproven in this project.
	protected void BuildPanel()
	{
		if (m_wPanel)
			return;

		Widget parent = ResolveDockParent();
		if (!parent)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wPanel = workspace.CreateWidgets(m_PanelLayout, parent);
		if (!m_wPanel)
		{
			Print("[Overthrow] OVT_MapLayersUI: could not create the layers panel layout " + m_PanelLayout, LogLevel.ERROR);
			return;
		}

		m_wPanel.SetVisible(false);

		m_wOverlayRows = FindPanelWidget(WIDGET_OVERLAY_ROWS);
		m_wTypeRows = FindPanelWidget(WIDGET_TYPE_ROWS);

		SetPanelText(WIDGET_PANEL_TITLE, LABEL_PANEL_TITLE);
		SetPanelText(WIDGET_OVERLAYS_HEADER, LABEL_OVERLAYS_HEADER);
		SetPanelText(WIDGET_MARKERS_HEADER, LABEL_MARKERS_HEADER);

		OnPanelBuilt();
	}

	//------------------------------------------------------------------------------------------------
	//! Wire the focus proxy once the panel widgets exist.
	//!
	//! The proxy is a full-stretch, fully transparent button sitting UNDERNEATH the panel content,
	//! copied from vanilla's journal focus button. It exists because engine focus navigation can
	//! legitimately land on the panel's outer widget, and a player focused on a container that does
	//! nothing has no way of telling that from a broken panel. The proxy bounces that focus straight
	//! on to the first row.
	protected void OnPanelBuilt()
	{
		Widget proxy = FindPanelWidget(WIDGET_FOCUS_PROXY);
		if (!proxy)
			return;

		SCR_EventHandlerComponent handler = SCR_EventHandlerComponent.Cast(proxy.FindHandler(SCR_EventHandlerComponent));
		if (!handler)
		{
			Print("[Overthrow] OVT_MapLayersUI: no SCR_EventHandlerComponent on widget '" + WIDGET_FOCUS_PROXY + "', focus landing on the panel body will not reach a row", LogLevel.ERROR);
			return;
		}

		m_FocusProxyHandler = handler;
		m_FocusProxyHandler.GetOnFocus().Insert(OnFocusProxyFocused);
	}

	//------------------------------------------------------------------------------------------------
	//! Remove exactly what OnPanelBuilt inserted.
	protected void UnwireFocusProxy()
	{
		if (!m_FocusProxyHandler)
			return;

		m_FocusProxyHandler.GetOnFocus().Remove(OnFocusProxyFocused);
		m_FocusProxyHandler = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] w The proxy widget that gained focus
	protected void OnFocusProxyFocused(Widget w)
	{
		FocusFirstRow();
	}

	//------------------------------------------------------------------------------------------------
	//! Named fallback chain for the widget the panel is parented onto.
	//! \return the dock parent, or null when even the root widget is unavailable
	protected Widget ResolveDockParent()
	{
		if (!m_RootWidget)
		{
			Print("[Overthrow] OVT_MapLayersUI: no map root widget, the layers panel cannot be docked", LogLevel.ERROR);
			return null;
		}

		Widget overlay = m_RootWidget.FindAnyWidget(WIDGET_DOCK_OVERLAY);
		if (overlay)
			return overlay;

		Print("[Overthrow] OVT_MapLayersUI: widget '" + WIDGET_DOCK_OVERLAY + "' not found, falling back to '" + WIDGET_DOCK_CONTAINER + "'", LogLevel.ERROR);

		Widget container = m_RootWidget.FindAnyWidget(WIDGET_DOCK_CONTAINER);
		if (container)
			return container;

		Print("[Overthrow] OVT_MapLayersUI: widget '" + WIDGET_DOCK_CONTAINER + "' not found either, docking the layers panel onto the map root widget", LogLevel.ERROR);

		return m_RootWidget;
	}

	//------------------------------------------------------------------------------------------------
	//! Null-guarded lookup inside the panel. A missing name is a silent no-op otherwise,
	//! which is the failure class that cost this epic two configured features.
	//! \param[in] name widget name authored in OVT_MapLayersPanel.layout
	//! \return the widget, or null
	protected Widget FindPanelWidget(string name)
	{
		if (!m_wPanel)
			return null;

		Widget w = m_wPanel.FindAnyWidget(name);
		if (!w)
			Print("[Overthrow] OVT_MapLayersUI: widget '" + name + "' not found in the layers panel layout", LogLevel.ERROR);

		return w;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets one of the panel's static labels, ERROR-logging the name when it cannot be resolved.
	//! The keys are also authored into the layout, so a script that never runs still shows a title
	//! rather than an empty box.
	//! \param[in] name widget name authored in OVT_MapLayersPanel.layout
	//! \param[in] text localization key to display
	protected void SetPanelText(string name, string text)
	{
		Widget w = FindPanelWidget(name);
		if (!w)
			return;

		TextWidget label = TextWidget.Cast(w);
		if (!label)
		{
			Print("[Overthrow] OVT_MapLayersUI: widget '" + name + "' in the layers panel layout is not a TextWidget, its label cannot be set", LogLevel.ERROR);
			return;
		}

		label.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Tool menu entry click handler
	protected void TogglePanel()
	{
		if (!m_wPanel)
		{
			BuildPanel();

			if (!m_wPanel)
				return;
		}

		if (m_wPanel.IsVisible())
			SetPanelVisible(false);
		else
			SetPanelVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Fired by other exclusive tool menu entries through GetOnDisableMapUIInvoker
	protected void ClosePanel()
	{
		SetPanelVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide the panel, copying SCR_MapJournalUI.ToggleVisible.
	//!
	//! FOCUS IS RE-ESTABLISHED HERE AND THE ORDER IS LOAD-BEARING. SCR_MapToolEntry's constructor
	//! inserts its own OnClick into m_OnClick before ours, and on a gamepad that handler clears the
	//! focused widget outright (SetToolMenuFocused(false) calls SetFocusedWidget(null)). Ours
	//! therefore runs after focus has been nulled and has to put it back - without this the panel
	//! opens focused on nothing, which looks perfect on a mouse and is completely dead on a pad.
	//! \param[in] visible target visibility
	protected void SetPanelVisible(bool visible)
	{
		if (!m_wPanel)
			return;

		m_wPanel.SetVisible(visible);

		if (m_ToolMenuEntry)
			m_ToolMenuEntry.SetActive(visible);

		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(FocusFirstRowDeferred);

		if (!visible)
		{
			ReleaseFocusInsidePanel();

			// Flush point 1 of 2, and the only one a player will normally reach. It is a no-op unless
			// a toggle actually happened, so the invoker-driven close that fires whenever another
			// exclusive tool opens costs nothing.
			FlushPrefs();

			return;
		}

		// Rebuilt on every open rather than cached. Seventeen rows is nothing, and it is the only
		// construction that is always correct: canvas layers register with the compositor per map
		// open, so a cached row set can be one map session out of date.
		BuildRows();

		FocusFirstRow();

		// Second attempt on the next frame, because the rows were created microseconds ago and
		// anything that clears focus during this frame would otherwise win. It refuses to act when
		// something already holds focus, so it can only ever fill a gap, never steal.
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().Call(FocusFirstRowDeferred);
	}

	//------------------------------------------------------------------------------------------------
	//! Put the gamepad on the first row.
	protected void FocusFirstRow()
	{
		if (!m_wPanel || !m_wPanel.IsVisible())
			return;

		if (!m_aRows || m_aRows.IsEmpty())
			return;

		OVT_MapLayerRowComponent first = m_aRows[0];
		if (!first)
			return;

		Widget target = first.GetFocusWidget();
		if (!target)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		workspace.SetFocusedWidget(target);
	}

	//------------------------------------------------------------------------------------------------
	//! Hand focus back to the map when it is sitting on something inside the panel.
	//!
	//! Called before every teardown of the rows. A focused widget that is then destroyed strands the
	//! gamepad on nothing, and it also robs SCR_ToolboxComponent of the OnFocusLost it uses to
	//! remove the menu-left/menu-right action listeners it registers on focus - the destroyed
	//! checkbox would keep them. Nulling the focus is exactly what vanilla's tool menu does when it
	//! releases the strip, so the map behaves the same way afterwards either way.
	protected void ReleaseFocusInsidePanel()
	{
		if (!m_wPanel)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget focused = workspace.GetFocusedWidget();
		if (!focused)
			return;

		Widget current = focused;
		while (current)
		{
			if (current == m_wPanel)
			{
				workspace.SetFocusedWidget(null);
				return;
			}

			current = current.GetParent();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Next-frame retry for FocusFirstRow, which only acts when nothing else owns focus.
	protected void FocusFirstRowDeferred()
	{
		if (!m_wPanel || !m_wPanel.IsVisible())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (workspace.GetFocusedWidget())
			return;

		FocusFirstRow();
	}

	//------------------------------------------------------------------------------------------------
	//! \return the panel widget while it is open, null otherwise
	Widget GetPanelWidget()
	{
		if (!m_wPanel)
			return null;

		if (!m_wPanel.IsVisible())
			return null;

		return m_wPanel;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true while the panel is open
	bool IsPanelVisible()
	{
		return m_wPanel && m_wPanel.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! Build one row per thing that draws, from the three sources.
	//!
	//! EVERY ROW'S STATE IS READ LIVE FROM THE OBJECT IT CONTROLS - IsLayerVisible,
	//! AreMarkersVisible, IsPlayerVisible - and never from a stored preference. A panel that read a
	//! store could disagree with the map it is describing, and the player would have no way to tell
	//! which of the two was lying.
	protected void BuildRows()
	{
		if (!m_wPanel)
			return;

		if (!m_wOverlayRows)
			m_wOverlayRows = FindPanelWidget(WIDGET_OVERLAY_ROWS);

		if (!m_wTypeRows)
			m_wTypeRows = FindPanelWidget(WIDGET_TYPE_ROWS);

		// The rows about to be destroyed may be holding focus.
		ReleaseFocusInsidePanel();

		ClearRows(m_wOverlayRows);
		ClearRows(m_wTypeRows);

		if (!m_aRows)
			m_aRows = new array<OVT_MapLayerRowComponent>();

		m_aRows.Clear();

		if (!m_aBuiltKeys)
			m_aBuiltKeys = new set<string>();

		m_aBuiltKeys.Clear();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (m_RowLayout.IsEmpty())
		{
			Print("[Overthrow] OVT_MapLayersUI: no row layout configured, the layers panel will be empty", LogLevel.ERROR);
			return;
		}

		BuildCanvasLayerRows(workspace);
		BuildPlayerRow(workspace);
		BuildLocationTypeRows(workspace);
	}

	//------------------------------------------------------------------------------------------------
	//! Remove every child of a row container, so a rebuild cannot stack duplicates.
	//! \param[in] container the OverlayRows or TypeRows widget
	protected void ClearRows(Widget container)
	{
		if (!container)
			return;

		while (container.GetChildren())
		{
			container.RemoveChild(container.GetChildren());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One row per registered canvas overlay.
	//!
	//! THE COMPOSITOR'S LIST IS READ, NEVER MUTATED - it owns the draw order, and the panel is one
	//! of several readers.
	//!
	//! A config-disabled module needs no special case here, and this is worth knowing rather than
	//! rediscovering: SCR_MapEntity.ActivateModules skips a module whose IsConfigDisabled is true
	//! BEFORE calling SetActive(true), and SetActive(true) is what subscribes OnMapOpen, which is
	//! what registers a layer with the compositor. A disabled module therefore never reaches
	//! GetLayers() at all, which is why the disabled threat grid can never produce a row.
	//! \param[in] workspace the workspace rows are created through
	protected void BuildCanvasLayerRows(notnull WorkspaceWidget workspace)
	{
		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (!compositor)
			return;

		array<OVT_MapCanvasLayer> layers = compositor.GetLayers();
		if (!layers)
			return;

		foreach (OVT_MapCanvasLayer layer : layers)
		{
			if (!layer)
				continue;

			string layerId = layer.GetLayerId();
			if (layerId.IsEmpty())
			{
				WarnEmptyLayerIdOnce(layer.ClassName());
				continue;
			}

			string displayName = layer.GetDisplayName();
			if (displayName.IsEmpty())
			{
				WarnEmptyDisplayNameOnce(layerId);
				continue;
			}

			string key = OVT_MapLayerPrefsStore.LayerKey(layerId);
			if (!ClaimKey(key, displayName))
				continue;

			// No icon source: an overlay is a named wash over the whole map rather than a category
			// of glyph, so the row shows a label and nothing else.
			CreateRow(workspace, m_wOverlayRows, key, displayName, string.Empty, string.Empty, layer.IsLayerVisible());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The one hand-built row.
	//!
	//! OVT_MapPlayerLocation is a SCR_MapUIBaseComponent, not an OVT_MapCanvasLayer, so it will
	//! never appear in the compositor's list. Reparenting a working, retained component onto the
	//! canvas-layer base purely to satisfy this list builder would risk a live feature to save one
	//! special case, so the special case stays and this comment is its documentation.
	//!
	//! A session that never created markers - difficulty preset with showPlayerOnMap off, or no
	//! controlled entity - gets NO ROW AT ALL rather than a toggle over nothing, because a dead
	//! control reads as a broken panel while an absent one reads as an unavailable feature.
	//! \param[in] workspace the workspace rows are created through
	protected void BuildPlayerRow(notnull WorkspaceWidget workspace)
	{
		OVT_MapPlayerLocation playerLocation = GetPlayerLocation();
		if (!playerLocation)
			return;

		if (!playerLocation.IsAvailableThisSession())
			return;

		string key = OVT_MapLayerPrefsStore.LayerKey(KEY_PLAYERS);
		if (!ClaimKey(key, LABEL_PLAYERS))
			return;

		CreateRow(workspace, m_wOverlayRows, key, LABEL_PLAYERS, string.Empty, string.Empty, playerLocation.AreMarkersVisible());
	}

	//------------------------------------------------------------------------------------------------
	//! One row per configured location type.
	//!
	//! The label comes from GetCategoryName, which already falls back to the display name and then
	//! to the class name with a warn-once of its own. THE ROW ALWAYS APPEARS: adding a type to
	//! OverthrowMap.conf must add a toggle with no code change, so a missing category name is a
	//! content gap rather than a structural one.
	//! \param[in] workspace the workspace rows are created through
	protected void BuildLocationTypeRows(notnull WorkspaceWidget workspace)
	{
		OVT_OverthrowMapUI mapUI = GetMapUI();
		if (!mapUI)
			return;

		array<ref OVT_MapLocationType> locationTypes = mapUI.GetLocationTypes();
		if (!locationTypes)
			return;

		foreach (OVT_MapLocationType locationType : locationTypes)
		{
			if (!locationType)
				continue;

			string key = OVT_MapLayerPrefsStore.TypeKey(locationType.ClassName());
			if (key.IsEmpty())
				continue;

			string label = locationType.GetCategoryName();

			if (!ClaimKey(key, label))
				continue;

			CreateRow(workspace, m_wTypeRows, key, label, locationType.GetIconImageset(), locationType.GetIconName(), locationType.IsPlayerVisible());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Instantiate one row and hand it its data.
	//! \param[in] workspace the workspace rows are created through
	//! \param[in] container OverlayRows or TypeRows
	//! \param[in] key namespaced preference key
	//! \param[in] label localization key for the row's name
	//! \param[in] imageset imageset for the leading glyph, or "" for a row with no icon source
	//! \param[in] icon quad name within imageset, or "" for a row with no icon source
	//! \param[in] visible the LIVE state read from the object this row controls
	protected void CreateRow(notnull WorkspaceWidget workspace, Widget container, string key, string label, ResourceName imageset, string icon, bool visible)
	{
		if (!container)
			return;

		Widget w = workspace.CreateWidgets(m_RowLayout, container);
		if (!w)
		{
			Print("[Overthrow] OVT_MapLayersUI: could not create a layer row from " + m_RowLayout + " for key '" + key + "'", LogLevel.ERROR);
			return;
		}

		OVT_MapLayerRowComponent row = OVT_MapLayerRowComponent.Cast(w.FindHandler(OVT_MapLayerRowComponent));
		if (!row)
		{
			Print("[Overthrow] OVT_MapLayersUI: no OVT_MapLayerRowComponent on the layer row layout, row '" + key + "' would be inert and was dropped", LogLevel.ERROR);
			w.RemoveFromHierarchy();
			return;
		}

		row.Init(key, label, imageset, icon, visible, this);

		m_aRows.Insert(row);
	}

	//------------------------------------------------------------------------------------------------
	//! Claim a key for this build, refusing a duplicate.
	//! Two config entries of the same location-type class, or a canvas layer whose id collides with
	//! the hand-built player row, would otherwise share one row and one preference. There are none
	//! today; the guard exists so that stops being true loudly rather than silently.
	//! \param[in] key namespaced preference key
	//! \param[in] label the label that would have been shown, for the warning
	//! \return true when the key is this build's first use of it
	protected bool ClaimKey(string key, string label)
	{
		if (key.IsEmpty())
			return false;

		if (m_aBuiltKeys.Contains(key))
		{
			WarnDuplicateKeyOnce(key, label);
			return false;
		}

		m_aBuiltKeys.Insert(key);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply a row's new state to the object it controls.
	//! Called by OVT_MapLayerRowComponent - rows never mutate anything themselves.
	//! \param[in] key namespaced preference key
	//! \param[in] visible the new state
	void OnRowToggled(string key, bool visible)
	{
		if (key.IsEmpty())
			return;

		// The map is updated FIRST and unconditionally. Applying a preference and persisting it are
		// separate operations on purpose: a toggle must take visible effect whether or not anything
		// ever reaches disk, so no failure below this line can cost the player their toggle.
		ApplyOne(key, visible);

		EnsurePrefsLoaded();

		// MEMORY ONLY - THERE IS NO FLUSH ON THIS PATH, AND ADDING ONE WOULD LOSE WRITES.
		// The engine throttles SaveUserSettings: two calls microseconds apart leave only the FIRST on
		// disk, and the second is dropped rather than deferred. A filter panel is a burst surface - a
		// player flipping five rows in three seconds would generate five flushes and the engine would
		// discard most of them. So the record is mutated here and written whole at the two
		// user-separated flush points, panel close and map close, where a dropped intermediate write
		// costs nothing because the next one carries every key again.
		m_PrefsStore.SetHidden(key, !visible);
		m_bPrefsDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Allocate the store and read the profile, once.
	//!
	//! The store is allocated whether or not the read succeeds, so every later caller can rely on it
	//! being there. Load answers false on a headless/console app and when the engine has no such
	//! settings module registered; neither is worth a log line, and both leave an empty store, which
	//! reads as "everything visible" and lets the session's toggles work in memory regardless.
	protected void EnsurePrefsLoaded()
	{
		if (m_PrefsStore)
			return;

		m_PrefsStore = new OVT_MapLayerPrefsStore();

		OVT_MapLayerSettingsAccessor.Load(m_PrefsStore);
	}

	//------------------------------------------------------------------------------------------------
	//! Write the whole record to the profile, at a flush point.
	//!
	//! Refuses when nothing has been toggled, so the two flush points cannot spend the engine's
	//! throttle window on a no-change close. The dirty flag is cleared only on a successful write:
	//! when there is no profile to write to, the state stays pending and is simply offered again at
	//! the next flush point rather than being quietly forgotten.
	protected void FlushPrefs()
	{
		if (!m_PrefsStore)
			return;

		if (!m_bPrefsDirty)
			return;

		if (OVT_MapLayerSettingsAccessor.Save(m_PrefsStore))
			m_bPrefsDirty = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Push the stored preference for every row source onto the object it names.
	//!
	//! IDEMPOTENT BY CONSTRUCTION - it writes the same values from the same store every time, so it
	//! can be called as often as it likes and two calls can never fight. That property is what makes
	//! the following safe:
	//!
	//! IT IS CALLED TWICE PER MAP OPEN, from OnMapOpen and again from the first Update tick after it.
	//! The reason is that OVT_MapCanvasCompositor's layer list is only populated while a map is open -
	//! layers register from their own OnMapOpen and unregister on close - so a stored "territory
	//! hidden" can only be applied to a layer that has already registered. Reading SCR_MapEntity's map
	//! open shows ActivateModules running before ActivateComponents, with the invoke after both, and
	//! since a ScriptInvoker fires in insertion order every module's OnMapOpen therefore runs before
	//! every component's - which would make the first call sufficient. THAT IS TRUE AND IT IS STILL AN
	//! ASSUMPTION: it rests on subscription order, and the reactivation path re-subscribes in a
	//! different loop. The first call is what avoids a visible flash of a layer the player has hidden;
	//! the second is provably after every module and component has run, whatever the order. The
	//! ongoing cost of the safety net is one boolean compare per frame, against "my saved toggles
	//! didn't stick" being the single most likely bug in this feature.
	protected void ApplyPreferences()
	{
		if (!m_PrefsStore)
			return;

		ApplyPlayerMarkerPreference();
		ApplyCanvasLayerPreferences();
		ApplyLocationTypePreferences();
	}

	//------------------------------------------------------------------------------------------------
	//! The hand-built player-marker row's preference.
	//!
	//! A session that offers no row must take no write either. BuildPlayerRow skips the row when the
	//! markers were never created - no controlled entity, or a difficulty preset with the player
	//! marker off - and this check is the same one for the same reason: writing the preference then
	//! would set a flag the player has no control over, and the value would survive into a later
	//! session that DOES draw markers.
	protected void ApplyPlayerMarkerPreference()
	{
		OVT_MapPlayerLocation playerLocation = GetPlayerLocation();
		if (!playerLocation)
			return;

		if (!playerLocation.IsAvailableThisSession())
			return;

		playerLocation.SetMarkersVisible(m_PrefsStore.IsVisible(OVT_MapLayerPrefsStore.LayerKey(KEY_PLAYERS)));
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered canvas overlay's preference. The compositor's list is read, never mutated.
	//! A layer with no id is skipped in silence here - it has no key to look up, and BuildRows already
	//! warns about it once when it refuses to give it a row.
	protected void ApplyCanvasLayerPreferences()
	{
		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (!compositor)
			return;

		array<OVT_MapCanvasLayer> layers = compositor.GetLayers();
		if (!layers)
			return;

		foreach (OVT_MapCanvasLayer layer : layers)
		{
			if (!layer)
				continue;

			string key = OVT_MapLayerPrefsStore.LayerKey(layer.GetLayerId());
			if (key.IsEmpty())
				continue;

			layer.SetLayerVisible(m_PrefsStore.IsVisible(key));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every configured location type's preference, followed by ONE visibility sweep for the whole
	//! batch. The sweep is the expensive half - it walks every icon on the map - and running it per
	//! type would repeat that work fourteen times over a run of half-applied intermediate states.
	protected void ApplyLocationTypePreferences()
	{
		OVT_OverthrowMapUI mapUI = GetMapUI();
		if (!mapUI)
			return;

		array<ref OVT_MapLocationType> locationTypes = mapUI.GetLocationTypes();
		if (!locationTypes)
			return;

		foreach (OVT_MapLocationType locationType : locationTypes)
		{
			if (!locationType)
				continue;

			string key = OVT_MapLayerPrefsStore.TypeKey(locationType.ClassName());
			if (key.IsEmpty())
				continue;

			locationType.SetPlayerVisible(m_PrefsStore.IsVisible(key));
		}

		mapUI.RefreshAllVisibility();
	}

	//------------------------------------------------------------------------------------------------
	//! Route one key to the object it names.
	//! The player row is tested first because its key lives in the layer namespace and would
	//! otherwise be shadowed by a canvas layer sweep that will never match it.
	//! \param[in] key namespaced preference key
	//! \param[in] visible the new state
	protected void ApplyOne(string key, bool visible)
	{
		if (ApplyPlayerMarkers(key, visible))
			return;

		if (ApplyCanvasLayer(key, visible))
			return;

		if (ApplyLocationType(key, visible))
			return;

		Print("[Overthrow] OVT_MapLayersUI: preference key '" + key + "' matched no canvas layer, player-marker row or location type; the toggle had no effect", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] key namespaced preference key
	//! \param[in] visible the new state
	//! \return true when the key named the player markers
	protected bool ApplyPlayerMarkers(string key, bool visible)
	{
		if (key != OVT_MapLayerPrefsStore.LayerKey(KEY_PLAYERS))
			return false;

		OVT_MapPlayerLocation playerLocation = GetPlayerLocation();
		if (!playerLocation)
			return false;

		playerLocation.SetMarkersVisible(visible);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SetLayerVisible, and never SetActive: SCR_MapModuleBase.SetActive(false) removes the module
	//! from the map entity's active list, and there is no script-reachable way back within a
	//! session, so a toggle built on it would switch a layer off permanently.
	//! \param[in] key namespaced preference key
	//! \param[in] visible the new state
	//! \return true when the key named a registered canvas layer
	protected bool ApplyCanvasLayer(string key, bool visible)
	{
		OVT_MapCanvasCompositor compositor = OVT_MapCanvasCompositor.GetInstance();
		if (!compositor)
			return false;

		array<OVT_MapCanvasLayer> layers = compositor.GetLayers();
		if (!layers)
			return false;

		foreach (OVT_MapCanvasLayer layer : layers)
		{
			if (!layer)
				continue;

			if (OVT_MapLayerPrefsStore.LayerKey(layer.GetLayerId()) != key)
				continue;

			layer.SetLayerVisible(visible);

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Write the type's presentation preference and then ask the map for one visibility sweep.
	//! The sweep is the caller's job by design - OVT_MapLocationType.SetPlayerVisible deliberately
	//! does not reach back into the UI, so a batch of types can be applied with one sweep after it
	//! rather than one sweep per type and a run of half-applied intermediate states.
	//! \param[in] key namespaced preference key
	//! \param[in] visible the new state
	//! \return true when the key named a configured location type
	protected bool ApplyLocationType(string key, bool visible)
	{
		OVT_OverthrowMapUI mapUI = GetMapUI();
		if (!mapUI)
			return false;

		array<ref OVT_MapLocationType> locationTypes = mapUI.GetLocationTypes();
		if (!locationTypes)
			return false;

		foreach (OVT_MapLocationType locationType : locationTypes)
		{
			if (!locationType)
				continue;

			if (OVT_MapLayerPrefsStore.TypeKey(locationType.ClassName()) != key)
				continue;

			locationType.SetPlayerVisible(visible);
			mapUI.RefreshAllVisibility();

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the Overthrow map UI component, or null
	protected OVT_OverthrowMapUI GetMapUI()
	{
		if (!m_MapEntity)
			return null;

		return OVT_OverthrowMapUI.Cast(m_MapEntity.GetMapUIComponent(OVT_OverthrowMapUI));
	}

	//------------------------------------------------------------------------------------------------
	//! \return the player-marker component, or null
	protected OVT_MapPlayerLocation GetPlayerLocation()
	{
		if (!m_MapEntity)
			return null;

		return OVT_MapPlayerLocation.Cast(m_MapEntity.GetMapUIComponent(OVT_MapPlayerLocation));
	}

	//------------------------------------------------------------------------------------------------
	//! Skip filter 1: a layer with no id cannot be addressed, so it cannot be persisted.
	//! \param[in] className the offending layer's class, for the log
	protected void WarnEmptyLayerIdOnce(string className)
	{
		if (m_bWarnedEmptyLayerId)
			return;

		m_bWarnedEmptyLayerId = true;

		Print("[Overthrow] OVT_MapLayersUI: canvas layer " + className + " has an empty m_sLayerId and was skipped - it has no addressable key, so it can be neither toggled nor remembered. Set m_sLayerId in Configs/Map/MapFullscreen.conf. Further occurrences are suppressed.", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Skip filter 2: a layer with no display name cannot be labelled.
	//! \param[in] layerId the offending layer's id, for the log
	protected void WarnEmptyDisplayNameOnce(string layerId)
	{
		if (m_bWarnedEmptyDisplayName)
			return;

		m_bWarnedEmptyDisplayName = true;

		Print("[Overthrow] OVT_MapLayersUI: canvas layer '" + layerId + "' has an empty m_sDisplayName and was skipped - a blank row is worse than no row. Set m_sDisplayName in Configs/Map/MapFullscreen.conf. Further occurrences are suppressed.", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Skip filter 3: two sources claiming one key would share one row and one preference.
	//! \param[in] key the contested key
	//! \param[in] label the label that would have been shown
	protected void WarnDuplicateKeyOnce(string key, string label)
	{
		if (m_bWarnedDuplicateKey)
			return;

		m_bWarnedDuplicateKey = true;

		Print("[Overthrow] OVT_MapLayersUI: preference key '" + key + "' is claimed twice; the later row ('" + label + "') was skipped so the two cannot share one toggle. Further occurrences are suppressed.", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Unreachable in practice - this component is never added as a widget handler, so the engine
	//! never detaches it. Kept because vanilla's map UI components all carry the same shape, and
	//! because it is free insurance if that ever changes. The real teardown is in OnMapClose.
	//! \param[in] w the widget being detached from
	override void HandlerDeattached(Widget w)
	{
		if (m_ToolMenuEntry)
		{
			m_ToolMenuEntry.m_OnClick.Remove(TogglePanel);
			m_ToolMenuEntry.GetOnDisableMapUIInvoker().Remove(ClosePanel);
		}

		UnwireFocusProxy();
	}
}
