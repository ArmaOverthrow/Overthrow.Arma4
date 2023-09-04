class OVT_PlayerData : Managed
{
	[NonSerialized()]
	int id;
	
	[NonSerialized()]
	bool firstSpawn;
	
	//Persisted
	string name;	
	vector home;
	vector camp;	
	int money;
	bool initialized;	
	bool isOfficer;
	
	int kills;
	int xp;
	int levelNotified=1;
	ref map<string,int> skills = new map<string,int>;
	
	//Not persisted	(controlled by skill effects)
	[NonSerialized()]
	float priceMultiplier=1;
	
	[NonSerialized()]
	float stealthMultiplier=1;
	
	[NonSerialized()]
	float diplomacy=0.1;
	
	[NonSerialized()]
	ref array<string> permissions = {};
	
	bool IsOffline()
	{
		return id == -1;
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