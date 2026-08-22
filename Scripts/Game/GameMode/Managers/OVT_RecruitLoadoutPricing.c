//------------------------------------------------------------------------------------------------
//! What a saved loadout is worth, item by item, at the shop nearest a given position.
//!
//! Returned by OVT_RecruitLoadoutPricing.Price(). Three numbers and one name, and the name is the
//! important one: an empty m_sUnpriceableResource means every resource in the record had a catalog
//! price, and a non-empty one means the whole loadout is REFUSED and this is the item to blame.
//!
//!   m_bRan        - the walk completed. False means it never started (no server, no loadout, no
//!                   economy) or it stopped on an unpriceable item, and the other numbers are partial.
//!   m_iSubtotal   - the sum of every priced resource at LOCAL BUY price, before the fee multiplier.
//!   m_iItemCount  - how many resources went into that sum: top-level items, their attachments, and
//!                   everything nested inside containers, recursively. This is "how many things you
//!                   are paying for" and it is what the quote shows the player.
//!   m_iTopLevelCount - how many entries the record's own item array has. A DIFFERENT number, and
//!                   deliberately kept apart: it is the denominator the apply reports success against
//!                   (OVT_LoadoutManagerComponent counts top-level items only), so it is what grades
//!                   the outcome, while m_iItemCount is what makes the price.
//------------------------------------------------------------------------------------------------
class OVT_RecruitLoadoutPrice : Managed
{
	//! Whether the walk completed with every resource priced.
	bool m_bRan;

	//! Sum of every priced resource at local buy price, before the fee multiplier.
	int m_iSubtotal;

	//! How many resources contributed to the subtotal, nesting included.
	int m_iItemCount;

	//! How many entries the loadout's own item array has.
	int m_iTopLevelCount;

	//! The first resource with no catalog price, or empty when everything priced.
	string m_sUnpriceableResource;

	//! One line per distinct resource, aggregated across every occurrence in the loadout - feeds the
	//! recruitment-tent warehouse discount (implementation.md §3.8). m_iSubtotal keeps its present
	//! meaning (everything, uncovered); this is additive.
	ref array<ref OVT_ItemSourcingLine> m_aManifest;

