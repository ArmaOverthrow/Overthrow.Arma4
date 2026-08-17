//------------------------------------------------------------------------------------------------
//! THE GM GROUP REGISTRY IS ACTUALLY FED BY THE CAMPAIGN, AND ITS SWEEP DOES NOT EAT LIVE ENTRIES.
//!
//! WHAT IT MEASURES. OVT_GMGroupRegistry has no spawn logic of its own - it is 17 one-line Tag() calls
//! sprinkled through four subsystems' spawn paths. Nothing in the type system says those calls exist,
//! nothing fails to compile if one is deleted, and the only consumer (the GM snapshot builder) is
//! multiplayer-only and therefore untestable here. This case is the ONLY automated evidence that the
//! tagging is wired up at all, so it asserts three separate things:
//!   1. after the occupying faction has spent its starting resources, the registry is NOT EMPTY;
//!   2. no entry carries OVT_EGroupOrigin.UNKNOWN - a Tag() call that passed a default-constructed
//!      origin would be silently useless to a GM, and only this catches it;
//!   3. Sweep() is idempotent: a second sweep, with every entry still resolving, removes nothing.
//!      That is the property the "tag and sweep, never untag" design rests on. A Sweep() that pruned
//!      on GetAgentsCount() instead of entity resolution would fail here as soon as one group's
//!      members were still queued (Reforger 1.8 AI spawn queue / dormancy).
//!
//! THE COUNT IS SWEPT FIRST, ON PURPOSE, AND THAT IS THE ANTI-VACUITY MEASURE. The registry is a
//! process-wide scripted singleton, NOT a component, so - unlike this project's manager singletons -
//! the engine never nulls it and it survives a world reload with a map full of dangling EntityIDs. An
//! assertion on the raw count could therefore pass on stale entries from a previous world and prove
//! nothing. Sweeping before counting makes the claim "at least one group tagged in THIS world is still
//! alive in THIS world", which is the claim worth making, and it makes the count and the sweep
//! assertions test each other: a Sweep() that removed everything would fail claim 1, a Sweep() that
//! removed nothing dangling would leave stale entries that claim 2 would still have to survive.
//!
//! WHY IT WAITS, AND WHERE THE OBSERVABLE COMES FROM NOW (re-derived 2026-08-17,
//! virtualization/integration T7.6). Nothing is tagged at campaign start. The producer this case used
//! to rely on - OVT_BasePatrolUpgrade.BuyPatrol() on a ~9-11 s base-upgrade timer - is still silenced
//! by the virtualization epic's legacy-spawn kill switch, which does not leave until the epic does.
//! The producer that IS live again is the DEPLOYMENT wave, and its chain is longer:
//!
//!   NewGameStart()  allocates baseResourcesPerTick (250) to the deployment manager, so the first
//!                   evaluation can already afford something;
//!   +10 s           OVT_DeploymentManagerComponent's first EvaluateDeployments() - the shipped
//!                   "Town Patrol" config costs 0 and the test world's one town is occupying-held
//!                   with no support, so it is created there;
//!   +8-12 s more    the new deployment's own jittered UpdateDeployment tick activates it, which
//!                   converges its infantry module - RegisterGroup, then the Tag() call in
//!                   OVT_BaseSpawningDeploymentModule.TagForGameMaster().
//!
//! So the observable lands ~18-22 s after the campaign start on the first evaluation, and ~48-52 s if
//! the first evaluation produces nothing and the 30 s one after it does. The budget below covers the
//! second cycle deliberately: it is a bound on the EXPLANATION, not a retry budget, and a green run
//! exits the moment the registry is non-empty, so the longer budget costs nothing except on a run
//! that was going to be red anyway. When it expires the failure prints the base-upgrade ledger, the
//! deployment ledger and the player-proximity state, because "registry empty" has three interesting
//! causes now and this tells them apart.
//!
//! THE PROXIMITY DIAGNOSTIC IS NOT DECORATION. Every base-upgrade spawn is gated on PlayerInRange()
//! at m_iMilitarySpawnDistance (1750 m). The test world is roughly 250 m across and the autotest
//! client spawns a real local player, so the gate is open - but if that ever changes, an empty
//! registry would be an environment fact rather than a broken tag, and the failure message has to say
//! which.
//!
//! IT ALSO PINS THE EPIC'S JOIN KEY. Every tagged group must carry an RplComponent, because the GM
//! snapshot sends groups keyed on RplId and a group without one cannot be matched to anything on a
//! client. Every group prefab Overthrow spawns roots at Prefabs/AI/Groups/Group_Base.et, which
//! declares one; this asserts it rather than trusting it, because losing it is exactly the kind of
//! prefab regression this project has hit before.
//!
//! READ-ONLY. Sweep() is the only registry method called that mutates anything, and it only drops
//! entries whose entity is already gone. No campaign state is touched, so no restore step is needed.
//!
//! PROVEN ABLE TO FAIL (static): commenting out the Tag() call in
//! OVT_BaseSpawningDeploymentModule.TagForGameMaster() removes the only origin the test world
//! produces while the kill switch is on, and the case then goes red on claim 1 with the deployment
//! ledger showing live deployments and an empty registry - the exact "groups exist, nothing tagged
//! them" signature. (The same used to be true of OVT_BasePatrolUpgrade.BuyPatrol(); that producer is
//! kill-switched until the virtualization epic ends and is no longer what this case measures.)
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 90)]
class OVT_TEST_Campaign_GMGroupRegistry : SCR_AutotestCaseBase
{
	//! Real milliseconds to wait for the first tagged group. The observable lands ~18-22 s after the
	//! campaign start, or ~48-52 s if the first deployment evaluation produces nothing (see the
	//! header); this is the diagnostic bound, not a retry budget.
	static const float MAX_WAIT_MS = 55000;

