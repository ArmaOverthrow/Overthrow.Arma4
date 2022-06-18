class OVT_PlayerManagerComponentClass: OVT_ComponentClass
{
};

class OVT_PlayerManagerComponent: OVT_Component
{	
	[Attribute()]
	ResourceName m_rMessageConfigFile;
	
	protected SCR_SimpleMessagePresets m_Messages;
	
	ref ScriptInvoker m_OnPlayerRegistered = new ScriptInvoker();
	
	static OVT_PlayerManagerComponent s_Instance;
	static OVT_PlayerManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_PlayerManagerComponent.Cast(pGameMode.FindComponent(OVT_PlayerManagerComponent));
		}

		return s_Instance;
	}
	
	static void EncodeIDAsInts(string persId, out int int1, out int int2, out int int3)
	{
		int1 = (persId.Substring(0,9)).ToInt();
		int2 = (persId.Substring(9,9)).ToInt();
		int3 = (persId.Substring(18,persId.Length() - 18)).ToInt();
	}
	
	protected ref map<int, string> m_mPersistentIDs;
	protected ref map<string, int> m_mPlayerIDs;
	
	void OVT_PlayerManagerComponent()
	{
		m_mPersistentIDs = new map<int, string>;
		m_mPlayerIDs = new map<string, int>;
		
		LoadMessageConfig();
	}
	
	protected void LoadMessageConfig()
	{
		Resource holder = BaseContainerTools.LoadContainer(m_rMessageConfigFile);
		if (holder)		
		{
			SCR_SimpleMessagePresets obj = SCR_SimpleMessagePresets.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
			if(obj)
			{
				m_Messages = obj;
			}
		}
	}
	
	void HintMessageAll(string tag, int townId = -1, int playerId = -1)
	{
		SCR_SimpleMessagePreset preset = m_Messages.GetPreset(tag);
		int index = m_Messages.m_aPresets.Find(preset);
		Rpc(RpcDo_HintMessage, index, townId, playerId);
		DoHintMessage(index, townId, playerId);
	}
	
	protected string GetMessageText(int index, int townId = -1, int playerId = -1)
	{
		string text = "";
		SCR_SimpleMessagePreset preset = m_Messages.m_aPresets[index];		
		if(preset.m_UIInfo){
			text += preset.m_UIInfo.GetDescription();
		}
		return text;
	}
	
	protected string GetMessageTitle(int index, int townId = -1, int playerId = -1)
	{
		SCR_SimpleMessagePreset preset = m_Messages.m_aPresets[index];		
		string text = "";
		
		string title = preset.m_UIInfo.GetName();
		if(title != "")
		{ 
			//Prepend anything from UIInfo
			text = title + " ";
		}
		string townName;
		if(townId > -1)
		{
			//Add town name
			OVT_TownManagerComponent towns = OVT_Global.GetTowns();
			OVT_TownData town = towns.m_Towns[townId];
			SCR_MapDescriptorComponent desc = towns.GetNearestTownMarker(town.location);
			text += desc.Item().GetDisplayName() + ": ";
		}
		if(playerId > -1)
		{
			//Add player name
			string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
			text += name + " ";
		}		
		return text;
	}
	
	protected void DoHintMessage(int index, int townId = -1, int playerId = -1)
	{
		string text = GetMessageText(index, townId, playerId);
		string title = GetMessageText(index, townId, playerId);
		SCR_HintManagerComponent.GetInstance().ShowCustom(text, title, 10, true);
	}
	
	void SendMessageAll(string tag, int townId = -1, int playerId = -1)
	{
		SCR_SimpleMessagePreset preset = m_Messages.GetPreset(tag);
		int index = m_Messages.m_aPresets.Find(preset);
		Rpc(RpcDo_SendMessage, index, townId, playerId);
		DoRcvMessage(index, townId, playerId);
	}
	
	protected void DoRcvMessage(int index, int townId = -1, int playerId = -1)
	{
		string text = GetMessageText(index, townId, playerId);
	}
	
	string GetPersistentIDFromPlayerID(int playerId)
	{
		return m_mPersistentIDs[playerId];
	}
	
	int GetPlayerIDFromPersistentID(string id)
	{
		return m_mPlayerIDs[id];
	}
	
	void RegisterPlayer(int playerId, string persistentId)
	{
		m_mPersistentIDs[playerId] = persistentId;
		m_mPlayerIDs[persistentId] = playerId;
		
		m_OnPlayerRegistered.Invoke(playerId, persistentId);
		
		int id1, id2, id3;
		EncodeIDAsInts(persistentId, id1, id2, id3);
		Rpc(RpcDo_RegisterPlayer, playerId, id1, id2, id3);
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{
		super.RplSave(writer);
		
		//Send JIP ids
		writer.Write(m_mPersistentIDs.Count(), 32); 
		for(int i; i<m_mPersistentIDs.Count(); i++)
		{
			writer.Write(m_mPersistentIDs.GetKey(i),32);	
			RPL_WritePlayerID(writer, m_mPersistentIDs.GetElement(i));			
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{		
		//Recieve JIP ids
		int length, playerId;
		string persId;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(playerId, 32)) return false;		
			if (!RPL_ReadPlayerID(reader, persId)) return false;
			
			m_mPersistentIDs[playerId] = persId;
			m_mPlayerIDs[persId] = playerId;
		}
		return true;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterPlayer(int playerId, int id1, int id2, int id3)
	{
		string s = ""+id1+id2+id3;
		m_mPersistentIDs[playerId] = s;
		m_mPlayerIDs[s] = playerId;
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_HintMessage(int msg, int townId, int playerId)
	{
		DoHintMessage(msg, townId, playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SendMessage(int msg, int townId, int playerId)
	{
		DoRcvMessage(msg, townId, playerId);
	}
	
	void ~OVT_PlayerManagerComponent()
	{
		if(m_mPersistentIDs)
		{
			m_mPersistentIDs.Clear();
			m_mPersistentIDs = null;
		}
		if(m_mPlayerIDs)
		{
			m_mPlayerIDs.Clear();
			m_mPlayerIDs = null;
		}
	}
}