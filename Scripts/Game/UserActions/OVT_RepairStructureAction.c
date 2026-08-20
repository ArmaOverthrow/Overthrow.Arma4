//------------------------------------------------------------------------------------------------
//! "Repair" - the held action on a ruined Overthrow structure that puts it back for money.
//!
//! Shown ONLY on a ruin, which is what makes it the inverse of every other action on these prefabs:
//! refuelling, recruiting, healing, the shop and the parking all disappear at phase 1 (D15) and this
//! one appears. Performable when the local player can pay the difficulty-scaled price, greyed out with
//! "cannot afford" when they cannot.
//!
//! THE COMPONENT IS NOT ALWAYS ON THIS ENTITY. The vehicle maintenance ramp's phase lives on a CHILD
//! of the bare root this action is mounted on, so both gates go up to the root parent and let
//! OVT_StructureDamage come back down one level - the same walk OVT_StructureDamage.IsUsable() does
//! for the tent's table child. The RplId sent to the server is the ROOT's, because the root is the
//! entity the server prices by prefab and the entity that carries the replicated identity.
//!
//! Nothing this class decides is authority: OVT_ResistanceRequestComponent.RpcAsk_RepairStructure
//! re-resolves the entity, re-tests that it is ruined, re-tests proximity, and
//! OVT_ResistanceFactionManager re-derives the price and takes the money.
//!
//! There is no script-side hold duration and adding one would give the same number two homes - the
//! Duration is authored in each prefab's additionalActions block (implementation.md §3.6).
//------------------------------------------------------------------------------------------------
class OVT_RepairStructureAction : ScriptedUserAction
{
	//! How long the cached ruin/price answers are reused, in milliseconds of world time. Both gates run
	//! every frame while a player looks at the structure; same caching shape as OVT_RearmVehicleAction.
	protected const float CHECK_TTL_MS = 1000;

	protected bool m_bCachedRuined;
	//! Dollars, or -1 when the structure cannot be priced for repair at all.
	protected int m_iCachedPrice;
	protected float m_fCacheExpiresAt;
	protected bool m_bHasCache;

	//------------------------------------------------------------------------------------------------
	//! Ask the server to repair this structure and charge us for it.
	//! \param[in] pOwnerEntity The entity the action lives on.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		IEntity root = ResolveRoot(pOwnerEntity);
		if(!root) return;

		RplComponent rpl = RplComponent.Cast(root.FindComponent(RplComponent));
		if(!rpl) return;

		OVT_ResistanceRequestComponent requests = OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get();
		if(!requests) return;

		requests.RepairStructure(rpl.Id());

		// The ruin state is about to change under us.
		m_bHasCache = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Shown only while this structure is a ruin AND has a price.
	//!
	//! The price half is not a gate on affordability - that is CanBePerformedScript's job, and a player
	//! who cannot pay must still see the action greyed out. It is a gate on REPAIRABILITY: a ruin the
	//! buildables config does not claim cannot be priced or repaired at all, and a client that has not
	//! yet read the config stream cannot price anything, so both would otherwise draw a label with no
	//! number in it.
	//! \param[in] user The character looking at the action.
	//! \return True while the structure is in its ruined phase and can be priced.
	override bool CanBeShownScript(IEntity user)
	{
		RefreshCache();
		return m_bCachedRuined && m_iCachedPrice >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, when the player cannot pay. Advisory only - the server re-derives both
	//! the price and the balance.
	//! \param[in] user The character looking at the action.
	//! \return True when the request is worth sending.
	override bool CanBePerformedScript(IEntity user)
	{
		RefreshCache();

		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if(m_iCachedPrice >= 0 && economy && !economy.LocalPlayerHasMoney(m_iCachedPrice))
		{
			SetCannotPerformReason("#OVT-CannotAfford");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The label carries the live price, the way every other priced action in the mod does.
	override bool GetActionNameScript(out string outName)
	{
		RefreshCache();

		if(m_iCachedPrice < 0)
		{
			outName = "#OVT-RepairStructure";
			return true;
		}

		outName = "#OVT-RepairStructure ($" + m_iCachedPrice.ToString() + ")";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs on the clicking machine only - PerformAction resolves the LOCAL player's controller.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Recompute the cached ruin state and price when they have gone stale.
	protected void RefreshCache()
	{
		float now = GetWorldTimeMs();
		if(m_bHasCache && now < m_fCacheExpiresAt) return;

		m_fCacheExpiresAt = now + CHECK_TTL_MS;
		m_bHasCache = true;

		m_bCachedRuined = false;
		m_iCachedPrice = -1;

		IEntity root = ResolveRoot(GetOwner());
		if(!root) return;

		m_bCachedRuined = OVT_StructureDamage.IsRuined(root);
		if(!m_bCachedRuined) return;

		// A client that has not yet read the config stream has no difficulty to price with; leaving the
		// price at -1 hides the action for that frame rather than drawing a wrong number.
		if(!OVT_Global.GetDifficulty()) return;

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return;

		m_iCachedPrice = resistance.GetRepairCost(root);
	}

	//------------------------------------------------------------------------------------------------
	//! The entity that carries the buildable identity - see the class header on the ramp.
	//! \param[in] entity The action's owner.
	//! \return Its root parent, or itself.
	protected IEntity ResolveRoot(IEntity entity)
	{
		if(!entity) return null;

		IEntity root = entity.GetRootParent();
		if(!root) return entity;

		return root;
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
