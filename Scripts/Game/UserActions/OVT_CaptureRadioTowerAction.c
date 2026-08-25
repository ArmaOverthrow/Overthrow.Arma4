//------------------------------------------------------------------------------------------------
//! Timed hold action that takes an occupying-faction radio tower for the resistance.
//!
//! 🔴 WHY IT EXISTS AT ALL (author, 2026-08-25): *"im here, its owned by them, theres noone here
//! (confirmed in GM). maybe there is a team walking here from somewhere but I cant see them."* The
//! automatic ground-control rule on OVT_OccupyingFactionManager counts REGISTERED members through the
//! virtualization core, and a group that has not materialised reports its RECORD position - which for
//! a recapture team is the tower it is still walking towards. So a tower can read as garrisoned while
//! the ground is visibly empty, and there was no way for the player to say otherwise. This action is
//! that way, and it asks the WORLD rather than the registry.
//!
//! ⚠ THE TWO GATES ARE THE AUTHOR'S, VERBATIM: disabled while wanted, and disabled while occupying
//! soldiers are within a reasonable distance. Both are re-checked on the server.
//!
//! ⚠ THE HOLD IS AUTHORED IN THE PREFAB, exactly as OVT_SabotageTowerAction's is - `Duration 20` in
//! the additionalActions block, not a constant here.
//------------------------------------------------------------------------------------------------
class OVT_CaptureRadioTowerAction : ScriptedUserAction
{
	//! How far from the tower an occupying soldier still counts as defending it. Wider than the
	//! action's own context radius on purpose: the question is "is this place contested", not "is
	//! somebody touching the mast".
	static const float DEFENDER_RADIUS_M = 100;

	//------------------------------------------------------------------------------------------------
	//! \param[in] pOwnerEntity The tower.
	//! \param[in] pUserEntity The acting player.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_TowerSabotageComponent towers = OVT_ControllerComponent<OVT_TowerSabotageComponent>.Get();
		if (!towers)
			return;

		towers.RequestCapture(pOwnerEntity.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Shown on occupying-held towers, to the resistance.
	//! \param[in] user The player looking at it.
	//! \return True when the action belongs here.
	override bool CanBeShownScript(IEntity user)
	{
		OVT_RadioTowerData tower = GetTower();
		if (!tower)
			return false;

		return tower.IsOccupyingFaction();
	}

	//------------------------------------------------------------------------------------------------
	//! Refused, with a stated reason, while the player is wanted or the tower is defended.
	//! \param[in] user The player performing it.
	//! \return True when the hold may start.
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		OVT_RadioTowerData tower = GetTower();
		if (!tower)
			return false;

		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(user.FindComponent(OVT_PlayerWantedComponent));
		if (wanted && wanted.GetWantedLevel() > 0)
		{
			SetCannotPerformReason("#OVT-CaptureTower_Wanted");
			return false;
		}

		if (OVT_ResistancePresence.IsGroundHeldByOccupying(tower.location, DEFENDER_RADIUS_M))
		{
			SetCannotPerformReason("#OVT-CaptureTower_Defended");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The tower this action's entity belongs to, or null when there is none in range.
	protected OVT_RadioTowerData GetTower()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if (!of)
			return null;

		return of.GetNearestRadioTower(GetOwner().GetOrigin());
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}
