//------------------------------------------------------------------------------------------------
//! Browsable shop categories.
//!
//! ALL is not a real category - it is the "no filter" tab, always shown first and never returned by
//! OVT_ShopCategoryHelper.GetCategory(). OTHER is the catch-all and is always shown last.
//------------------------------------------------------------------------------------------------
enum OVT_ShopCategory
{
	ALL,
	WEAPONS,
	AMMUNITION,
	ATTACHMENTS,
	EXPLOSIVES,
	MEDICAL,
	CLOTHING,
	GEAR,
	OTHER
}

//------------------------------------------------------------------------------------------------
//! Pure mapping from arsenal catalog data onto a browsable shop category.
//!
//! THE MODE IS CHECKED BEFORE THE TYPE, AND THAT ORDERING IS THE WHOLE POINT. A rifle magazine is
//! catalogued as type RIFLE with mode AMMUNITION; a scope for that rifle can arrive as type RIFLE
//! with mode ATTACHMENT. Checking the type first files both of them under Weapons, which is exactly
//! the complaint in issue #145. Every reordering of the checks below is a regression.
//!
//! UI-free, manager-free and world-free by design: the rule lives here so the Logic tier can assert
//! it, while the economy manager owns only the id -> category cache built on top of it.
//------------------------------------------------------------------------------------------------
class OVT_ShopCategoryHelper
{
	//------------------------------------------------------------------------------------------------
	//! Maps one catalogued item onto its browse category.
	//! \param[in] type Arsenal item type from SCR_ArsenalItem.GetItemType().
	//! \param[in] mode Arsenal item mode from SCR_ArsenalItem.GetItemMode().
	//! \return The category the item should be browsed under. Never ALL.
	static OVT_ShopCategory GetCategory(SCR_EArsenalItemType type, SCR_EArsenalItemMode mode)
	{
		// --- MODE FIRST. Ammunition is typed after the weapon it feeds, so the mode is the only
		// honest signal that a row is a magazine rather than a rifle.
		if (mode == SCR_EArsenalItemMode.AMMUNITION)
			return OVT_ShopCategory.AMMUNITION;

		if (mode == SCR_EArsenalItemMode.ATTACHMENT || type == SCR_EArsenalItemType.WEAPON_ATTACHMENT)
			return OVT_ShopCategory.ATTACHMENTS;

		if (mode == SCR_EArsenalItemMode.CONSUMABLE || type == SCR_EArsenalItemType.HEAL)
			return OVT_ShopCategory.MEDICAL;

		// --- THEN TYPE.
		if (type == SCR_EArsenalItemType.LETHAL_THROWABLE
			|| type == SCR_EArsenalItemType.NON_LETHAL_THROWABLE
			|| type == SCR_EArsenalItemType.EXPLOSIVES
			|| type == SCR_EArsenalItemType.MORTARS)
		{
			return OVT_ShopCategory.EXPLOSIVES;
		}

		if (type == SCR_EArsenalItemType.RIFLE
			|| type == SCR_EArsenalItemType.PISTOL
			|| type == SCR_EArsenalItemType.MACHINE_GUN
			|| type == SCR_EArsenalItemType.SNIPER_RIFLE
			|| type == SCR_EArsenalItemType.ROCKET_LAUNCHER)
		{
			return OVT_ShopCategory.WEAPONS;
		}

		if (type == SCR_EArsenalItemType.HEADWEAR
			|| type == SCR_EArsenalItemType.TORSO
			|| type == SCR_EArsenalItemType.VEST_AND_WAIST
			|| type == SCR_EArsenalItemType.LEGS
			|| type == SCR_EArsenalItemType.FOOTWEAR
			|| type == SCR_EArsenalItemType.HANDWEAR)
		{
			return OVT_ShopCategory.CLOTHING;
		}

		if (type == SCR_EArsenalItemType.BACKPACK
			|| type == SCR_EArsenalItemType.RADIO_BACKPACK
			|| type == SCR_EArsenalItemType.EQUIPMENT)
		{
			return OVT_ShopCategory.GEAR;
		}

		// VEHICLE, HELICOPTER and anything a future engine version adds. Vehicles deliberately do not
		// earn a tab: at a vehicle shop every row lands in one category, so the tab row stays hidden
		// and the vehicle browser looks exactly as it did before this feature.
		return OVT_ShopCategory.OTHER;
	}

	//------------------------------------------------------------------------------------------------
	//! Category for an item with no arsenal data at all (not registered in any catalog).
	//! \return OTHER, always. Exists so callers name the intent rather than hardcoding the fallback.
	static OVT_ShopCategory GetCategoryForUncatalogued()
	{
		return OVT_ShopCategory.OTHER;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key for a category's tab label.
	//! \param[in] category The category to label.
	//! \return An #OVT- localization key. Never empty.
	static string GetLabelKey(OVT_ShopCategory category)
	{
		if (category == OVT_ShopCategory.ALL)
			return "#OVT-ShopCategory_All";

		if (category == OVT_ShopCategory.WEAPONS)
			return "#OVT-ShopCategory_Weapons";

		if (category == OVT_ShopCategory.AMMUNITION)
			return "#OVT-ShopCategory_Ammunition";

		if (category == OVT_ShopCategory.ATTACHMENTS)
			return "#OVT-ShopCategory_Attachments";

		if (category == OVT_ShopCategory.EXPLOSIVES)
			return "#OVT-ShopCategory_Explosives";

		if (category == OVT_ShopCategory.MEDICAL)
			return "#OVT-ShopCategory_Medical";

		if (category == OVT_ShopCategory.CLOTHING)
			return "#OVT-ShopCategory_Clothing";

		if (category == OVT_ShopCategory.GEAR)
			return "#OVT-ShopCategory_Gear";

		return "#OVT-ShopCategory_Other";
	}

	//------------------------------------------------------------------------------------------------
	//! The order categories are presented in, ALL first and OTHER last.
	//! \param[out] categories Receives every category exactly once, in tab order. Cleared first.
	static void GetDisplayOrder(out array<OVT_ShopCategory> categories)
	{
		if (!categories)
			return;

		categories.Clear();

		categories.Insert(OVT_ShopCategory.ALL);
		categories.Insert(OVT_ShopCategory.WEAPONS);
		categories.Insert(OVT_ShopCategory.AMMUNITION);
		categories.Insert(OVT_ShopCategory.ATTACHMENTS);
		categories.Insert(OVT_ShopCategory.EXPLOSIVES);
		categories.Insert(OVT_ShopCategory.MEDICAL);
		categories.Insert(OVT_ShopCategory.CLOTHING);
		categories.Insert(OVT_ShopCategory.GEAR);
		categories.Insert(OVT_ShopCategory.OTHER);
	}
}
