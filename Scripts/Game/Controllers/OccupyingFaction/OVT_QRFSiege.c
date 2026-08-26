//------------------------------------------------------------------------------------------------
//! PURE STATICS - the arithmetic of a counter-attack siege: where the ring slots sit, when the muster
//! clock is worth broadcasting, how it is rendered, and whether the whole force is down.
//!
//! THE HARD RULE, the same one OVT_QRFBearing and OVT_ObjectivePhaseRules carry: every function here
//! is a function of its arguments and nothing else. No world, no entity, no manager, no config, no
//! clock and NO RANDOMNESS - the radius roll stays at the call site, so the whole of the geometry
//! below is assertable in the cheapest test tier. The tier's reviewer grep runs over a whole directory
//! and does not distinguish code from comments, so none of the banned identifiers appears anywhere in
//! this file, prose included.
//!
//! ==================================================================================================
//! 🔴 THE RING'S SIGN CONVENTION IS THE ONE THE BATTLE CONTROLLER ALREADY USES.
//! ==================================================================================================
//! GetRandomDirection() in OVT_QRFControllerComponent documents it in file, and OVT_QRFBearing
//! re-states it:
//!
//!     0 deg = North = -Z,   90 deg = East = +X,   180 deg = South = +Z,   270 deg = West = -X
//!
//! so a unit direction for a bearing is {sin(a), 0, -cos(a)}. RingSlotOffset() below is that same
//! expression scaled by a radius. Get the sign wrong and the ring is MIRRORED about the north-south
//! axis - which is invisible, because a full circle of slots is still a full circle of slots. It only
//! surfaces as "the group that was meant to be east of town walked west", with nothing in any log, so
//! the sign is asserted as a named case with an explicit due-north expectation rather than left to a
//! reviewer's mental arithmetic.
//!
//! ==================================================================================================
//! 🔴 AllNeutralised(0, 0) IS FALSE, AND THAT IS A DELIBERATE BIAS, NOT AN EDGE CASE.
//! ==================================================================================================
//! See its own header. It ends a battle early and hands the resistance the objective; a false positive
//! is the worst failure available in this feature.
//------------------------------------------------------------------------------------------------
class OVT_QRFSiege
{
	//! Degrees in a full turn. Named because it is a divisor here, not a coincidental literal.
	static const float FULL_CIRCLE = 360;

	//! The muster window, in milliseconds. FIFTEEN REAL MINUTES, not in-game minutes (D16, user
	//! decision): the window exists so players can physically drive to the fight, and a world running
	//! at 6x would otherwise leave two and a half real minutes, which is a warning shot rather than a
	//! window. Fits an int with three orders of magnitude to spare.
	//!
	//! ⚠ THIRTY UNTIL 2026-08-20, when the author play-tested the first counter-attack that actually
	//! fired and judged it too long ("im also thinking 30 mins is too long, maybe 15 is better").
	//! Halving it does not touch any other rule: nothing is scored during the window whatever its
	//! length, the early-wipe end is on its own 10 s cadence, and the minutes/seconds display crossover
	//! is PUBLISH_THRESHOLD_MS and unchanged. The only thing that scales with it is how far away a
	//! player can be and still make it, which is the knob itself.
	static const int MUSTER_TIME_MS = 900000;

	//! Innermost and outermost radius of a siege ring slot, in metres. Close enough that the ring
	//! reads as an encirclement from the middle of the objective, far enough that it is not standing
	//! in the town square.
	static const float SIEGE_RING_MIN = 100;
	static const float SIEGE_RING_MAX = 150;

	//! Below this many milliseconds remaining the clock is published every second and rendered in
	//! seconds; above it, every PUBLISH_COARSE_INTERVAL_MS and rendered in whole minutes.
	//!
	//! ⚠ THE BOUNDARY IS EXCLUSIVE ON THE MINUTES SIDE. At exactly 120 000 the display is already in
	//! seconds, so a standard battle - whose clock starts at exactly 120 000 - can never reach the
	//! minutes branch at all. That is the point: the standard countdown is not allowed to change.
	static const int PUBLISH_THRESHOLD_MS = 120000;

