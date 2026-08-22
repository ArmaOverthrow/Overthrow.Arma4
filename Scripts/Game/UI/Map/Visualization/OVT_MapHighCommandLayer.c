//------------------------------------------------------------------------------------------------
//! Map markers, selection and orders for every High Command group. The OVT_MapRecruitLocation shape.
//!
//! RECORDS ONLY - a High Command group is usually nowhere near the local player and its entities are
//! not streamed here. GetGroupEntity / GetVehicleEntity are server-only by construction.
//!
//! Three channels, one writer each, so nothing contends for a widget property:
//!   visibility  SetMarkersVisible only  - the layer-filter row
//!   opacity     Update() only           - own groups bright, other players' dimmed
//!   ID_D quad   Update()/selection only - normal / under the cursor / selected
//!
//! LAYOUT <-> CODE NAME CONTRACT
//!   MapHighCommandLocation.layout  "Image" "TypeIcon" "TagImage"
//!   MapHighCommandPanel.layout     "GroupName" "OwnerLine" "StanceLine" "DestinationLine"
//!                                  "DistanceLine" "StatusLine" "ReasonText"
//!                                  "OrderButton" "StanceButton" "CloseButton"
//------------------------------------------------------------------------------------------------
class OVT_MapHighCommandLayer : SCR_MapUIBaseComponent
{
	//! Opacity of a marker belonging to somebody else. Own groups draw at 1.0 (F7).
	protected const float OTHER_OWNER_OPACITY = 0.45;

	//! Affiliation quads, all three from the same shipped vanilla imageset.
	protected const ResourceName SYMBOL_IMAGESET = "{8479B3B5347DF5CF}UI/Imagesets/MilitarySymbol/ID_D.imageset";
	//! The marker base, set once at creation and NEVER swapped - see UpdateSymbolQuad.
	protected const string QUAD_NORMAL = "Friend_Land_Bcg";
	protected const string QUAD_FOCUS = "Friend_Focus_Land";
	protected const string QUAD_SELECT = "Friend_Select_Land";

	//! The unit-type glyph atlas the authored entry's m_sMapIcon names.
	protected const ResourceName TYPE_IMAGESET = "{27F2439D610D02B3}UI/Imagesets/MilitarySymbol/ICO_Land.imageset";
	protected const string TYPE_ICON_FALLBACK = "Infantry_Friend";

	//! Overthrow's own atlas, for the ammo badge.
	protected const ResourceName MARKER_IMAGESET = "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset";

	//! No badge art exists for OVT_HighCommandStatus's two tags, so its logical names are resolved onto
	//! quads that already ship. Replace these mappings, not the Data-tier constants, when art arrives.
	protected const string BADGE_CONTACT_QUAD = "Hostile_Land_Bcg";
	protected const string BADGE_NO_AMMO_QUAD = "recruit_ammo_empty";

	protected const string WIDGET_IMAGE = "Image";
	protected const string WIDGET_TYPE_ICON = "TypeIcon";
	protected const string WIDGET_SELECT_RING = "SelectRing";
	protected const string WIDGET_TAG_IMAGE = "TagImage";

	protected const string WIDGET_GROUP_NAME = "GroupName";
	protected const string WIDGET_OWNER_LINE = "OwnerLine";
	protected const string WIDGET_STANCE_LINE = "StanceLine";
	protected const string WIDGET_DESTINATION_LINE = "DestinationLine";
	protected const string WIDGET_DISTANCE_LINE = "DistanceLine";
	protected const string WIDGET_STATUS_LINE = "StatusLine";
	protected const string WIDGET_REASON_TEXT = "ReasonText";
	protected const string WIDGET_ORDER_BUTTON = "OrderButton";
	protected const string WIDGET_STANCE_BUTTON = "StanceButton";

	//! Vanilla's own map click, reused rather than bound again - already mouse:button0 and gamepad0:a
	//! in MapContext, so selecting a group costs no new input.
	protected const string ACTION_MAP_SELECT = "MapSelect";

	protected const string WIDGET_CLOSE_BUTTON = "CloseButton";

	//! Declared in Configs/System/chimeraInputCommon.conf. EXCLUSIVE (Flags bit 0x8) and leased one
	//! frame at a time, only while a group is selected - see TickCommandContext.
	protected const string INPUT_CONTEXT_COMMAND = "OverthrowMapCommandContext";

	[Attribute()]
	protected ResourceName m_Layout;

	[Attribute()]
	protected ResourceName m_PanelLayout;

	[Attribute(defvalue: "36", uiwidget: UIWidgets.EditBox, desc: "Cursor magnet radius in reference pixels. The widget tree cannot be used to enlarge a marker's hover target, so selection is done in script against the cursor's world position - which the gamepad's virtual cursor updates too")]
	protected float m_fSelectRadiusPx;

	[Attribute(defvalue: "", uiwidget: UIWidgets.Object, desc: "Optional faction colours for the High Command markers. Leave UNSET to use the shared default - High Command belongs to the resistance, so green.")]
	protected ref OVT_MapFactionPalette m_FactionColors;

	//! One widget per group record, keyed by group id. Rebuilt on map open and on table churn.
	protected ref map<string, ref Widget> m_Widgets;

	//! Last quad name pushed into each marker's TagImage, keyed by group id. A change filter only:
	//! LoadImageFromSet is a resource lookup and Update() runs every frame.
	protected ref map<string, string> m_mTagQuads;

	//! Last affiliation quad pushed into each marker's Image, keyed by group id. Same reason.
	protected ref map<string, string> m_mSymbolQuads;

	protected string m_sLocalPersistentId;

	//! Client-side presentation preference, pushed in by the layer-filter panel. Default true.
	protected bool m_bMarkersVisible = true;

