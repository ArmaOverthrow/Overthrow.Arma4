//------------------------------------------------------------------------------------------------
//! Map markers for every recruit the LOCAL player owns.
//!
//! Modelled line-for-line on OVT_MapPlayerLocation, which is the only other SCR_MapUIBaseComponent
//! in Overthrow that creates one widget per tracked thing and repositions it every frame. Where the
//! two differ, the difference is called out in a comment rather than left to be rediscovered.
//!
//! WHY THE MARKER SET IS KEYED BY RECRUIT ID AND NOT BY ENTITY. A parked (inactive) recruit is by
//! definition somewhere its owner is not, so its body is frequently NOT STREAMED to this client and
//! there is no entity to key on. The record always exists - it arrives with the JIP payload and is
//! kept current by RpcDo_RecruitCreated / RpcDo_RecruitRemoved - and it carries
//! m_vLastKnownPosition, refreshed on the owning client every STATUS_SYNC_INTERVAL_MS by
//! OVT_RecruitCommandComponent.RpcDo_RecruitStatus. That is the fallback position, and it is why a
//! garrison twelve kilometres away still draws.
//!
//! EVERYTHING VOLATILE IS READ PER FRAME, NEVER CACHED AT BUILD TIME - the inactive flag, the
//! status flags, the position. A recruit parked or unparked while the map is open must change
//! opacity on the next tick, not on the next map open.
//------------------------------------------------------------------------------------------------
class OVT_MapRecruitLocation : SCR_MapUIBaseComponent
{
	//! Opacity of an INACTIVE (parked) recruit's marker. Active recruits draw at 1.0. Dimming rather
	//! than re-colouring keeps the one marker art doing both jobs and reads instantly at a glance.
	protected const float INACTIVE_MARKER_OPACITY = 0.45;

	//! The atlas the base marker and the status tag are both cut from. Declared as a named constant
	//! rather than a raw GUID at the call site, following OVT_MapShopPriceIndicator.c:56.
	protected const ResourceName MARKER_IMAGESET = "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset";

	protected const string WIDGET_IMAGE = "Image";
	protected const string WIDGET_TAG_IMAGE = "TagImage";

	[Attribute()]
	protected ResourceName m_Layout;

	[Attribute(defvalue: "", uiwidget: UIWidgets.Object, desc: "Optional faction colours for the recruit markers. Leave UNSET to use the shared default (recruits belong to the resistance, so green). Recruit markers are NOT a canvas layer, which is why this component carries its own copy of the palette attribute - the same duplication OVT_MapPlayerLocation carries.")]
	protected ref OVT_MapFactionPalette m_FactionColors;

	//! One widget per owned recruit, keyed by recruit id. Rebuilt on map open and on roster churn.
	protected ref map<string, ref Widget> m_Widgets;

	//! Last quad name pushed into each marker's TagImage, keyed by recruit id. Purely a change
	//! filter: LoadImageFromSet is a resource lookup and Update() runs every frame, so the tag is
	//! only re-loaded when the status flags actually change it.
	protected ref map<string, string> m_mTagQuads;

	//! The local player's persistent id, resolved once per map open.
	protected string m_sLocalPersistentId;

	//! Whether the recruit markers are switched on in the map layer-filter panel.
	//!
	//! A CLIENT-SIDE PRESENTATION PREFERENCE, deliberately not an [Attribute]: it is pushed in from
	//! the filter panel and the per-profile store it loads, never authored in config. Default true.
	protected bool m_bMarkersVisible = true;

	//! Whether this component actually created markers for the CURRENT map session.
	//!
	//! False until OnMapOpen clears every one of its early returns, INCLUDING "this player owns no
	//! recruits". The filter panel reads it to decide whether to offer a Recruits row at all - a
	//! toggle over markers that were never going to exist reads as a broken control rather than as
	//! an unavailable feature. Same rule and same reason as the Players row.
	protected bool m_bAvailableThisSession;

	//! Whether this component is currently subscribed to the recruit manager's roster invokers.
	//! Those invokers live on a manager that OUTLIVES the map, so the subscription must be removed
	//! on map close or a second open would fire the rebuild twice.
	protected bool m_bSubscribed;

	//------------------------------------------------------------------------------------------------
	//! This component's faction palette: its own if the conf configured one, otherwise the shared
	//! default. Never null. Mirrors OVT_MapPlayerLocation.GetFactionPalette for the same reason it
	//! duplicates OVT_MapCanvasLayer's - this is a SCR_MapUIBaseComponent and shares no base class
	//! with the canvas layers.
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

		// Reset FIRST, before any early return, so a session that bails out below leaves this false
		// and the layer-filter panel omits the Recruits row entirely. Availability is a property of
		// THIS map session, not of the component, and the component outlives the session.
		m_bAvailableThisSession = false;

		m_sLocalPersistentId = string.Empty;

		if (!m_Widgets)
			m_Widgets = new map<string, ref Widget>;

		if (!m_mTagQuads)
			m_mTagQuads = new map<string, string>;

