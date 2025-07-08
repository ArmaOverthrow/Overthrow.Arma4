[BaseContainerProps(configRoot: true)]
class OVT_BaseSpawningDeploymentModule : OVT_BaseDeploymentModule
{
	protected ref array<ref EntityID> m_aSpawnedEntities;
	protected bool m_bSpawnedUnitsEliminated; // Flag to track if all spawned units have been killed
	
	//------------------------------------------------------------------------------------------------
	override void Initialize(OVT_DeploymentComponent parent)
	{
		super.Initialize(parent);
		m_aSpawnedEntities = new array<ref EntityID>;
	}
	
	array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;
		return entities;
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
		}
		
		return entity;
	}
	
	//------------------------------------------------------------------------------------------------
	// Elimination tracking
	//------------------------------------------------------------------------------------------------
	bool AreSpawnedUnitsEliminated()
	{
		return m_bSpawnedUnitsEliminated;
	}
	
	void SetSpawnedUnitsEliminated(bool eliminated)
	{
		m_bSpawnedUnitsEliminated = eliminated;
	}
	
	// Virtual method for subclasses to implement their own elimination checking
	protected bool CheckIfUnitsEliminated()
	{
		// Default implementation - subclasses should override this
		return false;
	}
	
}