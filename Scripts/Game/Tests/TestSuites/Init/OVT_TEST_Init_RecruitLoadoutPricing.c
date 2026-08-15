//------------------------------------------------------------------------------------------------
//! Pricing a saved loadout walks the WHOLE item tree, and one unregistered item refuses the lot.
//!
//! WHY THIS TIER. The arithmetic that turns a subtotal into a price is world-free and is pinned in
//! Tier A. THIS half is not: it needs a live economy to ask what an item is worth, and the two things
//! that make it dangerous are both economy behaviours rather than maths.
//!
//!   - GetInventoryId() is a bare map index. An unregistered prefab - looted gear that never entered
//!     the resource database - resolves to id 0, which is SOME OTHER ITEM'S PRICE.
//!   - GetPrice() cannot answer "unknown": it returns 500 for any id it has not heard of.
//!
//! So an unpriceable item cannot be detected by a zero check or a sanity range. Registration is the
//! only real signal, and the second half of this case is what proves the code asks that question
//! before every lookup rather than trusting the price it gets back.
//!
//! THE FIRST HALF IS ABOUT COMPLETENESS, and it is a money question too, in the opposite direction: a
//! walk that misses attachments or the contents of a rucksack charges the player for a fraction of
//! what the recruit will actually arrive carrying, on a route whose whole reason for existing is that
//! free gear used to be an exploit here. The probe loadout is built to have one of each shape - a
//! top-level item with an attachment, and a container with something inside it - so a walk that skips
//! either reports 3 where 4 is demanded.
//!
//! QUICK SLOTS ARE ASSERTED ABSENT, which is the subtle one. A quick-slot entry NAMES an item that is
//! already somewhere else in the tree; the apply spawns nothing for it (and returns early for an AI
//! character in any case), so pricing one would charge twice for the same rifle. The probe loadout
//! carries a quick-slot entry naming the same probe resource, and the expected count is unchanged
//! by it.
//!
//! SYNTHETIC RESOURCES, NOT REAL PREFABS. Two names that no prefab has, one registered by the case
//! itself and one deliberately never registered. Nothing loads them - the pricing walk only ever
//! looks them up in the resource map - so no asset is touched, and registering one appends a single
//! entry to a database that nothing iterates. The same convention as the economy price case, one
//! level up: that one uses synthetic IDs, this one needs a synthetic NAME because registration is
//! the thing under test.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 9, by deliberate fault + compile-check; running the suite is
//! the orchestrator's job, not this file's):
//!   OVT_RecruitLoadoutPricing.PriceItem's recursion into m_aChildItems was deleted - the "we forgot
//!   about rucksack contents" defect - and the tree recompiled CLEAN, which is the point: a walk that
//!   stops early is not a syntax error. The item-count assertion below then reads 3 where it demands
//!   4 and the case fails on "priced 3 resources, expected 4". Fault reverted, tree recompiled clean.
//!   The second half fails independently: making AddResource() skip its IsRegisteredResource() gate
//!   makes the unregistered item price at some other item's cost and the refusal assertion goes red.
//!   No maxAttempts: no polling, no timing, no async step anywhere in the case.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_RecruitLoadoutPricing : SCR_AutotestCaseBase
{
	//! A resource name no prefab has. Registered by this case so it has a price of its own.
	static const string PROBE_RESOURCE = "OVT_TEST_RecruitPurchaseProbe.et";

	//! A resource name no prefab has and that is never registered - the unpriceable item.
	static const string UNREGISTERED_RESOURCE = "OVT_TEST_RecruitPurchaseNeverRegistered.et";

	//! Base price seeded for the probe resource.
	static const int PROBE_PRICE = 1000;

	//! Owner of the probe loadout. Never a real player id.
	static const string TEST_PLAYER_ID = "OVT_TEST_RECRUIT_PURCHASE";

	//! How many resources the probe loadout should price: two top-level items, one attachment on the
	//! first and one item nested inside the second. The quick-slot entry is NOT one of them.
	static const int EXPECTED_PRICED_RESOURCES = 4;

	//! How many entries the probe loadout's own item array has.
	static const int EXPECTED_TOP_LEVEL = 2;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The walk refuses to run off the authority, and correctly so - a client's copy of a loadout
		// has no items. Named explicitly so a non-authority world reports as itself instead of as an
		// empty price.
		if (!Replication.IsServer())
		{
			SetFailure("This world is not the authority, so the pricing walk would refuse to run and every assertion below would be vacuous");
			return true;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (!economy)
		{
			SetFailure("OVT_Global.GetEconomy() is null - nothing can be priced");
			return true;
		}

		// The unregistered half is only meaningful if the name really is unknown. Asserted BEFORE
		// anything is registered, so a name collision with a real prefab reports as itself.
		if (economy.IsRegisteredResource(UNREGISTERED_RESOURCE))
		{
			SetFailure("%1 is already a registered resource, so the unpriceable half of this case would be vacuous", UNREGISTERED_RESOURCE);
			return true;
		}

		economy.RegisterResource(PROBE_RESOURCE);

		if (!economy.IsRegisteredResource(PROBE_RESOURCE))
		{
			SetFailure("RegisterResource() did not register %1 - the priceable half of this case cannot be measured", PROBE_RESOURCE);
			return true;
		}

		int probeId = economy.GetInventoryId(PROBE_RESOURCE);
		economy.SetPrice(probeId, PROBE_PRICE);

		// At "0 0 0" the town-stock and port-distance terms are skipped entirely, so this is the plain
		// base price plus the shop margin, and it is the same number for all four resources.
		int perItem = economy.GetBuyPrice(probeId, "0 0 0", -1);
		if (perItem <= 0)
		{
			SetFailure("The probe resource priced at %1 - there is nothing for the walk to add up", perItem.ToString());
			return true;
		}

		OVT_PlayerLoadout loadout = BuildProbeLoadout();

		OVT_RecruitLoadoutPrice price = OVT_RecruitLoadoutPricing.Price(loadout, "0 0 0", -1);
		if (!price)
		{
			SetFailure("OVT_RecruitLoadoutPricing.Price() returned null - it must always return a result");
			return true;
		}

		if (!price.IsPriceable())
		{
			SetFailure("A loadout of registered resources was reported unpriceable (blamed on '%1') - every item in it has a catalog price",
				price.m_sUnpriceableResource);
			return true;
		}

		if (price.m_iTopLevelCount != loadout.GetItemCount())
		{
			SetFailure("The price reports %1 top-level items but the loadout records %2 - the number the apply is graded against does not match the record",
				price.m_iTopLevelCount.ToString(), loadout.GetItemCount().ToString());
			return true;
		}

		if (price.m_iTopLevelCount != EXPECTED_TOP_LEVEL)
		{
			SetFailure("The probe loadout reports %1 top-level items, expected %2 - the fixture is not what the rest of this case assumes",
				price.m_iTopLevelCount.ToString(), EXPECTED_TOP_LEVEL.ToString());
			return true;
		}

		// THE COMPLETENESS ASSERTION. 4, not 2: an attachment and a nested item are separate prefabs
		// that the spawning apply really does spawn, so they are separate things to pay for. And not
		// 5: the quick-slot entry names an item already counted.
		if (price.m_iItemCount != EXPECTED_PRICED_RESOURCES)
		{
			SetFailure("Pricing walked %1 resources, expected %2 - a top-level item, its attachment, a container and the item inside it, with the quick-slot entry NOT priced",
				price.m_iItemCount.ToString(), EXPECTED_PRICED_RESOURCES.ToString());
			return true;
		}

		if (price.m_iSubtotal <= 0)
		{
			SetFailure("A loadout of four priced resources has a subtotal of %1", price.m_iSubtotal.ToString());
			return true;
		}

		int expectedSubtotal = perItem * EXPECTED_PRICED_RESOURCES;
		if (price.m_iSubtotal != expectedSubtotal)
		{
			// SetFailure takes at most three substitutions, so the arithmetic is spelled out in one.
			SetFailure("%1 - the walk is not using the shop buy price, or is not visiting each resource exactly once",
				string.Format("Subtotal is %1, expected %2 (%3 resources at the local buy price %4)",
					price.m_iSubtotal, expectedSubtotal, EXPECTED_PRICED_RESOURCES, perItem));
			return true;
		}

		// SECOND HALF: one item with no catalog price refuses the whole loadout, by name.
		OVT_LoadoutItem unknown = new OVT_LoadoutItem();
		unknown.m_sResourceName = UNREGISTERED_RESOURCE;
		loadout.AddItem(unknown);

		OVT_RecruitLoadoutPrice refused = OVT_RecruitLoadoutPricing.Price(loadout, "0 0 0", -1);
		if (!refused)
		{
			SetFailure("OVT_RecruitLoadoutPricing.Price() returned null for the unpriceable loadout");
			return true;
		}

		if (refused.IsPriceable())
		{
			SetFailure("A loadout containing an unregistered resource priced cleanly at %1 - an unregistered prefab resolves to another item's price, so this would charge a made-up number and hand out gear that was never paid for",
				refused.m_iSubtotal.ToString());
			return true;
		}

		if (refused.m_sUnpriceableResource != UNREGISTERED_RESOURCE)
		{
			SetFailure("The refusal names '%1', expected '%2' - the player is told which item to remove from the loadout, so the wrong name is worse than none",
				refused.m_sUnpriceableResource, UNREGISTERED_RESOURCE);
			return true;
		}

		PrintFormat("Recruit loadout pricing: %1 resources, subtotal %2 at %3 each; an unregistered item refuses the loadout by name",
			price.m_iItemCount.ToString(), price.m_iSubtotal.ToString(), perItem.ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One loadout with every shape the walk has to handle.
	//!
	//! Built by hand rather than captured off a character: this case is about the WALK, and a captured
	//! loadout's shape would depend on what the civilian loadout config happened to dress someone in.
	//! Every field the walk reads is set explicitly - the constructor initialises the arrays, but no
	//! attribute defaults apply to a hand-built object.
	//! \return A loadout with two top-level items, one attachment, one nested item and one quick slot.
	protected OVT_PlayerLoadout BuildProbeLoadout()
	{
		OVT_PlayerLoadout loadout = new OVT_PlayerLoadout();
		loadout.Initialize("RecruitPurchaseProbe", TEST_PLAYER_ID);

		// A weapon in hands, with an attachment. Attachments are separate prefabs and ARE spawned.
		OVT_LoadoutItem weapon = new OVT_LoadoutItem();
		weapon.m_sResourceName = PROBE_RESOURCE;
		weapon.m_bIsEquipped = true;
		weapon.AddAttachment(PROBE_RESOURCE);
		loadout.AddItem(weapon);

		// A container with something inside it. The spawning apply recurses into these.
		OVT_LoadoutItem container = new OVT_LoadoutItem();
		container.m_sResourceName = PROBE_RESOURCE;
		container.m_bIsEquipped = false;

		OVT_LoadoutItem nested = new OVT_LoadoutItem();
		nested.m_sResourceName = PROBE_RESOURCE;
		nested.m_bIsEquipped = false;
		container.AddChildItem(nested);

		loadout.AddItem(container);

		// A quick slot naming an item that is already in the tree. Must NOT be priced.
		array<string> quickSlots = {};
		quickSlots.Insert(PROBE_RESOURCE);
		loadout.SetQuickSlotItems(quickSlots);

		return loadout;
	}
}
