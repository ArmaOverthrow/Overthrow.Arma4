class OVT_UIContext : ScriptAndConfig
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Layout to show", params: "layout")]
	ResourceName m_Layout;
	
	[Attribute()]
	string m_sContextName;
	
	[Attribute()]
	string m_sOpenAction;
	
	[Attribute()]
	string m_sCloseAction;
	
	[Attribute("1")]
	bool m_bOpenActionCloses;
	
	[Attribute("1")]
	bool m_bHideHUDOnShow;
		
	protected IEntity m_Owner;
	protected bool m_bIsActive = false;	
	protected SCR_CharacterControllerComponent m_Controller;
	protected InputManager m_InputManager;
	protected OVT_EconomyManagerComponent m_Economy;
	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_UIManagerComponent m_UIManager;
	protected Widget m_wRoot;
	protected string m_sPlayerID;
	protected int m_iPlayerID;
	protected ChimeraCharacter m_Player;
	protected OVT_PlayerData m_PlayerData;
	
	void Init(IEntity owner, OVT_UIManagerComponent uimanager)
	{
		m_Owner = owner;
		m_InputManager = GetGame().GetInputManager();
		m_Economy = OVT_Global.GetEconomy();
		OVT_Global.GetConfig() = OVT_Global.GetConfig();
		m_UIManager = uimanager;		
		m_Player = ChimeraCharacter.Cast(owner);
		
		m_Controller = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		
		PostInit();
	}
	
	bool IsActive()
	{
		return m_bIsActive;
	}
	
	void OnControlledByPlayer()
	{
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(m_Owner);
		m_sPlayerID = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		m_iPlayerID = playerId;
		m_PlayerData = OVT_PlayerData.Get(m_iPlayerID);
	}
	
	void PostInit()
	{
	
	}
	
	void RegisterInputs()
	{
		if(!m_InputManager) return;
		if(m_sOpenAction != "")
		{
			m_InputManager.AddActionListener(m_sOpenAction, EActionTrigger.DOWN, ShowLayout);
		}
		if(m_sCloseAction != "")
		{
			m_InputManager.AddActionListener(m_sCloseAction, EActionTrigger.DOWN, CloseLayout);
		}
	}
	
	void UnregisterInputs()
	{
		if(!m_InputManager) return;
		if(m_sOpenAction != "")
		{
			m_InputManager.RemoveActionListener(m_sOpenAction, EActionTrigger.DOWN, ShowLayout);
		}
		if(m_sCloseAction != "")
		{
			m_InputManager.RemoveActionListener(m_sCloseAction, EActionTrigger.DOWN, CloseLayout);
		}
	}
	
	void EOnFrame(IEntity owner, float timeSlice)
	{	
		if(m_bIsActive)
		{
			m_InputManager.ActivateContext(m_sContextName);
			OnActiveFrame(timeSlice);
		}
		OnFrame(timeSlice);
	}
	
	protected void OnActiveFrame(float timeSlice)
	{
	
	}
	
	protected void OnFrame(float timeSlice)
	{
	
	}
	
	void ShowLayout()
	{
		Print("[Overthrow] ShowLayout() called on " + Type());
		Print("[Overthrow] m_Layout: " + m_Layout);
		if(!m_Layout)
		{
			Print("[Overthrow] ShowLayout() failed: m_Layout is null");
			return;
		}

		Print("[Overthrow] CanShowLayout(): " + CanShowLayout());
		if(!CanShowLayout())
		{
			Print("[Overthrow] ShowLayout() failed: CanShowLayout() returned false");
			return;
		}

		if(m_bOpenActionCloses && m_bIsActive)
		{
			Print("[Overthrow] ShowLayout() closing instead (already active)");
			CloseLayout();
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Print("[Overthrow] workspace: " + workspace);
		m_wRoot = workspace.CreateWidgets(m_Layout);
		Print("[Overthrow] m_wRoot created: " + m_wRoot);

		if(m_bHideHUDOnShow){
			SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
			if (hud)
				hud.SetVisible(false);
		}

		Enable();
		Print("[Overthrow] Calling OnShow()");
		OnShow();
		Print("[Overthrow] ShowLayout() complete");
	}
	
	bool CanShowLayout()
	{
		return true;
	}
	
	protected void OnShow()
	{
	
	}
	
	void CloseLayout()
	{
		if(!m_wRoot) return;
		if(!m_bIsActive) return;
		
		m_wRoot.RemoveFromHierarchy();
		
		if(m_bHideHUDOnShow){	
			SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
			if (hud)
				hud.SetVisible(true);
		}
		
		Disable();
		OnClose();
	}
	
	protected void OnClose()
	{
	
	}
	
	void SelectItem(ResourceName res)
	{
	
	}
	
	void Enable()
	{
		m_bIsActive = true;
	}
	
	void Disable()
	{
		m_bIsActive = false;
	}
	
	void ShowHint(string text)
	{		
		SCR_HintManagerComponent.GetInstance().ShowCustom(text);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Shows a notification using the OVT notification system with preset tags.
	//! More efficient than ShowHint for network communication.
	//! \\param[in] tag The preset tag to send.
	void ShowNotification(string tag)
	{
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if(notify) 
		{
			notify.SendTextNotification(tag, m_iPlayerID);
		}
		else
		{
			// Fallback - this shouldn't happen in normal gameplay
			Print("[Overthrow] Notification system unavailable, tried to send notification: " + tag);
		}
	}
}