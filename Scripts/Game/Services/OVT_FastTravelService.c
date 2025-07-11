//! Centralized fast travel service for Overthrow
//! Extracted from OVT_MapContext for reusability
class OVT_FastTravelService
{
	//! Global fast travel checks (wanted level, QRF, distance, etc.)
	static bool CanGlobalFastTravel(vector targetPos, string playerID, out string reason)
	{
		if (OVT_Global.GetConfig().m_bDebugMode) 
			return true;
		
		reason = "#OVT-CannotFastTravelThere";
		
		// Get player entity
		ChimeraCharacter playerEntity = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!playerEntity)
			return false;
		
		// Check minimum distance
		float dist = vector.Distance(targetPos, playerEntity.GetOrigin());
		if (dist < OVT_Global.GetConfig().m_Difficulty.minFastTravelDistance)
		{
			reason = "#OVT-CannotFastTravelDistance";
			return false;
		}
		
		// Check wanted level
		OVT_PlayerWantedComponent wantedComp = OVT_PlayerWantedComponent.Cast(playerEntity.FindComponent(OVT_PlayerWantedComponent));
		if (wantedComp && wantedComp.GetWantedLevel() > 0)
		{
			reason = "#OVT-CannotFastTravelWanted";
			return false;
		}
		
		// Check QRF restrictions
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (occupyingFaction && occupyingFaction.m_bQRFActive && 
			OVT_Global.GetConfig().m_Difficulty.QRFFastTravelMode != OVT_QRFFastTravelMode.FREE)
		{
			if (OVT_Global.GetConfig().m_Difficulty.QRFFastTravelMode == OVT_QRFFastTravelMode.DISABLED)
			{
				reason = "#OVT-CannotFastTravelDuringQRF";
				return false;
			}
			
			dist = vector.Distance(occupyingFaction.m_vQRFLocation, targetPos);
			if (dist < OVT_QRFControllerComponent.QRF_RANGE)
			{
				reason = "#OVT-CannotFastTravelToQRF";
				return false;
			}
		}
		
		// Check if player can afford fast travel
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return false;
			
		int intPlayerID = playerManager.GetPlayerIDFromPersistentID(playerID);
		if (intPlayerID < 0)
			return false;
			
		int cost = CalculateFastTravelCost(targetPos, intPlayerID);
		if (cost > 0)
		{
			OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
			if (!economy || !economy.PlayerHasMoney(playerID, cost))
			{
				reason = "#OVT-CannotAfford";
				return false;
			}
		}
		
		return true;
	}
	
	//! Calculate fast travel cost based on distance
	static int CalculateFastTravelCost(vector targetPos, int playerID)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || config.m_bDebugMode)
			return 0;
		
		// Get player entity
		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerID);
		if (!playerEntity)
			return 0;
		
		// Calculate distance in kilometers with minimum of 1km
		float dist = vector.Distance(targetPos, playerEntity.GetOrigin());
		float distKm = Math.Max(1.0, dist / 1000);
		
		// Calculate cost per km
		return Math.Round(distKm * config.m_Difficulty.fastTravelCost);
	}
	
	//! Execute fast travel with cost calculation
	static void ExecuteFastTravel(vector targetPos, int playerID)
	{
		if (playerID < 0)
			return;
		
		// Calculate distance-based cost
		int cost = CalculateFastTravelCost(targetPos, playerID);
		
		// Check if player can afford it
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		string playerPersistentID = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerID);
		
		if (cost > 0 && !economy.PlayerHasMoney(playerPersistentID, cost))
		{
			OVT_Global.ShowHint("#OVT-CannotAfford");
			return;
		}
		
		// Find safe spawn position
		targetPos = OVT_Global.FindSafeSpawnPosition(targetPos);
		
		// Get player entity
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return;
		
		// Handle vehicle travel
		ChimeraCharacter character = ChimeraCharacter.Cast(player);
		if (character && character.IsInVehicle())
		{
			CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
			if (compartmentAccess)
			{
				BaseCompartmentSlot slot = compartmentAccess.GetCompartment();
				if (slot.GetType() == ECompartmentType.PILOT)
				{
					// Player is driver, can fast travel vehicle
					if (cost > 0)
						economy.TakePlayerMoneyPersistentId(playerPersistentID, cost);
					OVT_Global.GetServer().RequestFastTravel(playerID, targetPos);
				}
				else
				{
					// Player is passenger, cannot fast travel
					OVT_Global.ShowHint("#OVT-MustBeDriver");
				}
			}
		}
		else
		{
			// Player is on foot, normal fast travel
			if (cost > 0)
				economy.TakePlayerMoneyPersistentId(playerPersistentID, cost);
			SCR_Global.TeleportPlayer(playerID, targetPos);
		}
	}
	
	//! Check if fast travel is available to a specific location type
	static bool CanFastTravelToLocationType(vector targetPos, string locationTypeName, string playerID, out string reason)
	{
		// First check global restrictions
		if (!CanGlobalFastTravel(targetPos, playerID, reason))
			return false;
		
		// Location type specific checks can be added here
		// For now, defer to the location type's own CanFastTravel method
		
		return true;
	}
}