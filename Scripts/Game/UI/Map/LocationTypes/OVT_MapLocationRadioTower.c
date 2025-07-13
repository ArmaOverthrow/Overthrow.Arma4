//! Radio tower location type for the Overthrow map system
//! Handles radio towers with faction control and flag display
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationRadioTower : OVT_MapLocationType
{	
	[Attribute(defvalue: "{40B12B0DF911B856}UI/Textures/Editor/EditableEntities/Factions/EditableEntity_Faction_USSR.edds", UIWidgets.ResourceNamePicker, desc: "Default faction icon for radio towers", params: "edds")]
	protected ResourceName m_DefaultFactionIcon;
	
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	
	//! Populate radio tower locations from the occupying faction manager
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_OccupyingFactionManager)
			m_OccupyingFactionManager = OVT_Global.GetOccupyingFaction();
		
		if (!m_OccupyingFactionManager)
			return;
		
		// Iterate through all radio towers
		array<ref OVT_RadioTowerData> towers = m_OccupyingFactionManager.m_RadioTowers;
		if (!towers)
			return;
			
		for (int i = 0; i < towers.Count(); i++)
		{
			OVT_RadioTowerData tower = towers[i];
			if (!tower)
				continue;
			
			// Create location data for this radio tower			
			OVT_MapLocationData locationData = new OVT_MapLocationData(tower.location, "#OVT-RadioTower", ClassName());
			
			// Store radio tower-specific data
			locationData.m_iID = i;
			locationData.SetDataInt("faction", tower.faction);
			
			locations.Insert(locationData);
		}
	}
	
	//! Setup radio tower-specific info panel content
	protected override void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		if (!locationInfoWidget || !location)
			return;
		
		// Populate radio tower data
		SetupRadioTowerInfo(locationInfoWidget, location);
	}
	
	//! Setup radio tower-specific information in the info widget
	protected void SetupRadioTowerInfo(Widget towerInfoWidget, OVT_MapLocationData location)
	{
		if (!towerInfoWidget || !location)
			return;
		
		// Controlling faction icon
		ImageWidget factionIcon = ImageWidget.Cast(towerInfoWidget.FindAnyWidget("FactionIcon"));
		if (factionIcon)
		{
			int towerFaction = location.GetDataInt("faction", -1);
			if (towerFaction >= 0)
			{
				FactionManager factionManager = GetGame().GetFactionManager();
				if (factionManager)
				{
					Faction faction = factionManager.GetFactionByIndex(towerFaction);
					if (faction && faction.GetUIInfo())
					{
						string iconPath = faction.GetUIInfo().GetIconPath();
						if (!iconPath.IsEmpty())
							factionIcon.LoadImageTexture(0, iconPath);
						else
							factionIcon.LoadImageTexture(0, m_DefaultFactionIcon);
					}
				}
			}
		}
	}
	
	//! Get icon color based on controlling faction
	override Color GetIconColor(OVT_MapLocationData location)
	{
		if (!location)
			return Color.White;
		
		// Color based on faction control
		int towerFaction = location.GetDataInt("faction", -1);
		if (towerFaction >= 0)
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				Faction faction = factionManager.GetFactionByIndex(towerFaction);
				if (faction)
				{
					return faction.GetFactionColor();
				}
			}
		}
		
		// Default to white if no faction control
		return Color.White;
	}
}