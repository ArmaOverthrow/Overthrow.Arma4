class OVT_ConvertSupporterAction : OVT_BaseCivilianUserAction
{
	//---------------------------------------------------------
 	override protected void PerformCivilianAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		//The diplomacy roll, validation and outcome hint are all server-side (BUG-063)
		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if(rpl)
		{
			OVT_ResistanceRequestComponent requests = OVT_ControllerComponent<OVT_ResistanceRequestComponent>.Get();
			if(requests)
				requests.ConvertSupporter(rpl.Id());
		}

		MarkAsPerformed();
 	}

	override bool GetActionNameScript(out string outName)
	{
		return false;
	}
}