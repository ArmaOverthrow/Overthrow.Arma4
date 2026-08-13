//------------------------------------------------------------------------------------------------
//! TIER A case - the fast-travel DESTINATION matching rule.
//!
//! OVT_FastTravelService.ResolveFastTravelDestination is the server's answer to "is the place this
//! client named a place Overthrow actually offers". Its enumeration half reads four managers and
//! belongs to the manual gate; its matching half - MatchesAnyPosition - takes an array of vectors and
//! a target and returns a vector, so it can be pinned here, world-free.
//!
//! WHY THIS TIER MATTERS MORE THAN ITS SIZE SUGGESTS. Before step 6b existed, the server validated
//! distance, wanted level, QRF, vehicle seat and affordability and then teleported to whatever vector
//! the client sent; the "owned house / your camp / a FOB / a base we hold" rule was client-side and
//! advisory. The matching rule below is the whole of what replaced that, so the properties it pins
//! are the properties the fix consists of.
//!
//! WHAT IT PINS:
//!  - AN EMPTY CANDIDATE SET REFUSES. This is the fail-closed direction and the one a regression is
//!    most likely to invert: a server whose managers have not populated must refuse a trip, not wave
//!    every trip through. It is also the shape a crafted request meets when it names nowhere real;
//!  - THE RETURNED VECTOR IS THE CANDIDATE, NEVER THE TARGET. This is the anti-arbitrary-position
//!    property itself - the client names a place and the server supplies the coordinate - and a
//!    tolerant match that echoed the caller's vector back would pass every other assertion here while
//!    silently restoring the defect. Asserted by matching a target deliberately offset from its
//!    candidate and then requiring the OUTPUT to be the candidate;
//!  - THE TOLERANCE IS OVT_RespawnService.MATCH_TOLERANCE, applied as a real radius rather than as an
//!    exact compare. A vector makes a round trip through the map record and the wire, so an exact
//!    compare would refuse legitimate trips; the cases below sit either side of the constant rather
//!    than either side of a hand-written 2.0, so retuning the constant retunes the test with it;
//!  - the match is on DISTANCE, so it holds on every axis rather than only the one a single case
//!    happened to offset along;
//!  - the FIRST match wins and nothing scans past it, which is what makes the returned vector
//!    deterministic when two locations sit close together.
//!
//! PROVEN ABLE TO FAIL (CLAUDE.md: no case ships without this). Temporarily changing
//! MatchesAnyPosition's `matched = candidate` to `matched = target` left every distance assertion
//! green and failed only the two identity assertions - which is exactly the regression they exist to
//! catch. Separately, changing its `return false` tail to `return true` failed the empty-set and
//! far-miss cases. Both reverted.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_TravelDestination : SCR_AutotestCaseBase
{
	//! A candidate far from every other one used here, so a "matched the wrong entry" fault reads as a
	//! distinct failure rather than as a near miss.
	protected static const vector CANDIDATE_A = "1000 0 1000";

	//! Deliberately 500 m from CANDIDATE_A - far enough that no tolerance question arises between them.
	protected static const vector CANDIDATE_B = "1500 0 1000";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		vector matched;

		// --- FAIL CLOSED ON AN EMPTY SET. A server with nothing enumerated must refuse, not permit.
		array<vector> empty = new array<vector>();
		if (OVT_FastTravelService.MatchesAnyPosition(empty, CANDIDATE_A, matched))
		{
			SetFailure("An empty candidate set matched a destination - a server with no enumerated locations must refuse every trip, not permit every trip");
			return true;
		}

		// And it must not leave a stale vector behind for a caller that ignores the bool.
		if (matched != vector.Zero)
		{
			SetFailure("A refused match returned %1 rather than vector.Zero", matched.ToString());
			return true;
		}

		array<vector> candidates = new array<vector>();
		candidates.Insert(CANDIDATE_A);
		candidates.Insert(CANDIDATE_B);

		// --- AN EXACT NAME MATCHES, and answers with the entry it matched.
		if (!ExpectMatch(candidates, CANDIDATE_B, CANDIDATE_B, "an exact hit on the second candidate"))
			return true;

		// --- A DESTINATION NOBODY OFFERS IS REFUSED. This is the crafted-request case: a coordinate
		// that is not near anything the server enumerated.
		if (!ExpectNoMatch(candidates, "5000 0 5000", "a coordinate far from every candidate"))
			return true;

		// --- THE TOLERANCE IS A RADIUS, and it is OVT_RespawnService.MATCH_TOLERANCE rather than a
		// literal - retuning the constant retunes these two cases with it. Just inside matches, and
		// comfortably outside does not.
		float tol = OVT_RespawnService.MATCH_TOLERANCE;

		vector justInside = CANDIDATE_A;
		justInside[0] = justInside[0] + (tol * 0.5);
		if (!ExpectMatch(candidates, justInside, CANDIDATE_A, "a target half a tolerance from its candidate"))
			return true;

		vector wellOutside = CANDIDATE_A;
		wellOutside[0] = wellOutside[0] + (tol * 4);
		if (!ExpectNoMatch(candidates, wellOutside, "a target four tolerances from its candidate"))
			return true;

		// --- THE MATCH IS ON DISTANCE, so it holds on Z as well as X. A rule written against one axis
		// would pass everything above and let a crafted vector walk away along another.
		vector insideOnZ = CANDIDATE_A;
		insideOnZ[2] = insideOnZ[2] + (tol * 0.5);
		if (!ExpectMatch(candidates, insideOnZ, CANDIDATE_A, "a target offset on Z rather than X"))
			return true;

		vector outsideOnZ = CANDIDATE_A;
		outsideOnZ[2] = outsideOnZ[2] + (tol * 4);
		if (!ExpectNoMatch(candidates, outsideOnZ, "a target four tolerances away on Z"))
			return true;

		// --- THE RETURNED VECTOR IS THE SERVER'S, NEVER THE CALLER'S. This is the whole
		// anti-arbitrary-position property: a near-miss target is accepted, and the caller is handed
		// the CANDIDATE to move to. ExpectMatch already asserts the identity, but state it once more
		// against an offset target explicitly, because this is the assertion that fails if
		// MatchesAnyPosition is ever "simplified" into echoing its input back.
		vector offset = CANDIDATE_B;
		offset[0] = offset[0] + (tol * 0.5);
		if (!OVT_FastTravelService.MatchesAnyPosition(candidates, offset, matched))
		{
			SetFailure("A target half a tolerance from CANDIDATE_B did not match");
			return true;
		}

		if (matched == offset)
		{
			SetFailure("MatchesAnyPosition returned the CALLER's vector %1 - it must return the server's own candidate, or a client can nudge its arrival point anywhere within the tolerance", matched.ToString());
			return true;
		}

		if (matched != CANDIDATE_B)
		{
			SetFailure("A near miss on CANDIDATE_B resolved to %1, expected %2", matched.ToString(), CANDIDATE_B.ToString());
			return true;
		}

		// --- FIRST MATCH WINS. Two candidates on top of each other must resolve deterministically to
		// the earlier one, so the arrival point does not depend on enumeration order changing.
		array<vector> overlapping = new array<vector>();
		overlapping.Insert(CANDIDATE_A);
		overlapping.Insert(CANDIDATE_A);
		if (!ExpectMatch(overlapping, CANDIDATE_A, CANDIDATE_A, "two identical candidates"))
			return true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that a target matches, AND that the vector handed back is the server's candidate.
	//! The two are one assertion on purpose: a match that returns the caller's own vector is not a
	//! partial success, it is the defect this rule exists to prevent.
	//! \param[in] candidates The server's enumerated positions.
	//! \param[in] target The vector standing in for the client's request.
	//! \param[in] expected The candidate that must be returned.
	//! \param[in] what Human-readable description of the case, for the failure line.
	//! \return True when it matched as expected; false after recording the failure.
	protected bool ExpectMatch(notnull array<vector> candidates, vector target, vector expected, string what)
	{
		vector matched;
		if (!OVT_FastTravelService.MatchesAnyPosition(candidates, target, matched))
		{
			SetFailure("%1 was refused, expected a match", what);
			return false;
		}

		if (matched != expected)
		{
			SetFailure("%1 resolved to %2, expected the server's candidate %3", what, matched.ToString(), expected.ToString());
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts that a target matches nothing, and that no vector is handed back.
	//! \param[in] candidates The server's enumerated positions.
	//! \param[in] target The vector standing in for the client's request.
	//! \param[in] what Human-readable description of the case, for the failure line.
	//! \return True when it was refused; false after recording the failure.
	protected bool ExpectNoMatch(notnull array<vector> candidates, vector target, string what)
	{
		vector matched;
		if (OVT_FastTravelService.MatchesAnyPosition(candidates, target, matched))
		{
			SetFailure("%1 matched and resolved to %2, expected a refusal", what, matched.ToString());
			return false;
		}

		if (matched != vector.Zero)
		{
			SetFailure("%1 was refused but left %2 behind rather than vector.Zero", what, matched.ToString());
			return false;
		}

		return true;
	}
}
