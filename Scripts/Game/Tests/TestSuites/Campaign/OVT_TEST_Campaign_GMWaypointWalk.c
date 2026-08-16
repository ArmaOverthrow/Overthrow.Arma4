//------------------------------------------------------------------------------------------------
//! THE CYCLE RECURSION IN THE GAME MASTER ROUTE WALK, PINNED AGAINST REAL WAYPOINT ENTITIES.
//!
//! WHY THIS IS NOT A LOGIC CASE. OVT_GMWaypointWalk.Flatten takes real AIWaypoint entities and casts
//! them, so it needs a world. Everything it depends on that does NOT need a world - the classifier
//! and the leg arithmetic - is pinned in the Logic tier instead.
//!
//! WHAT IT MEASURES, AND WHY IT IS THE MOST VALUABLE CASE IN THE FEATURE. Overthrow's perimeter
//! patrols put ALL of their waypoints inside a single AIWaypointCycle
//! (OVT_OverthrowConfigComponent.c:524-551), so the most common route in the game reports exactly ONE
//! waypoint - the container - unless the walk opens it. A walk that skipped that expansion would
//! compile, run, send a well-formed route over the wire and draw one short meaningless line for every
//! town and base patrol in the campaign. Nothing but this case can tell that apart from a route that
//! genuinely has one waypoint.
//!
//! THE FIXTURE IS CONSTRUCTED, NOT OBSERVED. It spawns 4 patrol + 4 wait waypoints through the same
//! config helpers GivePatrolWaypoints(PERIMETER) uses and nests them in a cycle, so the expected
//! count is 8 by construction rather than by whatever the campaign happens to have spawned. NO GROUP
//! AND NO AI ARE CREATED: the case is deterministic and costs nothing.
//!
//! SetWaypoints ON THE CYCLE IS FIXTURE CONSTRUCTION AND LIVES ONLY HERE. The shipped feature is
//! strictly read-only and its quality bar greps for that call; building the thing to be read is the
//! one legitimate use of it, and it must never appear in a non-test file.
//!
//! IT ALSO PINS THE CLASSIFIER AGAINST REAL PREFAB NAMES. Every flattened entry is classified from
//! the prefab it was actually spawned from, so the Logic tier's string literals are checked against
//! what the config really assigns rather than against themselves.
//!
//! ANTI-VACUITY. The failure messages print the flattened count and the first entry's class name, so
//! the two ways this can go wrong read differently: "count 1, AIWaypointCycle" is a missing
//! expansion, "count 0" is a fixture that never spawned.
//!
//! PROVEN ABLE TO FAIL (static, 2026-08-15): deleting the AIWaypointCycle.Cast branch from Flatten
//! leaves the container to be emitted by the plain path, so case 1 goes red with "flattened 1
//! waypoint(s) ... first entry is AIWaypointCycle" and cyclic false - the exact missing-expansion
//! signature, not a silent pass.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 60)]
class OVT_TEST_Campaign_GMWaypointWalk : SCR_AutotestCaseBase
{
	//! Patrol/wait pairs, matching what GivePatrolWaypoints(PERIMETER) builds.
	static const int CORNERS = 4;

	//! Waypoints the fixture therefore holds: one patrol and one wait per corner.
	static const int EXPECTED = 8;

	//! The cap used by the shipped server fan, and the one case 1 must not hit.
	static const int CAP = 32;

	//! A cap deliberately below EXPECTED, so case 2 measures truncation rather than declaring it.
	static const int SMALL_CAP = 3;

	//! Metres between fixture waypoints. Any spacing works - nothing here walks anywhere.
	static const float SPACING = 25;

	//! Everything spawned by the current fixture, deleted before the case returns on every path.
	protected ref array<IEntity> m_aSpawned;

	//! The cycle container holding the fixture's waypoints.
	protected AIWaypointCycle m_Cycle;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- CASE 1: a cycle is opened, and what comes out is its children.
		string built = BuildFixture();
		if (built != "")
		{
			SetFailure("The waypoint fixture could not be built: %1", built);
			return CleanUp();
		}

		array<AIWaypoint> source = new array<AIWaypoint>();
		source.Insert(m_Cycle);

		array<AIWaypoint> flat = new array<AIWaypoint>();
		bool cyclic = false;
		bool truncated = false;

		int count = OVT_GMWaypointWalk.Flatten(source, CAP, flat, cyclic, truncated);

		if (count != EXPECTED || flat.Count() != EXPECTED)
		{
			SetFailure("Flattening one cycle holding %1 waypoints gave %2. %3",
				EXPECTED.ToString(), count.ToString(), Describe(flat));
			return CleanUp();
		}

		if (flat.Find(m_Cycle) != -1)
		{
			SetFailure("The cycle container itself survived into the flattened route. %1", Describe(flat));
			return CleanUp();
		}

		if (!cyclic)
		{
			SetFailure("A route built from a cycle was not reported cyclic, so its closing leg would never be drawn. %1",
				Describe(flat));
			return CleanUp();
		}

		if (truncated)
		{
			SetFailure("A %1-waypoint route was reported truncated under a cap of %2", EXPECTED.ToString(), CAP.ToString());
			return CleanUp();
		}

		// Every entry classifies from the prefab it was really spawned from - this is what ties the
		// Logic tier's literals to the config's actual assignments.
		string misclassified = FindMisclassified(flat);
		if (misclassified != "")
		{
			SetFailure("A flattened waypoint did not classify as the kind it was spawned as: %1", misclassified);
			return CleanUp();
		}

		CleanUp();

