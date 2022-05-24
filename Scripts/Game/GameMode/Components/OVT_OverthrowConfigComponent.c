class OVT_OverthrowConfigComponentClass: OVT_ComponentClass
{
};


class OVT_OverthrowConfigComponent: OVT_Component
{
	[Attribute( defvalue: "FIA", uiwidget: UIWidgets.EditBox, desc: "Faction affiliation of the player's side")]
	string m_sPlayerFaction;
	
	[Attribute( defvalue: "USSR", uiwidget: UIWidgets.EditBox, desc: "The faction occupying this map (the enemy)")]
	string m_sOccupyingFaction;
	
	[Attribute( defvalue: "US", uiwidget: UIWidgets.EditBox, desc: "The faction supporting the player")]
	string m_sSupportingFaction;
	
	// Instance of this component
	private static OVT_OverthrowConfigComponent s_Instance = null;
	
	static OVT_OverthrowConfigComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_OverthrowConfigComponent.Cast(pGameMode.FindComponent(OVT_OverthrowConfigComponent));
		}

		return s_Instance;
	}
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(owner);
		if (!gameMode)
			// If parent is not gamemode, print an error
			Print("OVT_OverthrowConfigComponent has to be attached to a OVT_OverthrowGameMode (or inherited) entity!", LogLevel.ERROR);
		
		
		if (GetGame().InPlayMode())
		{
			// Validate faction manager
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				array<Faction> _ = {};
				int factionsCount = factionManager.GetFactionsList(_);
	
				if (factionsCount <= 0)
				{
					Debug.Error("No faction(s) found in FactionManager, OVT_OverthrowConfigComponent will malfunction!");
					Print("OVT_OverthrowConfigComponent found FactionManager, but no Factions are defined!", LogLevel.ERROR);
				}
			}
			else
			{
				Debug.Error("No FactionManager found, OVT_OverthrowConfigComponent will malfunction!");
				Print("OVT_OverthrowConfigComponent could not find a FactionManager, is one present in the world?", LogLevel.ERROR);
			}
		}
	}
}