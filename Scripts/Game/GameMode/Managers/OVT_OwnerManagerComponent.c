class OVT_OwnerManagerComponentClass: OVT_ComponentClass
{
	
}

class OVT_OwnerManagerComponent: OVT_Component
{
	ref map<string, ref array<string>> m_mOwned;
	ref map<string, ref array<string>> m_mRented;
	ref map<string, string> m_mOwners;
	ref map<string, string> m_mRenters;
	
	protected ref array<IEntity> m_aEntitySearch;
	
	void OVT_OwnerManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mOwned = new map<string, ref array<string>>;
		m_mOwners = new map<string, string>;
		m_mRented = new map<string, ref array<string>>;
		m_mRenters = new map<string, string>;
		m_aEntitySearch = new array<IEntity>;
	}
	
	void SetOwner(int playerId, IEntity building)
	{		
		DoSetOwner(playerId, building.GetOrigin());		
		Rpc(RpcDo_SetOwner, playerId, building.GetOrigin());		
	}
	
	void SetOwnerPersistentId(string persId, IEntity building)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		DoSetOwnerPersistentId(persId, building.GetOrigin());		
		Rpc(RpcDo_SetOwner, playerId, building.GetOrigin());		
	}
	
	void SetRenter(int playerId, IEntity building)
	{		
		DoSetRenter(playerId, building.GetOrigin());		
		Rpc(RpcDo_SetRenter, playerId, building.GetOrigin());		
	}
	
	void SetRenterPersistentId(string persId, IEntity building)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		DoSetRenterPersistentId(persId, building.GetOrigin());		
		Rpc(RpcDo_SetRenter, playerId, building.GetOrigin());		
	}
	
	string GetOwnerID(IEntity building)
	{
		if(!building) return "";
		vector pos = building.GetOrigin();
		return GetOwnerIDFromPos(pos);
	}
	
	string GetOwnerIDFromPos(vector pos)
	{
		if(!m_mOwners.Contains(pos.ToString(false))) return "";
		return m_mOwners[pos.ToString(false)];
	}
	
	string GetRenterID(IEntity building)
	{
		if(!building) return "";
		vector pos = building.GetOrigin();
		return GetRenterIDFromPos(pos);
	}
	
	string GetRenterIDFromPos(vector pos)
	{
		if(!m_mRenters.Contains(pos.ToString(false))) return "";
		return m_mRenters[pos.ToString(false)];
	}
	
	bool IsOwner(string playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		if (!building) return false;
		array<string> owner = m_mOwned[playerId];
		return owner.Contains(building.GetOrigin().ToString(false));
	}
	
	bool IsRenter(string playerId, EntityID entityId)
	{
		if(!m_mRented.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		array<string> rented = m_mRented[playerId];
		return rented.Contains(building.GetOrigin().ToString(false));
	}
	
	bool IsOwned(EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		for(int i=0; i< m_mOwned.Count(); i++)
		{
			array<string> owner = m_mOwned.GetElement(i);
			if(owner.Contains(building.GetOrigin().ToString(false)))
			{
				return true;
			}
		}
		return false;
	}
	
	bool IsRented(EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		for(int i=0; i< m_mRented.Count(); i++)
		{
			array<string> rented = m_mRented.GetElement(i);
			if(rented.Contains(building.GetOrigin().ToString(false)))
			{
				return true;
			}
		}
		return false;
	}
	
	IEntity GetNearestBuilding(vector pos, float range = 40)
	{
		m_aEntitySearch.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, null, FilterBuildingToArray, EQueryEntitiesFlags.STATIC);
		
		if(m_aEntitySearch.Count() == 0)
		{
			return null;
		}
		float nearest = range;
		IEntity nearestEnt;	
		
		foreach(IEntity ent : m_aEntitySearch)
		{
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		return nearestEnt;
	}	
	
	bool FilterBuildingToArray(IEntity entity)
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity")
		{
			m_aEntitySearch.Insert(entity);
		}
		return false;
	}
	
	set<EntityID> GetOwned(string playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(string posString : m_mOwned[playerId])
		{
			vector pos = posString.ToVector();
			IEntity building = GetNearestBuilding(pos);
			entities.Insert(building.GetID());
		}
		return entities;
	}
	
	set<EntityID> GetRented(string playerId)
	{
		if(!m_mRented.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(string posString : m_mRented[playerId])
		{
			vector pos = posString.ToVector();
			IEntity building = GetNearestBuilding(pos);
			entities.Insert(building.GetID());
		}
		return entities;
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{		
		
		//Send JIP owned
		writer.WriteInt(m_mOwned.Count()); 
		for(int i=0; i<m_mOwned.Count(); i++)
		{		
			array<string> ownedArray = m_mOwned.GetElement(i);
			writer.WriteString(m_mOwned.GetKey(i));			
			writer.WriteInt(ownedArray.Count());
			for(int t=0; t<ownedArray.Count(); t++)
			{	
				writer.WriteVector(ownedArray[t].ToVector());
			}
		}
		
		//Send JIP rented
		writer.WriteInt(m_mRented.Count()); 
		for(int i=0; i<m_mRented.Count(); i++)
		{		
			array<string> rentedArray = m_mRented.GetElement(i);
			writer.WriteString(m_mRented.GetKey(i));			
			writer.WriteInt(rentedArray.Count());
			for(int t=0; t<rentedArray.Count(); t++)
			{	
				writer.WriteVector(rentedArray[t].ToVector());
			}
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{	
		
		int length, ownedlength;
		string playerId;
		vector pos;
			
		//Recieve JIP owned
		if (!reader.ReadInt(length)) return false;
		
		for(int i=0; i<length; i++)
		{
			if (!reader.ReadString(playerId)) return false;		
			
			if (!reader.ReadInt(ownedlength)) return false;			
			for(int t=0; t<ownedlength; t++)
			{
				if (!reader.ReadVector(pos)) return false;				
				DoSetOwnerPersistentId(playerId, pos);
			}		
		}
		
		//Recieve JIP rented
		if (!reader.ReadInt(length)) return false;
		
		for(int i=0; i<length; i++)
		{
			if (!reader.ReadString(playerId)) return false;
			
			if (!reader.ReadInt(ownedlength)) return false;
			for(int t=0; t<ownedlength; t++)
			{
				if (!reader.ReadVector(pos)) return false;
				DoSetRenterPersistentId(playerId, pos);
			}		
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOwner(int playerId, vector pos)
	{
		DoSetOwner(playerId, pos);	
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetRenter(int playerId, vector pos)
	{
		DoSetRenter(playerId, pos);	
	}
	
	void DoSetOwner(int playerId, vector pos)
	{
		if(playerId == -1) {
			DoRemoveOwner(pos);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetOwnerPersistentId(persId, pos);
		}
	}
	
	void DoSetRenter(int playerId, vector pos)
	{
		if(playerId == -1) {
			DoRemoveRenter(pos);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetRenterPersistentId(persId, pos);
		}
	}
	
	void DoRemoveOwner(vector pos)
	{				
		string persId = GetOwnerIDFromPos(pos);
		
		if (!m_mOwned.Contains(persId)) {
			return;
		}
		
		int i = m_mOwned[persId].Find(pos.ToString(false));
		if(i == -1) return;
		m_mOwned[persId].Remove(i);
		m_mOwners.Remove(pos.ToString(false));
	}
	
	void DoRemoveRenter(vector pos)
	{		
		string persId = GetRenterIDFromPos(pos);
		
		if (!m_mRented.Contains(persId)) {
			return;
		}
		
		int i = m_mRented[persId].Find(pos.ToString(false));
		if(i == -1) return;
		m_mRented[persId].Remove(i);
		m_mRenters.Remove(pos.ToString(false));
	}
	
	void DoSetOwnerPersistentId(string persId, vector pos)
	{		
		if(!m_mOwned.Contains(persId)) m_mOwned[persId] = new array<string>;
		array<string> owner = m_mOwned[persId];
		owner.Insert(pos.ToString(false));
		
		m_mOwners[pos.ToString(false)] = persId;
	}
	
	void DoSetRenterPersistentId(string persId, vector pos)
	{
		if(!m_mRented.Contains(persId)) m_mRented[persId] = new array<string>;
		array<string> renter = m_mRented[persId];
		renter.Insert(pos.ToString(false));
		
		m_mRenters[pos.ToString(false)] = persId;
	}
	
	void ~OVT_OwnerManagerComponent()
	{
		if(m_mOwned)
		{
			m_mOwned.Clear();
			m_mOwned = null;
		}
		if(m_mRented)
		{
			m_mRented.Clear();
			m_mRented = null;
		}
		if(m_mOwners)
		{
			m_mOwners.Clear();
			m_mOwners = null;
		}
		if(m_mRenters)
		{
			m_mRenters.Clear();
			m_mRenters = null;
		}
	}
}