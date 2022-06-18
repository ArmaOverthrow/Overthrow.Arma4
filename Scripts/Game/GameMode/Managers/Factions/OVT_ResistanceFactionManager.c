class OVT_ResistanceFactionManagerClass: OVT_ComponentClass
{	
}

class OVT_ResistanceFactionManager: OVT_Component
{	
	ref set<RplId> m_FOBs;
	
	OVT_PlayerManagerComponent m_Players;
	
	static OVT_ResistanceFactionManager s_Instance;
	
	static OVT_ResistanceFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_ResistanceFactionManager.Cast(pGameMode.FindComponent(OVT_ResistanceFactionManager));
		}

		return s_Instance;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Players = OVT_Global.GetPlayers();
	}
	
	void OVT_ResistanceFactionManager()
	{
		m_FOBs = new set<RplId>;	
	}
	
	void RegisterFOB(IEntity ent, int playerId)
	{
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if(!rpl) return;
		Rpc(RpcAsk_RegisterFOB, rpl.Id(), playerId);
	}
	
	OVT_ResistanceFOBControllerComponent GetNearestFOB(vector pos)
	{
		IEntity nearestBase;
		float nearest = 9999999;
		foreach(RplId id : m_FOBs)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity marker = rpl.GetEntity();
			float distance = vector.Distance(marker.GetOrigin(), pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = marker;
			}
		}
		if(!nearestBase) return null;
		return OVT_ResistanceFOBControllerComponent.Cast(nearestBase.FindComponent(OVT_ResistanceFOBControllerComponent));
	}
	
	//RPC Methods	
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP FOBs
		writer.Write(m_FOBs.Count(), 32); 
		for(int i; i<m_FOBs.Count(); i++)
		{
			writer.WriteRplId(m_FOBs[i]);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP FOBs
		int length;
		RplId id;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadRplId(id)) return false;
			m_FOBs.Insert(id);
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RegisterFOB(RplId id, int playerId)
	{
		m_FOBs.Insert(id);
				
		Rpc(RpcDo_RegisterFOB, id);
		m_Players.HintMessageAll("PlacedFOB",-1,playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterFOB(RplId id)
	{
		m_FOBs.Insert(id);
	}
	
	void ~OVT_ResistanceFactionManager()
	{		
		if(m_FOBs)
		{
			m_FOBs.Clear();
			m_FOBs = null;
		}			
	}
}