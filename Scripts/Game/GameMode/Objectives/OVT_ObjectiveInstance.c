//------------------------------------------------------------------------------------------------
//! ONE OBJECTIVE IN FLIGHT: which plan is running, where, which phase it is in, and every scrap of
//! state the phase's modules have accumulated.
//!
//! ⚠ A Managed OBJECT INSIDE THE DIRECTOR, NOT A COMPONENT AND NOT AN ENTITY (D5). The only two
//! arguments for a component are replication and independent persistence and this feature has
//! neither: nothing about an objective replicates (the Game Master panel is a server-built snapshot,
//! not a replicated record) and the director already owns a game-mode component serializer that can
//! write N instances as a count plus N records. A component would need a prefab, an RplComponent,
//! persistence tracking, a load-order story against the deployment manager's pool refill and a
//! teardown story for a second campaign in one session - all for nothing at one objective. The
//! precedent next door made the OPPOSITE call for the right reason: a deployment needs a durable,
//! independently-persisted world marker, and an objective has no world presence at all.
//!
//! ⚠ EVERY BACK-REFERENCE IS WEAK AND EVERY OWNED MEMBER IS A ref. The modules hold the instance
//! weakly and the instance holds the director and the plan weakly; the registry owns the plan and the
//! director owns the instance. A strong reference in either direction would cycle and nothing would
//! ever be collected.
//!
//! =====================================================================================
//! ⚠ THE RECORD IS SHARED WITH THE DIRECTOR, NOT COPIED FROM IT. STRANGLER-ONLY, PHASES 2-6.
//! =====================================================================================
//! m_Record is the very object the director's m_Objective field points at, and m_mAssets is the very
//! map its m_mAssets field points at - not copies, not mirrors, not a synchronised pair. That is the
//! same structural trick Phase 1 used to wire the keyed asset API to the forward-base record, and it
//! is used again for the same reason: with one object there is no synchronisation step that can be
//! forgotten and no second source of truth to drift. The four thousand lines of runner that still
//! read m_Objective.phase are therefore reading the instance's own state, unedited, which is what
//! makes the framework land with byte-identical behaviour.
//! The record is folded INTO this class as the last shim dies (Phase 6), when the legacy phase enum
//! and the two success counters it still carries have no readers left.
//!
//! THE BAG IS THE SAVE FORMAT (D9). Modules declare Serialize/Deserialize and NONE of them override
//! them: every scrap of persisted module state is a bag key, written by the director's one serializer.
//! A per-module serializer would reproduce the CloneModule trap in the save layer - a module that
//! forgets to write a field loses it silently on every load - and there is no equivalent of the
//! dropped-line test for a format nobody can enumerate. The bag IS enumerable, so the round-trip case
//! asserts the whole thing rather than a list somebody maintained.
//!
//! ⚠ THE INT BAG IS INTEGERS AND NOTHING ELSE. A module that wants a metre-scale float scales it and
//! documents the unit in its key (fob.facingDeg), because a third typed map is a save-format change.
//! A module that wants a POSITION uses the vector bag; a module that wants a whole standing thing -
//! a forward base, a checkpoint - uses the asset registry, which is where the one string that fits
//! neither map lives.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveInstance : Managed
{
	//------------------------------------------------------------------------------------------------
	// BAG KEYS - the shipped set, with their owners
	//------------------------------------------------------------------------------------------------

	//! Harassment operations completed at this objective. Written by the town harassment behaviour
	//! module, read by the group ladder. ⚠ THE SUCCESS SIGNAL IS PULLED BY THE TICK, NEVER PUSHED BY
	//! THE COUNTER: raising this must not re-arm a timer or change a phase. See the director's
	//! ConsumeReportedOperations() for why - it cost two red cases in two suites once already.
	static const string BAG_HARASSMENT_SUCCESSES = "harassment.successes";

	//! Sabotage missions completed at this objective. Same rule as above.
	static const string BAG_SABOTAGE_SUCCESSES = "sabotage.successes";

	//! What Get() answers for a key nobody has written. Zero, which is what "no operations have
	//! completed" means for every counter the shipped machine keeps.
	static const int NO_VALUE = 0;

	//------------------------------------------------------------------------------------------------
	// STATE
	//------------------------------------------------------------------------------------------------

	//! The plan being run. WEAK - the registry owns it, and it outlives every objective.
	protected OVT_ObjectiveConfig m_Config;

	//! The director running this instance. WEAK - it owns the instance.
	protected OVT_ObjectiveDirectorComponent m_Director;

	//! What the objective is, where it is, what it is called, and every tick counter.
	//! ⚠ SHARED WITH THE DIRECTOR - see the class header.
	protected ref OVT_ObjectiveRecord m_Record;

	//! Which phase of the plan is running. Kept in step with the record's legacy phase enum by
	//! OVT_ObjectiveDirectorComponent.EnterPhase(), which is the one funnel every transition goes
	//! through.
	protected int m_iPhaseIndex;

	//! The authored name of that phase. THE PERSISTENCE KEY - the save carries this, never the index,
	//! so a plan may grow a phase in the middle without re-labelling saved objectives.
	protected string m_sPhaseName;

	//! Every integer a module has reported.
	protected ref map<string, int> m_mBag;

	//! Every position a module has handed forward. The only positions that cross a phase boundary in
	//! the shipped machine are the forward base's site and its source base, and both live on the asset
	//! record instead - this map is the general mechanism the next asset will use (D7).
	protected ref map<string, vector> m_mBagV;

	//! Everything the occupying faction has STANDING for this objective, keyed by asset key. The one
	//! generic container that carries a string (the deployment re-link name), which is why the two maps
	//! above can stay strictly numeric.
	protected ref map<string, ref OVT_ObjectiveAssetRecord> m_mAssets;

	//! The current phase's modules, cloned from the plan on entry and dropped on exit. NEVER the
	//! config's own module objects - see OVT_ObjectivePhase.CloneModules().
	protected ref array<ref OVT_BaseObjectiveModule> m_aRuntimeModules;

	//------------------------------------------------------------------------------------------------
	//! \param[in] director The director that owns this instance.
	void OVT_ObjectiveInstance(OVT_ObjectiveDirectorComponent director)
	{
		m_Director = director;

		m_Record = new OVT_ObjectiveRecord();
		m_mBag = new map<string, int>();
		m_mBagV = new map<string, vector>();
		m_mAssets = new map<string, ref OVT_ObjectiveAssetRecord>();
		m_aRuntimeModules = new array<ref OVT_BaseObjectiveModule>();

		m_iPhaseIndex = -1;
		m_sPhaseName = "";
	}

	//------------------------------------------------------------------------------------------------
	// THE BAG
	//------------------------------------------------------------------------------------------------

	//! \return An integer a module reported, or NO_VALUE when nobody has written that key.
	int Get(string key)
	{
		int value;
		if (!m_mBag || !m_mBag.Find(key, value))
			return NO_VALUE;

		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes an integer.
	//!
	//! ⚠ A PUBLIC MUTATOR MAY NEVER CHANGE PHASE. Nothing here advances, resets, re-arms a timer or
	//! ends an objective; only the director's tick moves the machine. That is not a style rule - it is
	//! the rule that keeps every transition behind the tick's three early returns, and breaking it once
	//! cost two red cases in two suites.
	void Set(string key, int value)
	{
		if (!m_mBag || key == "")
			return;

		m_mBag.Set(key, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds to an integer, creating it at the delta when it does not exist yet.
	//! \param[in] key The bag key, prefixed by the writing module.
	//! \param[in] delta How much to add. Negative is legal.
	//! \return The new value.
	int Report(string key, int delta)
	{
		int value = Get(key) + delta;
		Set(key, value);

		return value;
	}

	//! \return A position a module handed forward, or the zero vector when the key is unwritten.
	vector GetPos(string key)
	{
		vector value;
		if (!m_mBagV || !m_mBagV.Find(key, value))
			return vector.Zero;

		return value;
	}

	//! Writes a position. The same "never changes phase" rule as Set().
	void SetPos(string key, vector value)
	{
		if (!m_mBagV || key == "")
			return;

		m_mBagV.Set(key, value);
	}

	//------------------------------------------------------------------------------------------------
	//! Every int-bag key, for the serializer.
	//! \param[out] keys Receives the keys. Cleared first.
	//! \param[out] values Receives the values, in the same order.
	void ReadBag(notnull array<string> keys, notnull array<int> values)
	{
		keys.Clear();
		values.Clear();

		if (!m_mBag)
			return;

		foreach (string key, int value : m_mBag)
		{
			keys.Insert(key);
			values.Insert(value);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every vector-bag key, for the serializer.
	//! \param[out] keys Receives the keys. Cleared first.
	//! \param[out] values Receives the positions, in the same order.
	void ReadBagV(notnull array<string> keys, notnull array<vector> values)
	{
		keys.Clear();
		values.Clear();

		if (!m_mBagV)
			return;

		foreach (string key, vector value : m_mBagV)
		{
			keys.Insert(key);
			values.Insert(value);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces both bags wholesale, for the restore path.
	//!
	//! ⚠ CLEAR-AND-REBUILD, WHICH IS WHAT MAKES THE RESTORE IDEMPOTENT. Re-applying a save to a live
	//! session runs the deserialization again, and a merge rather than a replace would leave keys from
	//! the session's own objective on top of the restored one.
	//! \param[in] keys Bag keys, parallel with values.
	//! \param[in] values Bag values.
	void WriteBag(array<string> keys, array<int> values)
	{
		if (!m_mBag)
			return;

		m_mBag.Clear();

		if (!keys || !values || keys.Count() != values.Count())
			return;

		int count = keys.Count();
		for (int i = 0; i < count; i++)
		{
			if (keys[i] == "")
				continue;

			m_mBag.Set(keys[i], values[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the vector bag wholesale. Same rules as WriteBag().
	//! \param[in] keys Bag keys, parallel with values.
	//! \param[in] values Bag positions.
	void WriteBagV(array<string> keys, array<vector> values)
	{
		if (!m_mBagV)
			return;

		m_mBagV.Clear();

		if (!keys || !values || keys.Count() != values.Count())
			return;

		int count = keys.Count();
		for (int i = 0; i < count; i++)
		{
			if (keys[i] == "")
				continue;

			m_mBagV.Set(keys[i], values[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Empties both bags. Called when an objective is committed to or cleared - a bag is per-objective
	//! state and a key carried into the next objective would be a counter nobody earned.
	void ClearBags()
	{
		if (m_mBag)
			m_mBag.Clear();

		if (m_mBagV)
			m_mBagV.Clear();
	}

	//------------------------------------------------------------------------------------------------
	// THE ASSET REGISTRY
	//------------------------------------------------------------------------------------------------

	//! \return A standing asset by key, or null when the objective has no such asset.
	OVT_ObjectiveAssetRecord GetAsset(string key)
	{
		OVT_ObjectiveAssetRecord asset;
		if (!m_mAssets || !m_mAssets.Find(key, asset))
			return null;

		return asset;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers an asset record under a key.
	//!
	//! ⚠ THE MAP HOLDS THE RECORD ITSELF, NEVER A COPY. Everything that writes the record - the raise,
	//! the starvation counter, the teardown - and everything that reads it through IsAssetUp(key) is
	//! then looking at one object by construction. The only way the keyed API can disagree with the
	//! writer is if this call is deleted, which makes the whole key vanish rather than go subtly stale.
	//! \param[in] key The asset key.
	//! \param[in] asset The record to register.
	void SetAsset(string key, OVT_ObjectiveAssetRecord asset)
	{
		if (!m_mAssets || key == "" || !asset)
			return;

		m_mAssets.Set(key, asset);
	}

	//! \return The asset map itself, for the director to alias. STRANGLER-ONLY - see the class header.
	map<string, ref OVT_ObjectiveAssetRecord> GetAssetMap() { return m_mAssets; }

	//------------------------------------------------------------------------------------------------
	//! Every registered asset key, for the serializer.
	//! \param[out] keys Receives the keys. Cleared first.
	void ReadAssetKeys(notnull array<string> keys)
	{
		keys.Clear();

		if (!m_mAssets)
			return;

		foreach (string key, OVT_ObjectiveAssetRecord asset : m_mAssets)
		{
			keys.Insert(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	// THE PLAN AND THE PHASE
	//------------------------------------------------------------------------------------------------

	//! \return The plan this objective is running, or null when it was committed to before plans loaded.
	OVT_ObjectiveConfig GetConfig() { return m_Config; }

	//! \return The plan's persistence key, or an empty string when there is no plan.
	string GetConfigName()
	{
		if (!m_Config)
			return "";

		return m_Config.m_sObjectiveName;
	}

	//! Binds the instance to a plan. Called by the director when it commits, and by the restore path.
	void SetConfig(OVT_ObjectiveConfig config) { m_Config = config; }

	//! \return The director running this instance.
	OVT_ObjectiveDirectorComponent GetDirector() { return m_Director; }

	//! \return Which phase of the plan is running, or -1 before the first has been entered.
	int GetPhaseIndex() { return m_iPhaseIndex; }

	//! \return The authored name of that phase, or an empty string when there is none.
	string GetPhaseName() { return m_sPhaseName; }

	//------------------------------------------------------------------------------------------------
	//! Records which phase is running.
	//!
	//! ⚠ IT RECORDS, IT DOES NOT TRANSITION. Cloning the modules, re-arming the idle clock and pushing
	//! the evaluator anchor are the director's EnterPhase()'s job, and putting any of them here would
	//! give the machine a second way to change phase that does not go through the tick.
	//! \param[in] index The phase index.
	//! \param[in] name The phase's authored name.
	void RecordPhase(int index, string name)
	{
		m_iPhaseIndex = index;
		m_sPhaseName = name;
	}

	//! \return The phase the plan would advance into, or -1 when this is the last one.
	int GetNextPhaseIndex()
	{
		if (!m_Config)
			return -1;

		int next = m_iPhaseIndex + 1;
		if (next < 0 || next >= m_Config.GetPhaseCount())
			return -1;

		return next;
	}

	//------------------------------------------------------------------------------------------------
	// THE RUNTIME MODULES
	//------------------------------------------------------------------------------------------------

	//! \return How many modules the current phase is running.
	int GetRuntimeModuleCount()
	{
		if (!m_aRuntimeModules)
			return 0;

		return m_aRuntimeModules.Count();
	}

	//! \return One runtime module, or null when the index is out of range.
	OVT_BaseObjectiveModule GetRuntimeModule(int index)
	{
		if (!m_aRuntimeModules || index < 0 || index >= m_aRuntimeModules.Count())
			return null;

		return m_aRuntimeModules[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Clones a phase's modules into the runtime set and initialises each of them, in authored order.
	//!
	//! ⚠ THE OUTGOING SET IS EXITED FIRST, ALWAYS, even when the incoming phase is empty. A module that
	//! latched something on entry - a log line, a countdown, a marker - has to be told the phase ended
	//! or the next phase inherits it.
	//! \param[in] phase The phase being entered, or null to simply drop the current set.
	void EnterRuntimePhase(OVT_ObjectivePhase phase)
	{
		ExitRuntimePhase();

		if (!phase || !m_aRuntimeModules)
			return;

		phase.CloneModules(m_aRuntimeModules);

		foreach (OVT_BaseObjectiveModule module : m_aRuntimeModules)
		{
			if (module)
				module.Initialize(this);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tells every runtime module the phase is over and drops the set.
	void ExitRuntimePhase()
	{
		if (!m_aRuntimeModules)
			return;

		foreach (OVT_BaseObjectiveModule module : m_aRuntimeModules)
		{
			if (module)
				module.Exit();
		}

		m_aRuntimeModules.Clear();
	}

	//------------------------------------------------------------------------------------------------
	// THE TARGET, read straight off the shared record
	//------------------------------------------------------------------------------------------------

	//! \return The record itself, for the director to alias. STRANGLER-ONLY - see the class header.
	OVT_ObjectiveRecord GetRecord() { return m_Record; }

	//! \return What kind of place the objective is, or NONE.
	OVT_EObjectiveKind GetTargetKind() { return m_Record.kind; }

	//! \return Where the objective is. The zero vector when there is none.
	vector GetTargetPosition() { return m_Record.position; }

	//! \return The objective's display name. A LABEL, deliberately not persisted.
	string GetTargetName() { return m_Record.name; }

	//! \return True when this instance is running an objective rather than sitting idle.
	bool IsLive() { return m_Record.kind != OVT_EObjectiveKind.NONE; }

	//! \return Ticks left on the idle clock before the objective is abandoned as wedged.
	int GetPhaseTicks() { return m_Record.phaseTicks; }

	//! \return Ticks left before the next operation is sent.
	int GetNextOpTicks() { return m_Record.nextOpTicks; }
}
