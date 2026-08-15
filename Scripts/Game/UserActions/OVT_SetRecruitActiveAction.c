//------------------------------------------------------------------------------------------------
//! Timed hold action on one of YOUR OWN parked recruits that brings it back into your squad.
//!
//! Shown only while the recruit is INACTIVE - the opposite action (OVT_SetRecruitInactiveAction)
//! covers the other half, so the pair is never visible at the same time.
//!
//! NO IN-VEHICLE GUARD, DELIBERATELY. Reactivation puts the recruit back under its owner's command
//! without moving the body, and an owner reactivating a recruit is normally standing (or sitting)
//! right next to it. The guard on the parking action exists because a PARKED recruit in a seat is
//! stranded; a following one is not.
//------------------------------------------------------------------------------------------------
class OVT_SetRecruitActiveAction : OVT_BaseRecruitUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_RecruitData recruit = GetRecruitRecord();
		if(!recruit) return;

		OVT_RecruitCommandComponent commands = OVT_ControllerComponent<OVT_RecruitCommandComponent>.Get();
		if(!commands) return;

		commands.RequestSetInactive(recruit.m_sRecruitId, false);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool CanShowRecruitAction(IEntity user, notnull OVT_RecruitData recruit)
	{
		return recruit.m_bInactive;
	}
}
