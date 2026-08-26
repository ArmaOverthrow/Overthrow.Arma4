//------------------------------------------------------------------------------------------------
//! Opens the High Command purchase screen from a barracks desk. The OVT_BuyEquippedRecruitAction
//! shape (implementation.md §3.10): opening a screen changes nothing, so every gate here is a
//! courtesy - the server re-derives all of it on quote and on purchase.
//------------------------------------------------------------------------------------------------
class OVT_ManageHighCommandAction : ScriptedUserAction
{
	//! How long a "may this be shown" answer is reused for, in milliseconds - the
	//! OVT_BuyEquippedRecruitAction LOADOUT_CHECK_INTERVAL_MS shape. CanBeShownScript runs every frame
	//! the player looks at the desk, and the friendly-base test is a base lookup plus a distance
	//! check, not free enough to repeat every frame on a busy server.
	static const float BASE_CHECK_INTERVAL_MS = 1000;

	//! The last answer CanBeShownScript computed.
	protected bool m_bCachedShown;

	//! World time the answer above was measured at, or 0 when it never was.
	protected float m_fCheckedAt;

	//------------------------------------------------------------------------------------------------
	//! Open the High Command purchase screen.
	//! \param[in] pOwnerEntity The entity the action is authored on - the desk.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_UIManagerComponent uiManager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if (!uiManager) return;

		uiManager.ShowContext(OVT_HighCommandPurchaseContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Shown at a usable barracks belonging to a friendly base. Cached on BASE_CHECK_INTERVAL_MS.
	//! \param[in] user The acting character.
	//! \return True when this desk may open the purchase screen.
	override bool CanBeShownScript(IEntity user)
	{
		float now = GetWorldTimeMs();

		if (m_fCheckedAt > 0 && now >= m_fCheckedAt && now - m_fCheckedAt < BASE_CHECK_INTERVAL_MS)
			return m_bCachedShown;

		m_fCheckedAt = now;
		m_bCachedShown = ComputeShown();

		return m_bCachedShown;
	}

	//------------------------------------------------------------------------------------------------
	//! The uncached computation behind CanBeShownScript.
	//! \return True when the desk's own building is usable and the desk sits on a friendly base.
	protected bool ComputeShown()
	{
		// PHASE-0 GATE (core/damage D15): a ruin offers nothing.
		if (!OVT_StructureDamage.IsUsable(GetOwner())) return false;

		vector position = GetOwner().GetOrigin();

		// Re-uses the server's own query rather than a second one, so the client gate and the
		// server's NOT_AT_BARRACKS check can never disagree about what counts as a barracks here.
		if (!OVT_BarracksComponent.FindNearest(position, OVT_BarracksComponent.BARRACKS_USE_RADIUS))
			return false;

		return IsAtFriendlyBase(position);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the desk sits at a base the resistance holds - the ManageBase / barracks-ask rule.
	//! \param[in] position The desk's position.
	//! \return True when the nearest base is within baseRange and is not the occupying faction's.
	protected bool IsAtFriendlyBase(vector position)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying) return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_Difficulty) return false;

		OVT_BaseData base = occupying.GetNearestBase(position);
		if (!base) return false;

		if (base.IsOccupyingFaction()) return false;

		return vector.Distance(base.location, position) < config.m_Difficulty.baseRange;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, only at the member cap. Never hidden for it (BUG-102): a player who
	//! cannot buy right now still needs to see why.
	//! \param[in] user The acting character.
	//! \return True when the screen is worth opening.
	override bool CanBePerformedScript(IEntity user)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players) return false;

		string persId = players.GetPersistentIDFromControlledEntity(user);
		if (persId.IsEmpty()) return false;

		OVT_HighCommandManagerComponent manager = OVT_Global.GetHighCommand();
		if (manager)
		{
			int cap = manager.GetMemberCap();
			if (cap > 0 && manager.GetMemberCount(persId) >= cap)
			{
				SetCannotPerformReason("#OVT-HC_AtCap");
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! World time in milliseconds, guarded so the action still answers in a world-less context.
	//! \return The current world time, or 0.
	protected float GetWorldTimeMs()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world) return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Opening a screen changes nothing anyone else can see.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