	//! World time of this case's first tick, so the wait is measured in real time rather than frames.
	protected float m_fFirstTickMs;

	//! Ticks spent waiting, reported in the failure message.
	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The kill-switch trivial-pass guard that stood here from the start of the virtualization epic
		// until 2026-08-17 is GONE (integration T7.6). It was correct while the epic had silenced every
		// producer this case can see; deployments - including radio-tower garrisons, which are now an
		// ordinary deployment config - register and TAG their groups again, so an empty registry is a
		// broken tag once more and this case asserts for real. The base-upgrade producers are still
		// silenced and are not what it measures; see the header.

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			SetFailure("GetGame().GetWorld() is null in a campaign-tier case");
			return true;
		}

		if (m_iPolls == 0)
			m_fFirstTickMs = world.GetWorldTime();

		m_iPolls += 1;

		OVT_GMGroupRegistry registry = OVT_GMGroupRegistry.GetInstance();
		if (!registry)
		{
			SetFailure("OVT_GMGroupRegistry.GetInstance() returned null - the singleton failed to construct");
			return true;
		}

		// Sweep BEFORE counting: entries from an earlier world would otherwise satisfy the count
		// without a single group having been tagged in this one.
		registry.Sweep();
		int live = registry.Count();

		if (live < 1)
		{
			float waited = world.GetWorldTime() - m_fFirstTickMs;
			if (waited < MAX_WAIT_MS)
				return false; // keep polling

			// Both ledgers, because the live producer moved: deployments tag now, base upgrades are
			// kill-switched. "No deployment exists" and "deployments exist but nothing tagged them"
			// are completely different faults and only the deployment ledger tells them apart.
			string report = string.Format("No AI group was tagged in the GM group registry within %1 ms (%2 ticks).",
				waited.ToString(), m_iPolls.ToString());
			report += " Deployment ledger: " + DescribeDeploymentState();
			report += " Base upgrade ledger (kill-switched during the virtualization epic): " + DescribeSpawnState();

			SetFailure(report);
			return true;
		}

		// Claim 2: every entry names a real origin.
		array<EntityID> ids = new array<EntityID>();
		array<ref OVT_GMGroupOrigin> origins = new array<ref OVT_GMGroupOrigin>();
		registry.GetAll(ids, origins);

		if (ids.Count() != live || origins.Count() != live)
		{
			SetFailure("GetAll() disagrees with Count(): %1 id(s) and %2 origin(s) for a count of %3",
				ids.Count().ToString(), origins.Count().ToString(), live.ToString());
			return true;
		}

		foreach (int i, OVT_GMGroupOrigin origin : origins)
		{
			if (!origin)
			{
				SetFailure("Registry entry %1 of %2 has a null origin object", i.ToString(), live.ToString());
				return true;
			}

			if (origin.m_iType == OVT_EGroupOrigin.UNKNOWN)
			{
				SetFailure("Registry entry %1 was tagged with OVT_EGroupOrigin.UNKNOWN (index %2, reason '%3') - a spawn site passed a default-constructed origin",
					i.ToString(), origin.m_iIndex.ToString(), origin.m_sReason);
				return true;
			}
		}

		// The epic's join key: a group with no RplComponent cannot be sent to, or matched on, a client.
		string missingRpl = FindGroupWithoutRpl(world, ids, origins);
		if (missingRpl != "")
		{
			SetFailure("A tagged group carries no RplComponent, so it can never be keyed on RplId in a GM snapshot: %1", missingRpl);
			return true;
		}

		// Claim 3: the sweep is idempotent. Everything resolved a moment ago, so a second pass must
		// drop nothing at all.
		int afterFirst = registry.Count();
		registry.Sweep();
		int afterSecond = registry.Count();

		if (afterSecond != afterFirst)
		{
			SetFailure("Sweep() is not idempotent: a second sweep over entries that all resolve took the count from %1 to %2. It is pruning on something other than entity resolution.",
				afterFirst.ToString(), afterSecond.ToString());
			return true;
		}

		PrintFormat("GM group registry: %1 live tagged group(s) after %2 tick(s). %3",
			live.ToString(), m_iPolls.ToString(), DescribeRegistry(origins));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! First tagged group that resolves to an entity with no RplComponent.
	//! \param[in] world The world the ids are resolved against.
	//! \param[in] ids Tagged group EntityIDs.
	//! \param[in] origins Matching origins, index-aligned with ids.
	//! \return A description of the offender, or "" when every resolvable group carries one.
	protected string FindGroupWithoutRpl(BaseWorld world, array<EntityID> ids, array<ref OVT_GMGroupOrigin> origins)
	{
		foreach (int i, EntityID id : ids)
		{
			IEntity entity = world.FindEntityByID(id);
			if (!entity)
				continue; // swept a moment ago; a group that died since is not this claim's business

			if (RplComponent.Cast(entity.FindComponent(RplComponent)))
				continue;

			string reason = "?";
			if (origins && i < origins.Count() && origins[i])
				reason = OriginLabel(origins[i]);

			return "entry " + i.ToString() + " (" + reason + "), prefab " + OVT_Global.GetPrefabName(entity);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Human-readable form of one origin, for logs and failure messages.
	//! \param[in] origin The origin to describe.
	//! \return "TYPE#index:reason".
	protected string OriginLabel(OVT_GMGroupOrigin origin)
	{
		if (!origin)
			return "null";

		return typename.EnumToString(OVT_EGroupOrigin, origin.m_iType) + "#" + origin.m_iIndex.ToString() + ":" + origin.m_sReason;
	}

	//------------------------------------------------------------------------------------------------
	//! Everything currently registered, one label per entry.
	//! \param[in] origins The registry's origins, in GetAll() order.
	//! \return A single-line dump.
	protected string DescribeRegistry(array<ref OVT_GMGroupOrigin> origins)
	{
		string dump = "[";

		foreach (int i, OVT_GMGroupOrigin origin : origins)
		{
			if (i > 0)
				dump += ", ";
			dump += OriginLabel(origin);
		}

		return dump + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! What the deployment framework - the producer this case actually measures since the tagging
	//! moved there - has managed to do so far.
	//!
	//! THREE FAULTS, TOLD APART BY ONE LINE. "No deployment exists" means the evaluator never ran, was
	//! refused (0 players, an active QRF) or could not afford anything, and the tags are innocent.
	//! "Deployments exist, none has registered a group" means their update ticks have not fired yet or
	//! their convergence refused. "Deployments exist and hold groups, registry empty" is the one that
	//! is a real broken tag.
	//! \return A single-line diagnostic.
	protected string DescribeDeploymentState()
	{
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return "OVT_Global.GetDeploymentManager() is null";

		array<OVT_DeploymentComponent> deployments = manager.GetAllDeployments();
		if (!deployments || deployments.IsEmpty())
		{
			int resources = 0;
			if (OVT_Global.GetConfig())
				resources = manager.GetFactionResources(OVT_Global.GetConfig().GetOccupyingFactionIndex());

			return "no deployment exists yet (occupying faction deployment budget " + resources.ToString() + ")";
		}

		string report = deployments.Count().ToString() + " deployment(s) {";

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();

		foreach (int i, OVT_DeploymentComponent deployment : deployments)
		{
			if (i > 0)
				report += ", ";

			if (!deployment)
			{
				report += "null";
				continue;
			}

			report += deployment.GetDeploymentName() + " key='" + deployment.GetVirtualKey() + "'";
			report += " active=" + deployment.IsDeploymentActive().ToString();
			report += " wiped=" + deployment.GetSpawnedUnitsEliminated().ToString();
		}

		report += "}";

		if (virtualization)
			report += " registry holds " + virtualization.GetGroupCount().ToString() + " virtual group(s)";

		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the registry might legitimately be empty: what the bases have actually spawned, and whether
	//! the player-proximity gate every base-upgrade spawn sits behind is even open.
	//!
	//! This is the whole anti-vacuity story of the failure path. "Registry empty AND every upgrade
	//! reports zero groups" means the campaign spawned nothing and the tags are innocent; "registry
	//! empty AND upgrades report groups" means a Tag() call is missing or unreachable.
	//! \return A single-line diagnostic.
	protected string DescribeSpawnState()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return "OVT_Global.GetOccupyingFaction() is null";

		if (occupying.m_Bases.Count() < 1)
			return "no bases are registered, so no base upgrade can have spawned anything";

		string report = "";

		for (int i = 0; i < occupying.m_Bases.Count(); i++)
		{
			OVT_BaseControllerComponent controller = occupying.GetBaseByIndex(i);
			if (!controller)
			{
				report += "base " + i.ToString() + ": no controller. ";
				continue;
			}

			report += "base " + i.ToString() + " " + DescribeProximity(controller) + " {";

			if (!controller.m_aBaseUpgrades)
			{
				report += "no upgrade list";
			}
			else
			{
				foreach (int u, OVT_BaseUpgrade upgrade : controller.m_aBaseUpgrades)
				{
					if (!upgrade)
						continue;

					if (u > 0)
						report += ", ";

					report += upgrade.ClassName() + " res=" + upgrade.GetResources().ToString();

					OVT_BasePatrolUpgrade patrol = OVT_BasePatrolUpgrade.Cast(upgrade);
					if (patrol)
						report += " groups=" + patrol.GetNumGroups().ToString() + " live=" + patrol.m_Groups.Count().ToString();
				}
			}

			report += "} ";
		}

		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player is close enough to this base for its upgrades to spawn anything at all.
	//! \param[in] controller The base controller.
	//! \return A short diagnostic naming the nearest player distance and the gate's threshold.
	protected string DescribeProximity(OVT_BaseControllerComponent controller)
	{
		IEntity owner = controller.GetOwner();
		if (!owner)
			return "(base entity is null)";

		int spawnDistance = 0;
		if (OVT_Global.GetConfig())
			spawnDistance = OVT_Global.GetConfig().m_iMilitarySpawnDistance;

		array<int> players = new array<int>();
		PlayerManager manager = GetGame().GetPlayerManager();
		manager.GetPlayers(players);

		float nearest = -1;
		foreach (int playerId : players)
		{
			IEntity controlled = manager.GetPlayerControlledEntity(playerId);
			if (!controlled)
				continue;

			float distance = vector.Distance(controlled.GetOrigin(), owner.GetOrigin());
			if (nearest < 0 || distance < nearest)
				nearest = distance;
		}

		if (nearest < 0)
			return "(no player-controlled entity exists, so PlayerInRange() is false and NO base upgrade can spawn)";

		return "(nearest player " + nearest.ToString() + "m, spawn gate " + spawnDistance.ToString() + "m)";
	}
}
