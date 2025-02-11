class OVT_OwnerManagerComponentClass: OVT_ComponentClass
{
	
}

class OVT_OwnerManagerComponent: OVT_Component
{
	ref map<string, ref set<vector>> m_mOwned;
	ref map<string, ref set<vector>> m_mRented;
	ref map<vector, string> m_mOwners;
	ref map<vector, string> m_mRenters;
	
	void OVT_OwnerManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mOwned = new map<string, ref set<vector>>;
		m_mOwners = new map<vector, string>;
		m_mRented = new map<string, ref set<vector>>;
		m_mRenters = new map<vector, string>;
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
		if(!m_mOwners.Contains(pos)) return "";
		return m_mOwners[pos];
	}
	
	string GetRenterID(IEntity building)
	{
		if(!building) return "";
		vector pos = building.GetOrigin();
		return GetRenterIDFromPos(pos);
	}
	
	string GetRenterIDFromPos(vector pos)
	{
		if(!m_mRenters.Contains(pos)) return "";
		return m_mRenters[pos];
	}
	
	bool IsOwner(string playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		if (!building) return false;
		set<vector> owner = m_mOwned[playerId];
		return owner.Contains(building.GetOrigin());
	}
	
	bool IsRenter(string playerId, EntityID entityId)
	{
		if(!m_mRented.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		set<vector> rented = m_mRented[playerId];
		return rented.Contains(building.GetOrigin());
	}
	
	bool IsOwned(EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		for(int i=0; i< m_mOwned.Count(); i++)
		{
			set<vector> owner = m_mOwned.GetElement(i);
			if(owner.Contains(building.GetOrigin()))
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
			set<vector> rented = m_mRented.GetElement(i);
			if(rented.Contains(building.GetOrigin()))
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
	
	set<EntityID> GetOwned(string playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(vector pos : m_mOwned[playerId])
		{
			IEntity building = GetNearestBuilding(pos);
			entities.Insert(building.GetID());
		}
		return entities;
	}
	
	set<EntityID> GetRented(string playerId)
	{
		if(!m_mRented.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(vector pos : m_mRented[playerId])
		{
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
			set<RplId> ownedArray = m_mOwned.GetElement(i);
			writer.WriteString(m_mOwned.GetKey(i));			
			writer.WriteInt(ownedArray.Count());
			for(int t=0; t<ownedArray.Count(); t++)
			{	
				writer.WriteVector(ownedArray[t]);
			}
		}
		
		//Send JIP rented
		writer.WriteInt(m_mRented.Count()); 
		for(int i=0; i<m_mRented.Count(); i++)
		{		
			set<RplId> rentedArray = m_mRented.GetElement(i);
			writer.WriteString(m_mRented.GetKey(i));			
			writer.WriteInt(rentedArray.Count());
			for(int t=0; t<rentedArray.Count(); t++)
			{	
				writer.WriteVector(rentedArray[t]);
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
			DoRemoveOwner(id);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetOwnerPersistentId(persId, pos);
		}
	}
	
	void DoSetRenter(int playerId, vector pos)
	{
		if(playerId == -1) {
			DoRemoveRenter(id);
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
		
		int i = m_mOwned[persId].Find(pos);
		if(i == -1) return;
		m_mOwned[persId].Remove(i);
		m_mOwners.Remove(pos);
	}
	
	void DoRemoveRenter(vector pos)
	{		
		string persId = GetRenterIDFromPos(pos);
		
		if (!m_mRented.Contains(persId)) {
			return;
		}
		
		int i = m_mRented[persId].Find(id);
		if(i == -1) return;
		m_mRented[persId].Remove(i);
		m_mRenters.Remove(id);
	}
	
	void DoSetOwnerPersistentId(string persId, vector pos)
	{		
		if(!m_mOwned.Contains(persId)) m_mOwned[persId] = new set<vector>;
		set<vector> owner = m_mOwned[persId];
		owner.Insert(pos);
		
		m_mOwners[pos] = persId;
	}
	
	void DoSetRenterPersistentId(string persId, vector pos)
	{
		if(!m_mRented.Contains(persId)) m_mRented[persId] = new set<vector>;
		set<vector> renter = m_mRented[persId];
		renter.Insert(pos);
		
		m_mRenters[pos] = persId;
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