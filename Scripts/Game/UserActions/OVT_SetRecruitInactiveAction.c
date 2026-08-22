//------------------------------------------------------------------------------------------------
//! Timed hold action on one of YOUR OWN recruits that parks it: out of your squad, holding the
//! position it stands on, still owned and still counted against your recruit cap.
//!
//! Shown only while the recruit is ACTIVE - the opposite action (OVT_SetRecruitActiveAction) takes
//! over from there, so the pair is never visible at the same time.
//!
//! REFUSED, NOT HIDDEN, FOR A RECRUIT IN A VEHICLE. Parking changes which AI group commands the
//! body; it does not move the body. A recruit parked in a seat would be "holding position" inside a
//! vehicle that can then drive away with it. The server refuses this too
//! (OVT_RecruitCommandComponent.RESULT_IN_VEHICLE) - this just says so before the hold starts,
//! following OVT_SabotageTowerAction's rule that a relevant-but-blocked action stays VISIBLE with a
//! reason rather than silently vanishing.
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
