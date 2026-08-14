//------------------------------------------------------------------------------------------------
//! Pure schedule and prediction maths for the Game Master state seam.
//!
//! Every member of this class is a static function of its arguments alone: no state is read, no
//! state is written, nothing outside the arguments is consulted. That is deliberate and it is the
//! contract - it is what lets the Logic test tier assert this arithmetic directly, and it is what
//! lets a strictly read-only feature predict the occupying faction's next resource distribution
//! without handing that faction a free income tick.
//!
//! The resource-gain arithmetic here is a VERBATIM extraction of what
//! OVT_OccupyingFactionManager.GainResources() used to compute inline; that method now calls
//! PredictResourceGain() for the number and keeps only its side effects (the accumulation, the
//! deployment allocation and its logging). An edit to one without the other silently changes
//! campaign balance, so treat the two as one unit.
//------------------------------------------------------------------------------------------------
class OVT_GMSchedule
{
	//! Length in in-game seconds of one interval between schedule marks (six hours).
	static const int MARK_INTERVAL_SECONDS = 21600;

	//! In-game seconds in a full day.
	static const int DAY_SECONDS = 86400;

	//------------------------------------------------------------------------------------------------
	//! In-game seconds from the given in-game time of day until the next schedule mark.
	//!
	//! Both the occupying faction's resource distribution and the resistance payout fire on the
	//! in-game hours 0, 6, 12 and 18, so the marks are those four instants.
	//!
	//! BOUNDARY, DELIBERATE: the returned value is in the half-open range (0, 21600] - it is the
	//! time to the next mark STRICTLY AFTER the given instant. Exactly on a mark (for example
	//! 06:00:00) the answer is therefore a full 21600 rather than 0, because at that instant the
	//! tick has just fired and the next one is a whole interval away. A caller that wants "0 when
	//! due" wants a different function.
	//!
	//! 19:30:00 wraps past midnight and yields 16200 (to 00:00 the following day).
	//!
	//! \param[in] hours In-game hour of day.
	//! \param[in] minutes In-game minute of the hour.
	//! \param[in] seconds In-game second of the minute.
	//! \return In-game seconds until the next mark, in (0, 21600].
	static float InGameSecondsToNextMark(int hours, int minutes, int seconds)
	{
		int totalSeconds = (hours * 3600) + (minutes * 60) + seconds;

		// Normalise into [0, MARK_INTERVAL_SECONDS) without relying on the sign convention of
		// integer division for negative operands, so a malformed clock reading can never produce a
		// negative or out-of-range countdown.
		int elapsed = totalSeconds % MARK_INTERVAL_SECONDS;
		if (elapsed < 0)
		{
			elapsed += MARK_INTERVAL_SECONDS;
		}

		return MARK_INTERVAL_SECONDS - elapsed;
	}

	//------------------------------------------------------------------------------------------------
	//! Converts a duration measured in in-game seconds into real seconds.
	//!
	//! dayDurationRealSeconds is how many real seconds one in-game day currently takes under the
	//! acceleration in force, so the conversion is a straight proportion of the 86400-second day.
	//!
	//! A non-positive day duration is not an error worth propagating: it means the caller could not
	//! obtain a duration at all. In that case the in-game value is returned unchanged, which is the
	//! 1:1 answer, rather than dividing by zero.
	//!
	//! \param[in] inGameSeconds Duration in in-game seconds.
	//! \param[in] dayDurationRealSeconds Real seconds per in-game day; <= 0 disables the conversion.
	//! \return The duration in real seconds.
	static float RealSecondsFor(float inGameSeconds, float dayDurationRealSeconds)
	{
		if (dayDurationRealSeconds <= 0)
		{
			return inGameSeconds;
		}

		return inGameSeconds * dayDurationRealSeconds / DAY_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! The resources the occupying faction gains on one distribution tick.
	//!
	//! Extracted verbatim from OVT_OccupyingFactionManager.GainResources(): the threat factor is
	//! threat/1000 clamped at 4, and the result is scaled by one of five player-count bands. Note
	//! that the bands are strictly-greater-than tests, so a session with exactly 4, 8, 16, 24 or 32
	//! players sits in the LOWER band of each pair.
	//!
	//! No side effects: this neither accumulates the gain nor allocates any of it.
	//!
	//! \param[in] baseResourcesPerTick Flat amount granted every tick.
	//! \param[in] resourcesPerTick Amount scaled by the threat factor.
	//! \param[in] threat Current campaign threat.
	//! \param[in] playerCount Players currently online.
	//! \return The amount that would be gained.
	static int PredictResourceGain(int baseResourcesPerTick, int resourcesPerTick, float threat, int playerCount)
	{
		float threatFactor = threat / 1000;
		if(threatFactor > 4) threatFactor = 4;
		int newResources = baseResourcesPerTick + (resourcesPerTick * threatFactor);

		int numPlayersOnline = playerCount;

		//Scale resources by number of players online
		if(numPlayersOnline > 32)
		{
			newResources *= 6;
		}else if(numPlayersOnline > 24)
		{
			newResources *= 5;
		}else if(numPlayersOnline > 16)
		{
			newResources *= 4;
		}else if(numPlayersOnline > 8)
		{
			newResources *= 3;
		}else if(numPlayersOnline > 4)
		{
			newResources *= 2;
		}

		return newResources;
	}
}
