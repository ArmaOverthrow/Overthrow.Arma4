[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_PlayerWantedComponentClass: OVT_ComponentClass
{}

class OVT_PlayerWantedComponent: OVT_Component
{
	[Attribute("0", params: "0 5 1", desc: "The current wanted level of this character")]
	protected int m_iWantedLevel;
	protected bool m_bIsSeen = false;	
	int m_iWantedTimer = 0;
	int m_iLastSeen;
	
	protected const int LAST_SEEN_MAX = 15;
	protected const int WANTED_SYSTEM_FREQUENCY = 1000;
	
	protected FactionAffiliationComponent m_Faction;
	protected BaseWeaponManagerComponent m_Weapon;
	
	void SetWantedLevel(int level)
	{
		m_iWantedLevel = level;
		Rpc(RpcAsk_SetWantedLevel, level);		
	}
	
	void SetBaseWantedLevel(int level)
	{
		if(m_iWantedLevel < level){
			m_iWantedLevel = level;
			Rpc(RpcAsk_SetWantedLevel, level);
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
				
		if(!GetRpl().IsOwner()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, WANTED_SYSTEM_FREQUENCY, true, owner);		
	}
	
	void CheckUpdate()
	{		
		m_bIsSeen = false;
		m_iLastSeen = LAST_SEEN_MAX;
		
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 250, CheckEntity, FilterEntities, EQueryEntitiesFlags.ALL);
				
		
		
		OVT_BaseControllerComponent base = OVT_OccupyingFactionManager.GetInstance().GetNearestBase(GetOwner().GetOrigin());
		if(base)
		{
			float distanceToBase = vector.Distance(base.GetOwner().GetOrigin(), GetOwner().GetOrigin());
			if(m_iWantedLevel < 2 && distanceToBase < base.m_iCloseRange && m_bIsSeen)
			{
				SetBaseWantedLevel(2);
			}		
		}
		
		//Print("Last seen is: " + m_iLastSeen);
		
		if(m_iWantedLevel > 0 && !m_bIsSeen)
		{
			m_iWantedTimer -= WANTED_SYSTEM_FREQUENCY;
			//Print("Wanted timeout tick -1");
			if(m_iWantedTimer <= 0)
			{				
				m_iWantedLevel -= 1;
				Print("Downgrading wanted level to " + m_iWantedLevel);
				if(m_iWantedLevel > 1)
				{
					m_iWantedTimer = m_Config.m_Difficulty.wantedTimeout;
				}else{
					m_iWantedTimer = m_Config.m_Difficulty.wantedOneTimeout;
				}
			}
		}else if(m_iWantedLevel == 1 && m_bIsSeen) {
			SetWantedLevel(2);			
		}
	}
	
	bool CheckEntity(IEntity entity)
	{				
		PerceptionComponent perceptComp = PerceptionComponent.Cast(entity.FindComponent(PerceptionComponent));
		if(!perceptComp) return true;
		
		BaseTarget possibleTarget = perceptComp.GetClosestTarget(ETargetCategory.FACTIONLESS, LAST_SEEN_MAX);
				
		IEntity realEnt = NULL;
		
		if (possibleTarget)
			realEnt = possibleTarget.GetTargetEntity();
		
		if (realEnt && realEnt == GetOwner())
		{
			
			int lastSeen = possibleTarget.GetTimeSinceSeen();
			
			if(lastSeen < m_iLastSeen) m_iLastSeen = lastSeen;			
			
			if(TraceLOS(entity, GetOwner())){
				//Player can be seen with direct Line of sight
				m_bIsSeen = true;
				if(m_iWantedLevel > 1) return false;
								
				if (m_Weapon && m_Weapon.GetCurrentWeapon())
				{
					//Player is brandishing a weapon
					m_iWantedLevel = 2;
				}	
			}
			return false;		
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
						
		autoptr TraceParam param = new TraceParam;
		param.Start = headPos;
		param.End = dest.GetOrigin();
		param.LayerMask = EPhysicsLayerDefs.Perception;
		param.Flags = TraceFlags.ENTS | TraceFlags.WORLD; 
		param.Exclude = source;
			
		float percent = GetGame().GetWorld().TraceMove(param, null);
			
		// If trace hits the target entity or travels the entire path, return true	
		GenericEntity ent = GenericEntity.Cast(param.TraceEnt);
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
		
		if (ent.FindComponent(AIControlComponent)){
			FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
				
			if(!faction) return false;
			
			Faction currentFaction = faction.GetAffiliatedFaction();
			
			if(currentFaction && currentFaction.GetFactionKey() == m_Config.m_sOccupyingFaction)
				return true;
		}
				
		return false;		
	}	
	
	override void OnDelete(IEntity owner)
	{		
		GetGame().GetCallqueue().Remove(CheckUpdate);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetWantedLevel(int level)
	{
		m_iWantedLevel = level;		
		
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
	}	
}