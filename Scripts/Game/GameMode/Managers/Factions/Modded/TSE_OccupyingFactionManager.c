modded class OVT_OccupyingFactionManager
{
	//------------------------------------------------------------------------------------------------
	//! Enhanced RplSave to include faction resources and threat for client synchronization
	override bool RplSave(ScriptBitWriter writer)
	{
		// Call original RplSave first
		if (!super.RplSave(writer))
			return false;
			
		// Add faction resources and threat synchronization
		writer.WriteInt(m_iResources);
		writer.WriteFloat(m_iThreat);
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced RplLoad to receive faction resources and threat from server
	override bool RplLoad(ScriptBitReader reader)
	{
		// Call original RplLoad first
		if (!super.RplLoad(reader))
			return false;
			
		// Read faction resources and threat from server
		if (!reader.ReadInt(m_iResources)) return false;
		if (!reader.ReadFloat(m_iThreat)) return false;
		
		Print(string.Format("TSE_OccupyingFactionManager: Received faction data from server - Resources: %1, Threat: %2", m_iResources, m_iThreat));
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to update faction resources on all clients
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_UpdateFactionResources(int resources, float threat)
	{
		m_iResources = resources;
		m_iThreat = threat;
		
		Print(string.Format("TSE_OccupyingFactionManager: Updated faction data via RPC - Resources: %1, Threat: %2", m_iResources, m_iThreat));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced GainResources to broadcast updates to clients
	override int GainResources()
	{
		int gained = super.GainResources();
		
		// Broadcast the updated values to all clients
		if (Replication.IsServer() && gained > 0)
		{
			Rpc(RpcDo_UpdateFactionResources, m_iResources, m_iThreat);
		}
		
		return gained;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced OnAIKilled to broadcast threat updates to clients
	override void OnAIKilled(IEntity ai, IEntity instigator)
	{
		super.OnAIKilled(ai, instigator);
		
		// Broadcast the updated threat value to all clients
		if (Replication.IsServer())
		{
			Rpc(RpcDo_UpdateFactionResources, m_iResources, m_iThreat);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced Init to trigger initial synchronization for clients
	override void Init(IEntity owner)
	{
		super.Init(owner);
		
		// On server, broadcast current faction data to all clients after initialization
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().CallLater(SyncFactionDataToClients, 2000);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Synchronize current faction data to all clients
	protected void SyncFactionDataToClients()
	{
		if (Replication.IsServer())
		{
			Rpc(RpcDo_UpdateFactionResources, m_iResources, m_iThreat);
			Print(string.Format("TSE_OccupyingFactionManager: Synced faction data to clients - Resources: %1, Threat: %2", m_iResources, m_iThreat));
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Client-side method to request current faction data from server
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestFactionData()
	{
		// Server responds with current faction data
		if (Replication.IsServer())
		{
			Rpc(RpcDo_UpdateFactionResources, m_iResources, m_iThreat);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Public method for clients to request faction data
	void RequestFactionData()
	{
		if (!Replication.IsServer())
		{
			Rpc(RpcAsk_RequestFactionData);
		}
	}
} 