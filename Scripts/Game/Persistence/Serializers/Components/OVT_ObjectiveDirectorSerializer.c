//------------------------------------------------------------------------------------------------
//! Persists the occupying faction's current objective: what it is, which phase it is in, every
//! timer and counter, the blacklist, and the forward operating base.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! ⚠ WHY THIS IS ITS OWN SERIALIZER AND NOT AN APPENDIX TO THE OCCUPYING FACTION'S (D6).
//! OVT_OccupyingFactionManagerSerializer is version 2, fully positional, and its record classes carry
//! no version of their own - every field of OVT_PersistedBase is load-bearing by position, which is
//! why a dead `upgrades` array is still declared and still written there rather than removed. The
//! objective payload is new and expected to GROW as the remaining build phases land; putting an
//! actively-evolving structure inside the most fragile format in the epic is how that format got its
//! reputation. A separate serializer costs one config entry and buys a clean version-1 format that
//! grows by appending.
//!
//! ⚠ NOTHING HERE TOUCHES THE RESOURCE POOL OR ANY DEPLOYMENT, AND THAT IS A LOAD-ORDER RULE.
//! OVT_OccupyingFactionManager.c documents the hazard this inherits: the deployment manager's restore
//! CLEARS AND REFILLS the faction resource pool and runs AFTER the game-mode component serializers,
//! so anything credited or debited from in here would be overwritten moments later. Deployment
//! entities are separate tracked instances whose restore order relative to this payload is not
//! defined either. Both are therefore deferred to the director's FIRST TICK - the same pattern the
//! legacy upgrade refund uses (QueueLegacyUpgradeRefund -> CreditPendingLegacyRefund).
//!
//! DESERIALIZE IS A PURE CODEC. It reads the payload in order and makes exactly ONE side-effecting
//! call, into ApplyPersistedObjective(). No lookup, no query, no arithmetic on live state.
//!
//! A LIVE BATTLE STILL ROLLS BACK, exactly as the campaign has always done: nothing about
//! m_CurrentQRF is persisted, so an objective saved mid-counter-attack comes back with no battle to
//! wait for and the director resets it on its first tick, with the reason in the log.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). Every line of the apply is an
//! assignment or a clear-and-rebuild, so a second pass produces the same state.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version FIRST, and new
//! fields are APPENDED - never inserted, never reordered, never removed.
//!
//!   1  int    version                   = 1
//!   2  int    objectiveKind             OVT_EObjectiveKind
//!   3  vector objectivePosition         THE KEY - positions are what this epic already persists
//!   4  int    phase                     OVT_EObjectivePhase
//!   5  int    phaseTicks
//!   6  int    nextOpTicks
//!   7  int    harassmentSuccesses
//!   8  int    sabotageSuccesses
//!   9  array<vector> blacklistPositions
//!  10  array<int>    blacklistRounds
//!  11  bool   fobUp
//!  12  vector fobPosition
//!  13  vector fobSourceBasePosition
//!  14  int    fobSpent
//!  15  int    fobStarvationTicks
//!  16  string fobDeploymentName         the re-link key, matched on the first tick by name+position
//!
//! ⚠ THE OBJECTIVE'S DISPLAY NAME IS DELIBERATELY NOT IN THE PAYLOAD. It is a label resolved from the
//! town and base registries, not an identifier; storing it would let a save disagree with the world
//! it is restored into.
//------------------------------------------------------------------------------------------------
class OVT_ObjectiveDirectorSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_ObjectiveDirectorComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the current objective, the blacklist and the forward operating base.
	//! \param[in] owner The game mode entity owning the director.
	//! \param[in] component The director being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow objective director.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.Cast(component);
		if (!director)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 1);

		const int objectiveKind = director.GetObjectiveKind();
		context.Write(objectiveKind);

		const vector objectivePosition = director.GetObjectivePosition();
		context.Write(objectivePosition);

		const int phase = director.GetPhase();
		context.Write(phase);

		const int phaseTicks = director.GetPhaseTicks();
		context.Write(phaseTicks);

		const int nextOpTicks = director.GetNextOpTicks();
		context.Write(nextOpTicks);

		const int harassmentSuccesses = director.GetHarassmentSuccesses();
		context.Write(harassmentSuccesses);

		const int sabotageSuccesses = director.GetSabotageSuccesses();
		context.Write(sabotageSuccesses);

		// TWO PARALLEL ARRAYS rather than a record class, for the reason the deployment manager's
		// serializer gives for the same shape: it keeps the payload to the primitive array forms the
		// binary context is known to round-trip, and the pairing is re-associated by index on load.
		array<vector> blacklistPositions = new array<vector>();
		array<int> blacklistRounds = new array<int>();

		int blacklistCount = director.GetBlacklistCount();
		for (int i = 0; i < blacklistCount; i++)
		{
			blacklistPositions.Insert(director.GetBlacklistPosition(i));
			blacklistRounds.Insert(director.GetBlacklistRounds(i));
		}

		context.Write(blacklistPositions);
		context.Write(blacklistRounds);

		const bool fobUp = director.IsFOBUp();
		context.Write(fobUp);

		const vector fobPosition = director.GetFOBPosition();
		context.Write(fobPosition);

		const vector fobSourceBasePosition = director.GetFOBSourceBasePosition();
		context.Write(fobSourceBasePosition);

		const int fobSpent = director.GetFOBSpent();
		context.Write(fobSpent);

		const int fobStarvationTicks = director.GetFOBStarvationTicks();
		context.Write(fobStarvationTicks);

		const string fobDeploymentName = director.GetFOBDeploymentName();
		context.Write(fobDeploymentName);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the objective back and hands it to the director to apply.
	//! \param[in] owner The game mode entity owning the director.
	//! \param[in] component The director being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.Cast(component);
		if (!director)
			return false;

		// See OVT_DeploymentManagerSerializer.Deserialize(). An ABSENT payload must not clear live
		// state: a campaign saved before this serializer existed, or one whose record failed to read,
		// keeps whatever objective it currently has rather than being silently sent back to idle.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		int objectiveKind;
		context.Read(objectiveKind);

		vector objectivePosition;
		context.Read(objectivePosition);

		int phase;
		context.Read(phase);

		int phaseTicks;
		context.Read(phaseTicks);

		int nextOpTicks;
		context.Read(nextOpTicks);

		int harassmentSuccesses;
		context.Read(harassmentSuccesses);

		int sabotageSuccesses;
		context.Read(sabotageSuccesses);

		array<vector> blacklistPositions = new array<vector>();
		context.Read(blacklistPositions);

		array<int> blacklistRounds = new array<int>();
		context.Read(blacklistRounds);

		bool fobUp;
		context.Read(fobUp);

		vector fobPosition;
		context.Read(fobPosition);

		vector fobSourceBasePosition;
		context.Read(fobSourceBasePosition);

		int fobSpent;
		context.Read(fobSpent);

		int fobStarvationTicks;
		context.Read(fobStarvationTicks);

		string fobDeploymentName;
		context.Read(fobDeploymentName);

		// The six forward-base fields travel as one record purely so the apply call has a readable
		// signature. Building it is still a codec's job - it is plain data, and nothing is looked up.
		OVT_ObjectiveFOBRecord fob = new OVT_ObjectiveFOBRecord();
		fob.up = fobUp;
		fob.position = fobPosition;
		fob.sourceBasePosition = fobSourceBasePosition;
		fob.spent = fobSpent;
		fob.starvationTicks = fobStarvationTicks;
		fob.deploymentName = fobDeploymentName;

		director.ApplyPersistedObjective(objectiveKind, objectivePosition, phase, phaseTicks, nextOpTicks, harassmentSuccesses, sabotageSuccesses, blacklistPositions, blacklistRounds, fob);

		return true;
	}
}
