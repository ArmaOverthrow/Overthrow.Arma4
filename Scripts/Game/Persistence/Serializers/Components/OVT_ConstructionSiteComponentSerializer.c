//------------------------------------------------------------------------------------------------
//! Persists what a construction site was ordered to become.
//!
//! BINDING - THE OVERTHROW BUILDABLE CONFIGURATION {6B0E7A27C0D539F2} AND NOTHING ELSE (D16). The
//! site carries OVT_BuildableComponent, so it already matches that rule at Priority 35000 and gets
//! exactly one EntityPersistenceConfig; a rule of its own would be a second claim on the same entity.
//!
//! WHAT IS AT STAKE. Without this a reloaded site comes back as a concrete mixer that remembers
//! nothing: its build action can name no building and finishing it is impossible, while the money
//! the player paid at placement (D2) is gone. The crate piles would stay on the ground forever.
//!
//! THE NAME IS NOT PERSISTED. It is re-derived from buildables.conf by ApplyPersisted(), so a
//! renamed buildable renames every standing site instead of freezing the old label into every save.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first, then
//! buildableIndex, prefabIndex, angles.
//------------------------------------------------------------------------------------------------
class OVT_ConstructionSiteComponentSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ConstructionSiteComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the ordered buildable and the orientation it will stand at.
	//! \param[in] owner The site entity.
	//! \param[in] component The construction site being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not a construction site.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ConstructionSiteComponent site = OVT_ConstructionSiteComponent.Cast(component);
		if (!site)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The LOCAL NAME IS THE PROPERTY NAME - Write() derives the key from the variable it is handed
		// - so all three have to be spelled identically in Deserialize below.
		int buildableIndex = site.GetBuildableIndex();
		context.Write(buildableIndex);

		int prefabIndex = site.GetPrefabIndex();
		context.Write(prefabIndex);

		vector angles = site.GetAngles();
		context.Write(angles);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Restores what the site was ordered to become, or leaves it exactly as it is.
	//! \param[in] owner The site entity.
	//! \param[in] component The construction site being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ConstructionSiteComponent site = OVT_ConstructionSiteComponent.Cast(component);
		if (!site)
			return false;

		// No version means no payload - every save taken before construction sites existed. There is
		// no such thing as a saved site in one of those, so there is nothing to restore.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		// EVERY READ IS CHECKED AND ALL THREE COMPLETE BEFORE ANYTHING IS APPLIED. A failed Read()
		// leaves its destination at zero, and zero is a legal buildable index - applying it would turn
		// a Garage site into whatever buildables.conf lists first.
		int buildableIndex;
		if (!context.Read(buildableIndex))
			return AbortUnreadablePayload(owner);

		int prefabIndex;
		if (!context.Read(prefabIndex))
			return AbortUnreadablePayload(owner);

		vector angles;
		if (!context.Read(angles))
			return AbortUnreadablePayload(owner);

		site.ApplyPersisted(buildableIndex, prefabIndex, angles);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live site.
	//! \param[in] owner The site whose record could not be read.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload(notnull IEntity owner)
	{
		Print(string.Format("[Overthrow] Could not read the construction site record of '%1' - the site keeps whatever it already remembers rather than being pointed at buildable index 0, which is what a zeroed read would do. This is a save-format fault.", OVT_PrefabUtils.GetPrefabName(owner)), LogLevel.ERROR);
		return true;
	}
}
