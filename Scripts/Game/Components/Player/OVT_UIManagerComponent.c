[ComponentEditorProps(category: "Overthrow/Components/Player", description: "")]
class OVT_UIManagerComponentClass: OVT_ComponentClass
{}

class OVT_UIManagerComponent: OVT_Component
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_UIContext> m_aContexts;
	ref array<ref OVT_UIContext> m_aContextsPacked = new array<ref OVT_UIContext>();
	
	SCR_CharacterControllerComponent m_Controller;
					
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);		
		
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
			
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		GetGame().GetInputManager().ActivateContext("OverthrowGeneralContext");
		
		foreach(OVT_UIContext context : m_aContexts)
		{
			context.EOnFrame(owner, timeSlice);
		}	
	}
	
	protected void OnPlayerDeath(SCR_CharacterControllerComponent charController, IEntity instigator)
	{		
		if (!charController || charController != m_Controller)
			return;		
		
		foreach(OVT_UIContext context : m_aContexts)
		{
			context.UnregisterInputs();
		}
	}
	
	protected void OnControlledByPlayer(IEntity owner, bool controlled)
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
		}
	}
}