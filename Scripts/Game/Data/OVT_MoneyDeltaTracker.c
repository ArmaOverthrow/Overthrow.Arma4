//------------------------------------------------------------------------------------------------
//! Accumulating "+$500" ticker state for the HUD money readout.
//!
//! DERIVES ITS DELTA BY POLLING, NOT BY SUBSCRIBING. The HUD already polls the player's money every
//! frame and already has a timeSlice, and its lifetime does not obviously match the economy
//! manager's - so feeding it (currentMoney, timeSlice) needs no invoker to register or, more to the
//! point, to unregister. It also keeps the accumulate/reset rule in a class the Logic tier can drive
//! with a hand-written sequence instead of a clock.
//!
//! Semantics:
//!  - The FIRST Update() only seeds the baseline. Opening the HUD is not a transaction, so it must
//!    not flash the player's whole balance as a gain.
//!  - Every change ADDS to the running delta AND restarts the countdown, so two quick buys read as
//!    one combined "-$1,200" rather than racing each other off the screen.
//!  - After m_fResetSeconds with no change at all, the delta clears and the ticker hides.
//------------------------------------------------------------------------------------------------
class OVT_MoneyDeltaTracker : Managed
{
	//! Default seconds of no change before the ticker clears.
	static const float DEFAULT_RESET_SECONDS = 4.0;

	protected float m_fResetSeconds;
	protected int m_iLastMoney;
	protected int m_iDelta;
	protected float m_fTimer;
	protected bool m_bSeeded;

	//------------------------------------------------------------------------------------------------
	void OVT_MoneyDeltaTracker()
	{
		m_fResetSeconds = DEFAULT_RESET_SECONDS;
		Reset();
	}

	//------------------------------------------------------------------------------------------------
	//! Feeds the tracker one observation of the player's money.
	//! \param[in] currentMoney The player's balance right now.
	//! \param[in] timeSlice Seconds since the previous observation.
	void Update(int currentMoney, float timeSlice)
	{
		// First observation: learn the balance, show nothing.
		if (!m_bSeeded)
		{
			m_bSeeded = true;
			m_iLastMoney = currentMoney;
			m_iDelta = 0;
			m_fTimer = 0;
			return;
		}

		if (currentMoney != m_iLastMoney)
		{
			m_iDelta = m_iDelta + (currentMoney - m_iLastMoney);
			m_iLastMoney = currentMoney;
			m_fTimer = m_fResetSeconds;
			return;
		}

		if (m_iDelta == 0)
			return;

		m_fTimer = m_fTimer - timeSlice;

		if (m_fTimer <= 0)
		{
			m_iDelta = 0;
			m_fTimer = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets the baseline and the accumulated delta. The next Update() seeds again.
	void Reset()
	{
		m_iLastMoney = 0;
		m_iDelta = 0;
		m_fTimer = 0;
		m_bSeeded = false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The accumulated change since the ticker last cleared. 0 when nothing is showing.
	int GetDelta()
	{
		return m_iDelta;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the ticker has something to draw.
	bool IsVisible()
	{
		return m_iDelta != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The signed text to draw, e.g. "+$500". Empty while nothing is showing.
	string GetText()
	{
		return OVT_MoneyFormat.FormatDelta(m_iDelta);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds of no change before the ticker clears.
	float GetResetSeconds()
	{
		return m_fResetSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Overrides the clear timeout.
	//! \param[in] seconds Seconds of no change before the ticker clears. Non-positive is ignored.
	void SetResetSeconds(float seconds)
	{
		if (seconds <= 0)
			return;

		m_fResetSeconds = seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds left before the ticker clears. Diagnostic; the HUD does not need it.
	float GetTimeRemaining()
	{
		return m_fTimer;
	}
}
