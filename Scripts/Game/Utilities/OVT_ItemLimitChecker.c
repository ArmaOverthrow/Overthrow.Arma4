//! Utility class for checking item placement limits at different locations
class OVT_ItemLimitChecker
{
	protected int m_iItemCount;
	protected string m_sTargetLocationId;
	protected EOVTBaseType m_eTargetBaseType;
	
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_OccupyingFactionManager m_OccupyingFaction;
	protected OVT_ResistanceFactionManager m_Resistance;
	protected OVT_TownManagerComponent m_Towns;
	
	protected const float MAX_HOUSE_PLACE_DIS = 30;
	protected const float MAX_CAMP_PLACE_DIS = 75;
	protected const float MAX_FOB_PLACE_DIS = 100;
	protected const int MAX_CAMP_BUILD_DIS = 50;
	protected const int MAX_FOB_BUILD_DIS = 100;
	
	void OVT_ItemLimitChecker()
	{
		m_RealEstate = OVT_Global.GetRealEstate();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		m_Resistance = OVT_Global.GetResistanceFaction();
		m_Towns = OVT_Global.GetTowns();
	}
	
	//! Check if placing/building an item would exceed the limit at the given position
	bool CanPlaceItem(vector pos, string playerID, out string reason)
	{
		string locationId;
		EOVTBaseType baseType;
		int itemCount = CountItemsAtLocation(pos, playerID, locationId, baseType);
		
		if(itemCount > 0)
		{
			int limit = 0;
			if(baseType == EOVTBaseType.NONE)
				limit = OVT_Global.GetConfig().GetHouseItemLimit();
			else if(baseType == EOVTBaseType.CAMP)
				limit = OVT_Global.GetConfig().GetCampItemLimit();
			else if(baseType == EOVTBaseType.FOB || baseType == EOVTBaseType.BASE)
				limit = OVT_Global.GetConfig().GetFOBItemLimit();
			
			// If limit is 0 or negative, allow unlimited items
			if(limit <= 0)
				return true;
			
			if(itemCount >= limit)
			{
				reason = "#OVT-ItemLimitReached";
				return false;
			}
		}
		
		return true;
	}
	
	//! Count items at a location (for placement context)
	int CountItemsAtLocation(vector pos, string playerID, out string locationId, out EOVTBaseType baseType)
	{
		locationId = "";
		baseType = EOVTBaseType.NONE;
		
		IEntity house = m_RealEstate.GetNearestOwned(playerID, pos);
		if(house)
		{
			float dist = vector.Distance(house.GetOrigin(), pos);
			if(dist < MAX_HOUSE_PLACE_DIS)
			{
				locationId = house.GetID().ToString();
				baseType = EOVTBaseType.NONE;
				return CountItemsForLocation(locationId, baseType, house.GetOrigin());
			}
		}
		
		OVT_CampData camp = m_Resistance.GetNearestCampData(pos);	
		if(camp)
		{	
			float dist = vector.Distance(camp.location, pos);
			if(dist < MAX_CAMP_PLACE_DIS && camp.owner == playerID)
			{
				locationId = camp.persistentId;
				baseType = EOVTBaseType.CAMP;
				return CountItemsForLocation(locationId, baseType, camp.location);
			}
		}
		
		OVT_FOBData fob = m_Resistance.GetNearestFOBData(pos);	
		if(fob)
		{	
			float dist = vector.Distance(fob.location, pos);
			if(dist < MAX_FOB_PLACE_DIS)
			{
				locationId = fob.persistentId;
				baseType = EOVTBaseType.FOB;
				return CountItemsForLocation(locationId, baseType, fob.location);
			}
		}
		
		OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
		if(!base) return 0;
		float dist = vector.Distance(base.location, pos);
		if(!base.IsOccupyingFaction() && dist < OVT_Global.GetConfig().m_Difficulty.baseRange)
		{
			locationId = base.id.ToString();
			baseType = EOVTBaseType.BASE;
			return CountItemsForLocation(locationId, baseType, base.location);
		}
		
		return 0;
	}
	
