//! Template for creating entity serializers for Overthrow persistence
//! This is a REFERENCE FILE - copy and modify for each entity type that needs persistence
//!
//! USAGE:
//! 1. Copy this file and rename to match your entity (e.g., OVT_MyEntitySerializer.c)
//! 2. Update class name and GetTargetType() to match your entity/component
//! 3. Implement SerializeSpawnData() to save prefab and transform
//! 4. Implement DeserializeSpawnData() to restore prefab and transform
//! 5. Implement Serialize()/Deserialize() for entity state beyond spawn data
//! 6. Add StartTracking() when entity is created/placed

//! Example Entity Serializer
//! Serializes entities that need to be respawned (placeables, bases, etc.)
class _OVT_EntitySerializerTemplate : ScriptedEntitySerializer
{
	//! Specify which entity/component type this serializer handles
	//! Can target entity class or a component on the entity
	//! @return The typename of the entity/component this serializer works with
	override static typename GetTargetType()
	{
		// Option 1: Target entity type
		// return OVT_MyEntity;

		// Option 2: Target component on entity (more common)
		return OVT_MyEntityComponent;
	}

	//! Serialize spawn data (prefab and transform)
	//! Called BEFORE Serialize() - handles how to recreate the entity
	//! @param entity The entity to serialize spawn data for
	//! @param context Serialization context for writing data
	//! @param defaultData Output parameter for default spawn data (usually not used)
	//! @return Serialization result
	override protected ESerializeResult SerializeSpawnData(
		IEntity entity,
		BaseSerializationSaveContext context,
		out EntitySpawnParams defaultData)
	{
		// Get prefab name
		ResourceName prefab = entity.GetPrefabData().GetPrefabName();
		if (prefab.IsEmpty())
		{
			Print("OVT_MyEntitySerializer: Entity has no prefab!", LogLevel.ERROR);
			return ESerializeResult.DEFAULT;
		}

		// Get transform (position, rotation, scale)
		EntitySpawnParams params = new EntitySpawnParams();
		entity.GetTransform(params.Transform);

		// Serialize prefab and transform
		context.Write(prefab);
		context.Write(params);

		return ESerializeResult.OK;
	}

	//! Deserialize spawn data (prefab and transform)
	//! Called during entity respawn - provides info needed to recreate entity
	//! @param prefab Output: The prefab to spawn
	//! @param params Output: Spawn parameters (transform, etc.)
	//! @param context Serialization context for reading data
	//! @return True if successful
	override protected bool DeserializeSpawnData(
		out ResourceName prefab,
		out EntitySpawnParams params,
		BaseSerializationLoadContext context)
	{
		// Read prefab and spawn params
		if (!context.Read(prefab))
			return false;

		params = new EntitySpawnParams();
		if (!context.Read(params))
			return false;

		return true;
	}

	//! Serialize entity state (beyond spawn data)
	//! Called AFTER SerializeSpawnData() - handles entity-specific state
	//! @param entity The entity to serialize
	//! @param context Serialization context for writing data
	//! @return Serialization result
	override protected ESerializeResult Serialize(
		IEntity entity,
		BaseSerializationSaveContext context)
	{
		// Get component if using component-based targeting
		OVT_MyEntityComponent comp = OVT_Component.Find<OVT_MyEntityComponent>(entity);
		if (!comp)
			return ESerializeResult.DEFAULT;

		// ALWAYS write version first
		context.WriteValue("version", 1);

		// Example: Serialize entity state
		// context.WriteValue("m_iHealth", comp.GetHealth());
		// context.WriteValue("m_sOwnerPlayerId", comp.GetOwnerPlayerId());
		// context.Write(comp.GetCustomData());

		// Example: Serialize entity reference using UUID
		// IEntity referencedEntity = comp.GetReferencedEntity();
		// if (referencedEntity)
		// {
		//     UUID entityId = GetSystem().GetId(referencedEntity);
		//     context.Write(entityId);
		// }
		// else
		// {
		//     UUID nullId;
		//     context.Write(nullId);
		// }

		return ESerializeResult.OK;
	}

