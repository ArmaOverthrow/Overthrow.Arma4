[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "")]
class OVT_PlayerWantedComponentClass: ScriptComponentClass
{}

class OVT_PlayerWantedComponent: ScriptComponent
{
	[Attribute("0", params: "0 5 1", desc: "The current wanted level of this character")]
	protected int m_iWantedLevel = 0;
	protected bool m_bIsSeen = false;	
	protected int m_iWantedTimer = 0;
	
	protected const int LAST_SEEN_MAX = 10000;	
	protected const int WANTED_TIMEOUT = 15000;
	protected const int WANTED_SYSTEM_FREQUENCY = 1000;
	
	protected FactionAffiliationComponent m_Faction;
	
	void SetWantedLevel(int level)
	{
		m_iWantedLevel = level;
	}
	
	int GetWantedLevel()
	{
		return m_iWantedLevel;
	}
	
	void CheckUpdate()
	{		
		m_bIsSeen = false;
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 1000, CheckEntity, FilterEntities, EQueryEntitiesFlags.ALL);
				
		Faction currentFaction = m_Faction.GetAffiliatedFaction();
		
		if(m_iWantedLevel > 0 && !m_bIsSeen)
		{
			m_iWantedTimer -= WANTED_SYSTEM_FREQUENCY;
			if(m_iWantedTimer <= 0)
			{
				m_iWantedLevel -= 1;
				m_iWantedTimer = WANTED_TIMEOUT;
			}
		}
		
		
		if(m_iWantedLevel > 1 && !currentFaction)
		{
			Print("You are wanted now");
			m_Faction.SetAffiliatedFactionByKey("FIA");
		}
		
		if(m_iWantedLevel < 2 && currentFaction)
		{
			Print("You are no longer wanted");
			m_Faction.SetAffiliatedFactionByKey("");
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
			m_bIsSeen = true;
			if(m_iWantedLevel > 1) return false;
			
			BaseWeaponManagerComponent wpnMgr = BaseWeaponManagerComponent.Cast(realEnt.FindComponent(BaseWeaponManagerComponent));		
			if (wpnMgr && wpnMgr.GetCurrentWeapon())
			{
				m_iWantedLevel = 2;
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
			
			if(currentFaction && currentFaction.GetFactionKey() == "USSR")
				return true;
		}
				
		return false;		
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_iWantedTimer = WANTED_TIMEOUT;
		
		m_Faction = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, WANTED_SYSTEM_FREQUENCY, true, owner);		
	}
	
	override void OnDelete(IEntity owner)
	{
		if (!Replication.IsServer())
			return;
		
		GetGame().GetCallqueue().Remove(CheckUpdate);
	}
}