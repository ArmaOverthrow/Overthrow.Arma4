//------------------------------------------------------------------------------------------------
//! Timed hold action on occupying-faction radio towers. Completing it takes the tower off the air
//! for a difficulty-driven time (OVT_DifficultySettings.radioTowerSabotageTime), clearing the
//! tower's support penalty on nearby towns until it comes back up. Being seen doing it is illegal
//! (wanted level 2) - all validation and escalation is server-side in OVT_TowerSabotageComponent.
//!
//! While the tower is still down the action stays VISIBLE but cannot be performed, and its name
//! carries the time left (GetActionNameScript) so a player can see when it is worth coming back
//! rather than finding an action that silently vanished.
class OVT_SabotageTowerAction : ScriptedUserAction
{
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		OVT_TowerSabotageComponent sabotage = OVT_Global.GetTowerSabotage();
		if(!sabotage) return;

		sabotage.RequestSabotage(pOwnerEntity.GetOrigin());
 	}

	override bool CanBeShownScript(IEntity user)
	{
		OVT_RadioTowerData tower = GetTower();
		if(!tower) return false;

		return tower.IsOccupyingFaction();
	}

	//! A tower that is already down cannot be sabotaged again - the server refuses it too
	//! (OVT_TowerSabotageComponent.RpcAsk_SabotageTower), this just says so before the hold starts.
	override bool CanBePerformedScript(IEntity user)
	{
		OVT_RadioTowerData tower = GetTower();
		if(!tower) return false;

		if(tower.GetDisabledRemaining() > 0)
		{
			SetCannotPerformReason("#OVT-TowerAlreadySabotaged");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! While the tower is off the air the action name carries the time left, counted down live -
	//! OVT_RadioTowerData.GetDisabledRemaining() is what keeps that honest on a client, which only
	//! receives snapshots of the timer.
	//! \param[out] outName Replacement action name
	//! \return True when this action supplied a name, false to use the one configured on the prefab
	override bool GetActionNameScript(out string outName)
	{
		OVT_RadioTowerData tower = GetTower();
		if(!tower) return false;

		float remaining = tower.GetDisabledRemaining();
		if(remaining <= 0) return false;

		int totalSeconds = Math.Ceil(remaining);
		int minutes = totalSeconds / 60;
		int seconds = totalSeconds % 60;

		outName = "#OVT-TowerSabotaged (" + SCR_FormatHelper.GetTimeFormattingMinutesSeconds(minutes, seconds) + ")";
		return true;
	}

	//! The tower this action's entity belongs to, or null when there is none in range.
	protected OVT_RadioTowerData GetTower()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return null;

		return of.GetNearestRadioTower(GetOwner().GetOrigin());
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}
