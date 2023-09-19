class OVT_NotificationManagerComponentClass: OVT_ComponentClass
{	
};

class OVT_NotificationData : Managed
{
	SCR_SimpleMessagePreset msg;
	ref TimeContainer time;
	
	string param1;
	string param2;
	string param3;
	
	int displayTimer = 5000;
}

sealed class OVT_DiscordWebhookCallback : RestCallback
{
	override void OnSuccess(string data, int dataSize)
	{
		Print("Web Hook Success");
	}
	
	override void OnError(int errorCode)
	{
		Print("Web Hook Error: " + errorCode.ToString());
	}

	//------------------------------------------------------------------------------------------------
	override void OnTimeout()
	{
		Print("Web Hook Timeout");
	}
}

class OVT_NotificationManagerComponent: OVT_Component
{
	[Attribute()]
	protected ref SCR_SimpleMessagePresets m_Messages;
	
	[Attribute("25")]
	protected int m_iMaxNotifications;
	
	ref array<ref OVT_NotificationData> m_aNotifications = {};
	
	// Instance of this component
	private static OVT_NotificationManagerComponent s_Instance = null;
	
	static OVT_NotificationManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_NotificationManagerComponent.Cast(pGameMode.FindComponent(OVT_NotificationManagerComponent));
		}

		return s_Instance;
	}
	
	void SendTextNotification(string tag, int playerId = -1, string param1 = "", string param2="", string param3="")
	{		
		Rpc(RpcDo_RcvTextNotification, tag, playerId, param1, param2, param3);
		if(RplSession.Mode() != RplMode.Dedicated)
		{
			RpcDo_RcvTextNotification(tag, playerId, param1, param2, param3);
		}
	}
	
	void SendExternalNotifications(string tag, string param1 = "", string param2="", string param3="")
	{
		SCR_SimpleMessagePreset preset = m_Messages.GetPreset(tag);
		if(!preset) return;
					
		string localized = WidgetManager.Translate(preset.m_UIInfo.GetDescription(), param1, param2, param3);
		
		Print("Overthrow: " + localized);
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		if(config.m_ConfigFile && config.m_ConfigFile.discordWebHookURL.StartsWith("http"))
		{
			RestContext context = GetGame().GetRestApi().GetContext(config.m_ConfigFile.discordWebHookURL);
			int result = context.POST(new OVT_DiscordWebhookCallback(),"","content="+localized);
		}		
	}
	
	static string Serialize(Managed data)
	{
		ContainerSerializationSaveContext writer();
		JsonSaveContainer jsonContainer = new JsonSaveContainer();
		jsonContainer.SetMaxDecimalPlaces(5);
		writer.SetContainer(jsonContainer);
		writer.WriteValue("", data);
		return jsonContainer.ExportToString();
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RcvTextNotification(string tag, int playerId, string param1, string param2, string param3)
	{
		if(playerId > -1)
		{
			int localId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
			if(playerId != localId) return;
		}
		
		SCR_SimpleMessagePreset preset = m_Messages.GetPreset(tag);
		if(!preset) return;
		
		OVT_NotificationData data();
		
		data.msg = preset;
		data.param1 = param1;
		data.param2 = param2;
		data.param3 = param3;
		
		data.time = GetGame().GetTimeAndWeatherManager().GetTime();
		
		m_aNotifications.InsertAt(data, 0);
		
		if(m_aNotifications.Count() > m_iMaxNotifications)
		{
			m_aNotifications.Remove(m_aNotifications.Count() - 1);
		}
	}
}