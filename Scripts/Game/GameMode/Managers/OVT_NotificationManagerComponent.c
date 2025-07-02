//------------------------------------------------------------------------------------------------
//! Class definition for the OVT_NotificationManagerComponent.
class OVT_NotificationManagerComponentClass: OVT_ComponentClass
{	
};

//------------------------------------------------------------------------------------------------
//! Represents the data structure for a single notification.
//! Stores the message preset, timestamp, parameters, and display duration.
class OVT_NotificationData : Managed
{
	//! The preset message configuration.
	SCR_SimpleMessagePreset msg;
	//! The time the notification was created.
	ref TimeContainer time;
	
	//! Optional first parameter for message localization.
	string param1;
	//! Optional second parameter for message localization.
	string param2;
	//! Optional third parameter for message localization.
	string param3;
	
	//! Duration for how long the notification should be displayed
	int displayTimer = 500;
}

//------------------------------------------------------------------------------------------------
//! Handles callbacks for Discord webhook REST API calls.
sealed class OVT_DiscordWebhookCallback : RestCallback
{
	//------------------------------------------------------------------------------------------------
	//! Called when the REST API call is successful.
	//! \param data The response data from the server.
	//! \param dataSize The size of the response data.
	override void OnSuccess(string data, int dataSize)
	{
		Print("Web Hook Success");
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when the REST API call encounters an error.
	//! \param errorCode The error code returned by the API.
	override void OnError(int errorCode)
	{
		Print("Web Hook Error: " + errorCode.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Called when the REST API call times out.
	override void OnTimeout()
	{
		Print("Web Hook Timeout");
	}
}

//------------------------------------------------------------------------------------------------
//! Manages sending and displaying notifications to players and external systems like Discord.
//! Handles both in-game text notifications and external webhook posts.
class OVT_NotificationManagerComponent: OVT_Component
{
	//! Reference to the SCR_SimpleMessagePresets configuration.
	[Attribute()]
	protected ref SCR_SimpleMessagePresets m_Messages;
	
	//! Maximum number of notifications to keep in the history.
	[Attribute("25")]
	protected int m_iMaxNotifications;
	
	//! Array storing the recent notification data.
	ref array<ref OVT_NotificationData> m_aNotifications = {};
	
	//! Singleton instance of the notification manager component.
	private static OVT_NotificationManagerComponent s_Instance = null;
	
	//------------------------------------------------------------------------------------------------
	//! Gets the singleton instance of the OVT_NotificationManagerComponent.
	//! \return The static instance of the component, or null if not found.
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
	
	//------------------------------------------------------------------------------------------------
	//! Sends a text notification to players based on a tag and optional parameters.
	//! Can be targeted to a specific player or broadcast to all.
	//! \param[in] tag The identifier tag for the message preset in m_Messages.
	//! \param[in] playerId The ID of the player to send the notification to (-1 for broadcast).
	//! \param[in] param1 Optional first parameter for localization.
	//! \param[in] param2 Optional second parameter for localization.
	//! \param[in] param3 Optional third parameter for localization.
	void SendTextNotification(string tag, int playerId = -1, string param1 = "", string param2="", string param3="")
	{		
		Rpc(RpcDo_RcvTextNotification, tag, playerId, param1, param2, param3);
		if(RplSession.Mode() != RplMode.Dedicated)
		{
			RpcDo_RcvTextNotification(tag, playerId, param1, param2, param3);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sends notifications to external systems (currently Discord via webhook).
	//! Uses the provided tag to fetch the message preset and localizes it.
	//! \param[in] tag The identifier tag for the message preset in m_Messages.
	//! \param[in] param1 Optional first parameter for localization.
	//! \param[in] param2 Optional second parameter for localization.
	//! \param[in] param3 Optional third parameter for localization.
	void SendExternalNotifications(string tag, string param1 = "", string param2="", string param3="")
	{
		SCR_SimpleMessagePreset preset = m_Messages.GetPreset(tag);
		if(!preset) return;
					
		string localized = WidgetManager.Translate(preset.m_UIInfo.GetDescription(), param1, param2, param3);
		
		Print("[Overthrow.NotificationManagerComponent] " + localized);
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		
		if(config.m_ConfigFile && config.m_ConfigFile.discordWebHookURL.StartsWith("http"))
		{
			RestContext context = GetGame().GetRestApi().GetContext(config.m_ConfigFile.discordWebHookURL);
			int result = context.POST(new OVT_DiscordWebhookCallback(),"","content="+localized);
		}		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Serializes managed data into a JSON string.
	//! \param[in] data The managed object to serialize.
	//! \return A JSON string representation of the data.
	static string Serialize(Managed data)
	{
		ContainerSerializationSaveContext writer();
		JsonSaveContainer jsonContainer = new JsonSaveContainer();
		jsonContainer.SetMaxDecimalPlaces(5);
		writer.SetContainer(jsonContainer);
		writer.WriteValue("", data);
		return jsonContainer.ExportToString();
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method called on clients to receive and process a text notification.
	//! Adds the received notification data to the local notification history.
	//! \param[in] tag The identifier tag for the message preset.
	//! \param[in] playerId The target player ID (-1 if broadcast).
	//! \param[in] param1 Optional first parameter for localization.
	//! \param[in] param2 Optional second parameter for localization.
	//! \param[in] param3 Optional third parameter for localization.
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
		
		if(!m_Time) 
		{
			ChimeraWorld world = GetOwner().GetWorld();
			m_Time = world.GetTimeAndWeatherManager();
		}
		
		data.time = m_Time.GetTime();
		
		m_aNotifications.InsertAt(data, 0);
		
		if(m_aNotifications.Count() > m_iMaxNotifications)
		{
			m_aNotifications.Remove(m_aNotifications.Count() - 1);
		}
	}
}