		ClearMarkers();

		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId <= 0)
			return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return;

		m_sLocalPersistentId = players.GetPersistentIDFromPlayerID(localPlayerId);
		if (m_sLocalPersistentId.IsEmpty())
			return;

		// Subscribed even when the player currently owns no recruits: recruiting the first one with
		// the map open must produce a marker, and the rebuild is what re-runs the availability check.
		SubscribeToRoster();

		BuildMarkers();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMapClose(MapConfiguration config)
	{
		UnsubscribeFromRoster();

		ClearMarkers();

		m_bAvailableThisSession = false;

		super.OnMapClose(config);
	}

	//------------------------------------------------------------------------------------------------
	//! Create one marker widget per owned recruit and re-assert the visibility preference on them.
	//!
	//! Called on map open and again on every roster change while the map is open. It is the ONLY
	//! place m_bAvailableThisSession becomes true, so a player who owns nothing gets no filter row.
	protected void BuildMarkers()
	{
		ClearMarkers();

		if (m_sLocalPersistentId.IsEmpty())
			return;

		if (!m_RootWidget)
			return;

		if (m_Layout.IsEmpty())
		{
			Print("[Overthrow] OVT_MapRecruitLocation: no m_Layout configured, no recruit markers will be drawn. Set it in Configs/Map/MapOverthrow.conf", LogLevel.ERROR);
			return;
		}

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return;

		array<ref OVT_RecruitData> ownedRecruits = recruits.GetPlayerRecruits(m_sLocalPersistentId);
		if (!ownedRecruits || ownedRecruits.IsEmpty())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		// Recruits are the resistance by definition, so the role is asked for directly rather than
		// round-tripped through a faction index. The palette keeps these markers the same green as
		// the resistance's territory, rings and player markers.
		Color markerColour = GetFactionPalette().GetColorForRole(OVT_FactionType.RESISTANCE_FACTION);

		foreach (OVT_RecruitData recruit : ownedRecruits)
		{
			if (!recruit)
				continue;

			if (recruit.m_sRecruitId.IsEmpty())
				continue;

			Widget widget = workspace.CreateWidgets(m_Layout, m_RootWidget);
			if (!widget)
				continue;

			ImageWidget img = ImageWidget.Cast(widget.FindAnyWidget(WIDGET_IMAGE));
			if (img)
				img.SetColor(markerColour);

			// The tag starts hidden and is driven entirely by Update(). Nothing is decided here:
			// status flags and the inactive flag are volatile and are read per frame.
			ImageWidget tag = ImageWidget.Cast(widget.FindAnyWidget(WIDGET_TAG_IMAGE));
			if (tag)
				tag.SetVisible(false);

			m_Widgets[recruit.m_sRecruitId] = widget;
			m_mTagQuads[recruit.m_sRecruitId] = string.Empty;
		}

		if (m_Widgets.IsEmpty())
			return;

		// Past every early return and markers exist, so a Recruits row is worth offering.
		m_bAvailableThisSession = true;

		// Re-assert the current preference onto the freshly created widget set. The widgets are new
		// and therefore visible, while m_bMarkersVisible belongs to the component and survives a map
		// close - so without this, a player who hid the markers and then reopened the map would get a
		// new set of visible markers that Update() then refuses to reposition, leaving them parked
		// wherever the layout puts an unpositioned widget.
		SetMarkersVisible(m_bMarkersVisible);
	}

	//------------------------------------------------------------------------------------------------
	//! Destroy every marker widget and forget its cached tag quad.
	//!
	//! RemoveFromHierarchy() rather than just clearing the map: this runs on live rebuilds too, and
	//! a dropped reference to a widget still parented to the map root would leave a marker on screen
	//! that nothing owns and nothing repositions.
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
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeSlice)
	{
		if (!m_Widgets)
			return;

		// Nothing to position while the markers are filtered off. Skipping the loop is also what
		// keeps this method's opacity handling from fighting SetMarkersVisible: the two would
		// otherwise both be deciding whether a marker is on screen, using different widget
		// properties, and which one won would depend on tick order.
		if (!m_bMarkersVisible)
			return;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return;

		// Null on a client whose controller has not been assigned yet. A missing cache reads as
		// "no status", which every marker renders as a bare base image - never an error.
		OVT_RecruitCommandComponent commands = OVT_Global.GetRecruitCommands();

		for (int i = 0; i < m_Widgets.Count(); i++)
		{
			string recruitId = m_Widgets.GetKey(i);
			Widget w = m_Widgets.GetElement(i);
			if (!w)
				continue;

			OVT_RecruitData recruit = recruits.GetRecruit(recruitId);
			if (!recruit)
			{
				// The record went away between the last rebuild and this tick. Fading rather than
				// hiding keeps SetVisible the filter's exclusive property; the roster invoker will
				// rebuild the set momentarily.
				w.SetOpacity(0);
				continue;
			}

			// Fallback FIRST, live entity second: an inactive recruit's body is usually not streamed
			// to this client, and m_vLastKnownPosition is the only thing that will ever be true for it.
			vector pos = recruit.m_vLastKnownPosition;
			float heading = 0;
			bool haveHeading = false;

			IEntity recruitEntity = recruits.GetRecruitEntity(recruitId);
			if (recruitEntity)
			{
				pos = recruitEntity.GetOrigin();
				heading = recruitEntity.GetYawPitchRoll()[0];
				haveHeading = true;
			}

			float opacity = 1.0;
			if (recruit.m_bInactive)
				opacity = INACTIVE_MARKER_OPACITY;

			w.SetOpacity(opacity);

			float x, y;
			m_MapEntity.WorldToScreen(pos[0], pos[2], x, y, true);

			x = GetGame().GetWorkspace().DPIUnscale(x);
			y = GetGame().GetWorkspace().DPIUnscale(y);

			ImageWidget img = ImageWidget.Cast(w.FindAnyWidget(WIDGET_IMAGE));
			if (img)
			{
				// An unresolved body has no heading anyone can trust, so the marker is left pointing
				// north rather than frozen at whatever it was doing when it stopped streaming.
				if (haveHeading)
					img.SetRotation(heading);
				else
					img.SetRotation(0);
			}

			UpdateTag(recruitId, w, commands);

			FrameSlot.SetPos(w, x, y);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drive one marker's status tag from the client-side status cache.
	//!
	//! An empty quad name from OVT_RecruitStatus.TagIcon means "no tag" - an unarmed recruit, or one
	//! this client has no status for yet - and hides the widget. The quad is only re-loaded when it
	//! changes, because this runs every frame for every marker.
	//! \param[in] recruitId The recruit this marker belongs to.
	//! \param[in] markerWidget The marker's root widget.
	//! \param[in] commands The local status cache, may be null.
	protected void UpdateTag(string recruitId, notnull Widget markerWidget, OVT_RecruitCommandComponent commands)
	{
		ImageWidget tag = ImageWidget.Cast(markerWidget.FindAnyWidget(WIDGET_TAG_IMAGE));
		if (!tag)
			return;

		int flags = 0;
		if (commands)
			flags = commands.GetStatusFlags(recruitId);

		string quad = OVT_RecruitStatus.TagIcon(flags);

		string lastQuad;
		if (m_mTagQuads && m_mTagQuads.Find(recruitId, lastQuad))
		{
			if (lastQuad == quad)
				return;
		}

		if (m_mTagQuads)
			m_mTagQuads.Set(recruitId, quad);

		if (quad.IsEmpty())
		{
			tag.SetVisible(false);
			return;
		}

		tag.LoadImageFromSet(0, MARKER_IMAGESET, quad);
		tag.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Show or hide every recruit marker.
	//!
	//! SetVisible, and deliberately NEVER SetOpacity. Update() OWNS OPACITY: it is what draws an
	//! inactive recruit dimmed and an active one solid, and it rewrites that value every frame. A
	//! filter built on opacity would be contending with it for the same property on the same
	//! widgets, and the parked-recruit dimming would win or lose depending on tick order. Visibility
	//! is a property nothing else in this class touches.
	//! \param[in] visible True to draw the recruit markers, false to hide them all.
	void SetMarkersVisible(bool visible)
	{
		m_bMarkersVisible = visible;

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
	//! \return True while the recruit markers are switched on.
	bool AreMarkersVisible()
	{
		return m_bMarkersVisible;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this map session actually created recruit markers - see m_bAvailableThisSession.
	//! \return True when a Recruits row is worth offering in the layer-filter panel.
	bool IsAvailableThisSession()
	{
		return m_bAvailableThisSession;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribe to roster churn so recruiting or losing a recruit with the map open is visible.
	//!
	//! The manager outlives the map, so this is strictly paired with UnsubscribeFromRoster() on map
	//! close. m_bSubscribed guards against a double insert if OnMapOpen ever runs twice.
	protected void SubscribeToRoster()
	{
		if (m_bSubscribed)
			return;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return;

		recruits.m_OnRecruitAdded.Insert(OnRosterChanged);
		recruits.m_OnRecruitRemoved.Insert(OnRosterChanged);

		m_bSubscribed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove everything SubscribeToRoster() inserted.
	protected void UnsubscribeFromRoster()
	{
		if (!m_bSubscribed)
			return;

		m_bSubscribed = false;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if (!recruits)
			return;

		recruits.m_OnRecruitAdded.Remove(OnRosterChanged);
		recruits.m_OnRecruitRemoved.Remove(OnRosterChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! A recruit was added to or removed from SOMEBODY's roster - rebuild this player's marker set.
	//!
	//! The record is not inspected: filtering on ownership here would duplicate the filter
	//! BuildMarkers() already applies, and a full rebuild of at most MAX_RECRUITS_PER_PLAYER widgets
	//! on an event that fires a handful of times a session is not worth optimising.
	//! \param[in] recruit The record that changed. Unused, present to match the invoker's signature.
	protected void OnRosterChanged(OVT_RecruitData recruit)
	{
		BuildMarkers();
	}
};
