//------------------------------------------------------------------------------------------------
//! "Treat wounds" - the held action on a medical bed (or the medical tent itself) that fully heals
//! the player.
//!
//! NOTHING HERE IS AUTHORITY. It used to be: the old version called SetHealthScaled() straight onto
//! the local damage manager, which restored the health bar on the performing client and cured nothing
//! else - not a bleeding, not a broken limb, not the persistent effects driving either - because a
//! proxy's damage manager is not the one the game reads and the default hit zone is not the whole
//! character. OVT_ResistanceRequestComponent.RpcAsk_HealPlayer re-resolves the entity, re-tests the
//! ruin gate and the proximity, and calls FullHeal() on the server.
//!
//! The gates below decide only what the player SEES. CanBeHealed() is vanilla's own "is there any
//! physical damage or a bleeding" test, so the action greys out when there is genuinely nothing to
//! treat rather than when the health bar alone happens to be full.
//!
//! There is no script-side hold duration - the Duration is authored in each prefab's additionalActions
//! block, the same rule OVT_RepairStructureAction follows.
//------------------------------------------------------------------------------------------------
class OVT_HealAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Ask the server to heal us here.
	//! \param[in] pOwnerEntity The bed or structure the action lives on.
	//! \param[in] pUserEntity The performing character.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanBePerformedScript(pUserEntity))
			return;

		RplComponent rpl = ResolveRpl(pOwnerEntity);
		if (!rpl)
			return;

		OVT_ResistanceRequestComponent requests = OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get();
		if (!requests)
			return;

		requests.HealPlayer(rpl.Id());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] user The character looking at the action.
	//! \return False only while the structure this sits on is a ruin.
	override bool CanBeShownScript(IEntity user)
	{
		// PHASE-0 GATE (core/damage D15): a ruin offers nothing. IsUsable() walks to the ROOT, so this
		// answers for the tent even when the action is mounted on one of its cots.
		return OVT_StructureDamage.IsUsable(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] user The character looking at the action.
	//! \return True while this machine's own character has something to treat and the bed is nameable
	//!         across the network.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(user.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
			return false;

		// Vanilla's own predicate: any physical damage OR any bleeding. Testing health alone is what
		// let a bleeding player with a full bar see the action greyed out.
		if (!damage.CanBeHealed())
			return false;

		RplComponent userRpl = RplComponent.Cast(user.FindComponent(RplComponent));
		if (!userRpl || !userRpl.IsOwner())
			return false;

		return ResolveRpl(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The RplComponent that names this action's owner to the server.
	//!
	//! A cot is a static child of the tent and carries no RplComponent of its own, so the id sent is
	//! the ROOT's - the same entity the server prices, gates and range-checks.
	//! \param[in] entity The entity the action lives on.
	//! \return Its own or its root's RplComponent, or null.
	protected RplComponent ResolveRpl(IEntity entity)
	{
		if (!entity)
			return null;

		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (rpl)
			return rpl;

		IEntity root = entity.GetRootParent();
		if (!root || root == entity)
			return null;

		return RplComponent.Cast(root.FindComponent(RplComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The heal itself is a server request, so the action performs nowhere but here.
	override bool HasLocalEffectOnlyScript() { return true; };
}
