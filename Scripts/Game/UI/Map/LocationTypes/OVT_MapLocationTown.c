//! Town location type for the new map system
//! Handles towns and cities with support/stability display
[BaseContainerProps()]
class OVT_MapLocationTown : OVT_MapLocationType
{
	[Attribute(defvalue: "Town", desc: "Display name for town locations")]
	protected string m_sTownDisplayName;
	
	[Attribute(defvalue: "1.5", desc: "Visibility zoom level for towns")]
	protected float m_fTownVisibilityZoom;
	
	[Attribute(defvalue: "{6ACF6BED95B2BC1E}UI/Layouts/Map/OVT_MapLocationElement.layout", UIWidgets.ResourceNamePicker, desc: "Town icon layout", params: "layout")]
	protected ResourceName m_sTownIconLayout;
	
	[Attribute(defvalue: "{6ACF6BED95B2BC20}UI/Layouts/Map/OVT_MapInfoTown.layout", UIWidgets.ResourceNamePicker, desc: "Town info layout", params: "layout")]
	protected ResourceName m_sTownInfoLayout;
	
	[Attribute(defvalue: "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset", UIWidgets.ResourceNamePicker, desc: "Town icon imageset", params: "imageset")]
	protected ResourceName m_sTownImageset;
	
	[Attribute(defvalue: "false", desc: "Allow fast travel to towns by default")]
	protected bool m_bTownFastTravel;
	
	override void PostInit()
	{
		super.PostInit();
		
		// Override base attributes with town-specific values
		m_sDisplayName = m_sTownDisplayName;
		m_fVisibilityZoom = m_fTownVisibilityZoom;
		m_IconLayout = m_sTownIconLayout;
		m_InfoLayout = m_sTownInfoLayout;
		m_IconImageset = m_sTownImageset;
		m_bCanFastTravel = m_bTownFastTravel;
		m_bShowDistance = true;
	}
	
