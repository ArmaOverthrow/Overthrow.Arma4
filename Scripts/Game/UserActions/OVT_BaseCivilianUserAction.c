//! Base class for user actions that interact with civilians
//! Provides common functionality for checking character state and recruit status
class OVT_BaseCivilianUserAction : ScriptedUserAction
{
	protected bool m_bHasBeenPerformed = false;
	
	//! Override this to implement specific action logic
	protected void PerformCivilianAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Implemented by derived classes
	}
	
	//! Override this to provide specific validation logic
	protected bool CanPerformCivilianAction(IEntity user)
	{
		return !m_bHasBeenPerformed;
	}
	
	//! Override this to control when action should be visible
	protected bool CanShowCivilianAction(IEntity user)
	{
		return !m_bHasBeenPerformed;
	}
	
	//! Final implementation - calls derived class logic
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_bHasBeenPerformed) 
			return;
			
		PerformCivilianAction(pOwnerEntity, pUserEntity);
	}
	
	//! Final implementation with common validation
	override bool CanBePerformedScript(IEntity user)
	{
		// Don't allow actions on recruits
		if (IsRecruit(GetOwner()))
			return false;
			
		// Don't allow actions on dead/unconscious characters
		if (!IsCharacterAliveAndConscious(GetOwner()))
			return false;
			
		// Call derived class validation
		return CanPerformCivilianAction(user);
	}
	
	//! Final implementation with common visibility rules
	override bool CanBeShownScript(IEntity user)
	{
		// Don't show for recruits
		if (IsRecruit(GetOwner()))
			return false;
			
		// Don't show for dead or unconscious characters
		if (!IsCharacterAliveAndConscious(GetOwner()))
			return false;
			
		// Call derived class visibility logic
		return CanShowCivilianAction(user);
	}
	
	//! Check if the entity is a recruit
	protected bool IsRecruit(IEntity entity)
	{
		if (!entity)
			return false;
			
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return false;
			
		return recruitManager.GetRecruitFromEntity(entity) != null;
	}
	
	//! Check if character is alive and conscious
	protected bool IsCharacterAliveAndConscious(IEntity entity)
	{
		if (!entity)
			return false;
			
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;
			
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return false;
			
		// Only allow actions on alive and conscious characters
		return controller.GetLifeState() == ECharacterLifeState.ALIVE;
	}
	
	//! Mark action as performed to prevent repeated execution
	protected void MarkAsPerformed()
	{
		m_bHasBeenPerformed = true;
	}
	
	//! Check if action has been performed
	protected bool HasBeenPerformed()
	{
		return m_bHasBeenPerformed;
	}
	
	//! Default: actions are local only
	override bool HasLocalEffectOnlyScript() 
	{ 
		return true; 
	}
}