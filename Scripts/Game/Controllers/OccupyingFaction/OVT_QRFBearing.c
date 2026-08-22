//------------------------------------------------------------------------------------------------
//! PURE STATICS - which way a battle's landing zone lies from the place being attacked.
//!
//! THE HARD RULE, the same one OVT_ObjectivePhaseRules carries: every function here is a function of
//! its arguments and nothing else. No world, no entity, no manager, no config, no clock and - unlike
//! everything that calls it - NO RANDOMNESS. The variance roll stays at the call site, so the whole of
//! the geometry below is assertable in the cheapest test tier, which is the only place the sign of a
//! bearing can be pinned at all.
//!
//! ==================================================================================================
//! 🔴 THE SIGN. READ THIS BEFORE CHANGING ANYTHING HERE.
//! ==================================================================================================
//! OVT_QRFControllerComponent.GetLandingZone() builds its candidate as
//!
//!     checkpos = qrfpos + (dir * distance)
//!
//! where qrfpos is the place under attack. `dir` therefore points FROM THE TARGET OUT TO THE LANDING
//! ZONE - it is not the direction the attackers travel in, it is the direction of the ground they are
//! put down on, measured from the objective.
//!
//! A wave is supposed to land on the side of the objective its SOURCE is on, so that an attack from a
//! base to the north visibly arrives out of the north. The bearing this class returns is therefore the
//! compass bearing of the SOURCE AS SEEN FROM THE TARGET - target -> source - and never the other way
//! round.
//!
//! Getting it backwards compiles, runs, produces a perfectly valid landing zone and puts every wave on
//! the FAR SIDE of the objective from the men who sent it. It reads in play as a pathing or physics
//! fault rather than as a sign error, which is why the direction is asserted as a named case with a
//! due-north source rather than left to a reviewer's mental arithmetic.
//!
//! THE CONVENTION IS THE ONE GetRandomDirection() DOCUMENTS IN FILE (OVT_QRFControllerComponent.c):
//!     0 deg = North = -Z,  90 deg = East = +X,  180 deg = South = +Z,  270 deg = West = -X
//! so a direction vector is {sin(a), 0, -cos(a)} and a bearing is atan2(dx, -dz). DirectionForDegrees()
//! below is that same expression, kept here so the inverse and the forward transform can be asserted
//! against each other in one case.
//------------------------------------------------------------------------------------------------
class OVT_QRFBearing
{
	//! Degrees in a full turn. Named because it is a wrap modulus in three places below, not a
	//! coincidence of three literals.
	static const float FULL_CIRCLE = 360;

	//! What a source standing on top of its own target answers.
	//!
	//! ⚠ IT IS A DEFINED ANSWER, NOT A SENTINEL, AND THAT IS THE POINT. atan2(0, 0) is not specified to
	//! be anything in particular and a degenerate delta must never reach it: the caller uses this value
	//! as a compass bearing straight away, and a value that is not a number would propagate through the
	//! variance roll into a landing zone at "nan nan nan", which the ocean test and the trace would both
	//! accept. North is chosen because it is the convention's own zero and because a coincident source
	//! genuinely has no preferred side.
	static const int DEFAULT_DEGREES = 0;

	//------------------------------------------------------------------------------------------------
	//! The compass bearing a wave's landing zone should be laid out on, given where the wave is coming
	//! from and what it is attacking.
	//!
	//! ⚠ THE ARGUMENT ORDER IS (source, target) AND THE ANSWER IS target -> source. See the sign block
	//! in the file header; this is the one function in the feature whose inverse is also a plausible
	//! implementation.
	//!
	//! HEIGHT IS DISCARDED. A bearing is a compass reading, and the source base being 200 m up a hill
	//! does not change which side of the town its men should appear on.
	//! \param[in] source Where the wave is being sent from - a held base, or the forward operating base.
	//! \param[in] target The place under attack.
	//! \return A bearing in [0, 360), in the 0 = North = -Z convention. DEFAULT_DEGREES when the two
	//! positions coincide.
	static int PreferredDegreesFromSource(vector source, vector target)
	{
		float dx = source[0] - target[0];
		float dz = source[2] - target[2];

		// The degenerate case, handled BEFORE the transcendental rather than after it - see
		// DEFAULT_DEGREES for why an undefined answer here is worse than a wrong one.
		if (dx == 0 && dz == 0)
			return DEFAULT_DEGREES;

		// atan2(x, -z): x grows to the EAST and z grows to the SOUTH, so negating z turns the pair into
		// the (east, north) frame a compass bearing is measured in.
		float degrees = Math.Atan2(dx, -dz) * Math.RAD2DEG;

		int rounded = Math.Round(WrapDegrees(degrees));

		// A bearing of 359.7 rounds to 360, which is the same direction as 0 and must be spelled that
		// way: the caller samples an integer band around this value and 360 is outside the range every
		// other answer lives in.
		if (rounded >= FULL_CIRCLE)
			rounded = 0;

		return rounded;
	}

	//------------------------------------------------------------------------------------------------
	//! Brings any angle back into [0, 360).
	//!
	//! ⚠ A LOOP, NOT A SINGLE ADD. The caller adds a variance roll to a derived bearing, so the input
	//! is bounded in practice - but this is a public static and an unbounded input must not come back
	//! out unwrapped. Bounded by construction: each pass moves the value a whole turn toward the range.
	//! \param[in] degrees Any angle.
	//! \return The same direction expressed in [0, 360).
	static float WrapDegrees(float degrees)
	{
		float wrapped = degrees;

		while (wrapped < 0)
		{
			wrapped = wrapped + FULL_CIRCLE;
		}

		while (wrapped >= FULL_CIRCLE)
		{
			wrapped = wrapped - FULL_CIRCLE;
		}

		return wrapped;
	}

	//------------------------------------------------------------------------------------------------
	//! A unit direction vector for a compass bearing.
	//!
	//! ⚠ IT MUST STAY IDENTICAL TO GetRandomDirection()'s OWN EXPRESSION. That method is the authored
	//! path and is deliberately untouched by this feature; this one is the derived path. If the two ever
	//! disagree, a battle whose source is known would point somewhere different from one whose direction
	//! was authored by hand, with nothing in any log to say so.
	//! \param[in] degrees A bearing, wrapped or not.
	//! \return A normalized direction on the horizontal plane.
	static vector DirectionForDegrees(float degrees)
	{
		float wrapped = WrapDegrees(degrees);

		vector dir = {Math.Sin(wrapped * Math.DEG2RAD), 0, -Math.Cos(wrapped * Math.DEG2RAD)};

		return dir.Normalized();
	}
}
