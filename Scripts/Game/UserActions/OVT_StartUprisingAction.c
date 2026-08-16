class OVT_StartUprisingAction : ScriptedUserAction
{
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return;

		OVT_UprisingRequestComponent uprising = OVT_ControllerComponent<OVT_UprisingRequestComponent>.Get();
		if(!uprising)
		{
			Print("[Overthrow] No OVT_UprisingRequestComponent on the local controller - uprising request dropped", LogLevel.ERROR);
			return;
		}

		int townId = OVT_Global.GetTowns().GetNearestTownId(pOwnerEntity.GetOrigin());
		uprising.RequestStartUprising(townId);
 	}

	override bool CanBeShownScript(IEntity user)
	{
		if(OVT_Global.GetOccupyingFaction().m_bQRFActive) return false;

		OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(GetOwner().GetOrigin());
		if(!town) return false;

		// Villages flip peacefully through support, no battle to start
		if(town.size == OVT_TownSize.VILLAGE) return false;

		return town.IsOccupyingFaction();
	}

	override bool CanBePerformedScript(IEntity user)
	{
		OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(GetOwner().GetOrigin());
		if(!town) return false;

		if(town.SupportPercentage() <= OVT_OccupyingFactionManager.UPRISING_SUPPORT_THRESHOLD)
		{
			SetCannotPerformReason("#OVT-SupportTooLow");
			return false;
		}

		return true;
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}
