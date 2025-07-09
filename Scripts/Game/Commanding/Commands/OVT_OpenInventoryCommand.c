//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseGroupCommandTitleField("m_sCommandName")]
class OVT_OpenInventoryCommand : SCR_BaseGroupCommand
{
    //------------------------------------------------------------------------------------------------
    override bool Execute(IEntity cursorTarget, IEntity target, vector targetPosition, int playerID, bool isClient)
    {
		if (isClient)
		{
			//place to place a logic that would be executed for other players
			return true;
		}	
		
        if (!target && !cursorTarget)
            return false;
        
        SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerID));
		if (!playerController)
			return false;
		
		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
		if (!groupController)
			return false;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(cursorTarget);
		if (!character)
			return false;
		
		IEntity playerEntity = playerController.GetMainEntity();
		if(!playerEntity)
			return false;
            
        return OpenCharacterInventory(playerEntity, character);
    }
        
    //------------------------------------------------------------------------------------------------
    bool OpenCharacterInventory(IEntity playerEntity, SCR_ChimeraCharacter targetCharacter)
    {
        // Get target's inventory manager
        SCR_InventoryStorageManagerComponent targetInventoryManager = SCR_InventoryStorageManagerComponent.Cast(
            targetCharacter.FindComponent(SCR_InventoryStorageManagerComponent)
        );
        
        if (!targetInventoryManager)
            return false;     
		
		SCR_CharacterControllerComponent characterController = SCR_CharacterControllerComponent.Cast(
			targetCharacter.FindComponent(SCR_CharacterControllerComponent)
		);   
		
		if (!characterController)
            return false;     
		
		// Don't allow opening inventory of dead recruits
		if (characterController.GetLifeState() != ECharacterLifeState.ALIVE)
			return false;
                
        // Check if the recruit is owned by the player (for Overthrow)
        OVT_PlayerOwnerComponent ownerComponent = OVT_PlayerOwnerComponent.Cast(
            targetCharacter.FindComponent(OVT_PlayerOwnerComponent)
        );
        
        if (ownerComponent)
        {
            string persId = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(playerEntity);
            if (persId != ownerComponent.GetPlayerOwnerUid())
            {
                // Player doesn't own this recruit
                return false;
            }
        }
		
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(playerEntity);
        
        // Use RPC to set possessed entity on server and open inventory on client
        OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
        if (!comms)
            return false;
        
        // Note: We don't set up inventory listener here because this runs on server
        // The client will handle inventory close detection
        
        // Call RPC to possess on server and open inventory on client
        comms.SetPossessedEntityAndOpenInventory(playerId, targetCharacter);
        
        return true;
    }
    
    //------------------------------------------------------------------------------------------------
    override bool CanBeShown()
    {
        if (!CanBeShownInCurrentLifeState())
            return false;
        
        SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
        if (!groupController)
            return false;
            
        // Check if we have a valid target
        PlayerCamera camera = GetGame().GetPlayerController().GetPlayerCamera();
        if (!camera)
            return false;
        
        IEntity cursorTarget = camera.GetCursorTarget();
        if (!cursorTarget)
            return false;
        
        // Check if target is a character
        SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(cursorTarget);
        if (character)
        {
            // Check if it's a recruit owned by the player
            OVT_PlayerOwnerComponent ownerComponent = OVT_PlayerOwnerComponent.Cast(
                character.FindComponent(OVT_PlayerOwnerComponent)
            );
            
            if (!ownerComponent)
                return false;
                
            IEntity playerEntity = SCR_PlayerController.GetLocalControlledEntity();
            if (!playerEntity)
                return false;
                
            string persId = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(playerEntity);
            return persId == ownerComponent.GetPlayerOwnerUid();
        }
        
        // Check if target is a group
        SCR_AIGroup group = SCR_AIGroup.Cast(cursorTarget);
        if (group)
        {
            // For now, allow showing for any AI group
            // You might want to add additional checks here
            return true;
        }
        
        return false;
    }
    
    //------------------------------------------------------------------------------------------------
    override string GetCommandName()
    {
        return "openInventory";
    }
}