	//! False until OnMapOpen clears every early return INCLUDING "there are no groups at all", so a
	//! campaign with no High Command gets no filter row rather than a toggle over nothing.
	protected bool m_bAvailableThisSession;

	//! Whether this layer holds subscriptions on the manager's invokers. The manager OUTLIVES the map,
	//! so the subscription is strictly paired with the removal on map close.
	protected bool m_bSubscribed;

	//! Whether the map-level action listeners are installed. Same pairing rule, different owner.
	protected bool m_bInputsRegistered;

	//! The selected group, or "" for none. Owns the info panel's existence and the input lease.
	protected string m_sSelectedGroupId;

	//! The group the cursor magnet currently claims, or "" for none.
	protected string m_sMagnetGroupId;

	protected Widget m_wPanel;

	//! Whether the command context has already skipped its opening frame - see TickCommandContext.
	protected bool m_bNavContextArmed;

	//------------------------------------------------------------------------------------------------
	//! This layer's faction palette: its own if the conf configured one, otherwise the shared default.
	//! Never null. Duplicated for the same reason OVT_MapRecruitLocation duplicates it - this is a
	//! SCR_MapUIBaseComponent and shares no base class with the canvas layers.
	//! \return The configured palette, or the shared default.
	OVT_MapFactionPalette GetFactionPalette()
	{
		if (m_FactionColors)
			return m_FactionColors;

		return OVT_MapFactionPalette.GetDefault();
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		// Reset FIRST, before any early return, so a session that bails out below leaves this false and
		// the layer-filter panel omits the High Command row entirely.
		m_bAvailableThisSession = false;

		m_sLocalPersistentId = string.Empty;
		m_sSelectedGroupId = string.Empty;
		m_sMagnetGroupId = string.Empty;
		m_bNavContextArmed = false;

		if (!m_Widgets)
			m_Widgets = new map<string, ref Widget>;

		if (!m_mTagQuads)
			m_mTagQuads = new map<string, string>;

		if (!m_mSymbolQuads)
			m_mSymbolQuads = new map<string, string>;

		ClearMarkers();

		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId <= 0)
			return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return;

		// An empty persistent id is not fatal here: every group still draws, none of them read as
		// "mine", and the order buttons refuse with a reason. That is the correct degradation.
		m_sLocalPersistentId = players.GetPersistentIDFromPlayerID(localPlayerId);

		// Subscribed even when no group exists yet: buying the first one with the map open must produce
		// a marker, and the rebuild is what re-runs the availability check.
		SubscribeToTable();
		RegisterInputs();

		BuildMarkers();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		UnregisterInputs();
		UnsubscribeFromTable();

		ClearSelection();
		ClearMarkers();

		m_bAvailableThisSession = false;
		m_bNavContextArmed = false;

		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	//! Create one marker widget per group record and re-assert the visibility preference on them.
	//! The ONLY place m_bAvailableThisSession becomes true, so no groups means no filter row.
	protected void BuildMarkers()
	{
		ClearMarkers();

		if (!m_RootWidget)
			return;

		if (m_Layout.IsEmpty())
		{
			Print("[Overthrow] OVT_MapHighCommandLayer: no m_Layout configured, no High Command markers will be drawn. Set it in Configs/Map/MapOverthrow.conf", LogLevel.ERROR);
			return;
		}

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand || !highCommand.m_mGroups)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		// High Command is the resistance by definition, so the role is asked for directly rather than
		// round-tripped through a faction index. Green, the same green as the territory and the rings.
		Color markerColour = GetFactionPalette().GetColorForRole(OVT_FactionType.RESISTANCE_FACTION);

		for (int i = 0; i < highCommand.m_mGroups.Count(); i++)
		{
			OVT_HighCommandRecord record = highCommand.m_mGroups.GetElement(i);
			if (!record)
				continue;

			if (record.m_sGroupId.IsEmpty())
				continue;

			Widget widget = workspace.CreateWidgets(m_Layout, m_RootWidget);
			if (!widget)
				continue;

			ImageWidget img = ImageWidget.Cast(widget.FindAnyWidget(WIDGET_IMAGE));
			if (img)
				img.SetColor(markerColour);

			// The glyph keeps its authored dark colour: it is the NATO symbol drawn ON the coloured
			// field, and tinting it green too would leave green on green.
			ImageWidget typeIcon = ImageWidget.Cast(widget.FindAnyWidget(WIDGET_TYPE_ICON));
			if (typeIcon)
				typeIcon.LoadImageFromSet(0, TYPE_IMAGESET, ResolveTypeIcon(record));

			// The badge starts hidden and is driven entirely by Update(): the status flags are volatile
			// and are read per frame, never decided here.
			ImageWidget tag = ImageWidget.Cast(widget.FindAnyWidget(WIDGET_TAG_IMAGE));
			if (tag)
				tag.SetVisible(false);

			m_Widgets[record.m_sGroupId] = widget;
			m_mTagQuads[record.m_sGroupId] = string.Empty;
			m_mSymbolQuads[record.m_sGroupId] = string.Empty;
		}

		if (m_Widgets.IsEmpty())
			return;

		m_bAvailableThisSession = true;

