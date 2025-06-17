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
		// Don't allow converting someone who is already a recruit
		if (IsRecruit(GetOwner()))
			return false;
			
		return !m_bHasBeenConverted;
	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
	
	override bool CanBeShownScript(IEntity user) {
		// Don't show for recruits
		if (IsRecruit(GetOwner()))
			return false;
			
		return !m_bHasBeenConverted;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; }
	
	//! Check if the entity is a recruit
	protected bool IsRecruit(IEntity entity)
	{
		if (!entity)
			return false;
			
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return false;
			
		return recruitManager.GetRecruitFromEntity(entity) != null;
	}
}