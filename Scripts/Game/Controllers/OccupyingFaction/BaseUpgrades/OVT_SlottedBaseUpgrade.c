class OVT_SlottedBaseUpgrade : OVT_BasePatrolUpgrade
{
	protected EntityID m_Spawned;
	protected vector m_vPos;
	
	private IEntity FindSlot(array<ref EntityID> slots)
	{
		if(slots.Count() == 0) return null;
		int i = 0;
		while(i < 30)
		{
			i++;
			int index = s_AIRandomGenerator.RandInt(0,slots.Count()-1);
			EntityID id = slots[index];
			if(!m_BaseController.m_aSlotsFilled.Contains(id))
			{
				return GetGame().GetWorld().FindEntityByID(id);
			}
		}		
		return null;
	}
	
	private IEntity NearestSlot(vector pos)
	{
		IEntity nearest;
		float nearestDist = -1;
		foreach(EntityID id : m_BaseController.m_AllSlots)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(pos, ent.GetOrigin());
			if(nearestDist == -1 || dist < nearestDist)
			{
				nearest = ent;
				nearestDist = dist;
			}
		}
		return nearest;
	}
	
	protected void RegisterFilledSlot(IEntity entity)
	{
		m_BaseController.m_aSlotsFilled.Insert(entity.GetID());
	}
	
	protected IEntity GetSmallSlot()
	{
		return FindSlot(m_BaseController.m_SmallSlots);
	}
	
	protected IEntity GetMediumSlot()
	{
		return FindSlot(m_BaseController.m_MediumSlots);
	}
	
	protected IEntity GetLargeSlot()
	{
		return FindSlot(m_BaseController.m_LargeSlots);
	}
	
	protected IEntity SpawnCompositionInSmallSlot(OVT_FactionComposition comp)
	{
		IEntity slot = GetSmallSlot();
		if(!slot) return null;
		
		return SpawnCompositionInSlot(slot, comp);
	}
	
	protected IEntity SpawnCompositionInMediumSlot(OVT_FactionComposition comp)
	{
		IEntity slot = GetMediumSlot();
		if(!slot) return null;
		
		return SpawnCompositionInSlot(slot, comp);
	}
	
	protected IEntity SpawnCompositionInLargeSlot(OVT_FactionComposition comp)
	{
		IEntity slot = GetLargeSlot();
		if(!slot) return null;
		
		return SpawnCompositionInSlot(slot, comp);
	}
	
	private IEntity SpawnCompositionInSlot(IEntity slot, OVT_FactionComposition comp)
	{
		if(comp.m_aPrefabs.Count() == 0) return null;
		
		IEntity spawn = SpawnInSlot(slot, comp.m_aPrefabs.GetRandomElement());
		if(!spawn) return null;
		
		SCR_AIWorld aiworld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		aiworld.RequestNavmeshRebuildEntity(spawn);
		
		m_Spawned = spawn.GetID();
		RegisterFilledSlot(slot);
		
		m_vPos = slot.GetOrigin();
		
		Setup();
		
		if(comp.m_aGroupPrefabs.Count() == 0) return spawn;
		
		BuyPatrol(0,comp.m_aGroupPrefabs.GetRandomElement(),spawn.GetOrigin());
		
		return spawn;
	}
	
	void Setup()
	{
	
	}
	
	override void AddWaypoints(SCR_AIGroup aigroup)
	{
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnDefendWaypoint(aigroup.GetOrigin()));
	}
	
	protected IEntity SpawnInSlot(IEntity slot, ResourceName res)
	{
		vector mat[4];
		slot.GetTransform(mat);		
		IEntity ent = OVT_Global.SpawnEntityPrefabMatrix(res, mat);
		return ent;
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		return null;
	}
	
	override bool Deserialize(OVT_BaseUpgradeData struct)	
	{		
		return true;
	}

}