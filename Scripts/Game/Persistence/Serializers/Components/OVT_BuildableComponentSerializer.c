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
	//! Writes the owner's persistent id and the camp / FOB / base association.
	//! \param[in] owner The built entity.
	//! \param[in] component The buildable component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow buildable.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(component);
		if (!buildable)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		const string ownerPersistentId = buildable.GetOwnerPersistentId();
		context.Write(ownerPersistentId);

		const string associatedBaseId = buildable.GetAssociatedBaseId();
		context.Write(associatedBaseId);

		int baseType = buildable.GetBaseType();
		context.Write(baseType);

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

		return true;
	}
}
