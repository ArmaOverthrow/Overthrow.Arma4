modded class SCR_CharacterDamageManagerComponent : ScriptedDamageManagerComponent
{
	protected bool m_bCheckedFaction = false;
	protected bool m_bIsOccupyingFaction = false;
	
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
				
		if(IsOccupyingFaction())
			OVT_Global.GetOccupyingFaction().OnAIKilled(GetOwner(), instigator);
		
		if(instigator)
		{
			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(instigator.FindComponent(OVT_PlayerWantedComponent));
			
			if(wanted)
			{
				wanted.SetBaseWantedLevel(3);
			}
		}
		
	}
	
	protected bool IsOccupyingFaction()
	{
		if(!m_bCheckedFaction)
		{
			OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
			FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(GetOwner().FindComponent(FactionAffiliationComponent));
			Faction fac = aff.GetAffiliatedFaction();
			if(fac.GetFactionKey() == config.m_sOccupyingFaction)
			{
				m_bIsOccupyingFaction = true;
			}
			m_bCheckedFaction = true;
		}
		return m_bIsOccupyingFaction;
	}
	
	protected override void OnDamageStateChanged(EDamageState state)
	{
		super.OnDamageStateChanged(state);
		
		UpdateBloodyFace();
		
		if(IsOccupyingFaction())
			OVT_Global.GetOccupyingFaction().OnAIKilled(GetOwner(), null);
		
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