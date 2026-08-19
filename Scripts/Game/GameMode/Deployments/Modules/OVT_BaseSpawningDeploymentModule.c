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

	//------------------------------------------------------------------------------------------------
	//! Turn a heading into the rotation SpawnEntity() below expects, forced upright.
	//!
	//! THE ONE PLACE a deployment module converts an orientation into a spawn rotation, because the two
	//! engine angle APIs use DIFFERENT ORDERS and mixing them up is silent:
	//!
	//!   IEntity.GetAngles() / IEntity.SetAngles()   -> (X = pitch, Y = yaw, Z = roll)
	//!   Math3D.AnglesToMatrix() / MatrixToAngles()  -> (yaw, pitch, roll)
	//!
	//! Feed a GetAngles() vector straight into AnglesToMatrix() and the yaw lands in the PITCH slot: a
	//! marker at yaw -172 degrees spawns its vehicle on its nose. Every caller that has a heading and
	//! wants a spawn rotation comes through here instead of writing the vector out by hand.
	//!
	//! Pitch and roll are always zero. Nothing a deployment spawns wants to arrive tilted; the terrain
	//! and the physics settle it.
	//! \param[in] yaw Heading in degrees, rotation about the world Y axis.
	//! \return Rotation in Math3D.AnglesToMatrix order, level.
	static vector GetUprightSpawnRotation(float yaw)
	{
		return Vector(yaw, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn a prefab already carrying its final transform, and track it.
	//!
	//! The rotation goes into the SPAWN TRANSFORM and is never applied afterwards. Rotating an entity
	//! that already exists is a different and much more dangerous operation - on anything with a
	//! physics body it desynchronises the rigid body from the entity node, which is how vehicles end up
	//! flipped and jittering. Vanilla needs BaseGameEntity.Teleport() plus zeroed velocities for that
	//! (SCR_EditableEntityComponent.SetTransformOwner); spawning with the transform needs none of it.
	//! \param[in] prefab Prefab to spawn.
	//! \param[in] position World position.
	//! \param[in] rotation Rotation in Math3D.AnglesToMatrix order - (yaw, pitch, roll), NOT the
	//!            (pitch, yaw, roll) order IEntity.GetAngles() returns. Use GetUprightSpawnRotation().
	//! \return The spawned entity, or null.
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