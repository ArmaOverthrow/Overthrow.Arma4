[ComponentEditorProps(category: "GameScripted/Editor (Editables)", description: "Renders an Overthrow town, base or radio tower as a Game Master icon.")]
class OVT_GMEditableCampaignComponentClass : SCR_EditableSystemComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Makes an Overthrow town controller, base controller or radio tower visible in the Game Master
//! editor: an icon at a readable height, tinted by controlling faction, hoverable and selectable,
//! and impossible to delete or drag.
//!
//! IT RIDES THE VANILLA EDITABLE-ENTITY SYSTEM AND ADDS NOTHING TO THE WIRE. The per-frame
//! reprojection, the hover ring, selection, the entity list and the tooltip pipeline all come free
//! from SCR_EditableEntityComponent; this subclass only answers three questions differently - what
//! the icon and name are (per-entity, via OVT_GMCampaignUIInfo), which faction owns it (from
//! Overthrow's own records rather than a faction-affiliation component the towns and towers do not
//! have), and whether the editor may move it (never).
//!
//! THE PREFAB BLOCK IS HALF OF THIS COMPONENT, AND IT FAILS SILENTLY. Every value below has to be
//! authored on the prefab, and a wrong one produces no compile error and usually no log line:
//!   m_EntityType       SYSTEM  - 48 px icon slot with a faction-tinted frame; the type Conflict's
//!                                own military bases use (ConflictBase_Base.et:36-45)
//!   m_bAutoRegister    ALWAYS  - WHEN_SPAWNED would never register a world-placed entity at all
//!   m_Flags            2052    - NON_DELETABLE (2048) | HAS_FACTION (4), on ALL FIVE prefabs
//!   m_fMaxDrawDistance 20000   - left at 0, SYSTEM's 1000 m default shrinks to a 316 m radius at
//!                                ground-level camera altitude and a strategic GM sees nothing
//!   m_vIconPos                 - metres above the entity origin, per family
//!
//! LOCAL (8) IS NOT SET ANYWHERE, AND THAT WAS MEASURED, NOT ASSUMED. SCR_EditableEntityComponent
//! .OnPostInit (:2237-2256) nulls this component and logs an ERROR when LOCAL is set on an entity
//! that HAS an RplComponent, and equally when it is absent from one that has NOT. Overthrow's three
//! TransmitterTower_01*_base.et files LOOK like they have no RplComponent - but they are same-GUID,
//! same-path DELTAS over the vanilla prefabs of those names ({68CA807F237FFB7A},
//! {01E409EF2B75692E}, {4DE187A3508D5A46}), each of which carries one. They inherit the mesh, the
//! ladder and the map descriptor from the same place. So every one of the five entities has an
//! RplComponent and none of them may be LOCAL. OVT_TEST_Init_GMIcons asserts exactly that
//! invariant, for every family, so a future prefab edit cannot re-introduce the mistake silently.
//!
//! NON_INTERACTIVE MUST NEVER BE SET - it removes hover and selection, which is the whole feature.
//------------------------------------------------------------------------------------------------
class OVT_GMEditableCampaignComponent : SCR_EditableSystemComponent
{
	//-----------------------------------------------------------------------
	// ATTRIBUTES
	//-----------------------------------------------------------------------

	[Attribute("0", UIWidgets.ComboBox, "Which kind of Overthrow campaign object this entity is", "", ParamEnumArray.FromEnum(OVT_EGMIconKind))]
	protected OVT_EGMIconKind m_eKind;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	//! MUST be a strong ref: SetInfoInstance() stores a WEAK reference and says so
	//! (SCR_EditableEntityComponent.c:178-185). Nothing else holds this object.
	protected ref OVT_GMCampaignUIInfo m_Info;

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Installs the per-entity UI info. Runs on every machine, including a dedicated server where no
	//! icon will ever be drawn - it is cheap and keeps the two sides identical.
	//! \param[in] owner Owning entity.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!owner)
			return;

		// The World Editor loads editable entities for thumbnails while a game may be running; the
		// base class bails out here for the same reason, and there are no Overthrow managers to ask.
		if (SCR_Global.IsEditMode(owner))
			return;

		// Null after super means the base class disabled this component - a LOCAL/RplComponent
		// mismatch on the prefab. There is nothing to decorate, and the ERROR is already in the log.
		if (!GetOwnerScripted())
			return;

		m_Info = new OVT_GMCampaignUIInfo();

