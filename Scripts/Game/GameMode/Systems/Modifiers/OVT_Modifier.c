[BaseContainerProps()]
class OVT_Modifier : ScriptAndConfig
{
	string m_sName;
	int m_iIndex;
	
	protected OVT_TownManagerComponent m_Towns;
	
	void Init()
	{
		m_Towns = OVT_Global.GetTowns();
	}
	
	//Called after game init
	void OnPostInit()
	{
		
	}
	
	//Called at game start for each town
	void OnStart(OVT_TownData town)
	{
	
	}
	
	//Called every so often
	void OnTick(OVT_TownData town)
	{
	
	}
	
	//Called every so often when town has this modifier, return false to remove modifier
	bool OnActiveTick(OVT_TownData town)
	{
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Whether a killed character counts as a CIVILIAN death for the nearest town.
	//!
	//! Player characters are affiliated to CIV (Character_Player.et), so everything that separates a
	//! player from a townsperson at the moment of death lives here. The engine's controlled-entity
	//! lookup alone is not enough: it can already have been cleared by the time DESTROYED reaches
	//! script, and a player who slipped past it was counted as a murdered civilian - their own death
	//! costing their own town support and stability. The two prefab/controller checks behind it hold
	//! no matter what the player mapping says at that instant.
	//! \param[in] victim The killed entity
	//! \return True only for an AI civilian - never a player, a possessed body or a recruit
	protected bool IsCivilianCharacterDeath(IEntity victim)
	{
		if(!victim) return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(victim);
		if(!character) return false;

		// A player, by the engine's own mapping
		if(SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(victim) > 0) return false;

		// A player, by the character controller - still true where the mapping above has gone
		bool isPlayerBody = false;
		CharacterControllerComponent controller = character.GetCharacterController();
		if(controller && controller.IsPlayerControlled()) isPlayerBody = true;

		// A player, by the prefab: OVT_UIManagerComponent sits on the player character and on
		// nothing else, so this survives any death-time controller teardown
		if(victim.FindComponent(OVT_UIManagerComponent)) isPlayerBody = true;

		if(isPlayerBody)
		{
			// Reaching here means the engine had already let go of this body's player - the exact
			// hole player deaths fell through. Logged because it is the only way to watch it happen.
			Print("[Overthrow] Civilian-death modifier skipped a player body the controlled-entity lookup no longer recognised", LogLevel.NORMAL);
			return false;
		}

		// A recruit is the player's own AI, not a townsperson
		OVT_RecruitManagerComponent recruits = OVT_Global.GetRecruits();
		if(recruits && recruits.GetRecruitFromEntity(victim)) return false;

		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(victim.FindComponent(FactionAffiliationComponent));
		if(!factionComp) return false;

		Faction faction = factionComp.GetAffiliatedFaction();
		if(!faction) return false;

		return faction.GetFactionKey() == "CIV";
	}

	//Cleanup yourself here
	void OnDestroy()
	{
		
	}
}