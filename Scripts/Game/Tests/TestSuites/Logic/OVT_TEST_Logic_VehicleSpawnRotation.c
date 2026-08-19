//------------------------------------------------------------------------------------------------
//! TIER A cases - the rotation a deployment hands to a vehicle spawn.
//!
//! WHAT SHIPPED BROKEN, AND WHAT THESE CASES CAN AND CANNOT SEE. Patrol vehicles were arriving at
//! authored base markers standing on their noses with the physics fighting itself. The mechanism was
//! not the value: OVT_VehicleSpawningDeploymentModule spawned the vehicle level and then called
//! Vehicle.SetAngles() on the finished entity, which rotates a live rigid body out from under the
//! solver. Nothing in this tier can see that - it needs a physics body, a world and a frame of
//! simulation - so the FIX is what is pinned here, not the fault. The fix moves the orientation into
//! the spawn transform, and doing that means converting a heading into a rotation vector, which is
//! the one part of the whole path that is pure arithmetic and therefore decidable here.
//!
//! WHY THE CONVERSION IS WORTH A TEST AT ALL. The two engine angle APIs disagree on ordering:
//!
//!   IEntity.GetAngles() / SetAngles()          -> (X = pitch, Y = yaw, Z = roll)
//!   Math3D.AnglesToMatrix() / MatrixToAngles() -> (yaw, pitch, roll)
//!
//! A heading that crosses that boundary in the wrong order becomes a PITCH. Eden's markers run from
//! -179 to +151 degrees of heading, so the wrong order does not produce a subtle lean - it produces
//! the exact symptom that was reported, a vehicle nose-down or inverted. The swap compiles, replicates
//! and saves cleanly, and there is no runtime error to notice. The second case below spells the trap
//! out in numbers so that a later "simplify" of GetUprightSpawnRotation() fails here rather than in a
//! player's screenshot.
//!
//! WHAT IS NOT TESTED HERE, BY CONSTRUCTION RATHER THAN BY OMISSION. The other half of the fix is that
//! OVT_BaseControllerComponent.GetRandomVehiclePatrolSpawn() now answers a float heading instead of an
//! angle vector, so a marker's pitch and roll cannot reach a vehicle at all. That is unrepresentable
//! rather than assertable: there is no longer a value to pass in that could tilt anything. Reading the
//! heading off a placed marker needs a world, so it belongs to the manual verification, not to Tier A.
//!
//! HOW THE EXPECTED MATRIX WAS DERIVED, so the assertions are not folklore. Math3D.AnglesToMatrix's own
//! documented example, AnglesToMatrix("70 15 45") -> mat[2] = <0.907673, 0.258819, 0.330366>, matches
//! mat[2] = (sin(yaw)cos(pitch), sin(pitch), cos(yaw)cos(pitch)) to six figures. With pitch and roll
//! zero that collapses to forward = (sin(yaw), 0, cos(yaw)) and up = (0, 1, 0) exactly, which is what
//! the cases below assert. Same source gives up[1] = cos(pitch)cos(roll), which is what makes the
//! swapped-order case's number checkable rather than merely "not upright".
//!
//! PROVEN ABLE TO FAIL (by deliberate fault + compile-check; running the suite is the orchestrator's
//! job, not this file's):
//!   1. GetUprightSpawnRotation() was changed from Vector(yaw, 0, 0) to Vector(0, yaw, 0) - the exact
//!      mistake of handing an IEntity.GetAngles()-ordered vector to Math3D.AnglesToMatrix(). The tree
//!      recompiled CLEAN, which is the whole point of the case: a swapped axis is not a syntax error
//!      and nothing else in the toolchain notices it. ..._MarkerHeadingSpawnsLevel then fails on its
//!      first non-zero heading, 14.567, whose up axis becomes <0, 0.9679, -0.2515> - the vehicle is
//!      leaning 14.6 degrees - and fails outright at -172.727, where up is <0, -0.9920, 0.1266> and
//!      the vehicle is inverted. Reverted and recompiled clean.
//!   2. The heading was dropped instead of misplaced - Vector(0, 0, 0) - to model "upright but facing
//!      whatever the prefab faces". Also recompiled CLEAN. ..._MarkerHeadingSpawnsLevel passes its
//!      upright check and its horizontal check at every heading and fails the forward-axis check, at
//!      14.567 first, which is why the heading is asserted separately from the level rather than being
//!      assumed to follow from it. Reverted and recompiled clean.
//!   Deliberately NOT claimed: that the assertions were watched going red in a live run. The faults
//!   above were injected and the tree recompiled clean each time (that is the part a compile gate can
//!   answer); which assertion fires is derived from the matrix identities quoted above, not observed.
//!   No maxAttempts anywhere: this is trigonometry over constants and it cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Every marker heading produces a LEVEL transform that still faces the way the marker faced.
//!
//! Both halves matter and they fail independently. "Level" is the shipped bug's symptom: a vehicle
//! whose up axis is not world up is a vehicle on its nose or its roof, whatever else is right about
//! it. "Faces the way the marker faced" is the reason the markers exist at all - an upright vehicle
//! pointing at a wall is a patrol that cannot leave the base.
//!
//! The headings are not invented. 14.567, 151.335, -125.868 and -172.727 are all real values read off
//! the authored OVT_VehiclePatrolSpawn markers in Worlds/MP/OVT_Campaign_Eden_Layers/slots.layer, and
//! -172.727 is additionally the value baked into the marker prefab itself - the single number most
//! likely to be seen on a nose-down vehicle in a bug report.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleSpawnRotation_MarkerHeadingSpawnsLevel : SCR_AutotestCaseBase
{
	//! Unit-vector tolerance. The components are all O(1) and come out of the engine's own trig, so
	//! this is roughly a thousand times the error that is actually possible - loose enough never to
	//! flake, tight enough that a swapped axis (which moves a component by 0.25 or more at the
	//! smallest heading tested) cannot slip through.
	static const float UNIT_EPSILON = 0.001;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (!CheckHeading(0, "a marker facing due north"))
			return true;

		if (!CheckHeading(14.567, "Eden's first authored marker"))
			return true;

		if (!CheckHeading(90, "a marker facing due east"))
			return true;

		if (!CheckHeading(151.335, "an authored marker that also carries 4.7 degrees of terrain pitch"))
			return true;

		if (!CheckHeading(-125.868, "an authored marker with a negative heading"))
			return true;

		if (!CheckHeading(-172.727, "the heading baked into the marker prefab - a nose-stand if it reaches the pitch slot"))
			return true;

		if (!CheckHeading(-179.346, "the most nearly-reversed authored marker"))
			return true;

		Print("Vehicle spawn rotation: every authored heading produces a level transform that keeps its heading");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts the whole contract for one heading.
	//! \param[in] yaw Heading in degrees.
	//! \param[in] description What this heading stands for, quoted back in any failure.
	//! \return False once a failure has been recorded, true while everything holds.
	protected bool CheckHeading(float yaw, string description)
	{
		vector rotation = OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation(yaw);

		vector mat[3];
		Math3D.AnglesToMatrix(rotation, mat);

		// mat[1] is the UP axis. A rotation with no pitch and no roll leaves it exactly on world up,
		// and a vehicle spawned with anything else is the reported bug.
		if (Math.AbsFloat(mat[1][0]) > UNIT_EPSILON || Math.AbsFloat(mat[1][1] - 1) > UNIT_EPSILON || Math.AbsFloat(mat[1][2]) > UNIT_EPSILON)
		{
			SetFailure(string.Format("Spawn rotation for heading %1 is NOT UPRIGHT - up axis %2 instead of world up. A vehicle spawned with this stands on its nose. Heading was %3.", yaw, mat[1], description));
			return false;
		}

		// mat[2] is the FORWARD axis. Horizontal first, because a forward axis with any vertical
		// component means the heading has leaked into the pitch even if the up axis survived.
		if (Math.AbsFloat(mat[2][1]) > UNIT_EPSILON)
		{
			SetFailure(string.Format("Spawn rotation for heading %1 points the forward axis out of the horizontal plane (%2). Heading was %3.", yaw, mat[2], description));
			return false;
		}

		float yawRadians = yaw * Math.DEG2RAD;

		if (Math.AbsFloat(mat[2][0] - Math.Sin(yawRadians)) > UNIT_EPSILON || Math.AbsFloat(mat[2][2] - Math.Cos(yawRadians)) > UNIT_EPSILON)
		{
			SetFailure(string.Format("Spawn rotation for heading %1 is level but faces the wrong way - forward %2. Heading was %3.", yaw, mat[2], description));
			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The angle-order trap, in numbers, next to the conversion that avoids it.
//!
//! This case asserts an ENGINE fact deliberately: that handing an IEntity.GetAngles()-ordered vector
//! straight to Math3D.AnglesToMatrix() tips the result over by the whole heading. It earns its place
//! for two reasons. First, it is the justification for GetUprightSpawnRotation() existing at all - a
//! reader who thinks the helper is a pointless one-line wrapper can see here exactly what the wrapper
//! is standing between. Second, if a future engine build ever reconciled the two orderings, this case
//! going red is how we would find out, rather than discovering it through a behaviour change.
//!
//! The two halves run on the SAME heading so the comparison is exact: one ordering leaves the vehicle
//! upright, the other leaves it 172.7 degrees over, and nothing else differs.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_VehicleSpawnRotation_SwappedAngleOrderTipsTheVehicle : SCR_AutotestCaseBase
{
	static const float UNIT_EPSILON = 0.001;

	//! The marker prefab's own baked heading, and the number in the original report.
	static const float MARKER_YAW = -172.727;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// The wrong order: what a caller gets by passing IEntity.GetAngles() output - (pitch, yaw,
		// roll) - to a function that reads (yaw, pitch, roll). The heading lands in the pitch slot.
		vector swapped = Vector(0, MARKER_YAW, 0);

		vector swappedMat[3];
		Math3D.AnglesToMatrix(swapped, swappedMat);

		// up[1] is cos(pitch) * cos(roll); with the heading in the pitch slot that is cos(-172.727),
		// which is -0.992 - the vehicle is very nearly inverted.
		float expectedTippedUp = Math.Cos(MARKER_YAW * Math.DEG2RAD);

		if (Math.AbsFloat(swappedMat[1][1] - expectedTippedUp) > UNIT_EPSILON)
		{
			SetFailure(string.Format("The angle-order trap no longer behaves as documented: a heading in the pitch slot gave up axis %1, expected vertical component %2. The comment on GetUprightSpawnRotation() needs re-checking against the engine.", swappedMat[1], expectedTippedUp));
			return true;
		}

		if (swappedMat[1][1] > 0)
		{
			SetFailure(string.Format("Expected a heading in the pitch slot to leave the vehicle past vertical, but its up axis still points up (%1)", swappedMat[1]));
			return true;
		}

		// The right order, same heading, through the conversion the spawn path actually uses.
		vector correctMat[3];
		Math3D.AnglesToMatrix(OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation(MARKER_YAW), correctMat);

		if (Math.AbsFloat(correctMat[1][0]) > UNIT_EPSILON || Math.AbsFloat(correctMat[1][1] - 1) > UNIT_EPSILON || Math.AbsFloat(correctMat[1][2]) > UNIT_EPSILON)
		{
			SetFailure(string.Format("GetUprightSpawnRotation() has fallen into the very trap it exists to avoid - heading %1 produced up axis %2 instead of world up", MARKER_YAW, correctMat[1]));
			return true;
		}

		Print("Vehicle spawn rotation: the swapped angle order inverts the vehicle, and the spawn path's conversion does not");

		return true;
	}
}