	//! Populate town locations from the town manager
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_TownManager)
			return;
		
		// Iterate through all towns
		for (int i = 0; i < m_TownManager.m_Towns.Count(); i++)
		{
			OVT_TownData town = m_TownManager.m_Towns[i];
			if (!town)
				continue;
			
			// Create location data for this town
			string townName = m_TownManager.GetTownName(i);
			OVT_MapLocationData locationData = new OVT_MapLocationData(town.location, townName, ClassName());
			
			// Store town-specific data
			locationData.m_iID = i;
			locationData.SetDataInt("population", town.population);
			locationData.SetDataInt("support", town.support);
			locationData.SetDataInt("stability", town.stability);
			locationData.SetDataInt("faction", town.faction);
			locationData.SetDataString("townType", GetTownTypeString(town));
			
			// Set visibility based on town size or other criteria
			locationData.m_bVisible = ShouldShowTown(town, i);
			
			locations.Insert(locationData);
		}
	}
		
	//! Handle town selection
	override void OnLocationSelected(OVT_MapLocationData location, OVT_MapLocationElement element)
	{
		// Could add selection sound or visual feedback here
		Print(location.m_sName);
	}
	
	//! Handle town click
	override void OnLocationClicked(OVT_MapLocationData location, OVT_MapLocationElement element)
	{
		// Show town info panel
		super.OnLocationClicked(location, element);
	}
	
	//! Update town-specific info panel
	override void UpdateInfoPanel(OVT_MapLocationData location, Widget infoPanel)
	{
		if (!infoPanel || !location)
			return;
		
		// Create town info layout in the content slot
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget townInfoWidget = workspace.CreateWidgets(m_InfoLayout, infoPanel);
		if (!townInfoWidget)
			return;
		
		// Populate town data
		SetupTownInfo(townInfoWidget, location);
	}
	
	//! Setup town-specific information in the info widget
	protected void SetupTownInfo(Widget townInfoWidget, OVT_MapLocationData location)
	{
		if (!townInfoWidget || !location)
			return;
		
		// Population
		TextWidget populationText = TextWidget.Cast(townInfoWidget.FindAnyWidget("Population"));
		if (populationText)
		{
			int population = location.GetDataInt("population", 0);
			populationText.SetText(population.ToString());
		}
		
		// Support
		TextWidget supportText = TextWidget.Cast(townInfoWidget.FindAnyWidget("Support"));
		if (supportText)
		{
			int support = location.GetDataInt("support", 0);
			int population = location.GetDataInt("population", 1);
			float supportPercentage = (support * 100.0) / population;
			supportText.SetText(string.Format("%1 (%2%)", support, Math.Round(supportPercentage)));
		}
		
		// Stability
		TextWidget stabilityText = TextWidget.Cast(townInfoWidget.FindAnyWidget("Stability"));
		if (stabilityText)
		{
			int stability = location.GetDataInt("stability", 0);
			stabilityText.SetText(stability.ToString() + "%");
		}
		
		// Controlling faction icon
		ImageWidget factionIcon = ImageWidget.Cast(townInfoWidget.FindAnyWidget("FactionIcon"));
		if (factionIcon)
		{
			int townFaction = location.GetDataInt("faction", -1);
			if (townFaction >= 0)
			{
				FactionManager factionManager = GetGame().GetFactionManager();
				if (factionManager)
				{
					Faction faction = factionManager.GetFactionByIndex(townFaction);
					if (faction && faction.GetUIInfo())
					{
						string iconPath = faction.GetUIInfo().GetIconPath();
						if (!iconPath.IsEmpty())
							factionIcon.LoadImageTexture(0, iconPath);
					}
				}
			}
		}
	}
	
	//! Get location name with town type prefix
	override string GetLocationName(OVT_MapLocationData location)
	{
		if (!location)
			return "";
		
		string townType = location.GetDataString("townType", "");
		if (!townType.IsEmpty())
			return townType + " of " + location.m_sName;
		
		return location.m_sName;
	}
	
	//! Get town description
	override string GetLocationDescription(OVT_MapLocationData location)
	{
		if (!location)
			return m_sDisplayName;
		
		int population = location.GetDataInt("population", 0);
		if (population > 0)
			return string.Format("%1 (Pop: %2)", m_sDisplayName, population);
		
		return m_sDisplayName;
	}
	
	//! Check if town should be visible
	override bool ShouldShowLocation(OVT_MapLocationData location, string playerID)
	{
		// Always show towns for now
		// Could add fog of war or discovery mechanics here
		return location.m_bVisible;
	}
	
	//! Custom icon setup for towns
	override protected void OnSetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall)
	{
		if (!iconWidget || !location)
			return;
		
		ImageWidget icon = ImageWidget.Cast(iconWidget.FindAnyWidget("Icon"));
		if (!icon)
			return;
		
		// Set icon based on town size/type
		string iconName = m_sIconName;
				
		// Color based on faction control
		int townFaction = location.GetDataInt("faction", -1);
		if (townFaction >= 0)
		{
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				Faction faction = factionManager.GetFactionByIndex(townFaction);
				if (faction)
				{
					icon.SetColor(faction.GetFactionColor());
				}
			}
		}
	}
		
	//! Get town type string for display
	protected string GetTownTypeString(OVT_TownData town)
	{
		if (!town)
			return "";
		
		// Determine type based on population
		if (town.size == 1)
			return "Village";
		else if (town.size == 2)
			return "Town";
		else if (town.size == 3)
			return "City";
		return "Town";
	}
	
	//! Check if town should be shown based on size and other criteria
	protected bool ShouldShowTown(OVT_TownData town, int townIndex)
	{
		if (!town)
			return false;
		
		// Show all towns for now
		// Could add filters based on size, discovery, etc.
		return true;
	}
	
	//! Towns don't allow fast travel - player must use other locations
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		reason = "#OVT-CannotFastTravelThere";
		return false;
	}
}