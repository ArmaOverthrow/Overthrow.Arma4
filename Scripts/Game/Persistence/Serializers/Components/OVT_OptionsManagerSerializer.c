//------------------------------------------------------------------------------------------------
//! One persisted option override. A dedicated class, so the wire shape of the live registry stays
//! free to change without touching a save file.
//!
//! FIELD ORDER IS THE FORMAT: binary contexts are positional, so a new field may only be APPENDED.
//------------------------------------------------------------------------------------------------
class OVT_PersistedOption
{
	string id;
	string value;
}

//------------------------------------------------------------------------------------------------
//! Persists the option overrides held by OVT_OptionsManagerComponent.
//!
//! Only a value that differs from its own default is written. ApplyPersisted fires the change
//! invoker for every record, so the whole re-application runs on the load path with nothing left
//! waiting for a player to connect.
//!
//! Binary contexts are positional. Write order must equal read order, version first.
//------------------------------------------------------------------------------------------------
class OVT_OptionsManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_OptionsManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes one record per stored override.
	//! \param[in] owner The game mode entity owning the options manager.
	//! \param[in] component The manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow options manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_OptionsManagerComponent options = OVT_OptionsManagerComponent.Cast(component);
		if (!options)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		// The LOCAL NAME IS THE PROPERTY NAME - Write() derives the key from the variable it is
		// handed - so `records` must be spelled identically in Deserialize below.
		array<ref OVT_PersistedOption> records = new array<ref OVT_PersistedOption>();
		options.GetPersistableOverrides(records);

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the option records back and hands them to the manager to apply.
	//! \param[in] owner The game mode entity owning the options manager.
	//! \param[in] component The manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_OptionsManagerComponent options = OVT_OptionsManagerComponent.Cast(component);
		if (!options)
			return false;

		// No version means no payload - every save taken before this feature existed.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<ref OVT_PersistedOption> records = new array<ref OVT_PersistedOption>();
		if (!context.Read(records))
			return AbortUnreadablePayload();

		options.ApplyPersisted(records);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reports an unreadable payload and consumes it without touching the live registry.
	//! \return True - the payload is consumed either way; nothing was applied.
	protected bool AbortUnreadablePayload()
	{
		Print("[Overthrow] Could not read the options table - the live registry is left exactly as it is", LogLevel.ERROR);
		return true;
	}
}
