class OVT_RplOwnerManagerComponentClass: OVT_ComponentClass
{
	
}

//------------------------------------------------------------------------------------------------
//! Base class for a manager that manages the ownership and renting of replicated entities using RplId.
//! Keeps track of which player owns or rents which entity based on its RplId.
class OVT_RplOwnerManagerComponent: OVT_Component
{
	ref map<string, ref set<RplId>> m_mOwned; //!< Map of player persistent ID to a set of owned entity RplIds.
	ref map<string, ref set<RplId>> m_mRented; //!< Map of player persistent ID to a set of rented entity RplIds.
	ref map<ref RplId, string> m_mOwners; //!< Map of entity RplId to owner's persistent ID.
	ref map<ref RplId, string> m_mRenters; //!< Map of entity RplId to renter's persistent ID.
	ref map<ref RplId, vector> m_mLocations; //!< Map of entity RplId to its last known world position.
	
	//------------------------------------------------------------------------------------------------
	//! Constructor for the OVT_RplOwnerManagerComponent.
	void OVT_RplOwnerManagerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		m_mOwned = new map<string, ref set<RplId>>;
		m_mOwners = new map<ref RplId, string>;
		m_mRented = new map<string, ref set<RplId>>;
		m_mRenters = new map<ref RplId, string>;
		m_mLocations = new map<ref RplId, vector>;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the last known location of an entity by its RplId.
	//! \param[in] id The RplId of the entity.
	//! \return The vector position of the entity, or "0 0 0" if not found.
	vector GetLocationFromId(RplId id)
	{
		if(!m_mLocations.Contains(id)) return "0 0 0";
		return m_mLocations[id];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the owner of a building entity using its RplId. Also sends an RPC.
	//! \param[in] playerId The ID of the player who will own the building. -1 to remove ownership.
	//! \param[in] building The building entity to set the owner for.
	void SetOwner(int playerId, IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		
		RplId rplId = null;
		if(rpl != null) rplId = rpl.Id();
		
		DoSetOwner(playerId, rplId);		
		Rpc(RpcDo_SetOwner, playerId, rplId);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the owner of a building entity using the player's persistent ID and the entity's RplId. Also sends an RPC.
	//! \param[in] persId The persistent ID of the player who will own the building.
	//! \param[in] building The building entity to set the owner for.
	void SetOwnerPersistentId(string persId, IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		DoSetOwnerPersistentId(persId, rpl.Id());		
		Rpc(RpcDo_SetOwner, playerId, rpl.Id());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the renter of a building entity using its RplId. Also sends an RPC.
	//! \param[in] playerId The ID of the player who will rent the building. -1 to remove renter.
	//! \param[in] building The building entity to set the renter for.
	void SetRenter(int playerId, IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		DoSetRenter(playerId, rpl.Id());		
		Rpc(RpcDo_SetRenter, playerId, rpl.Id());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets the renter of a building entity using the player's persistent ID and the entity's RplId. Also sends an RPC.
	//! \param[in] persId The persistent ID of the player who will rent the building.
	//! \param[in] building The building entity to set the renter for.
	void SetRenterPersistentId(string persId, IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(persId);
		DoSetRenterPersistentId(persId, rpl.Id());		
		Rpc(RpcDo_SetRenter, playerId, rpl.Id());		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the persistent ID of the owner of a specific building entity via its RplId.
	//! \param[in] building The building entity.
	//! \return The persistent ID of the owner, or an empty string if not owned, building is null, or RplComponent not found.
	string GetOwnerID(IEntity building)
	{
		if(!building) return "";
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		if(!rpl) return "";
		if(!m_mOwners.Contains(rpl.Id())) return "";
		return m_mOwners[rpl.Id()];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the persistent ID of the renter of a specific building entity via its RplId.
	//! \param[in] building The building entity.
	//! \return The persistent ID of the renter, or an empty string if not rented, building is null, or RplComponent not found.
	string GetRenterID(IEntity building)
	{
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		if(!rpl) return "";
		if(!m_mRenters.Contains(rpl.Id())) return "";
		return m_mRenters[rpl.Id()];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a specific player owns a given entity via its RplId.
	//! \param[in] playerId The persistent ID of the player.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the player owns the entity, false otherwise.
	bool IsOwner(string playerId, EntityID entityId)
	{
		if(!m_mOwned.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		if (!building) return false;
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		if (!rpl) return false;
		set<RplId> owner = m_mOwned[playerId];
		return owner.Contains(rpl.Id());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a specific player rents a given entity via its RplId.
	//! \param[in] playerId The persistent ID of the player.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the player rents the entity, false otherwise.
	bool IsRenter(string playerId, EntityID entityId)
	{
		if(!m_mRented.Contains(playerId)) return false;
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		set<RplId> rented = m_mRented[playerId];
		return rented.Contains(rpl.Id());
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a given entity is owned by any player via its RplId.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the entity is owned, false otherwise.
	bool IsOwned(EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		for(int i=0; i< m_mOwned.Count(); i++)
		{
			set<RplId> owner = m_mOwned.GetElement(i);
			if(owner.Contains(rpl.Id()))
			{
				return true;
			}
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if a given entity is rented by any player via its RplId.
	//! \param[in] entityId The ID of the entity to check.
	//! \return True if the entity is rented, false otherwise.
	bool IsRented(EntityID entityId)
	{
		IEntity building = GetGame().GetWorld().FindEntityByID(entityId);
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		for(int i=0; i< m_mRented.Count(); i++)
		{
			set<RplId> rented = m_mRented.GetElement(i);
			if(rented.Contains(rpl.Id()))
			{
				return true;
			}
		}
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a set of EntityIDs for all entities owned by a specific player.
	//! \param[in] playerId The persistent ID of the player.
	//! \return A set containing the EntityIDs of owned entities. Returns an empty set if the player owns nothing.
	set<EntityID> GetOwned(string playerId)
	{
		if(!m_mOwned.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(RplId id : m_mOwned[playerId])
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if(!rpl) continue;
			entities.Insert(rpl.GetEntity().GetID());
		}
		return entities;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets a set of EntityIDs for all entities rented by a specific player.
	//! \param[in] playerId The persistent ID of the player.
	//! \return A set containing the EntityIDs of rented entities. Returns an empty set if the player rents nothing.
	set<EntityID> GetRented(string playerId)
	{
		if(!m_mRented.Contains(playerId)) return new set<EntityID>;
		set<EntityID> entities = new set<EntityID>;
		foreach(RplId id : m_mRented[playerId])
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			if(!rpl) continue;
			entities.Insert(rpl.GetEntity().GetID());
		}
		return entities;
	}
	
	//RPC Methods
	
	//------------------------------------------------------------------------------------------------
	//! Saves the ownership and rental data (using RplIds) for network synchronization (Join-in-Progress).
	//! \param[in,out] writer The bit writer to serialize data into.
	//! \return True on successful serialization.
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
				writer.WriteRplId(ownedArray[t]);
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
				writer.WriteRplId(rentedArray[t]);
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loads the ownership and rental data (using RplIds) received from the server (Join-in-Progress).
	//! \param[in] reader The bit reader to deserialize data from.
	//! \return True on successful deserialization.
	override bool RplLoad(ScriptBitReader reader)
	{	
		
		int length, ownedlength;
		string playerId;
		RplId id;
			
		//Recieve JIP owned
		if (!reader.ReadInt(length)) return false;
		
		for(int i=0; i<length; i++)
		{
			if (!reader.ReadString(playerId)) return false;		
			
			if (!reader.ReadInt(ownedlength)) return false;			
			for(int t=0; t<ownedlength; t++)
			{
				if (!reader.ReadRplId(id)) return false;				
				DoSetOwnerPersistentId(playerId, id);
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
				if (!reader.ReadRplId(id)) return false;
				DoSetRenterPersistentId(playerId, id);
			}		
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method called on all clients to set the owner of an entity using its RplId.
	//! \param[in] playerId The ID of the new owner player. -1 to remove ownership.
	//! \param[in] id The RplId of the entity.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOwner(int playerId, RplId id)
	{
		DoSetOwner(playerId, id);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method called on all clients to set the renter of an entity using its RplId.
	//! \param[in] playerId The ID of the new renter player. -1 to remove renter.
	//! \param[in] id The RplId of the entity.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetRenter(int playerId, RplId id)
	{
		DoSetRenter(playerId, id);	
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set or remove the owner based on player ID and entity RplId. Converts player ID to persistent ID.
	//! \param[in] playerId The ID of the player. -1 removes the owner.
	//! \param[in] id The RplId of the entity.
	protected void DoSetOwner(int playerId, RplId id)
	{
		if(playerId == -1) {
			DoRemoveOwner(id);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetOwnerPersistentId(persId, id);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set or remove the renter based on player ID and entity RplId. Converts player ID to persistent ID.
	//! \param[in] playerId The ID of the player. -1 removes the renter.
	//! \param[in] id The RplId of the entity.
	protected void DoSetRenter(int playerId, RplId id)
	{
		if(playerId == -1) {
			DoRemoveRenter(id);
		}else{
			string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
			DoSetRenterPersistentId(persId, id);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to remove the owner of the entity with the specified RplId.
	//! \param[in] id The RplId of the entity.
	protected void DoRemoveOwner(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if(!rpl) return;		
		
		string persId = GetOwnerID(rpl.GetEntity());
		
		if (!m_mOwned.Contains(persId)) {
			return;
		}
		
		int i = m_mOwned[persId].Find(id);
		if(i == -1) return;
		m_mOwned[persId].Remove(i);
		m_mOwners.Remove(id);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to remove the renter of the entity with the specified RplId.
	//! \param[in] id The RplId of the entity.
	protected void DoRemoveRenter(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if(!rpl) return;		
		
		string persId = GetRenterID(rpl.GetEntity());
		
		if (!m_mRented.Contains(persId)) {
			return;
		}
		
		int i = m_mRented[persId].Find(id);
		if(i == -1) return;
		m_mRented[persId].Remove(i);
		m_mRenters.Remove(id);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set the owner using the player's persistent ID and entity RplId. Updates internal maps and location cache.
	//! \param[in] persId The persistent ID of the player.
	//! \param[in] id The RplId of the entity.
	protected void DoSetOwnerPersistentId(string persId, RplId id)
	{		
		if(!m_mOwned.Contains(persId)) m_mOwned[persId] = new set<RplId>;
		set<RplId> owner = m_mOwned[persId];
		owner.Insert(id);
		
		m_mOwners[id] = persId;
		
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if(!rpl) return;
		m_mLocations[id] = rpl.GetEntity().GetOrigin();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Internal logic to set the renter using the player's persistent ID and entity RplId. Updates internal maps.
	//! \param[in] persId The persistent ID of the player.
	//! \param[in] id The RplId of the entity.
	protected void DoSetRenterPersistentId(string persId, RplId id)
	{
		if(!m_mRented.Contains(persId)) m_mRented[persId] = new set<RplId>;
		set<RplId> renter = m_mRented[persId];
		renter.Insert(id);
		
		m_mRenters[id] = persId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Destructor for the OVT_RplOwnerManagerComponent. Clears internal maps.
	void ~OVT_RplOwnerManagerComponent()
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
		if(m_mLocations)
		{
			m_mLocations.Clear();
			m_mLocations = null;
		}
	}
}