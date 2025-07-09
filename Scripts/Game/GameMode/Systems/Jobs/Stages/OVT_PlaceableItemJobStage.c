class OVT_PlaceableItemJobStage : OVT_JobStage
{
	[Attribute("", UIWidgets.EditBox, "Name of placeable item to check for (from placeables.conf)")]
	string m_sPlaceableName;
	
	[Attribute("25", UIWidgets.EditBox, "Search radius in meters")]
	float m_fSearchRadius;
	
	protected ref array<IEntity> m_aFoundEntities;
		
	override bool OnTick(OVT_Job job)
	{
		if (m_sPlaceableName.IsEmpty())
			return false;
		
		if (job.owner.IsEmpty())
			return true;
		
		// Get player data and entity
		OVT_PlayerData playerData = OVT_Global.GetPlayers().GetPlayer(job.owner);
		if (!playerData)
			return true;
		
		int playerId = OVT_Global.GetPlayers().GetPlayerIDFromPersistentID(job.owner);
		
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!player)
			return true;
		
		string jobOwnerPersistentId = job.owner;
		
		// Clear previous results
		m_aFoundEntities = new array<IEntity>();
		
		// Query entities in sphere around player position
		GetGame().GetWorld().QueryEntitiesBySphere(player.GetOrigin(), m_fSearchRadius, null, FilterPlaceableEntities, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
		
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