		// CopyFrom FIRST: it assigns Icon, IconSetName, labels and budget arrays from the
		// prefab-authored info, so calling it after Configure() would throw the icon away.
		m_Info.CopyFrom(GetInfo());
		m_Info.Configure(owner, m_eKind);

		SetInfoInstance(m_Info);
	}

	//-----------------------------------------------------------------------
	// PUBLIC METHODS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Controlling faction, which is what tints the icon
	//! (SCR_EditableEntityBaseSlotUIComponent.SetFactionColor, called for entities flagged
	//! HAS_FACTION).
	//!
	//! Bases go through vanilla: OVT_BaseControllerComponent.SetControllingFaction() writes to the
	//! SCR_FactionAffiliationComponent on the same prefab, which replicates and which the base class
	//! already listens to - so a captured base retints live on every machine, for free.
	//!
	//! Towns and towers have no such component, so their faction is read from Overthrow's own
	//! records. Those records are local on every machine (towns are discovered locally and their
	//! faction is JIP-streamed; tower faction arrives with the occupying-faction JIP payload), so
	//! this needs no network of its own. It re-reads on every call, and the vanilla icon slot
	//! re-reads whenever the icon is (re)created - so a flipped town retints the next time its icon
	//! comes back into draw range or the editor is reopened, rather than instantly. See context.md.
	//! \return Controlling faction, or null when nothing is known yet.
	override Faction GetFaction()
	{
		if (m_eKind == OVT_EGMIconKind.BASE)
			return super.GetFaction();

		IEntity owner = GetOwnerScripted();
		if (!owner || !GetGame())
			return super.GetFaction();

		vector pos = owner.GetOrigin();

		if (m_eKind == OVT_EGMIconKind.TOWN)
		{
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			if (!towns)
				return null;

			OVT_TownData town = towns.GetNearestTown(pos);
			if (!town)
				return null;

			return FactionByIndex(town.faction);
		}

		if (m_eKind == OVT_EGMIconKind.RADIO_TOWER)
		{
			OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
			if (!occupying)
				return null;

			OVT_RadioTowerData tower = occupying.GetNearestRadioTower(pos);
			if (!tower)
				return null;

			return FactionByIndex(tower.faction);
		}

		return super.GetFaction();
	}

	//------------------------------------------------------------------------------------------------
	//! Always false - a deliberate, documented lie to the base class, and the mechanism that stops a
	//! Game Master from wrecking the campaign by dragging a controller entity off its town.
	//!
	//! IT IS THE MOVEMENT LOCK. Three editor systems, and only these three, gate their work on it,
	//! and each of them simply skips an entity that answers false:
	//!   transform / rotate  SCR_TransformingEditorComponent.c:88, :98, :121
	//!   re-parent           SCR_EntitiesManagerEditorComponent.c:140-141
	//!   layer moves         SCR_LayersEditorManager.c:138, :259
	//! Deletion is blocked separately and independently by the NON_DELETABLE flag
	//! (SCR_EditableEntityComponent.c:851-853, SCR_DeleteSelectedContextAction.c:64).
	//!
	//! It costs nothing else: every other replicated behaviour of this component is gated on
	//! IsServer(), not on this. It is the ONLY movement lock all three families have - none of them
	//! is LOCAL (see the class header), so none of them gets one for free.
	//! If a play-test ever finds a genuine consequence, DELETE THIS OVERRIDE - stock behaviour
	//! returns and only the movement lock is lost.
	//! \param[out] replicationID Untouched, exactly as the base class leaves it on its own LOCAL path.
	//! \return Always false.
	override bool IsReplicated(out RplId replicationID = -1)
	{
		return false;
	}

	//-----------------------------------------------------------------------
	// GETTERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return Which kind of campaign object this entity is.
	OVT_EGMIconKind GetIconKind()
	{
		return m_eKind;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The per-entity UI info, or null before OnPostInit / in the World Editor.
	OVT_GMCampaignUIInfo GetCampaignInfo()
	{
		return m_Info;
	}

	//-----------------------------------------------------------------------
	// PROTECTED METHODS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Faction from an Overthrow faction index, with the faction manager checked - unlike
	//! OVT_TownData.ControllingFactionData(), which dereferences it.
	//! \param[in] index Faction index as stored in Overthrow's records.
	//! \return The faction, or null.
	protected Faction FactionByIndex(int index)
	{
		if (index < 0)
			return null;

		FactionManager factions = GetGame().GetFactionManager();
		if (!factions)
			return null;

		return factions.GetFactionByIndex(index);
	}
}
