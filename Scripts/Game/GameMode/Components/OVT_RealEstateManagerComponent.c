class OVT_RealEstateManagerComponentClass: OVT_OwnerManagerComponentClass
{
};

class OVT_RealEstateManagerComponent: OVT_OwnerManagerComponent
{
	ref map<string, RplId> m_mHomes;
	
	protected OVT_TownManagerComponent m_Town;
	
	static OVT_RealEstateManagerComponent s_Instance;
	
	static OVT_RealEstateManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_RealEstateManagerComponent.Cast(pGameMode.FindComponent(OVT_RealEstateManagerComponent));
		}

		return s_Instance;
	}
	
	void OVT_RealEstateManagerComponent()
	{
		m_mHomes = new map<string, RplId>;
	}
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;		
		
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));	
	}
	
	void SetHome(int playerId, IEntity building)
	{	
		RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
		Rpc(RpcAsk_SetHome, playerId, rpl.Id());		
	}
	
	IEntity GetNearestOwned(string playerId, vector pos)
	{
		if(!m_mOwned.Contains(playerId)) return null;
		
		BaseWorld world = GetGame().GetWorld();
		
		float nearest = 999999;
		IEntity nearestEnt;		
		
		set<RplId> owner = m_mOwned[playerId];
		foreach(RplId id : owner)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity ent = rpl.GetEntity();
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetHome(string playerId)
	{				
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(m_mHomes[playerId]));
		if(rpl)
		{
			return rpl.GetEntity();
		}
		
		return null;
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{
		super.RplSave(writer);
		
		//Send JIP homes
		writer.Write(m_mHomes.Count(), 32); 
		for(int i; i<m_mHomes.Count(); i++)
		{			
			RPL_WritePlayerID(writer, m_mHomes.GetKey(i));
			writer.WriteRplId(m_mHomes.GetElement(i));
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		if(!super.RplLoad(reader)) return false;
		
		//Recieve JIP homes
		int length, ownedlength;
		string playerId;
		RplId id;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!RPL_ReadPlayerID(reader, playerId)) return false;
			if (!reader.ReadRplId(id)) return false;		
			
			m_mHomes[playerId] = id;
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetHome(int playerId, RplId id)
	{
		DoSetHome(playerId, id);		
		Rpc(RpcDo_SetHome, playerId, id);		
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetHome(int playerId, RplId id)
	{
		DoSetHome(playerId, id);	
	}
	
	void DoSetHome(int playerId, RplId id)
	{
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		m_mHomes[persId] = id;
	}
}