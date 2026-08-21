//------------------------------------------------------------------------------------------------
//! KEEPING A GROUP THAT IS DRIVING A VEHICLE ALIVE **AND RUNNING**, and saying why it is not.
//!
//! ==========================================================================================
//! 🔴 "MATERIALISED" AND "ACTIVE" ARE TWO DIFFERENT GATES. THIS FILE OWNS THE SECOND ONE.
//! ==========================================================================================
//!
//! Every spawner in this tree already knows about the first gate: SCR_AIGroup's ProximityDriven
//! lifecycle policy decides whether a group's member CHARACTERS exist at all, and Overthrow's
//! virtualization core stamps it from the ring a consumer registered with. A crew registered at
//! 100 km is inside that ring from anywhere on the map, so its men exist. That is the gate the
//! insertion and vehicle modules were written against, it works, and it is not enough.
//!
//! THE SECOND GATE IS THE PER-AGENT LOD SYSTEM, and it is not distance-configurable from a spawn
//! ring at all. Vanilla states it twice, in its own words:
//!
//!   AIAgent.c (generated):     "AIAgents get disabled in MaxLOD by default."
//!   SCR_AIGroup.c:118-123:     "the per-agent LOD system steps every DYNAMICSIM_LASTLOD_DISTANCE /
//!                               sMaxLODs metres (default 100 m/level) and only fires EOnDeactivate
//!                               -> IsAIActivated=false at LOD max (default >=1000 m)"
//!   SCR_AIToggleMaxLOD.c:90:   "prevents AIAgent to change to MAX lod - that disactivates AI"
//!
//! So a driver more than roughly a kilometre from the nearest observer is a materialised man sitting
//! in a materialised truck with NO BEHAVIOUR TREE RUNNING. He holds the move waypoint he was given and
//! never executes it. From the outside that is indistinguishable from "the crew never spawned", which
//! is exactly how it was misdiagnosed: an insertion whose truck "never left its spawn point" while
//! IsCrewAlive() answered true throughout (user play-test, 2026-08-21, 2.4 km from the convoy).
//!
//! ⚠ THIS IS WHY A BIGGER SPAWN RING CAN NEVER FIX IT. RIDING_SPAWN_DISTANCE is already 100000 and the
//! truck still did not move. The ring answers "do these men exist"; nothing about it answers "is
//! anyone home".
//!
//! THE RECIPE IS VANILLA'S OWN, COPIED RATHER THAN INVENTED. SCR_ResupplyTaskSolver is Conflict's
//! AI-driven supply convoy - the one vanilla feature with this exact requirement, a truck crossing
//! country with no player near it - and it does precisely two things: PreventMaxLOD() on the group as
//! it issues the boarding waypoint (:159), and AllowMaxLOD() on BOTH the completion and the failure
//! path (:216, :243). SCR_AIStaticArtilleryBehavior does the same for a gun crew that has to keep
//! firing unobserved (:93, :100). If a materialised group could drive unobserved without this, neither
//! of those would exist.
//!
//! ⚠ SetLOD BEFORE PreventMaxLOD, AND BOTH ARE NEEDED. The proto's own warning is "If the AIAgent is
//! in LOD10 this won't change it" - PreventMaxLOD stops an agent REACHING max LOD, it does not lift one
//! that is already there, and a crew registered 2 km from the nearest player is already there by the
//! time anything script-side sees it. The resupply solver drops such an agent to maxLOD-1 first; so
//! does this.
//!
//! ==========================================================================================
//! ⚠ PIN NARROWLY, RELEASE WIDELY. THE ASYMMETRY IS THE LEAK PREVENTION.
//! ==========================================================================================
//! A pinned agent costs simulation for as long as it is pinned, so the pin is applied only to men who
//! have a job to do (a crew), while the release is applied to EVERY rider a caller ever touched,
//! whatever role it thought they had. A role misclassification can therefore waste a pin but can never
//! strand one. AllowMaxLOD() on an agent that was never pinned is a no-op, which is what makes the wide
//! release free.
//!
//! ⚠ AND THE PIN CANNOT OUTLIVE THE MEN. It lives on the AGENT, not on the group record and not in any
//! save: a dormant transition deletes the member entities outright, so a group that goes back on the
//! ordinary proximity ring drops every pin it was carrying whether or not anybody released it. The
//! explicit releases below are the first line of defence; returning the group to its ordinary ring is
//! the backstop, and callers must still do both.
//!
//! ⚠ PASSENGERS ARE DELIBERATELY NOT PINNED. See OVT_InsertionSpawningDeploymentModule.HoldRidersActive
//! for the reasoning - in short, an active passenger acts on the plan he is carrying, and the plan
//! points at the objective, which is how a squad leader ends up driving the convoy somewhere nobody
//! asked for. Cargo has no job; the driver does.
//------------------------------------------------------------------------------------------------
class OVT_MountedGroupActivation
{
	//! Mirrors SCR_AIGroup's own m_fVeryNearBlockDistance default (:125). It is protected there with no
	//! getter, and this class needs it only to SAY whether a group is inside the engine's pop-in block -
	//! nothing here branches on it. See DescribeSpawnState for why that band matters so much to a group
	//! registered on a huge ring.
	static const int VERY_NEAR_BLOCK_M = 150;