	//! How much of the clock has to have elapsed before a fresh broadcast is worth sending, while the
	//! display is in whole minutes.
	//!
	//! ⚠ WHY THIS EXISTS. The battle controller broadcasts an RPC to EVERY client on every one-second
	//! tick. Over a 30-minute muster that is 1 800 broadcasts where a standard battle sends 120. A ten
	//! second granularity is invisible on a minutes display and cuts it to ~180.
	static const int PUBLISH_COARSE_INTERVAL_MS = 10000;

	//! Milliseconds in a minute.
	static const int MS_PER_MINUTE = 60000;

	//------------------------------------------------------------------------------------------------
	//! The compass bearing of one ring slot, measured from the centre of the objective.
	//!
	//! Slots are spread evenly: with N groups, slot i sits at 360/N * i. SLOT 0 IS ALWAYS DUE NORTH,
	//! which is worth knowing because it is the one row of the geometry that can be asserted without
	//! reproducing the formula.
	//!
	//! ⚠ THE COUNT IS THE FINAL QUEUE LENGTH AND MUST BE KNOWN BEFORE THE FIRST SLOT IS ASKED FOR.
	//! Assigning slots as groups spawn - i.e. with a count that grows - collapses the ring: the first
	//! group is told "you are 1 of 1, stand due north", the second "you are 2 of 2, stand due south",
	//! and every slot after that lands on top of an earlier one.
	//! \param[in] index Which slot, 0-based. Wrapped, so an out-of-range index is still a real slot.
	//! \param[in] count How many slots the ring has. Zero or negative answers DUE NORTH rather than
	//! dividing by zero.
	//! \return A bearing in [0, 360), in the 0 = North = -Z convention.
	static float RingSlotBearing(int index, int count)
	{
		if (count <= 0)
			return 0;

		int wrapped = index % count;

		// EnforceScript's % keeps the sign of the dividend, so a negative index lands on a negative
		// slot. It is not a case any caller produces today; it is a case a future one might.
		if (wrapped < 0)
			wrapped = wrapped + count;

		return (FULL_CIRCLE / count) * wrapped;
	}

