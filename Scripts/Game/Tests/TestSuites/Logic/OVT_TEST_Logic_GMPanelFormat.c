//------------------------------------------------------------------------------------------------
//! TIER A case - the Overthrow GM panel's display formatting.
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
