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
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		m_mHomes = new map<string, RplId>;
		m_mOwned = new map<string, ref set<RplId>>;
		m_Town = OVT_TownManagerComponent.Cast(GetOwner().FindComponent(OVT_TownManagerComponent));	
	}
	
	void SetHome(string playerId, IEntity building)
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
		
		if(!m_mHomes.Contains(playerId))
		{
			IEntity newHome = m_Town.GetRandomHouse();
			SetOwner(playerId, newHome);
			SetHome(playerId, newHome);			
			OVT_VehicleManagerComponent.GetInstance().SpawnStartingCar(newHome, playerId);
			return newHome;
		}
		
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
			writer.Write(m_mHomes.GetKey(i),32);
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
			if (!RPL_ReadString(reader, playerId)) return false;
			if (!reader.ReadRplId(id)) return false;		
			
			m_mHomes[playerId] = id;
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void RpcAsk_SetHome(string playerId, RplId id)
	{
		DoSetHome(playerId, id);		
		Rpc(RpcDo_SetHome, playerId, id);		
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetHome(string playerId, RplId id)
	{
		DoSetHome(playerId, id);	
	}
	
	void DoSetHome(string playerId, RplId id)
	{
		m_mHomes[playerId] = id;
	}
}