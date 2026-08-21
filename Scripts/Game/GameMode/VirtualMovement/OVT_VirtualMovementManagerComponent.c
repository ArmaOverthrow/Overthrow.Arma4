[EntityEditorProps(category: "Overthrow/Managers", description: "Walks dormant virtualized AI groups along their own waypoint plans")]
class OVT_VirtualMovementManagerComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Virtual movement - the one missing tick (implementation.md §3.4).
//!
//! WHAT THIS IS. Virtualization core gives every registered group a durable, dormant group entity
//! with a writable origin and its waypoint plan kept verbatim on the record. NOTHING ADVANCES IT: a
//! patrol registered at the north end of its route is still at the north end four campaign hours
//! later, and materialises there when a player finally walks up. This manager walks those dormant
//! groups along their own plans, in straight lines, at a fixed configurable speed.
//!
//! SERVER ONLY. Nothing here replicates: no replicated properties, no remote calls, no JIP payload,
//! no client half. Nothing here PERSISTS either - progress is transient and re-derivable by design
//! (see OVT_VirtualMovementState), so there is no serializer and no Overthrow.conf entry. The
//! feature's Definition of Done greps this directory to prove both.
//!
//! =============================== THE FOUR RULES THIS FILE OBEYS ===============================
//! 1. IsSpawned() IS THE WHOLE GATE. It is the FIRST statement of the per-handle body, before any
//!    read, and a group that materialises mid-route is dropped from the state map on the same pass.
//!    A live group is never written to. Vehicle groups - which stay materialised by design via a huge
//!    spawnDistanceOverride - are therefore excluded BY CONSTRUCTION: there is no vehicle flag and no
//!    road routing anywhere in this feature, and no code here knows what a vehicle is (D2).
//! 2. SetPosition() IS THE ONLY MUTATION. Core's frozen §10 surface is all this feature programs
//!    against - GetAllHandles, GetRecord, GetPosition, SetPosition, IsSpawned - and SetPosition is
//!    the only writer among them. Waypoint ENTITIES and group lifecycle are never touched (D11).
//! 3. EVERY POSITION WRITTEN IS VALID. Ground-snapped through GetSurfaceY, and never in water: the
//!    virtual accumulator advances through a bay, the written origin holds at the last land sample.
//!    A group can only ever MATERIALISE on land, which is the requirement that actually matters (D6).
//! 4. THE PLAN IS THE OPT-IN. Every registered dormant group with something to advance is advanced;
//!    an empty or DEFEND-only plan means "this group belongs here" and is never touched. There is no
//!    movement flag, no core field and no registration argument (D10).
//! =============================================================================================
//!
//! ⚠ KNOWN LIMITATION, DOCUMENTED RATHER THAN ENGINEERED AROUND (finding F-B). A virtually moved
//! group materialises where movement put it and then walks its route from PLAN INDEX 0, because the
//! engine only ever retires a route point that live AI actually finished - and a dormant group
//! finished none. Cost: up to one lap for a cycling patrol, nothing for a single-target plan. The
//! only legitimate fix is a CORE change (re-order or rebuild a group's own route entities at
//! materialisation) requested through api.md's additive process - never a rewrite bolted on here.
//------------------------------------------------------------------------------------------------
class OVT_VirtualMovementManagerComponent : OVT_Component
{
	//------------------------------------------------------------------------------------------------
	// ATTRIBUTES
	//------------------------------------------------------------------------------------------------

	[Attribute(defvalue: "1.5", desc: "Metres per second a dormant group walks along its plan. Infantry pace. 0 or less freezes every group. Doubles as the play-test accelerator - crank it to 10-20 to compress an observation window into a minute")]
	protected float m_fVirtualSpeedMs;

	[Attribute(defvalue: "2000", desc: "Movement tick interval in ms. Mirrors the virtualization ambient cadence; the cost knob nobody should need to touch. Per-group speed does NOT depend on it - elapsed time is measured per group")]
	protected int m_iTickIntervalMs;

	[Attribute(defvalue: "8", desc: "Groups advanced per tick - the round-robin slice size. With 40 registered groups and 8 per tick each is advanced once per 10 s, and dt keeps its effective speed identical to a group advanced every 2 s")]
	protected int m_iGroupsPerTick;

	[Attribute(defvalue: "false", desc: "DEBUG ONLY: log every advance - handle, from -> to, target index, step metres, wait remaining, land/water and whether the state was just adopted or resumed. This is the play-test instrument. Leave off for normal play")]
	protected bool m_bDebugMovementLogging;

