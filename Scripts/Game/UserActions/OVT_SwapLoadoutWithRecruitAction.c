//------------------------------------------------------------------------------------------------
//! Timed hold action on one of YOUR OWN active recruits that exchanges your entire kit with theirs -
//! clothing, armour, rucksack, weapons and everything carried inside any of it.
//!
//! Shown only while the recruit is ACTIVE. A parked recruit is holding a position with the gear it
//! was left with, and taking that gear off it from across the fence is not a squad-management move;
//! bring it back into the squad first and the action appears.
//!
//! WHAT THIS SENDS IS AN RplId OF THE BODY, AND NOTHING ELSE. The other half of the swap - the
//! player's own character - is never named by the client: the server derives it from the controller
//! entity the request arrived on. Everything else (ownership, active state, liveness, distance) is
//! re-checked server-side in OVT_RecruitCommandComponent.ValidateSwapRequest, so this action's
//! visibility rules are a courtesy to the player, never a security boundary.
//!
//! NOTHING IS CREATED AND NOTHING IS DESTROYED by the operation this starts (decision D13): the
//! server moves the real item entities between the two inventories. The worst outcome the player can
//! see is an item on the ground at their feet, and the hint they get back says so.
//!
//! Duration is set PER PREFAB INSTANCE (5 s), not here - Duration is a native BaseUserAction config
//! field and script only gets to read it (BaseUserAction.c:68).
//------------------------------------------------------------------------------------------------
class OVT_SwapLoadoutWithRecruitAction : OVT_BaseRecruitUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if(!pOwnerEntity) return;

		OVT_RecruitCommandComponent commands = OVT_Global.GetRecruitCommands();
		if(!commands) return;

		// The body has to be nameable across the wire. A recruit body always carries an RplComponent
		// (it is a replicated character), but this runs on clients during JIP where anything can be
		// half-built, and a null here is a silent no-op rather than a crash.
		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if(!rpl) return;

		commands.RequestSwapLoadout(rpl.Id());
	}

	//------------------------------------------------------------------------------------------------
	//! Only on a recruit that is following you.
	override protected bool CanShowRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		return !recruit.m_bInactive;
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors the server's liveness refusal so a five-second hold is not spent on a request that was
	//! never going to be accepted.
	//!
	//! Visible-with-a-reason rather than hidden, following OVT_SabotageTowerAction's rule: a recruit
	//! bleeding out at your feet is exactly when a player needs to be told WHY the swap is off, and an
	//! action that vanishes says nothing.
	override protected bool CanPerformRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		if(!IsAliveAndConscious(GetOwner()))
		{
			SetCannotPerformReason("#OVT-Recruit_CannotSwapDown");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a character is up and awake.
	//!
	//! Same pair of reads as OVT_RecruitCommandComponent.IsAliveAndConscious, deliberately in step
	//! with it: the check the action shows and the check the server enforces have to agree, or the
	//! player gets an action that always refuses.
	//! \param[in] entity The character to test.
	//! \return True when it is alive and conscious.
	protected bool IsAliveAndConscious(IEntity entity)
	{
		if(!entity) return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if(!controller) return false;

		if(controller.GetLifeState() != ECharacterLifeState.ALIVE) return false;

		return !controller.IsUnconscious();
	}
}