	//! Count items at a location (for build context)
	int CountItemsAtLocationForBuild(vector pos, out string locationId, out EOVTBaseType baseType)
	{
		locationId = "";
		baseType = EOVTBaseType.NONE;
		
		OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
		if(!base) return 0;
		float dist = vector.Distance(base.location, pos);
		if(!base.IsOccupyingFaction() && dist < OVT_Global.GetConfig().m_Difficulty.baseRange)
		{
			locationId = base.id.ToString();
			baseType = EOVTBaseType.BASE;
			return CountItemsForLocation(locationId, baseType, base.location);
		}
		
		OVT_TownData town = m_Towns.GetNearestTown(pos);
		dist = vector.Distance(town.location, pos);
		int range = m_Towns.m_iCityRange;
		if(town.size < 3) range = m_Towns.m_iTownRange;
		if(town.size < 2) range = m_Towns.m_iVillageRange;
		if(dist < range)
		{
			locationId = town.location.ToString();
			baseType = EOVTBaseType.NONE;
			return CountItemsForLocation(locationId, baseType, town.location);
		}
		
		OVT_CampData camp = m_Resistance.GetNearestCampData(pos);
		if(camp)
		{
			dist = vector.Distance(camp.location, pos);
			if(dist < MAX_CAMP_BUILD_DIS)
			{
				locationId = camp.persistentId;
				baseType = EOVTBaseType.CAMP;
				return CountItemsForLocation(locationId, baseType, camp.location);
			}
		}
		
		OVT_FOBData fob = m_Resistance.GetNearestFOBData(pos);
		if(fob)
		{
			dist = vector.Distance(fob.location, pos);
			if(dist < MAX_FOB_BUILD_DIS)
			{
				locationId = fob.persistentId;
				baseType = EOVTBaseType.FOB;
				return CountItemsForLocation(locationId, baseType, fob.location);
			}
		}
		
		return 0;
	}
	
	//! Count items for a specific location using QueryEntitiesBySphere
	int CountItemsForLocation(string locationId, EOVTBaseType baseType, vector searchCenter)
	{
		m_iItemCount = 0;
		m_sTargetLocationId = locationId;
		m_eTargetBaseType = baseType;
		
		// Determine search radius based on location type
		float searchRadius = 200; // Default radius
		if(baseType == EOVTBaseType.NONE) // House/Town
			searchRadius = MAX_HOUSE_PLACE_DIS + 50;
		else if(baseType == EOVTBaseType.CAMP)
			searchRadius = MAX_CAMP_PLACE_DIS + 50;
		else if(baseType == EOVTBaseType.FOB)
			searchRadius = MAX_FOB_PLACE_DIS + 50;
		else if(baseType == EOVTBaseType.BASE)
			searchRadius = 500; // Bases can be quite large
		
		BaseWorld world = GetGame().GetWorld();
		world.QueryEntitiesBySphere(
			searchCenter, 
			searchRadius, 
			CountItemCallback, 
			FilterItemCallback, 
			EQueryEntitiesFlags.ALL // Include both static and dynamic entities
		);
		
		return m_iItemCount;
	}
	
	//! Filter callback - only process entities with placeable or buildable components
	protected bool FilterItemCallback(IEntity entity)
	{
		if(!entity) return false;
		
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		if(placeableComp) return true;
		
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if(buildableComp) return true;
		
		return false;
	}
	
	//! Count callback - increment count for matching items
	protected bool CountItemCallback(IEntity entity)
	{
		if(!entity) return true;
		
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		if(placeableComp)
		{
			if(m_eTargetBaseType == EOVTBaseType.NONE)
			{
				if(placeableComp.GetAssociatedBaseId() == m_sTargetLocationId || placeableComp.GetAssociatedBaseId() == "")
					m_iItemCount++;
			}
			else if(placeableComp.GetAssociatedBaseId() == m_sTargetLocationId && placeableComp.GetBaseType() == m_eTargetBaseType)
			{
				m_iItemCount++;
			}
		}
		
		OVT_BuildableComponent buildableComp = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if(buildableComp)
		{
			if(m_eTargetBaseType == EOVTBaseType.NONE)
			{
				if(buildableComp.GetAssociatedBaseId() == m_sTargetLocationId || buildableComp.GetAssociatedBaseId() == "")
					m_iItemCount++;
			}
			else if(buildableComp.GetAssociatedBaseId() == m_sTargetLocationId && buildableComp.GetBaseType() == m_eTargetBaseType)
			{
				m_iItemCount++;
			}
		}
		
		return true;
	}
}