		// Re-assert the current preference onto the freshly created widget set. The widgets are new and
		// therefore visible, while m_bMarkersVisible belongs to the component and survives a map close.
		SetMarkersVisible(m_bMarkersVisible);
	}

	//------------------------------------------------------------------------------------------------
	//! Destroy every marker widget and forget its cached quads. RemoveFromHierarchy rather than just
	//! clearing the map: this runs on live rebuilds, and a dropped reference leaves an orphan marker.
	protected void ClearMarkers()
	{
		if (m_Widgets)
		{
			for (int i = 0; i < m_Widgets.Count(); i++)
			{
				Widget w = m_Widgets.GetElement(i);
				if (w)
					w.RemoveFromHierarchy();
			}

			m_Widgets.Clear();
		}

		if (m_mTagQuads)
			m_mTagQuads.Clear();

		if (m_mSymbolQuads)
			m_mSymbolQuads.Clear();

		m_sMagnetGroupId = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		if (!m_Widgets)
			return;

		// Nothing to position while the markers are filtered off. Skipping the loop is also what keeps
		// this method's opacity handling from fighting SetMarkersVisible: the two would otherwise both
		// be deciding whether a marker is on screen, using different widget properties.
		if (!m_bMarkersVisible)
			return;

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand)
			return;

		TickMagnet(highCommand);

		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			string groupId = m_Widgets.GetKey(i);
			Widget w = m_Widgets.GetElement(i);
			if (!w)
				continue;

			OVT_HighCommandRecord record = highCommand.GetGroup(groupId);
			if (!record)
			{
				// The record went away between the last rebuild and this tick. Fading rather than
				// hiding keeps SetVisible the filter's exclusive property; the removal invoker will
				// rebuild the set momentarily.
				w.SetOpacity(0);
				continue;
			}

			float opacity = 1.0;
			if (record.m_sOwnerPersistentId != m_sLocalPersistentId)
				opacity = OTHER_OWNER_OPACITY;

			w.SetOpacity(opacity);

			float x, y;
			m_MapEntity.WorldToScreen(record.m_vLastKnownPosition[0], record.m_vLastKnownPosition[2], x, y, true);

			x = GetGame().GetWorkspace().DPIUnscale(x);
			y = GetGame().GetWorkspace().DPIUnscale(y);

			UpdateSymbolQuad(groupId, w);
			UpdateTag(groupId, w, record);

			FrameSlot.SetPos(w, x, y);
		}

		RefreshDistanceLine();

		TickCommandContext();
	}

	//------------------------------------------------------------------------------------------------
	//! Claim the nearest marker within the magnet radius of the map cursor.
	//!
	//! THE CURSOR MAGNET, and the reason there is no widget-side hover here at all: the widget tree
	//! cannot be used to enlarge a hover target (trace is clipped to parent bounds, ancestor size
	//! overrides squeeze grown containers - two failed attempts in map/core). Driven from the cursor's
	//! WORLD position, which the gamepad's virtual cursor updates exactly as the mouse does.
	//! \param[in] highCommand The manager, for the record lookup. Never null at the call site.
	protected void TickMagnet(notnull OVT_HighCommandManagerComponent highCommand)
	{
		string best = FindGroupNearCursor(highCommand);

		if (best == m_sMagnetGroupId)
			return;

		// Reassign the claim FIRST: UpdateSymbolQuad asks whether a group is the magnet, and both the
		// released and the claimed marker have to be repainted against the NEW answer.
		string previous = m_sMagnetGroupId;
		m_sMagnetGroupId = best;

		if (!previous.IsEmpty())
			RepaintSymbol(previous);

		if (!best.IsEmpty())
			RepaintSymbol(best);
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest visible group marker within m_fSelectRadiusPx of the map cursor.
	//! \param[in] highCommand The manager, for the record lookup.
	//! \return The group id, or "" when the cursor is not near one.
	protected string FindGroupNearCursor(notnull OVT_HighCommandManagerComponent highCommand)
	{
		if (!m_MapEntity || !m_Widgets)
			return string.Empty;

		if (m_fSelectRadiusPx <= 0)
			return string.Empty;

		float zoom = m_MapEntity.GetCurrentZoom();
		if (zoom <= 0)
			return string.Empty;

		float cursorX, cursorZ;
		m_MapEntity.GetMapCursorWorldPosition(cursorX, cursorZ);

		// Never claim a marker sitting under the layer-filter panel - walking its rows would otherwise
		// light up whatever is beneath it. The location info panel is deliberately NOT tested, the same
		// call the shipped hover magnet makes: suppressing under it would hide, un-suppress and flicker.
		Widget layersPanel = GetLayersPanelWidget();
		if (layersPanel && IsCursorInsideWidget(layersPanel, cursorX, cursorZ))
			return string.Empty;

		// This layer's OWN panel is tested, because unlike the location panel it carries live controls
		// whose press must not also land on the map underneath them.
		if (m_wPanel && IsCursorInsideWidget(m_wPanel, cursorX, cursorZ))
			return string.Empty;

		// Screen px -> world metres, the same conversion the shipped hover magnet uses.
		float radiusWorld = m_fSelectRadiusPx / zoom;

		string best = string.Empty;
		float bestDistSq;

		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			string groupId = m_Widgets.GetKey(i);

			Widget w = m_Widgets.GetElement(i);
			if (!w || !w.IsVisible())
				continue;

			OVT_HighCommandRecord record = highCommand.GetGroup(groupId);
			if (!record)
				continue;

			float dx = record.m_vLastKnownPosition[0] - cursorX;
			float dz = record.m_vLastKnownPosition[2] - cursorZ;
			float distSq = (dx * dx) + (dz * dz);

			if (distSq > radiusWorld * radiusWorld)
				continue;

			if (best.IsEmpty() || distSq < bestDistSq)
			{
				best = groupId;
				bestDistSq = distSq;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the map cursor's screen position falls inside a widget's rect.
	//! \param[in] widget The widget to test. Null, or a zero-sized rect, is never "inside".
	//! \param[in] worldX Cursor world X.
	//! \param[in] worldZ Cursor world Z.
	//! \return True when the cursor is over the widget.
	protected bool IsCursorInsideWidget(Widget widget, float worldX, float worldZ)
	{
		if (!widget || !m_MapEntity)
			return false;

		if (!widget.IsVisible())
			return false;

		float screenX, screenY;
		m_MapEntity.WorldToScreen(worldX, worldZ, screenX, screenY, true);

		float panelX, panelY, panelW, panelH;
		widget.GetScreenPos(panelX, panelY);
		widget.GetScreenSize(panelW, panelH);

		if (panelW <= 0 || panelH <= 0)
			return false;

		if (screenX < panelX || screenX > panelX + panelW)
			return false;

		if (screenY < panelY || screenY > panelY + panelH)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The map layer-filter panel's widget while it is open, resolved on demand.
	//! \return The panel widget while it is open, null otherwise.
	protected Widget GetLayersPanelWidget()
	{
		if (!m_MapEntity)
			return null;

		OVT_MapLayersUI layersUI = OVT_MapLayersUI.Cast(m_MapEntity.GetMapUIComponent(OVT_MapLayersUI));
		if (!layersUI)
			return null;

		return layersUI.GetPanelWidget();
	}

	//------------------------------------------------------------------------------------------------
	//! Repaint one marker's affiliation quad against the current selection and magnet claims.
	//! \param[in] groupId The group whose marker to repaint.
	protected void RepaintSymbol(string groupId)
	{
		if (!m_Widgets)
			return;

		Widget w;
		if (!m_Widgets.Find(groupId, w))
			return;

		if (!w)
			return;

		UpdateSymbolQuad(groupId, w);
	}

	//------------------------------------------------------------------------------------------------
	//! Drive one marker's SELECTION RING - the selection channel, which is neither visibility nor
	//! opacity.
	//!
	//! The ring is its OWN widget over a permanently filled Friend_Land_Bcg base. Friend_Focus_Land and
	//! Friend_Select_Land are hollow overlay quads: swapping the BASE image to one of them (the first
	//! shape this shipped with) replaced the filled green field with an outline, so a hovered or
	//! selected group read as transparent. Play-test caught it.
	//! Change-filtered, because LoadImageFromSet is a resource lookup and this runs every frame.
	//! \param[in] groupId The group this marker belongs to.
	//! \param[in] markerWidget The marker's root widget.
	protected void UpdateSymbolQuad(string groupId, notnull Widget markerWidget)
	{
		ImageWidget ring = ImageWidget.Cast(markerWidget.FindAnyWidget(WIDGET_SELECT_RING));
		if (!ring)
			return;

		string quad = string.Empty;
		if (groupId == m_sSelectedGroupId)
			quad = QUAD_SELECT;
		else if (groupId == m_sMagnetGroupId)
			quad = QUAD_FOCUS;

		string lastQuad;
		if (m_mSymbolQuads && m_mSymbolQuads.Find(groupId, lastQuad))
		{
			if (lastQuad == quad)
				return;
		}

		if (m_mSymbolQuads)
			m_mSymbolQuads.Set(groupId, quad);

		if (quad.IsEmpty())
		{
			ring.SetVisible(false);
			return;
		}

		ring.LoadImageFromSet(0, SYMBOL_IMAGESET, quad);
		ring.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Drive one marker's status badge from its record's flags.
	//!
	//! An empty quad from OVT_HighCommandStatus.TagIcon means "no badge" and hides the widget. Only
	//! re-loaded when it changes, because this runs every frame for every marker.
	//! \param[in] groupId The group this marker belongs to.
	//! \param[in] markerWidget The marker's root widget.
	//! \param[in] record The group's record.
	protected void UpdateTag(string groupId, notnull Widget markerWidget, notnull OVT_HighCommandRecord record)
	{
		ImageWidget tag = ImageWidget.Cast(markerWidget.FindAnyWidget(WIDGET_TAG_IMAGE));
		if (!tag)
			return;

		string quad = OVT_HighCommandStatus.TagIcon(record.m_iStatusFlags);

		string lastQuad;
		if (m_mTagQuads && m_mTagQuads.Find(groupId, lastQuad))
		{
			if (lastQuad == quad)
				return;
		}

		if (m_mTagQuads)
			m_mTagQuads.Set(groupId, quad);

		if (quad.IsEmpty())
		{
			tag.SetVisible(false);
			return;
		}

		if (quad == OVT_HighCommandStatus.TAG_CONTACT)
		{
			tag.LoadImageFromSet(0, SYMBOL_IMAGESET, BADGE_CONTACT_QUAD);
			tag.SetColor(Color.FromRGBA(255, 70, 70, 255));
			tag.SetVisible(true);
			return;
		}

		tag.LoadImageFromSet(0, MARKER_IMAGESET, BADGE_NO_AMMO_QUAD);
		tag.SetColor(Color.FromRGBA(255, 255, 255, 255));
		tag.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Renew the map command input context, for exactly the frames a group is selected.
	//!
	//! EXCLUSIVE (Flags 0x2e, bit 0x8) because both verbs sit on inputs MapContext already owns:
	//! gamepad0:x is MapContextualMenu (SCR_MapRadialUI is carried by MapFullscreen.conf, which
	//! MapOverthrow.conf inherits) and keyboard:KC_B is MapToolProtractor. Priority alone never
	//! shadows. It claims ONLY the two verbs and references no Menu* action - an exclusive claim on
	//! those would take the left stick, the d-pad and gamepad0:a away from the map's own panning,
	//! tool menu and selection, which is how a pad moves here.
	//!
	//! Activation is a single-frame lease, so NOT calling this is the entire teardown. The one-frame
	//! arm delay is SCR_MapMarkersUI's: the press that selects a group is gamepad0:a, and going live
	//! in that frame would offer the same press to the verbs.
	//!
	//! The menu guard is what keeps this and Phase 4's OverthrowHighCommandContext from ever being
	//! live together - they share gamepad0:x and keyboard:KC_B, and Buy spends money.
	protected void TickCommandContext()
	{
		if (m_sSelectedGroupId.IsEmpty() || !m_bMarkersVisible || IsOverthrowMenuOpen())
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

		inputManager.ActivateContext(INPUT_CONTEXT_COMMAND);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any Overthrow UI context currently owns the screen.
	//! \return True while an Overthrow menu or placement session is up.
	protected bool IsOverthrowMenuOpen()
	{
		IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
		if (!controlled)
			return false;

		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(controlled.FindComponent(OVT_UIManagerComponent));
		if (!ui)
			return false;

		return ui.IsAnyContextBlocking();
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide every High Command marker. SetVisible, and deliberately NEVER SetOpacity - Update()
	//! owns opacity and rewrites it every frame. Hiding also drops the selection, or the input lease
	//! would stay live over a marker set the player has switched off.
	//! \param[in] visible True to draw the High Command markers, false to hide them all.
	void SetMarkersVisible(bool visible)
	{
		m_bMarkersVisible = visible;

		if (!visible)
			ClearSelection();

		if (!m_Widgets)
			return;

		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			Widget w = m_Widgets.GetElement(i);
			if (w)
				w.SetVisible(visible);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the High Command markers are switched on.
	bool AreMarkersVisible()
	{
		return m_bMarkersVisible;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this map session actually created markers - see m_bAvailableThisSession.
	//! \return True when a High Command row is worth offering in the layer-filter panel.
	bool IsAvailableThisSession()
	{
		return m_bAvailableThisSession;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribe to the High Command table. The manager outlives the map, so this is strictly paired
	//! with UnsubscribeFromTable().
	protected void SubscribeToTable()
	{
		if (m_bSubscribed)
			return;

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand)
			return;

		highCommand.m_OnHCGroupAdded.Insert(OnTableChanged);
		highCommand.m_OnHCGroupRemoved.Insert(OnTableChanged);
		highCommand.m_OnHCGroupUpdated.Insert(OnGroupUpdated);

		OVT_HighCommandRequestComponent requests = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();
		if (requests)
			requests.m_OnHCResult.Insert(OnHCResult);

		m_bSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove everything SubscribeToTable() inserted.
	protected void UnsubscribeFromTable()
	{
		if (!m_bSubscribed)
			return;

		m_bSubscribed = false;

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (highCommand)
		{
			highCommand.m_OnHCGroupAdded.Remove(OnTableChanged);
			highCommand.m_OnHCGroupRemoved.Remove(OnTableChanged);
			highCommand.m_OnHCGroupUpdated.Remove(OnGroupUpdated);
		}

		OVT_HighCommandRequestComponent requests = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();
		if (requests)
			requests.m_OnHCResult.Remove(OnHCResult);
	}

	//------------------------------------------------------------------------------------------------
	//! A record changed in place - an order or a heartbeat. The MARKERS need no rebuild for this,
	//! because Update() re-reads every record every frame; only the open panel is redrawn, and only
	//! when the change was about the group it is describing.
	//! \param[in] record The record that changed.
	protected void OnGroupUpdated(OVT_HighCommandRecord record)
	{
		if (!record)
			return;

		if (record.m_sGroupId != m_sSelectedGroupId)
			return;

		RefreshPanel();
	}

	//------------------------------------------------------------------------------------------------
	//! A group appeared or disappeared - rebuild the marker set. A rebuild is only warranted here
	//! because the widget SET changed; an in-place record change goes through OnGroupUpdated.
	//! \param[in] record The record that changed. Unused, present to match the invoker's signature.
	protected void OnTableChanged(OVT_HighCommandRecord record)
	{
		string keepSelected = m_sSelectedGroupId;

		BuildMarkers();

		// A rebuild forgets nothing about the selection except the widget it painted, so re-assert it -
		// unless the selected group is what just went away.
		if (keepSelected.IsEmpty())
			return;

		if (!m_Widgets || !m_Widgets.Contains(keepSelected))
		{
			ClearSelection();
			return;
		}

		m_sSelectedGroupId = keepSelected;
		RepaintSymbol(keepSelected);
	}

	//------------------------------------------------------------------------------------------------
	//! Install the two map-level action listeners. Paired with UnregisterInputs on map close: the input
	//! manager outlives the map, so a listener left behind would fire into a dead layer.
	protected void RegisterInputs()
	{
		if (m_bInputsRegistered)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener(ACTION_MAP_SELECT, EActionTrigger.UP, OnMapSelect);

		m_bInputsRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove everything RegisterInputs() installed.
	protected void UnregisterInputs()
	{
		if (!m_bInputsRegistered)
			return;

		m_bInputsRegistered = false;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.RemoveActionListener(ACTION_MAP_SELECT, EActionTrigger.UP, OnMapSelect);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla's map click, on this layer's terms: the magnet's claim becomes the selection, and a
	//! click that claims nothing deselects.
	//! \param[in] value Unused, present to match the input listener's signature.
	//! \param[in] reason Unused, present to match the input listener's signature.
	protected void OnMapSelect(float value, EActionTrigger reason)
	{
		if (!m_bMarkersVisible)
			return;

		if (m_sMagnetGroupId.IsEmpty())
		{
			// Only a click on bare map deselects. A click over a panel claims nothing AND must change
			// nothing, or a mouse user could never reach this layer's own buttons.
			if (IsCursorOverAnyPanel())
				return;

			ClearSelection();
			return;
		}

		SelectGroup(m_sMagnetGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the map cursor is over the layer-filter panel or this layer's own panel.
	protected bool IsCursorOverAnyPanel()
	{
		if (!m_MapEntity)
			return false;

		float cursorX, cursorZ;
		m_MapEntity.GetMapCursorWorldPosition(cursorX, cursorZ);

		if (IsCursorInsideWidget(GetLayersPanelWidget(), cursorX, cursorZ))
			return true;

		return IsCursorInsideWidget(m_wPanel, cursorX, cursorZ);
	}

	//------------------------------------------------------------------------------------------------
	//! Select a group and raise its info panel. Selecting a group the local player does not own is
	//! allowed and shows the same panel with its controls disabled and a reason (F8, BUG-102).
	//!
	//! PUBLIC (resistance/high-command Phase 8): this is the roster's "Show on Map" entry point too -
	//! OVT_HighCommandRosterContext calls it once the map finishes opening rather than duplicating the
	//! selection state (m_sSelectedGroupId, the ID_D quad channel, the info panel) a second time.
	//! \param[in] groupId The group to select.
	void SelectGroup(string groupId)
	{
		if (groupId == m_sSelectedGroupId)
			return;

		string previous = m_sSelectedGroupId;

		// Reassign the claim FIRST, for the same reason the magnet does: UpdateSymbolQuad asks whether
		// a group is selected and both markers must be repainted against the NEW answer.
		m_sSelectedGroupId = groupId;

		if (!previous.IsEmpty())
			RepaintSymbol(previous);

		RepaintSymbol(groupId);

		ShowPanel();
	}

	//------------------------------------------------------------------------------------------------
	//! Drop the selection, tear the panel down and let the input lease expire on the next frame.
	protected void ClearSelection()
	{
		string previous = m_sSelectedGroupId;

		m_sSelectedGroupId = string.Empty;
		m_bNavContextArmed = false;

		if (!previous.IsEmpty())
			RepaintSymbol(previous);

		HidePanel();
	}

	//------------------------------------------------------------------------------------------------
	//! Create the info panel and wire its two verbs. The panel is destroyed and recreated per
	//! selection, so the invoker Clear() is belt-and-braces rather than load-bearing.
	protected void ShowPanel()
	{
		HidePanel();

		if (!m_RootWidget)
			return;

		if (m_PanelLayout.IsEmpty())
		{
			Print("[Overthrow] OVT_MapHighCommandLayer: no m_PanelLayout configured, a selected High Command group will show no info panel. Set it in Configs/Map/MapOverthrow.conf", LogLevel.ERROR);
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_wPanel = workspace.CreateWidgets(m_PanelLayout, m_RootWidget);
		if (!m_wPanel)
		{
			Print("[Overthrow] OVT_MapHighCommandLayer: could not create the High Command info panel layout " + m_PanelLayout, LogLevel.ERROR);
			return;
		}

		SCR_InputButtonComponent orderComp = ResolveVerb(WIDGET_ORDER_BUTTON);
		if (orderComp)
		{
			orderComp.m_OnActivated.Clear();
			orderComp.m_OnActivated.Insert(OnOrderPressed);
		}

		SCR_InputButtonComponent stanceComp = ResolveVerb(WIDGET_STANCE_BUTTON);
		if (stanceComp)
		{
			stanceComp.m_OnActivated.Clear();
			stanceComp.m_OnActivated.Insert(OnStancePressed);
		}

		// Deselect rides Overthrow's existing map action (KC_C / gamepad0:b) through the button rather
		// than through a listener of this layer's own: the panel is the only time deselect means
		// anything, and going through the button makes the mouse click work as well as the keybind.
		SCR_InputButtonComponent closeComp = ResolveVerb(WIDGET_CLOSE_BUTTON);
		if (closeComp)
		{
			closeComp.m_OnActivated.Clear();
			closeComp.m_OnActivated.Insert(ClearSelection);
		}

		RefreshPanel();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve one of the panel's navigation buttons.
	//!
	//! SCR_InputButtonComponent.OnInput fires m_OnActivated on the bound action REGARDLESS OF FOCUS and
	//! early-returns when the button is not enabled in hierarchy - so the button is both the pad glyph
	//! and the keybind handler. A second AddActionListener on the same action would double-fire it.
	//! \param[in] widgetName The button's widget name.
	//! \return Its input component, or null on a stale layout.
	protected SCR_InputButtonComponent ResolveVerb(string widgetName)
	{
		if (!m_wPanel)
			return null;

		ButtonWidget button = ButtonWidget.Cast(m_wPanel.FindAnyWidget(widgetName));
		if (!button)
		{
			Print("[Overthrow] OVT_MapHighCommandLayer: the High Command info panel has no '" + widgetName + "' widget, that order verb is unreachable", LogLevel.ERROR);
			return null;
		}

		SCR_InputButtonComponent comp = SCR_InputButtonComponent.Cast(button.FindHandler(SCR_InputButtonComponent));
		if (!comp)
			Print("[Overthrow] OVT_MapHighCommandLayer: '" + widgetName + "' has no SCR_InputButtonComponent - check the inherited component GUID in MapHighCommandPanel.layout", LogLevel.ERROR);

		return comp;
	}

	//------------------------------------------------------------------------------------------------
	//! Destroy the info panel.
	protected void HidePanel()
	{
		if (!m_wPanel)
			return;

		m_wPanel.RemoveFromHierarchy();
		m_wPanel = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-read everything volatile onto the open panel: the record is refreshed by a broadcast every
	//! time the group is ordered and by the heartbeat every ten seconds, and the player's own distance
	//! to it changes with no event at all.
	protected void RefreshPanel()
	{
		if (!m_wPanel)
			return;

		if (m_sSelectedGroupId.IsEmpty())
		{
			HidePanel();
			return;
		}

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand)
			return;

		OVT_HighCommandRecord record = highCommand.GetGroup(m_sSelectedGroupId);
		if (!record)
		{
			ClearSelection();
			return;
		}

		// Every nested key is resolved BEFORE it is substituted: WidgetManager.Translate replaces a
		// placeholder with the literal string it is handed and does not translate it a second time, so
		// a raw "#OVT-..." passed as %1 would render as the key.
		SetPanelText(WIDGET_GROUP_NAME, ResolveGroupTitle(record));
		SetPanelText(WIDGET_OWNER_LINE, "#OVT-Owner: " + ResolveOwnerName(record));
		SetPanelText(WIDGET_STANCE_LINE, WidgetManager.Translate("#OVT-HC_Map_StanceLine", WidgetManager.Translate(StanceLabel(record.m_iStance))));
		SetPanelText(WIDGET_DESTINATION_LINE, WidgetManager.Translate("#OVT-HC_Map_DestinationLine", FormatDistance(vector.Distance(record.m_vLastKnownPosition, record.m_vDestination))));
		SetPanelText(WIDGET_STATUS_LINE, WidgetManager.Translate("#OVT-HC_Map_StatusLine", WidgetManager.Translate(StatusLabel(record)), record.m_iAliveMembers.ToString(), record.m_iTotalMembers.ToString()));

		RefreshDistanceLine();

		// NOT named `owned` - that is a reserved EnforceScript keyword and a local using it fails to
		// parse with "Broken expression (missing ';'?)".
		bool isOwner = IsOwnedLocally(record);

		SetVerbEnabled(WIDGET_ORDER_BUTTON, isOwner);
		SetVerbEnabled(WIDGET_STANCE_BUTTON, isOwner);

		if (!isOwner)
			SetReason(WidgetManager.Translate("#OVT-HC_Map_NotOwner", ResolveOwnerName(record)));
	}

	//------------------------------------------------------------------------------------------------
	//! The one line on the panel that changes with no event behind it - the player walks, the group
	//! walks, and neither move fires an invoker. Everything else is refreshed from the record deltas.
	protected void RefreshDistanceLine()
	{
		if (!m_wPanel)
			return;

		OVT_HighCommandRecord record = GetSelectedRecord();
		if (!record)
			return;

		SetPanelText(WIDGET_DISTANCE_LINE, WidgetManager.Translate("#OVT-HC_Map_DistanceLine", FormatDistanceFromPlayer(record)));
	}

	//------------------------------------------------------------------------------------------------
	//! Show a verb, enabled or not. Never hidden: hiding would leave the refusal unexplained (BUG-102).
	//! SetEnabled(false) kills both input paths - OnInput and OnClick share IsEnabledInHierarchy.
	//! \param[in] widgetName The button's widget name.
	//! \param[in] enabled Whether the verb may be used.
	protected void SetVerbEnabled(string widgetName, bool enabled)
	{
		if (!m_wPanel)
			return;

		ButtonWidget button = ButtonWidget.Cast(m_wPanel.FindAnyWidget(widgetName));
		if (!button)
			return;

		button.SetVisible(true);
		button.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return True when the local player owns this group.
	protected bool IsOwnedLocally(notnull OVT_HighCommandRecord record)
	{
		if (m_sLocalPersistentId.IsEmpty())
			return false;

		return record.m_sOwnerPersistentId == m_sLocalPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! Order the selected group to the map cursor's world position, keeping its current stance.
	//!
	//! The client-side ownership check is a courtesy, not the gate: RpcAsk_OrderGroup refuses a group
	//! the caller does not own with RESULT_NOT_OWNER whatever this method does.
	protected void OnOrderPressed()
	{
		OVT_HighCommandRecord record = GetSelectedRecord();
		if (!record)
			return;

		if (!m_MapEntity)
			return;

		float cursorX, cursorZ;
		m_MapEntity.GetMapCursorWorldPosition(cursorX, cursorZ);

		vector destination = Vector(cursorX, 0, cursorZ);

		if (!OVT_HighCommandRules.IsDestinationLegal(destination))
		{
			SetReason("#OVT-HC_Map_BadDestination");
			return;
		}

		SendOrder(record.m_sGroupId, record.m_iStance, destination);
	}

	//------------------------------------------------------------------------------------------------
	//! Cycle the selected group's stance and re-issue at the destination it already has. SpawnGroup
	//! seeds a fresh group's destination with where it was bought, so this is always legal; the
	//! last-known position is the fallback if it ever is not.
	protected void OnStancePressed()
	{
		OVT_HighCommandRecord record = GetSelectedRecord();
		if (!record)
			return;

		int stance = record.m_iStance + 1;
		if (stance > OVT_EHighCommandStance.ATTACK)
			stance = OVT_EHighCommandStance.DEFEND;

		vector destination = record.m_vDestination;
		if (!OVT_HighCommandRules.IsDestinationLegal(destination))
			destination = record.m_vLastKnownPosition;

		if (!OVT_HighCommandRules.IsDestinationLegal(destination))
		{
			SetReason("#OVT-HC_Map_BadDestination");
			return;
		}

		SendOrder(record.m_sGroupId, stance, destination);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The selected group's record, or null when nothing is selected or the record has gone.
	protected OVT_HighCommandRecord GetSelectedRecord()
	{
		if (m_sSelectedGroupId.IsEmpty())
			return null;

		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand)
			return null;

		return highCommand.GetGroup(m_sSelectedGroupId);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] groupId The group to order.
	//! \param[in] stance An OVT_EHighCommandStance value.
	//! \param[in] destination Where to send it.
	protected void SendOrder(string groupId, int stance, vector destination)
	{
		OVT_HighCommandRequestComponent requests = OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get();
		if (!requests)
		{
			SetReason("#OVT-HC_Map_NoSeam");
			return;
		}

		ClearReason();

		requests.RequestOrderGroup(groupId, stance, destination);
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to an order. Only the selected group's answer is rendered, and only a
	//! refusal - a successful order announces itself by moving the marker.
	//! \param[in] resultCode An OVT_HighCommandRequestComponent.RESULT_* value.
	//! \param[in] charged Unused here; the order path never charges.
	//! \param[in] groupId The group the answer is about.
	protected void OnHCResult(int resultCode, int charged, string groupId)
	{
		if (groupId != m_sSelectedGroupId)
			return;

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_OK)
		{
			ClearReason();
			RefreshPanel();
			return;
		}

		SetReason(RefusalText(resultCode));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] resultCode An OVT_HighCommandRequestComponent.RESULT_* value.
	//! \return The localization key explaining it.
	protected string RefusalText(int resultCode)
	{
		if (resultCode == OVT_HighCommandRequestComponent.RESULT_NOT_OWNER)
			return "#OVT-HC_Map_RefusedNotOwner";

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_NO_GROUP)
			return "#OVT-HC_Map_RefusedNoGroup";

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_BAD_DESTINATION)
			return "#OVT-HC_Map_BadDestination";

		if (resultCode == OVT_HighCommandRequestComponent.RESULT_BAD_STANCE)
			return "#OVT-HC_Map_RefusedBadStance";

		return "#OVT-HC_Map_RefusedGeneric";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] text A localization key or a finished string.
	protected void SetReason(string text)
	{
		if (!m_wPanel)
			return;

		TextWidget reason = TextWidget.Cast(m_wPanel.FindAnyWidget(WIDGET_REASON_TEXT));
		if (!reason)
			return;

		reason.SetText(text);
		reason.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearReason()
	{
		if (!m_wPanel)
			return;

		TextWidget reason = TextWidget.Cast(m_wPanel.FindAnyWidget(WIDGET_REASON_TEXT));
		if (!reason)
			return;

		reason.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] widgetName A text widget's name inside the panel.
	//! \param[in] text The finished string.
	protected void SetPanelText(string widgetName, string text)
	{
		if (!m_wPanel)
			return;

		TextWidget widget = TextWidget.Cast(m_wPanel.FindAnyWidget(widgetName));
		if (!widget)
			return;

		widget.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return The authored entry's ICO_Land quad name, or the infantry fallback.
	protected string ResolveTypeIcon(notnull OVT_HighCommandRecord record)
	{
		OVT_HighCommandGroupEntry entry = ResolveEntry(record);
		if (!entry || entry.m_sMapIcon.IsEmpty())
			return TYPE_ICON_FALLBACK;

		return entry.m_sMapIcon;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return The authored entry's display name, or the entry key when the catalog cannot answer.
	protected string ResolveGroupTitle(notnull OVT_HighCommandRecord record)
	{
		OVT_HighCommandGroupEntry entry = ResolveEntry(record);
		if (!entry || entry.m_sTitle.IsEmpty())
			return record.m_sEntryKey;

		return entry.m_sTitle;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return The authored catalog entry the record names, or null.
	protected OVT_HighCommandGroupEntry ResolveEntry(notnull OVT_HighCommandRecord record)
	{
		OVT_HighCommandManagerComponent highCommand = OVT_Global.GetHighCommand();
		if (!highCommand)
			return null;

		return highCommand.GetEntryByKey(record.m_sEntryKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns an ALREADY-TRANSLATED name: it is concatenated into one string and substituted into
	//! another, and a raw "#OVT-" key renders literally in both of those positions.
	//! \param[in] record The group's record.
	//! \return The owning player's name, or a translated placeholder when the roster cannot answer.
	protected string ResolveOwnerName(notnull OVT_HighCommandRecord record)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return WidgetManager.Translate("#OVT-HC_Map_UnknownOwner");

		string name = players.GetPlayerName(record.m_sOwnerPersistentId);
		if (name.IsEmpty())
			return WidgetManager.Translate("#OVT-HC_Map_UnknownOwner");

		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] stance An OVT_EHighCommandStance value.
	//! \return Its localization key.
	protected string StanceLabel(int stance)
	{
		if (stance == OVT_EHighCommandStance.PATROL)
			return "#OVT-HC_Stance_Patrol";

		if (stance == OVT_EHighCommandStance.ATTACK)
			return "#OVT-HC_Stance_Attack";

		return "#OVT-HC_Stance_Defend";
	}

	//------------------------------------------------------------------------------------------------
	//! One word for what the group is doing, contact first because it is the one a player acts on.
	//! \param[in] record The group's record.
	//! \return Its localization key.
	protected string StatusLabel(notnull OVT_HighCommandRecord record)
	{
		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.CONTACT))
			return "#OVT-HC_Map_StatusContact";

		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.NO_AMMO))
			return "#OVT-HC_Map_StatusNoAmmo";

		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.MOVING))
			return "#OVT-HC_Map_StatusMoving";

		return "#OVT-HC_Map_StatusHolding";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return The local player's distance to the group, formatted, or a placeholder.
	protected string FormatDistanceFromPlayer(notnull OVT_HighCommandRecord record)
	{
		IEntity controlled = SCR_PlayerController.GetLocalControlledEntity();
		if (!controlled)
			return WidgetManager.Translate("#OVT-HC_Map_DistanceUnknown");

		return FormatDistance(vector.Distance(controlled.GetOrigin(), record.m_vLastKnownPosition));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] distance Metres.
	//! \return The map's own distance formatting, so every readout on this screen agrees.
	protected string FormatDistance(float distance)
	{
		string value, units;
		SCR_Global.GetDistForHUD(distance, false, value, units);

		return value + " " + units;
	}
}
