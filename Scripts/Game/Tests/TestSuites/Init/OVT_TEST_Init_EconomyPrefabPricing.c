//------------------------------------------------------------------------------------------------
//! Every prefab a player can hold has a price: registered, inherited, or classified.
//!
//! The economy's resource database is built from the factions' ITEM entity catalogues and nothing
//! else, but a saved loadout is whatever the player was wearing - looted civilian clothes (vanilla's
//! *_Dirty variants live only on character prefabs), weapon presets, modded gear. Before this seam the
//! equipped-recruit quote refused the whole loadout on the first such item ("this item has no price"),
//! which players read, correctly, as a bug: the dirty trousers ARE the catalogue trousers with a
//! different texture.
//!
//! Three routes, each pinned here against a real vanilla prefab:
//!   1. registered            - the catalogue prefab prices as itself (control).
//!   2. inherited             - Pants_M70_Dirty is in no catalogue; its prefab parent Pants_M70 is
//!                              (FIA). ResolvePricingResource must answer the parent and the buy price
//!                              must equal the parent's, at the same position and player.
//!   3. classified            - Rifle_Base is in no catalogue and inherits only Weapon_Base; it carries
//!                              a WeaponComponent with WeaponType Rifle, so it classifies as RIFLE and
//!                              prices off the RIFLE rule in itemPrices.conf. No registered ancestor
//!                              (asserted), a positive base price, and margin applied on top.
//!   4. nothing               - a name no prefab has is still refused (-1), so the refusal path that
//!                              OVT_TEST_Init_RecruitLoadoutPricing pins is intact.
//! Then the whole walk: a loadout wearing the dirty trousers must be priceable end to end.
//!
//! NOTHING IS REGISTERED BY THIS CASE. The point of the seam is that it never appends a resource id
//! (the ids are the wire format), and the case asserts the count is unchanged afterwards.
//!
//! PROVEN ABLE TO FAIL (by deliberate fault + compile-check): ResolvePricingResource returning empty
//! for every unregistered prefab fails route 2 on "priced as itself"; OVT_PrefabItemClassifier
//! returning false for weapons fails route 3 on "no base price"; AddResource gating on
//! IsRegisteredResource again fails the walk on "refused the dirty trousers".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_EconomyPrefabPricing : SCR_AutotestCaseBase
{
	//! Catalogued (FIA) - the control and the parent of the dirty variant.
	static const ResourceName CLEAN_PANTS = "{06BC3F18B47799AE}Prefabs/Characters/Uniforms/Pants_M70.et";

	//! Worn by FIA character prefabs, in no catalogue, inherits CLEAN_PANTS.
	static const ResourceName DIRTY_PANTS = "{4DC4FD2F6D353B12}Prefabs/Characters/Uniforms/Pants_M70_Dirty.et";

	//! In no catalogue, no registered ancestor, WeaponComponent WeaponType Rifle.
	static const ResourceName BARE_RIFLE = "{911D6C8DC7BA2D63}Prefabs/Weapons/Core/Rifle_Base.et";

	//! A name no prefab has.
	static const string NOT_A_PREFAB = "OVT_TEST_EconomyPrefabPricingNeverExists.et";

	static const string TEST_PLAYER_ID = "OVT_TEST_ECONOMY_PREFAB_PRICING";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The pricing walk refuses to run off the authority; named so a client world reports as itself.
		if (!Replication.IsServer())
		{
			SetFailure("This world is not the authority, so the pricing walk would refuse to run and the end-to-end half would be vacuous");
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null - nothing can be priced");
			return true;
		}

		if (!economy.IsRegisteredResource(CLEAN_PANTS))
		{
			SetFailure("%1 is not a registered resource - the FIA catalogue no longer lists it, so the inherited route has no parent to resolve to", CLEAN_PANTS);
			return true;
		}

		if (economy.IsRegisteredResource(DIRTY_PANTS))
		{
			SetFailure("%1 IS registered - vanilla catalogued the dirty variant, so the inherited route is vacuous here; pick another uncatalogued child", DIRTY_PANTS);
			return true;
		}

		if (economy.IsRegisteredResource(BARE_RIFLE))
		{
			SetFailure("%1 IS registered - the classified route is vacuous here", BARE_RIFLE);
			return true;
		}

		int resourceCountBefore = CountRegistered(economy);

		// --- 1. registered prices as itself
		if (economy.ResolvePricingResource(CLEAN_PANTS) != CLEAN_PANTS)
		{
			SetFailure("ResolvePricingResource(%1) did not answer the registered prefab itself", CLEAN_PANTS);
			return true;
		}

		int cleanId = economy.GetInventoryId(CLEAN_PANTS);
		int cleanPrice = economy.GetBuyPrice(cleanId, "0 0 0", -1);
		if (economy.GetBuyPriceForPrefab(CLEAN_PANTS, "0 0 0", -1) != cleanPrice)
		{
			SetFailure("GetBuyPriceForPrefab(%1) = %2, GetBuyPrice(id) = %3 - a registered prefab must price exactly as before", CLEAN_PANTS, economy.GetBuyPriceForPrefab(CLEAN_PANTS, "0 0 0", -1).ToString(), cleanPrice.ToString());
			return true;
		}

		// --- 2. inherited: the dirty variant prices as its catalogued parent
		ResourceName resolved = economy.ResolvePricingResource(DIRTY_PANTS);
		if (resolved != CLEAN_PANTS)
		{
			SetFailure("ResolvePricingResource(%1) = '%2', expected the prefab parent %3", DIRTY_PANTS, resolved, CLEAN_PANTS);
			return true;
		}

		int dirtyPrice = economy.GetBuyPriceForPrefab(DIRTY_PANTS, "0 0 0", -1);
		if (dirtyPrice != cleanPrice)
		{
			SetFailure("the dirty trousers priced at %1, the clean ones at %2 - an inherited prefab must cost what its catalogued parent costs", dirtyPrice.ToString(), cleanPrice.ToString());
			return true;
		}

		// --- 3. classified: no registered ancestor, priced off its WeaponComponent
		if (!economy.ResolvePricingResource(BARE_RIFLE).IsEmpty())
		{
			SetFailure("%1 resolved to registered ancestor '%2' - the classified route is not what priced it; pick a prefab with no catalogued ancestor", BARE_RIFLE, economy.ResolvePricingResource(BARE_RIFLE));
			return true;
		}

		int rifleBase = economy.GetFallbackBasePrice(BARE_RIFLE);
		if (rifleBase <= 0)
		{
			SetFailure("GetFallbackBasePrice(%1) = %2 - a prefab with a WeaponComponent (WeaponType Rifle) must classify as RIFLE and price off the RIFLE rule", BARE_RIFLE, rifleBase.ToString());
			return true;
		}

		int riflePrice = economy.GetBuyPriceForPrefab(BARE_RIFLE, "0 0 0", -1);
		if (riflePrice <= rifleBase)
		{
			SetFailure("GetBuyPriceForPrefab(%1) = %2 against base %3 - the shop margin must apply to a classified price too", BARE_RIFLE, riflePrice.ToString(), rifleBase.ToString());
			return true;
		}

		// --- 4. nothing: a name no prefab has is still refused
		if (!economy.ResolvePricingResource(NOT_A_PREFAB).IsEmpty() || economy.GetBuyPriceForPrefab(NOT_A_PREFAB, "0 0 0", -1) != -1)
		{
			SetFailure("%1 priced at %2 - a name that is no prefab must stay unpriceable", NOT_A_PREFAB, economy.GetBuyPriceForPrefab(NOT_A_PREFAB, "0 0 0", -1).ToString());
			return true;
		}

		// --- the seam never registers
		int resourceCountAfter = CountRegistered(economy);
		if (resourceCountAfter != resourceCountBefore)
		{
			SetFailure("the resource database grew from %1 to %2 - pricing an unregistered prefab must never append a resource id (the ids are the wire format)", resourceCountBefore.ToString(), resourceCountAfter.ToString());
			return true;
		}

		// --- the whole walk: a loadout wearing the dirty trousers is priceable
		OVT_PlayerLoadout loadout = new OVT_PlayerLoadout();
		loadout.m_sLoadoutName = "OVT_TEST_DirtyTrousers";
		loadout.m_sPlayerId = TEST_PLAYER_ID;
		OVT_LoadoutItem pants = new OVT_LoadoutItem();
		pants.m_sResourceName = DIRTY_PANTS;
		loadout.m_aItems.Insert(pants);

		OVT_RecruitLoadoutPrice price = OVT_RecruitLoadoutPricing.Price(loadout, "0 0 0", -1);
		if (!price || !price.IsPriceable())
		{
			string blamed = "";
			if (price) blamed = price.m_sUnpriceableResource;
			SetFailure("the loadout walk refused the dirty trousers (blamed '%1') - the equipped-recruit quote must price inherited gear", blamed);
			return true;
		}

		if (price.m_iSubtotal != cleanPrice || price.m_iItemCount != 1)
		{
			SetFailure("the loadout walk priced the dirty trousers at %1 over %2 items, expected %3 over 1", price.m_iSubtotal.ToString(), price.m_iItemCount.ToString(), cleanPrice.ToString());
			return true;
		}

		Print("Economy prefab pricing: registered, inherited and classified prefabs all price, nothing unknown does, and the resource database is untouched");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Probe the database size without a getter: ids are dense from 0, so the first invalid id is the count.
	protected int CountRegistered(OVT_EconomyManagerComponent economy)
	{
		int count = 0;
		while (economy.IsValidResourceId(count))
			count++;
		return count;
	}
}
