class OVT_SlottedBaseUpgrade : OVT_BasePatrolUpgrade
{
	protected EntityID m_Spawned;
	
	private IEntity FindSlot(array<ref EntityID> slots)
	{
		foreach(EntityID id : slots)
		{
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
		float nearestDist = 9999999;
		foreach(EntityID id : m_BaseController.m_AllSlots)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(pos, ent.GetOrigin());
			if(dist < nearestDist)
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
		
		m_Spawned = spawn.GetID();
		RegisterFilledSlot(slot);
		
		if(comp.m_aGroupPrefabs.Count() == 0) return spawn;
		
		BuyPatrol(0,comp.m_aGroupPrefabs.GetRandomElement(),spawn.GetOrigin());
		
		return spawn;
	}
	
	override void AddWaypoints(SCR_AIGroup aigroup)
	{
		aigroup.AddWaypoint(SpawnDefendWaypoint(aigroup.GetOrigin()));
	}
	
	protected IEntity SpawnInSlot(IEntity slot, ResourceName res)
	{
		EntitySpawnParams spawn_params = EntitySpawnParams();
		spawn_params.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		slot.GetTransform(mat);
		spawn_params.Transform = mat;		
		IEntity ent = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), spawn_params);
		return ent;
	}
	
	override OVT_BaseUpgradeStruct Serialize(inout array<string> rdb)
	{
		OVT_BaseUpgradeStruct struct = super.Serialize(rdb);
		
		struct.resources = 0; //Do not respend any resources
		
		if(m_Spawned)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(m_Spawned);
			if(!ent) return struct;
			
			OVT_VehicleStruct veh = new OVT_VehicleStruct();
			if(veh.Parse(ent, rdb))
				struct.vehicles.Insert(veh);
		}
		
		return struct;
	}
	
	override bool Deserialize(OVT_BaseUpgradeStruct struct, array<string> rdb)	
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		foreach(OVT_VehicleStruct veh : struct.vehicles)
		{
			IEntity slot = NearestSlot(veh.pos);
			if(slot)
			{
				m_Spawned = SpawnInSlot(slot, rdb[veh.res]).GetID();
				RegisterFilledSlot(slot);
			}
		}
		
		return true;
	}
}