	//------------------------------------------------------------------------------------------------
	//! Whether this loadout can be sold at all.
	//! \return True when the walk completed and named no unpriceable resource.
	bool IsPriceable()
	{
		if (!m_bRan) return false;

		return m_sUnpriceableResource.IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! Prices a saved loadout for the equipped-recruit purchase. SERVER ONLY.
//!
//! THIS IS THE ONLY PLACE IN THE PHASE THAT LOOKS UP A PRICE. The quote the player is shown and the
//! amount the server takes are both built from this result by OVT_RecruitPurchaseRules.TotalPrice(),
//! on the same machine, so the preview and the charge cannot drift (decisions D18/D19, quality Q12).
//!
//! IT MUST RUN ON THE SERVER, AND NOT ONLY FOR AUTHORITY REASONS: a client physically cannot do this.
//! Loadout JIP replicates metadata only - name, owner, description, flags, timestamps - and every
//! client's copy of a loadout has an EMPTY item array (OVT_LoadoutManagerComponent.c:40, RplSave
//! :2153-2180, RplLoad :2248). A client-side price would silently be zero.
//!
//! ! REGISTRATION IS CHECKED BEFORE EVERY LOOKUP, AND THAT IS THE WHOLE SAFETY ARGUMENT. Two engine
//! behaviours make a missing check catastrophic rather than merely wrong:
//!   - GetInventoryId() is a bare map index. An unregistered prefab - looted gear that never entered
//!     the resource database - resolves to id 0, which is SOME OTHER ITEM'S PRICE, and its own doc
//!     comment says so (OVT_EconomyManagerComponent.c:1679-1691).
//!   - GetPrice() cannot answer "unknown": it returns 500 for any id it has never heard of (:209-213).
//! So neither a zero check nor a sanity range can detect an unpriceable item. Registration is the only
//! real signal, one unregistered resource makes the WHOLE loadout unpriceable, and the refusal names
//! the item so the player can re-save the loadout without it (decision D20).
//!
//! WHAT IS WALKED, AND WHAT IS NOT. Every top-level item, every one of its m_aAttachments (separate
//! prefabs that ARE spawned - OVT_LoadoutManagerComponent.ApplyWeaponAttachments :2085), and every
//! m_aChildItems entry, recursively, because the spawning apply recurses into containers too
//! (ApplyNestedItemsSpawn :1603). m_aQuickSlotItems is NEVER priced: those entries name items that are
//! already somewhere else in the tree, ApplyQuickSlots spawns nothing, and it returns early for an AI
//! character anyway (:1933-1935) - pricing them would charge twice for one rifle.
//------------------------------------------------------------------------------------------------
class OVT_RecruitLoadoutPricing : Managed
{
	//! The economy this run is asking. Resolved once so a deep recursion is not a chain of lookups.
	protected OVT_EconomyManagerComponent m_Economy;

	//! Where the buying happens - the tent. Feeds the town-stock term of the shop price.
	protected vector m_vPosition;

	//! Who is buying. Feeds their own price multiplier perk, so the quote is personal.
	protected int m_iPlayerId;

	//! The tally handed back to the caller.
	protected ref OVT_RecruitLoadoutPrice m_Result;

	//------------------------------------------------------------------------------------------------
	//! Price a loadout. THE ONLY ENTRY POINT.
	//!
	//! Static so callers cannot hold a half-finished walk, and internally an instance so the run's
	//! state does not have to be threaded through the recursion.
	//! \param[in] loadout The saved loadout, with its items (server-side copy).
	//! \param[in] pos Where the purchase is happening - the tent's position.
	//! \param[in] playerId Runtime id of the buying player, for their price multiplier.
	//! \return What it costs. Never null; check IsPriceable() before using the subtotal.
	static OVT_RecruitLoadoutPrice Price(OVT_PlayerLoadout loadout, vector pos, int playerId)
	{
		OVT_RecruitLoadoutPricing run = new OVT_RecruitLoadoutPricing();

		return run.Run(loadout, pos, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The walk itself. See the class header.
	//! \param[in] loadout The saved loadout.
	//! \param[in] pos Where the purchase is happening.
	//! \param[in] playerId Runtime id of the buying player.
	//! \return What it costs.
	protected OVT_RecruitLoadoutPrice Run(OVT_PlayerLoadout loadout, vector pos, int playerId)
	{
		m_Result = new OVT_RecruitLoadoutPrice();
		m_Result.m_aManifest = new array<ref OVT_ItemSourcingLine>();

		// SERVER-SIDE ONLY: a client's copy of a loadout has no items to price
		if (!Replication.IsServer())
		{
			Print("[Overthrow] OVT_RecruitLoadoutPricing: Attempted to run on client - loadout contents exist only on the server!", LogLevel.WARNING);
			return m_Result;
		}

		if (!loadout)
		{
			Print("[Overthrow] OVT_RecruitLoadoutPricing: Refused - no loadout", LogLevel.WARNING);
			return m_Result;
		}

		m_Economy = OVT_Global.GetEconomy();
		if (!m_Economy)
		{
			Print("[Overthrow] OVT_RecruitLoadoutPricing: Refused - no economy manager", LogLevel.WARNING);
			return m_Result;
		}

		m_vPosition = pos;
		m_iPlayerId = playerId;

		array<ref OVT_LoadoutItem> items = loadout.GetItems();
		if (!items)
		{
			// An empty record is not a failure of this routine - it is a loadout with nothing in it,
			// which the caller refuses on its own terms (RESULT_LOADOUT_EMPTY).
			m_Result.m_bRan = true;
			return m_Result;
		}

		m_Result.m_iTopLevelCount = items.Count();

		foreach (OVT_LoadoutItem item : items)
		{
			// A stop here leaves m_bRan false and m_sUnpriceableResource set: partial numbers that
			// must never be charged, which is what the two flags together say.
			if (!PriceItem(item))
				return m_Result;
		}

		m_Result.m_bRan = true;
		return m_Result;
	}

	//------------------------------------------------------------------------------------------------
	//! Price one item, its attachments and everything inside it.
	//! \param[in] item The record entry. A null hole in the array is skipped, not refused.
	//! \return False when something under this item has no catalog price.
	protected bool PriceItem(OVT_LoadoutItem item)
	{
		if (!item) return true;

		if (!AddResource(item.m_sResourceName))
			return false;

		array<string> attachments = item.GetAttachments();
		if (attachments)
		{
			foreach (string attachment : attachments)
			{
				if (!AddResource(attachment))
					return false;
			}
		}

		array<ref OVT_LoadoutItem> children = item.GetChildItems();
		if (children)
		{
			foreach (OVT_LoadoutItem child : children)
			{
				if (!PriceItem(child))
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Add one resource to the subtotal, or refuse the whole loadout.
	//!
	//! An EMPTY resource name is skipped rather than refused: the record is malformed for that entry,
	//! the apply cannot spawn it either, and nothing arrives - so nothing is owed. It still counts
	//! against the top-level total, which is what makes the outcome PARTIAL rather than silently OK.
	//! \param[in] res The prefab to price.
	//! \return False when the resource has no catalog price.
	protected bool AddResource(ResourceName res)
	{
		if (res.IsEmpty()) return true;

		if (!m_Economy.IsRegisteredResource(res))
		{
			m_Result.m_sUnpriceableResource = res;

			Print(string.Format("[Overthrow] OVT_RecruitLoadoutPricing: %1 is not a registered resource - the loadout cannot be priced", res), LogLevel.WARNING);
			return false;
		}

		int unitPrice = m_Economy.GetBuyPrice(m_Economy.GetInventoryId(res), m_vPosition, m_iPlayerId);

		m_Result.m_iSubtotal = m_Result.m_iSubtotal + unitPrice;
		m_Result.m_iItemCount = m_Result.m_iItemCount + 1;

		AddManifestLine(res, unitPrice);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Add one priced occurrence to the per-resource manifest, aggregated by resource.
	//!
	//! AGGREGATED, NOT ONE LINE PER OCCURRENCE. OVT_ItemSourcingRules.SplitCoverage checks each line's
	//! stock independently, so two lines naming the same resource would each see the SAME warehouse
	//! total and double-count coverage that exists once.
	//! \param[in] res The resource just priced.
	//! \param[in] unitPrice What one unit costs at this position, for this player.
	protected void AddManifestLine(ResourceName res, int unitPrice)
	{
		foreach (OVT_ItemSourcingLine line : m_Result.m_aManifest)
		{
			if (line.m_sResource == res)
			{
				line.m_iNeeded = line.m_iNeeded + 1;
				return;
			}
		}

		OVT_ItemSourcingLine line = new OVT_ItemSourcingLine();
		line.m_sResource = res;
		line.m_iNeeded = 1;
		line.m_iUnitPrice = unitPrice;

		m_Result.m_aManifest.Insert(line);
	}
}
