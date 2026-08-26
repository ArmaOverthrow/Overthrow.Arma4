//------------------------------------------------------------------------------------------------
//! Persists a built structure's ownership and which camp / FOB / base it belongs to.
//!
//! BINDING. Listed in the ComponentSerializers block of the BUILDABLE entity configuration in
//! Configs/Systems/Persistence/Overthrow.conf, which matches on
//! `ComponentClassPersistenceConfigRule { ComponentClass "OVT_BuildableComponent" }`.
//!
//! WHY BUILDABLES ARE SPAWNED ENTITIES, NOT WORLD-PLACED ONES. OVT_ResistanceFactionManager.
//! BuildItem() spawns a prefab from the buildables config at the player's chosen transform - there
//! is no authored instance in the world file to modify. A save therefore has to be able to CREATE
//! the entity again, which is why its configuration carries SelfSpawn and why the build site calls
//! OVT_PersistenceTracking.Track(). Component state alone would restore nothing.
//!
//! IDENTICAL SHAPE TO OVT_PlaceableComponentSerializer, deliberately: the two components are
//! field-for-field twins in the shipped code (down to the doc comments) and EPF gave them twin save
//! data. Keeping the serializers symmetrical means a change to one is obviously needed in the other.
//! See that file's header for the association and enum-ordinal reasoning, which applies here too.
//!
//! IDEMPOTENT ON A LIVE SESSION. Both applies are plain assignments of the same values.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//!
//! VERSION 2 ADDS THE DESTRUCTION PHASE (core/damage, 2026-08-20): 0 intact, 1 ruined, and 0 for a
//! structure that carries no destruction component at all. It is appended, so a version 1 payload -
//! every save taken before that feature - reads its three fields and stops. There is no new record
//! and no new binding: a ruined structure is the SAME entity with a different mesh, which is the
//! whole reason this feature is one int rather than a persistence problem.
//!
//! THE NAVMESH QUEUE STAYS THE FIRST LINE OF Deserialize(), BEFORE THE PHASE IS RESTORED, and that
//! order is deliberate. It captures the bounds of the INTACT structure; the destruction component's
//! own regeneration then captures them again after the ruin model is in place. Measuring the larger
//! model and rebuilding once the smaller one appears is what the live destruction path does too.
//------------------------------------------------------------------------------------------------
class OVT_BuildableComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_BuildableComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the owner's persistent id, the camp / FOB / base association and the destruction phase.
	//! \param[in] owner The built entity.
	//! \param[in] component The buildable component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow buildable.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(component);
		if (!buildable)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 2);

		const string ownerPersistentId = buildable.GetOwnerPersistentId();
		context.Write(ownerPersistentId);

		const string associatedBaseId = buildable.GetAssociatedBaseId();
		context.Write(associatedBaseId);

		int baseType = buildable.GetBaseType();
		context.Write(baseType);

		// Read live off the destruction component (decision D9 - the phase has one home). A structure
		// whose prefab was never retrofitted writes 0, because the field is positional and always
		// present in a version 2 payload.
		int damagePhase;
		OVT_StructureDestructionComponent destruction = OVT_StructureDamage.Resolve(owner);
		if (destruction)
			damagePhase = destruction.GetDamagePhase();

		context.Write(damagePhase);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores ownership and association onto a respawned buildable.
	//! \param[in] owner The built entity.
	//! \param[in] component The buildable component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(component);
		if (!buildable)
			return false;

		// THE NAVMESH DOES NOT COME BACK WITH THE OBJECT - see the matching call and the full
		// explanation in OVT_PlaceableComponentSerializer.Deserialize(). Buildables are the worse
		// half of the problem: they are the guard towers, tents and garages an AI cannot squeeze
		// past, where a placeable is often a sign or a poster.
		OVT_NavmeshRebuild.Queue(owner);

		// No version means no payload for this component - see OVT_TownManagerSerializer.Deserialize().
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		string ownerPersistentId;
		context.Read(ownerPersistentId);

		string associatedBaseId;
		context.Read(associatedBaseId);

		int baseType;
		context.Read(baseType);

		buildable.SetOwnerPersistentId(ownerPersistentId);
		buildable.SetAssociatedBase(associatedBaseId, baseType);

		if (version < 2)
			return true;

		int damagePhase;
		context.Read(damagePhase);

		// Silent by contract: a load must not explode, smoke or sound. RestorePhase() is a no-op when
		// the structure is already in that phase, which is the ordinary case - an intact one.
		OVT_StructureDestructionComponent destruction = OVT_StructureDamage.Resolve(owner);
		if (destruction)
			destruction.RestorePhase(damagePhase);

		return true;
	}
}
