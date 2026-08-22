//------------------------------------------------------------------------------------------------
//! Timed hold action on a recruitment tent that opens the loadouts screen in PURCHASE mode: pick one
//! of your own saved loadouts and buy a recruit already wearing it.
//!
//! The tent's second action. The first, OVT_RecruitFromTentAction, hires an unequipped civilian for
//! half the base recruit cost; this one hires the same recruit and dresses it, for that cost plus the
//! kit at local shop prices times a server-configured fee. Both take a supporter from the nearest
//! town and both count against the sixteen-recruit cap - an equipped recruit is a tent recruit that
//! happens to arrive dressed (decision D23).
//!
//! ! THIS ACTION BUYS NOTHING. It opens a screen. The purchase is a separate, deliberate press inside
//! that screen, after the player has seen a price - which is the whole reason the flow has a quote
//! step at all. Nothing here talks to the server.
//!
//! IT MUST RESOLVE THE TENT'S ROOT. The action is authored on the tent's table child
//! (OVT_RecruitmentTent.et:52-94), and it is the ROOT that carries the RplComponent the request names
//! the tent by and the OVT_BuildableComponent the server checks it against. ResolveTentRoot() walks up
//! looking for exactly the component-and-type pair the server tests, so an action that can be
//! performed is one whose tent the server will accept. Precedent for walking up: OVT_DeployFOBAction.
//!
//! Duration is set PER PREFAB INSTANCE, not here - Duration is a native BaseUserAction config field
//! and script only gets to read it (BaseUserAction.c:68).
//------------------------------------------------------------------------------------------------
class OVT_BuyEquippedRecruitAction : ScriptedUserAction
{
	//! How long a "does this player have any loadouts" answer is reused for, in milliseconds.
	//!
	//! CanBePerformedScript runs every frame the player is looking at the tent, and the only way to
	//! answer that question walks the whole loadout index - EVERY player's, splitting each key - which
	//! is a per-frame allocation on a busy server for an answer that changes when somebody saves a
	//! loadout, i.e. never mid-look. Same caching shape as OVT_SellVehicleCargoAction.
	static const float LOADOUT_CHECK_INTERVAL_MS = 1000;

	//! Whether the acting player had any saved loadouts when last asked.
	protected bool m_bHasLoadouts;

	//! World time the answer above was measured at, or 0 when it never was.
	protected float m_fLoadoutsCheckedAt;

	//------------------------------------------------------------------------------------------------
	//! Open the loadouts screen against this tent.
	//! \param[in] pOwnerEntity The entity the action is authored on - the tent's table child.
	//! \param[in] pUserEntity The acting character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		IEntity tent = ResolveTentRoot(pOwnerEntity);
		if(!tent) return;

		OVT_UIManagerComponent uiManager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!uiManager) return;

		OVT_LoadoutsContext loadoutsContext = OVT_LoadoutsContext.Cast(uiManager.GetContext(OVT_LoadoutsContext));
		if(loadoutsContext)
			loadoutsContext.SetRecruitmentTent(tent);

		uiManager.ShowContext(OVT_LoadoutsContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Shown on anything that really is a recruitment tent.
	//!
	//! Deliberately NOT hidden for a player with no saved loadouts, at the cap, or out of money: those
	//! are told why in CanBePerformedScript, following OVT_SabotageTowerAction's rule that a relevant
	//! but blocked action stays visible with a reason. An action that silently vanishes is how a player
	//! never learns the feature exists.
	//! \param[in] user The acting character.
	//! \return True when this entity belongs to a recruitment tent.
	override bool CanBeShownScript(IEntity user)
	{
		// PHASE-0 GATE (core/damage D15): a ruin offers nothing. IsUsable() answers true for every
		// owner that is not a retrofitted structure, so this costs the other contexts nothing.
		if(!OVT_StructureDamage.IsUsable(GetOwner())) return false;

		return ResolveTentRoot(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Blocked, with a reason, when the player has nothing to buy with.
	//!
	//! Only the two conditions a CLIENT can answer honestly are checked here. The price, the town's
	//! supporters and the player's balance are all server truth and are re-derived there - this is a
	//! courtesy, never a security boundary, and every one of these rules is enforced again in
	//! OVT_RecruitCommandComponent.ValidateTentPurchase.
	//! \param[in] user The acting character.
	//! \return True when the screen is worth opening.
	override bool CanBePerformedScript(IEntity user)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(!players) return false;

		string persId = players.GetPersistentIDFromControlledEntity(user);
		if(persId.IsEmpty()) return false;

		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(recruits && !recruits.CanRecruit(persId))
		{
			SetCannotPerformReason("#OVT-RecruitLimitReached");
			return false;
		}

		if(!HasAnyLoadouts(persId))
		{
			SetCannotPerformReason("#OVT-Recruit_NoLoadouts");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this player has saved any loadouts, answered at most once a second.
	//!
	//! Loadout NAMES are replicated (the contents are not), so this is answerable on a client and
	//! answers about that client's own loadouts only. It is cached because the lookup is
	//! disproportionately expensive for a per-frame question - see LOADOUT_CHECK_INTERVAL_MS.
	//! \param[in] persId The acting player's persistent id.
	//! \return True when the player has at least one saved loadout.
	protected bool HasAnyLoadouts(string persId)
	{
		float now = GetWorldTimeMs();

		// A cached answer is only reused while it is fresh AND while time is moving forward - a world
		// time that went backwards means a new world, and a stale answer with it.
		if(m_fLoadoutsCheckedAt > 0 && now >= m_fLoadoutsCheckedAt && now - m_fLoadoutsCheckedAt < LOADOUT_CHECK_INTERVAL_MS)
			return m_bHasLoadouts;

		m_fLoadoutsCheckedAt = now;
		m_bHasLoadouts = false;

		OVT_LoadoutManagerComponent loadouts = OVT_Global.GetLoadouts();
		if(!loadouts)
		{
			// No manager is not "no loadouts": refusing here would hide the action during JIP. Let the
			// screen open and let the server answer.
			m_bHasLoadouts = true;
			return true;
		}

		array<string> names = loadouts.GetAvailableLoadouts(persId);
		if(names && !names.IsEmpty())
			m_bHasLoadouts = true;

		return m_bHasLoadouts;
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

	//------------------------------------------------------------------------------------------------
	//! Walk up from the action's owner to the entity that IS the tent.
	//!
	//! The test is the server's test: an OVT_BuildableComponent declaring itself a recruitment tent.
	//! Matching on the component rather than on "the topmost parent" means this cannot start returning
	//! a scenery entity if the tent prefab's hierarchy is ever rearranged - it would return null
	//! instead, and the action would hide, which is the safe direction to fail in.
	//! \param[in] actionOwner The entity the action is authored on.
	//! \return The tent's root entity, or null when this is not part of a recruitment tent.
	protected IEntity ResolveTentRoot(IEntity actionOwner)
	{
		IEntity candidate = actionOwner;

		while(candidate)
		{
			OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(candidate.FindComponent(OVT_BuildableComponent));
			if(buildable && buildable.GetBuildableType() == OVT_RecruitCommandComponent.RECRUITMENT_TENT_TYPE)
				return candidate;

			candidate = candidate.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Opening a screen changes nothing anyone else can see.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
