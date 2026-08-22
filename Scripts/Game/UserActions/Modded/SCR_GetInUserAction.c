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
			OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(pOwnerEntity);
			if(playerowner)
			{
				string ownerUid = playerowner.GetPlayerOwnerUid();
				if(ownerUid == "")
				{
					// Request ownership on the server. The claimant is no longer named in the request:
					// the server derives it from the controller the request arrives on, which is also
					// why the local player id is not looked up here any more.
					OVT_VehicleRequestComponent vehicles = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
					if (vehicles)
					{
						vehicles.ClaimUnownedVehicle(pOwnerEntity);
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

		// Vanilla refuses a cargo bed holding supplies (SCR_GetInUserAction.c:78) by reading
		// SCR_ResourceComponent, which Overthrow never fills - so the bed seats stayed open on a loaded
		// truck and a passenger spawned inside the crates. Same gate, driven off our own ledger.
		// BlockSuppliesIfOccupied() is true only on the bed's compartment manager, never the cab's.
		if (m_CompartmentManager && m_CompartmentManager.BlockSuppliesIfOccupied() && CargoBedIsLoaded(compartment))
		{
			SetCannotPerformReason("#AR-UserAction_SeatOccupied");
			return false;
		}

		IEntity owner = compartment.GetOwner();
		Vehicle vehicle = Vehicle.Cast(SCR_EntityHelper.GetMainParent(owner, true));
		if (vehicle)
		{
			Faction characterFaction = character.GetFaction();
			Faction vehicleFaction = vehicle.GetFaction();
			// Same-faction never blocks: a faction can read as enemy to ITSELF on a client whose
			// relation table is empty or in FFA-style setups - vanilla carries this guard on the
			// door/handbrake actions but forgot it here (BUG-132)
			if (characterFaction && vehicleFaction && vehicleFaction != characterFaction && characterFaction.IsFactionEnemy(vehicleFaction))
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
		
		OVT_PlayerOwnerComponent playerowner = OVT_ComponentFinder<OVT_PlayerOwnerComponent>.Find(vehicle);
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

	//------------------------------------------------------------------------------------------------
	//! \param[in] compartment The seat being asked about.
	//! \return True when the vehicle this seat belongs to carries any resources at all.
	protected bool CargoBedIsLoaded(notnull BaseCompartmentSlot compartment)
	{
		IEntity owner = compartment.GetOwner();
		if (!owner)
			return false;

		OVT_ResourceStoreComponent store = OVT_ResourceUtils.GetStore(SCR_EntityHelper.GetMainParent(owner, true));
		if (!store)
			return false;

		return store.GetUsedLitres() > 0;
	}
};
