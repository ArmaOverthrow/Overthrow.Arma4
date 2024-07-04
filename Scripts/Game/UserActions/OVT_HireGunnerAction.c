class OVT_HireGunnerAction : ScriptedUserAction
{	
	protected ref array<IEntity> m_Vehicles;
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		int playerId = SCR_PlayerController.GetLocalPlayerId();
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		int cost = config.m_Difficulty.baseRecruitCost;
		
		SCR_BaseCompartmentManagerComponent comp = SCR_BaseCompartmentManagerComponent.Cast(pOwnerEntity.FindComponent(SCR_BaseCompartmentManagerComponent));
		if(!comp) return;
		
		array<BaseCompartmentSlot> slots = {};		
		comp.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
		if(slots.Count() == 0) return;
		
		BaseCompartmentSlot slot = slots[0];
		if(slot.GetOccupant())
		{
			SCR_HintManagerComponent.ShowCustomHint("#OVT-AlreadyOccupied");
			return;
		}
		
		OVT_TownData town = towns.GetNearestTown(pUserEntity.GetOrigin());
		if(town.support <= 0 || town.population <= 0)
		{
			SCR_HintManagerComponent.ShowCustomHint("#OVT-NoSupportersToRecruit");
			return;
		}
		
		if(!economy.LocalPlayerHasMoney(cost))
		{
			SCR_HintManagerComponent.ShowCustomHint("#OVT-CannotAfford");
			return;
		}
		
		economy.TakeLocalPlayerMoney(cost);
		OVT_Global.GetServer().SpawnGunner(pOwnerEntity, playerId); 
 	}
		
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}	
			
	override bool CanBeShownScript(IEntity user) {
		return true;
	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}