	//------------------------------------------------------------------------------------------------
	//! One ring slot as a LOCAL OFFSET from the centre of the objective.
	//!
	//! ⚠ AN OFFSET, NOT A POSITION. The caller adds it to the objective's own origin, which keeps the
	//! objective's ground height - exactly what GetLandingZone() does with its own direction vector.
	//! Returning a world position would need a world to resolve the height against, which is the one
	//! thing this class may not have.
	//!
	//! ⚠ NOT NORMALIZED. The radius IS the length, so a caller that normalizes this loses the ring.
	//! \param[in] index Which slot, 0-based.
	//! \param[in] count How many slots the ring has.
	//! \param[in] radius How far out the slot sits, in metres.
	//! \return An offset on the horizontal plane, of length `radius`, in the 0 = North = -Z convention.
	static vector RingSlotOffset(int index, int count, float radius)
	{
		float bearing = RingSlotBearing(index, count);

		// The same {sin, 0, -cos} expression GetRandomDirection() and OVT_QRFBearing both use, scaled
		// rather than normalized. If the three ever drift, a besieging group would stand somewhere
		// different from where its own landing zone was reckoned, with nothing to say so.
		vector offset =
		{
			Math.Sin(bearing * Math.DEG2RAD) * radius,
			0,
			-Math.Cos(bearing * Math.DEG2RAD) * radius
		};

		return offset;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the muster clock is worth broadcasting to every client this tick.
	//!
	//! Three ways to answer yes, in the order they are asked:
	//!   1. NOTHING HAS BEEN PUBLISHED YET (`lastPublishedMs` negative). The first value of a new clock
	//!      always goes out, whatever the cadence would have said.
	//!   2. THE CLOCK WENT UP. A siege arms the muster window to 1 800 000 over a value that was
	//!      previously 120 000, and the fresh, much larger number has to reach the panel at once.
	//!   3. The cadence: every tick inside the last two minutes, every PUBLISH_COARSE_INTERVAL_MS
	//!      above it.
	//! \param[in] remainingMs What the clock reads now.
	//! \param[in] lastPublishedMs What was last sent, or any negative value for "nothing yet".
	//! \return True when this value should be broadcast.
	static bool ShouldPublishTimer(int remainingMs, int lastPublishedMs)
	{
		if (lastPublishedMs < 0)
			return true;

		if (remainingMs > lastPublishedMs)
			return true;

		if (remainingMs <= PUBLISH_THRESHOLD_MS)
			return true;

		return (lastPublishedMs - remainingMs) >= PUBLISH_COARSE_INTERVAL_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a countdown of this length should be shown in whole minutes rather than in seconds.
	//!
	//! ⚠ STRICTLY GREATER THAN. A standard battle's countdown starts at exactly PUBLISH_THRESHOLD_MS
	//! and must render in seconds from its very first frame, exactly as it does today.
	//! \param[in] remainingMs What the clock reads.
	//! \return True for the minutes form, false for today's seconds form.
	static bool ShouldRenderMinutes(int remainingMs)
	{
		return remainingMs > PUBLISH_THRESHOLD_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! The muster clock in whole minutes, ROUNDED UP.
	//!
	//! Up, because the number answers "how long have I still got", and a 29:59 clock reading "29" would
	//! be telling a player they have less time than they do. A freshly armed window therefore reads
	//! exactly 30 and holds it for the first minute.
	//!
	//! ⚠ THE DIVISION IS FORCED TO FLOAT. `ms / MS_PER_MINUTE` between two ints truncates, which would
	//! make the ceiling a no-op and turn the whole display into a round-DOWN.
	//! \param[in] ms What the clock reads. Negative and zero both answer 0.
	//! \return Whole minutes remaining, at least 0.
	static int MusterMinutesRemaining(int ms)
	{
		if (ms <= 0)
			return 0;

		float minutes = ms / (float)MS_PER_MINUTE;

		int rounded = Math.Ceil(minutes);

		return rounded;
	}

	//------------------------------------------------------------------------------------------------
	//! The clock as "M:SS" - one string for the whole countdown, both QRF modes.
	//!
	//! ⚠ IT REPLACES A TWO-BRANCH DISPLAY, and that is the point rather than a side effect. The HUD used
	//! to render whole minutes above PUBLISH_THRESHOLD_MS and a bare seconds count below it, through two
	//! different localised strings - so one countdown changed both its unit and its wording partway
	//! through, and a player watching "3" turn into "119" had to work out that nothing had gone wrong.
	//! A single M:SS reads identically at 15:00 and at 0:07 and needs no crossover at all (author,
	//! 2026-08-20).
	//!
	//! ⚠ SECONDS ARE ZERO-PADDED AND MINUTES ARE NOT. "9:07" is a clock; "9:7" is a typo and reads as
	//! nine minutes seven-something. The padding is what makes the string scannable at a glance, which
	//! is the entire job of a HUD countdown.
	//!
	//! ⚠ IT TRUNCATES RATHER THAN ROUNDING, unlike MusterMinutesRemaining's deliberate round-UP. A clock
	//! showing seconds has no reason to flatter: 0:59 means fifty-nine seconds, and rounding up would
	//! make the last second of the window read 0:01 for two seconds and then jump to zero.
	//!
	//! NEGATIVE AND ZERO BOTH ANSWER "0:00" rather than a negative clock, which is what the display
	//! shows for the instant between the window expiring and the stage advancing.
	//! \param[in] ms What the clock reads.
	//! \return The countdown as M:SS, never empty.
	static string FormatClock(int ms)
	{
		int total = ms / MS_PER_SECOND;
		if (total < 0)
			total = 0;

		int minutes = total / SECONDS_PER_MINUTE;
		int seconds = total % SECONDS_PER_MINUTE;

		string padded = seconds.ToString();
		if (seconds < 10)
			padded = "0" + padded;

		return minutes.ToString() + ":" + padded;
	}

	//! Milliseconds in a second and seconds in a minute. Named for the same reason FULL_CIRCLE is:
	//! they are divisors in the arithmetic above, not incidental literals.
	static const int MS_PER_SECOND = 1000;
	static const int SECONDS_PER_MINUTE = 60;

	//------------------------------------------------------------------------------------------------
	//! Whether ONE tracked group counts as neutralised, given only the three facts a caller can read
	//! off it.
	//!
	//! ==============================================================================================
	//! 🔴 A GROUP THAT RESOLVED AND HAS **ZERO AGENTS** IS ALIVE. THIS IS THE WHOLE POINT OF THIS
	//! FUNCTION EXISTING SEPARATELY FROM ITS CALLER (D16).
	//! ==============================================================================================
	//! "Zero agents means the group is dead" is a KNOWN-BAD prune in this engine - the AI spawn queue
	//! and dormancy both legitimately produce it, and it is unfixed at HEAD. Siege groups are spawned
	//! live and never virtualised, so it should not arise; but if it ever does, resolving it to DEAD
	//! ends the battle and hands the resistance the objective for free, with nothing in any log. The
	//! rule lives here, in the tier that can assert it, rather than only in a comment beside a loop.
	//! \param[in] groupResolved False when the entity is gone from the world, or is no longer a group.
	//! \param[in] agentCount How many agents the group reports. ZERO MEANS "UNKNOWN", NOT "DEAD".
	//! \param[in] fightingFitCount How many of those agents are alive and conscious.
	//! \return True only when the group is definitely out of the fight.
	static bool GroupNeutralised(bool groupResolved, int agentCount, int fightingFitCount)
	{
		// Gone from the world, or no longer a group at all. The engine disposes of a group once it is
		// empty of agents, so this is the ordinary way a wiped-out group presents.
		if (!groupResolved)
			return true;

		// 🔴 ALIVE. See the block above.
		if (agentCount <= 0)
			return false;

		return fightingFitCount <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the whole siege force is down, given how many groups were tracked and how many of them
	//! were judged neutralised.
	//!
	//! ==============================================================================================
	//! 🔴 THIS PREDICATE ENDS A BATTLE. IT IS BIASED TOWARD "STILL ALIVE" ON PURPOSE (D16).
	//! ==============================================================================================
	//! Firing it early does not cost a little accuracy - it ends the muster window, starts scoring
	//! against a force that is still standing, and hands the resistance the objective for free, in a
	//! way no log would ever explain. Waiting too long costs a siege that had already been wiped out
	//! sitting out the rest of its clock, which nobody would even notice. So:
	//!
	//!   - `tracked <= 0` is **FALSE**. Nothing tracked is not "everything is dead", it is "the
	//!     question has not been asked of anything yet" - the state a controller is in before its
	//!     first group has spawned, which is exactly when a true answer would be catastrophic.
	//!   - the caller's own per-group test resolves a group with a LIVE ENTITY and ZERO AGENTS to
	//!     ALIVE, because "zero agents" is a known-unreliable reading in this engine (the AI spawn
	//!     queue and dormancy both produce it) rather than evidence of death.
	//! \param[in] tracked How many groups the battle ever put on the ground.
	//! \param[in] neutralised How many of them the caller judged dead.
	//! \return True only when at least one group was tracked and every one of them is down.
	static bool AllNeutralised(int tracked, int neutralised)
	{
		if (tracked <= 0)
			return false;

		return neutralised >= tracked;
	}
}
