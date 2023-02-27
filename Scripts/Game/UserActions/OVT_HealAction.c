class OVT_HealAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if (!CanBePerformedScript(pUserEntity))
		 	return;
		
		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(pUserEntity.FindComponent( SCR_CharacterDamageManagerComponent ));
		if (!dmg)
			return;	
		
		dmg.SetHealthScaled(dmg.GetMaxHealth());
		
		SCR_HintManagerComponent.ShowCustomHint("#OVT-Healed", "", 4);
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
	
	override bool CanBePerformedScript(IEntity user)
 	{
		
		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(user.FindComponent( SCR_CharacterDamageManagerComponent ));
		if (!dmg)
			return false;
		
		if(dmg.GetHealth() >= dmg.GetMaxHealth()) return false;
		
		RplComponent genericRpl = RplComponent.Cast(user.FindComponent( RplComponent ));
		if (!genericRpl)
			return false;
		
		return genericRpl.IsOwner();
 	}
}