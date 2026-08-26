modded class SCR_CharacterDamageManagerComponent : SCR_ExtendedDamageManagerComponent
{
	protected bool m_bCheckedFaction = false;
	protected bool m_bIsOccupyingFaction = false;
	protected bool m_bIsPlayerFaction = false;
	protected IEntity m_eLastInstigator;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		GetOnDamage().Insert(WhenDamaged);
		GetOnDamageStateChanged().Insert(WhenDamageStateChanged);
	}
	
	void WhenDamaged(BaseDamageContext damageContext)
	{		
		if(damageContext.instigator)
		{	
			IEntity entity = damageContext.instigator.GetInstigatorEntity();
			if(entity && !IsSelfInflicted(entity) && !IsPlayerFaction()) 
			{
				OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(entity.FindComponent(OVT_PlayerWantedComponent));
				
				if(wanted)
				{
					// Check if player was disguised - if so, blow their cover
					if(wanted.IsDisguisedAsOccupying())
					{
						wanted.SetBaseWantedLevel(2, "WantedDisguiseBlown");
						wanted.BlowDisguise();
					}
					else
					{
						// Not disguised, normal wanted level increase
						wanted.SetBaseWantedLevel(2);
					}
				}
			}
		}
	}
	
	

	void WhenDamageStateChanged(EDamageState state)
	{		
		// GetInstigator() is not guaranteed non-null - a death with no attributable source (drowning,
		// a despawned killer) would take the whole death path down with it.
		IEntity instigator;
		Instigator source = GetInstigator();
		if(source)
			instigator = source.GetInstigatorEntity();

		if(state == EDamageState.DESTROYED){
			// Fire universal character killed event for all characters regardless of faction
			OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
			if(gameMode)
			{
				gameMode.GetOnCharacterKilled().Invoke(GetOwner(), instigator);
			}
			
			if(IsOccupyingFaction())
			{
				OVT_Global.GetOccupyingFaction().OnAIKilled(GetOwner(), instigator);	
				
				//Check immediate surrounds for a vehicle (hoping for a better way soon pls BI)
				GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 5, CheckVehicleSetWanted, FilterVehicleEntities, EQueryEntitiesFlags.ALL);		
			}
			// The kill events above deliberately keep the real instigator - a man who kills himself is
			// still dead - but only a crime raises a wanted level.
			if(instigator && !IsSelfInflicted(instigator) && !IsPlayerFaction())
			{			
				OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(instigator.FindComponent(OVT_PlayerWantedComponent));
				
				if(wanted)
				{
					// Check if player was disguised - if so, blow their cover
					if(wanted.IsDisguisedAsOccupying())
					{
						wanted.SetBaseWantedLevel(3, "WantedDisguiseBlown");
						wanted.BlowDisguise();
					}
					else
					{
						// Not disguised, normal wanted level increase
						wanted.SetBaseWantedLevel(3);
					}
				}
			}
		}		
	}
	
	//! Whether a damage event is this character hurting HIMSELF - crashing his own car, falling, his own
	//! grenade going off at his feet. Self-harm is not a crime and must never raise a wanted level; the
	//! instigator of a car crash resolves to the driver, so without this every prang made you wanted.
	//! \param[in] instigatorEntity The entity the damage was attributed to.
	//! \return True when the victim and the instigator are the same character.
	protected bool IsSelfInflicted(IEntity instigatorEntity)
	{
		return instigatorEntity == GetOwner();
	}

	//! Resolves this character's side ONCE, answering both questions from the same lookup so they can
	//! never disagree.
	//!
	//! The cache is only marked done once it actually RESOLVED - the config and the affiliation are
	//! both things that can be missing early - so a call made too soon retries instead of freezing a
	//! wrong "no" for the character's whole life. A character with no affiliation component answers
	//! false to both rather than faulting, which the previous version did not.
	protected void CacheFaction()
	{
		if(m_bCheckedFaction)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config)
			return;

		FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(GetOwner().FindComponent(FactionAffiliationComponent));
		if(!aff)
			return;

		Faction fac = aff.GetAffiliatedFaction();
		if(!fac)
			return;

		string key = fac.GetFactionKey();
		m_bIsOccupyingFaction = key == config.m_sOccupyingFaction;
		m_bIsPlayerFaction = key == config.m_sPlayerFaction;
		m_bCheckedFaction = true;
	}

	protected bool IsOccupyingFaction()
	{
		CacheFaction();
		return m_bIsOccupyingFaction;
	}

	//! Whether the VICTIM is on the player's own side. Winging a comrade in a firefight is not something
	//! the occupiers would ever charge anybody with, and being shot at by your own side is not a reason
	//! for THEM to become wanted either.
	protected bool IsPlayerFaction()
	{
		CacheFaction();
		return m_bIsPlayerFaction;
	}
	
	protected bool FilterVehicleEntities(IEntity entity)
	{
		if(entity.ClassName() == "Vehicle") return true;
		return false;
	}
	
	protected bool CheckVehicleSetWanted(IEntity entity)
	{
		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(entity.FindComponent(SCR_BaseCompartmentManagerComponent));
		
		autoptr array<IEntity> occupants = new array<IEntity>;
		
		mgr.GetOccupants(occupants);
				
		foreach(IEntity occupant : occupants)
		{	
			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(occupant.FindComponent(OVT_PlayerWantedComponent));
		
			if(wanted)
			{
				// Check if player was disguised - if so, blow their cover
				if(wanted.IsDisguisedAsOccupying())
				{
					wanted.SetBaseWantedLevel(3, "WantedDisguiseBlown");
					wanted.BlowDisguise();
				}
				else
				{
					// Not disguised, normal wanted level increase
					wanted.SetBaseWantedLevel(3);
				}
			}
		}
		
		return true;
	}
}