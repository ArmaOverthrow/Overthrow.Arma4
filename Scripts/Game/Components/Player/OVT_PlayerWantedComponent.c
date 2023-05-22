[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_PlayerWantedComponentClass: OVT_ComponentClass
{}

class OVT_PlayerWantedComponent: OVT_Component
{	
	[RplProp()]
	protected int m_iWantedLevel = 0;
	[RplProp()]
	protected bool m_bIsSeen = false;	
	
	int m_iWantedTimer = 0;
	int m_iLastSeen;
	
	protected bool m_bTempSeen = false;
	
	protected const int LAST_SEEN_MAX = 15;
	protected const int WANTED_SYSTEM_FREQUENCY = 1000;
	
	protected FactionAffiliationComponent m_Faction;
	protected BaseWeaponManagerComponent m_Weapon;
	protected SCR_CharacterControllerComponent m_Character;
	protected SCR_CompartmentAccessComponent m_Compartment;
	
	protected ref TraceParam m_TraceParams;
	
	void SetWantedLevel(int level)
	{
		m_iWantedLevel = level;	
		Replication.BumpMe();
	}
	
	void SetBaseWantedLevel(int level)
	{
		if(m_iWantedLevel < level){
			m_iWantedLevel = level;
			Replication.BumpMe();
		}		
	}
	
	int GetWantedLevel()
	{
		return m_iWantedLevel;
	}
	
	bool IsSeen()
	{
		return m_bIsSeen;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if(SCR_Global.IsEditMode())
			return;		
		
		m_iWantedTimer = m_Config.m_Difficulty.wantedTimeout;
		
		m_Faction = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		m_Weapon = BaseWeaponManagerComponent.Cast(owner.FindComponent(BaseWeaponManagerComponent));
		m_Character = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		m_Compartment = SCR_CompartmentAccessComponent.Cast(owner.FindComponent(SCR_CompartmentAccessComponent));
				
		if(!GetRpl().IsOwner()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, WANTED_SYSTEM_FREQUENCY, true, owner);		
	}
	
	void CheckUpdate()
	{
		m_bTempSeen = false;
		m_iLastSeen = LAST_SEEN_MAX;
		
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 250, CheckEntity, FilterEntities, EQueryEntitiesFlags.DYNAMIC);
						
		OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(GetOwner().GetOrigin());
		if(base)
		{
			float distanceToBase = vector.Distance(base.location, GetOwner().GetOrigin());
			if(m_iWantedLevel < 2 && distanceToBase < base.closeRange && m_bTempSeen)
			{
				SetBaseWantedLevel(2);
			}		
		}
		
		OVT_RadioTowerData tower = OVT_Global.GetOccupyingFaction().GetNearestRadioTower(GetOwner().GetOrigin());
		if(tower)
		{
			float distanceToBase = vector.Distance(tower.location, GetOwner().GetOrigin());
			if(m_iWantedLevel < 2 && distanceToBase < 20 && m_bTempSeen)
			{
				SetBaseWantedLevel(2);
			}		
		}
		
		//Print("Last seen is: " + m_iLastSeen);
		
		if(m_iWantedLevel > 0 && !m_bTempSeen)
		{
			m_iWantedTimer -= WANTED_SYSTEM_FREQUENCY;
			//Print("Wanted timeout tick -1");
			if(m_iWantedTimer <= 0)
			{				
				m_iWantedLevel -= 1;
				//Print("Downgrading wanted level to " + m_iWantedLevel);
				if(m_iWantedLevel > 1)
				{
					m_iWantedTimer = m_Config.m_Difficulty.wantedTimeout;
				}else{
					m_iWantedTimer = m_Config.m_Difficulty.wantedOneTimeout;
				}
			}
		}else if(m_iWantedLevel == 1 && m_bTempSeen) {
			SetWantedLevel(2);			
		}
		
		Faction currentFaction = m_Faction.GetAffiliatedFaction();
		if(m_iWantedLevel > 1 && !currentFaction)
		{
			//Print("You are wanted now");
			m_Faction.SetAffiliatedFactionByKey(m_Config.m_sPlayerFaction);
		}
		
		if(m_iWantedLevel < 2 && currentFaction)
		{
			//Print("You are no longer wanted");
			m_Faction.SetAffiliatedFactionByKey("");
		}
		
		if(m_bTempSeen != m_bIsSeen)
		{
			m_bIsSeen = m_bTempSeen;
			Replication.BumpMe();
		}
	}
	
	bool CheckEntity(IEntity entity)
	{		
		//Search through every nearby enemy AI with a perception component and determine if we:
		//1. Are a known target
		//2. Can be seen in LOS
		//3. If the above is true, are we currently breaking any laws (ie open carrying a weapon)
				
		PerceptionComponent perceptComp = PerceptionComponent.Cast(entity.FindComponent(PerceptionComponent));
		if(!perceptComp) return true;
		
		if(perceptComp.GetTargetCount(ETargetCategory.FACTIONLESS) == 0) return true;
		
		array<BaseTarget> targets = new array<BaseTarget>;
		
		perceptComp.GetTargetsList(targets, ETargetCategory.FACTIONLESS);
		
		foreach(BaseTarget possibleTarget : targets)
		{		
			IEntity realEnt = possibleTarget.GetTargetEntity();
			
			if (realEnt && realEnt == GetOwner())
			{				
				int lastSeen = possibleTarget.GetTimeSinceSeen();
				
				if(lastSeen < m_iLastSeen) m_iLastSeen = lastSeen;	
				
				int newLevel = m_iWantedLevel;
				bool inVehicle = false;
				
				if(m_Compartment && m_Compartment.IsInCompartment())
				{		
					//Player is in a vehicle
					IEntity veh = m_Compartment.GetVehicle();
					if(veh && TraceLOSVehicle(entity, veh)){	
						//Vehicle can be seen					
						m_bTempSeen = true;
						inVehicle = true;
					}
				}else{				
					if(TraceLOS(entity, GetOwner())){
						//Player can be seen
						m_bTempSeen = true;	
					}
				}
				
				if(m_bTempSeen)
				{
					if (m_Weapon && m_Weapon.GetCurrentWeapon())
					{
						//Player is brandishing a weapon
						newLevel = 2;
					}	
					//To-Do: check for illegal attire (uniforms, etc)
					
					if(inVehicle)
					{
						IEntity veh = m_Compartment.GetVehicle();
						if(veh){
							//Check if vehicle is armed
							SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(veh.FindComponent(SCR_EditableVehicleComponent));
							if(editable)
							{
								SCR_UIInfo uiinfo = editable.GetInfo();
								if(uiinfo)
								{
									SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.Cast(uiinfo);
									if(info)
									{
										array<EEditableEntityLabel> entityLabels = new array<EEditableEntityLabel>;
										info.GetEntityLabels(entityLabels);
										if(entityLabels.Contains(EEditableEntityLabel.TRAIT_ARMED))
										{
											newLevel = 4;
										}
									}
								}
							}
						}
					}
					
					if(newLevel != m_iWantedLevel)
					{
						SetBaseWantedLevel(newLevel);						
					}
					
					//We can see and have checked this player, we don't need to continue the search
					return false; 
				}				
			}
		}
		
		//Continue search
		return true;
	}
	
	private bool TraceLOS(IEntity source, IEntity dest)
	{		
		int headBone = source.GetBoneIndex("head");
		vector matPos[4];
		
		source.GetBoneMatrix(headBone, matPos);
		vector headPos = source.CoordToParent(matPos[3]);
		
		headBone = dest.GetBoneIndex("head");		
		dest.GetBoneMatrix(headBone, matPos);
		vector destHead = dest.CoordToParent(matPos[3]);
		
		
		if (!m_TraceParams)
		{
			m_TraceParams = new TraceParam();		
			m_TraceParams.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_TraceParams.LayerMask =  EPhysicsLayerDefs.Perception;
		}						
		
		m_TraceParams.Start = headPos;
		m_TraceParams.End = destHead;		
		m_TraceParams.Exclude = source;
			
		float percent = GetGame().GetWorld().TraceMove(m_TraceParams, null);
			
		// If trace hits the target entity or travels the entire path, return true	
		GenericEntity ent = GenericEntity.Cast(m_TraceParams.TraceEnt);
		if (ent)
		{
			if ( ent == dest || ent.GetParent() == dest )
				return true;
		} 
		else if (percent == 1)
			return true;
				
		return false;
	}
	
	private bool TraceLOSVehicle(IEntity source, IEntity dest)
	{		
		int headBone = source.GetBoneIndex("head");
		vector matPos[4];
		
		source.GetBoneMatrix(headBone, matPos);
		vector headPos = source.CoordToParent(matPos[3]);		
		
		vector destVeh = dest.GetOrigin();		
		
		if (!m_TraceParams)
		{
			m_TraceParams = new TraceParam();		
			m_TraceParams.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_TraceParams.LayerMask =  EPhysicsLayerDefs.Perception;
		}						
		
		m_TraceParams.Start = headPos;
		m_TraceParams.End = destVeh;		
		m_TraceParams.Exclude = source;
			
		float percent = GetGame().GetWorld().TraceMove(m_TraceParams, null);
			
		// If trace hits the target entity or travels the entire path, return true	
		GenericEntity ent = GenericEntity.Cast(m_TraceParams.TraceEnt);
		if (ent)
		{
			if ( ent == dest || ent.GetParent() == dest )
				return true;
		} 
		else if (percent == 1)
			return true;
				
		return false;
	}
	
	bool FilterEntities(IEntity ent) 
	{			
		
		if(ent == GetOwner())
			return false;
		
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(ent.FindComponent(SCR_DamageManagerComponent));
		if(dmg && dmg.GetHealth() == 0)
		{
			//Is dead, ignore
			return false;
		}
		
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));			
		if(!faction) return false;
		
		Faction currentFaction = faction.GetAffiliatedFaction();
		
		if(currentFaction && currentFaction.GetFactionKey() == m_Config.m_sOccupyingFaction)
			return true;
				
		return false;		
	}	
	
	override void OnDelete(IEntity owner)
	{		
		GetGame().GetCallqueue().Remove(CheckUpdate);
	}
}