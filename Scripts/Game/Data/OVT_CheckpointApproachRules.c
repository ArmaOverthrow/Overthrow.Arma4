//! Pure spine of the mobile checkpoint's approach chooser: compass arithmetic, the band a checkpoint
//! may sit in, and the rule that a relocation must not land on the bearing it just left. Every static
//! here takes plain numbers or a plain list of numbers - nothing is asked about the map, about roads
//! or about any live object - so each one is proven directly rather than through something that has
//! to be built and torn down.
//!
//! ⚠ THE ROAD TEST IS NOT HERE AND CANNOT BE. "Is there a road within N metres of this point" is the
//! one question the chooser asks of the map, and it is the caller's job: it samples the bearings this
//! file hands it, keeps the ones a road answered for, and hands the survivors back to
//! ChooseBearingIndex(). Splitting it that way is what lets the exclusion rule - the part that goes
//! wrong silently - be asserted with no map at all.
class OVT_CheckpointApproachRules
{
	//! "No bearing has been used yet", for the previousBearing argument below. Deliberately negative:
	//! zero is a real compass bearing (due north) and an unset field starts at zero, so a sentinel of
	//! zero would silently exclude north on the very first choice of every checkpoint.
	static const float NO_PREVIOUS_BEARING = -1;

	//! A full turn, in degrees. Written once so no caller re-types it.
	static const float FULL_TURN_DEGREES = 360;

	//------------------------------------------------------------------------------------------------
	//! Folds any bearing into [0, 360).
	//! \param[in] degrees Any bearing, positive or negative, of any magnitude.
	//! \return The same direction expressed in [0, 360).
	static float NormaliseBearing(float degrees)
	{
		float folded = degrees - (Math.Floor(degrees / FULL_TURN_DEGREES) * FULL_TURN_DEGREES);

		// Math.Floor already guarantees this for every finite input; the clamp is here because the one
		// value it does not - a bearing that folds to exactly 360 through float rounding - would be
		// reported as a full turn away from north instead of as north.
		if (folded >= FULL_TURN_DEGREES)
			return 0;

		if (folded < 0)
			return 0;

		return folded;
	}

	//------------------------------------------------------------------------------------------------
	//! The bearing of one evenly spaced sample around a full turn.
	//! \param[in] index Which sample, from zero.
	//! \param[in] sampleCount How many samples the turn is divided into.
	//! \return The bearing in [0, 360), or NO_PREVIOUS_BEARING when there is nothing to divide.
	static float BearingForSample(int index, int sampleCount)
	{
		if (sampleCount < 1)
			return NO_PREVIOUS_BEARING;

		return NormaliseBearing((index * FULL_TURN_DEGREES) / (float)sampleCount);
	}

