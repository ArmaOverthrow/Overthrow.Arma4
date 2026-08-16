[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_UIManagerComponentClass: OVT_ComponentClass
{}

class OVT_UIManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_UIContext> m_aContexts;
	
	SCR_CharacterControllerComponent m_Controller;

	//! The local player's tutorial pipeline, resolved lazily and dropped the moment this character
	//! stops being controlled.
	//!
	//! Cached because EOnFrame asks it a question every frame and OVT_Global.GetTutorials() is four
	//! lookups deep (controlled entity -> player id -> controller -> component). Dropped in
	//! AfterControlledByPlayer rather than held for the component's lifetime: the controller is a
	//! DIFFERENT entity with its own lifetime, and a reference to a component on a deleted entity is
	//! the shape of bug that OVT_TutorialManagerComponent's missing OnDelete already cost this feature
	//! once.
	protected OVT_TutorialComponent m_Tutorials;


	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);	
		
		if(SCR_Global.IsEditMode()) return;	
		
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast( owner.FindComponent(SCR_CharacterControllerComponent) );		
				
		if(controller){
			m_Controller = controller;
			
			m_Controller.m_OnControlledByPlayer.Insert(this.OnControlledByPlayer);
			m_Controller.m_OnPlayerDeath.Insert(this.OnPlayerDeath);
		}
		
		foreach(OVT_UIContext context : m_aContexts)
		{
			context.Init(owner, this);				
		}
	}
	
	void ShowContext(typename typeName)
	{
		foreach(OVT_UIContext context : m_aContexts)
		{
			if(context.ClassName() == typeName.ToString())
			{
				context.ShowLayout();
				break;
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Whether any Overthrow context currently owns the screen.
	//!
	//! One of the three facts OVT_TutorialComponent folds into the tutorial gate's blockingUiOpen
	//! argument (the other two are the base-game menu stack and the map). Deliberately a question
	//! about ALL contexts rather than a named few: a popup drawn over any of the seventeen is wrong.
	//!
	//! Asks IsBlockingPopups() rather than IsActive() because the two are NOT the same question.
	//! OVT_PlaceContext and OVT_BuildContext close their menus before they start driving a ghost, so
	//! they are inactive for the whole of placement while still owning the screen and the rotate keys.
	//! \return True when at least one context reports that it must not be drawn over.
	bool IsAnyContextBlocking()
	{
		if(!m_aContexts) return false;

		foreach(OVT_UIContext context : m_aContexts)
		{
			if(!context) continue;
			if(context.IsBlockingPopups()) return true;
		}

		return false;
	}

	OVT_UIContext GetContext(typename typeName)
	{
		foreach(OVT_UIContext context : m_aContexts)
		{
			if(context.ClassName() == typeName.ToString())
			{
				return context;
			}
		}
		
		return null;
	}
	
	OVT_UIContext GetContextByString(string typeName)
	{
		foreach(OVT_UIContext context : m_aContexts)
		{
			if(context.ClassName() == typeName)
			{
				return context;
			}
		}
		
		return null;
	}
			
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		GetGame().GetInputManager().ActivateContext("OverthrowGeneralContext");

		ActivateTutorialHudContext();

		foreach(OVT_UIContext context : m_aContexts)
		{
			context.EOnFrame(owner, timeSlice);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the non-modal tutorial tip's input context alive for exactly as long as a tip is on screen.
	//!
	//! WHY HERE AND NOT IN THE HUD DISPLAY. An ActionContext has to be activated EVERY FRAME to stay
	//! live, and OVT_TutorialInfo has no per-frame hook it can trust: it drives itself off a 100 ms
	//! CallLater precisely because SCR_InfoDisplay.UpdateValues does not run while the HUD is hidden,
	//! which is the situation this whole change exists to support. This method does run every frame,
	//! on the character, whatever the HUD is doing.
	//!
	//! The context carries one action, OverthrowTutorialHudLearnMore, on inputs nothing else uses
	//! (keyboard KC_F5, and left_trigger+Y as a combo on the pad). Nothing is taken from the player
	//! while a tip is up; the combo does still fire its own parts, which is how InputSourceCombo works
	//! everywhere in this game and the trade the mod already accepted for LT+pad_down.
	protected void ActivateTutorialHudContext()
	{
		OVT_TutorialComponent tutorials = m_Tutorials;
		if (!tutorials)
		{
			tutorials = OVT_Global.GetTutorials();
			if (!tutorials)
				return;

			m_Tutorials = tutorials;
		}

		if (!tutorials.IsShowingNonModal())
			return;

		GetGame().GetInputManager().ActivateContext("OverthrowTutorialHudContext");
	}
	
	protected void OnPlayerDeath()
	{		
		foreach(OVT_UIContext context : m_aContexts)
		{
			context.CloseLayout();
			context.UnregisterInputs();
		}
	}
	
	protected void AfterControlledByPlayer(IEntity owner, bool controlled)
	{
		// LISTEN HOST: this component sits on Character_Player, so the host owns an instance for EVERY
		// player character in the world - not just its own. m_OnControlledByPlayer fires on the host
		// with controlled=true for remote players too, and the base game says so out loud:
		// SCR_GadgetManagerComponent.OnControlledByPlayer carries the same discard ("for hosted server,
		// if we get controlled=true for entity which isnt ours"). Without this guard the host registered
		// a SECOND (third, fourth...) full set of UI contexts per joined player, so one press of U built
		// one main menu per player stacked on top of each other, only the clicked one closed, and the
		// survivors kept activating their menu action context every frame - which is what took the
		// place/build rotate keys away from the host and no one else.
		//
		// Only the controlled=true branch is discarded, exactly as vanilla does. Unpossess must still
		// run for our own character, and by then GetLocalControlledEntity() is no longer it.
		if (controlled && owner != SCR_PlayerController.GetLocalControlledEntity())
			controlled = false;

		if (!controlled)
		{
			ClearEventMask(owner, EntityEvent.FRAME);

			m_Tutorials = null;

			foreach(OVT_UIContext context : m_aContexts)
			{
				context.UnregisterInputs();
			}
		}
		else if (owner)
		{			
			SetEventMask(owner, EntityEvent.FRAME);			
			
			foreach(OVT_UIContext context : m_aContexts)
			{
				context.OnControlledByPlayer();
				context.RegisterInputs();				
			}
			
			OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
			int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(owner);
			
			if(gameMode) gameMode.OnPlayerSpawnedLocal(OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId));
		}
	}
	
	protected void OnControlledByPlayer(IEntity owner, bool controlled)
	{		
		GetGame().GetCallqueue().CallLater(AfterControlledByPlayer, 0, false, owner, controlled);
	}
}