//------------------------------------------------------------------------------------------------
//! Timed hold action on one of YOUR OWN recruits that parks it: out of your squad, holding the
//! position it stands on, still owned and still counted against your recruit cap.
//!
//! Shown only while the recruit is ACTIVE - the opposite action (OVT_SetRecruitActiveAction) takes
//! over from there, so the pair is never visible at the same time.
//!
//! ⚠ HIDDEN, NOT REFUSED, FOR A RECRUIT IN A VEHICLE - AND THAT CHANGED ON 2026-08-24. It used to
//! stay visible with a reason, following OVT_SabotageTowerAction's rule for a relevant-but-blocked
//! action. Play-test overruled it: a seated recruit's actions crowd the VEHICLE's own actions out of
//! the shared context menu, so OVT_BaseRecruitUserAction.CanBeShownScript() now hides every recruit
//! action on a body in a compartment. The reason string below is therefore UNREACHABLE in normal
//! play and is kept only as belt-and-braces behind the base rule.
//!
//! The rule it enforces is unchanged and still real: parking changes which AI group commands the
//! body, it does not move the body, so a recruit parked in a seat would be "holding position" inside
//! a vehicle that can then drive away with it. The server refuses it too
//! (OVT_RecruitCommandComponent.RESULT_IN_VEHICLE).
//------------------------------------------------------------------------------------------------
class OVT_SetRecruitInactiveAction : OVT_BaseRecruitUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_RecruitData recruit = GetRecruitRecord();
		if(!recruit) return;

		OVT_RecruitCommandComponent commands = OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get();
		if(!commands) return;

		commands.RequestSetInactive(recruit.m_sRecruitId, true);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool CanShowRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		return !recruit.m_bInactive;
	}

	//------------------------------------------------------------------------------------------------
	override protected bool CanPerformRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		if(IsInCompartment(GetOwner()))
		{
			SetCannotPerformReason("#OVT-Recruit_CannotParkInVehicle");
			return false;
		}

		return true;
	}
}
