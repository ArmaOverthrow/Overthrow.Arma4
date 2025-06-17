[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_PlayerWantedComponentClass: OVT_ComponentClass
{}

class OVT_PlayerWantedComponent: OVT_Component
{	
	[RplProp()]
	protected int m_iWantedLevel = 0;
	[RplProp()]
	protected bool m_bIsSeen = false;	
	[RplProp()]
	float m_fVisualRecognitionFactor = 1;
	[RplProp()]
	float m_bWantedSystemEnabled = true;
	
	[Attribute("250")]
	float m_fBaseDistanceSeenAt;
	
	int m_iWantedTimer = 0;
	int m_iLastSeen;
	
	protected bool m_bTempSeen = false;
	
	protected const int LAST_SEEN_MAX = 15;
	protected const int WANTED_SYSTEM_FREQUENCY = 1000;
	
	protected FactionAffiliationComponent m_Faction;
	protected BaseWeaponManagerComponent m_Weapon;
	protected SCR_CharacterControllerComponent m_Character;
	protected SCR_CompartmentAccessComponent m_Compartment;
	protected CharacterPerceivableComponent m_Percieve;
	
	
	
	protected ref TraceParam m_TraceParams;
	
	protected ref OVT_PlayerData m_PlayerData;
	protected ref OVT_RecruitData m_RecruitData;
	
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
	
	//! Enable the wanted system for this entity
	void EnableWantedSystem()
	{
		m_bWantedSystemEnabled = true;
	}
	
	//! Disable the wanted system for this entity
	void DisableWantedSystem()
	{
		m_bWantedSystemEnabled = false;
		// Reset wanted level when disabling
		SetWantedLevel(0);
	}
	
	//! Check if wanted system is enabled
	bool IsWantedSystemEnabled()
	{
		return m_bWantedSystemEnabled;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if(SCR_Global.IsEditMode())
			return;		
		
		m_iWantedTimer = OVT_Global.GetConfig().m_Difficulty.wantedTimeout;
		
		m_Faction = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		m_Weapon = BaseWeaponManagerComponent.Cast(owner.FindComponent(BaseWeaponManagerComponent));
		m_Character = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		m_Compartment = SCR_CompartmentAccessComponent.Cast(owner.FindComponent(SCR_CompartmentAccessComponent));
		m_Percieve = CharacterPerceivableComponent.Cast(owner.FindComponent(CharacterPerceivableComponent));
						
		if(!GetRpl().IsOwner()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, WANTED_SYSTEM_FREQUENCY, true, owner);		
		
		OVT_Global.GetOccupyingFaction().m_OnPlayerLoot.Insert(OnPlayerLoot);

	}
	
	void OnPlayerLoot(IEntity player)
	{
		if(m_bIsSeen)
		{
			SetBaseWantedLevel(2);
			CheckWanted();
		}
	}
	
	void CheckUpdate()
	{
		if(!m_bWantedSystemEnabled) return;
		
		// Check if this is a player or recruit
		if(!m_PlayerData && !m_RecruitData)
		{
			int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(GetOwner());
			if(playerId > 0)
			{
				m_PlayerData = OVT_PlayerData.Get(playerId);
			}
			else
			{
				// Check if this is a recruit
				OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
				if(recruitManager)
				{
					m_RecruitData = recruitManager.GetRecruitFromEntity(GetOwner());
				}
				
				// If neither player nor recruit, exit
				if(!m_RecruitData)
					return;
			}
		}
		
		m_bTempSeen = false;
		m_iLastSeen = LAST_SEEN_MAX;
		
		m_fVisualRecognitionFactor = m_Percieve.GetVisualRecognitionFactor();
		
		//GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 250, CheckEntity, FilterEntities, EQueryEntitiesFlags.DYNAMIC);
		
		array<AIAgent> agents();
		
		AIWorld aiworld = GetGame().GetAIWorld();
		aiworld.GetAIAgents(agents);
		
		vector pos = GetOwner().GetOrigin();
		
		// Get stealth multiplier from player or recruit
		float stealthMultiplier = 1.0;
		if(m_PlayerData)
		{
			stealthMultiplier = m_PlayerData.stealthMultiplier;
		}
		else if(m_RecruitData)
		{
			// Recruits inherit stealth from their owner's skills
			OVT_PlayerData ownerData = OVT_Global.GetPlayers().GetPlayer(m_RecruitData.m_sOwnerPersistentId);
			if(ownerData)
			{
				stealthMultiplier = ownerData.stealthMultiplier;
			}
		}
		
		float distanceSeen = 5 + (m_fBaseDistanceSeenAt * stealthMultiplier); 
		
		foreach(AIAgent agent : agents)
		{
			AIGroup group = AIGroup.Cast(agent);
			if(!group) continue;
			
			array<AIAgent> groupAgents();
			group.GetAgents(groupAgents);
			
			foreach(AIAgent member : groupAgents)
			{			
				IEntity entity = member.GetControlledEntity();
				if(!entity) continue;
				float dist = vector.Distance(entity.GetOrigin(), pos);
				if(dist > distanceSeen) continue;
				if(FilterEntities(entity))
				{
					CheckEntity(entity);
				}
			}
		}		
						
		OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(GetOwner().GetOrigin());
		if(base && base.IsOccupyingFaction())
		{
			float distanceToBase = vector.Distance(base.location, GetOwner().GetOrigin());
			if(m_iWantedLevel < 2 && distanceToBase < OVT_Global.GetConfig().m_Difficulty.baseCloseRange && m_bTempSeen)
			{
				SetBaseWantedLevel(2);
			}		
		}
		
		OVT_RadioTowerData tower = OVT_Global.GetOccupyingFaction().GetNearestRadioTower(GetOwner().GetOrigin());
		if(tower && tower.IsOccupyingFaction())
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
				SetWantedLevel(m_iWantedLevel - 1);
				//Print("Downgrading wanted level to " + m_iWantedLevel);
				if(m_iWantedLevel > 1)
				{
					m_iWantedTimer = OVT_Global.GetConfig().m_Difficulty.wantedTimeout;
				}else{
					m_iWantedTimer = OVT_Global.GetConfig().m_Difficulty.wantedOneTimeout;
				}
			}
		}else if(m_iWantedLevel == 1 && m_bTempSeen) {
			SetWantedLevel(2);			
		}
		
		CheckWanted();
		
		if(m_bTempSeen != m_bIsSeen)
		{
			m_bIsSeen = m_bTempSeen;
			Replication.BumpMe();
		}
	}
	
	protected void CheckWanted()
	{		
		Faction currentFaction = m_Faction.GetAffiliatedFaction();
		string factionKey = currentFaction.GetFactionKey();
		
		if(m_iWantedLevel > 1 && factionKey == "CIV")
		{
			//Print("You are wanted now");
			if(OVT_Global.GetConfig().m_sPlayerFaction.IsEmpty()) OVT_Global.GetConfig().m_sPlayerFaction = "FIA";
			m_Faction.SetAffiliatedFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction);
		}
		
		if(m_iWantedLevel < 1 && factionKey != "CIV")
		{
			//Print("You are no longer wanted");
			m_Faction.SetAffiliatedFactionByKey("CIV");
		}
		
		if(m_Compartment && m_Compartment.IsInCompartment())
		{		
			//Player is in a vehicle, may need to update vehicle's faction
			SCR_VehicleFactionAffiliationComponent vfac = EPF_Component<SCR_VehicleFactionAffiliationComponent>.Find(m_Compartment.GetVehicle());
			Faction vehFaction = vfac.GetAffiliatedFaction();
			string vehFactionKey = vehFaction.GetFactionKey();
			if(m_iWantedLevel > 1 && vehFactionKey == "CIV")
			{
				if(OVT_Global.GetConfig().m_sPlayerFaction.IsEmpty()) OVT_Global.GetConfig().m_sPlayerFaction = "FIA";
				vfac.SetAffiliatedFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction);
			}
			if(m_iWantedLevel < 1 && vehFactionKey != "CIV")
			{
				vfac.SetAffiliatedFactionByKey("CIV");
			}
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
		
		if(perceptComp.GetTargetCount(ETargetCategory.FRIENDLY) == 0) return true;
		
		autoptr array<BaseTarget> targets = new array<BaseTarget>;
		
		perceptComp.GetTargetsList(targets, ETargetCategory.FRIENDLY);
		
		float dist = vector.Distance(GetOwner().GetOrigin(), entity.GetOrigin());
		
		//Is player in a vehicle?
		bool inVehicle = false;
		if(m_Compartment && m_Compartment.IsInCompartment())
		{		
			//Player is in a vehicle
			inVehicle = true;
		}		
		
		// Get stealth multiplier for this check
		float stealthMult = 1.0;
		if(m_PlayerData)
		{
			stealthMult = m_PlayerData.stealthMultiplier;
		}
		else if(m_RecruitData)
		{
			// Recruits inherit stealth from their owner's skills
			OVT_PlayerData ownerData = OVT_Global.GetPlayers().GetPlayer(m_RecruitData.m_sOwnerPersistentId);
			if(ownerData)
			{
				stealthMult = ownerData.stealthMultiplier;
			}
		}
		
		if(m_fVisualRecognitionFactor < 0.2 && dist > (10 * stealthMult) && !inVehicle)
		{
			//Definitely can't see you, but continue search
			return true;
		}
		
		foreach(BaseTarget possibleTarget : targets)
		{		
			IEntity realEnt = possibleTarget.GetTargetEntity();
			
			if (realEnt && realEnt == GetOwner())
			{				
				int lastSeen = possibleTarget.GetTimeSinceSeen();
				
				if(lastSeen < m_iLastSeen) m_iLastSeen = lastSeen;	
				
				int newLevel = m_iWantedLevel;
				
				if(inVehicle)
				{		
					//Player is in a vehicle
					IEntity veh = m_Compartment.GetVehicle();
					if(veh && TraceLOSVehicle(entity, veh)){	
						//Vehicle can be seen					
						m_bTempSeen = true;
					}
				}else{				
					if(TraceLOS(entity, GetOwner())){
						//Player can be seen
						m_bTempSeen = true;	
					}
				}
				
				if(m_bTempSeen)
				{					
					if (m_Weapon)
					{
						BaseWeaponComponent weapon = m_Weapon.GetCurrentWeapon();
						if(weapon){
							Print(weapon);
							//Player is brandishing a weapon
							Print("Weapon");
							newLevel = 2;
						}
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
										autoptr array<EEditableEntityLabel> entityLabels = new array<EEditableEntityLabel>;
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
		int headBone = source.GetAnimation().GetBoneIndex("head");
		vector matPos[4];
		
		source.GetAnimation().GetBoneMatrix(headBone, matPos);
		vector headPos = source.CoordToParent(matPos[3]);
		
		headBone = dest.GetAnimation().GetBoneIndex("head");		
		dest.GetAnimation().GetBoneMatrix(headBone, matPos);
		vector destHead = dest.CoordToParent(matPos[3]);
		
		
		if (!m_TraceParams)
		{
			m_TraceParams = new TraceParam();		
			m_TraceParams.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_TraceParams.LayerMask =  EPhysicsLayerDefs.Projectile;
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
		int headBone = source.GetAnimation().GetBoneIndex("head");
		vector matPos[4];
		
		source.GetAnimation().GetBoneMatrix(headBone, matPos);
		vector headPos = source.CoordToParent(matPos[3]);		
		
		vector destVeh = dest.GetOrigin();		
		
		if (!m_TraceParams)
		{
			m_TraceParams = new TraceParam();		
			m_TraceParams.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_TraceParams.LayerMask =  EPhysicsLayerDefs.Projectile;
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
		if (percent > 0.99)
			return true;
				
		return false;
	}
	
	bool FilterEntities(IEntity ent) 
	{			
		
		if(ent == GetOwner())
			return false;
		
		DamageManagerComponent dmg = DamageManagerComponent.Cast(ent.FindComponent(DamageManagerComponent));
		if(dmg && dmg.IsDestroyed())
		{
			//Is dead, ignore
			return false;
		}
		
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));			
		if(!faction) return false;
		
		Faction currentFaction = faction.GetAffiliatedFaction();
		
		if(currentFaction && currentFaction.GetFactionKey() == OVT_Global.GetConfig().m_sOccupyingFaction)
			return true;
				
		return false;		
	}	
	
	override void OnDelete(IEntity owner)
	{		
		GetGame().GetCallqueue().Remove(CheckUpdate);
	}
}
