//! Town location type for the new map system
//! Handles towns and cities with support/stability display
[BaseContainerProps()]
class OVT_MapLocationTown : OVT_MapLocationType
{	
	
	[Attribute(defvalue: "{EAA13B9FD255CB26}UI/Layouts/Map/OVT_TownModifierWidget.layout", UIWidgets.ResourceNamePicker, desc: "Town modifier widget layout", params: "layout")]
	protected ResourceName m_sTownModifierLayout;
	
	[Attribute(defvalue: "waypoint", desc: "Icon name for villages (size 1)")]
	protected string m_sVillageIconName;
	
	[Attribute(defvalue: "waypoint", desc: "Icon name for towns (size 2)")]
	protected string m_sTownIconName;
	
	[Attribute(defvalue: "waypoint", desc: "Icon name for cities (size 3)")]
	protected string m_sCityIconName;
	
	
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
						
			locations.Insert(locationData);
		}
	}
	
	//! Setup town-specific info panel content
	protected override void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		if (!locationInfoWidget || !location)
			return;
		
		// Populate town data
		SetupTownInfo(locationInfoWidget, location);
		
		// Setup modifiers
		SetupTownModifiers(locationInfoWidget, location);
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
			supportText.SetText(string.Format("%1 (%2%%)", support, Math.Round(supportPercentage)));
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
	
	//! Get display name - returns the specific town type (Village, Town, City)
	override string GetDisplayName()
	{
		// This will be overridden per location in GetDisplayNameForLocation
		return m_sDisplayName;
	}
	
	//! Get display name for a specific location - returns the town type
	override string GetDisplayNameForLocation(OVT_MapLocationData location)
	{
		if (!location)
			return m_sDisplayName;
		
		string townType = location.GetDataString("townType", "");
		if (!townType.IsEmpty())
			return townType;
		
		return m_sDisplayName;
	}
	
	//! Get icon color based on controlling faction
	override Color GetIconColor(OVT_MapLocationData location)
	{
		if (!location)
			return Color.Black;
		
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
					return faction.GetFactionColor();
				}
			}
		}
		
		// Default to black if no faction control
		return Color.Black;
	}
	
	//! Custom icon setup for towns - sets icon based on town size
	override protected void OnSetupIconWidget(Widget iconWidget, OVT_MapLocationData location, bool isSmall)
	{
		if (!iconWidget || !location)
			return;
		
		// Get the appropriate icon name based on town size
		string iconName = GetIconNameForTownSize(location);
		
		// Update the icon with the size-specific icon name
		ImageWidget image = ImageWidget.Cast(iconWidget.FindAnyWidget("Icon"));
		if (image && !m_IconImageset.IsEmpty() && !iconName.IsEmpty())
		{
			image.LoadImageFromSet(0, m_IconImageset, iconName);
			
			// Apply icon color (handled by base class but we need to reapply after changing icon)
			Color iconColor = GetIconColor(location);
			image.SetColor(iconColor);
		}
	}
	
	//! Get the appropriate icon name based on town size
	protected string GetIconNameForTownSize(OVT_MapLocationData location)
	{
		if (!location)
			return m_sIconName;
		
		string townType = location.GetDataString("townType", "");
		
		if (townType == "#OVT-Village")
			return m_sVillageIconName;
		else if (townType == "#OVT-Town")
			return m_sTownIconName;
		else if (townType == "#OVT-City")
			return m_sCityIconName;
		
		// Default fallback
		return m_sTownIconName;
	}
		
	//! Get town type string for display
	protected string GetTownTypeString(OVT_TownData town)
	{
		if (!town)
			return "";
		
		// Determine type based on population
		if (town.size == 1)
			return "#OVT-Village";
		else if (town.size == 2)
			return "#OVT-Town";
		else if (town.size == 3)
			return "#OVT-City";
		return "#OVT-Town";
	}
		
	//! Setup town modifiers display
	protected void SetupTownModifiers(Widget townInfoWidget, OVT_MapLocationData location)
	{
		if (!townInfoWidget || !location || !m_TownManager)
			return;
		
		// Get the actual town data
		int townID = location.m_iID;
		if (townID < 0 || townID >= m_TownManager.m_Towns.Count())
			return;
		
		OVT_TownData townData = m_TownManager.m_Towns[townID];
		if (!townData)
			return;
		
		// Setup support modifiers
		SetupModifierContainer(townInfoWidget, "SupportModifiersContainer", townData.supportModifiers, OVT_TownSupportModifierSystem);
		
		// Setup stability modifiers
		SetupModifierContainer(townInfoWidget, "StabilityModifiersContainer", townData.stabilityModifiers, OVT_TownStabilityModifierSystem);
	}
	
	//! Setup a specific modifier container
	protected void SetupModifierContainer(Widget parentWidget, string containerName, array<ref OVT_TownModifierData> modifiers, typename systemType)
	{
		if (!parentWidget || !m_TownManager)
			return;
		
		Widget container = parentWidget.FindAnyWidget(containerName);
		if (!container)
			return;
		
		// Clear existing modifiers
		Widget child = container.GetChildren();
		while (child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		
		// Get the modifier system
		OVT_TownModifierSystem modifierSystem = m_TownManager.GetModifierSystem(systemType);
		if (!modifierSystem)
			return;
		
		// Track processed modifier IDs to handle stackable modifiers
		array<int> processedIDs = new array<int>;
		
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		
		foreach (OVT_TownModifierData modifierData : modifiers)
		{
			if (processedIDs.Contains(modifierData.id))
				continue;
			
			OVT_ModifierConfig modifierConfig = modifierSystem.m_Config.m_aModifiers[modifierData.id];
			if (!modifierConfig)
				continue;
			
			// Calculate effect value (handle stackable modifiers)
			int effectValue = modifierConfig.baseEffect;
			if (modifierConfig.flags & OVT_ModifierFlags.STACKABLE)
			{
				effectValue = 0;
				foreach (OVT_TownModifierData checkData : modifiers)
				{
					if (checkData.id == modifierData.id)
						effectValue += modifierConfig.baseEffect;
				}
			}
			
			// Create modifier widget
			Widget modifierWidget = workspace.CreateWidgets(m_sTownModifierLayout, container);
			if (!modifierWidget)
				continue;
			
			// Get the handler and initialize it
			OVT_TownModifierWidgetHandler handler = OVT_TownModifierWidgetHandler.Cast(modifierWidget.FindHandler(OVT_TownModifierWidgetHandler));
			if (handler)
				handler.Init(modifierData, modifierConfig, effectValue);
			
			processedIDs.Insert(modifierData.id);
		}
	}
	
	//! Towns don't allow fast travel - player must use other locations
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		reason = "#OVT-CannotFastTravelThere";
		return false;
	}
}