	//------------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//------------------------------------------------------------------------------------------------

	//! handle -> transient progress. Allocated on the SERVER ONLY; null everywhere else, which is what
	//! makes the tick and every entry point below fail closed on a client.
	//!
	//! ⚠ WHAT IS AND IS NOT IN HERE. An entry exists only for a dormant group that is actively walking
	//! a plan, or that WAS walking one and has latched stationary at the end of it (a DEFEND post, a
	//! route that ran out). A group whose PLAN has nothing to advance - empty, null or DEFEND-only -
	//! deliberately holds NO entry at all: it is re-classified, cheaply, on each pass instead. That is
	//! what keeps the map empty in a campaign full of garrisons, and it is why the runtime latch has to
	//! keep its entry - dropping it would re-derive a movable plan and walk the group off the post it
	//! just took up.
	protected ref map<int, ref OVT_VirtualMovementState> m_mState;

	//! Round-robin cursor: where the last tick's slice stopped. Indexes into the SORTED handle list, and
	//! handles come from a monotonic counter, so ascending order is registration order - a stable
	//! round-robin in which no group can starve.
	protected int m_iCursor;

	//! True once the movement CallLater is running, so it can never be installed twice (two ticks would
	//! double every group's effective speed and neither would be visible in a log).
	protected bool m_bTickRunning;

	//------------------------------------------------------------------------------------------------
	// STATIC
	//------------------------------------------------------------------------------------------------

	static OVT_VirtualMovementManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The manager on the game mode, or null before the game mode exists / when the component
	//! is not on the prefab.
	//!
	//! ⚠ RE-RESOLVES ACROSS A WORLD. A static outlives a campaign - virtualization core measured this
	//! in its own Phase 1 spike: statics survive both a Continue and a new campaign in the same
	//! process. The cached instance is therefore only trusted while it still belongs to the game mode
	//! that exists NOW; a manager left over from the previous campaign is dropped and looked up again,
	//! so the second campaign of a session can never be handed the first one's state map. The tick
	//! leans on the same static as its arbiter (see MovementTick).
	static OVT_VirtualMovementManagerComponent GetInstance()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();

		// No game mode yet (very early init, or a world without one): answer with whatever OnPostInit
		// claimed rather than dropping a perfectly good instance.
		if (!gameMode)
			return s_Instance;

		IEntity gameModeEntity = gameMode;
		if (s_Instance && s_Instance.GetOwner() != gameModeEntity)
			s_Instance = null;