	//------------------------------------------------------------------------------------------------
	//! Keeps every member of a group out of max LOD, so its behaviour tree keeps running however far
	//! away the nearest observer is.
	//!
	//! IDEMPOTENT AND CHEAP - call it every tick. Both calls are no-ops on an agent that is already
	//! below max LOD and already prevented, which matters because a core-registered group fills
	//! PROGRESSIVELY through the engine's spawn queue: the man who arrives on the third dispatch was
	//! not there to be pinned on the first.
	//! \param[in] group The group to keep running; null is legal (it may be dormant or gone).
	static void HoldGroupActive(SCR_AIGroup group)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			HoldAgentActive(agent);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The per-member version, for the arrival hook that fires once per man as the spawn queue produces
	//! him. Same two calls, same order, same reasons - see the class header.
	//! \param[in] agent The man to keep running.
	static void HoldAgentActive(AIAgent agent)
	{
		if (!agent)
			return;

		int maxLod = AIAgent.GetMaxLOD();

		// Lift him off max first: PreventMaxLOD only stops an agent REACHING it.
		if (agent.GetLOD() == maxLod)
			agent.SetLOD(maxLod - 1);

		agent.PreventMaxLOD();
	}

	//------------------------------------------------------------------------------------------------
	//! Hands a group back to the LOD system. Safe on a group that was never pinned, which is the whole
	//! point - see "pin narrowly, release widely" in the class header.
	//! \param[in] group The group to release; null is legal.
	static void ReleaseGroupActive(SCR_AIGroup group)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			ReleaseAgentActive(agent);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] agent The man to hand back to the LOD system.
	static void ReleaseAgentActive(AIAgent agent)
	{
		if (!agent)
			return;

		agent.AllowMaxLOD();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] group Any group, or null.
	//! \return How many members are materialised right now. 0 for a dormant or missing group.
	static int MaterialisedCount(SCR_AIGroup group)
	{
		if (!group)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		return agents.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] group Any group, or null.
	//! \return How many of its materialised members have a behaviour tree actually running.
	static int ActiveCount(SCR_AIGroup group)
	{
		if (!group)
			return 0;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		int active = 0;
		foreach (AIAgent agent : agents)
		{
			if (agent && agent.IsAIActivated())
				active = active + 1;
		}

		return active;
	}

	//------------------------------------------------------------------------------------------------
	//! THE WORST LOD IN THE GROUP, because that is the one that decides whether anything happens: one
	//! man at max LOD in a two-man crew is a truck with no driver half the time.
	//! \param[in] group Any group, or null.
	//! \return The highest (worst) LOD any member is at, or -1 when there are no members.
	static int WorstAgentLOD(SCR_AIGroup group)
	{
		if (!group)
			return -1;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		int worst = -1;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			int lod = agent.GetLOD();
			if (lod > worst)
				worst = lod;
		}

		return worst;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE LINE THAT ANSWERS "why is nothing happening" - the diagnostic this whole file exists because
	//! nobody had. Reads only; never pins, never releases.
	//!
	//! Deliberately says BOTH counts and the LOD. "0 materialised" is a spawn-ring or AI-budget problem;
	//! "2 materialised, 0 AI-active, LOD 10 of 10" is this file's problem and names itself.
	//! \param[in] group Any group, or null.
	//! \return A compact human-readable state, e.g. "2 materialised, 2 AI-active, LOD 4 of 10".
	static string DescribeActivation(SCR_AIGroup group)
	{
		if (!group)
			return "no group entity";

		int materialised = MaterialisedCount(group);
		if (materialised == 0)
		{
			// ⚠ NO LONGER "dormant or spawn-queued". That phrase was honest and useless; the caller now
			// appends DescribeSpawnState() and DescribeAiBudget(), which say which.
			return "0 materialised";
		}

		int worstLod = WorstAgentLOD(group);

		return string.Format("%1 materialised, %2 AI-active, worst LOD %3 of %4",
			materialised.ToString(), ActiveCount(group).ToString(), worstLod.ToString(),
			AIAgent.GetMaxLOD().ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! SPLITS "0 materialised" INTO THE THREE THINGS IT CAN ACTUALLY MEAN.
	//!
	//! ==========================================================================================
	//! ⚠ THE OLD LINE SAID "dormant or spawn-queued" AND THAT AMBIGUITY COST A ROUND OF PLAY-TESTING.
	//! ==========================================================================================
	//! A group with no members is in one of three completely different situations, wanting three
	//! completely different fixes, and the engine can be asked about all three from script:
	//!
	//!   DORMANT, NO OBSERVER          IsDormant() true and HasObserverInRange(ring) FALSE. The ring is
	//!                                 not doing what the registration thought it was. Look at the ring.
	//!   DORMANT, OBSERVER IN RANGE    the group SHOULD be materialising and is not. Either its spawn
	//!                                 request is being refused (see the budget clause) or its dormant
	//!                                 alive count is 0, which makes SCR_AIGroup.RequestSpawn refuse
	//!                                 outright as "eliminated" (SCR_AIGroup.c:2680-2688) - that one is
	//!                                 OUR bug, a survivor mask and engine counts out of agreement.
	//!   NOT DORMANT, STILL EMPTY      the request is sitting in ChimeraAIWorld's queue waiting for a
	//!                                 dispatch that has not come. Read the budget clause.
	//!
	//! ⚠ AND SINCE 2026-08-21 IT ALSO CARRIES THE ONE THING NONE OF THE THREE COULD ANSWER: whether the
	//! engine's spawn queue has ever actually DISPATCHED this group. Every clause above describes the
	//! group's ELIGIBILITY, and three rounds of play-testing established that a crew can be eligible on
	//! every single one of them and still have nobody in it a minute later. See modded SCR_AIGroup's
	//! counter block - "requests made, queue dispatched N" is the whole diagnosis in two numbers.
	//!
	//! ⚠ GetDormantAliveCount() == -1 IS THE "NEVER DESPAWNED" SENTINEL, not a count. A freshly
	//! registered virtual group reads -1 because core builds it with zero members and it has never been
	//! through a dormant transition; RequestSpawn treats that as "spawn one and let the expand pass top
	//! it up". A 0 there is the dangerous value and is why it is printed rather than summarised.
	//! ==========================================================================================
	//! ⚠ AND THE FOURTH THING IT CAN MEAN, WHICH THE THREE ABOVE DO NOT COVER AND WHICH COST A SECOND
	//! PLAY-TEST: THE POP-IN BLOCK (2026-08-21).
	//! ==========================================================================================
	//! SCR_AIGroup.LifecycleTick has a clause between "observer is in range" and "ask for a spawn"
	//! (:3036-3044): an observer inside m_fVeryNearBlockDistance - 150 m by default and NOT settable
	//! through SetLifecyclePolicy's -1 - makes it RETURN WITHOUT ENQUEUEING ANYTHING, so the men never
	//! pop into existence in somebody's face. It is skipped only when the observer arrived suddenly,
	//! which the tick decides by comparing this tick's "inside the spawn ring" bit against last
	//! tick's.
	//!
	//! 🔴 A GROUP ON A HUGE RING CAN NEVER TAKE THAT ESCAPE HATCH. Every observer is inside a 100 km
	//! ring on every tick, so the previous-tick bit is true from the first tick onwards and the
	//! approach is always judged "gradual" - which means ANY observer within 150 m of the group's
	//! registration point (a player, and also a Game Master free camera, which
	//! SCR_DefenderSpawnerComponent:604 names as an observer) blocks that group's men from ever
	//! materialising, permanently, and nothing anywhere logs it. That is the exact opposite of what a
	//! 100 km ring was asked for. Reading the band is how that gets settled by measurement.
	//!
	//! ⚠ THE 150 IS MIRRORED, NOT READ. m_fVeryNearBlockDistance is protected on SCR_AIGroup with no
	//! getter; this is a diagnostic string, so a server that dialled it elsewhere gets a slightly
	//! wrong sentence rather than a wrong decision. Nothing branches on it.
	//!
	//! ⚠ AND THE REFILL SEAM IS ASKED TOO. IsExpandComplete() answering TRUE on a group with no
	//! members means the queue books every request as satisfied and drops it: the group is a HUSK that
	//! nothing will ever repopulate. For a core-owned group that is the survivor mask and the
	//! per-activation slot list disagreeing (see modded SCR_AIGroup.ExpandOneMember), and it is the one
	//! shape in this line that is Overthrow's bug rather than a proximity rule.
	//! \param[in] group The group to describe; null is legal.
	//! \param[in] spawnDistance The ring the group was registered on, in metres.
	//! \return A compact description of WHY it has no members, or an empty string when it has some.
	static string DescribeSpawnState(SCR_AIGroup group, int spawnDistance)
	{
		if (!group)
			return "no group entity";

		string dormant = "engine says NOT dormant";
		if (group.IsDormant())
			dormant = "engine says dormant";

		// ⚠ THE RADIUS IS OURS, THE ANSWER IS THE ENGINE'S, AND CONFUSING THE TWO IS HOW THIS LINE COULD
		// LIE. `spawnDistance` is what OUR RECORD says the ring should be; HasObserverInRange is a
		// ChimeraAIGroup proto (generated/AI/ChimeraAIGroup.c:24) doing a real 2D observer query. So
		// "observer IS within its ring" is an honest engine answer to a question WE chose the radius for -
		// which is worth almost nothing on a 100 km ring, since everything is inside it. What the engine's
		// own LifecycleTick actually gates on is m_fSpawnDistance, read back below.
		string observer = "no observer within the ring OUR RECORD claims";
		if (spawnDistance > 0 && group.HasObserverInRange(spawnDistance))
			observer = "observer IS within the ring OUR RECORD claims";

		// 🔴 THE READ-BACK, AND IT IS THE ONLY THING HERE THAT CAN CATCH A RING THAT WAS NEVER APPLIED.
		// Everything else in this line describes what we INTENDED. These two are what SCR_AIGroup is
		// actually carrying, and they are what LifecycleTick tests against (:3003 despawn, :3016 spawn),
		// so a disagreement between them and the record is the bug rather than a symptom of it.
		//
		// SetLifecyclePolicy does NOT clamp - it assigns straight through (SCR_AIGroup.c:2920-2925) - so
		// they SHOULD read back exactly what ApplyLifecyclePolicy passed. "Should" is the reason to print
		// them: nobody had ever confirmed the call reached this group at all.
		string engineRings = string.Format("the engine is holding spawn %1 m / despawn %2 m",
			Math.Round(group.GetSpawnDistance()).ToString(), Math.Round(group.GetDespawnDistance()).ToString());

		string engineObserver = "and NO observer inside the engine's own spawn ring - it will not even ask";
		if (group.GetSpawnDistance() > 0 && group.HasObserverInRange(group.GetSpawnDistance()))
			engineObserver = "and an observer IS inside the engine's own spawn ring";

		string popIn = "nobody inside the pop-in band";
		if (group.HasObserverInRange(VERY_NEAR_BLOCK_M))
			popIn = string.Format("an observer is INSIDE the %1 m pop-in band, which blocks the spawn request outright", VERY_NEAR_BLOCK_M.ToString());

		string refill = "the refill seam still has slots to fill";
		if (group.IsExpandComplete())
			refill = "the refill seam says COMPLETE with nobody in it - this group is a husk and the queue will drop every request";

		return string.Format("%1, dormant counts alive %2 / dead %3, %4, %5 %6, %7, %8; queue: %9",
			dormant, group.GetDormantAliveCount().ToString(), group.GetDormantDeadCount().ToString(),
			observer, engineRings, engineObserver, popIn, refill,
			group.GetOVTSpawnQueueDiagnostic());
	}

	//------------------------------------------------------------------------------------------------
	//! WHETHER THE ENGINE'S AI BUDGET WOULD LET THIS GROUP IN AT ALL.
	//!
	//! ⚠ THIS IS THE CLAUSE THAT SETTLES "the ring is right, the observer is there, and still nobody
	//! spawned". ChimeraAIWorld's spawn queue re-validates at DISPATCH time and orders by importance,
	//! and CanActivateGroup() is the per-tier soft cap it applies - LOW 0.50, NORMAL 0.70, HIGH 0.90,
	//! CRITICAL 1.00 of GetLimitOfActiveAIs() (SCR_EAISpawnImportance). A NORMAL-importance transport
	//! crew registered while a base fight has the budget at 75% is refused, silently, for as long as the
	//! fight lasts - and nothing anywhere logs that.
	//!
	//! ⚠ GetLastTickEvictions() IS READ TOO, because the queue does not only refuse: it EVICTS. A
	//! non-zero number here beside an empty group means somebody is being thrown out to make room, which
	//! is a different conversation from "nobody had room in the first place".
	//!
	//! ⚠ AND THE HONEST CAVEAT, WHICH BELONGS IN THE LOG RATHER THAN IN A COMMENT NOBODY READS AT 2 AM:
	//! pinning crews out of max LOD (see this class's header) keeps their behaviour trees running, and
	//! whether an agent at max LOD counts towards GetCurrentNumOfActiveAIs() is engine-side and not
	//! documented in the script API. If it does not, then every pinned crew is a NEW charge against this
	//! budget that the same campaign did not pay before 2026-08-21. Printing the numbers is how that
	//! gets settled by measurement instead of by argument.
	//! \param[in] group The group whose importance tier should be tested; null asks about the global cap.
	//! \return A compact description of the active-AI budget and this group's standing in it.
	static string DescribeAiBudget(SCR_AIGroup group)
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return "no AI world";

		string allowed = "budget REFUSES this group";
		if (aiWorld.CanActivateGroup(group))
			allowed = "budget allows this group";

		string evictions = "";
		ChimeraAIWorld chimeraWorld = ChimeraAIWorld.Cast(aiWorld);
		if (chimeraWorld)
			evictions = string.Format(", %1 eviction(s) last queue tick", chimeraWorld.GetLastTickEvictions().ToString());

		return string.Format("%1 of %2 active AI, %3%4",
			aiWorld.GetCurrentNumOfActiveAIs().ToString(), aiWorld.GetLimitOfActiveAIs().ToString(),
			allowed, evictions);
	}

	//------------------------------------------------------------------------------------------------
	//! HOW FAR THE NEAREST LIVE PLAYER IS, in metres - the closest script can get to "how far the
	//! nearest OBSERVER is", which is the number the LOD system actually uses.
	//!
	//! ⚠ IT IS A PROXY AND THE DIFFERENCE MATTERS WHEN READING THE LOG. Observers are not players
	//! (virtualization/core Phase 1 T1.2): a Game Master camera, the deploy-point preload observer and
	//! a parked recruit group are all observers with no player body, so a line that says "nearest player
	//! 2400 m" and a group that is nevertheless at a low LOD is not a contradiction. Dead players are
	//! skipped, exactly as OVT_WorldUtils.PlayerInRange skips them.
	//! \param[in] position The place to measure from.
	//! \return Metres to the nearest live player, or -1 when there are none.
	static float NearestPlayerDistance(vector position)
	{
		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return -1;

		array<int> players = {};
		manager.GetPlayers(players);

		float nearest = -1;

		foreach (int playerId : players)
		{
			IEntity player = manager.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			DamageManagerComponent damage = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				continue;

			float distance = vector.Distance(player.GetOrigin(), position);
			if (nearest < 0 || distance < nearest)
				nearest = distance;
		}

		return nearest;
	}
}
