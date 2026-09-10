[BaseContainerProps(configRoot: true)]
class OVT_OptionsManagerComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The options singleton. This class is the engine half: the game mode seam, `OVT_Global.GetOptions()`,
//! and the change invoker. `OVT_OptionsRegistry` is the pure half and holds every rule.
//!
//! `RegisterDefaults()` runs from `OnPostInit` on every machine, so a client always has the two
//! built-in definitions even though only values ever travel the network (D3).
//------------------------------------------------------------------------------------------------
class OVT_OptionsManagerComponent : OVT_Component
{
	//! Id of the built-in autosave enabled toggle.
	static const string OPTION_AUTOSAVE_ENABLED = "autosave.enabled";

	//! Id of the built-in autosave interval slider, stored in seconds.
	static const string OPTION_AUTOSAVE_INTERVAL = "autosave.interval";

	//! Join-in-progress stream version. Raise it whenever the field order of RplSave changes.
	protected const int OPTIONS_STREAM_VERSION = 1;

	protected ref OVT_OptionsRegistry m_Registry;
	protected ref OVT_OptionsLocalStore m_LocalStore;

	protected static OVT_OptionsManagerComponent s_Instance;

	//! Fires on every machine when a value changes. Args: (string id, string value).
	ref ScriptInvoker m_OnOptionChanged = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! \return The manager singleton, or null before the game mode's OnPostInit has run.
	static OVT_OptionsManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		s_Instance = this;

		// RplLoad can build the registry first, and its values must survive this call.
		if (!m_Registry)
			m_Registry = new OVT_OptionsRegistry();

		RegisterDefaults();

		if (!m_LocalStore)
			m_LocalStore = new OVT_OptionsLocalStore();

		OVT_OptionsLocalSettingsAccessor.Load(m_LocalStore);

