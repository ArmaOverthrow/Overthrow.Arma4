//------------------------------------------------------------------------------------------------
//! Which kind of Overthrow campaign object an editable-entity icon represents.
//!
//! PUBLIC AND STABLE BY DESIGN. gm-map reuses this enum (and the image names below) rather than
//! inventing a parallel classification, so the GM view and the GM map agree on what a thing is and
//! what it looks like. Add values here, never a second enum somewhere else.
//------------------------------------------------------------------------------------------------
enum OVT_EGMIconKind
{
	TOWN,
	BASE,
	RADIO_TOWER
}

//------------------------------------------------------------------------------------------------
//! Per-ENTITY UI info for a town, occupying-faction base or radio tower shown in the Game Master
//! editor. It supplies the icon and the name; nothing here talks to the network.
//!
//! WHY THIS EXISTS AT ALL. SCR_EditableEntityComponent.GetInfo() falls back to the info authored on
//! the COMPONENT CLASS (SCR_EditableEntityComponent.c:151-162) - one object shared by every town in
//! the world. Anything per-entity (this town's name, this town's village/town/city icon) therefore
//! needs an instance installed with SetInfoInstance(), which is what
//! OVT_GMEditableCampaignComponent does. Vanilla does exactly this for AI groups
//! (SCR_EditableGroupComponent.c:324-329).
//!
//! ICON CONVENTIONS (gm-map: match these, do not re-pick).
//!   town      -> overthrow_mapicons "village" / "town" / "city", by OVT_TownData.size
//!   radio tower -> overthrow_mapicons "tower"
//!   base      -> the vanilla editor texture EditableEntity_System_Base.edds, the same one Conflict's
//!                military bases use (Prefabs/Systems/MilitaryBase/ConflictBase_Base.et:40)
//!
//! EVERY LOOKUP IS LATE AND GUARDED. An instance of this class is built during the owning entity's
//! OnPostInit - before the town and occupying-faction managers have discovered anything, and on a
//! dedicated server where no icon is ever drawn - and the content browser can also instantiate one
//! straight from a prefab container with no owner at all. So the icon is re-derived on each draw
//! (SetIconTo) and the name on each read, and every manager, record and index is checked first.
//!
//! HOVER TOOLTIPS RIDE GetDescription(). Every editable entity type's tooltip already runs a
//! SCR_DescriptionTooltipDetail, which does entity.GetInfo().SetDescriptionTo(m_Text)
//! (SCR_DescriptionTooltipDetail.c:9) after gating on HasDescription() (:17) - so overriding
//! GetDescription() here puts live campaign state in the vanilla tooltip with no config fork, no
//! layout fork and no modded class. Two consequences that are easy to get wrong:
//!   - HasDescription() IS GetDescription() (SCR_UIDescription.c:39-41). Returning an empty string
//!     for a live entity makes InitDetail return false and the tooltip detail widget is REMOVED
//!     before it ever renders (SCR_EntityTooltipDetail.CreateDetail :37-46). So every path below
//!     ends in a non-empty line, DESC_UNAVAILABLE at worst.
//!   - The text is written ONCE PER HOVER, not ticked: SCR_EntityTooltipDetail.NeedUpdate() returns
//!     false (:17-20) and the description detail does not override it. Fresh on every new hover,
//!     frozen for the duration of one. That is correct for a tooltip - do NOT add a timer to
//!     "fix" it. The click-through detail readout in the panel is what tracks live values.
//! Group and player tooltips are deliberately absent (plan D6): vanilla owns the group info
//! instance and SCR_EditableGroupUIInfo has no back-reference to its entity.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_GMCampaignUIInfo : SCR_EditableEntityUIInfo
{
	//! Overthrow's own map icon set. Also used by the HUD and the map, so a new icon added for the
	//! GM view is automatically available to both.
	static const ResourceName ICON_IMAGESET = "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset";

	//! Vanilla editor texture for a military base - a plain .edds, NOT an image set.
	static const ResourceName ICON_BASE_TEXTURE = "{DD5F23CBB1731598}UI/Textures/Editor/EditableEntities/Systems/EditableEntity_System_Base.edds";

	static const string IMAGE_VILLAGE = "village";
	static const string IMAGE_TOWN = "town";
	static const string IMAGE_CITY = "city";
	static const string IMAGE_TOWER = "tower";

	//! Shown when a town record cannot be resolved yet (world still loading, or a client before JIP).
	static const LocalizedString NAME_TOWN_FALLBACK = "#OVT-GMIcon_Town";

	//! "Base %1" - the %1 is the base index, which is also the join key every gm-state base record
	//! is filed under, so the icon and the detail rows name the same base the same way.
	static const LocalizedString NAME_BASE_FORMAT = "#OVT-GMIcon_Base";

	//! Shown when the base index cannot be resolved yet.
	static const LocalizedString NAME_BASE_FALLBACK = "#OVT-GMIcon_BaseGeneric";

	static const LocalizedString NAME_RADIO_TOWER = "#OVT-GMIcon_RadioTower";

	//! "Support 62% - Stability 80%". Both arguments arrive pre-formatted, percent sign included, so
	//! the string carries no literal '%' of its own to be confused with a placeholder.
	static const LocalizedString DESC_TOWN_FORMAT = "#OVT-GMIcon_Tooltip_Town";

	//! "50 Resources - 5 Garrison Groups". Only shown when the gm-state store actually holds this
	//! base; no faction here - the vanilla tooltip already shows it below the description.
	static const LocalizedString DESC_BASE_FORMAT = "#OVT-GMIcon_Tooltip_Base";

	static const LocalizedString DESC_TOWER_ONLINE = "#OVT-GMIcon_Tooltip_TowerOnline";

	//! "Sabotaged - 4:12", the countdown formatted by OVT_GMPanelFormat.
	static const LocalizedString DESC_TOWER_SABOTAGED = "#OVT-GMIcon_Tooltip_TowerSabotaged";

	//! The last line of defence for HasDescription(). Anything that cannot be resolved yet - a world
	//! still loading, a client before JIP, a machine with no Overthrow managers at all - reads this
	//! rather than an empty string, which would delete the tooltip detail widget outright.
	static const LocalizedString DESC_UNAVAILABLE = "#OVT-GMIcon_Tooltip_NoData";

	//! The entity this info describes. A PLAIN reference on purpose: the info is held by a component
	//! which is held by this very entity, so a strong ref would be a cycle, and an entity outliving
	//! its own component is not a state this class can be asked about.
	protected IEntity m_Owner;

	//! What the owner is, copied from the component's authored attribute.
	protected OVT_EGMIconKind m_eKind;

	//------------------------------------------------------------------------------------------------
	//! Binds this info to one entity and one kind, and resolves the icon as far as it can right now.
	//! Safe to call before any Overthrow manager exists - the icon is re-derived on every draw.
	//! \param[in] owner The entity carrying OVT_GMEditableCampaignComponent.
	//! \param[in] kind What that entity is.
	void Configure(IEntity owner, OVT_EGMIconKind kind)
	{
		m_Owner = owner;
		m_eKind = kind;

		RefreshIcon();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The entity this info describes; null on a container-instantiated copy.
	IEntity GetOwnerEntity()
	{
		return m_Owner;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Which kind of campaign object this is.
	OVT_EGMIconKind GetKind()
	{
		return m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	//! The name on the icon and in the tooltip header: the town's own name, "Base 3", "Radio Tower".
	//!
	//! A shared placeholder appearing on every icon means SetInfoInstance() did not happen - that is
	//! the symptom to look for, and it is Phase 5 Step 1 check 3.
	//! \return Localized name; never empty.
	override LocalizedString GetName()
	{
		if (!m_Owner)
			return super.GetName();

		if (m_eKind == OVT_EGMIconKind.TOWN)
			return ResolveTownName();

		if (m_eKind == OVT_EGMIconKind.BASE)
			return ResolveBaseName();

		if (m_eKind == OVT_EGMIconKind.RADIO_TOWER)
			return NAME_RADIO_TOWER;

		return super.GetName();
	}

	//------------------------------------------------------------------------------------------------
	//! The one line under the entity name in the vanilla hover tooltip: this town's support and
	//! stability, this base's faction and garrison, this tower's status.
	//!
	//! NEVER EMPTY FOR A LIVE ENTITY - see the class header. HasDescription() calls straight through
	//! to here, and an empty answer destroys the tooltip's description widget before it renders, so
	//! every branch ends in a sentence even when nothing can be resolved.
	//!
	//! ONE LINE, NOT TWO. The tooltip is 276 px wide (Tooltip_Entity.layout:31); a second line is
	//! affordable but every readout here fits comfortably in one, and one line cannot wrap the
	//! tooltip into a shape that covers the icon a GM is aiming at.
	//! \return Localized description; never empty while this info has an owner.
	override LocalizedString GetDescription()
	{
		if (!m_Owner)
			return DescribeFallback();

		if (m_eKind == OVT_EGMIconKind.TOWN)
			return DescribeTown();

		if (m_eKind == OVT_EGMIconKind.BASE)
			return DescribeBase();

		if (m_eKind == OVT_EGMIconKind.RADIO_TOWER)
			return DescribeRadioTower();

		return DescribeFallback();
	}

	//------------------------------------------------------------------------------------------------
	//! Re-derives the icon before handing it to the widget.
	//!
	//! This is the ONLY moment at which a town's size is reliably known: town controllers in the
	//! campaign world are spawned by the manager and only given their data AFTER the spawn
	//! (OVT_TownManagerComponent.SpawnTownControllers), so an icon fixed at OnPostInit would be a
	//! village forever. Called once per icon widget creation
	//! (SCR_CustomEditableEntityUIComponent.OnInit), not per frame.
	//! \param[in] imageWidget Target widget.
	//! \return True when an image was set.
	override bool SetIconTo(ImageWidget imageWidget)
	{
		RefreshIcon();

		return super.SetIconTo(imageWidget);
	}

	//------------------------------------------------------------------------------------------------
	//! Overridden WITHOUT the 'protected' keyword so the owning component can seed this instance from
	//! the prefab-authored info - the same trick vanilla uses in SCR_EditableGroupUIInfo.c:73.
	//!
	//! ORDER MATTERS AT THE CALL SITE: SCR_UIInfo.CopyFrom() assigns Icon and IconSetName from the
	//! source, so it must run BEFORE Configure(), never after, or the icon this class picked is
	//! replaced by the prefab's (empty) one.
	//! \param[in] source Info to copy from; null is tolerated.
	override void CopyFrom(SCR_UIName source)
	{
		super.CopyFrom(source);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes Icon and IconSetName for the current kind. Icon is an image set for towns and towers
	//! (SCR_UIInfo.SetIconTo branches on the ".imageset" extension and then needs IconSetName -
	//! SCR_UIInfo.c:102-113) and a plain texture for bases, where IconSetName must be empty.
	protected void RefreshIcon()
	{
		if (m_eKind == OVT_EGMIconKind.BASE)
		{
			Icon = ICON_BASE_TEXTURE;
			IconSetName = string.Empty;
			return;
		}

		if (m_eKind == OVT_EGMIconKind.RADIO_TOWER)
		{
			Icon = ICON_IMAGESET;
			IconSetName = IMAGE_TOWER;
			return;
		}

		Icon = ICON_IMAGESET;
		IconSetName = ResolveTownImage();
	}

	//------------------------------------------------------------------------------------------------
	//! Village, town or city, from the nearest town record. OVT_TownData.size is filled during local
	//! discovery on EVERY machine (OVT_TownManagerComponent.FilterTownControllerEntities /
	//! ProcessTown), not replicated, so this reads correctly on a client too - which the town
	//! controller's own m_Size attribute would not, being a prefab default there.
	//! \return Image name inside ICON_IMAGESET; the generic town icon when nothing is known yet.
	protected string ResolveTownImage()
	{
		OVT_TownManagerComponent towns = GetTownsSafe();
		if (!towns || !m_Owner)
			return IMAGE_TOWN;

		OVT_TownData town = towns.GetNearestTown(m_Owner.GetOrigin());
		if (!town)
			return IMAGE_TOWN;

		if (town.size == OVT_TownSize.VILLAGE)
			return IMAGE_VILLAGE;

		// No capital icon exists in the set; a capital reads as a city.
		if (town.size == OVT_TownSize.CITY || town.size == OVT_TownSize.CAPITAL)
			return IMAGE_CITY;

		return IMAGE_TOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! The town's display name.
	//!
	//! BOUNDS AND CACHE ARE BOTH CHECKED BEFORE GetTownName() IS CALLED. It indexes m_TownNames and
	//! m_Towns unguarded, and when the cached name is empty it falls back to a 5 m map-marker query
	//! whose result it dereferences without a null check (OVT_TownManagerComponent.c:858-867). This
	//! runs from widget code on every hover, so it takes neither risk.
	//! \return Localized name; never empty.
	protected LocalizedString ResolveTownName()
	{
		OVT_TownManagerComponent towns = GetTownsSafe();
		if (towns && towns.m_Towns && towns.m_TownNames)
		{
			int townId = towns.GetNearestTownId(m_Owner.GetOrigin());

			// GetNearestTownId() returns 0 for an EMPTY town list, so the bounds check below is what
			// keeps an un-discovered world out of the array.
			if (townId >= 0 && townId < towns.m_Towns.Count() && townId < towns.m_TownNames.Count() && towns.m_TownNames[townId] != string.Empty)
				return towns.GetTownName(townId);
		}

		// The controller carries the same name the manager cached (it is where the manager read it
		// from, OVT_TownManagerComponent.c:1150-1153), so it answers before discovery has run.
		OVT_TownControllerComponent controller = OVT_TownControllerComponent.Cast(m_Owner.FindComponent(OVT_TownControllerComponent));
		if (controller && controller.m_sName != string.Empty)
			return controller.m_sName;

		return NAME_TOWN_FALLBACK;
	}

	//------------------------------------------------------------------------------------------------
	//! "Base 3", where 3 is the index into OVT_OccupyingFactionManager.m_Bases - the same join key
	//! gm-state files base records under. The index is JIP-streamed to clients, so this resolves on
	//! every machine once the campaign state has arrived.
	//! \return Localized name; never empty.
	protected LocalizedString ResolveBaseName()
	{
		OVT_OccupyingFactionManager occupying = GetOccupyingSafe();
		if (occupying)
		{
			OVT_BaseData base = occupying.GetNearestBase(m_Owner.GetOrigin());
			if (base)
			{
				int index = occupying.GetBaseIndex(base);
				if (index >= 0)
				{
					// Resolved here rather than returned as a raw "#key %1": a UI info's name goes
					// straight into TextWidget.SetText(), which has no channel for format arguments.
					return WidgetManager.Translate(NAME_BASE_FORMAT, index.ToString());
				}
			}
		}

		return NAME_BASE_FALLBACK;
	}

	//------------------------------------------------------------------------------------------------
	//! "Support 62% - Stability 80%", both from the locally replicated town record, so the tooltip is
	//! true on a client that has never been authorized by the gm-state seam.
	//!
	//! Support is run through the shared formatter rather than divided here: an integer division of
	//! support by population reads "0%" for a town at two thirds support, and that trap is already
	//! documented and Logic-tested in one place.
	//! \return One localized line; never empty.
	protected LocalizedString DescribeTown()
	{
		OVT_TownManagerComponent towns = GetTownsSafe();
		if (!towns)
			return DescribeFallback();

		OVT_TownData town = towns.GetNearestTown(m_Owner.GetOrigin());
		if (!town)
			return DescribeFallback();

		string support = OVT_GMIconFormat.FormatSupport(town.support, town.population);
		string stability = town.stability.ToString() + "%";

		// Resolved here rather than handed on as a raw "#key %1 %2": a description ends up in
		// TextWidget.SetText() (SCR_UIDescription.SetDescriptionTo), which has no format arguments.
		return WidgetManager.Translate(DESC_TOWN_FORMAT, support, stability);
	}

	//------------------------------------------------------------------------------------------------
	//! "50 Resources - 5 Garrison Groups" (user decision 2026-08-15: no faction here - the vanilla
	//! tooltip already shows the entity's faction below the description).
	//!
	//! BOTH VALUES ARE GATED. Resources and garrison count live in the gm-state store, which is empty
	//! for an unauthorized player and before the first snapshot. When the store cannot answer, the
	//! line reads the "no campaign data yet" fallback rather than claiming zeros - a wrong number is
	//! worse than a short sentence. This asks the seam for nothing else.
	//! \return One localized line; never empty.
	protected LocalizedString DescribeBase()
	{
		OVT_OccupyingFactionManager occupying = GetOccupyingSafe();
		if (!occupying)
			return DescribeFallback();

		OVT_BaseData base = occupying.GetNearestBase(m_Owner.GetOrigin());
		if (!base)
			return DescribeFallback();

		OVT_GMCampaignState state = GetCampaignStateSafe();
		if (!state || !state.HasData())
			return DescribeFallback();

		int index = occupying.GetBaseIndex(base);
		if (index < 0)
			return DescribeFallback();

		OVT_GMBaseRecord record = state.FindBase(index);
		if (!record)
			return DescribeFallback();

		return WidgetManager.Translate(DESC_BASE_FORMAT, record.m_iResources.ToString(), record.m_iGroups.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! "Online", or "Sabotaged - 4:12".
	//!
	//! The countdown is read through GetDisabledRemaining(), which extrapolates from the last
	//! snapshot on a client, and formatted by the panel's own countdown formatter - so the tooltip,
	//! the panel and the detail readout all render a duration identically. The value is sampled when
	//! the tooltip opens and does not tick while it is up (class header).
	//! \return One localized line; never empty.
	protected LocalizedString DescribeRadioTower()
	{
		OVT_OccupyingFactionManager occupying = GetOccupyingSafe();
		if (!occupying)
			return DescribeFallback();

		OVT_RadioTowerData tower = occupying.GetNearestRadioTower(m_Owner.GetOrigin());
		if (!tower)
			return DescribeFallback();

		if (!tower.IsDisabled())
			return DESC_TOWER_ONLINE;

		return WidgetManager.Translate(DESC_TOWER_SABOTAGED, OVT_GMPanelFormat.FormatCountdown(tower.GetDisabledRemaining()));
	}

	//------------------------------------------------------------------------------------------------
	//! What every unresolved path returns. Its whole job is to be non-empty, so that HasDescription()
	//! stays true and the tooltip keeps its description widget.
	//! \return A localized line; never empty.
	protected LocalizedString DescribeFallback()
	{
		return DESC_UNAVAILABLE;
	}

	//------------------------------------------------------------------------------------------------
	//! The gm-state client store, resolved through the controller component on every read.
	//!
	//! Never cached: it is null on a dedicated server, null on a client before ownership assignment,
	//! and the object behind it is replaced wholesale by each snapshot. A hover is rare enough that
	//! three pointer hops cost nothing.
	//! \return The store, or null when the seam is unavailable.
	protected OVT_GMCampaignState GetCampaignStateSafe()
	{
		if (!GetGame())
			return null;

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
			return null;

		return gm.GetState();
	}

	//------------------------------------------------------------------------------------------------
	//! Town manager, or null when there is no game yet. GetInstance() dereferences GetGame() without
	//! a guard, and this class is built during world init and from bare prefab containers.
	//! \return The manager or null.
	protected OVT_TownManagerComponent GetTownsSafe()
	{
		if (!GetGame())
			return null;

		return OVT_Global.GetTowns();
	}

	//------------------------------------------------------------------------------------------------
	//! Occupying-faction manager, or null when there is no game yet. See GetTownsSafe().
	//! \return The manager or null.
	protected OVT_OccupyingFactionManager GetOccupyingSafe()
	{
		if (!GetGame())
			return null;

		return OVT_Global.GetOccupyingFaction();
	}
}
