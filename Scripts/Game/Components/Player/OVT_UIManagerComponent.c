[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_UIManagerComponentClass: OVT_ComponentClass
{}

class OVT_UIManagerComponent: OVT_Component
{
	Widget m_wOverlay;
	OVT_MainMenuWidgets m_wMainMenu;
	SCR_CharacterControllerComponent m_Controller;
	InputManager m_InputManager;
	
	ref OVT_MainMenu m_mainMenu;
	ref OVT_PlaceMenu m_placeMenu;
	
	bool m_bMainMenuShowing = false;
	bool m_bPlaceMenuShowing = false;
			
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
		m_InputManager.AddActionListener("OverthrowPlaceMenuEsc", EActionTrigger.DOWN, ClosePlaceMenu);
	}
	
	protected void UnregisterInputs()
	{
		if (!m_InputManager)
			return;
						
		m_InputManager.RemoveActionListener("OverthrowMainMenu", EActionTrigger.DOWN, OnMainMenu);
		m_InputManager.RemoveActionListener("OverthrowMainMenuEsc", EActionTrigger.DOWN, CloseMainMenu);
		m_InputManager.RemoveActionListener("OverthrowPlaceMenuEsc", EActionTrigger.DOWN, ClosePlaceMenu);
	}
	
	protected void OnMainMenu(float value, EActionTrigger reason)
	{
		if(m_bMainMenuShowing)
		{
			CloseMainMenu(value, reason);
			return;
		}
		m_bMainMenuShowing = true;
		
		DisableViewControls();
		
		m_wOverlay = OpenLayout("{5201CB56B09FB27A}UI/Layouts/Menu/MainMenu.layout");
		
		OVT_MainMenuWidgets mainMenuWidgets = new OVT_MainMenuWidgets();
		mainMenuWidgets.Init(m_wOverlay);
		
		m_mainMenu = new OVT_MainMenu(mainMenuWidgets, m_wOverlay, this);
		
		m_mainMenu.OnUpdate(GetOwner());
	}
	
	void OpenPlaceMenu()
	{
		if(m_bMainMenuShowing)
		{
			CloseMainMenu();
		}
		m_bPlaceMenuShowing = true;
		
		DisableViewControls();
		
		m_wOverlay = OpenLayout("{02AA5A91E0DD49DF}UI/Layouts/Menu/PlaceMenu.layout");
				
		OVT_PlaceMenuWidgets widgets = new OVT_PlaceMenuWidgets();
		widgets.Init(m_wOverlay);
		
		m_placeMenu = new OVT_PlaceMenu(widgets, m_wOverlay, this);
		
		m_placeMenu.OnUpdate();
	}
	
	protected Widget OpenLayout(string layout)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		Widget overlay = workspace.CreateWidgets(layout);
		
		SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
		if (hud)
			hud.SetVisible(false);
		
		return overlay;
	}
	
	protected void DisableViewControls()
	{
		m_Controller.SetDisableViewControls(true);
		m_Controller.SetDisableMovementControls(true);
		m_Controller.SetDisableWeaponControls(true);
	}
	
	protected void EnableViewControls()
	{
		m_Controller.SetDisableViewControls(false);
		m_Controller.SetDisableMovementControls(false);
		m_Controller.SetDisableWeaponControls(false);
	}
	
	void CloseMainMenu(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		m_bMainMenuShowing = false;
		
		CloseCurrentMenu();
	}
	
	void ClosePlaceMenu(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		m_bPlaceMenuShowing = false;
		
		CloseCurrentMenu();
	}
	
	protected void CloseCurrentMenu()
	{
		EnableViewControls();
		
		m_wOverlay.RemoveFromHierarchy();
		
		SCR_HUDManagerComponent hud = GetGame().GetHUDManager();
		if (hud)
			hud.SetVisible(true);
	}
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{		
		if (m_bMainMenuShowing)	
		{			
			m_InputManager.ActivateContext("OverthrowMainMenuContext");
		}
		
		if (m_bPlaceMenuShowing)	
		{
			m_InputManager.ActivateContext("OverthrowPlaceMenuContext");
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