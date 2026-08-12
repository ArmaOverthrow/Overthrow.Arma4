//! Shared panel plumbing for the two location types that render OVT_MapInfoShop.layout.
//!
//! ================================================================================================
//! LAYOUT <-> CODE NAME CONTRACT - UI/Layouts/Map/LocationTypes/OVT_MapInfoShop.layout
//! ================================================================================================
//!   "Rows"           VerticalLayoutWidgetClass  Plain explanatory lines. This is the name the
//!                                               inherited AddInfoRow / AddInfoIconRow write into,
//!                                               and it is deliberately the SAME name the shared
//!                                               OVT_MapInfoRows.layout uses, so both helpers work
//!                                               here unchanged.
//!   "Badge"          OverlayWidgetClass         Shop-wide remoteness badge. Authored "Is Visible" 0
//!                                               and shown only for a positive band.
//!   "BadgeIcon"      ImageWidgetClass           The badge's caret glyph.
//!   "BadgeText"      TextWidgetClass            The badge's sentence.
//!   "SectionHeader"  TextWidgetClass            Heading above the item rows. Authored
//!                                               "Is Visible" 0 and shown only when rows follow.
//!   "ScarcityRows"   VerticalLayoutWidgetClass  Per-item caret rows.
//!
//! Renaming any of the six in the layout without changing this file produces a panel that silently
//! renders less than it should - FindAnyWidget returning null is a no-op the compiler cannot see,
//! and it is exactly how map/core's IconLayout (D1) and CloseButton (D2) shipped dead. Q-8 requires
//! this list to be audited name-by-name against the layout on every change.
//! ================================================================================================
//!
//! WHY AN INTERMEDIATE CLASS. OVT_MapLocationShop and OVT_MapLocationGunDealer show COMPLETELY
//! DIFFERENT CONTENT (top-N carets versus the four weapons this dealer rolled - section 4.6b) but
//! paint it into the SAME layout with the same badge and the same caret glyphs. Putting the shared
//! painting here rather than duplicating it keeps the widget-name contract above in ONE place, which
//! is the entire point of the Q-8 audit.
//!
//! NEVER RENDERS A NUMBER. No price, no currency amount, no percentage, no stock count reaches a
//! widget from anywhere in this hierarchy. The map indicator is a teaser; the shop menu remains the
//! only place with real figures.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationShopBase : OVT_MapLocationType
{
	//! Widget names this class looks up. Constants rather than literals so the contract above and the
	//! code cannot drift apart silently.
	protected static const string WIDGET_ROWS = "Rows";
	protected static const string WIDGET_BADGE = "Badge";
	protected static const string WIDGET_BADGE_ICON = "BadgeIcon";
	protected static const string WIDGET_BADGE_TEXT = "BadgeText";
	protected static const string WIDGET_SECTION_HEADER = "SectionHeader";
	protected static const string WIDGET_SCARCITY_ROWS = "ScarcityRows";

	//! How many rows each direction contributes at most (section 4.6). Six rows is also the cap that
	//! keeps N8's per-item prefab load acceptable.
	protected static const int MAX_ROWS_PER_DIRECTION = 3;

	//! The calculator. Held per type instance so its display-name and arsenal-type caches survive
	//! across map opens - the type objects come from Configs/Map/OverthrowMap.conf and are built
	//! once, even though Init() re-runs on every open.
	protected ref OVT_MapShopPriceIndicator m_PriceIndicator = new OVT_MapShopPriceIndicator();

	//------------------------------------------------------------------------------------------------
	//! Resolves the shop component behind a map record, tolerating every kind of partial replication.
	//! \param[in] location The record being described.
	//! \return The component, or null when the entity has not streamed in or carries no shop.
	protected OVT_ShopComponent GetShopForLocation(OVT_MapLocationData location)
	{
		if (!location)
			return null;

		IEntity entity = location.m_pEntity;

		if (!entity)
			entity = location.GetEntity();

		if (!entity)
			return null;

		return OVT_ShopComponent.Cast(entity.FindComponent(OVT_ShopComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the shop-wide remoteness badge, or leaves it hidden.
	//!
	//! Hidden covers three distinct cases and must not distinguish them on screen: within ~500 m of a
	//! port, no ports registered at all (N7 - DistanceToNearestPort returns -1), and a vehicle shop
	//! (whose prices carry no distance term at all).
	//! \param[in] root The instantiated OVT_MapInfoShop.layout.
	//! \param[in] level Band from OVT_MapShopPriceIndicator.GetRemotenessLevel.
	protected void ShowRemotenessBadge(Widget root, OVT_MapPriceLevel level)
	{
		if (!root)
			return;

		Widget badge = root.FindAnyWidget(WIDGET_BADGE);
		if (!badge)
			return;

		if (OVT_MapShopPriceBands.IsNeutral(level))
		{
			badge.SetVisible(false);
			return;
		}

		string quad = OVT_MapShopPriceBands.GetCaretIcon(level);
		if (quad.IsEmpty())
		{
			badge.SetVisible(false);
			return;
		}

		ImageWidget icon = ImageWidget.Cast(root.FindAnyWidget(WIDGET_BADGE_ICON));
		if (icon)
		{
			icon.LoadImageFromSet(0, OVT_MapShopPriceBands.PRICE_ICON_IMAGESET, quad);
			icon.SetVisible(true);
		}

		TextWidget text = TextWidget.Cast(root.FindAnyWidget(WIDGET_BADGE_TEXT));
		if (text)
			text.SetText("#OVT-Map_Shop_Remote");

		badge.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the heading above the item rows.
	//! Never called when no rows follow - an empty section reads as a bug (section 4.6).
	//! \param[in] root The instantiated OVT_MapInfoShop.layout.
	//! \param[in] headerKey Localization key for the heading.
	protected void ShowSectionHeader(Widget root, string headerKey)
	{
		if (!root || headerKey.IsEmpty())
			return;

		TextWidget header = TextWidget.Cast(root.FindAnyWidget(WIDGET_SECTION_HEADER));
		if (!header)
			return;

		header.SetText(headerKey);
		header.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one item row carrying a caret glyph.
	//!
	//! F-6 (up versus down must not depend on colour): the glyph carries the direction as its SHAPE and
	//! the magnitude as one, two or three chevrons, and the row layout draws it in the same white as
	//! every other row icon - so colour is not load-bearing. The shop rows previously repeated the
	//! direction in words ("Dearer"/"Cheaper"); that was dropped 2026-08-10 by user directive, leaving
	//! shape as the sole direction cue. Gun-dealer rows still pass a label, but a weapon-kind one.
	//!
	//! The caret uses the shared OVT_MapInfoRow layout's own RowIcon rather than a bespoke
	//! CaretIcon/CaretText pair: the plan named those two widgets before Phase 5 built RowIcon /
	//! RowLabel / RowValue, and a second parallel row widget would duplicate the Q-8 surface for no
	//! gain. Icon sizing belongs to that shared row and is deliberately not overridden here.
	//! \param[in] rows The ScarcityRows container.
	//! \param[in] level Band for this item. NEUTRAL draws no glyph, only the text.
	//! \param[in] labelKey Left-hand caption, or "" to hide the label slot entirely (the shop rows).
	//! \param[in] displayName Already-localized item name.
	protected void AddCaretRow(Widget rows, OVT_MapPriceLevel level, string labelKey, string displayName)
	{
		if (!rows)
			return;

		string quad = OVT_MapShopPriceBands.GetCaretIcon(level);

		if (quad.IsEmpty())
		{
			AddInfoRow(rows, labelKey, displayName);
			return;
		}

		AddInfoIconRow(rows, labelKey, displayName, OVT_MapShopPriceBands.PRICE_ICON_IMAGESET, quad);
	}
}
