class OVT_PlayerData : Managed
{
	[NonSerialized()]
	int id=0;
	
	[NonSerialized()]
	bool firstSpawn = true;
	
	//Persisted
	string name="";	
	vector home="0 0 0";
	vector camp="0 0 0";	
	int money = 0;
	bool initialized = false;	
	bool isOfficer = false;
	
	int kills = 0;
	int xp = 0;
	int levelNotified=1;
	ref map<string,int> skills = new map<string,int>;

	//Persisted, SERVER ONLY
	//------------------------------------------------------------------------------------------------
	//! Persistence id of the player's stored CHARACTER, so the same body - with the gear it was
	//! carrying - can be spawned back after a quit/continue or a reconnect instead of a fresh one.
	//! Empty when no body has been stored.
	//!
	//! WRITTEN in exactly three places and nowhere else:
	//!   - OVT_PlayerManagerComponent.SyncPlayerBodyIds(), from PreShutdownPersist, before every save;
	//!   - OVT_OverthrowGameMode.OnPlayerDisconnected(), after the leaving character is written;
	//!   - OVT_SpawnLogic, which refreshes it on handover and CLEARS it on death.
	//!
	//! CLEARED ON DEATH. Death is complete loss in Overthrow - a killed player respawns as a fresh
	//! civilian and the corpse stays where it fell as lootable remains. The id is what would bring the
	//! old body back, so OVT_SpawnLogic.OnPlayerKilled_S drops it before the respawn is scheduled.
	//!
	//! NEVER REPLICATED. It is a handle into the persistence system, which only exists on the authority
	//! (SystemLocation Server); a client can neither resolve it nor spawn anything with it. It is
	//! deliberately absent from OVT_PlayerManagerComponent's RplSave/RplLoad JIP payload - do not add
	//! it there. It IS persisted, by OVT_PlayerManagerSerializer (version 2), which writes it by hand.
	string m_sBodyPersistenceId = "";

	//------------------------------------------------------------------------------------------------
	//! Where the player's body was standing when it was last written down, and which way it faced.
	//! The zero vector means "no stored position" - fall back to the player's home.
	//!
	//! WHY THIS EXISTS SEPARATELY FROM m_sBodyPersistenceId. The stored body carries its own transform,
	//! so when it can be spawned back this pair is redundant. It is the answer for when it CANNOT:
	//! measured on a dedicated server (2026-08-04), a body released with StopTracking(keepData) was
	//! answered NOT_FOUND after a server restart, and the player was rebuilt at their home on the far
	//! side of the map. Gear can be lost that way; POSITION must not be, so it is stored as plain data
	//! on the player's own record, which travels in the game-mode record and does not depend on the
	//! character record surviving at all.
	//!
	//! WRITTEN wherever m_sBodyPersistenceId is written, and - deliberately - even when capturing the id
	//! FAILS, which is exactly the case this exists for.
	//!
	//! CLEARED ON DEATH, with the id. Death is complete loss and sends the player home; leaving the
	//! position set would respawn them on their own corpse instead.
	//!
	//! NEVER REPLICATED, same reasoning as the id above. Persisted by OVT_PlayerManagerSerializer
	//! (version 3).
	vector m_vLastKnownPosition = "0 0 0";

	//! Yaw/pitch/roll to match m_vLastKnownPosition. Meaningless on its own - always test the position.
	vector m_vLastKnownAngles = "0 0 0";

	//------------------------------------------------------------------------------------------------
	//! Tutorial entry ids this player has dismissed, part of the campaign record like money and XP.
	//!
	//! MOVED HERE FROM THE PER-MACHINE PROFILE (2026-08-18, user decision): the ModuleGameSettings
	//! store proved unreliable in practice - tips re-showed on every load - so seen-tutorial state now
	//! rides the same save the rest of the campaign does. Consequence, accepted: a NEW campaign shows
	//! the tips again.
	//!
	//! SERVER-AUTHORITATIVE. The owning client mirrors this through OVT_TutorialComponent (pushed once
	//! at player finalization, mutated back through that component's server RPCs); it is NOT in the
	//! player manager's RplSave/RplLoad JIP payload - no other client has any use for it.
	ref array<string> m_aSeenTutorials = {};

	//! The player's "Don't show tips again" choice, persisted and synced exactly as the ids above.
	bool m_bTutorialsDisabled = false;

	//Not persisted	(controlled by skill effects)
	[NonSerialized()]
	float priceMultiplier=1;
	
	[NonSerialized()]
	float stealthMultiplier=1;
	
	[NonSerialized()]
	float diplomacy=0.1;
	
	[NonSerialized()]
	ref array<string> permissions = {};
	
	//------------------------------------------------------------------------------------------------
	//! True when this record has no live runtime player behind it: id is 0 for a record that has not
	//! connected this session (e.g. loaded from a save) and -1 after OnPlayerDisconnected.
	bool IsOffline()
	{
		return id <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Resets every field that is DERIVED from skill levels back to the value a fresh player has.
	//!
	//! These are the [NonSerialized()] outputs of the skill effects, not stored state: they exist
	//! only as the accumulated result of replaying each earned skill level's effects. Anything that
	//! REPLAYS those effects (loading a saved player record, re-applying persisted data to a live
	//! session) has to zero them first or the effects stack on top of themselves.
	//!
	//! The defaults here are the field initializers above and must stay in step with them.
	void ResetSkillEffects()
	{
		priceMultiplier = 1;
		stealthMultiplier = 1;
		diplomacy = 0.1;

		if (!permissions)
			permissions = new array<string>();
		else
			permissions.Clear();
	}
	
	float GetRawLevel()
	{
		return 1 + (0.1 * Math.Sqrt(xp));
	}
	
	int GetLevel()
	{		
		return Math.Floor(GetRawLevel());
	}	
	
	bool HasPermission(string perm)
	{
		return permissions.Contains(perm);
	}
	
	void GivePermission(string perm)
	{
		if(HasPermission(perm)) return;
		permissions.Insert(perm);
	}
	
	float GetLevelProgress()
	{
		int levelFromXP = GetLevelXP(GetLevel()-1);
		int levelToXP = GetNextLevelXP();
		int total = levelToXP - levelFromXP;
		int current = xp - levelFromXP;
		
		return current / total;
	}
	
	int GetLevelXP(int level)
	{
		return Math.Pow(level / 0.1,2);
	}
	
	int GetNextLevelXP()
	{
		int level = GetLevel();
		return Math.Pow(level / 0.1,2);
	}
	
	int CountSkills()
	{
		int count = 0;
		for(int t=0; t < skills.Count(); t++)
		{
			count += skills.GetElement(t);
		}
		
		return count;
	}
	
	static OVT_PlayerData Get(string persId)
	{
		return OVT_Global.GetPlayers().GetPlayer(persId);
	}	
	
	static OVT_PlayerData Get(int playerId)
	{
		OVT_PlayerManagerComponent pm = OVT_Global.GetPlayers();
		string persId = pm.GetPersistentIDFromPlayerID(playerId);
		return pm.GetPlayer(persId);
	}
}