		PullLocalValuesFromStore();
	}

	//------------------------------------------------------------------------------------------------
	override event void OnDelete(IEntity owner)
	{
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the built-in options. Runs on every machine (D3), so a client always has a
	//! definition to draw a row from, even though a value travels and a definition never does.
	protected void RegisterDefaults()
	{
		RegisterOption(OVT_OptionDefinition.MakeToggle(
			OPTION_AUTOSAVE_ENABLED,
			OVT_EOptionScope.WORLD,
			true,
			"#OVT-Options_Autosave_Enabled",
			"#OVT-Options_Autosave_EnabledDesc"));

		OVT_OptionDefinition interval = OVT_OptionDefinition.MakeSlider(
			OPTION_AUTOSAVE_INTERVAL,
			OVT_EOptionScope.WORLD,
			600,
			120,
			3600,
			60,
			"#OVT-Options_Autosave_Interval",
			"#OVT-Options_Autosave_IntervalDesc");
		interval.m_fShownMultiplier = 1.0 / 60.0;
		interval.m_sFormatKey = "#OVT-Options_Minutes";

		RegisterOption(interval);
	}

	//------------------------------------------------------------------------------------------------
	//! Registers one definition with the registry. A subsystem calls this once for its own option.
	//!
	//! A LOCAL definition that arrives after the profile load (order is not guaranteed) pulls its
	//! value out of the local store the moment it registers, so late registration never loses a
	//! saved LOCAL value.
	//! \param[in] def The definition to register.
	//! \return True on acceptance, false when the definition is invalid or the id is already known.
	bool RegisterOption(OVT_OptionDefinition def)
	{
		bool accepted = m_Registry.Register(def);

		if (accepted && def.m_eScope == OVT_EOptionScope.LOCAL && m_LocalStore && m_LocalStore.Has(def.m_sId))
			m_Registry.SetValue(def.m_sId, m_LocalStore.Get(def.m_sId));

		return accepted;
	}

	//------------------------------------------------------------------------------------------------
	//! Pushes every stored LOCAL value whose id already has a definition into the registry mirror.
	//! An id with no definition yet stays in the store untouched; `RegisterOption` pulls it later.
	protected void PullLocalValuesFromStore()
	{
		array<string> ids = {};
		array<string> values = {};
		m_LocalStore.ToPairs(ids, values);

		for (int i = 0; i < ids.Count(); i++)
		{
			OVT_OptionDefinition def = m_Registry.GetDefinition(ids[i]);
			if (!def)
				continue;

			if (def.m_eScope != OVT_EOptionScope.LOCAL)
				continue;

			m_Registry.SetValue(ids[i], values[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: writes a LOCAL value in memory and tells the consumers. Nothing reaches the network
	//! (section 3.6): a LOCAL value must never travel.
	//! \param[in] id The option id. Refused when it has no definition or the scope is not LOCAL.
	//! \param[in] value The candidate value, in the option's string encoding.
	void SetLocalOption(string id, string value)
	{
		OVT_OptionDefinition def = m_Registry.GetDefinition(id);
		if (!def)
			return;

		if (def.m_eScope != OVT_EOptionScope.LOCAL)
			return;

		string stored = m_Registry.Normalize(id, value);

		m_Registry.SetValue(id, stored);

		if (!m_LocalStore)
			m_LocalStore = new OVT_OptionsLocalStore();

		m_LocalStore.Set(id, stored);

		m_OnOptionChanged.Invoke(id, stored);
	}

	//------------------------------------------------------------------------------------------------
	//! Flushes every LOCAL value to the profile. The Phase 7 menu calls this once, on close: the only
	//! flush point, so two writes can never collapse into the engine's save throttle (R6).
	void FlushLocalOptions()
	{
		if (!m_LocalStore)
			return;

		OVT_OptionsLocalSettingsAccessor.Save(m_LocalStore);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The pure LOCAL store backing this manager.
	OVT_OptionsLocalStore GetLocalStore()
	{
		return m_LocalStore;
	}

	//------------------------------------------------------------------------------------------------
	//! Overrides a definition's default value only. A stored value is never touched, so a saved
	//! override wins whether it lands before or after this call.
	//! \param[in] id The option id.
	//! \param[in] value The new default, in the option's string encoding.
	void OverrideDefault(string id, string value)
	{
		m_Registry.SetDefault(id, value);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The definition, or null when the id is unknown.
	OVT_OptionDefinition GetDefinition(string id)
	{
		return m_Registry.GetDefinition(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return True when a definition is registered for the id.
	bool HasOption(string id)
	{
		return m_Registry.HasDefinition(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Lists the ids registered for one scope, in registration order.
	//! \param[in] scope The scope to filter on.
	//! \param[out] ids Receives the ids. Allocated when null and cleared otherwise.
	void GetIdsForScope(OVT_EOptionScope scope, out array<string> ids)
	{
		m_Registry.GetIdsForScope(scope, ids);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The stored value, else the definition's default, else `""`.
	string GetValue(string id)
	{
		return m_Registry.GetValue(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The value read as a bool.
	bool GetBool(string id)
	{
		return m_Registry.GetBool(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The value read as a float.
	float GetFloat(string id)
	{
		return m_Registry.GetFloat(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The value read as an int.
	int GetInt(string id)
	{
		return m_Registry.GetInt(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] id The option id.
	//! \return The stored choice key.
	string GetChoice(string id)
	{
		return m_Registry.GetChoice(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The pure registry backing this manager, for a later phase's serializer and request seam.
	OVT_OptionsRegistry GetRegistry()
	{
		return m_Registry;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: normalize, store and broadcast one option value.
	//!
	//! The direct RpcDo_SetOption call is what applies the write on the authority, because the engine
	//! never loops a broadcast back to the machine that sent it.
	//! \param[in] id The option id.
	//! \param[in] value The candidate value, in the option's string encoding.
	void SetOption(string id, string value)
	{
		if (!Replication.IsServer()) return;

		if (!m_Registry.HasDefinition(id))
		{
			Print("[Overthrow] Options: refused a write to '" + id + "', which has no definition", LogLevel.WARNING);
			return;
		}

		string stored = m_Registry.Normalize(id, value);

		RpcDo_SetOption(id, stored);
		// Rpc arity audited: 2 payload args match RpcDo_SetOption(string id, string value)
		Rpc(RpcDo_SetOption, id, stored);
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine: store the server's value and tell the consumers.
	//! \param[in] id The option id.
	//! \param[in] value The value, already normalized by the server.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetOption(string id, string value)
	{
		ApplyValue(id, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Stores one value in this machine's mirror and fires the change invoker. A machine with no
	//! definition for the id keeps the value pending until the definition arrives (D5).
	//! \param[in] id The option id.
	//! \param[in] value The value to store.
	protected void ApplyValue(string id, string value)
	{
		m_Registry.SetValue(id, value);
		m_OnOptionChanged.Invoke(id, m_Registry.GetValue(id));
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one persisted record per stored override, for the save serializer.
	//!
	//! A pending id, one with no definition, is dropped rather than saved (D6): it has no type and no
	//! constraints, so writing it back would only keep an unreadable string. Every dropped id is named
	//! in one WARNING line, so a save never loses an option silently.
	//! \param[out] records Receives one record per override. Cleared first.
	void GetPersistableOverrides(notnull array<ref OVT_PersistedOption> records)
	{
		records.Clear();

		array<string> ids = {};
		array<string> values = {};
		m_Registry.GetOverrides(ids, values);

		for (int i = 0; i < ids.Count(); i++)
		{
			OVT_PersistedOption record = new OVT_PersistedOption();
			record.id = ids[i];
			record.value = values[i];
			records.Insert(record);
		}

		array<string> pending = {};
		m_Registry.GetPendingIds(pending);

		if (pending.IsEmpty())
			return;

		string names = "";
		foreach (string id : pending)
		{
			names = names + id + " ";
		}

		Print("[Overthrow] Options: dropped " + pending.Count().ToString() + " pending value(s) from the save, no definition claimed them: " + names, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Applies every persisted record to this machine's mirror, on the load path.
	//!
	//! Each record goes through `ApplyValue`, which fires the change invoker itself, so the whole
	//! re-application happens here and nothing waits for a player to connect (BUG-104 rule).
	//! \param[in] records The persisted overrides to apply.
	void ApplyPersisted(notnull array<ref OVT_PersistedOption> records)
	{
		foreach (OVT_PersistedOption record : records)
		{
			if (!record || record.id == "")
				continue;

			ApplyValue(record.id, record.value);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The definition set this machine knows: every registered id, scope by scope and in registration
	//! order inside each scope, each one followed by a semicolon. A mismatch means the two builds
	//! declare different options (R7).
	//! \return The signature.
	protected string BuildDefinitionSignature()
	{
		if (!m_Registry)
			return "";

		array<string> all = {};
		array<string> scoped = {};

		m_Registry.GetIdsForScope(OVT_EOptionScope.LOCAL, scoped);
		all.InsertAll(scoped);

		m_Registry.GetIdsForScope(OVT_EOptionScope.FACTION, scoped);
		all.InsertAll(scoped);

		m_Registry.GetIdsForScope(OVT_EOptionScope.WORLD, scoped);
		all.InsertAll(scoped);

		string signature = "";
		foreach (string id : all)
		{
			signature = signature + id + ";";
		}

		return signature;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the join-in-progress snapshot: the stream version, the definition signature, the pair
	//! count, then the EFFECTIVE value of every FACTION and WORLD option.
	//!
	//! The effective value, and not only an override, because a default that a server operator changed
	//! is invisible to the client that compiled its own default.
	//!
	//! READ ORDER EQUALS WRITE ORDER. A new field may only go last, and it must raise the version.
	//! \param[in] writer The JIP stream.
	//! \return True. The payload is always writable.
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(OPTIONS_STREAM_VERSION);
		writer.WriteString(BuildDefinitionSignature());

		array<string> sendIds = {};

		if (m_Registry)
		{
			array<string> scoped = {};

			m_Registry.GetIdsForScope(OVT_EOptionScope.FACTION, scoped);
			sendIds.InsertAll(scoped);

			m_Registry.GetIdsForScope(OVT_EOptionScope.WORLD, scoped);
			sendIds.InsertAll(scoped);
		}

		writer.WriteInt(sendIds.Count());

		foreach (string id : sendIds)
		{
			writer.WriteString(id);
			writer.WriteString(m_Registry.GetValue(id));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the join-in-progress snapshot to this client's mirror.
	//!
	//! A version mismatch rejects the whole stream. A signature mismatch only warns, because a partial
	//! mirror beats an empty one: an id this build does not know stays pending (R7).
	//! \param[in] reader The JIP stream.
	//! \return False on the first unreadable field, or on a version mismatch.
	override bool RplLoad(ScriptBitReader reader)
	{
		int version;
		if (!reader.ReadInt(version)) return false;

		if (version != OPTIONS_STREAM_VERSION)
		{
			Print("[Overthrow] Options JIP stream version mismatch (got " + version.ToString() + ", expected " + OPTIONS_STREAM_VERSION.ToString() + ") - the client and the server run different Overthrow versions", LogLevel.ERROR);
			return false;
		}

		string signature;
		if (!reader.ReadString(signature)) return false;

		int count;
		if (!reader.ReadInt(count)) return false;

		if (!m_Registry)
			m_Registry = new OVT_OptionsRegistry();

		if (signature != BuildDefinitionSignature())
			Print("[Overthrow] Options JIP definition signature mismatch - this machine declares a different option set. Every value it knows still applies.", LogLevel.WARNING);

		for (int i = 0; i < count; i++)
		{
			string id, value;
			if (!reader.ReadString(id)) return false;
			if (!reader.ReadString(value)) return false;

			ApplyValue(id, value);
		}

		return true;
	}
}
