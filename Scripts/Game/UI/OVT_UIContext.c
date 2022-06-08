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
	protected int m_iPlayerID;
	
	void Init(IEntity owner, OVT_UIManagerComponent uimanager)
	{
		m_Owner = owner;
		m_InputManager = GetGame().GetInputManager();
		m_Economy = OVT_EconomyManagerComponent.GetInstance();
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		m_UIManager = uimanager;		
		
		m_Controller = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		
		PostInit();
	}
	
	void OnControlledByPlayer()
	{
		m_iPlayerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(m_Owner);
	}
	
	void PostInit()
	{
	
	}
	
	void RegisterInputs()
	{
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
		if(!m_Layout) return;
		
		if(m_bOpenActionCloses && m_bIsActive)
		{
			CloseLayout();
			return;
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
	
	void Enable()
	{
		m_bIsActive = true;
	}
	
	void Disable()
	{
		m_bIsActive = false;
	}
	
	protected void ShowHint(string text)
	{		
		SCR_HintManagerComponent.GetInstance().ShowCustom(text);		
	}
}