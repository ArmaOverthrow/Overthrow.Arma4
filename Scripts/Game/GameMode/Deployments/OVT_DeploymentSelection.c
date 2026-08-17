//------------------------------------------------------------------------------------------------
//! Deployment selection - PURE, SYSTEM-FREE arithmetic over names and priorities.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! One question lives here, and it is the question a base fortifying itself keeps asking:
//!
//!     given the configs that are SUITABLE at a place, and the names of the ones that place
//!     ALREADY HOLDS, which one does it buy next?
//!
//! The answer is the lowest-priority config it is still missing, with ties going to the order the
//! caller supplied (which is registry order), and "nothing" when it is missing none of them. That
//! rule IS the escalation contract - see OVT_DeploymentManagerComponent.FindBestDeploymentConfig(),
//! whose header states it in full - and it is written here as a pure function of its arguments so
//! the cheapest test tier can assert it without a world, a manager or a running campaign.
//!
//! WHY THE ALREADY-PRESENT SET IS AN ARGUMENT rather than something this class works out: answering
//! "is one of these standing within 250 m of here" needs the live deployment list, which is exactly
//! the kind of state this file may not see. The caller asks that question per config and hands the
//! answers in.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentSelection
{
	//! What SelectNextConfigIndex() answers when this place has nothing left to buy - no suitable
	//! configs at all, or every suitable one already standing here.
	static const int NOTHING_TO_BUY = -1;

	//------------------------------------------------------------------------------------------------
	//! The index of the config to create next, out of parallel name/priority lists.
	//!
	//! LOWER PRIORITY VALUE WINS, which is the convention m_iPriority has always used, and a strictly
	//! less-than comparison is what keeps a tie on the FIRST entry - so two configs at the same
	//! priority are acquired in the order the registry lists them, one per pass, rather than one of
	//! them being shut out forever.
	//!
	//! RAGGED INPUT IS REFUSED OUTRIGHT rather than clamped to the shorter list. The two lists are
	//! consumed by index and are built side by side by the only caller; a mismatch is a programming
	//! error, and quietly answering out of the first N entries would silently drop configs off the
	//! end of the registry where nobody would ever see it.
	//! \param[in] names Every suitable config's name, in registry order.
	//! \param[in] priorities Each of those configs' m_iPriority, same order, same length.
	//! \param[in] presentNames The names this place already holds. May be null or empty.
	//! \return An index into names, or NOTHING_TO_BUY.
	static int SelectNextConfigIndex(array<string> names, array<int> priorities, array<string> presentNames)
	{
		if (!names || !priorities)
			return NOTHING_TO_BUY;

		if (names.Count() != priorities.Count())
			return NOTHING_TO_BUY;

		int best = NOTHING_TO_BUY;
		int bestPriority = 0;

		int count = names.Count();
		for (int i = 0; i < count; i++)
		{
			if (IsAlreadyPresent(presentNames, names[i]))
				continue;

			// Strictly less-than: the first entry at a given priority keeps the win, so ties resolve
			// to the caller's order.
			if (best == NOTHING_TO_BUY || priorities[i] < bestPriority)
			{
				best = i;
				bestPriority = priorities[i];
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a config name appears in the already-present set.
	//!
	//! A null or empty set means "this place holds nothing", which is the state every position is in
	//! before its first deployment and the state every position stays in for configs that were never
	//! suitable here.
	//! \param[in] presentNames The names this place already holds. May be null.
	//! \param[in] name The config name to look for.
	//! \return True when the place already holds a deployment of that config.
	static bool IsAlreadyPresent(array<string> presentNames, string name)
	{
		if (!presentNames)
			return false;

		foreach (string present : presentNames)
		{
			if (present == name)
				return true;
		}

		return false;
	}
}
