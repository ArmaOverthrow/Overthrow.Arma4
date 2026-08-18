//------------------------------------------------------------------------------------------------
//! The world side of sleeping: the gates that decide whether a player may sleep, the accounting
//! catch-up, the cooldown stamp and the clock advance.
//!
//! SINGLE PLAYER ONLY, AND THEREFORE STATIC (implementation.md D12). In single player
//! (RplMode.None) the machine that clicks the action IS the authority, so there is no client-server
//! seam to build: no RPC, no controller component, no replicated state and no OVT_Global accessor.
//! Do not cargo-cult the recruit/shop client->server pattern into this file - every moving part it
//! would add could only ever no-op here. OVT_FastTravelService and OVT_RespawnService are the
//! precedent for a static service in this directory.
//!
//! THE DIVISION OF LABOUR WITH OVT_SleepSchedule. Everything that is ARITHMETIC - how many payouts a
//! window contains, what the landing hour is, how much cooldown is left, how to spell it - lives in
//! OVT_SleepSchedule, which touches no world and is pinned by the Logic tier. Everything that is a
//! WORLD OPERATION - reading the clock, moving it, walking manager collections - lives here. Nothing
//! here re-derives a number the schedule can answer.
//!
//! THE ORDER OF OPERATIONS IS THE CONTRACT (implementation.md 3.3). Both HandleTimeSkip calls happen
//! BEFORE the clock is advanced, because both managers read the current in-game time as the START of
//! the skipped window. Advancing first would make the window measure from the landing time and
//! silently pay for a window that has already elapsed. See PerformSleepNow().
//!
//! WHY PerformSleep RE-VALIDATES. OVT_OccupyingFactionManager.HandleTimeSkip deliberately does NOT
//! re-check the QRF flag its live CheckUpdate returns early on, so that a skip cannot replay the
//! economy while silently skipping the war. That makes CanSleep the ONLY QRF gate on this path, and
//! PerformSleep calling it before anything else is what keeps the pair safe.
//------------------------------------------------------------------------------------------------
class OVT_SleepService
{
	//! In-game hours one sleep skips.
	static const int SKIP_HOURS = 8;

	//! In-game hours that must pass before the player may sleep again, measured from the moment they
	//! WAKE (D16, revised 2026-08-19 by the user) - so the full twelve are ahead of them when the
	//! screen fades back in, and a bed will not take them again until twenty in-game hours after they
	//! lay down. The stamp is written accordingly by StampCooldown(); this constant is not the place
	//! that decides it.
	static const int COOLDOWN_HOURS = 12;

	//------------------------------------------------------------------------------------------------
	// LOCATION GATE DISTANCES
	//
	// SOURCE OF TRUTH: Scripts/Game/Utilities/OVT_ItemLimitChecker.c (MAX_HOUSE_PLACE_DIS,
	// MAX_CAMP_PLACE_DIS, MAX_FOB_PLACE_DIS and the m_Difficulty.baseRange used for bases). The
	// checker's members are protected and its method answers an item COUNT rather than a boolean, so
	// the ordered test below has to be reproduced - but the NUMBERS are re-declared here explicitly,
	// with this comment, rather than copied silently. Verified equal to the checker's values on
	// 2026-08-18; if they ever diverge, the checker wins and these follow.
	//------------------------------------------------------------------------------------------------

	//! Owned house: OVT_ItemLimitChecker.MAX_HOUSE_PLACE_DIS.
	static const float MAX_HOUSE_PLACE_DIS = 30;

	//! Own camp: OVT_ItemLimitChecker.MAX_CAMP_PLACE_DIS.
	static const float MAX_CAMP_PLACE_DIS = 75;

	//! Deployed FOB: OVT_ItemLimitChecker.MAX_FOB_PLACE_DIS.
	static const float MAX_FOB_PLACE_DIS = 100;

