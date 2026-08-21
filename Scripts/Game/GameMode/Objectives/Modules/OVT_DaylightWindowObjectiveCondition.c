//------------------------------------------------------------------------------------------------
//! "IT IS LIGHT ENOUGH TO ATTACK." The world clock against an authored window (D17).
//!
//! The port of the daylight conjunct of the hard-coded counter-attack gate:
//! OVT_ObjectivePhaseRules.IsCounterAttackWindow() over the shipped 05:00-15:00 window. Ending at
//! 15:00 rather than at dusk is what puts the WHOLE battle in daylight without a second check
//! anywhere - a counter-attack that starts inside this window has its build-up, its muster and its
//! fight before dark.
//!
//! =============================================================================================
//! 🔴 IT HOLDS THE PHASE TIMEOUT AND NOTHING ELSE, AND THAT IS THE D17 CORRECTION ITSELF.
//! =============================================================================================
//! The decision's original wording said a daylight wait ticks "no starvation or timeout counter". Its
//! author corrected that on 2026-08-19 to mean only what it was for: waiting for morning must not
//! count as the objective FAILING. The two clocks are not the same kind of thing.
//!
//!   - THE PHASE TIMEOUT is a clock the director runs against ITSELF. Left running, a gate met at
//!     16:00 would spend the objective's remaining patience waiting out the night and the objective
//!     would be ABANDONED FOR BEING DARK. It is HELD - see HoldsIdleClock() below.
//!   - STARVATION IS THE PLAYER'S COUNTERPLAY. It answers to facts about the WORLD - the supplying
//!     base taken or emptied, a strong resistance presence - and those facts do not stop being true
//!     after sunset. Freezing it would mean a player who empties the supplying garrison at 22:00
//!     watches the forward base stand for hours and then launch a counter-attack anyway, which
//!     contradicts F7 outright and punishes a correct play. It RUNS, and it may take the objective
//!     down mid-wait exactly as it would at midday, blacklist and all.
//!   - THE OPERATION CADENCE runs too, because the garrison sender is what the starvation rule is
//!     measured against and because a frozen cadence would either never fire or fire every tick.
//!
//! This module is a CONDITION and nothing more, so all three of those keep running by construction:
//! a false condition leaves the phase running and the runner goes on to the aborts and the operations
//! exactly as it would on any other tick. The single exemption it asks for is the idle clock.
//! =============================================================================================
//!
//! ⚠ AN UNREADABLE CLOCK ALLOWS THE BATTLE, and that is a decision rather than a fallthrough. A world
//! with no time-and-weather manager has no night to protect anyone from, and failing the other way
//! would mean the occupying faction never counter-attacked at all in such a world - which is exactly
//! the silent, undiagnosable passivity this feature exists to end.
//!
//! ⚠ THE CLOCK HANDLE IS RESOLVED LAZILY AND CACHED. A module is cloned per phase entry, so there is
//! no OnPostInit to fill it in; the first tick that asks resolves it off the world.
//!
//! ⚠ THE WAIT IS ANNOUNCED BY THE RUNNER, NOT BY THIS MODULE, AND THAT IS DELIBERATE. A condition
//! cannot see the others, so a wait logged from here would be said the first time night fell on a
//! forward-base phase whose ramp was nowhere near finished - which is precisely the distinction the
//! hard-coded gate drew between "not ready" and "not now". The runner is the only thing that knows the
//! hold actually applies, and it says so once per phase entry: see
//! OVT_ObjectiveDirectorComponent.LogIdleClockHold(), which quotes this module's authored name. This
//! module therefore stays completely side-effect free, log latch included.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_DaylightWindowObjectiveCondition : OVT_BaseObjectiveConditionModule
{
	[Attribute(defvalue: "5", desc: "First hour of the day this conjunct is satisfied, INCLUSIVE (0-23). The shipped counter-attack window opens at 05:00. An hour outside 0-23, or a window of zero width, means 'no restriction' rather than 'never'")]
	int m_iStartHour;

	[Attribute(defvalue: "15", desc: "First hour of the day it is no longer satisfied, EXCLUSIVE (0-23), so the shipped 15 means the last minute a battle may start is 14:59. Ending at 15:00 rather than at dusk is what puts the whole battle in daylight without a second check anywhere. start > end is a window that crosses midnight and is honoured as such")]
	int m_iEndHour;

	//! The world clock, resolved on the first tick that needs it. NOT an inherited field: a module is
	//! not a component and has no OnPostInit to fill one in.
	protected TimeAndWeatherManagerEntity m_Clock;

	//------------------------------------------------------------------------------------------------
	//! \return True when the world clock is inside the authored window.
	override bool Evaluate()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		return IsInWindow();
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE ONE EXEMPTION THE DAYLIGHT WAIT ASKS FOR. See the class header.
	//! \return True, always: a night that has not ended is never the objective's fault.
	override bool HoldsIdleClock()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the world clock is inside the window right now.
	//! \return True when it is, and true when there is no clock to read.
	bool IsInWindow()
	{
		int hour = ResolveWorldHour();
		if (hour < 0)
			return true;

		return OVT_ObjectivePhaseRules.IsCounterAttackWindow(hour, m_iStartHour, m_iEndHour);
	}

	//------------------------------------------------------------------------------------------------
	//! The world's hour, 0-23.
	//! \return The hour, or -1 when there is no clock to read.
	protected int ResolveWorldHour()
	{
		if (!m_Clock)
		{
			ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
			if (world)
				m_Clock = world.GetTimeAndWeatherManager();
		}

		if (!m_Clock)
			return -1;

		TimeContainer time = m_Clock.GetTime();
		if (!time)
			return -1;

		return time.m_iHours;
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the condition reads the world clock and writes nothing.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop either hour and the clone reads 0 for it, so the window
	//! becomes 0-0 - a zero-width window, which IsCounterAttackWindow() reads as "no restriction" and
	//! which therefore silently removes the daylight gate from the whole campaign. That is the one
	//! dropped line here with NO symptom at all: battles simply start at night again.
	//!
	//! ⚠ THE CACHED CLOCK HANDLE IS NOT COPIED. A clone is a fresh module for a fresh phase entry and
	//! re-resolves the world clock on its first tick.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_DaylightWindowObjectiveCondition clone = new OVT_DaylightWindowObjectiveCondition();

		clone.m_sModuleName = m_sModuleName;
		clone.m_iStartHour = m_iStartHour;
		clone.m_iEndHour = m_iEndHour;

		return clone;
	}
}
