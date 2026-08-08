[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_UIManagerComponentClass: OVT_ComponentClass
{}

class OVT_UIManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_UIContext> m_aContexts;
	
	SCR_CharacterControllerComponent m_Controller;
					
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
		
		foreach(OVT_UIContext context : m_aContexts)
		{
			context.EOnFrame(owner, timeSlice);
		}	
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
		if (!controlled)
		{			
			ClearEventMask(owner, EntityEvent.FRAME);
						
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