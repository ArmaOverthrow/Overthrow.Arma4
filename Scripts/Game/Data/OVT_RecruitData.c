//! Data structure for storing AI recruit information
class OVT_RecruitData : Managed
{
	//! Unique identifier for this recruit
	string m_sRecruitId;
	
	//! Display name of the recruit
	string m_sName;
	
	//! Persistent ID of the player who owns this recruit
	string m_sOwnerPersistentId;
	
	//! Number of enemies killed by this recruit
	int m_iKills = 0;
	
	//! Experience points accumulated
	int m_iXP = 0;
	
	//! Current level (cached for performance)
	int m_iLevel = 1;
	
	//! Skills and their levels
	ref map<string, int> m_mSkills = new map<string, int>;
	
	//! Whether the recruit is currently in training
	bool m_bIsTraining = false;
	
	//! Game time when training will be complete
	float m_fTrainingCompleteTime = 0;
	
	//! Last known position of the recruit
	vector m_vLastKnownPosition = "0 0 0";

	//! Persistence system id (UUID as a string) of this recruit's BODY.
	//!
	//! A recruit body is an ordinary tracked character: vanilla's AI-character configuration serializes
	//! it with its whole inventory, and Overthrow only turns OFF the "spawn it back by itself" bit
	//! (SelfSpawn 0, Configs/Systems/Persistence/Overthrow.conf). This id is how the manager asks for
	//! THAT body back - gear and all - instead of building a new one from the prefab.
	//!
	//! Empty means "no body has been stored for this recruit"; the respawn path then falls back to a
	//! fresh prefab with a civilian loadout. NEVER compare it as a UUID - it is kept as a string
	//! because that is what OVT_PersistenceTracking hands out and what the record persists.
	string m_sBodyPersistenceId;

	//! Whether the recruit entity is currently spawned in the world
	bool m_bIsOnline = false;

	//! Whether this recruit is INACTIVE: owned by the player, but deliberately taken OUT of the owner's
	//! group to hold the position where it stands. An inactive recruit is still owned, still counts
	//! against the per-player recruit cap, is still persisted and is still respawned with its owner.
	//!
	//! THIS IS NOT m_bIsOnline. That flag is about having a BODY in the world - a fact about this
	//! session, decided by the spawn/despawn path. This one is about SQUAD MEMBERSHIP - a campaign fact
	//! the player chose, which outlives the body. The four combinations are all reachable and all mean
	//! different things, so neither may ever be read as a proxy for the other.
	//!
	//! SERVER-AUTHORITATIVE. Clients receive it through the JIP payload
	//! (OVT_RecruitManagerComponent.RplSave/RplLoad) and, for live changes, through the dedicated
	//! broadcast RPC RpcDo_RecruitActiveStateChanged. A client never writes it.
	bool m_bInactive = false;

	//! ID of the town where this recruit was hired from
	int m_iTownId = -1;
	
	//------------------------------------------------------------------------------------------------
	//! Calculate level from XP (same formula as player)
	int GetLevel()
	{
		return Math.Floor(1 + (0.1 * Math.Sqrt(m_iXP)));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get XP required for next level
	int GetNextLevelXP()
	{
		int level = GetLevel();
		return Math.Pow(level / 0.1, 2);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get XP required for a specific level
	int GetLevelXP(int level)
	{
		return Math.Pow(level / 0.1, 2);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get progress to next level as percentage (0-1)
	float GetLevelProgress()
	{
		int levelFromXP = GetLevelXP(GetLevel() - 1);
		int levelToXP = GetNextLevelXP();
		int total = levelToXP - levelFromXP;
		int current = m_iXP - levelFromXP;
		
		if (total <= 0)
			return 0;
			
		return current / total;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add experience points and update level
	void AddXP(int xp)
	{
		m_iXP += xp;
		m_iLevel = GetLevel();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if recruit has a specific skill
	bool HasSkill(string skillName)
	{
		return m_mSkills.Contains(skillName);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get skill level (0 if not learned)
	int GetSkillLevel(string skillName)
	{
		if (!HasSkill(skillName))
			return 0;
			
		return m_mSkills[skillName];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add or increase skill level
	void AddSkill(string skillName, int level = 1)
	{
		if (HasSkill(skillName))
			m_mSkills[skillName] = m_mSkills[skillName] + level;
		else
			m_mSkills[skillName] = level;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set the recruit's display name
	void SetName(string name)
	{
		m_sName = name;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the recruit's display name
	string GetName()
	{
		return m_sName;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the recruit's hometown name
	string GetHometown()
	{
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (townManager && m_iTownId != -1)
		{
			return townManager.GetTownName(m_iTownId);
		}
		return "Unknown";
	}
	
	//------------------------------------------------------------------------------------------------
	//! Static method to get recruit data from entity
	static OVT_RecruitData GetRecruitDataFromEntity(IEntity entity)
	{
		if (!entity)
			return null;
			
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (!recruitManager)
			return null;
			
		return recruitManager.GetRecruitFromEntity(entity);
	}
}