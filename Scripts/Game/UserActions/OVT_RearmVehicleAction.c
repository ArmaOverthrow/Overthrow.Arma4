//------------------------------------------------------------------------------------------------
//! "Re-arm" - one full restock of an armed vehicle's weapons, authored in
//! Prefabs/Vehicles/Core/Vehicle_Base.et across seven contexts (helicopter repair point, doors,
//! APC hatches), so it reaches the APCs, the armed jeeps and both helicopter families.
//!
//! Shown on any vehicle that structurally has something to re-arm (a turret gun magazine or a
//! rocket pod - OVT_VehicleRearmUtils decides), so an unarmed car never sees it. Blocked cases stay
//! VISIBLE with a reason, following OVT_SabotageTowerAction's rule that a relevant but blocked
//! action must not silently vanish.
//!
//! THE GATE FOLLOWS THE MONEY (R1 as amended). Ammunition the crew already carries is spent before
//! money, so a covered re-arm is free and performable anywhere; only a PURCHASE needs a supply
//! point - a built helipad or garage at a resistance-held base, or a deployed FOB.
//!
//! COVERAGE CANNOT BE COMPUTED HERE. Ledger contents never leave the server
//! (OVT_StorageComponent.c:35-38), so the price comes from a server quote asked once per cache
//! window and delivered to this client alone (OVT_ShopTransactionComponent.RequestRearmQuote).
//! WITH NO QUOTE IN HAND THE ACTION ASSUMES NOTHING IS COVERED - today's full price, today's site
//! gate - so a missing, late or lost quote can only make the button more restrictive than the truth,
//! never less.
//!
//! Nothing this class decides is authority: the server re-derives the site, the base, the distance,
//! the need, the coverage and the price in OVT_ShopTransactionComponent.RpcAsk_RearmVehicle.
//!
//! Local-effect-only: the client wrapper resolves the LOCAL player's controller entity
//! (OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get), so PerformAction must run on the machine that clicked.
//------------------------------------------------------------------------------------------------
class OVT_RearmVehicleAction : ScriptedUserAction
{
	//! How long computed answers are reused, in milliseconds of world time. Both gates run every
	//! frame while a player looks at the vehicle, and the honest answers cost a slot walk (armed /
	//! needs ammo), a world sphere query (the site) and now ONE RPC pair (the quote) - which is why
	//! this window is twice OVT_SellVehicleCargoAction's. Two seconds is still far below the time it
	//! takes a player to walk out of range and far above the frame rate.
	protected const float CHECK_TTL_MS = 2000;

	//! Cached local gate answers and the world time (ms) at which they go stale.
	protected bool m_bCachedArmed;
	protected bool m_bCachedNeedsRearm;
	protected bool m_bCachedAtSite;
	protected float m_fCacheExpiresAt;
	protected bool m_bHasCache;

	//! The last quote the server sent FOR THIS VEHICLE. Display only.
	protected bool m_bHasQuote;
	protected int m_iQuotedTotal;
	protected int m_iQuotedCovered;
	protected int m_iQuotedCost;

	//! The transaction component this action subscribed to, so it unsubscribes from the same one.
	//! Non-null only while a quote of ours is outstanding.
	protected OVT_ShopTransactionComponent m_Transactions;

	//------------------------------------------------------------------------------------------------
	//! Ask the server for a full re-arm of this vehicle.
	//! \param[in] pOwnerEntity The vehicle this action lives on.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return;

		transactions.RearmVehicle(pOwnerEntity);