	//! Deserialize entity state (beyond spawn data)
	//! Called AFTER entity is spawned - restores entity-specific state
	//! @param entity The entity to deserialize into
	//! @param context Serialization context for reading data
	//! @return True if successful
	override protected bool Deserialize(
		IEntity entity,
		BaseSerializationLoadContext context)
	{
		// Get component if using component-based targeting
		OVT_MyEntityComponent comp = OVT_Component.Find<OVT_MyEntityComponent>(entity);
		if (!comp)
			return false;

		// Read version
		int version;
		if (!context.ReadValue("version", version))
			return false;

		if (version == 1)
		{
			// Example: Deserialize entity state
			// int health;
			// context.ReadValue("m_iHealth", health);
			// comp.SetHealth(health);

			// string ownerId;
			// context.ReadValue("m_sOwnerPlayerId", ownerId);
			// comp.SetOwnerPlayerId(ownerId);

			// Example: Deserialize entity reference using UUID + WhenAvailable
			// UUID entityId;
			// context.Read(entityId);
			//
			// if (!entityId.IsNull())
			// {
			//     // Create context tuple for callback
			//     Tuple2<OVT_MyEntityComponent, IEntity> ctx(comp, entity);
			//
			//     // Request async entity resolution with 60s timeout
			//     PersistenceWhenAvailableTask task(OnReferencedEntityAvailable, ctx);
			//     GetSystem().WhenAvailable(entityId, task, 60.0);
			// }
		}
		else
		{
			Print(string.Format("OVT_MyEntitySerializer: Unknown version %1", version), LogLevel.ERROR);
			return false;
		}

		return true;
	}

	//! Callback for async entity reference resolution
	//! Called when referenced entity becomes available (or times out)
	//! @param instance The entity that was found (or null if timeout)
	//! @param task The deferred task
	//! @param expired True if request timed out
	//! @param context User context passed to WhenAvailable
	protected static void OnReferencedEntityAvailable(
		Managed instance,
		PersistenceDeferredDeserializeTask task,
		bool expired,
		Managed context)
	{
		// Cast the found entity
		IEntity referencedEntity = IEntity.Cast(instance);

		// Extract context
		auto ctx = Tuple2<OVT_MyEntityComponent, IEntity>.Cast(context);
		if (!ctx || !ctx.param1)
			return;

		OVT_MyEntityComponent comp = ctx.param1;

		// Handle timeout
		if (expired || !referencedEntity)
		{
			Print("OVT_MyEntitySerializer: Referenced entity not found or timed out", LogLevel.WARNING);
			comp.SetReferencedEntity(null);
			return;
		}

		// Set the resolved entity
		comp.SetReferencedEntity(referencedEntity);
		Print("OVT_MyEntitySerializer: Referenced entity resolved successfully", LogLevel.NORMAL);
	}
}

//! ENTITY TRACKING
//! Add this when entity is created/placed:
//!
//! // After entity is spawned
//! if (Replication.IsServer())
//!     PersistenceSystem.GetInstance().StartTracking(entity);

//! COLLECTION REGISTRATION
//! Register your serializer with the appropriate collection in OVT_PersistenceManagerComponent:
//!
//! PersistenceCollection.GetOrCreate("YourEntityCollection").SetSaveTypes(
//!     ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);

//! ENTITY REFERENCE PATTERN (CRITICAL!)
//!
//! ❌ NEVER use EntityID across network - it's not persistent!
//! ❌ NEVER assume entity references are immediately available after load!
//!
//! ✅ ALWAYS use UUID for entity references
//! ✅ ALWAYS use WhenAvailable() pattern for async resolution
//!
//! Example pattern:
//!
//! // SERIALIZE
//! IEntity target = comp.GetTarget();
//! UUID targetId;
//! if (target)
//!     targetId = GetSystem().GetId(target);
//! context.Write(targetId);
//!
//! // DESERIALIZE
//! UUID targetId;
//! context.Read(targetId);
//! if (!targetId.IsNull())
//! {
//!     Tuple1<OVT_MyComponent> ctx(comp);
//!     PersistenceWhenAvailableTask task(OnTargetAvailable, ctx);
//!     GetSystem().WhenAvailable(targetId, task, 60.0);  // 60s timeout
//! }
//!
//! // CALLBACK
//! protected static void OnTargetAvailable(
//!     Managed instance,
//!     PersistenceDeferredDeserializeTask task,
//!     bool expired,
//!     Managed context)
//! {
//!     IEntity targetEntity = IEntity.Cast(instance);
//!     auto ctx = Tuple1<OVT_MyComponent>.Cast(context);
//!
//!     if (!expired && targetEntity && ctx && ctx.param1)
//!         ctx.param1.SetTarget(targetEntity);
//! }

//! SPAWN DATA vs STATE
//!
//! - SerializeSpawnData/DeserializeSpawnData: How to CREATE the entity
//!   - Prefab name (what to spawn)
//!   - Transform (where to spawn)
//!   - Called FIRST
//!
//! - Serialize/Deserialize: Entity-specific STATE after spawning
//!   - Health, owner, upgrades, etc.
//!   - Entity references (using UUID)
//!   - Called SECOND (after entity exists)

//! TESTING
//! Test your entity serializer:
//! 1. Spawn/place entity in-game
//! 2. Modify entity state
//! 3. Trigger manual save
//! 4. Restart server
//! 5. Verify entity respawns at correct location
//! 6. Verify entity state restored correctly
//! 7. If using entity references, verify async resolution works
//! 8. Test timeout handling (delete referenced entity before load)