	//------------------------------------------------------------------------------------------------
	// REFUSAL REASONS
	//
	// Localization keys shown by the action's SetCannotPerformReason. A refusal with an EMPTY key is
	// deliberate and means "there is nothing to tell the player": not single player, no resolvable
	// record, no clock. Those are states in which the action is not shown at all, so a reason would
	// never reach a screen.
	//------------------------------------------------------------------------------------------------

	static const string REASON_QRF = "#OVT-SleepQRF";
	static const string REASON_WANTED = "#OVT-SleepWanted";
	static const string REASON_COOLDOWN = "#OVT-SleepCooldown";

	//------------------------------------------------------------------------------------------------
	//! How long after the action is performed the skip actually runs, in milliseconds.
	//!
	//! THIS IS FADE_DURATION + BLACK_DURATION, IN MILLISECONDS - 1.0 s to fade to black plus 0.5 s
	//! held there - which is the shape SCR_FastTravelComponent uses (:264-296). The plan contradicts
	//! itself here (3.2 sketches +1.2 s, T5.1 specifies (FADE_DURATION + BLACK_DURATION) * 1000);
	//! T5.1's number is the one used, and it was already in place before the fade landed so that the
	//! deferred structure never had to be retrofitted.
	//!
	//! IT IS NOT DERIVED FROM THE TWO FLOAT CONSTANTS ON PURPOSE - a literal is what the call queue
	//! takes and there is nothing to round. If either duration below changes, change this too.
	//------------------------------------------------------------------------------------------------
	static const int WORK_DELAY_MS = 1500;

	//------------------------------------------------------------------------------------------------
	// THE FADE (T5.1)
	//
	// FLAVOUR, AND TREATED AS FLAVOUR (implementation.md 7, Q4, R2). Every step of the lookup is
	// null-guarded and NOTHING about the time skip depends on any of it succeeding: the CallLater that
	// does the work is scheduled unconditionally by ScheduleSleep(), so a world with no screen-effects
	// display, or a display with no fade effect registered, still skips eight hours correctly - the
	// player simply gets no fade.
	//
	// SCR_FadeInOutEffect.FadeOutEffect SILENTLY DOES NOTHING for a duration below 0.1 s. Neither
	// constant below may go under that, and neither is a config knob.
	//------------------------------------------------------------------------------------------------

	//! Seconds the screen takes to fade to black, and to fade back in afterwards.
	static const float FADE_DURATION = 1.0;

	//! Seconds the screen is held fully black after the fade out, before the skip runs.
	static const float BLACK_DURATION = 0.5;

	//! Shown on waking. The payout notifications the catch-up fires arrive over the black screen and
	//! immediately after; they are deliberately NOT suppressed (R9) - they are the player's evidence
	//! that the accounting actually ran, and this hint is only the "time passed" cue beside them.
	static const string HINT_SLEPT_WELL = "#OVT-SleptWell";

	//! Seconds the wake hint stays on screen, matching the other Overthrow hints (OVT_SetHomeAction.c:12).
	static const int HINT_DURATION = 4;

	//------------------------------------------------------------------------------------------------
	//! The ONLY instance state in this file, and it exists for one mechanical reason: CallLater binds
	//! a method on an OBJECT, so a static function cannot be scheduled (see OVT_NavmeshRebuild.c:53,
	//! which pays the same toll). It holds nothing but the pending sleeper's id and a re-entry guard.
	//------------------------------------------------------------------------------------------------
	protected static ref OVT_SleepService s_Instance;

	//! Persistent id of the player whose skip is in flight. The USER ENTITY is deliberately not held:
	//! nothing after the gates needs it, and a stale entity reference across a delay is a hazard.
	protected string m_sPendingPlayerId;

	//! True between scheduling the work and running it, so a second perform cannot stack two skips.
	protected bool m_bWorkScheduled;

