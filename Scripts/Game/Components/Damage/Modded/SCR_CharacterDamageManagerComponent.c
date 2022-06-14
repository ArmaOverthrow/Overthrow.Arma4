modded class SCR_CharacterDamageManagerComponent : ScriptedDamageManagerComponent
{
	protected override void OnDamage(
				EDamageType type,
				float damage,
				HitZone pHitZone,
				IEntity instigator, 
				inout vector hitTransform[3], 
				float speed,
				int colliderID, 
				int nodeID)
	{
		super.OnDamage(type, damage, pHitZone, instigator, hitTransform, speed, colliderID, nodeID);
		
		UpdateBloodyFace();
		
		if(instigator)
		{
			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(instigator.FindComponent(OVT_PlayerWantedComponent));
			
			if(wanted)
			{
				wanted.SetBaseWantedLevel(2);
			}
		}
	}
	
	override void Kill(IEntity instigator = null)
	{
		super.Kill(instigator);
		
		OVT_Global.GetOccupyingFaction().m_OnAIKilled.Invoke(GetOwner());
		
		if(instigator)
		{
			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(instigator.FindComponent(OVT_PlayerWantedComponent));
			
			if(wanted)
			{
				wanted.SetBaseWantedLevel(3);
			}
		}
		
	}
	
	protected override void OnDamageStateChanged(EDamageState state)
	{
		super.OnDamageStateChanged(state);
		
		UpdateBloodyFace();
		
		OVT_Global.GetOccupyingFaction().m_OnAIKilled.Invoke(GetOwner());
		
		//Check immediate surrounds for a vehicle (hoping for a better way soon pls BI)
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 5, CheckVehicleSetWanted, FilterVehicleEntities, EQueryEntitiesFlags.ALL);
		
	}
	
	protected bool FilterVehicleEntities(IEntity entity)
	{
		if(entity.ClassName() == "Vehicle") return true;
		return false;
	}
	
	protected bool CheckVehicleSetWanted(IEntity entity)
	{
		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(entity.FindComponent(SCR_BaseCompartmentManagerComponent));
		
		array<IEntity> occupants = new array<IEntity>;
		
		mgr.GetOccupants(occupants);
				
		foreach(IEntity occupant : occupants)
		{	
			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(occupant.FindComponent(OVT_PlayerWantedComponent));
		
			if(wanted)
			{
				wanted.SetBaseWantedLevel(3);
			}
		}
		
		return true;
	}
}