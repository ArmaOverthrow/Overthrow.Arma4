[BaseContainerProps()]
class OVT_ResistanceFactionStruct : OVT_BaseSaveStruct
{
	ref array<ref OVT_EntityStruct> placed = {};
	ref array<ref OVT_VectorStruct> fobs = {};
	ref array<ref OVT_VehicleStruct> built = {};
	
	override bool Serialize()
	{
		placed.Clear();
		fobs.Clear();
		built.Clear();
		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		
		foreach(EntityID id : rf.m_Placed)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(!ent) continue;
			OVT_EntityStruct struct = new OVT_EntityStruct();
			if(struct.Parse(ent, rdb))
				placed.Insert(struct);
		}
		
		foreach(vector p : rf.m_FOBs)
		{
			OVT_VectorStruct struct = new OVT_VectorStruct();
			fobs.Insert(struct);
		}
		
		foreach(EntityID id : rf.m_Built)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(!ent) continue;
			OVT_VehicleStruct struct = new OVT_VehicleStruct();
			if(struct.Parse(ent, rdb))
				built.Insert(struct);
		}
		
		return true;
	}
	
	override bool Deserialize()
	{		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		foreach(OVT_EntityStruct struct : placed)
		{			
			IEntity ent = struct.Spawn(rdb);
			rf.m_Placed.Insert(ent.GetID());
		}
		
		foreach(OVT_VectorStruct struct : fobs)
		{	
			rf.m_FOBs.Insert(struct.pos);
		}
		
		foreach(OVT_VehicleStruct struct : built)
		{			
			IEntity ent = struct.Spawn(rdb);
			rf.m_Built.Insert(ent.GetID());
		}
			
		return true;
	}
	
	void OVT_ResistanceFactionStruct()
	{		
		RegV("placed");
		RegV("fobs");
		RegV("built");
	}
}

class OVT_EntityStruct : SCR_JsonApiStruct
{
	int res;
	vector pos;
	vector ang;	
	ref array<int> inv = {};
	
	IEntity Spawn(array<string> resources)
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		vector mat[4];		
		Math3D.AnglesToMatrix(ang, mat);
		mat[3] = pos;
		
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;		
		spawnParams.Transform = mat;
		
		IEntity ent = GetGame().SpawnEntityPrefab(Resource.Load(resources[res]), GetGame().GetWorld(), spawnParams);
				
		InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
		if(invMgr)
		{
			foreach(int id : inv)
			{
				invMgr.TrySpawnPrefabToStorage(resources[id]);
			}
		}
		
		return ent;
	}
	
	bool Parse(IEntity ent, inout array<string> resources)
	{
		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(ent.FindComponent(SCR_DamageManagerComponent));
		if(dmg)
		{
			if(dmg.IsDestroyed()) return false;
		}		
		string resource = ent.GetPrefabData().GetPrefabName();
		
		res = resources.Find(resource);
		if(res == -1)
		{
			resources.Insert(resource);
			res = resources.Count() - 1;
		}		
		
		vector mat[4];
		ent.GetTransform(mat);
		ang = Math3D.MatrixToAngles(mat);
		pos = mat[3];
				
		InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
		if(invMgr)
		{
			array<IEntity> items = new array<IEntity>;
			int count = invMgr.GetItems(items);
			if(count > 0)
			{
				foreach(IEntity item : items)
				{
					string r = item.GetPrefabData().GetPrefabName();
					int id = resources.Find(r);
					if(id == -1)
					{
						resources.Insert(r);
						id = resources.Count() - 1;
					}
					inv.Insert(id);
				}
			}
		}
				
		return true;
	}
		
	void OVT_EntityStruct()
	{		
		RegV("res");
		RegV("pos");
		RegV("ang");		
		RegV("inv");
	}
}