		// Both answers are now certainly stale: the weapons are about to fill and the ledgers that
		// covered them are about to empty. Dropping the quote returns the button to the conservative
		// full-price state until the next one arrives.
		m_bHasCache = false;
		ForgetQuote();
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only on a vehicle that has rearmable weapons at all. Deliberately NOT hidden when the
	//! vehicle is off a supply point, full, or the player is broke - those are told why in
	//! CanBePerformedScript.
	//! \param[in] user The character looking at the action.
	//! \return True when this vehicle has anything a re-arm could ever touch.
	override bool CanBeShownScript(IEntity user)
	{
		RefreshCache();
		return m_bCachedArmed;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, unless something is missing and either the ledgers cover it or the
	//! vehicle is at a supply point with the money to hand. A courtesy, never a security boundary -
	//! the server re-derives every one of these.
	//! \param[in] user The character looking at the action.
	//! \return True when the request is worth sending.
	override bool CanBePerformedScript(IEntity user)
	{
		RefreshCache();

		if(!m_bCachedNeedsRearm)
		{
			SetCannotPerformReason("#OVT-NothingToRearm");
			return false;
		}

		// Free when the crew brought the ammunition: no site, no funds, no gate at all.
		int cost = QuotedCost();
		if(cost <= 0) return true;

		if(!m_bCachedAtSite)
		{
			SetCannotPerformReason("#OVT-Rearm_NeedsSupplyPoint");
			return false;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(economy && !economy.LocalPlayerHasMoney(cost))
		{
			SetCannotPerformReason("#OVT-CannotAfford");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Two labels: the price when one is owed, "from storage" when a quote says the ledgers cover it.
	//! Without a quote the price shown is the full one - see QuotedCost().
	override bool GetActionNameScript(out string outName)
	{
		if(m_bHasQuote && m_iQuotedCost <= 0)
		{
			outName = "#OVT-RearmVehicle_FromStorage";
			return true;
		}

		int cost = QuotedCost();
		outName = "#OVT-RearmVehicle ($" + cost.ToString() + ")";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the clicking machine only - see the class header.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops any outstanding subscription. The invoker lives on the local player's controller and not
	//! on this vehicle, so a subscription that outlived this action would fire into freed memory.
	void ~OVT_RearmVehicleAction()
	{
		UnsubscribeQuote();
	}

	//------------------------------------------------------------------------------------------------
	//! What the player is being charged, as far as this client knows.
	//!
	//! ⚠ THE NO-QUOTE ANSWER IS THE FULL PRICE ON PURPOSE. "Nothing covered" is the most restrictive
	//! assumption available, and ProratedCost never returns more than the full price, so every gate
	//! keyed off this number is at least as strict before a quote arrives as after one does. The
	//! reverse - assuming free - would offer a re-arm the server then refuses.
	//! \return The quoted price, or the full difficulty-scaled price when no quote has arrived.
	protected int QuotedCost()
	{
		if(m_bHasQuote) return m_iQuotedCost;

		return OVT_VehicleRearmUtils.GetRearmCost();
	}

	//------------------------------------------------------------------------------------------------
	//! Recompute the three local gate answers when they have gone stale, and re-ask for the quote.
	//!
	//! The site query and the quote are only paid for a vehicle that is armed and actually missing
	//! something, so an unarmed car costs one slot walk per window and no world query, and a full
	//! LAV costs a slot walk and a sphere query but no RPC.
	protected void RefreshCache()
	{
		float now = GetWorldTimeMs();
		if(m_bHasCache && now < m_fCacheExpiresAt) return;

		m_fCacheExpiresAt = now + CHECK_TTL_MS;
		m_bHasCache = true;

		m_bCachedArmed = false;
		m_bCachedNeedsRearm = false;
		m_bCachedAtSite = false;

		// No Overthrow game mode (world editor, other scenarios): none of the managers exist.
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!gameMode) return;

		IEntity vehicle = GetOwner();
		if(!vehicle) return;

		array<BaseMagazineComponent> magazines;
		array<BaseMuzzleComponent> muzzles;
		array<IEntity> rocketWeapons;
		OVT_VehicleRearmUtils.GetRearmableWeapons(vehicle, magazines, muzzles, rocketWeapons);

		m_bCachedArmed = !magazines.IsEmpty() || !rocketWeapons.IsEmpty();
		if(!m_bCachedArmed) return;

		m_bCachedNeedsRearm = OVT_VehicleRearmUtils.AnyAmmoMissing(magazines, rocketWeapons);

		OVT_VehicleRearmUtils rearmUtils = new OVT_VehicleRearmUtils();
		m_bCachedAtSite = rearmUtils.IsAtRearmSite(vehicle.GetOrigin());

		if(m_bCachedNeedsRearm)
			AskForQuote(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! One quote request per cache window, through the local player's own controller.
	//! \param[in] vehicle This action's vehicle.
	protected void AskForQuote(IEntity vehicle)
	{
		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return;

		SubscribeQuote(transactions);

		// On a listen host / in single player this runs the whole pair synchronously, so OnRearmQuote
		// has already fired by the time this returns.
		transactions.RequestRearmQuote(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Listens for a quote, for as long as it takes one about THIS vehicle to arrive.
	//!
	//! The invoker lives on the local player's controller, not on the vehicle, so every armed vehicle
	//! the player walks past shares it. A fresh subscription always drops the previous one first, so
	//! a re-ask replaces rather than stacks.
	//! \param[in] transactions The component to subscribe to.
	protected void SubscribeQuote(OVT_ShopTransactionComponent transactions)
	{
		if(!transactions || !transactions.m_OnRearmQuote) return;

		UnsubscribeQuote();

		m_Transactions = transactions;
		m_Transactions.m_OnRearmQuote.Insert(OnRearmQuote);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the subscription, from the same component it was made on.
	protected void UnsubscribeQuote()
	{
		if(m_Transactions && m_Transactions.m_OnRearmQuote)
			m_Transactions.m_OnRearmQuote.Remove(OnRearmQuote);

		m_Transactions = null;
	}

	//------------------------------------------------------------------------------------------------
	//! A quote landed on this client. Stored only when it is about this action's own vehicle.
	//!
	//! ⚠ A MISMATCH MUST NOT UNSUBSCRIBE. Two armed vehicles parked together is the ordinary case, and
	//! both actions hold a subscription to the one invoker; dropping ours on the neighbour's quote
	//! would leave this vehicle permanently un-quoted.
	//! \param[in] vehicleId The vehicle the server quoted.
	//! \param[in] totalUnits Units the re-arm needs.
	//! \param[in] coveredUnits Units the reachable ledgers hold.
	//! \param[in] cost Money owed for the remainder.
	protected void OnRearmQuote(RplId vehicleId, int totalUnits, int coveredUnits, int cost)
	{
		if(vehicleId != GetOwnerRplId()) return;

		UnsubscribeQuote();

		m_bHasQuote = true;
		m_iQuotedTotal = totalUnits;
		m_iQuotedCovered = coveredUnits;
		m_iQuotedCost = cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Back to "nothing is covered" until the server says otherwise.
	protected void ForgetQuote()
	{
		m_bHasQuote = false;
		m_iQuotedTotal = 0;
		m_iQuotedCovered = 0;
		m_iQuotedCost = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! This vehicle's networked name - the only entity reference that means the same thing on both
	//! machines, which is why the quote carries one.
	//! \return The owner's RplId, or RplId.Invalid() when it is not replicated.
	protected RplId GetOwnerRplId()
	{
		IEntity vehicle = GetOwner();
		if(!vehicle) return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if(!rpl) return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! World time in milliseconds, guarded so the action still answers in a world-less context.
	//! \return The current world time, or 0.
	protected float GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if(!world) return 0;

		return world.GetWorldTime();
	}
}
