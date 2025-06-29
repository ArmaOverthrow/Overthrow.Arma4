modded class SCR_GetInUserAction : SCR_CompartmentUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;
		
		ChimeraCharacter character = ChimeraCharacter.Cast(pUserEntity);
		if (!character)
			return;
		
		BaseCompartmentSlot targetCompartment = GetCompartmentSlot();
		if (!targetCompartment)
			return;
		
		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return;
		
		if (!compartmentAccess.GetInVehicle(pOwnerEntity, targetCompartment, false, GetRelevantDoorIndex(pUserEntity), ECloseDoorAfterActions.RETURN_TO_PREVIOUS_STATE, false))
			return;
		
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());

		if(ot && PilotCompartmentSlot.Cast(targetCompartment)) {
			OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(pOwnerEntity);
			if(playerowner)
			{
				string ownerUid = playerowner.GetPlayerOwnerUid();
				if(ownerUid == "")
				{
					// Get the player ID and request ownership on server
					int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
					if (playerId > 0)
					{
						OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
						if (comms)
						{
							comms.ClaimUnownedVehicle(pOwnerEntity, playerId);
						}
					}
				}
			}
		}
		
		super.PerformAction(pOwnerEntity, pUserEntity);
	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		if (m_DamageManager && m_DamageManager.GetState() == EDamageState.DESTROYED)
			return false;

		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return false;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;
		
		IEntity owner = compartment.GetOwner();
		Vehicle vehicle = Vehicle.Cast(SCR_EntityHelper.GetMainParent(owner, true));
		if (vehicle)
		{
			Faction characterFaction = character.GetFaction();
			Faction vehicleFaction = vehicle.GetFaction();
			if (characterFaction && vehicleFaction && characterFaction.IsFactionEnemy(vehicleFaction))
			{
				SetCannotPerformReason("#AR-UserAction_SeatHostile");
				return false;
			}
		}
		
		if (compartment.GetOccupant())
		{
			SetCannotPerformReason("#AR-UserAction_SeatOccupied");
			return false;
		}
		
		// Check if the position isn't lock.
		if (m_pLockComp && m_pLockComp.IsLocked(user, compartment))
		{
			SetCannotPerformReason(m_pLockComp.GetCannotPerformReason(user));
			return false;
		}
		
		// Make sure vehicle can be enter via provided door, if not, set reason.
		if (!compartmentAccess.CanGetInVehicleViaDoor(owner, m_CompartmentManager, GetRelevantDoorIndex(user)))
		{
			SetCannotPerformReason("#AR-UserAction_SeatObstructed");
			return false;
		}
				
		OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!ot) return true;
		
		OVT_PlayerOwnerComponent playerowner = EPF_Component<OVT_PlayerOwnerComponent>.Find(vehicle);
		if(!playerowner || !playerowner.IsLocked()) return true;
		
		string ownerUid = playerowner.GetPlayerOwnerUid();
		if(ownerUid == "") return true;
		
		string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(user);
		if(ownerUid != playerUid)
		{
			SetCannotPerformReason("#OVT-Locked");
			return false;
		}
		
		return true;
	}		
};
