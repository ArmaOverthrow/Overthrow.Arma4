//------------------------------------------------------------------------------------------------
//! "Re-arm" - stop-gap full restock of an armed helicopter's weapons, on the heli_repair_point
//! context every helicopter carries (authored in Prefabs/Vehicles/Core/Helicopter_Base.et). To be
//! superseded by the logistics epic.
//!
//! Shown on any helicopter that structurally has something to re-arm (a turret gun magazine or a
//! rocket pod - OVT_VehicleRearmUtils decides). Performable only while the helicopter sits on a
//! BUILT helipad at a resistance-held base, something is actually missing, and the player can
//! afford the difficulty-scaled price; each blocked case stays visible with a reason, following
//! OVT_SabotageTowerAction's rule that a relevant but blocked action must not silently vanish.
//!
//! Nothing this class decides is authority: the server re-derives the helipad, the base, the
//! distance, the need and the price in OVT_ShopTransactionComponent.RpcAsk_RearmVehicle.
//!
//! Local-effect-only: the client wrapper resolves the LOCAL player's controller entity
//! (OVT_Global.GetShopTransactions), so PerformAction must run on the machine that clicked.
//------------------------------------------------------------------------------------------------
class OVT_RearmVehicleAction : ScriptedUserAction
{
	//! How long computed answers are reused, in milliseconds of world time. Both gates run every
	//! frame while a player looks at the helicopter, and the honest answers cost a slot walk
	//! (armed / needs ammo) plus a world sphere query (helipad). Same caching shape as
	//! OVT_SellVehicleCargoAction.
	protected const float CHECK_TTL_MS = 1000;

	//! Cached gate answers and the world time (ms) at which they go stale.
	protected bool m_bCachedArmed;
	protected bool m_bCachedNeedsRearm;
	protected bool m_bCachedOnHelipad;
	protected float m_fCacheExpiresAt;
	protected bool m_bHasCache;

	//------------------------------------------------------------------------------------------------
	//! Ask the server for a full re-arm of this helicopter.
	//! \param[in] pOwnerEntity The helicopter this action lives on.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_ShopTransactionComponent transactions = OVT_Global.GetShopTransactions();
		if(!transactions) return;

		transactions.RearmVehicle(pOwnerEntity);

		// The needs-rearm answer is now certainly stale (the pods are about to fill).
		m_bHasCache = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only on a helicopter that has rearmable weapons at all. Deliberately NOT hidden when
	//! the helicopter is off a helipad, full, or the player is broke - those are told why in
	//! CanBePerformedScript.
	//! \param[in] user The character looking at the action.
	//! \return True when this helicopter has anything a re-arm could ever touch.
	override bool CanBeShownScript(IEntity user)
	{
		RefreshCache();
		return m_bCachedArmed;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, unless the helicopter is on a built helipad at a friendly base,
	//! something is missing, and the player can pay. A courtesy, never a security boundary - the
	//! server re-derives every one of these.
	//! \param[in] user The character looking at the action.
	//! \return True when the request is worth sending.
	override bool CanBePerformedScript(IEntity user)
	{
		RefreshCache();

		if(!m_bCachedOnHelipad)
		{
			SetCannotPerformReason("#OVT-MustBeOnHelipad");
			return false;
		}

		if(!m_bCachedNeedsRearm)
		{
			SetCannotPerformReason("#OVT-NothingToRearm");
			return false;
		}

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(economy && !economy.LocalPlayerHasMoney(OVT_VehicleRearmUtils.GetRearmCost()))
		{
			SetCannotPerformReason("#OVT-CannotAfford");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Put the difficulty-scaled price in the action label, the way OVT_RecruitCivilianAction does.
	override bool GetActionNameScript(out string outName)
	{
		int cost = OVT_VehicleRearmUtils.GetRearmCost();
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
	//! Recompute the three cached gate answers when they have gone stale.
	//!
	//! The helipad query is only paid for a helicopter that is armed in the first place, so an
	//! unarmed transport helicopter costs one slot walk per second and no world query.
	protected void RefreshCache()
	{
		float now = GetWorldTimeMs();
		if(m_bHasCache && now < m_fCacheExpiresAt) return;

		m_fCacheExpiresAt = now + CHECK_TTL_MS;
		m_bHasCache = true;

		m_bCachedArmed = false;
		m_bCachedNeedsRearm = false;
		m_bCachedOnHelipad = false;

		// No Overthrow game mode (world editor, other scenarios): none of the managers exist.
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!gameMode) return;

		IEntity vehicle = GetOwner();
		if(!vehicle) return;

		array<BaseMagazineComponent> magazines;
		array<IEntity> rocketWeapons;
		OVT_VehicleRearmUtils.GetRearmableWeapons(vehicle, magazines, rocketWeapons);

		m_bCachedArmed = !magazines.IsEmpty() || !rocketWeapons.IsEmpty();
		if(!m_bCachedArmed) return;

		m_bCachedNeedsRearm = OVT_VehicleRearmUtils.AnyAmmoMissing(magazines, rocketWeapons);

		OVT_VehicleRearmUtils rearmUtils = new OVT_VehicleRearmUtils();
		m_bCachedOnHelipad = rearmUtils.IsOnHelipadAtFriendlyBase(vehicle.GetOrigin());
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