	//------------------------------------------------------------------------------------------------
	//! How far apart two bearings are, going the short way round.
	//! \param[in] first One bearing.
	//! \param[in] second The other.
	//! \return The separation in degrees, never more than half a turn and never negative.
	static float AngularSeparation(float first, float second)
	{
		float difference = Math.AbsFloat(NormaliseBearing(first) - NormaliseBearing(second));

		if (difference > FULL_TURN_DEGREES * 0.5)
			return FULL_TURN_DEGREES - difference;

		return difference;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a candidate bearing is far enough from the one just used to be worth moving to.
	//!
	//! ⚠ THIS IS THE RULE THAT STOPS A TWO-ROAD TOWN OSCILLATING. Without it a checkpoint with two
	//! usable approaches alternates between the same pair forever, or - worse, with one usable
	//! approach - relocates onto the spot it is already standing on and reports it as a relocation.
	//! \param[in] candidate The bearing being considered.
	//! \param[in] previous The bearing last used, or NO_PREVIOUS_BEARING when there is none.
	//! \param[in] minSeparation How many degrees apart counts as a different approach. Non-positive
	//! switches the rule off, which is an authored choice and not an error.
	//! \return True when the candidate may be used.
	static bool BearingIsFreshEnough(float candidate, float previous, float minSeparation)
	{
		if (previous < 0)
			return true;

		if (minSeparation <= 0)
			return true;

		return AngularSeparation(candidate, previous) >= minSeparation;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a distance inside the authored band, whichever way round the band was authored.
	//! \param[in] distance The distance to clamp.
	//! \param[in] minDistance One end of the band.
	//! \param[in] maxDistance The other end.
	//! \return The distance, clamped into the band, and never negative.
	static float ClampToBand(float distance, float minDistance, float maxDistance)
	{
		float low = minDistance;
		float high = maxDistance;

		if (low > high)
		{
			low = maxDistance;
			high = minDistance;
		}

		if (low < 0)
			low = 0;

		if (high < low)
			high = low;

		if (distance < low)
			return low;

		if (distance > high)
			return high;

		return distance;
	}

	//------------------------------------------------------------------------------------------------
	//! Where in the authored band this checkpoint sits, for a roll in [0, 1].
	//! \param[in] minDistance The near edge of the band, in metres.
	//! \param[in] maxDistance The far edge.
	//! \param[in] roll A roll in [0, 1]; anything outside is clamped rather than refused.
	//! \return A distance inside the band.
	static float BandDistance(float minDistance, float maxDistance, float roll)
	{
		float clampedRoll = roll;
		if (clampedRoll < 0)
			clampedRoll = 0;

		if (clampedRoll > 1)
			clampedRoll = 1;

		float low = ClampToBand(minDistance, minDistance, maxDistance);
		float high = ClampToBand(maxDistance, minDistance, maxDistance);

		return low + ((high - low) * clampedRoll);
	}

	//------------------------------------------------------------------------------------------------
	//! Picks one of N candidates from a roll in [0, 1].
	//!
	//! ⚠ A ROLL OF EXACTLY 1 MUST NOT INDEX PAST THE END. This exists as its own function precisely
	//! because the equivalent inline arithmetic is where an off-by-one lives: the tree's own integer
	//! roll is max-EXCLUSIVE, so a caller reaching for a count rather than a count minus one is right,
	//! and a caller multiplying a float roll by the count is one clamp away from being wrong.
	//! \param[in] candidateCount How many candidates there are.
	//! \param[in] roll A roll in [0, 1]; anything outside is clamped.
	//! \return The chosen index, or -1 when there is nothing to choose from.
	static int PickCandidate(int candidateCount, float roll)
	{
		if (candidateCount < 1)
			return -1;

		float clampedRoll = roll;
		if (clampedRoll < 0)
			clampedRoll = 0;

		if (clampedRoll > 1)
			clampedRoll = 1;

		// A cast rather than a floor: the roll is already clamped to [0, 1] so it is never
		// negative, and truncation and flooring agree for every non-negative value.
		int index = (int)(clampedRoll * candidateCount);

		if (index >= candidateCount)
			return candidateCount - 1;

		if (index < 0)
			return 0;

		return index;
	}

	//------------------------------------------------------------------------------------------------
	//! THE WHOLE CHOICE: drop every candidate too close to the bearing just used, then pick uniformly
	//! among what is left.
	//!
	//! ⚠ IT REFUSES RATHER THAN FALLING BACK ON THE EXCLUDED BEARING. A relocation that cannot find a
	//! different approach must report that it could not, so the caller keeps the checkpoint where it
	//! is and tries again on the next clock rather than announcing a move it did not make.
	//!
	//! ⚠ THE SURVIVORS ARE COLLECTED IN AUTHORED ORDER. Nothing here removes from the input list -
	//! the tree's own array removal is a swap-with-the-last and would shuffle the caller's parallel
	//! lists of positions and headings out from under the index this returns.
	//! \param[in] bearings The candidate bearings, in the caller's own order.
	//! \param[in] previous The bearing last used, or NO_PREVIOUS_BEARING when there is none.
	//! \param[in] minSeparation How many degrees apart counts as a different approach.
	//! \param[in] roll A roll in [0, 1] deciding which survivor is taken.
	//! \return An index INTO bearings, or -1 when nothing qualified.
	static int ChooseBearingIndex(notnull array<float> bearings, float previous, float minSeparation, float roll)
	{
		array<int> survivors = new array<int>();

		for (int i = 0; i < bearings.Count(); i++)
		{
			if (BearingIsFreshEnough(bearings[i], previous, minSeparation))
				survivors.Insert(i);
		}

		int picked = PickCandidate(survivors.Count(), roll);
		if (picked == -1)
			return -1;

		return survivors[picked];
	}
}
