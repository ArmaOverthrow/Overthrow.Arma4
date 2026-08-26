//------------------------------------------------------------------------------------------------
//! "Sell Cargo Here" - the returning Arma 3 Overthrow trunk action.
//!
//! Appears on a vehicle only while it is parked within selling range of a shop that actually buys
//! from players, and only while its STORAGE LEDGER holds something. Performing it hands the whole job to
//! OVT_ShopTransactionComponent.SellVehicleCargo: ONE client->server request regardless of cargo size
//! (quality bar Q4), after which the server re-derives the shop, the range, the lock/ownership rule,
//! the driver rule, per-item eligibility and every price. Nothing this class decides is authority -
//! the gates below exist so the action does not offer a sale the server would refuse.
//!
//! Local-effect-only: the client wrapper resolves the LOCAL player's controller entity
//! (OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get()), so PerformAction must run on the
//! machine that clicked. A server-executed PerformAction would find no local controller on a
//! dedicated server.
//------------------------------------------------------------------------------------------------
class OVT_SellVehicleCargoAction : ScriptedUserAction
{
	//! Mirrors OVT_ShopTransactionComponent.SHOP_MAX_DISTANCE, which is protected there. Keep the two
	//! in step: showing the action further away than the server accepts would produce an action that
	//! is visible but always reports "nothing sold".
	protected const float SHOP_MAX_DISTANCE = 30;

	//! How long a visibility answer is reused before it is recomputed, in milliseconds of world time.
	//! CanBeShownScript runs every frame per nearby action, and the honest answer costs a shop scan
	//! (R6). One second is far below the time it takes a player to walk into or out of range and far
	//! above the frame rate.
	protected const float VISIBILITY_TTL_MS = 1000;

	//! Cached CanBeShownScript answer and the world time (ms) at which it goes stale.
	protected bool m_bCachedVisible;
	protected float m_fCacheExpiresAt;
	protected bool m_bHasCache;

	//! The transaction component this action subscribed to, so it unsubscribes from the same one.
	//! Non-null only while a sell request of ours is in flight.
	protected OVT_ShopTransactionComponent m_Transactions;

	//------------------------------------------------------------------------------------------------
	//! Sells everything in this vehicle's cargo that the nearest eligible shop buys.
	//! \param[in] pOwnerEntity The vehicle this action lives on.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if(!pOwnerEntity) return;

		// Hosted on truck cargo beds too, whose storage lives one hop up on the vehicle root.
		pOwnerEntity = OVT_StorageUtils.ResolveStorageHolder(pOwnerEntity);

		// The shop is resolved again here rather than reused from the visibility cache: the cached
		// answer may be up to a second old and the vehicle may have rolled since.
		OVT_ShopComponent shop = FindEligibleShop(pOwnerEntity);
		if(!shop)
		{
			ShowHint("#OVT-Shop_SoldNothing");
			return;
		}

		OVT_ShopTransactionComponent transactions = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
		if(!transactions) return;

		SubscribeSellResult(transactions);

		transactions.SellVehicleCargo(pOwnerEntity, shop);

		// The visibility answer is now certainly stale (the cargo is about to empty).
		InvalidateCache();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether to show the action at all: an eligible shop in range of the VEHICLE, and cargo to sell.
	//!
	//! Measured from the vehicle, not the player, because that is what the server bounds - a player
	//! standing between a shop and a car parked just outside range must not be offered the sale.
	//! Throttled by VISIBILITY_TTL_MS.
	//! \param[in] user The character looking at the action.
	//! \return True when the action should be listed.
	override bool CanBeShownScript(IEntity user)
	{
		float now = GetWorldTimeMs();

		if(m_bHasCache && now < m_fCacheExpiresAt) return m_bCachedVisible;

		m_bCachedVisible = ComputeVisible();
		m_fCacheExpiresAt = now + VISIBILITY_TTL_MS;
		m_bHasCache = true;

		return m_bCachedVisible;
	}

	//------------------------------------------------------------------------------------------------
	//! Lock/ownership and driver rules, the same two OVT_UnloadStorageAction applies (and the same two
	//! the server re-derives in RpcAsk_SellVehicleCargo, because a user action's gates are not
	//! authority). Reuses that action's existing reason keys rather than inventing new ones.
	//! \param[in] user The character looking at the action.
	//! \return True when the action may be performed.
	override bool CanBePerformedScript(IEntity user)
	{
		if(!user) return false;

		RplComponent userRpl = RplComponent.Cast(user.FindComponent(RplComponent));
		if(!userRpl) return false;

		IEntity vehicle = OVT_StorageUtils.ResolveStorageHolder(GetOwner());
		if(!vehicle) return false;

		// Somebody still in the driver's seat: the same rule (and the same message) as unloading.
		if(VehicleHasPilot(vehicle))
		{
			SetCannotPerformReason("#OVT-DriverMustExit");
			return false;
		}

		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return userRpl.IsOwner();

		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
		if(!playerowner || !playerowner.IsLocked()) return userRpl.IsOwner();

		string ownerUid = playerowner.GetPlayerOwnerUid();
		if(ownerUid == "") return userRpl.IsOwner();

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string playerUid = players.GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid)
		{
			SetCannotPerformReason("#OVT-Locked");
			return false;
		}

		return userRpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the clicking machine only - see the class header.
	override bool HasLocalEffectOnlyScript() { return true; }

