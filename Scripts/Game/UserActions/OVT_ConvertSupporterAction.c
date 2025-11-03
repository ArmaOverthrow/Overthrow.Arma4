class OVT_ConvertSupporterAction : OVT_BaseCivilianUserAction
{	
	//---------------------------------------------------------
 	override protected void PerformCivilianAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		
		if(s_AIRandomGenerator.RandFloat01() < player.diplomacy)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-ConvertedSupporter");
			OVT_Global.GetServer().AddSupporters(pOwnerEntity.GetOrigin(),1);
		}else{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NotConvertedSupporter");
		}
		
		MarkAsPerformed();					
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}
}