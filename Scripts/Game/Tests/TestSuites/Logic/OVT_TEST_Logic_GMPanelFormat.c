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
//! The two objective rows: every phase is named, no state renders blank, and "unknown" is not "none".
//!
//! WHY IT IS WORTH A CASE. Both formatters are trivial and both have one failure mode that is invisible
//! from anywhere else: a phase this build has never heard of. The phase crosses the Game Master wire as
//! an INTEGER, and the two ends of that wire can be different builds. A formatter that answered "None"
//! for an unrecognised value would tell a Game Master the occupying faction has no target while it is
//! actively assaulting a town - a lie the panel presents with complete confidence.
//!
//! ⚠ THE EXPECTED VALUES ARE LOCALIZATION KEYS, NOT ENGLISH. That is the point: the formatter must
//! never build a display sentence, and pinning the KEY rather than the text means a translator changing
//! the wording cannot fail a test. The keys are re-stated here as literals rather than read off the
//! subject, because reading the subject's own constants back would assert nothing at all.
//!
//! ⚠ THE PHASE INTEGERS ARE ALSO LITERALS, and deliberately. They are a wire format that may never be
//! renumbered; writing 3 here rather than naming the enum member means a renumbering breaks this case
//! instead of silently travelling with it.
//!
//! CAN-FAIL, three faults, injected one at a time and compiled. All three exited compile-check 0:
//!   F1. FOLD UNKNOWN INTO NONE - `return PHASE_NONE;` as the final fallthrough. Compiled clean (exit
//!       0). Every shipped row passes. The case fails on "a phase this build does not know must say so
//!       rather than claiming the campaign has no objective".
//!   F2. RETURN THE NAME UNCONDITIONALLY - drop the empty test in FormatObjectiveName. Compiled clean
//!       (exit 0). The case fails on "an empty objective name must render as the None key, never as an
//!       empty row".
//!   F3. SHIFT THE PHASE TABLE BY ONE - test HARASSMENT against 2, FOB against 3 and so on. Compiled
//!       clean (exit 0). The case fails on "phase 1 is harassment".
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

		// --- The phase row, every shipped value, against the INTEGERS that cross the wire.
		if (!ExpectPhase(0, "#OVT-GMPanel_ObjectivePhaseNone", "phase 0 is idle, which means there is no objective"))
			return true;

		if (!ExpectPhase(1, "#OVT-GMPanel_ObjectivePhaseHarassment", "phase 1 is harassment"))
			return true;

		if (!ExpectPhase(2, "#OVT-GMPanel_ObjectivePhaseForwardBase", "phase 2 is the forward operating base"))
			return true;

		if (!ExpectPhase(3, "#OVT-GMPanel_ObjectivePhaseCounterAttack", "phase 3 is the counter-attack"))
			return true;

		// --- And the value nobody has shipped yet. Both directions: a future phase and a nonsense one.
		if (!ExpectPhase(4, "#OVT-GMPanel_ObjectivePhaseUnknown", "a phase this build does not know must say so rather than claiming the campaign has no objective"))
			return true;

		if (!ExpectPhase(-1, "#OVT-GMPanel_ObjectivePhaseUnknown", "a negative phase is not idle either - it is a wire fault and must read as unknown"))
			return true;

		Print("GM panel: every campaign phase has a name, an absent objective says so explicitly, and a phase from a build this one does not recognise reads as unknown rather than as no objective at all");

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
	//! Asserts one formatted phase.
	//! \param[in] phase The phase integer handed to the formatter.
	//! \param[in] expected The key the display specification requires.
	//! \param[in] label Human description, used only in the failure message.
	//! \return True when it matched; false after recording the failure.
	protected bool ExpectPhase(int phase, string expected, string label)
	{
		string actual = OVT_GMPanelFormat.FormatObjectivePhase(phase);

		if (actual == expected)
			return true;

		SetFailure("%1: phase " + phase.ToString() + " formatted as '%2', expected '%3'", label, actual, expected);

		return false;
	}
}