	//-----------------------------------------------------------------------
	// VISIBILITY
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The unthrottled visibility answer. Every lookup null-guarded: this runs in the world editor and
	//! in scenarios with no Overthrow game mode, where none of the managers exist.
	//! \return True when an eligible shop is in range and the vehicle has cargo.
	protected bool ComputeVisible()
	{
		IEntity vehicle = OVT_StorageUtils.ResolveStorageHolder(GetOwner());
		if(!vehicle) return false;

		if(!FindEligibleShop(vehicle)) return false;

		return VehicleHasCargo(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest shop to a position that also has a sell path.
	//!
	//! Derives the two inputs of the sell-path rule client-side exactly as the server derives them in
	//! OVT_ShopTransactionComponent.ShopBuysHere: the shop's own procurement flag plus the difficulty
	//! config's gun dealer multiplier. Same rule class, same inputs, so the action is visible in
	//! precisely the cases the server would honour.
	//!
	//! Known limitation, deliberate: GetNearestShop returns the NEAREST shop, so a vehicle parked
	//! closer to a vehicle shop than to the general shop next door sees no action. That matches the
	//! plan's gating and keeps this to one scan; a nearest-eligible search would need a new manager
	//! seam.
	//! \param[in] vehicle The vehicle to measure from.
	//! \return The shop, or null when there is none in range or it does not buy.
	protected OVT_ShopComponent FindEligibleShop(IEntity vehicle)
	{
		if(!vehicle) return null;

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(!economy) return null;

		OVT_ShopComponent shop = economy.GetNearestShop(vehicle.GetOrigin(), SHOP_MAX_DISTANCE);
		if(!shop) return null;

		if(!OVT_ShopSellRules.ShopBuysFromPlayers(shop.m_ShopType, shop.m_bProcurement, GetGunDealerMultiplier()))
			return null;

		return shop;
	}

	//------------------------------------------------------------------------------------------------
	//! The difficulty config's gun dealer sell multiplier, read the way the shop menu and the server
	//! read it. Falls back to 0, which hides the action at a gun dealer rather than promising a sale
	//! at an unconfigured rate.
	//! \return The multiplier, or 0.
	protected float GetGunDealerMultiplier()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return 0;

		return config.m_Difficulty.gunDealerSellPriceMultiplier;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle's storage ledger holds anything.
	//!
	//! Reads the same ledger the server sells out of. Items sitting loose in the vanilla cargo bed do
	//! NOT count - they are moved into storage through the transfer screen first.
	//! \param[in] vehicle The vehicle to check.
	//! \return True when the ledger holds at least one item.
	protected bool VehicleHasCargo(IEntity vehicle)
	{
		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(vehicle);
		if(!storage) return false;

		return storage.GetTotalCount() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether anybody is still in a pilot/driver compartment.
	//! \param[in] vehicle The vehicle to check.
	//! \return True when a driver is aboard.
	protected bool VehicleHasPilot(IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if(!compartments) return false;

		array<IEntity> pilots = {};
		compartments.GetOccupantsOfType(pilots, ECompartmentType.PILOT);

		return !pilots.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Forces the next CanBeShownScript to recompute.
	protected void InvalidateCache()
	{
		m_bHasCache = false;
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

	//-----------------------------------------------------------------------
	// RESULT FEEDBACK
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Listens for the outcome of THIS sale, for exactly as long as it takes to report it.
	//!
	//! The invoker lives on the local player's controller component, not on the vehicle, so every
	//! instance of this action in the world could otherwise pile handlers onto the same invoker. Two
	//! rules keep that to exactly one handler: the handler removes itself as soon as it fires, and a
	//! fresh subscription always drops the previous one first (so a second click while the first
	//! request is still in flight replaces rather than stacks). The result RPC is Owner-targeted, so
	//! only the requesting client ever reaches this at all - a bystander's action instance on the same
	//! vehicle never shows a hint.
	//! \param[in] transactions The component to subscribe to.
	protected void SubscribeSellResult(OVT_ShopTransactionComponent transactions)
	{
		if(!transactions || !transactions.m_OnSellResult) return;

		// Never hold two subscriptions to the same invoker.
		UnsubscribeSellResult();

		m_Transactions = transactions;
		m_Transactions.m_OnSellResult.Insert(OnSellResult);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the subscription, from the same component it was made on.
	protected void UnsubscribeSellResult()
	{
		if(m_Transactions && m_Transactions.m_OnSellResult)
			m_Transactions.m_OnSellResult.Remove(OnSellResult);

		m_Transactions = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports what the SERVER actually did. Display only - money and cargo already arrived through
	//! their own replicated paths.
	//! \param[in] soldCount Items actually sold.
	//! \param[in] totalEarned Money credited, server-computed.
	//! \param[in] skippedCount Cargo items this shop would not buy.
	//! \param[in] result OVT_ShopSellResult value.
	protected void OnSellResult(int soldCount, int totalEarned, int skippedCount, int result)
	{
		UnsubscribeSellResult();
		InvalidateCache();

		if(soldCount > 0)
		{
			string text = WidgetManager.Translate("#OVT-Shop_SoldItems", soldCount.ToString(), OVT_MoneyFormat.FormatMoney(totalEarned));

			if(skippedCount > 0)
				text = text + " - " + WidgetManager.Translate("#OVT-Shop_SkippedItems", skippedCount.ToString());

			ShowHintText(text);
			return;
		}

		if(skippedCount > 0)
		{
			ShowHintText(WidgetManager.Translate("#OVT-Shop_SkippedItems", skippedCount.ToString()));
			return;
		}

		ShowHint("#OVT-Shop_SoldNothing");
	}

	//------------------------------------------------------------------------------------------------
	//! Shows a localization key as a hint.
	//! \param[in] key An #OVT- key.
	protected void ShowHint(string key)
	{
		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if(!hints) return;

		hints.ShowCustom(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows already-translated text as a hint.
	//! \param[in] text The text to show.
	protected void ShowHintText(string text)
	{
		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if(!hints) return;

		hints.ShowCustom(text);
	}
}
