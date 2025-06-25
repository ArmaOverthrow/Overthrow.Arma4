[BaseContainerProps(configRoot: true)]
class OVT_BaseSpawningDeploymentModule : OVT_BaseDeploymentModule
{
	protected ref array<ref EntityID> m_aSpawnedEntities;
	
	//------------------------------------------------------------------------------------------------
	override void Initialize(OVT_DeploymentComponent parent)
	{
		super.Initialize(parent);
		m_aSpawnedEntities = new array<ref EntityID>;
	}
	
	//------------------------------------------------------------------------------------------------
	// Common spawning functionality
	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnEntity(ResourceName prefab, vector position, vector rotation = vector.Zero)
	{
		if (!prefab || prefab.IsEmpty())
			return null;
			
		// Create transform matrix
		vector mat[4];
		Math3D.AnglesToMatrix(rotation, mat);
		mat[3] = position;
		
		// Spawn through unified API (TODO: create unified spawning API)
		IEntity entity = OVT_Global.SpawnEntityPrefabMatrix(prefab, mat);
		if (entity)
		{
			m_aSpawnedEntities.Insert(entity.GetID());
			OnEntitySpawned(entity);
		}
		
		return entity;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DespawnEntity(EntityID entityId)
	{
		if (!entityId)
			return;
			
		IEntity entity = GetGame().GetWorld().FindEntityByID(entityId);
		if (entity)
		{
			OnEntityDespawned(entity);
			delete entity;
		}
		
		int index = m_aSpawnedEntities.Find(entityId);
		if (index != -1)
			m_aSpawnedEntities.Remove(index);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DespawnAllEntities()
	{
		// Create a copy to avoid modification during iteration
		array<ref EntityID> entitiesToDespawn = new array<ref EntityID>;
		foreach (EntityID id : m_aSpawnedEntities)
			entitiesToDespawn.Insert(id);
			
		foreach (EntityID id : entitiesToDespawn)
			DespawnEntity(id);
			
		m_aSpawnedEntities.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool AreAllEntitiesAlive()
	{
		foreach (EntityID id : m_aSpawnedEntities)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (!entity)
				return false;
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetSpawnedEntityCount()
	{
		return m_aSpawnedEntities.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;
		foreach (EntityID id : m_aSpawnedEntities)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (entity)
				entities.Insert(entity);
		}
		return entities;
	}
	
	//------------------------------------------------------------------------------------------------
	// Virtual callbacks for spawning events
	//------------------------------------------------------------------------------------------------
	protected void OnEntitySpawned(IEntity entity) {}
	protected void OnEntityDespawned(IEntity entity) {}
	
	//------------------------------------------------------------------------------------------------
	override void OnCleanup()
	{
		DespawnAllEntities();
		super.OnCleanup();
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods for finding spawn positions
	//------------------------------------------------------------------------------------------------
	protected vector FindSpawnPosition(vector center, float radius, bool requireGroundContact = true)
	{
		for (int attempts = 0; attempts < 10; attempts++)
		{
			float angle = Math.RandomFloat01() * Math.PI2;
			float distance = Math.RandomFloat(0, radius);
			
			vector offset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			vector position = center + offset;
			
			if (requireGroundContact)
			{
				// Trace to ground
				vector start = position + Vector(0, 50, 0);
				vector end = position + Vector(0, -50, 0);
				
				TraceParam param = new TraceParam();
				param.Start = start;
				param.End = end;
				param.Flags = TraceFlags.WORLD;
				
				float result = GetGame().GetWorld().TraceMove(param, null);
				if (result < 1.0)
				{
					position = vector.Lerp(start, end, result);
					return position;
				}
			}
			else
			{
				return position;
			}
		}
		
		// Fall back to original position if no suitable spot found
		return center;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector FindNearestRoad(vector center, float searchRadius = 500)
	{
		// Simple implementation - could be improved with road network queries
		vector bestPosition = center;
		float bestDistance = float.MAX;
		
		for (int i = 0; i < 8; i++)
		{
			float angle = (i / 8.0) * Math.PI2;
			vector testPos = center + Vector(Math.Cos(angle) * searchRadius, 0, Math.Sin(angle) * searchRadius);
			
			// Trace to find ground
			TraceParam param = new TraceParam();
			param.Start = testPos + Vector(0, 50, 0);
			param.End = testPos + Vector(0, -50, 0);
			param.Flags = TraceFlags.WORLD;
			
			float result = GetGame().GetWorld().TraceMove(param, null);
			if (result < 1.0)
			{
				vector groundPos = vector.Lerp(param.Start, param.End, result);
				float distance = vector.Distance(center, groundPos);
				
				if (distance < bestDistance)
				{
					bestDistance = distance;
					bestPosition = groundPos;
				}
			}
		}
		
		return bestPosition;
	}
}