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
//! WHY IT WAITS, AND WHERE THE OBSERVABLE COMES FROM NOW (re-derived 2026-08-18, at the end of the
//! virtualization epic). Nothing is tagged at campaign start. The producer this case relied on when it
//! was written - a base-upgrade patrol timer - does not exist any more: base defense is nine
//! Configs/Deployment/Deployment_Base*.conf deployments, so DEPLOYMENTS ARE NOW THE ONLY PRODUCER THIS
//! WORLD HAS, and their chain is longer than a timer's:
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
//! that was going to be red anyway. When it expires the failure prints the DEPLOYMENT LEDGER, which is
//! the one diagnostic left worth printing: it tells "the evaluator never bought anything" apart from
//! "deployments exist and hold groups but nothing tagged them", and those are completely different
//! faults. The base-upgrade ledger that used to sit beside it went with the base upgrades.
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
//! produces, and the case then goes red on claim 1 with the deployment ledger showing live
//! deployments and an empty registry - the exact "groups exist, nothing tagged them" signature.
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
		// The virtualization epic's kill-switch trivial-pass guard that stood here is GONE, and so is
		// the kill switch itself. Every producer this world has is a deployment, they register and TAG
		// their groups, so an empty registry is a broken tag and this case asserts for real.

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

			// One ledger, because there is one producer. "No deployment exists" and "deployments exist
			// but nothing tagged them" are completely different faults and this tells them apart.
			string report = string.Format("No AI group was tagged in the GM group registry within %1 ms (%2 ticks).",
				waited.ToString(), m_iPolls.ToString());
			report += " Deployment ledger: " + DescribeDeploymentState();

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
}
