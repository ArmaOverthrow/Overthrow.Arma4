[BaseContainerProps()]
class OVT_ResistanceFactionStruct : OVT_BaseSaveStruct
{
	ref array<ref OVT_EntityStruct> placed = {};
	ref array<ref OVT_VectorStruct> fobs = {};
	ref array<ref OVT_VehicleStruct> built = {};
	ref array<string> officers = {};
	ref array<ref OVT_PlayerStruct> players = {};
	
	override bool Serialize()
	{
		placed.Clear();
		fobs.Clear();
		built.Clear();
		players.Clear();
		officers.Clear();
		
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
			struct.pos = p;
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
		
		foreach(string id : rf.m_Officers)
		{
			officers.Insert(id);
		}		
		
		PlayerManager mgr = GetGame().GetPlayerManager();
		
		OVT_PlayerManagerComponent playerMgr = OVT_Global.GetPlayers();
		for(int i = 0; i < playerMgr.m_mPlayers.Count(); i++)
		{
			OVT_PlayerData player = playerMgr.m_mPlayers.GetElement(i);
			string persId = playerMgr.m_mPlayers.GetKey(i);
			OVT_PlayerStruct struct = new OVT_PlayerStruct();
			if(player.id > -1)
			{				
				IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(player.id);
				if(controlledEntity)
				{
					struct.pos = controlledEntity.GetOrigin();
				}
			}else{
				struct.pos = player.location;
			}
			struct.id = persId;			
			struct.camp = player.camp;
			
			players.Insert(struct);
		}
		
		return true;
	}
	
	override bool Deserialize()
	{		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		OVT_PlayerManagerComponent pm = OVT_Global.GetPlayers();
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
		
		foreach(string id : officers)
		{
			rf.m_Officers.Insert(id);
		}
		
		foreach(OVT_PlayerStruct struct : players)
		{			
			OVT_PlayerData player = new OVT_PlayerData;
			player.id = -1;
			player.location = struct.pos;
			player.camp = struct.camp;
			pm.m_mPlayers[struct.id] = player;
		}
			
		return true;
	}
	
	void OVT_ResistanceFactionStruct()
	{		
		RegV("placed");
		RegV("fobs");
		RegV("built");
		RegV("officers");
		RegV("camps");
		RegV("players");
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

class OVT_PlayerStruct : SCR_JsonApiStruct
{
	string id;
	vector pos;
	vector camp;
	
	void OVT_PlayerStruct()
	{		
		RegV("id");
		RegV("pos");
		RegV("camp");
	}
}