	//------------------------------------------------------------------------------------------------
	// GATES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Whether a position is somewhere this player is entitled to sleep.
	//!
	//! Reproduces the ORDER of OVT_ItemLimitChecker.CountItemsAtLocation() exactly - owned house,
	//! own camp, any deployed FOB, any captured base - because that is the ordering the placement
	//! system already applies and "where the player may build" is the same question as "where the
	//! player may sleep".
	//!
	//! THE DISTANCE CHECK IS NOT OPTIONAL. Every GetNearest* below returns the nearest record ON THE
	//! WHOLE MAP with no maximum range, so without the comparison this would answer true anywhere a
	//! player owns anything at all.
	//! \param[in] pos World position of the bed.
	//! \param[in] playerPersistentId Persistent id of the player asking. Empty answers false.
	//! \return True when the position is inside an owned house, the player's own camp, a deployed FOB
	//! or a captured base.
	static bool IsSleepLocation(vector pos, string playerPersistentId)
	{
		if (playerPersistentId == "")
			return false;

		// 1. A house this player owns.
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (realEstate)
		{
			IEntity house = realEstate.GetNearestOwned(playerPersistentId, pos);
			if (house && vector.Distance(house.GetOrigin(), pos) < MAX_HOUSE_PLACE_DIS)
				return true;
		}

		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if (resistance)
		{
			// 2. This player's OWN camp - somebody else's camp is not a bed the player may use.
			OVT_CampData camp = resistance.GetNearestCampData(pos);
			if (camp && camp.owner == playerPersistentId
				&& vector.Distance(camp.location, pos) < MAX_CAMP_PLACE_DIS)
			{
				return true;
			}

			// 3. Any deployed FOB. FOBs are the resistance's, not one player's.
			OVT_FOBData fob = resistance.GetNearestFOBData(pos);
			if (fob && vector.Distance(fob.location, pos) < MAX_FOB_PLACE_DIS)
				return true;
		}

		// 4. A base the resistance holds. The range is the difficulty's, as it is for placement.
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (occupying && config && config.m_Difficulty)
		{
			OVT_BaseData base = occupying.GetNearestBase(pos);
			if (base && !base.IsOccupyingFaction()
				&& vector.Distance(base.location, pos) < config.m_Difficulty.baseRange)
			{
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The one rule set for "may this player sleep right now".
	//!
	//! ONE PLACE, TWO CALLERS: the action's CanBePerformedScript uses it to decide whether to offer
	//! the action (and what reason to show when it refuses), and PerformSleep re-runs it before
	//! touching anything, so a state that changed between the two cannot get through.
	//!
	//! WHAT IS NOT HERE: the LOCATION gate. That is the action's VISIBILITY question
	//! (CanBeShownScript -> IsSleepLocation) rather than a refusal with a reason - a Sleep action on
	//! a random village bed should not appear at all, greyed out or otherwise.
	//! \param[in] user The player's controlled entity.
	//! \param[out] reasonKey Localization key describing the refusal, or "" when there is nothing to
	//! show the player (see the REFUSAL REASONS block).
	//! \return True when the sleep may proceed.
	static bool CanSleep(IEntity user, out string reasonKey)
	{
		reasonKey = "";

		// Single player only. Not a refusal a player is told about - the action is not shown at all
		// on a listen host or a dedicated client.
		if (RplSession.Mode() != RplMode.None)
			return false;

		if (!user)
			return false;

		string persistentId = ResolvePersistentId(user);
		if (persistentId == "")
			return false;

		// A QRF is a battle: the player is needed awake, and eight hours of catch-up in the middle of
		// one would resolve it off-screen.
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_bQRFActive)
		{
			reasonKey = REASON_QRF;
			return false;
		}

		// Wanted level is read LIVE off the character - it is deliberately never persisted, so there
		// is no stored copy of it to consult.
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (character)
		{
			OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
			if (wanted && wanted.GetWantedLevel() > 0)
			{
				reasonKey = REASON_WANTED;
				return false;
			}
		}

		if (GetCooldownRemainingHours(persistentId) > 0)
		{
			reasonKey = REASON_COOLDOWN;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! In-game hours of sleep cooldown left for one player. 0 means ready.
	//!
	//! FAILS OPEN on anything it cannot resolve - no record, no clock - for the same reason
	//! OVT_SleepSchedule.CooldownRemainingHours fails open on a negative elapsed time (D17): a
	//! cooldown that can never be satisfied would lock the action permanently on that campaign, and
	//! the worst case of the other direction is one extra sleep.
	//! \param[in] playerPersistentId Persistent id of the player.
	//! \return Hours remaining, 0 when ready.
	static float GetCooldownRemainingHours(string playerPersistentId)
	{
		if (playerPersistentId == "")
			return 0;

		OVT_PlayerData player = OVT_PlayerData.Get(playerPersistentId);
		if (!player)
			return 0;

		TimeAndWeatherManagerEntity time = ResolveTimeManager();
		if (!time)
			return 0;

		return OVT_SleepSchedule.CooldownRemainingHours(ReadAbsoluteGameHours(time), player.m_fLastSleepGameHours, COOLDOWN_HOURS);
	}

	//------------------------------------------------------------------------------------------------
	// PERFORMING THE SLEEP
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Performs one sleep: validates, then schedules the skip.
	//!
	//! The work is DEFERRED rather than run inline because the screen has to go black first (Phase 5)
	//! and because the payout notifications the catch-up fires should land during the black, not while
	//! the player is still looking at the bed. The schedule is unconditional once the gates pass: no
	//! screen effect can prevent the clock advancing.
	//! \param[in] user The player's controlled entity.
	static void PerformSleep(IEntity user)
	{
		// RE-VALIDATION, AND WHY IT IS FIRST. The action's gates ran up to a second ago behind a
		// cache; the QRF flag in particular can have flipped since, and the occupying faction's
		// HandleTimeSkip does not re-check it. Refuse silently - the player has already been told by
		// the action's own disabled reason if there was anything to tell.
		string reasonKey;
		if (!CanSleep(user, reasonKey))
			return;

		string persistentId = ResolvePersistentId(user);
		if (persistentId == "")
			return;

		if (!s_Instance)
			s_Instance = new OVT_SleepService();

		s_Instance.ScheduleSleep(persistentId);
	}

	//------------------------------------------------------------------------------------------------
	//! Schedules the deferred half of one sleep, ignoring a second request while one is in flight.
	//! \param[in] playerPersistentId Persistent id of the sleeping player.
	protected void ScheduleSleep(string playerPersistentId)
	{
		if (m_bWorkScheduled)
			return;

		m_bWorkScheduled = true;
		m_sPendingPlayerId = playerPersistentId;

		// Flavour first, and it can fail freely: SetFade() is null-guarded end to end and returns
		// quietly when there is no screen-effects display or no fade effect (Q4).
		SetFade(true);

		// THE WORK IS SCHEDULED UNCONDITIONALLY, on the line after the fade and not inside it. This is
		// the structural guarantee behind Q4 - there is no branch between here and the CallLater that a
		// missing screen effect could ever take.
		GetGame().GetCallqueue().CallLater(PerformSleepNow, WORK_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The skip itself, run once the screen has had time to go black.
	//!
	//! THE ORDER OF THESE FOUR STEPS IS THE FEATURE (implementation.md 3.3):
	//!
	//!   1+2. BOTH HandleTimeSkip CALLS HAPPEN BEFORE THE CLOCK MOVES. Each manager reads the current
	//!        in-game time as the START of the window (start, start + SKIP_HOURS] and replays what
	//!        that window contains. Advancing the clock first would make them measure from the landing
	//!        time - eight hours that have already been accounted for - so every payout would be
	//!        counted against the wrong window. This is the single ordering constraint the whole
	//!        catch-up design rests on and it is asserted by nothing but this comment and the shape of
	//!        the code below: keep the AdvanceClock call last.
	//!   3.   The cooldown is stamped off the PRE-SKIP calendar too (T2.4), for the same reason - but
	//!        the value written is the WAKE instant, pre-skip + SKIP_HOURS (D16, revised 2026-08-19),
	//!        so the twelve hours run from waking and the player is awake for all twelve of them. See
	//!        StampCooldown(); it must still run before step 4.
	//!   4.   Only then does the world clock move.
	//!   5+6. The wake hint and the fade back in. Both are presentation, both come after every piece of
	//!        state has already changed, and neither can prevent any of it.
	protected void PerformSleepNow()
	{
		m_bWorkScheduled = false;

		string persistentId = m_sPendingPlayerId;
		m_sPendingPlayerId = "";

		// 1. Economy: income, NPC shop buying, restock and rent, for the window about to be skipped.
		OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
		if (economy)
			economy.HandleTimeSkip(SKIP_HOURS);

		// 2. Occupying faction: resource gain, base spending and threat decay, chronologically.
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying)
			occupying.HandleTimeSkip(SKIP_HOURS);

		// 3. The cooldown stamp - still read off the pre-skip clock, but recording the WAKE instant.
		StampCooldown(persistentId);

		// 4. And now, last of the state changes, the clock.
		AdvanceClock(SKIP_HOURS);

		// 5. The wake cue. Deliberately NOT a replacement for the payout notifications (R9).
		SCR_HintManagerComponent.ShowCustomHint(HINT_SLEPT_WELL, "", HINT_DURATION);

		// 6. Back to the world. Null-safe; if the fade out never happened this is a no-op too.
		SetFade(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Fades the screen to black, or back in from it. Does nothing at all when the screen-effects
	//! display or the fade effect is unavailable.
	//!
	//! FOLLOWS SCR_FastTravelComponent.c:264-296 exactly - the same manager lookup, the same cast, the
	//! same call - because that is the one path in the base game known to work from script.
	//!
	//! THE FADE EFFECT IS INHERITED, NOT REGISTERED HERE. SCR_FadeInOutEffect lives inside the
	//! SCR_ScreenEffectsManager on vanilla's SCR_HUDManagerComponent
	//! (ArmaReforger/Prefabs/Characters/Core/DefaultPlayerController.et:49-53), and Overthrow's
	//! Prefabs/Characters/Core/OVT_PlayerController.et only ADDS two components to
	//! DefaultPlayerControllerMP.et - it never declares SCR_HUDManagerComponent, so the whole effects
	//! tree comes through untouched.
	//!
	//! NOTHING CALLING THIS MAY DEPEND ON IT. See Q4 and the comment in ScheduleSleep().
	//! \param[in] toBlack True to fade out to black, false to fade back in.
	protected static void SetFade(bool toBlack)
	{
		SCR_ScreenEffectsManager manager = SCR_ScreenEffectsManager.GetScreenEffectsDisplay();
		if (!manager)
			return;

		SCR_FadeInOutEffect fade = SCR_FadeInOutEffect.Cast(manager.GetEffect(SCR_FadeInOutEffect));
		if (!fade)
			return;

		// FadeOutEffect silently ignores a duration below 0.1 s - FADE_DURATION is 1.0 and is not a knob.
		fade.FadeOutEffect(toBlack, FADE_DURATION);
	}

	//------------------------------------------------------------------------------------------------
	//! Records "this player woke here" on their campaign record, in absolute in-game hours.
	//!
	//! THE STAMP IS THE WAKE INSTANT, NOT THE MOMENT OF LYING DOWN (D16, revised 2026-08-19 by the
	//! user). The twelve hours run from waking, so a sleeper is awake for twelve in-game hours before
	//! a bed will take them again - twenty hours door to door, not twelve.
	//!
	//! IT IS STILL COMPUTED FROM THE PRE-SKIP CLOCK, deliberately: the wake instant IS
	//! "pre-skip + SKIP_HOURS", by definition of the skip, and AdvanceClock preserves minutes and
	//! seconds - so adding the constant keeps the stamp on the SAME clock read the accounting catch-up
	//! used and needs no second read, no re-ordering of PerformSleepNow() and no dependence on the
	//! clock advance having succeeded. MUST STILL BE CALLED BEFORE AdvanceClock.
	//!
	//! TWO EDGES, BOTH ACCEPTED, BOTH FAIL-OPEN.
	//!
	//!  1. OVT_SleepSchedule.AbsoluteGameHours is monotonic rather than calendar-exact (DAYS_PER_YEAR is
	//!     366 for every year), so on the one night a year a skip crosses new year the real post-skip
	//!     stamp is 24 h larger than pre-skip + 8 and the cooldown reads as already expired. Same
	//!     fail-open direction as D17, at most one extra sleep per in-game year; reading the post-skip
	//!     clock instead would trade it for a dependency on the SetDate ladder having worked, which is
	//!     the worse of the two.
	//!  2. A CLOCK THAT REFUSES TO MOVE leaves this stamp in the future, and CooldownRemainingHours
	//!     fails open on a future stamp (D17) - so the player could sleep again at once and replay the
	//!     accounting each time. This is the ONE way the wake-relative stamp degrades worse than the
	//!     lie-down-relative one it replaced, which blocked for twelve hours instead. It is bounded:
	//!     the precondition is "the eight-hour skip visibly did nothing", which is a hard play-test
	//!     gate, and AdvanceClock now logs it rather than letting it pass in silence.
	//!
	//! Silently does nothing when the record or the clock cannot be resolved: an unstamped cooldown
	//! means one extra sleep, which is again the fail-open direction GetCooldownRemainingHours takes.
	//! \param[in] playerPersistentId Persistent id of the sleeping player.
	protected static void StampCooldown(string playerPersistentId)
	{
		if (playerPersistentId == "")
			return;

		OVT_PlayerData player = OVT_PlayerData.Get(playerPersistentId);
		if (!player)
			return;

		TimeAndWeatherManagerEntity time = ResolveTimeManager();
		if (!time)
			return;

		player.m_fLastSleepGameHours = ReadAbsoluteGameHours(time) + SKIP_HOURS;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the world clock forward by whole in-game hours, rolling the date when it wraps.
	//!
	//! THE DATE DOES NOT ROLL BY ITSELF. SetTimeOfTheDay only moves the time WITHIN a day
	//! (BaseWeatherManagerEntity.c:256) and there is no "advance one day" helper anywhere in the
	//! engine, so a night sleep would otherwise land on the same calendar day - and the cooldown
	//! stamp, which is built from the date, would go BACKWARDS by sixteen hours.
	//!
	//! THE VALIDATION LADDER (D15). Rolling a day is done by ASKING THE ENGINE: SetDate returns false
	//! for an invalid date (:309), so day + 1, then month + 1 / day 1, then year + 1 / month 1 / day 1
	//! is a month-length table with leap years in it that this file does not have to write, cannot get
	//! wrong, and would otherwise need its own tests. Never hand-write calendar arithmetic here.
	//!
	//! MINUTES AND SECONDS ARE PRESERVED - a whole-hour skip from 23:47 lands on 07:47, not 07:00 -
	//! because the skipped window is measured in whole hours from the current minute and the managers
	//! have already been paid for exactly that window.
	//! \param[in] hours Whole in-game hours to advance. Non-positive does nothing.
	protected static void AdvanceClock(int hours)
	{
		if (hours <= 0)
			return;

		TimeAndWeatherManagerEntity time = ResolveTimeManager();
		if (!time)
			return;

		int hour, minute, second;
		time.GetHoursMinutesSeconds(hour, minute, second);

		// How many midnights the skip crosses. Written as a float floor for the same reason every
		// floor in OVT_SleepSchedule is: EnforceScript's integer division is context-dependent.
		int daysCrossed = (int)Math.Floor((float)(hour + hours) / OVT_SleepSchedule.HOURS_PER_DAY);

		for (int i = 0; i < daysCrossed; i++)
		{
			AdvanceDate(time);
		}

		int landingHour = OVT_SleepSchedule.LandingHour(hour, hours);

		float timeOfDay = landingHour + ((float)minute / OVT_SleepSchedule.MINUTES_PER_HOUR)
			+ ((float)second / 3600);

		// THE RETURN VALUE IS CHECKED PURELY TO MAKE A SILENT FAILURE AUDIBLE. Nothing branches on it -
		// by then the accounting has already run and the cooldown has already been stamped, and there
		// is nothing sensible to undo. It is logged because since D16a the stamp is the WAKE instant:
		// a clock that refuses to move leaves that stamp in the future, CooldownRemainingHours fails
		// open on it (D17), and the player could sleep again immediately - replaying the accounting
		// each time. That degraded path is bounded (a clock that does not move is a hard play-test
		// gate: "the clock did not move" is called out as a real defect), but it must not be silent.
		if (!time.SetTimeOfTheDay(timeOfDay, true))
			Print("[Overthrow.SleepService] SetTimeOfTheDay REFUSED - the clock did not advance; the sleep cooldown will read as ready");
	}

	//------------------------------------------------------------------------------------------------
	//! Advances the calendar by exactly one day, using SetDate's own validation as the calendar.
	//! \param[in] time The time manager, already resolved.
	protected static void AdvanceDate(notnull TimeAndWeatherManagerEntity time)
	{
		int year, month, day;
		time.GetDate(year, month, day);

		// Tomorrow, if that day exists in this month.
		if (time.SetDate(year, month, day + 1, true))
			return;

		// It did not, so this was the last day of the month: the first of the next one, if that month
		// exists in this year.
		if (time.SetDate(year, month + 1, 1, true))
			return;

		// Nor that, so it was the last day of December: new year's day. If even this is refused the
		// calendar is stuck, which R3 rates as the likeliest way the cooldown stamp misbehaves - so say
		// so rather than let it pass unremarked.
		if (!time.SetDate(year + 1, 1, 1, true))
			Print("[Overthrow.SleepService] SetDate REFUSED all three steps of the ladder - the in-game date did not advance");
	}

	//------------------------------------------------------------------------------------------------
	// RESOLUTION HELPERS
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The current in-game instant as one absolute number of hours, for cooldown comparison.
	//! \param[in] time The time manager, already resolved.
	//! \return Absolute in-game hours, from OVT_SleepSchedule.AbsoluteGameHours.
	protected static float ReadAbsoluteGameHours(notnull TimeAndWeatherManagerEntity time)
	{
		int hour, minute, second;
		time.GetHoursMinutesSeconds(hour, minute, second);

		return OVT_SleepSchedule.AbsoluteGameHours(time.GetYear(), time.GetDayInYear(), hour, minute);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the world's time and weather manager.
	//!
	//! Null-guarded at every call site rather than assumed: a world without one is not a crash here,
	//! it is a sleep that does not happen (SCR_CaptureAndHoldManager takes the same care).
	//! \return The manager, or null.
	protected static TimeAndWeatherManagerEntity ResolveTimeManager()
	{
		BaseWorld baseWorld = GetGame().GetWorld();
		if (!baseWorld)
			return null;

		ChimeraWorld world = ChimeraWorld.CastFrom(baseWorld);
		if (!world)
			return null;

		return world.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	//! The persistent id behind a controlled entity.
	//! \param[in] user The player's controlled entity.
	//! \return The persistent id, or "" when it cannot be resolved.
	protected static string ResolvePersistentId(IEntity user)
	{
		if (!user)
			return "";

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return "";

		return players.GetPersistentIDFromControlledEntity(user);
	}
}
