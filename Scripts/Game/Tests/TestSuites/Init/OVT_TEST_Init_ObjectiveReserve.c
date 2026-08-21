//------------------------------------------------------------------------------------------------
//! TIER B - the objective RESERVE FLOOR where it meets live components: absent by default, pushed by
//! the objective machine only while it is genuinely short of money, and gone again the moment it
//! stops asking.
//!
//! WHAT THIS TIER CAN SEE THAT THE CHEAP ONE CANNOT. The budget arithmetic is a pure function of two
//! integers and is pinned in OVT_TEST_Logic_ObjectiveReserveFloor.c. Three things about the floor are
//! not arithmetic and can only be asserted against real components:
//!
//!   1. THAT NO FACTION IN A RUNNING WORLD IS FLOORED unless something deliberately floors it. This
//!      is the whole "with no objective the evaluator behaves exactly as it did before" claim reduced
//!      to something a case can state: the evaluation pass resolves a reserve per faction per pass,
//!      and if every one of those lookups answers nothing then every faction's budget is the pool it
//!      always was.
//!   2. THAT THE OBJECTIVE MACHINE AND THE DEPLOYMENT FRAMEWORK AGREE. The machine pushes at the
//!      occupying faction's index and the evaluator reads at whichever index it is evaluating; a
//!      mismatch there is silent, permanent, and looks exactly like a feature that does nothing.
//!   3. 🔴 THAT THE FLOOR LAPSES WITHOUT A TEARDOWN. This is the highest-value assertion in the file
//!      and it is the one a reviewer would not think to ask for. A floor is only correct while the
//!      machine still intends to send the thing it is saving up for; a floor that persists after it
//!      stops asking is a permanent tax on routine garrisoning, with no error, no failed deployment
//!      and no symptom but a faction that quietly stops reinforcing itself. The case below drives the
//!      machine into a block, confirms the floor, then takes ONE MORE TICK on which the cadence has
//!      not elapsed - no reset, no teardown, no phase change, the objective still live - and requires
//!      the floor to be gone.
//!
//! ⚠ THE EVALUATOR IS NOT DRIVEN, AND THAT IS DELIBERATE. EvaluateFactionDeployments() creates real
//! deployments and debits a real pool; driving it to observe a budget would mutate the shared
//! initialisation world for every case that follows and could not be made deterministic. The seam it
//! reads is asserted directly instead - which is why GetObjectiveReserveCost() is public, on the
//! precedent GetObjectiveAnchor() set in the same file.
//!
//! ⚠ NOTHING HERE IS SCOPED TO A GLOBAL COUNT. The initialisation and persistence worlds run a live
//! deployment wave; every assertion below is about one synthetic faction index this file sets and
//! clears itself, or about the occupying faction over a handful of synchronous calls with its pool
//! saved and restored.
//!
//! ⚠ CASE ORDER MATTERS AND THE NAMES ARE CHOSEN FOR IT. Cases run alphabetically by class name.
//! `...AbsentForEveryFactionByDefault` asserts a pristine store, so it must run before the case that
//! drives the machine into pushing one - A sorts before D. The driving case restores anyway, so the
//! ordering is belt and braces rather than either alone.
//!
//! No polling, no waiting, no maxAttempts: every subject is either a hand-built value or a single
//! synchronous call, and the one tick that is taken is taken deliberately rather than waited for.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! No faction in this world is floored, so the routine evaluator spends the pool it always spent -
//! and the store round-trips, refuses a dead floor, latches its announcement and clears.
//!
//! THE FIRST HALF IS THE REGRESSION PROPERTY, STATED THE ONLY WAY IT CAN BE. "The evaluator is
//! unchanged with no reserve" is not directly observable without driving a pass that spends real
//! money, but it decomposes into two things that are: the budget function is the pool when nothing is
//! reserved (pinned in the cheap tier, with an exact `==`), and every faction lookup in a world
//! nobody has floored answers NO_RESERVE (pinned here). Together those say the evaluation pass
//! computes exactly the integer it computed before D18.
//!
//! THE SECOND HALF EXISTS SO THE FIRST CANNOT PASS VACUOUSLY. A store that had been renamed, never
//! allocated, or wired to always answer nothing would satisfy the first half perfectly. So the case
//! then sets a floor, reads it back, re-prices it, refuses a dead one, exercises the announcement
//! latch, clears a faction that was never floored - and puts the store back the way it found it.
//!
//! ⚠ THE LATCH ROWS ARE NOT DECORATION. An unchanged re-push MUST keep the existing record rather
//! than replacing it, because the owner of a floor re-pushes on every one of its ticks - six times a
//! real minute at 6x - and a record replaced each time would re-arm the `announced` flag each time
//! and turn one explanatory line into an unbounded log flood. That is a property of SetObjectiveReserve(),
//! not of the pure key predicate, so it can only be asserted here.
//!
//! ⚠ THE FIXTURE FACTION INDEX IS ONE NO FACTION HOLDS, deliberately. The evaluator only ever looks
//! up indices it got from the faction manager, so a synthetic index cannot reach the live wave even
//! for the few microseconds this case holds it.
//!
//! PROVEN ABLE TO FAIL: SetObjectiveReserve() was changed to always allocate a fresh record instead of
//! keeping an unchanged one. The tree recompiled CLEAN (tools/compile-check.sh exit 0) - a re-armed
//! log latch is not a script error, and nothing in the campaign reports it - and the case then reports
//! "an unchanged re-push must keep the existing record, or the explanatory line is re-armed six times
//! a real minute for as long as the faction is saving up". Line restored, tree recompiled clean.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveReserve_AbsentForEveryFactionByDefault : SCR_AutotestCaseBase
{
	//! A fixture operation name. Never resolved as a config - the store treats it as an opaque string.
	static const string FIXTURE_OPERATION = "Reserve Fixture Operation";

	//! A fixture price, and a re-price. Both non-round so a fault returning a constant cannot match.
	static const int FIXTURE_COST = 137;
	static const int FIXTURE_REPRICE = 311;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			SetFailure("OVT_Global.GetDeploymentManager() is null - the deployment framework did not resolve");
			return true;
		}

		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
		{
			SetFailure("The faction manager did not resolve, so 'no faction is floored' could not be asked of anybody");
			return true;
		}

		array<Faction> factions = new array<Faction>();
		factionManager.GetFactionsList(factions);

		if (factions.IsEmpty())
		{
			SetFailure("The world has no factions at all, so the absent-by-default claim would pass vacuously");
			return true;
		}

		// --- HALF ONE: nobody is floored. Every index the evaluator could ever look up answers
		//     NO_RESERVE, and the budget it would compute from that is the pool itself.
		int highestIndex = -1;
		foreach (Faction faction : factions)
		{
			int factionIndex = factionManager.GetFactionIndex(faction);
			if (factionIndex > highestIndex)
				highestIndex = factionIndex;

			int reserve = deployments.GetObjectiveReserveCost(factionIndex);
			if (reserve != OVT_DeploymentSelection.NO_RESERVE)
			{
				SetFailure(string.Format("faction %1 has %2 reserved in a world where nothing has reserved anything - the evaluator would not be spending what it spent before the reserve floor existed",
					factionIndex.ToString(), reserve.ToString()));
				return true;
			}

			int pool = deployments.GetFactionResources(factionIndex);
			if (OVT_DeploymentSelection.SpendableResources(pool, reserve) != pool)
			{
				SetFailure(string.Format("faction %1 holds %2 and may spend %3 of it - with nothing reserved those must be the same integer",
					factionIndex.ToString(), pool.ToString(), OVT_DeploymentSelection.SpendableResources(pool, reserve).ToString()));
				return true;
			}
		}

		// --- HALF TWO: the store works, so half one is a fact about this world and not about a store
		//     that never answers anything. An index no faction holds, so the live wave cannot see it.
		int fixtureIndex = highestIndex + 100;

		deployments.SetObjectiveReserve(fixtureIndex, FIXTURE_OPERATION, FIXTURE_COST);

		OVT_DeploymentObjectiveReserve stored = deployments.GetObjectiveReserve(fixtureIndex);
		if (!stored)
		{
			SetFailure(string.Format("the store must hand back the reserve it was given for the faction it was given it for, but faction %1 reads back unfloored",
				fixtureIndex.ToString()));
			return true;
		}

		if (stored.cost != FIXTURE_COST || stored.operation != FIXTURE_OPERATION)
		{
			SetFailure(string.Format("the stored reserve reads '%1' at %2, set as '%3' at %4",
				stored.operation, stored.cost.ToString(), FIXTURE_OPERATION, FIXTURE_COST.ToString()));
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		if (deployments.GetObjectiveReserveCost(fixtureIndex) != FIXTURE_COST)
		{
			SetFailure("the integer accessor the evaluator actually uses must agree with the record it is reading");
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		// --- THE LATCH. An unchanged re-push keeps the record, so the announcement stays said.
		stored.announced = true;
		deployments.SetObjectiveReserve(fixtureIndex, FIXTURE_OPERATION, FIXTURE_COST);

		OVT_DeploymentObjectiveReserve afterRepush = deployments.GetObjectiveReserve(fixtureIndex);
		if (!afterRepush || !afterRepush.announced)
		{
			SetFailure("an unchanged re-push must keep the existing record, or the explanatory line is re-armed six times a real minute for as long as the faction is saving up");
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		// --- ...and a re-price is NEWS, so it replaces the record and un-latches the line.
		deployments.SetObjectiveReserve(fixtureIndex, FIXTURE_OPERATION, FIXTURE_REPRICE);

		OVT_DeploymentObjectiveReserve afterReprice = deployments.GetObjectiveReserve(fixtureIndex);
		if (!afterReprice || afterReprice.cost != FIXTURE_REPRICE)
		{
			SetFailure("re-pricing a reserve must REPLACE it rather than being ignored - a faction saving up for a different amount is saving up for a different thing");
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		if (afterReprice.announced)
		{
			SetFailure("a re-priced reserve must be announced again - a latch that survived the re-price would hide the new figure behind the old one");
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		// --- A dead reserve is refused rather than stored, so "is there a reserve?" is the whole test
		//     at the call site and a zero can never hold anybody off.
		deployments.SetObjectiveReserve(fixtureIndex, FIXTURE_OPERATION, 0);
		if (deployments.GetObjectiveReserve(fixtureIndex))
		{
			SetFailure("a reserve of nothing must be refused rather than stored, or the evaluator carries a floor that withholds nothing and explains itself anyway");
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		deployments.SetObjectiveReserve(fixtureIndex, FIXTURE_OPERATION, -50);
		if (deployments.GetObjectiveReserve(fixtureIndex))
		{
			SetFailure("a negative reserve must be refused rather than stored - it would read as a bonus rather than as a floor");
			deployments.ClearObjectiveReserve(fixtureIndex);
			return true;
		}

		// --- And clearing is safe on a faction nobody floored, which every teardown path relies on.
		deployments.SetObjectiveReserve(fixtureIndex, FIXTURE_OPERATION, FIXTURE_COST);
		deployments.ClearObjectiveReserve(fixtureIndex);
		deployments.ClearObjectiveReserve(fixtureIndex);

		if (deployments.GetObjectiveReserve(fixtureIndex))
		{
			SetFailure(string.Format("clearing a reserve must remove it - faction %1 is still floored after being cleared twice",
				fixtureIndex.ToString()));
			return true;
		}

		if (deployments.GetObjectiveReserveCost(fixtureIndex) != OVT_DeploymentSelection.NO_RESERVE)
		{
			SetFailure("a cleared faction must read back as NO_RESERVE through the integer accessor as well as through the record");
			return true;
		}

		Print("Objective reserve floor: no faction in this world is floored, so every evaluation pass spends the pool it always spent - and the store sets, re-prices, latches its announcement, refuses a dead floor and clears");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The objective machine floors the pool ONLY while it is genuinely short of money for an operation
//! it has just asked for, and the floor lapses on the very next tick that does not ask.
//!
//! 🔴 WHY THE LAPSE IS THE HEADLINE. A reserve floor that outlives the intent is worse than no floor
//! at all: it holds routine garrisoning off indefinitely for an operation nobody is sending, silently,
//! for the rest of the campaign. The mechanism that prevents it is an ordering - the tick DROPS the
//! floor on its first line and only an ask that is refused for money re-pushes it - and an ordering is
//! exactly the kind of thing a refactor reverses without noticing. So the case drives the machine into
//! a real block, confirms a real floor, and then takes one more tick with the operation cadence planted
//! HIGH so that nothing can ask. No reset, no teardown, no phase change, the objective still live: the
//! floor must simply be gone.
//!
//! HOW THE BLOCK IS ARRANGED, AND WHY IT SPENDS NOTHING. The occupying faction's pool is emptied for
//! the duration of the case. Every sender the harassment phase can reach then refuses on cost before
//! it creates anything, which is the same path a poor campaign takes and the reason this fixture is
//! safe in a shared world: a refused create makes no deployment, no truck, no group and no debit. The
//! pool is put back before anything is asserted.
//!
//! ⚠ THE OPERATION NAME IS NOT HARD-CODED, AND THAT IS DELIBERATE. Which sender answers first depends
//! on the world - a radio tower in range makes it the recapture team, otherwise it is the harassment
//! ladder's first rung - and pinning one would make this case a statement about map authoring. What IS
//! asserted is stronger and world-independent: the floor names a config the deployment registry knows,
//! and its price is exactly that config's total resource cost. That is the claim that matters - the
//! floor is ONE OPERATION DEEP and it is the operation the machine actually asked for, not a
//! percentage, not a running total and not a guess.
//!
//! ⚠ THE CADENCE IS PLANTED AT 1 FOR THE FIRST TICK AND HIGH FOR THE SECOND. A tick decrements the
//! countdown before it reads it, so 1 becomes 0 and the senders run; a high value stays above zero and
//! they do not. Committing an objective arms the countdown to ZERO, so a tick taken straight afterwards
//! would reach the spend path - which is safe here only because the pool is empty, and which every
//! other case that drives this machine avoids by planting a countdown. Both halves of this case plant.
//!
//! ⚠ EVERYTHING IS PUT BACK. The initialisation world is shared, and the machine is left with no
//! objective, no floor and its original pool whatever this case does - including on every failure path,
//! which is why the teardown runs before the assertions rather than after them.
//!
//! PROVEN ABLE TO FAIL, two faults, injected one at a time and compiled; both exited
//! tools/compile-check.sh 0, and the subject was restored and re-compiled clean afterwards:
//!   R1. THE PUSH REMOVED - delete the PushObjectiveReserve() call from
//!       OVT_ObjectiveDirectorComponent.CanSendObjectiveDeployment()'s pool-short branch. Compiled
//!       clean (a floor nobody pushes is not a script error, and the campaign logs nothing about it).
//!       The case then reports "an objective machine that has just been refused an operation for want
//!       of resources must floor the pool, but the occupying faction reads back unfloored".
//!   R2. THE LAPSE REMOVED - delete the DropObjectiveReserve() call from the first line of
//!       DirectorTick(). Compiled clean, every other case in the tree stays green, and a campaign
//!       would run for hours before anybody noticed routine garrisoning had stopped. The case then
//!       reports "the floor must lapse on the first tick that does not ask for anything".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_ObjectiveReserve_DirectorFloorsOnlyWhileBrokeAndLapsesWhenItStopsAsking : SCR_AutotestCaseBase
{
	//! Where the fixture objective is. Deliberately nowhere near anything: with the pool emptied
	//! nothing can be created there, and the position is only ever read as three numbers.
	static const vector FIXTURE_POSITION = "12000 0 12000";

	//! Planted phase timeout, high enough that no drive here can run it out. Not a value any phase
	//! entry produces, so a re-arm would be visible if a later case ever looked.
	static const int PLANTED_PHASE_TICKS = 199;

	//! Planted operation cadence for the tick that MUST ask. One tick-down lands it on zero.
	static const int CADENCE_ELAPSES = 1;

	//! Planted operation cadence for the tick that must NOT ask. See the class header.
	static const int CADENCE_PENDING = 44;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!director || !deployments || !config)
		{
			SetFailure("The objective machine, the deployment framework or the config did not resolve");
			return true;
		}

		int occupyingIndex = config.GetOccupyingFactionIndex();
		if (occupyingIndex < 0)
		{
			SetFailure("This world cannot resolve its own occupying faction index, so a floor pushed at the right index and one pushed at no index would look identical");
			return true;
		}

		if (deployments.GetObjectiveReserve(occupyingIndex))
		{
			SetFailure(string.Format("faction %1 is already floored before this case has done anything - a case that ran earlier did not put the store back, and the assertions below would be about its reserve rather than this one's",
				occupyingIndex.ToString()));
			return true;
		}

		// --- ARRANGE: empty the pool, so every sender is refused on cost and nothing is created. The
		//     live map is written directly rather than through the credit and debit methods, which are
		//     the campaign's own accounting and are not a fixture's to call.
		map<int, int> pools = deployments.GetAllFactionResources();
		if (!pools)
		{
			SetFailure("The deployment framework has no resource map, so the pool could not be emptied and the machine could not be made short of money");
			return true;
		}

		int savedPool = pools.Get(occupyingIndex);
		pools.Set(occupyingIndex, 0);

		// --- ACT, HALF ONE: an objective, a cadence that elapses on the next tick, and a tick.
		director.CommitObjective(OVT_EObjectiveKind.TOWN, FIXTURE_POSITION, "reserve fixture");
		director.SetPhaseTimeout(PLANTED_PHASE_TICKS);
		director.SetOperationCountdown(CADENCE_ELAPSES);

		director.DirectorTick();

		OVT_DeploymentObjectiveReserve floored = CopyReserve(deployments.GetObjectiveReserve(occupyingIndex));

		// --- ACT, HALF TWO: the cadence is planted high, so this tick cannot ask for anything. Nothing
		//     else changes - the objective is still live, still in the same phase, still at the same
		//     place, and the faction is still just as poor.
		director.SetOperationCountdown(CADENCE_PENDING);

		director.DirectorTick();

		bool lapsed = deployments.GetObjectiveReserve(occupyingIndex) == null;
		string phaseAfterLapse = director.GetObjectivePhaseName();

		// --- ACT, HALF THREE: floored again, and then torn down. Every runtime path that ends an
		//     objective funnels through the record clear, which drops the floor in the same breath as
		//     the deployment bias.
		director.SetOperationCountdown(CADENCE_ELAPSES);
		director.DirectorTick();

		bool flooredAgain = deployments.GetObjectiveReserve(occupyingIndex) != null;

		director.ResetObjective("initialisation-tier reserve fixture torn down", false);

		bool clearedByReset = deployments.GetObjectiveReserve(occupyingIndex) == null;

		// --- RESTORE, BEFORE ASSERTING. The world is shared and a SetFailure returns immediately.
		pools.Set(occupyingIndex, savedPool);
		director.ResetObjective("initialisation-tier reserve fixture torn down", false);
		deployments.ClearObjectiveReserve(occupyingIndex);

		// --- ASSERT: the push happened, at the right index, for a real operation at its real price.
		if (!floored)
		{
			SetFailure(string.Format("an objective machine that has just been refused an operation for want of resources must floor the pool, but the occupying faction (%1) reads back unfloored - routine garrisoning would drain the next credit before the operation could ever be afforded",
				occupyingIndex.ToString()));
			return true;
		}

		if (floored.cost <= 0)
		{
			SetFailure(string.Format("a floor must withhold something: the reserve reads %1",
				floored.cost.ToString()));
			return true;
		}

		if (floored.operation == "")
		{
			SetFailure("a floor must name what it is being held for, or the log line that explains a faction spending less than it holds cannot say what it is saving up for");
			return true;
		}

		OVT_DeploymentConfig named = deployments.FindConfigByName(floored.operation);
		if (!named)
		{
			SetFailure(string.Format("the floor names '%1', which the deployment registry does not know - the machine must reserve for an operation it actually asked for, not for a label",
				floored.operation));
			return true;
		}

		if (floored.cost != named.GetTotalResourceCost())
		{
			SetFailure(string.Format("the floor must be ONE OPERATION DEEP - the price of '%1' is %2 and the reserve is %3. A reserve that is not exactly the next operation's cost is a war chest",
				floored.operation, named.GetTotalResourceCost().ToString(), floored.cost.ToString()));
			return true;
		}

		// --- ASSERT: and it lapses the moment the machine stops asking, with nothing torn down.
		if (!lapsed)
		{
			SetFailure("the floor must lapse on the first tick that does not ask for anything - the objective was still live, still in the same phase and the faction still just as poor, and a floor that survives that is a permanent tax on routine spending for an operation nobody is sending");
			return true;
		}

		if (phaseAfterLapse != "Harassment")
		{
			SetFailure(string.Format("the lapse must be a lapse and not a teardown: the objective left the harassment phase (now %1), so the claim above may be about an objective that ended rather than about a tick that did not ask",
				phaseAfterLapse));
			return true;
		}

		if (!flooredAgain)
		{
			SetFailure("a machine that is still broke must floor the pool again on the next tick that asks - otherwise the lapse above is a floor that can never come back, and the operation is never saved up for at all");
			return true;
		}

		if (!clearedByReset)
		{
			SetFailure("ending the objective must drop the floor, or routine spending stays held off for an operation that has been abandoned");
			return true;
		}

		Print("Objective reserve floor: the machine floors the pool by exactly the price of the operation it was just refused, floors it again for as long as it stays broke, and gives the whole pool back on the first tick that does not ask and on the teardown");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Snapshots a reserve so it can be asserted after the world has been put back.
	//! \param[in] source The live reserve, or null.
	//! \return A detached copy, or null when there was nothing to copy.
	protected OVT_DeploymentObjectiveReserve CopyReserve(OVT_DeploymentObjectiveReserve source)
	{
		if (!source)
			return null;

		OVT_DeploymentObjectiveReserve copy = new OVT_DeploymentObjectiveReserve();
		copy.operation = source.operation;
		copy.cost = source.cost;
		copy.announced = source.announced;

		return copy;
	}
}
