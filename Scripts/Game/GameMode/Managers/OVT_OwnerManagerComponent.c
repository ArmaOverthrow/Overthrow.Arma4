class OVT_OwnerManagerComponentClass: OVT_ComponentClass
{
	
}

//------------------------------------------------------------------------------------------------
//! Base class for a manager that manages the ownership and renting of entities (ie buildings) within the game.
//! Keeps track of which player owns or rents which entity based on its position.
class OVT_OwnerManagerComponent: OVT_Component
{
	ref map<string, ref array<string>> m_mOwned; //!< Map of player persistent ID to an array of owned entity position strings.
	ref map<string, ref array<string>> m_mRented; //!< Map of player persistent ID to an array of rented entity position strings.
	ref map<string, string> m_mOwners; //!< Map of entity position string to owner's persistent ID.
	ref map<string, string> m_mRenters; //!< Map of entity position string to renter's persistent ID.
	
	protected ref array<IEntity> m_aEntitySearch; //!< Internal cache for entity search results.
	
	//------------------------------------------------------------------------------------------------
	//! Constructor for the OVT_OwnerManagerComponent.
	void OVT_OwnerManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mOwned = new map<string, ref array<string>>;
		m_mOwners = new map<string, string>;
		m_mRented = new map<string, ref array<string>>;
		m_mRenters = new map<string, string>;
		m_aEntitySearch = new array<IEntity>;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the owner of a building entity. Also sends an RPC to update other clients.
	//! \param[in] playerId The ID of the player who will own the building. -1 to remove ownership.
	//! \param[in] building The building entity to set the owner for.
	void SetOwner(int playerId, IEntity building)
	{		
		DoSetOwner(playerId, building.GetOrigin());		
		Rpc(RpcDo_SetOwner, playerId, building.GetOrigin());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the owner of a building entity using the player's persistent ID. Also sends an RPC.
	//! \param[in] persId The persistent ID of the player who will own the building.
	//! \param[in] building The building entity to set the owner for.
	void SetOwnerPersistentId(string persId, IEntity building)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		DoSetOwnerPersistentId(persId, building.GetOrigin());		
		Rpc(RpcDo_SetOwner, playerId, building.GetOrigin());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the renter of a building entity. Also sends an RPC to update other clients.
	//! \param[in] playerId The ID of the player who will rent the building. -1 to remove renter.
	//! \param[in] building The building entity to set the renter for.
	void SetRenter(int playerId, IEntity building)
	{		
		DoSetRenter(playerId, building.GetOrigin());		
		Rpc(RpcDo_SetRenter, playerId, building.GetOrigin());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the renter of a building entity using the player's persistent ID. Also sends an RPC.
	//! \param[in] persId The persistent ID of the player who will rent the building.
	//! \param[in] building The building entity to set the renter for.
	void SetRenterPersistentId(string persId, IEntity building)
	{
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		DoSetRenterPersistentId(persId, building.GetOrigin());		
		Rpc(RpcDo_SetRenter, playerId, building.GetOrigin());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the persistent ID of the owner of a specific building entity.
	//! \param[in] building The building entity.
	//! \return The persistent ID of the owner, or an empty string if not owned or building is null.
	string GetOwnerID(IEntity building)
	{
		if(!building) return "";
		vector pos = building.GetOrigin();
		return GetOwnerIDFromPos(pos);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the persistent ID of the owner of the entity at a specific position.
	//! \param[in] pos The world position of the entity.
	//! \return The persistent ID of the owner, or an empty string if no owner is registered at this position.
	string GetOwnerIDFromPos(vector pos)
	{
		if(!m_mOwners.Contains(pos.ToString(false))) return "";
		return m_mOwners[pos.ToString(false)];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the persistent ID of the renter of a specific building entity.
	//! \param[in] building The building entity.
	//! \return The persistent ID of the renter, or an empty string if not rented or building is null.
	string GetRenterID(IEntity building)
	{
		if(!building) return "";
		vector pos = building.GetOrigin();
		return GetRenterIDFromPos(pos);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the persistent ID of the renter of the entity at a specific position.
	//! \param[in] pos The world position of the entity.
	//! \return The persistent ID of the renter, or an empty string if no renter is registered at this position.
	string GetRenterIDFromPos(vector pos)
	{
		if(!m_mRenters.Contains(pos.ToString(false))) return "";
		return m_mRenters[pos.ToString(false)];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a specific player owns a given entity.
	//! \param[in] playerId The persistent ID of the player.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the player owns the entity, false otherwise.
	bool IsOwner(string playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		if (!building) return false;
		array<string> owner = m_mOwned[playerId];
		return owner.Contains(building.GetOrigin().ToString(false));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a specific player rents a given entity.
	//! \param[in] playerId The persistent ID of the player.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the player rents the entity, false otherwise.
	bool IsRenter(string playerId, EntityID entityId)
	{
		if(!m_mRented.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		array<string> rented = m_mRented[playerId];
		return rented.Contains(building.GetOrigin().ToString(false));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a given entity is owned by any player.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the entity is owned, false otherwise.
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
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a given entity is rented by any player.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the entity is rented, false otherwise.
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
	
	//------------------------------------------------------------------------------------------------
	//! Finds the nearest building entity to a given position within a specified range.
	//! Uses a sphere query and filters for "SCR_DestructibleBuildingEntity".
	//! \param[in] pos The center position for the search.
	//! \param[in] range The maximum search radius (default 40 meters).
	//! \return The nearest building entity found, or null if none are found within the range.
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
	
	//------------------------------------------------------------------------------------------------
	//! Filter function used by GetNearestBuilding to add "SCR_DestructibleBuildingEntity" types to the search results.
	//! \param[in] entity The entity being checked by the query.
	//! \return Always returns false to continue the query.
	protected bool FilterBuildingToArray(IEntity entity)
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity")
		{
			m_aEntitySearch.Insert(entity);
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a set of EntityIDs for all buildings owned by a specific player.
	//! Note: This relies on finding the nearest building to the stored position, which might not be perfectly accurate if buildings are very close.
	//! \param[in] playerId The persistent ID of the player.
	//! \return A set containing the EntityIDs of owned buildings. Returns an empty set if the player owns nothing.
	set<EntityID> GetOwned(string playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(string posString : m_mOwned[playerId])
		{
			vector pos = posString.ToVector();
			IEntity building = GetNearestBuilding(pos);
			if(building) // Add null check as GetNearestBuilding can return null
				entities.Insert(building.GetID());
		}
		return entities;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a set of EntityIDs for all buildings rented by a specific player.
	//! Note: This relies on finding the nearest building to the stored position, which might not be perfectly accurate if buildings are very close.
	//! \param[in] playerId The persistent ID of the player.
	//! \return A set containing the EntityIDs of rented buildings. Returns an empty set if the player rents nothing.
	set<EntityID> GetRented(string playerId)
	{
		if(!m_mRented.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(string posString : m_mRented[playerId])
		{
			vector pos = posString.ToVector();
			IEntity building = GetNearestBuilding(pos);
			if(building) // Add null check as GetNearestBuilding can return null
				entities.Insert(building.GetID());
		}
		return entities;
	}
	
	//RPC Methods
	
	//------------------------------------------------------------------------------------------------
	//! Saves the ownership and rental data for network synchronization (Join-in-Progress).
	//! \param[in,out] writer The bit writer to serialize data into.
	//! \return True on successful serialization.
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
	
	//------------------------------------------------------------------------------------------------
	//! Loads the ownership and rental data received from the server (Join-in-Progress).
	//! \param[in] reader The bit reader to deserialize data from.
	//! \return True on successful deserialization.
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
	
	//------------------------------------------------------------------------------------------------
	//! RPC method called on all clients to set the owner of an entity at a specific position.
	//! \param[in] playerId The ID of the new owner player. -1 to remove ownership.
	//! \param[in] pos The position of the entity.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOwner(int playerId, vector pos)
	{
		DoSetOwner(playerId, pos);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method called on all clients to set the renter of an entity at a specific position.
	//! \param[in] playerId The ID of the new renter player. -1 to remove renter.
	//! \param[in] pos The position of the entity.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetRenter(int playerId, vector pos)
	{
		DoSetRenter(playerId, pos);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set or remove the owner based on player ID. Converts player ID to persistent ID.
	//! \param[in] playerId The ID of the player. -1 removes the owner.
	//! \param[in] pos The position of the entity.
	protected void DoSetOwner(int playerId, vector pos)
	{
		if(playerId == -1) {
			DoRemoveOwner(pos);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetOwnerPersistentId(persId, pos);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set or remove the renter based on player ID. Converts player ID to persistent ID.
	//! \param[in] playerId The ID of the player. -1 removes the renter.
	//! \param[in] pos The position of the entity.
	protected void DoSetRenter(int playerId, vector pos)
	{
		if(playerId == -1) {
			DoRemoveRenter(pos);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetRenterPersistentId(persId, pos);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to remove the owner of the entity at the specified position.
	//! \param[in] pos The position of the entity.
	protected void DoRemoveOwner(vector pos)
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
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to remove the renter of the entity at the specified position.
	//! \param[in] pos The position of the entity.
	protected void DoRemoveRenter(vector pos)
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
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set the owner using the player's persistent ID. Updates internal maps.
	//! \param[in] persId The persistent ID of the player.
	//! \param[in] pos The position of the entity.
	protected void DoSetOwnerPersistentId(string persId, vector pos)
	{		
		if(!m_mOwned.Contains(persId)) m_mOwned[persId] = new array<string>;
		array<string> owner = m_mOwned[persId];
		owner.Insert(pos.ToString(false));
		
		m_mOwners[pos.ToString(false)] = persId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set the renter using the player's persistent ID. Updates internal maps.
	//! \param[in] persId The persistent ID of the player.
	//! \param[in] pos The position of the entity.
	protected void DoSetRenterPersistentId(string persId, vector pos)
	{
		if(!m_mRented.Contains(persId)) m_mRented[persId] = new array<string>;
		array<string> renter = m_mRented[persId];
		renter.Insert(pos.ToString(false));
		
		m_mRenters[pos.ToString(false)] = persId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Destructor for the OVT_OwnerManagerComponent. Clears internal maps.
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