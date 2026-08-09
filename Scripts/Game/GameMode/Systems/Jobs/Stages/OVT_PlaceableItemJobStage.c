class OVT_PlaceableItemJobStage : OVT_JobStage
{
	[Attribute("", UIWidgets.EditBox, "Name of placeable item to check for (from placeables.conf)")]
	string m_sPlaceableName;
	
	[Attribute("25", UIWidgets.EditBox, "Search radius in meters. Ignored when m_bInJobTown is set")]
	float m_fSearchRadius;

	//! Off by default so the five onboarding jobs keep their original "anywhere near you" behaviour.
	//! Turn it on for town jobs whose whole point is WHERE the thing goes - and pair it with
	//! OVT_TownPlaceableCountJobCondition, which counts over exactly the same radius
	[Attribute("0", UIWidgets.CheckBox, "Require the placeable to be inside the job's town, instead of near the player")]
	bool m_bInJobTown;

	protected ref array<IEntity> m_aFoundEntities;

	override bool OnTick(OVT_Job job)
	{
		if (m_sPlaceableName.IsEmpty())
			return false;

		if (job.owner.IsEmpty())
			return true;

		string jobOwnerPersistentId = job.owner;

		vector searchCenter;
		float searchRadius = m_fSearchRadius;

		if (m_bInJobTown)
		{
			// Anchored to the town, not the player: the placement that finishes this job must be the
			// same one OVT_TownPlaceableCountJobCondition will count when deciding whether to offer
			// the job here again. The player does not need to still be standing next to it
			OVT_TownData town = job.GetTown();
			if (!town)
				return true;

			searchCenter = town.location;
			searchRadius = OVT_Global.GetTowns().GetTownRange(town);
		}
		else
		{
			// Get player data and entity
			OVT_PlayerData playerData = OVT_Global.GetPlayers().GetPlayer(job.owner);
			if (!playerData)
				return true;

			int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);

			IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!player)
				return true;

			searchCenter = player.GetOrigin();
		}

		// Clear previous results
		m_aFoundEntities = new array<IEntity>();

		// Query entities in sphere around the search center
		GetGame().GetWorld().QueryEntitiesBySphere(searchCenter, searchRadius, null, FilterPlaceableEntities, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		
		// Check found entities for ownership and type match
		foreach (IEntity entity : m_aFoundEntities)
		{
			OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
			if (!placeableComp)
				continue;
			
			// Check if placeable type matches what we're looking for
			if (placeableComp.GetPlaceableType() != m_sPlaceableName)
				continue;
			
			// Check if owner matches job owner (using persistent ID)
			string ownerPersistentId = placeableComp.GetOwnerPersistentId();
			if (ownerPersistentId.IsEmpty())
				continue;
			
			// Compare with job owner's persistent ID
			if (ownerPersistentId == jobOwnerPersistentId)
			{
				// Found matching placeable, job stage complete
				return false;
			}
		}
		
		// Continue searching
		return true;
	}
	
	protected bool FilterPlaceableEntities(IEntity entity)
	{
		if (!entity || !m_aFoundEntities)
			return false;
		
		// Check if entity has a placeable component
		OVT_PlaceableComponent placeableComp = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		if (!placeableComp)
			return false;
		
		// Add all entities with placeable components to results
		m_aFoundEntities.Insert(entity);
		return false; // Continue searching to find all entities
	}
}