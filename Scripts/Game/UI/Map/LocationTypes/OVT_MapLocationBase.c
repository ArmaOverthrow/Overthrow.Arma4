//! Military base location type for the new map system
//! Handles military bases with faction control and garrison display
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationBase : OVT_MapLocationType
{	
	[Attribute(defvalue: "{40B12B0DF911B856}UI/Textures/Editor/EditableEntities/Factions/EditableEntity_Faction_USSR.edds", UIWidgets.ResourceNamePicker, desc: "Default faction icon for bases", params: "edds")]
	protected ResourceName m_DefaultFactionIcon;
	
	[Attribute(defvalue: "base", desc: "Icon name for resistance-controlled bases")]
	protected string m_sResistanceIconName;
	
	[Attribute(defvalue: "base_enemy", desc: "Icon name for enemy-controlled bases")]
	protected string m_sEnemyIconName;
	
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	
	//! Populate base locations from the occupying faction manager
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		if (!m_OccupyingFactionManager)
			m_OccupyingFactionManager = OVT_Global.GetOccupyingFaction();
		
		if (!m_OccupyingFactionManager)
			return;
		
		// Iterate through all bases
		array<ref OVT_BaseData> bases = m_OccupyingFactionManager.m_Bases;
		if (!bases)
			return;
			
		for (int i = 0; i < bases.Count(); i++)
		{
			OVT_BaseData base = bases[i];
			if (!base)
				continue;
			
			// Get base controller to get the name
			IEntity baseEntity = GetGame().GetWorld().FindEntityByID(base.entId);
			if (!baseEntity)
				continue;
				
			OVT_BaseControllerComponent controller = OVT_BaseControllerComponent.Cast(baseEntity.FindComponent(OVT_BaseControllerComponent));
			if (!controller)
				continue;
			
			// Create location data for this base
			string baseName = controller.m_sName;
			if (baseName.IsEmpty())
				baseName = "Military Base";
				
			OVT_MapLocationData locationData = new OVT_MapLocationData(base.location, baseName, ClassName());
			
			// Store base-specific data
			locationData.m_iID = i;
			locationData.SetDataInt("faction", base.faction);
			locationData.SetDataBool("isOccupying", base.IsOccupyingFaction());
			locationData.SetDataInt("garrisonCount", base.garrison.Count());
			
						
			locations.Insert(locationData);
		}
	}
	
	//! Setup base-specific info panel content
	protected override void OnSetupLocationInfo(Widget locationInfoWidget, OVT_MapLocationData location)
	{
		if (!locationInfoWidget || !location)
			return;
		
		// Populate base data
		SetupBaseInfo(locationInfoWidget, location);
	}
	
	//! Setup base-specific information in the info widget
	protected void SetupBaseInfo(Widget baseInfoWidget, OVT_MapLocationData location)
	{
		if (!baseInfoWidget || !location)
			return;
		
		// Garrison size
		TextWidget garrisonText = TextWidget.Cast(baseInfoWidget.FindAnyWidget("Garrison"));
		if (garrisonText)
		{
			int garrisonCount = location.GetDataInt("garrisonCount", 0);
			garrisonText.SetText(garrisonCount.ToString());
		}
		
		// Controlling faction icon
		ImageWidget factionIcon = ImageWidget.Cast(baseInfoWidget.FindAnyWidget("FactionIcon"));
		if (factionIcon)
		{
			int baseFaction = location.GetDataInt("faction", -1);
			if (baseFaction >= 0)
			{
				FactionManager factionManager = GetGame().GetFactionManager();
				if (factionManager)
				{
					Faction faction = factionManager.GetFactionByIndex(baseFaction);
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

		// Color based on faction control, through the one shared lookup the town and radio-tower markers
		// and the territory overlay also use. The WHITE fallback is this marker's own and stays here -
		// the town marker falls back to black, so no single shared fallback could serve both.
		Color factionColor = OVT_MapLocationType.GetFactionColorByIndex(location.GetDataInt("faction", -1));
		if (factionColor)
			return factionColor;

		// Default to white if no faction control
		return Color.White;
	}
		
	//! Get the appropriate icon name based on faction control
	protected string GetIconNameForBase(OVT_MapLocationData location)
	{
		if (!location)
			return m_sIconName;
		
		bool isOccupying = location.GetDataBool("isOccupying", true);
		
		if (isOccupying)
			return m_sEnemyIconName;
		else
			return m_sResistanceIconName;
	}
	
	//! Bases allow fast travel when controlled by resistance
	override bool CanFastTravel(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!location)
		{
			reason = "#OVT-CannotFastTravelThere";
			return false;
		}
		
		// Check if base is controlled by resistance
		bool isOccupying = location.GetDataBool("isOccupying", true);
		if (isOccupying)
		{
			reason = "#OVT-CannotFastTravelEnemyBase";
			return false;
		}
		
		// Check global fast travel restrictions
		return OVT_FastTravelService.CanGlobalFastTravel(location.m_vPosition, playerID, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! A base the occupying faction still holds is theirs; anything else may be respawned at.
	//!
	//! Reads the same "isOccupying" key CanFastTravel does, minus the global fast-travel tail, whose
	//! distance, wanted and fare rules are all measured from a living player and are meaningless for
	//! a respawn.
	//! \param[in] location The record being tested
	//! \param[in] playerID Persistent id of the local player
	//! \param[out] reason Localization key explaining a refusal
	//! \return True when this player may respawn at this base
	override bool CanRespawn(OVT_MapLocationData location, string playerID, out string reason)
	{
		if (!location)
		{
			reason = "#OVT-Respawn_NotEligible";
			return false;
		}

		if (!OVT_RespawnService.IsBaseEligible(location.GetDataBool("isOccupying", true)))
		{
			reason = "#OVT-Respawn_EnemyBase";
			return false;
		}

		if (OVT_RespawnService.IsPositionInActiveQRF(location.m_vPosition))
		{
			reason = "#OVT-Respawn_QRF";
			return false;
		}

		return true;
	}
}