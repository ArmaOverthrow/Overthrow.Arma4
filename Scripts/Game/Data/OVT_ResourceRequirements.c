//------------------------------------------------------------------------------------------------
//! The three POSITION-BASED requirement entry points: what a spot needs, what is around it, and
//! taking it.
//!
//! FROZEN SIGNATURES. building-repair is planned against these exact three; the maths behind them
//! lives in OVT_ResourceRules where the Logic tier can assert it without a world. Nothing here
//! re-implements a rule - it looks the world up and hands the answers to the pure statics.
//!
//! ONE QUERY OBJECT PER CALL, never a shared accumulator: OVT_InventoryManagerComponent's static
//! search buffer (:497) is the concurrency defect this project keeps re-learning.
//!
//! DISTANCES ARE SQUARED throughout. vector.Distance is not correctly rounded, so a decision taken
//! on it at an exact boundary is a coin flip.
//!
//! ONLY CRATE PILES ARE READ AND DRAINED. The sweep filters on OVT_ResourcePileComponent, not on
//! OVT_ResourceStoreComponent, so a truck parked beside a construction site is never emptied into it.
//------------------------------------------------------------------------------------------------
class OVT_ResourceRequirements
{
	//------------------------------------------------------------------------------------------------
	//! The scaled requirement list for a buildable's authored one.
	//!
	//! THE SINGLE CALL BEHIND BOTH THE DISPLAYED AND THE CONSUMED FIGURE. Every reader goes through
	//! here, so the build card, the site's readout, the site's action label and the server's own
	//! consumption can never disagree about what a building costs.
	//!
	//! Duplicate ids in one .conf entry are summed into a single line. Left as two lines they would
	//! each be checked against the FULL nearby total, which passes a requirement that cannot be paid
	//! and then drains the piles half way - the one way Consume()'s all-or-nothing could be broken
	//! from a config typo.
	//! \param[in] conf The buildable's authored requirements.
	//! \param[out] scaled Receives one entry per distinct id. Cleared first.
	static void ScaleForDifficulty(notnull array<ref OVT_BuildableResourceRequirement> conf, out array<ref OVT_ResourceAmount> scaled)
	{
		if (!scaled)
			scaled = new array<ref OVT_ResourceAmount>();

		scaled.Clear();

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		foreach (OVT_BuildableResourceRequirement requirement : conf)
		{
			if (!requirement)
				continue;

			if (requirement.m_sResourceId == "")
				continue;

			if (requirement.m_iQuantity <= 0)
				continue;

			int existing = IndexOfId(scaled, requirement.m_sResourceId);
			if (existing != -1)
			{
				scaled[existing].m_iQuantity = scaled[existing].m_iQuantity + Scale(config, requirement.m_iQuantity);
				continue;
			}

			OVT_ResourceAmount amount = new OVT_ResourceAmount();
			amount.m_sId = requirement.m_sResourceId;
			amount.m_iQuantity = Scale(config, requirement.m_iQuantity);
			scaled.Insert(amount);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! What the crate piles around a position hold, of the ids a requirement list names.
	//!
	//! CLIENT-SAFE. It reads each pile's REPLICATED contents through OVT_ResourceStoreComponent's
	//! mirror, so a site's readout and its build action are correct on every machine with no round
	//! trip. The server re-derives all of it before it consumes anything.
	//! \param[in] pos Centre of the sweep, normally the construction site.
	//! \param[in] requirements The ids to total up. An entry with no quantity is still reported.
	//! \param[out] available One entry per distinct requirement id, summed across every pile in
	//! radius. Cleared first, and always index-comparable through OVT_ResourceRules.AmountOf().
	static void NearbyAvailability(vector pos, notnull array<ref OVT_ResourceAmount> requirements, out array<ref OVT_ResourceAmount> available)
	{
		if (!available)
			available = new array<ref OVT_ResourceAmount>();

		available.Clear();

		foreach (OVT_ResourceAmount requirement : requirements)
		{
			if (!requirement || requirement.m_sId == "")
				continue;

			if (IndexOfId(available, requirement.m_sId) != -1)
				continue;

			OVT_ResourceAmount total = new OVT_ResourceAmount();
			total.m_sId = requirement.m_sId;
			total.m_iQuantity = 0;
			available.Insert(total);
		}

		if (available.IsEmpty())
			return;

		array<IEntity> piles = new array<IEntity>();
		CollectPiles(pos, piles);

		foreach (IEntity pile : piles)
		{
			OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(pile);
			if (!store)
				continue;

			OVT_ResourceLedger ledger = store.GetLedger();
			if (!ledger)
				continue;

			foreach (OVT_ResourceAmount total : available)
			{
				total.m_iQuantity = total.m_iQuantity + ledger.Count(total.m_sId);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER ONLY. Takes a requirement list out of the crate piles around a position.
	//!
	//! ALL OR NOTHING, AND IT REFUSES BEFORE IT MUTATES. Availability is summed from the SAME pile
	//! array the drain then walks, in the same call, with nothing yielding between the two - so a
	//! satisfied check cannot become an unsatisfied drain. Every early return above the drain loop
	//! leaves every ledger exactly as it found it.
	//!
	//! Piles are drained NEAREST FIRST, stably, through OVT_ResourceRules.SortPilesForConsumption on
	//! squared distances, so two sites consuming the same field take the same piles in the same order
	//! on every machine and in every session.
	//! \param[in] pos Centre of the sweep.
	//! \param[in] requirements What to take. Duplicate ids are summed before anything is checked.
	//! \return True when everything was taken; false when nothing was.
	static bool Consume(vector pos, notnull array<ref OVT_ResourceAmount> requirements)
	{
		if (!Replication.IsServer())
			return false;

		array<ref OVT_ResourceAmount> need = new array<ref OVT_ResourceAmount>();
		Coalesce(requirements, need);

		if (need.IsEmpty())
			return true;

		array<IEntity> piles = new array<IEntity>();
		array<ref OVT_ResourceLedger> ledgers = new array<ref OVT_ResourceLedger>();
		array<float> sqDistances = new array<float>();
		CollectStockedPiles(pos, piles, ledgers, sqDistances);

		// ---- REFUSE BEFORE MUTATE ----------------------------------------------------------------
		array<ref OVT_ResourceAmount> have = new array<ref OVT_ResourceAmount>();
		foreach (OVT_ResourceAmount requirement : need)
		{
			OVT_ResourceAmount total = new OVT_ResourceAmount();
			total.m_sId = requirement.m_sId;
			total.m_iQuantity = 0;

			foreach (OVT_ResourceLedger ledger : ledgers)
			{
				total.m_iQuantity = total.m_iQuantity + ledger.Count(total.m_sId);
			}

			have.Insert(total);
		}

		string shortId;
		if (!OVT_ResourceRules.IsSatisfied(need, have, shortId))
			return false;
		// ---- NOTHING ABOVE THIS LINE HAS TOUCHED A LEDGER ------------------------------------------

		array<int> order = new array<int>();
		OVT_ResourceRules.SortPilesForConsumption(sqDistances, order);

		// 0/1 per pile, never array<bool> - the parallel-array rule the persistence layer also carries.
		array<int> drained = new array<int>();
		for (int i = 0; i < piles.Count(); i++)
		{
			drained.Insert(0);
		}

		foreach (OVT_ResourceAmount requirement : need)
		{
			int remaining = requirement.m_iQuantity;

			foreach (int index : order)
			{
				if (remaining <= 0)
					break;

				int taken = ledgers[index].Take(requirement.m_sId, remaining);
				if (taken <= 0)
					continue;

				remaining = remaining - taken;
				drained[index] = 1;
			}

			// Unreachable: the check above summed these same ledgers with nothing yielding in between.
			if (remaining > 0)
				Print(string.Format("[Overthrow] OVT_ResourceRequirements.Consume() came up %1 short of '%2' AFTER the availability check passed against the same piles. The piles have been part-drained.", remaining.ToString(), requirement.m_sId), LogLevel.ERROR);
		}

		PublishDrainedPiles(piles, drained);

		return true;
	}

	//-----------------------------------------------------------------------------------------------
	// PROTECTED
	//-----------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One authored quantity at this difficulty. A non-zero requirement never scales to free.
	//! \param[in] config The Overthrow config, or null before it exists.
	//! \param[in] baseQty The authored quantity.
	//! \return The scaled quantity, or the authored one when there is no config to scale by.
	protected static int Scale(OVT_OverthrowConfigComponent config, int baseQty)
	{
		if (!config)
			return baseQty;

		return config.GetBuildableResourceCost(baseQty);
	}

	//------------------------------------------------------------------------------------------------
	//! Sums duplicate ids into one entry per id, dropping empties.
	//! \param[in] src The list as the caller wrote it.
	//! \param[out] dst Receives the canonical list. Cleared first.
	protected static void Coalesce(notnull array<ref OVT_ResourceAmount> src, notnull array<ref OVT_ResourceAmount> dst)
	{
		dst.Clear();

		foreach (OVT_ResourceAmount amount : src)
		{
			if (!amount || amount.m_sId == "" || amount.m_iQuantity <= 0)
				continue;

			int existing = IndexOfId(dst, amount.m_sId);
			if (existing != -1)
			{
				dst[existing].m_iQuantity = dst[existing].m_iQuantity + amount.m_iQuantity;
				continue;
			}

			OVT_ResourceAmount copy = new OVT_ResourceAmount();
			copy.m_sId = amount.m_sId;
			copy.m_iQuantity = amount.m_iQuantity;
			dst.Insert(copy);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] amounts The list to search.
	//! \param[in] id The id to find.
	//! \return Its index, or -1.
	protected static int IndexOfId(notnull array<ref OVT_ResourceAmount> amounts, string id)
	{
		for (int i = 0; i < amounts.Count(); i++)
		{
			if (amounts[i] && amounts[i].m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Every crate pile within the supply radius of a position.
	//! \param[in] pos Centre of the sweep.
	//! \param[out] piles Receives the piles. Cleared by the query.
	protected static void CollectPiles(vector pos, notnull array<IEntity> piles)
	{
		piles.Clear();

		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();
		if (!resources)
			return;

		OVT_ResourcePileQuery query = new OVT_ResourcePileQuery();
		query.Run(pos, resources.GetSupplyRadius(), piles);
	}

	//------------------------------------------------------------------------------------------------
	//! The piles that can actually give something up, with their ledgers and squared distances, all
	//! three index-aligned. One sweep serves both the availability check and the drain.
	//! \param[in] pos Centre of the sweep.
	//! \param[out] piles Receives the piles. Cleared first.
	//! \param[out] ledgers Receives their ledgers. Cleared first.
	//! \param[out] sqDistances Receives their squared distances from pos. Cleared first.
	protected static void CollectStockedPiles(vector pos, notnull array<IEntity> piles, notnull array<ref OVT_ResourceLedger> ledgers, notnull array<float> sqDistances)
	{
		ledgers.Clear();
		sqDistances.Clear();

		array<IEntity> found = new array<IEntity>();
		CollectPiles(pos, found);

		piles.Clear();

		foreach (IEntity pile : found)
		{
			OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(pile);
			if (!store)
				continue;

			OVT_ResourceLedger ledger = store.GetLedger();
			if (!ledger)
				continue;

			piles.Insert(pile);
			ledgers.Insert(ledger);
			sqDistances.Insert(vector.DistanceSq(pos, pile.GetOrigin()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Republishes every pile the drain touched, deleting the ones it emptied.
	//!
	//! An emptied pile is untracked and deleted in the same call that emptied it, so it is never
	//! published as an empty crate stack and never saved as one - the request component's rule.
	//! \param[in] piles The swept piles.
	//! \param[in] drained 1 for each pile something was taken from, index-aligned with piles.
	protected static void PublishDrainedPiles(notnull array<IEntity> piles, notnull array<int> drained)
	{
		OVT_ResourceManagerComponent resources = OVT_Global.GetResources();

		for (int i = 0; i < piles.Count(); i++)
		{
			if (drained[i] == 0)
				continue;

			if (resources && resources.DeletePileIfEmpty(piles[i]))
				continue;

			OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(piles[i]);
			if (store)
				store.PublishContents();
		}
	}
}
