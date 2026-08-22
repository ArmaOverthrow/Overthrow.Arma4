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
	
	override bool CanBeShownScript(IEntity user)
	{
		// PHASE-0 GATE (core/damage D15): a ruin offers nothing. IsUsable() answers true for every
		// owner that is not a retrofitted structure, so this costs the other contexts nothing.
		return OVT_StructureDamage.IsUsable(GetOwner());
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
	
	override bool HasLocalEffectOnlyScript() { return true; };
}