//------------------------------------------------------------------------------------------------
//! Persists a placed object's ownership and which camp / FOB / base it belongs to.
//!
//! BINDING. Listed in the ComponentSerializers block of the PLACEABLE entity configuration in
//! Configs/Systems/Persistence/Overthrow.conf, which matches on
//! `ComponentClassPersistenceConfigRule { ComponentClass "OVT_PlaceableComponent" }`.
//!
//! THIS IS COMPONENT STATE ONLY. The entity itself comes back because that configuration has
//! SelfSpawn set and the entity is tracked at its spawn site
//! (OVT_ResistanceFactionManager.PlaceItem -> OVT_PersistenceTracking.Track). Prefab and transform
//! are the GenericEntitySerializer's job; all this adds is the three fields Overthrow hangs off a
//! placeable, which is exactly what EPF's OVT_PlaceableData carried.
//!
//! WHY THE ASSOCIATION MATTERS AND IS NOT DERIVED. m_sAssociatedBaseId is stamped at placement time
//! from whichever camp / FOB / base was nearest THEN, and it is what
//! OVT_ResistanceFactionManager.CleanupCampObjects() and OVT_ItemLimitChecker read later. Deriving
//! it again on load would re-point objects at whatever happens to be nearest now, quietly moving
//! things between camps, so it is stored.
//!
//! THE BASE TYPE TRAVELS AS AN INT. EOVTBaseType is an enum whose ORDINALS are the stored values;
//! writing the int keeps the payload a primitive the binary context is known to round-trip and
//! mirrors EPF's own field type. Reordering EOVTBaseType would invalidate saves - it has not changed
//! since it was introduced and must not be reordered.
//!
//! IDEMPOTENT ON A LIVE SESSION. Both applies are plain assignments of the same values.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_PlaceableComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_PlaceableComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the owner's persistent id and the camp / FOB / base association.
	//! \param[in] owner The placed entity.
	//! \param[in] component The placeable component being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow placeable.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_PlaceableComponent placeable = OVT_PlaceableComponent.Cast(component);
		if (!placeable)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		const string ownerPersistentId = placeable.GetOwnerPersistentId();
		context.Write(ownerPersistentId);

		const string associatedBaseId = placeable.GetAssociatedBaseId();
		context.Write(associatedBaseId);

		int baseType = placeable.GetBaseType();
		context.Write(baseType);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores ownership and association onto a respawned placeable.
	//! \param[in] owner The placed entity.
	//! \param[in] component The placeable component being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_PlaceableComponent placeable = OVT_PlaceableComponent.Cast(component);
		if (!placeable)
			return false;

		// THE NAVMESH DOES NOT COME BACK WITH THE OBJECT. This entity was respawned by the
		// persistence system (SelfSpawn, see the BINDING note above), but the navmesh loaded with the
		// world is the BAKED one - carved before this object ever existed. Until this call the AI
		// paths straight through whatever the player built in an earlier session and walks into it.
		// PlaceItem() rebuilds at the moment of placement, which is the only reason the problem is
		// invisible in the session you build in.
		//
		// Queued, not immediate: a save restores many of these at once and the queue merges their
		// areas. Before the version guard on purpose - the entity is in the world and blocking
		// pathfinding whether or not it brought a component payload with it.
		OVT_NavmeshRebuild.Queue(owner);

		// No version means no payload for this component - see OVT_TownManagerSerializer.Deserialize().
		// Without the guard an absent payload would blank a live placeable's owner.
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

		placeable.SetOwnerPersistentId(ownerPersistentId);
		placeable.SetAssociatedBase(associatedBaseId, baseType);

		return true;
	}
}
