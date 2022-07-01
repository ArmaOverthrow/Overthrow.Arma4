[BaseContainerProps()]
class OVT_OverthrowSaveStruct : SCR_MissionStruct
{
	[Attribute()]
	protected ref OVT_EconomyStruct m_EconomyStruct;
	
	[Attribute()]
	protected ref OVT_RealEstateStruct m_RealEstateStruct;
	
	[Attribute()]
	protected ref OVT_VehiclesStruct m_VehiclesStruct;
	
	[Attribute()]
	protected ref OVT_OccupyingFactionStruct m_OccupyingStruct;
	
	[Attribute()]
	protected ref OVT_ResistanceFactionStruct m_ResistanceStruct;
	
	protected string m_sOccupyingFaction;
	protected ref OVT_TimeDateStruct m_DateTime;
	protected ref array<ref OVT_TownStruct> m_aTowns = {};
	
	override bool Serialize()
	{
		if (m_EconomyStruct && !m_EconomyStruct.Serialize())
			return false;
		
		if (m_RealEstateStruct && !m_RealEstateStruct.Serialize())
			return false;
		
		if (m_VehiclesStruct && !m_VehiclesStruct.Serialize())
			return false;
		
		if (m_OccupyingStruct && !m_OccupyingStruct.Serialize())
			return false;
		
		if (m_ResistanceStruct && !m_ResistanceStruct.Serialize())
			return false;
		
		TimeAndWeatherManagerEntity timeMgr = GetGame().GetTimeAndWeatherManager();
		TimeContainer time = timeMgr.GetTime();
				
		m_DateTime = new OVT_TimeDateStruct();
		m_DateTime.m_iHours = time.m_iHours;
		m_DateTime.m_iMinutes = time.m_iMinutes;
		m_DateTime.m_iSeconds = time.m_iSeconds;
		timeMgr.GetDate(m_DateTime.m_iYear, m_DateTime.m_iMonth, m_DateTime.m_iDay);
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		m_sOccupyingFaction = config.m_sOccupyingFaction;
		
		foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
		{
			OVT_TownStruct struct = new OVT_TownStruct();
			struct.Parse(town);
			m_aTowns.Insert(struct);
		}
		
		return true;
	}
	override bool Deserialize()
	{
		if (m_EconomyStruct && !m_EconomyStruct.Deserialize())
			return false;
		
		if (m_RealEstateStruct && !m_RealEstateStruct.Deserialize())
			return false;
		
		if (m_VehiclesStruct && !m_VehiclesStruct.Deserialize())
			return false;
		
		if (m_OccupyingStruct && !m_OccupyingStruct.Deserialize())
			return false;
		
		if (m_ResistanceStruct && !m_ResistanceStruct.Deserialize())
			return false;
					
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		config.SetOccupyingFaction(m_sOccupyingFaction);
		
		if(m_DateTime)
		{
			TimeAndWeatherManagerEntity timeMgr = GetGame().GetTimeAndWeatherManager();
			timeMgr.SetDate(m_DateTime.m_iYear, m_DateTime.m_iMonth, m_DateTime.m_iDay, true);
			TimeContainer time = timeMgr.GetTime();
			time.m_iHours = m_DateTime.m_iHours;
			time.m_iMinutes = m_DateTime.m_iMinutes;
			time.m_iSeconds = m_DateTime.m_iSeconds;
			timeMgr.SetTime(time);
		}
		
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		mode.DoStartGame();
		
		foreach(OVT_TownData town : OVT_Global.GetTowns().m_Towns)
		{			
			foreach(OVT_TownStruct struct : m_aTowns)
			{
				float dist = vector.Distance(struct.m_vLocation, town.location);
				if(dist < 50)
				{
					town.population = struct.m_iPopulation;
					town.support = struct.m_iSupport;
					foreach(int i : struct.m_aSupportMods)
					{
						town.supportModifiers.Insert(i);
					}
					foreach(int i : struct.m_aSupportModTimers)
					{
						town.supportModifierTimers.Insert(i);
					}
					foreach(int i : struct.m_aStabilityMods)
					{
						town.stabilityModifiers.Insert(i);
					}
					foreach(int i : struct.m_aStabilityModTimers)
					{
						town.stabilityModifierTimers.Insert(i);
					}					
					
					Faction fac = GetGame().GetFactionManager().GetFactionByKey(struct.m_sFaction);
					if(fac)
					{
						town.faction = GetGame().GetFactionManager().GetFactionIndex(fac);
					}
					break;
				}
			}
		}
		
		return true;
	}
	
	void OVT_OverthrowSaveStruct()
	{
		RegV("m_EconomyStruct");
		RegV("m_RealEstateStruct");
		RegV("m_VehiclesStruct");
		RegV("m_OccupyingStruct");
		RegV("m_ResistanceStruct");
		RegV("m_sOccupyingFaction");
		RegV("m_aTowns");
		RegV("m_DateTime");
	}
}

class OVT_TimeDateStruct : SCR_JsonApiStruct
{
	int m_iHours;
	int m_iMinutes;
	int m_iSeconds;
	int m_iYear;
	int m_iMonth;
	int m_iDay;
	
	void OVT_TimeDateStruct()
	{
		RegV("m_iHours");
		RegV("m_iMinutes");
		RegV("m_iSeconds");
		RegV("m_iYear");
		RegV("m_iMonth");
		RegV("m_iDay");
	}
}

class OVT_TownStruct : SCR_JsonApiStruct
{
	vector m_vLocation;
	int m_iPopulation;
	int m_iSupport;
	string m_sFaction;
	ref array<ref int> m_aSupportMods = {};
	ref array<ref int> m_aSupportModTimers = {};
	ref array<ref int> m_aStabilityMods = {};	
	ref array<ref int> m_aStabilityModTimers = {};
	
	void Parse(OVT_TownData town)
	{
		FactionManager factions = GetGame().GetFactionManager();
		
		m_vLocation = town.location;
		m_iPopulation = town.population;
		m_iSupport = town.support;
		m_sFaction = factions.GetFactionByIndex(town.faction).GetFactionKey();
		m_aSupportMods = town.supportModifiers;
		m_aStabilityMods = town.stabilityModifiers;
		m_aSupportModTimers = town.supportModifierTimers;
		m_aStabilityModTimers = town.stabilityModifierTimers;
	}
	
	void OVT_TownStruct()
	{
		RegV("m_vLocation");
		RegV("m_iPopulation");
		RegV("m_iSupport");
		RegV("m_sFaction");
		RegV("m_aSupportMods");
		RegV("m_aSupportModTimers");
		RegV("m_aStabilityMods");
		RegV("m_aStabilityModTimers");
	}
}