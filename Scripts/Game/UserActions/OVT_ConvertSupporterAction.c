class OVT_ConvertSupporterAction : ScriptedUserAction
{	
	bool m_bHasBeenConverted;
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if(m_bHasBeenConverted) return;
		
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		OVT_PlayerData player = OVT_PlayerData.Get(playerId);
		
		if(s_AIRandomGenerator.RandFloat01() < player.diplomacy)
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-ConvertedSupporter");
			OVT_Global.GetServer().AddSupporters(pOwnerEntity.GetOrigin(),1);
		}else{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-NotConvertedSupporter");
		}
		
		m_bHasBeenConverted = true;						
 	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		return !m_bHasBeenConverted;
	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
	
	override bool CanBeShownScript(IEntity user) {
		return !m_bHasBeenConverted;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; }
}