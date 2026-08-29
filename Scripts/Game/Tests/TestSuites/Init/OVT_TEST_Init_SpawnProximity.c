//------------------------------------------------------------------------------------------------
//! THE NO-SPAWN-NEAR-RESISTANCE GATE (2026-08-29).
//!
//! ==========================================================================================
//! 🔴 WHY THIS FILE EXISTS. "There are still enemy spawning in front of the players eyes, we've put
//! in so many fixes for this but none of them are working." (author, 2026-08-29.)
//! ==========================================================================================
//! Nothing in the suite covered the gate at all, and that is the whole story of the bug it was
//! written for. The rule had been implemented four separate times - a creation condition module, a
//! per-post filter, a rebuy radius, a contact cooldown - and every one of them was correct in
//! isolation while men kept appearing, because the path that actually registers a shortfall
//! (OVT_InfantrySpawningDeploymentModule.ConvergeGroups) had no gate on it and no case looking at
//! it. A previous session found that exact line, wrote a comment naming it, and left a LOG LINE
//! instead of a refusal.
//!
//! So these cases pin the two things a fifth silent failure would need:
//!   A. the PREDICATE actually answers "held" for a player standing there - the fail-closed fix,
//!      which is what made the other four gates leak;
//!   B. the SEAM and the EXEMPTION behave, so the gate cannot be quietly turned off by a test that
//!      forgets to disarm, or turned ON for a riding crew that must exist to drive its truck.
//!
//! ⚠ WHAT THESE CASES DO NOT PROVE, stated rather than hidden. None of them drives a real
//! convergence: ConvergeGroups() is reached from a live deployment's activation, which is a whole
//! update interval away and behind the post-join grace period, and no Init-tier fixture in this tree
//! spans one. What is asserted is the DECISION - the radius each module resolves and the predicate it
//! resolves it against - which is where all four previous failures lived. A case that watches men
//! actually fail to appear needs a play-test.
//!
//! FIXTURE DISCIPLINE (inherited from OVT_TEST_Init_HunterKillerSweep.c): the seam is a STATIC, so
//! every case that arms it restores it on EVERY exit path including each failure, and never leaves a
//! campaign with its spawn gate disabled.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Exposes the protected radius resolver. A subclass is the only way to read a protected seam, and
//! reading the DECISION is the point - m_fNoSpawnNearResistance alone says nothing about whether the
//! exemption or the override got a look at it.
class OVT_TEST_SpawnProximityInfantryProbe : OVT_InfantrySpawningDeploymentModule
{
	float ProbeResolvedRadius()
	{
		return ResolveNoSpawnNearResistance();
	}
}

//------------------------------------------------------------------------------------------------
//! The same probe on the insertion side of the hierarchy, where the gate must never apply.
class OVT_TEST_SpawnProximityInsertionProbe : OVT_InsertionSpawningDeploymentModule
{
	float ProbeResolvedRadius()
	{
		return ResolveNoSpawnNearResistance();
	}
}

//------------------------------------------------------------------------------------------------
//! CLAIM A: the predicate the gate is built on sees a player standing on the position.
//!
//! 🔴 THE ONE ASSERTION THAT WOULD HAVE CAUGHT THE ORIGINAL BUG. OVT_ResistancePresence.IsGroundHeld
//! is a sphere query, and the query has a reproduced blind spot - it has answered "nobody" with a
//! real player-controlled character on the search centre. The creation gate carried a private
//! PlayerManager fallback for that from 2026-08-25; every OTHER caller inherited the blind spot,
//! which is why the rebuy gate passed while a garrison rebuilt itself in somebody's face. The
//! fallback now lives in the primitive, and this case is what stops it being "simplified" back out.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_SpawnProximity_GroundHeldSeesAPlayer : SCR_AutotestCaseBase
{
	//! The shipped gate radius.
	static const float GATE_RADIUS = 500;

	//! Well outside GATE_RADIUS in any world this suite runs in.
	static const float FAR_AWAY = 5000;

	//! Frame polls allowed for the local player to have a character.
	static const int MAX_POLLS = 300;

	protected int m_iPolls;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);

		IEntity body;
		if (!players.IsEmpty())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(players[0]);

		if (!body)
		{
			m_iPolls += 1;
			if (m_iPolls > MAX_POLLS)
			{
				SetFailure("No player-controlled character appeared in %1 polls, so 'the resistance is standing here' could not be put to the predicate at all", m_iPolls.ToString());
				return true;
			}

			return false;
		}

		vector playerPos = OVT_WorldUtils.GetWorldOrigin(body);

		if (!OVT_ResistancePresence.IsGroundHeld(playerPos, GATE_RADIUS))
		{
			SetFailure("IsGroundHeld() answered NOBODY with a player-controlled character standing on the search centre (radius %1 m). This is the exact false negative that let four separate proximity gates pass while men materialised in front of the player - the PlayerManager fallback inside IsGroundHeld has been removed or broken",
				GATE_RADIUS.ToString());
			return true;
		}

		if (OVT_ResistancePresence.GetLastHold().IsEmpty())
		{
			SetFailure("IsGroundHeld() answered HELD but named nobody. A gate that refuses without saying which character held the ground, and how far out, is the dead end this whole investigation was stuck in for three play-test rounds");
			return true;
		}

		vector farPos = playerPos + Vector(FAR_AWAY, 0, FAR_AWAY);
		if (OVT_ResistancePresence.IsGroundHeld(farPos, GATE_RADIUS))
		{
			SetFailure("IsGroundHeld() answered HELD %1 m from the nearest resistance. A predicate that is always true means no deployment ever converges anywhere and the occupying faction quietly stops existing",
				FAR_AWAY.ToString());
			return true;
		}

		Print("[Overthrow.TEST] IsGroundHeld sees a player on the position and nobody 5 km away, and names what held it");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! CLAIM B: the test seam overrides the authored radius in BOTH directions, and hands control back.