		// --- CASE 2: the cap is honoured, not merely declared.
		built = BuildFixture();
		if (built != "")
		{
			SetFailure("The waypoint fixture could not be rebuilt for the truncation case: %1", built);
			return CleanUp();
		}

		source.Clear();
		source.Insert(m_Cycle);
		flat.Clear();
		cyclic = false;
		truncated = false;

		count = OVT_GMWaypointWalk.Flatten(source, SMALL_CAP, flat, cyclic, truncated);

		if (count != SMALL_CAP || flat.Count() != SMALL_CAP)
		{
			SetFailure("A cap of %1 over an %2-waypoint cycle emitted %3 waypoint(s)",
				SMALL_CAP.ToString(), EXPECTED.ToString(), count.ToString());
			return CleanUp();
		}

		if (!truncated)
		{
			SetFailure("A cap of %1 stopped an %2-waypoint cycle without reporting truncation, so a GM would see a partial route presented as a whole one",
				SMALL_CAP.ToString(), EXPECTED.ToString());
			return CleanUp();
		}

		PrintFormat("GM waypoint walk: a cycle holding %1 waypoints flattens to %1 real waypoints with the container dropped and the route marked cyclic, and a cap of %2 truncates honestly",
			EXPECTED.ToString(), SMALL_CAP.ToString());

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns 4 patrol + 4 wait waypoints and nests them in a cycle, exactly as a perimeter patrol is
	//! built. No group and no AI: nothing here is ever executed by anything.
	//! \return "" on success, or a description of what was missing.
	protected string BuildFixture()
	{
		m_aSpawned = new array<IEntity>();
		m_Cycle = null;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return "OVT_Global.GetConfig() is null";

		vector center = FixtureCenter();

		array<AIWaypoint> children = new array<AIWaypoint>();

		for (int i = 0; i < CORNERS; i++)
		{
			vector pos = center + Vector(SPACING * (i + 1), 0, 0);

			AIWaypoint patrol = config.SpawnPatrolWaypoint(pos);
			if (!patrol)
				return "SpawnPatrolWaypoint() produced nothing at corner " + i.ToString();

			m_aSpawned.Insert(patrol);
			children.Insert(patrol);

			AIWaypoint wait = config.SpawnWaitWaypoint(pos, 60);
			if (!wait)
				return "SpawnWaitWaypoint() produced nothing at corner " + i.ToString();

			m_aSpawned.Insert(wait);
			children.Insert(wait);
		}

		AIWaypointCycle cycle = AIWaypointCycle.Cast(config.SpawnBasicCycleWaypoint(center));
		if (!cycle)
			return "the configured cycle prefab did not produce an AIWaypointCycle";

		m_aSpawned.Insert(cycle);
		m_Cycle = cycle;

		// Fixture construction, and the one permitted use of this API anywhere in the feature.
		cycle.SetWaypoints(children);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Somewhere in the test world to put the fixture. A player body if there is one, so the waypoints
	//! land on loaded terrain; the world origin otherwise. Nothing about the case depends on where.
	//! \return The position the fixture is built around.
	protected vector FixtureCenter()
	{
		PlayerManager manager = GetGame().GetPlayerManager();
		if (manager)
		{
			array<int> players = new array<int>();
			manager.GetPlayers(players);

			foreach (int playerId : players)
			{
				IEntity controlled = manager.GetPlayerControlledEntity(playerId);
				if (controlled)
					return controlled.GetOrigin();
			}
		}

		return "0 0 0";
	}

	//------------------------------------------------------------------------------------------------
	//! First flattened entry whose classification disagrees with the prefab it was spawned from.
	//! \param[in] flat The flattened route.
	//! \return "" when every entry is a PATROL or a WAIT, or a description of the first that is not.
	protected string FindMisclassified(array<AIWaypoint> flat)
	{
		foreach (int i, AIWaypoint waypoint : flat)
		{
			if (!waypoint)
				return "entry " + i.ToString() + " is null";

			string prefab = OVT_PrefabUtils.GetPrefabName(waypoint);
			int type = OVT_GMWaypointFormat.ClassifyPrefab(prefab);

			if (type == OVT_EGMWaypointType.PATROL || type == OVT_EGMWaypointType.WAIT)
				continue;

			return "entry " + i.ToString() + " spawned from '" + prefab + "' classified as "
				+ typename.EnumToString(OVT_EGMWaypointType, type) + ", expected PATROL or WAIT";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! What actually came out of the walk, in the terms that tell the failure modes apart: a count of
	//! 1 whose first entry is an AIWaypointCycle is a missing expansion, a count of 0 is a fixture
	//! that never spawned.
	//! \param[in] flat The flattened route.
	//! \return A single-line diagnostic.
	protected string Describe(array<AIWaypoint> flat)
	{
		if (!flat)
			return "The flattened list is null";

		if (flat.Count() < 1)
			return "The flattened list is EMPTY - the fixture spawned nothing, rather than the expansion failing";

		string first = "null";
		if (flat[0])
			first = flat[0].ClassName();

		return "Flattened " + flat.Count().ToString() + " waypoint(s); first entry is " + first
			+ " (a count of 1 naming AIWaypointCycle means the cycle was never opened)";
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes everything the fixture spawned. Called on every exit path, including failures.
	//! \return True, so it can be returned directly from the test step.
	protected bool CleanUp()
	{
		m_Cycle = null;

		if (!m_aSpawned)
			return true;

		foreach (IEntity entity : m_aSpawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}

		m_aSpawned.Clear();

		return true;
	}
}
