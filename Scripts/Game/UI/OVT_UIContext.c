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

	//------------------------------------------------------------------------------------------------
	//! Whether this context is doing something a tutorial popup must not be drawn over.
	//!
	//! Deliberately a SEPARATE question from IsActive(). A context can own the screen without having
	//! a layout up: OVT_PlaceContext.StartPlace() closes its menu (clearing m_bIsActive) and only then
	//! starts driving a ghost entity and activating OverthrowPlaceContext every frame, and
	//! OVT_BuildContext does the same. A popup that appeared during that would sit on top of the very
	//! placement it is describing, and its shortcuts would fight the rotate keys.
	//!
	//! Override in any context that keeps working after CloseLayout(); the default is correct for the
	//! fifteen that do not.
	//! \return True while this context must suppress tutorial popups.
	bool IsBlockingPopups()
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
		if(!m_Layout)
		{
			return;
		}

		if(!CanShowLayout())
		{
			return;
		}

		if(m_bIsActive)
		{
			if(m_bOpenActionCloses)
			{
				CloseLayout();
				return;
			}

			// Re-opening while active: close the existing layout first, otherwise the
			// new m_wRoot orphans the previous panel on screen
			CloseLayout();
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_wRoot = workspace.CreateWidgets(m_Layout);

		if(m_bHideHUDOnShow){
			SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
			if (hud)
				hud.SetVisible(false);
		}

		Enable();
		OnShow();

		// Client-local MENU_OPENED tutorial trigger. Fired AFTER Enable(), so the context is already
		// active and the gate correctly holds the popup back until this menu closes. Static and
		// self-guarding: no controller yet means the trigger is dropped, never an error.
		OVT_TutorialComponent.NotifyMenuOpened(ClassName());
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