//!
//! ⚠ A RADIUS OVERRIDE, NOT AN ON/OFF SWITCH, and the "gate fires" direction is the half that
//! matters: a seam that could only DISABLE the gate would let a case prove men spawn and never prove
//! they are refused.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnProximity_SeamOverridesBothWays : SCR_AutotestCaseBase
{
	//! Authored on the probe by hand - attribute defvalues do not apply to `new`.
	static const float AUTHORED = 500;

	//! Neutralises the gate.
	static const float NEUTRALISED = 0;

	//! A value no default could be mistaken for, and larger than the authored one.
	static const float WIDENED = 913;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TEST_SpawnProximityInfantryProbe probe = new OVT_TEST_SpawnProximityInfantryProbe();
		probe.m_fNoSpawnNearResistance = AUTHORED;

		// UNARMED: the authored value wins.
		OVT_InfantrySpawningDeploymentModule.s_fTestNoSpawnRadiusOverride = -1;
		if (probe.ProbeResolvedRadius() != AUTHORED)
		{
			SetFailure("With the seam disarmed the module resolved %1 m, expected the authored %2 - the seam is leaking into normal play, which means every campaign is running with an overridden spawn gate",
				probe.ProbeResolvedRadius().ToString(), AUTHORED.ToString());
			return true;
		}

		// NEUTRALISED: a case can let men through deliberately.
		OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(NEUTRALISED);
		if (probe.ProbeResolvedRadius() != NEUTRALISED)
		{
			OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(-1);
			SetFailure("An armed seam of 0 resolved %1 m - a fixture cannot neutralise the gate, so every case that needs a deployment to place men in the small autotest world fails for a reason unrelated to what it tests",
				probe.ProbeResolvedRadius().ToString());
			return true;
		}

		// WIDENED: a case can prove the gate REFUSING.
		OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(WIDENED);
		if (probe.ProbeResolvedRadius() != WIDENED)
		{
			OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(-1);
			SetFailure("An armed seam of %1 m resolved %2 - the seam cannot widen the gate, so no case can ever assert that it FIRES",
				WIDENED.ToString(), probe.ProbeResolvedRadius().ToString());
			return true;
		}

		// DISARMED: control goes back, or a leaked seam disables the gate for the rest of the session.
		OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(-1);
		if (probe.ProbeResolvedRadius() != AUTHORED)
		{
			SetFailure("After disarming, the module resolved %1 m instead of the authored %2 - the seam cannot be handed back, and a single case arming it would disable the spawn gate for every campaign that follows it",
				probe.ProbeResolvedRadius().ToString(), AUTHORED.ToString());
			return true;
		}

		Print("[Overthrow.TEST] The spawn-gate test seam neutralises, widens and hands control back");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! CLAIM C: an insertion is exempt STRUCTURALLY, and the seam cannot override that.
//!
//! 🔴 WHY THE EXEMPTION IS NOT A RADIUS OF ZERO. An insertion's crew and passengers register at
//! RIDING_SPAWN_DISTANCE because they must physically be sitting in a truck that may be driving
//! across the map. Refusing that registration does not defer a pop-in - it stops the convoy forming,
//! leaving a hull nobody crews and nobody owns. That is a different question from "how close may men
//! appear", so it is answered by AppliesNoSpawnNearResistanceGate() and not by a number a fixture
//! could accidentally change.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnProximity_InsertionIsStructurallyExempt : SCR_AutotestCaseBase
{
	static const float AUTHORED = 500;
	static const float WIDENED = 913;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TEST_SpawnProximityInsertionProbe probe = new OVT_TEST_SpawnProximityInsertionProbe();
		probe.m_fNoSpawnNearResistance = AUTHORED;

		OVT_InfantrySpawningDeploymentModule.s_fTestNoSpawnRadiusOverride = -1;
		if (probe.ProbeResolvedRadius() != 0)
		{
			SetFailure("An insertion module resolved a gate radius of %1 m with %2 authored, expected 0. Its crew registers at RIDING_SPAWN_DISTANCE and must exist to drive the truck - a refusal here leaves an uncrewed, unowned hull rather than deferring anything",
				probe.ProbeResolvedRadius().ToString(), AUTHORED.ToString());
			return true;
		}

		// AND THE SEAM MAY NOT REACH IT. A test that wants a crew registered should not have to know a
		// radius exists at all.
		OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(WIDENED);
		float underSeam = probe.ProbeResolvedRadius();
		OVT_InfantrySpawningDeploymentModule.SetTestNoSpawnRadiusOverride(-1);

		if (underSeam != 0)
		{
			SetFailure("An armed seam of %1 m reached an insertion module, which resolved %2 - the structural exemption is being decided by a radius, so a fixture could stop a convoy forming without meaning to",
				WIDENED.ToString(), underSeam.ToString());
			return true;
		}

		Print("[Overthrow.TEST] An insertion module is exempt from the spawn gate, and an armed seam cannot reach it");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! CLAIM D: every infantry subclass carries the gate radius through CloneModule().
//!
//! ⚠ THE TRAP THIS PINS HAS FIRED BEFORE, TWICE. Every deployment gets a CLONE of its config's
//! modules and CloneModule is copy-by-hand, so a forgotten field ships the class default - which for
//! a float is ZERO, which DISABLES this gate silently on every config in the game. That is exactly
//! how m_fMaxCruiseSpeed was lost on the vehicle module for a whole release.
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_SpawnProximity_EverySubclassClonesTheRadius : SCR_AutotestCaseBase
{
	//! Nothing a `new` could produce.
	static const float PROBE = 617;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_InfantrySpawningDeploymentModule plain = new OVT_InfantrySpawningDeploymentModule();
		plain.m_fNoSpawnNearResistance = PROBE;
		if (!CheckClone(plain.CloneModule(), "OVT_InfantrySpawningDeploymentModule"))
			return true;

		OVT_PlacedInfantrySpawningDeploymentModule placed = new OVT_PlacedInfantrySpawningDeploymentModule();
		placed.m_fNoSpawnNearResistance = PROBE;
		if (!CheckClone(placed.CloneModule(), "OVT_PlacedInfantrySpawningDeploymentModule"))
			return true;

		OVT_CompositionSpawningDeploymentModule composition = new OVT_CompositionSpawningDeploymentModule();
		composition.m_fNoSpawnNearResistance = PROBE;
		if (!CheckClone(composition.CloneModule(), "OVT_CompositionSpawningDeploymentModule"))
			return true;

		Print("[Overthrow.TEST] The gate radius survives CloneModule on the plain, placed and composition infantry modules");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] cloned What CloneModule() answered.
	//! \param[in] className The subclass being checked, for the failure text.
	//! \return True when the clone carries the probe value.
	protected bool CheckClone(OVT_BaseDeploymentModule cloned, string className)
	{
		OVT_InfantrySpawningDeploymentModule clone = OVT_InfantrySpawningDeploymentModule.Cast(cloned);
		if (!clone)
		{
			SetFailure("%1.CloneModule() did not answer an infantry module at all", className);
			return false;
		}

		if (clone.m_fNoSpawnNearResistance != PROBE)
		{
			SetFailure("%1 cloned a gate radius of %2, expected the authored %3. A zero here DISABLES the no-spawn-near-resistance gate on every deployment built from that config, silently",
				className, clone.m_fNoSpawnNearResistance.ToString(), PROBE.ToString());
			return false;
		}

		return true;
	}
}
