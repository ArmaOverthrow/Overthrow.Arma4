[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_UIManagerComponentClass: OVT_ComponentClass
{}

class OVT_UIManagerComponent: OVT_Component
{
	Widget m_wOverlay;
	OVT_MainMenuWidgets m_wMainMenu;
	SCR_CharacterControllerComponent m_Controller;
	InputManager m_InputManager;
	
	bool m_bMainMenuShowing = false;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_InputManager = GetGame().GetInputManager();
		
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast( owner.FindComponent(SCR_CharacterControllerComponent) );		
		
		if(controller){
			m_Controller = controller;
			m_Controller.m_OnControlledByPlayer.Insert(this.OnControlledByPlayer);
			m_Controller.m_OnPlayerDeath.Insert(this.OnPlayerDeath);
		}
	}

	protected void RegisterInputs()
	{
		if (!m_InputManager)
			return;
						
		m_InputManager.AddActionListener("OverthrowMainMenu", EActionTrigger.DOWN, OnMainMenu);
		m_InputManager.AddActionListener("OverthrowMainMenuEsc", EActionTrigger.DOWN, CloseMainMenu);
	}
	
	protected void UnregisterInputs()
	{
		if (!m_InputManager)
			return;
						
		m_InputManager.RemoveActionListener("OverthrowMainMenu", EActionTrigger.DOWN, OnMainMenu);
		m_InputManager.RemoveActionListener("OverthrowMainMenuEsc", EActionTrigger.DOWN, CloseMainMenu);
	}
	
	protected void OnMainMenu(float value, EActionTrigger reason)
	{
		if(m_bMainMenuShowing)
		{
			CloseMainMenu(value, reason);
			return;
		}
		m_bMainMenuShowing = true;
		
		m_Controller.SetDisableViewControls(true);
		m_Controller.SetDisableMovementControls(true);
		m_Controller.SetDisableWeaponControls(true);
		
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		m_wOverlay = workspace.CreateWidgets("{5201CB56B09FB27A}UI/Layouts/Menu/MainMenu.layout");
		
		OVT_MainMenuWidgets mainMenu = new OVT_MainMenuWidgets();
		mainMenu.Init(m_wOverlay);
		mainMenu.OnUpdate(GetOwner());
	}
	
	protected void CloseMainMenu(float value, EActionTrigger reason)
	{
		m_bMainMenuShowing = false;
		
		m_Controller.SetDisableViewControls(false);
		m_Controller.SetDisableMovementControls(false);
		m_Controller.SetDisableWeaponControls(false);
		
		m_wOverlay.RemoveFromHierarchy();
	}
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{		
		if (m_bMainMenuShowing)	
		{
			m_InputManager.ActivateContext("OverthrowMainMenuContext");
		}
	}
	
	protected void OnPlayerDeath(SCR_CharacterControllerComponent charController, IEntity instigator)
	{		
		if (!charController || charController != m_Controller)
			return;
		UnregisterInputs();
		m_bMainMenuShowing = false;
	}
	
	protected void OnControlledByPlayer(IEntity owner, bool controlled)
	{		
		if (!controlled)
		{			
			UnregisterInputs();
			ClearEventMask(owner, EntityEvent.FRAME);
		}
		else if (owner)
		{			
			RegisterInputs();
			SetEventMask(owner, EntityEvent.FRAME);
		}
	}
}