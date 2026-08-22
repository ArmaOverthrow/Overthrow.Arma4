//------------------------------------------------------------------------------------------------
//! TIER A cases - the Overthrow GM panel's display formatting.
//!
//! TWO CASES IN THIS FILE. The first covers the countdown and threat formatters this panel shipped
//! with; the second, added by occupying/counter-attacks Phase 8, covers the two objective rows.
//!
//! OVT_GMPanelFormat is a class of pure statics, and it is the entirety of that panel's automatable
//! surface: everything else the panel does is a text write into a widget inside the Game Master
//! editor, which no suite can open.
//!
//! WHAT IT PINS:
//!  - THE FORMAT SWITCH, at both sides of the hour. 3600 grows an hours field; 3599 must NOT, and an
//!    implementation that compares against 60 minutes with a rounding error switches early on
//!    exactly one of these two values;
//!  - THE SECONDS PAD. Under a minute reads "0:07", not "0:7" and not "7" - the leading zero on the
//!    seconds is what makes a countdown readable as a clock at a glance, and minutes and hours
//!    deliberately do NOT carry one, so "12:34" cannot be mistaken for an hours field;
//!  - THE CLAMP. A negative duration reads "0:00" and never "-0:05". The store clamps at zero, but a
//!    display that renders a minus sign when it is handed one anyway is a bug a GM sees before
//!    anyone reads the store;
//!  - THE THREAT DECIMAL, which is the whole reason this formatter exists rather than an integer
//!    cast: 3.87 must read 3.9, and a whole number must keep its decimal place (3.0, not 3) or the
//!    row visibly changes width every time the threat crosses an integer.
//!
//! The expected strings below are stated by the display specification, never obtained by calling the
//! subject to find out what it does.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GMPanelFormat : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- UNDER A MINUTE. Two-digit seconds, single-digit minutes.
		if (!ExpectCountdown(7, "0:07", "seven seconds"))
			return true;

		if (!ExpectCountdown(59, "0:59", "one second short of a minute"))
			return true;

		// --- MINUTES.
		if (!ExpectCountdown(754, "12:34", "twelve and a half minutes"))
			return true;

		if (!ExpectCountdown(60, "1:00", "exactly one minute"))
			return true;

		// --- THE HOUR BOUNDARY, from both sides.
		if (!ExpectCountdown(3600, "1:00:00", "exactly one hour - the format switches"))
			return true;

		if (!ExpectCountdown(3599, "59:59", "one second short of an hour - it does not switch early"))
			return true;

		if (!ExpectCountdown(5025, "1:23:45", "an hour, twenty-three minutes and forty-five seconds"))
			return true;

		// --- ZERO AND THE CLAMP. A countdown never renders a minus sign.
		if (!ExpectCountdown(0, "0:00", "due now"))
			return true;

		if (!ExpectCountdown(-5, "0:00", "five seconds overdue clamps, never reads -0:05"))
			return true;

		if (!ExpectCountdown(-7200, "0:00", "two hours overdue clamps"))
			return true;

		// --- THREAT PRECISION. One decimal place, always present.
		if (!ExpectThreat(3.87, "3.9", "threat rounds to one decimal"))
			return true;

		if (!ExpectThreat(3.0, "3.0", "a whole threat keeps its decimal place"))
			return true;

		if (!ExpectThreat(0, "0.0", "no threat at all"))
			return true;

		// --- PURITY. The same input gives the same answer, interleaved with a different query so a
		// cached-last-answer implementation cannot pass.
		string firstCall = OVT_GMPanelFormat.FormatCountdown(754);
		OVT_GMPanelFormat.FormatCountdown(3600);
		string secondCall = OVT_GMPanelFormat.FormatCountdown(754);
		if (firstCall != secondCall)
		{
			SetFailure("754 seconds formatted as '%1' and then as '%2'; the formatter holds no state and must be repeatable",
				firstCall, secondCall);
			return true;
		}

		Print("GM panel formatting: countdowns pad seconds to two digits, grow an hours field at exactly 3600 s and not before, clamp negatives to 0:00, and threat always carries one decimal place");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted countdown, naming the case on failure.
	//! \param[in] seconds Duration handed to the formatter.
	//! \param[in] expected String the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectCountdown(float seconds, string expected, string label)
	{
		string actual = OVT_GMPanelFormat.FormatCountdown(seconds);

		if (actual == expected)
			return true;

		SetFailure("%1: %2 seconds formatted as '%3', expected '" + expected + "'",
			label, seconds.ToString(), actual);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted threat value, naming the case on failure.
	//! \param[in] threat Threat handed to the formatter.
	//! \param[in] expected String the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectThreat(float threat, string expected, string label)
	{
		string actual = OVT_GMPanelFormat.FormatThreat(threat);

		if (actual == expected)
			return true;

		SetFailure("%1: threat %2 formatted as '%3', expected '" + expected + "'",
			label, threat.ToString(), actual);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The two objective rows: the target has a name, the phase row carries the AUTHORED plan and phase
//! names, no state renders blank, and "unknown" is not "none".
//!
//! WHY IT IS WORTH A CASE. Both formatters are trivial and both have failure modes that are invisible
//! from anywhere else. The phase used to cross the Game Master wire as an ENUM INTEGER, which meant
//! every phase a Game Master could be shown had to exist as a member and a localization key in the
//! CLIENT'S build: a modded doctrine's phases, authored perfectly, rendered as "Unknown". Build phase 7
//! of occupying/objectives replaced that integer with the two authored names, and these rows are what
//! pin the four answers the replacement has to give.
//!
//! ⚠ THE '#'-KEY ROW IS THE ONE THAT IS NOT OBVIOUS. A '#'-prefixed string is resolved by the widget
//! ONLY when it is the whole string it was handed, so a plan or phase authored as a localization key
//! must never be concatenated with the other half - "Town Offensive: #Some_Key" renders the raw key on
//! a Game Master's screen. The formatter drops the plan half instead, and this case is the only thing
//! that says so.
//!
//! ⚠ AN EMPTY PHASE WITH A PLAN IS "unknown"; EMPTY BOTH IS "none". Reporting a running campaign as
//! having no target is a lie the panel presents with complete confidence, which is the same reason the
//! integer form kept an explicit unknown.
//!
//! ⚠ THE EXPECTED VALUES ARE LOCALIZATION KEYS OR AUTHORED NAMES, NOT ENGLISH. Pinning the KEY rather
//! than the text means a translator changing the wording cannot fail a test; pinning the authored name
//! means a phase name that is passed through is asserted to be passed through VERBATIM, which is the
//! whole point of the wire change.
//!
//! CAN-FAIL, four faults, injected one at a time and compiled. All four exited compile-check 0:
//!   F1. FOLD UNKNOWN INTO NONE - answer PHASE_NONE whenever the phase name is empty. Compiled clean;
//!       the case fails on "a plan that is running with no phase name must say unknown".
//!   F2. RETURN THE NAME UNCONDITIONALLY - drop the empty test in FormatObjectiveName. Compiled clean;
//!       the case fails on "an empty objective name must render as the None key".
//!   F3. CONCATENATE REGARDLESS - drop the IsLocalizationKey() guards. Compiled clean; the case fails
//!       on "a phase name that is a localization key must be handed to the widget ALONE".
//!   F4. DROP THE PLAN HALF - return the phase name unconditionally. Compiled clean; the case fails on
//!       "the phase row names the plan and the phase, in that order".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_GMPanelFormat_ObjectiveRows : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// --- The objective row. A real name passes through untouched; nothing else may.
		if (!ExpectName("Levie", "Levie", "a real objective name is passed through unchanged - it is a proper noun and must not be translated"))
			return true;

		if (!ExpectName("", "#OVT-GMPanel_ObjectiveNone", "an empty objective name must render as the None key, never as an empty row"))
			return true;

		// --- The phase row, as both shipped plans author it: the plan, then the phase.
		if (!ExpectPhase("Town Offensive", "Harassment", "Town Offensive: Harassment", "the phase row names the plan and the phase, in that order"))
			return true;

		if (!ExpectPhase("Base Offensive", "CounterAttack", "Base Offensive: CounterAttack", "an authored phase name is displayed VERBATIM - this build has no table of phase names and needs none"))
			return true;

		// --- A plan with no name of its own still shows its phase rather than nothing.
		if (!ExpectPhase("", "Harassment", "Harassment", "a phase with no plan name beside it is still worth showing on its own"))
			return true;

		// --- The two empty states, which mean different things.
		if (!ExpectPhase("", "", "#OVT-GMPanel_ObjectivePhaseNone", "no plan and no phase is the ordinary no-objective state"))
			return true;

		if (!ExpectPhase("Town Offensive", "", "#OVT-GMPanel_ObjectivePhaseUnknown", "a plan that is running with no phase name must say unknown, never that the campaign has no target"))
			return true;

		// --- A name authored AS a localization key. Resolvable only when it is the whole string.
		if (!ExpectPhase("Town Offensive", "#MyMod_PhaseName", "#MyMod_PhaseName", "a phase name that is a localization key must be handed to the widget ALONE, or it renders as raw key text"))
			return true;

		if (!ExpectPhase("#MyMod_PlanName", "Harassment", "Harassment", "a plan name that is a localization key cannot be prefixed onto the phase either - the phase alone is the readable answer"))
			return true;

		Print("GM panel: the objective row names the target, the phase row names the authored plan and phase, an absent objective says so explicitly, a running plan with no phase reads as unknown rather than none, and a name authored as a localization key is never concatenated with anything");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted objective name.
	//! \param[in] name The name handed to the formatter.
	//! \param[in] expected The string the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectName(string name, string expected, string label)
	{
		string actual = OVT_GMPanelFormat.FormatObjectiveName(name);

		if (actual == expected)
			return true;

		SetFailure("%1: got '%2', expected '%3'", label, actual, expected);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Asserts one formatted phase row.
	//! \param[in] planName The plan name handed to the formatter.
	//! \param[in] phaseName The phase name handed to the formatter.
	//! \param[in] expected The string the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectPhase(string planName, string phaseName, string expected, string label)
	{
		string actual = OVT_GMPanelFormat.FormatPhaseRow(planName, phaseName);

		if (actual == expected)
			return true;

		SetFailure("%1: plan '" + planName + "' phase '" + phaseName + "' formatted as '%2', expected '%3'", label, actual, expected);

		return false;
	}
}
