class OVT_SetHomeAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if (!CanBePerformedScript(pUserEntity))
		 	return;
		
		OVT_Global.GetServer().SetHome(SCR_PlayerController.GetLocalPlayerId());
				
		SCR_HintManagerComponent.ShowCustomHint("#OVT-HomeSet", "", 4);
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
	
	override bool CanBePerformedScript(IEntity user)
 	{		
		RplComponent genericRpl = RplComponent.Cast(user.FindComponent( RplComponent ));
		if (!genericRpl)
			return false;
		
		return genericRpl.IsOwner();
 	}
	
	override bool CanBeShownScript(IEntity user)
	{
		OVT_BaseControllerComponent baseController = EPF_Component<OVT_BaseControllerComponent>.Find(GetOwner());
		if (!baseController)
		{
			//is an FOB or camp
			return true;
		}
		return !baseController.IsOccupyingFaction();
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}