		if (!s_Instance)
			s_Instance = OVT_VirtualMovementManagerComponent.Cast(gameMode.FindComponent(OVT_VirtualMovementManagerComponent));

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server guard FIRST, allocation second (virtualization core's shape) - a client that allocated
	//! the state map would look initialised while its tick could never legitimately write anything.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Claimed here rather than lazily so the tick's dead-world arbiter has something to compare
		// against from the first frame. OnDelete clears it.
		s_Instance = this;

		if (!Replication.IsServer())
			return;

		m_mState = new map<int, ref OVT_VirtualMovementState>();
		m_iCursor = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from OVT_OverthrowGameMode.EOnInit, AFTER the virtualization manager - this feature reads
	//! that registry every tick and must never be the thing that brings it into existence.
	//!
	//! Deliberately installs nothing: the tick starts at PostGameStart so it never runs against a
	//! half-built world.
	//! \param[in] owner The game mode entity.
	void Init(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		// VERBOSE like core's own informational lines: the only things this feature prints above VERBOSE
		// are operator-actionable problems and the debug logging, which is off by default.
		Print(string.Format("[Overthrow] VirtualMovementManager initialized (%1 m/s, %2 ms tick, %3 groups per tick)",
			m_fVirtualSpeedMs.ToString(), m_iTickIntervalMs.ToString(), m_iGroupsPerTick.ToString()), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from OVT_OverthrowGameMode.DoStartGame(), after the virtualization manager's own
	//! PostGameStart - so anything core registers at game start (its debug test group, and later any
	//! consumer's groups) is already in the registry when the first pass runs.
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		EnsureMovementTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Restart hygiene. A second campaign in the same session must inherit nothing: no tick pointing at
	//! a dead component, no progress from the previous world's groups, and no stale s_Instance.
	//!
	//! These are core's Phase 6 lessons copied verbatim - it found four separate teardown bugs this way,
	//! and the failure mode here would be silent: two ticks running means every group walks at double
	//! speed, which no log line would show.
	override void OnDelete(IEntity owner)
	{
		// The call queue lives on the GAME, not on the world, so an entry left here would survive into
		// the next campaign of the same session and tick a dead manager. Null-guarded because a teardown
		// can run late enough that the game object is already going away.
		ScriptCallQueue queue;
		if (GetGame())
			queue = GetGame().GetCallqueue();

		if (queue)
			queue.Remove(MovementTick);

		m_bTickRunning = false;
		m_iCursor = 0;

		// Nothing in here is worth keeping and everything in it is re-derivable, so a teardown simply
		// throws it away (that IS the design - see OVT_VirtualMovementState).
		if (m_mState)
			m_mState.Clear();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// THE TICK
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Installs the movement CallLater once. Idempotent: two ticks would double every group's effective
	//! speed and halve the round-robin period, and neither would be visible in a log.
	protected void EnsureMovementTick()
	{
		if (m_bTickRunning)
			return;

		if (!Replication.IsServer())
			return;

		int interval = m_iTickIntervalMs;
		if (interval <= 0)
			interval = 2000;

		GetGame().GetCallqueue().CallLater(MovementTick, interval, true);
		m_bTickRunning = true;
	}

	//------------------------------------------------------------------------------------------------
	//! One round-robin pass over a slice of the registry (§3.4).
	//!
	//! WHY A SLICE AND NOT THE WHOLE REGISTRY. Per-tick cost has to be flat as the campaign grows, so
	//! only m_iGroupsPerTick handles are advanced per pass and the cursor guarantees fairness. The
	//! per-group step is NOT "speed x interval" but "speed x time since THIS group was last touched", so
	//! a registry of 40 groups moves each of them at exactly the same metres per second as a registry
	//! of 8 (D5). The only whole-registry work per pass is one array, one insertion sort and a
	//! count-guarded purge - no world query, no entity iteration, no proximity check.
	protected void MovementTick()
	{
		// SELF-CANCEL FOR A DEAD WORLD. The call queue outlives a campaign, so a repeating entry whose
		// OnDelete never ran would keep ticking a manager from the previous world - and once the next
		// campaign's manager installs its own tick, both would run and every group would walk twice as
		// fast. The static is the arbiter: exactly one manager can be s_Instance.
		if (s_Instance != this)
		{
			ScriptCallQueue queue;
			if (GetGame())
				queue = GetGame().GetCallqueue();

			if (queue)
				queue.Remove(MovementTick);

			m_bTickRunning = false;
			return;
		}

		// A client's map is never allocated, so this is also the client fail-closed.
		if (!m_mState)
			return;

		// Core absent (a world without the manager, or a client) means there is nothing to advance. Not a
		// warning: this feature is a consumer of core, not a requirement of it.
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (!virt)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		array<int> handles = virt.GetAllHandles();

		// Nothing registered: drop everything tracked. This is the purge in its degenerate form, and it
		// is what makes "the tracked count returns to 0 once everything is unregistered" true.
		if (!handles || handles.IsEmpty())
		{
			if (m_mState.Count() > 0)
				m_mState.Clear();

			return;
		}

		// GetAllHandles allocates a fresh array per call, so this sorts in place without copying. Core
		// returns the registry MAP's order, which is not stable; a round-robin over an unstable order can
		// starve a group forever, and ascending handle order is registration order (D7).
		OVT_VirtualMovementMath.SortHandlesAscending(handles);

		// COUNT-GUARDED. Walking the state map against the handle list every pass would be the one piece
		// of per-tick work that grows with the registry; it can only be needed when there are more
		// tracked handles than live ones. A stale entry that slips past the guard (a walking group
		// unregistered while a stationary one still holds no entry) is harmless - handles are monotonic
		// and never reused, so it can never be misattributed - and the next real shrink collects it.
		if (m_mState.Count() > handles.Count())
			PurgeMissingState(handles);

		array<int> slice = OVT_VirtualizationMath.SliceIndices(handles.Count(), ResolveSliceSize(), m_iCursor);
		m_iCursor = OVT_VirtualizationMath.AdvanceCursor(m_iCursor, ResolveSliceSize(), handles.Count());

		float nowMs = world.GetWorldTime();

		foreach (int index : slice)
		{
			AdvanceHandle(virt, world, handles[index], nowMs);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The round-robin slice size, never below 1. A 0 is treated as the default rather than as
	//! an off switch - the off switch is a speed of 0, which stops every group without also stopping the
	//! bookkeeping that keeps their state honest.
	protected int ResolveSliceSize()
	{
		if (m_iGroupsPerTick <= 0)
			return 8;

		return m_iGroupsPerTick;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops progress for handles that are no longer registered.
	//!
	//! Collected first and removed second: removing from a map while iterating it is not safe.
	//! \param[in] handles The live handles this pass, ascending.
	protected void PurgeMissingState(notnull array<int> handles)
	{
		array<int> gone = new array<int>();

		foreach (int handle, OVT_VirtualMovementState state : m_mState)
		{
			if (handles.Find(handle) == -1)
				gone.Insert(handle);
		}

		foreach (int handle : gone)
		{
			m_mState.Remove(handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One group's pass: the gate, the state, the step, the type semantics and the write.
	//! \param[in] virt The virtualization registry.
	//! \param[in] world The world, for the ground snap.
	//! \param[in] handle The registered handle to advance.
	//! \param[in] nowMs World time in milliseconds, read once per tick.
	protected void AdvanceHandle(notnull OVT_VirtualizationManagerComponent virt, notnull BaseWorld world, int handle, float nowMs)
	{
		// ================================ THE GATE (G2/R10) ================================
		// FIRST, before anything else is read. Core's SetPosition on a spawned group would relocate the
		// group entity out from under its living members, and the doc rule against it is not enforced in
		// core's code - so this is where it is enforced. The state goes with it: when the group despawns
		// again its progress is re-derived from wherever the live AI actually left it, not from where
		// this tick last had it.
		if (virt.IsSpawned(handle))
		{
			m_mState.Remove(handle);
			return;
		}

		OVT_VirtualGroupRecord record = virt.GetRecord(handle);
		if (!record)
		{
			m_mState.Remove(handle);
			return;
		}

		// "The record exists, the entity does not" is a legitimate runtime state (the engine can delete a
		// group entity with no callback). Core answers with the record's last known position and books
		// SetPosition onto the record either way, so nothing here needs a special case - and must not
		// add one.
		vector recorded = virt.GetPosition(handle);

		OVT_VirtualMovementState state = m_mState.Get(handle);
		bool adopted = false;
		bool resumed = false;

		// ================== AUTO-ADOPTION AND EXTERNAL-MOVE RE-DERIVATION (D9) ==================
		// One comparison covers three cases that would otherwise each need their own machinery: a
		// consumer registering a group at a live delivery position (no state at all - the same code
		// path), a consumer calling SetPosition itself, and a group handed back by live AI somewhere
		// this tick never chose. No invoker, no notification API, no registration argument.
		if (state && OVT_VirtualMovementMath.IsExternalMove(recorded, state.m_vLastWritten, OVT_VirtualMovementMath.EXTERNAL_MOVE_EPSILON_M))
		{
			m_mState.Remove(handle);
			state = null;
			resumed = true;
		}

		if (!state)
		{
			// Live-handoff fix (2026-08-17): ask core for the group's LIVE current-waypoint plan index first. On a route
			// whose legs coincide (a two-point cycling patrol walks the same line both ways) the
			// position-only projection cannot recover direction, and a group that despawned walking
			// home would be resumed walking out again. -1 (no entity, no waypoint, unmatchable list)
			// falls back to the projection - loads and teleports resume exactly as before.
			int liveIndex = virt.GetCurrentPlanIndex(handle);

			state = DeriveState(record, recorded, nowMs, liveIndex);

			// A plan with nothing to advance holds NO entry: it is re-classified (a walk of one short
			// array) on each pass instead. That is what keeps the map empty in a campaign of garrisons.
			// A state that latches stationary LATER, at a DEFEND post or the end of a route, is kept -
			// see m_mState's comment.
			if (state.m_bStationary)
				return;

			m_mState.Insert(handle, state);

			if (!resumed)
				adopted = true;
		}

		// PER-GROUP dt (D5), clamped: a negative delta (a world-time reset) floors at 0 rather than
		// throwing the group backwards, and a stalled call queue or a paused debugger cannot bank time
		// into one enormous jump. Stamped HERE so every branch below - stationary, waiting, water - has
		// already consumed the elapsed time it is declining to use.
		float dt = (nowMs - state.m_fLastTickMs) / 1000;
		if (dt < 0)
			dt = 0;

		if (dt > OVT_VirtualMovementMath.MAX_STEP_SECONDS)
			dt = OVT_VirtualMovementMath.MAX_STEP_SECONDS;

		state.m_fLastTickMs = nowMs;

		// Latched at a DEFEND post or the end of a non-cycling single-point route. Permanent until
		// somebody moves the group, which the external-move check above turns back into a fresh
		// derivation. Deliberately silent: a garrison must not produce a log line every 2 s forever.
		if (state.m_bStationary)
			return;

		// WAIT is honoured FROM THE PLAN, unconditionally. The upstream defect where
		// SpawnWaitWaypoint(pos, time) drops its duration is a LIVE-AI defect; this tick never calls that
		// helper, so the two are independent (D6).
		if (state.m_fWaitRemaining > 0)
		{
			state.m_fWaitRemaining = OVT_VirtualMovementMath.DrainWait(state.m_fWaitRemaining, dt);
			LogAdvance(handle, state.m_vVirtual, state.m_vVirtual, state, 0, "wait", adopted, resumed);
			return;
		}

		// Re-read the plan every pass rather than caching it on the state: a consumer may legitimately
		// re-register or reshape a group between passes, and an index cached against a plan that has
		// since shrunk would read the wrong position - or none at all.
		OVT_VirtualWaypointPlan plan = record.m_Plan;
		if (!plan || !plan.m_aPositions || plan.m_aPositions.IsEmpty())
		{
			state.m_bStationary = true;
			return;
		}

		int count = plan.m_aPositions.Count();

		if (state.m_iTargetIndex < 0 || state.m_iTargetIndex >= count)
		{
			// The plan changed under us. Re-project rather than index out of range.
			state.m_iTargetIndex = OVT_VirtualMovementMath.ProjectOntoPlan(plan.m_aPositions, plan.m_bCycle, recorded);
			if (state.m_iTargetIndex < 0)
			{
				state.m_bStationary = true;
				return;
			}
		}

		float step = OVT_VirtualMovementMath.ResolveStepDistance(m_fVirtualSpeedMs, dt, OVT_VirtualMovementMath.MAX_STEP_SECONDS);

		// A zero step is the derivation pass (dt is 0 because the state was just stamped with now) or a
		// speed of 0. Returning here rather than falling through means neither manufactures an arrival
		// out of the ARRIVAL_RADIUS_M tolerance, and neither writes a position identical to the last one.
		if (step <= 0)
			return;

		vector from = state.m_vVirtual;
		bool arrived = false;

		state.m_vVirtual = OVT_VirtualMovementMath.AdvanceTowardsXZ(from, plan.m_aPositions[state.m_iTargetIndex], step, arrived);

		if (arrived)
			ApplyArrival(state, plan, count);

		WriteAdvance(virt, world, handle, state, from, step, adopted, resumed);
	}

	//------------------------------------------------------------------------------------------------
	// STATE DERIVATION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Builds a group's progress from nothing but its position and its plan (§3.3).
	//!
	//! THIS IS THE WHOLE RESUME MECHANISM. It runs on the first touch of a handle - which is also the
	//! first touch after a load, after a restart, after the group spent time materialised, and
	//! immediately after a consumer registers a delivered group. Projection answers "which leg was it
	//! walking?" for a group on its route, and "which leg should it head for?" for one that is nowhere
	//! near it, with the same line of code.
	//!
	//! Direction is NOT recoverable by projection and always starts at +1, so a resumed ping-pong route
	//! may be walked the other way. Accepted by the requirements: worst case is re-walking part of one
	//! leg, and nobody can observe where a dormant patrol "should" have been.
	//! \param[in] record The registry record, for its plan.
	//! \param[in] position The group's actual position right now.
	//! \param[in] nowMs World time in milliseconds - stamped so the first pass has a dt of 0 and cannot
	//!                  jump.
	//! \return A fresh state. m_bStationary is set when the PLAN has nothing to advance, in which case
	//!         the caller deliberately does not keep it.
	protected OVT_VirtualMovementState DeriveState(notnull OVT_VirtualGroupRecord record, vector position, float nowMs, int liveIndex = -1)
	{
		OVT_VirtualMovementState state = new OVT_VirtualMovementState();

		state.m_vVirtual = position;
		state.m_vLastWritten = position;
		state.m_fLastTickMs = nowMs;
		state.m_iDirection = 1;
		state.m_fWaitRemaining = 0;
		state.m_iTargetIndex = -1;

		OVT_VirtualWaypointPlan plan = record.m_Plan;
		if (!plan)
		{
			state.m_bStationary = true;
			return state;
		}

		// THE OPT-IN (D10). No plan, an empty one, an all-DEFEND one, or a ragged one core would have
		// refused: all of them mean "this group belongs here".
		if (OVT_VirtualMovementMath.IsStationaryPlan(plan.m_aPositions, plan.m_aTypes))
		{
			state.m_bStationary = true;
			return state;
		}

		// Live-handoff fix (2026-08-17): the live current-waypoint index (from core, via the caller) beats the projection -
		// it is the one source that knows which WAY the group was walking. The projection remains the
		// resume path for everything with no live truth: loads, teleports, deliveries off the route.
		if (liveIndex >= 0 && liveIndex < plan.m_aPositions.Count())
		{
			state.m_iTargetIndex = liveIndex;
			return state;
		}

		state.m_iTargetIndex = OVT_VirtualMovementMath.ProjectOntoPlan(plan.m_aPositions, plan.m_bCycle, position);
		if (state.m_iTargetIndex < 0)
			state.m_bStationary = true;

		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the arrived-at waypoint's type and picks the next index (§3.5).
	//!
	//! | MOVE   | walk to the point, advance to the next index                                        |
	//! | PATROL | identical to MOVE - the m_aParams radius is NOT simulated, because nobody can observe
	//!            a dormant group patrolling a circle and live AI does the patrolling on materialisation |
	//! | WAIT   | hold for m_aParams[i] seconds, then advance; <= 0 advances immediately              |
	//! | DEFEND | the group is at its post: latch stationary, permanently                             |
	//! | CYCLE  | end of route: wrap to index 0                                                        |
	//!
	//! End of route (D8): a cycling plan wraps to 0 forever; a non-cycling plan with two or more points
	//! PING-PONGS, because a patrol that reaches the end of its route and then freezes for the rest of
	//! the campaign is the exact frozen-world defect this feature exists to remove; a single-point plan
	//! arrives and latches.
	//! \param[inout] state The state that just arrived.
	//! \param[in] plan The group's plan.
	//! \param[in] count Number of points in the plan.
	protected void ApplyArrival(notnull OVT_VirtualMovementState state, notnull OVT_VirtualWaypointPlan plan, int count)
	{
		int index = state.m_iTargetIndex;

		int type = OVT_EVirtualWaypointType.MOVE;
		if (plan.m_aTypes && index >= 0 && index < plan.m_aTypes.Count())
			type = plan.m_aTypes[index];

		if (type == OVT_EVirtualWaypointType.DEFEND)
		{
			state.m_bStationary = true;
			return;
		}

		if (type == OVT_EVirtualWaypointType.CYCLE)
		{
			// Wrapping onto the index just arrived at would re-arrive forever without ever moving, so a
			// CYCLE that IS index 0 (or a one-point plan) latches instead. Core also appends a looping
			// route entity of its own when m_bCycle is set; both routes mean the same thing here.
			if (count <= 1 || index == 0)
			{
				state.m_bStationary = true;
				return;
			}

			state.m_iTargetIndex = 0;
			state.m_iDirection = 1;
			return;
		}

		// SEARCH is a WAIT to the dormant walk: nobody can see the men poke around the house, so the
		// only thing worth simulating is how long they are there before they move on.
		if (type == OVT_EVirtualWaypointType.WAIT || type == OVT_EVirtualWaypointType.SEARCH)
		{
			float wait = 0;
			if (plan.m_aParams && index >= 0 && index < plan.m_aParams.Count())
				wait = plan.m_aParams[index];

			if (wait > 0)
				state.m_fWaitRemaining = wait;
		}

		int direction = state.m_iDirection;
		int next = OVT_VirtualMovementMath.NextTargetIndex(index, count, plan.m_bCycle, direction);
		state.m_iDirection = direction;

		if (next < 0)
		{
			state.m_bStationary = true;
			return;
		}

		state.m_iTargetIndex = next;
	}

	//------------------------------------------------------------------------------------------------
	// THE WRITE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Ground-snaps and writes the advanced position - THE ONLY MUTATION THIS FEATURE MAKES (§3.6).
	//!
	//! THE WATER RULE (D6). The virtual accumulator has already advanced; if it is over water, nothing
	//! is written and the group's origin stays at the last land sample - the shore - while progress
	//! keeps accruing, so the first land sample on the far side is written normally. The alternative,
	//! holding AT the water's edge, permanently deadlocks any cycling route with a leg that clips a bay:
	//! the group could never reach its target, so it could never advance its index. The cost is that a
	//! group crosses water in a straight line as though it had a boat, which is unobservable by
	//! construction (the group is dormant - that is the entire premise) and cannot put anyone in the sea
	//! (a materialisation only ever happens at a written, land, ground-snapped position).
	//!
	//! No clearance probe, no FindSafeSpawnPosition (a documented trap for anything but a
	//! pedestrian-sized caller) and no TraceBox: a group origin is a locator, and the engine places the
	//! members itself when the group materialises.
	//! \param[in] virt The virtualization registry.
	//! \param[in] world The world, for GetSurfaceY.
	//! \param[in] handle The handle being advanced.
	//! \param[inout] state Its state; m_vLastWritten is updated on a real write.
	//! \param[in] from Where the accumulator was before this step, for the log.
	//! \param[in] step The step distance covered, for the log.
	//! \param[in] adopted True when the state was derived from nothing this pass.
	//! \param[in] resumed True when the state was re-derived after an external move this pass.
	protected void WriteAdvance(notnull OVT_VirtualizationManagerComponent virt, notnull BaseWorld world, int handle,
		notnull OVT_VirtualMovementState state, vector from, float step, bool adopted, bool resumed)
	{
		if (OVT_WorldUtils.IsOceanAtPosition(state.m_vVirtual))
		{
			LogAdvance(handle, from, state.m_vVirtual, state, step, "water", adopted, resumed);
			return;
		}

		vector position = state.m_vVirtual;
		position[1] = world.GetSurfaceY(position[0], position[2]);

		virt.SetPosition(handle, position);
		state.m_vLastWritten = position;

		LogAdvance(handle, from, position, state, step, "land", adopted, resumed);
	}

	//------------------------------------------------------------------------------------------------
	// DIAGNOSTICS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many handles currently hold transient progress.
	//!
	//! ⚠ DIAGNOSTIC, NOT API. Nothing in the feature reads it and no consumer should: the state map is
	//! transient, re-derivable and deliberately empty for every group whose PLAN cannot move (a campaign
	//! full of garrisons tracks nothing). It exists so the no-leak claim - the tracked count returns to 0
	//! once everything is unregistered - can be asserted from a test instead of taken on trust, and so a
	//! play-test can read the number without a debugger.
	//! \return The number of tracked handles; 0 on a client, where the map is never allocated.
	int GetTrackedCount()
	{
		if (!m_mState)
			return 0;

		return m_mState.Count();
	}

	//------------------------------------------------------------------------------------------------
	// DEBUG LOGGING
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! One line per advance, and nothing at all unless m_bDebugMovementLogging is switched on.
	//!
	//! Printed at NORMAL rather than VERBOSE precisely BECAUSE the attribute is off by default: somebody
	//! who turned it on is watching a play-test and wants the lines in the console without a log-level
	//! change. A stationary group logs nothing ever - that is the D10 opt-in being visible by absence.
	//! \param[in] handle The handle advanced.
	//! \param[in] from Where the accumulator was.
	//! \param[in] to Where it (and, on a land write, the group) now is.
	//! \param[in] state The state, for the target index and the wait timer.
	//! \param[in] step Step distance in metres.
	//! \param[in] verdict "land", "water" or "wait".
	//! \param[in] adopted True when the state was derived from nothing this pass.
	//! \param[in] resumed True when the state was re-derived after an external move this pass.
	protected void LogAdvance(int handle, vector from, vector to, notnull OVT_VirtualMovementState state, float step,
		string verdict, bool adopted, bool resumed)
	{
		if (!m_bDebugMovementLogging)
			return;

		string derivation = "";
		if (adopted)
			derivation = " adopted";
		else if (resumed)
			derivation = " resumed";

		Print(string.Format("[Overthrow] VirtualMovement: handle %1 %2 -> %3 target %4 step %5 m wait %6 s %7%8",
			handle.ToString(), from.ToString(), to.ToString(), state.m_iTargetIndex.ToString(),
			step.ToString(), state.m_fWaitRemaining.ToString(), verdict, derivation), LogLevel.NORMAL);
	}
}
