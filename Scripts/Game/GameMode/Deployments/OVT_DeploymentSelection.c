//------------------------------------------------------------------------------------------------
//! Deployment selection - PURE, SYSTEM-FREE arithmetic over names and priorities.
//!
//! ===========================================================================================
//! HARD RULE: NOTHING IN THIS FILE MAY TOUCH A SYSTEM, THE GAME MODE OR ANY LIVE STATE.
//! ===========================================================================================
//!
//! TWO questions live here. The first is the one a base fortifying itself keeps asking:
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
//!
//! The second question was added 2026-08-19 with the objective RESERVE FLOOR (D18):
//!
//!     given a faction pool, and an amount somebody else has earmarked out of it, how much may the
//!     ROUTINE evaluator spend?
//!
//! Same rule about arguments, for the same reason: who earmarked it, why, and whether they still
//! mean it are questions about a live component this file may not see. The caller looks the number
//! up and hands it in. See SpendableResources().
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

	//------------------------------------------------------------------------------------------------
	// THE OBJECTIVE RESERVE FLOOR - what the ROUTINE evaluator may spend out of an earmarked pool
	//------------------------------------------------------------------------------------------------

	//! "Nothing is earmarked", which is the state of every faction in every campaign that never starts
	//! an objective, and of an occupying faction whose director is not currently short of money.
	static const int NO_RESERVE = 0;

	//------------------------------------------------------------------------------------------------
	//! How much of a faction's deployment pool the ROUTINE evaluator is allowed to spend.
	//!
	//! ================================ THE TWO INVARIANTS ======================================
	//!
	//!  1. NO RESERVE IS BYTE-IDENTICAL TO TODAY. A non-positive reserve returns the caller's own
	//!     integer, having performed no arithmetic on it at all - not pool - 0, not Math.Max(0, pool),
	//!     the same int. That is what makes every faction nobody earmarks for, every campaign with no
	//!     objective and every world that never starts one spend exactly what it spent before this
	//!     existed. It is also why the branch is written as an early return rather than as a clamp
	//!     that happens to be a no-op: "happens to be a no-op" is an argument, and an early return is
	//!     a fact.
	//!
	//!  2. IT NEVER ANSWERS MORE THAN THE POOL AND NEVER ANSWERS LESS THAN NOTHING. A reserve bigger
	//!     than the pool means "this pass buys nothing", not "this pass is in debt": the caller
	//!     subtracts the answer from a real pool, and a negative answer would CREDIT it. That is the
	//!     one way this function could break the conserved-total identity (G5), so it is clamped here
	//!     rather than at the call site.
	//!
	//! ==========================================================================================
	//!
	//! ⚠ IT MOVES NO MONEY AND KNOWS OF NO SECOND POOL. There is exactly one pool
	//! (OVT_DeploymentManagerComponent.m_mFactionResources) and this function does not touch it; it
	//! answers a question about a number. The reserve is a CEILING ON ONE SPENDER, not a wallet, not a
	//! transfer and not a balance - nothing is ever added to it, drawn from it, or carried over.
	//! \param[in] pool What the faction actually holds.
	//! \param[in] reserve What is earmarked out of it. Non-positive means nothing is.
	//! \return What the routine evaluator may spend, in [0, pool].
	static int SpendableResources(int pool, int reserve)
	{
		// INVARIANT 1. No arithmetic, no clamp, no re-derivation - the caller's own int back.
		if (reserve <= NO_RESERVE)
			return pool;

		int spendable = pool - reserve;

		// INVARIANT 2. A reserve larger than the pool is the NORMAL case while a faction saves up for
		// an operation it cannot yet afford, and it must read as "spend nothing".
		if (spendable < 0)
			return 0;

		return spendable;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the routine evaluator may buy something of this price out of an earmarked pool.
	//!
	//! Written out rather than left as an inline `>=` at the one call site because it is the whole
	//! decidable content of the reserve floor - "is this spend allowed given a pool, a cost and a
	//! floor" - and the cheapest test tier can only assert a decision that has a name.
	//! \param[in] pool What the faction actually holds.
	//! \param[in] reserve What is earmarked out of it. Non-positive means nothing is.
	//! \param[in] cost What the evaluator wants to buy.
	//! \return True when the purchase leaves the reserve intact.
	static bool MayEvaluatorSpend(int pool, int reserve, int cost)
	{
		return SpendableResources(pool, reserve) >= cost;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether two reserves are the same earmark, and therefore whether the second one is silenced by
	//! the first.
	//!
	//! ⚠ THE SAME SHAPE AS THE DIRECTOR'S (config, reason) REFUSAL KEY, AND FOR THE SAME REASON. The
	//! owner of an earmark re-pushes it on EVERY one of its ticks for as long as it is short - that
	//! re-push is what stops the floor outliving the intent - so an unlatched announcement would be one
	//! log line every ten real seconds, indefinitely. Keyed on the pair, only a genuinely identical
	//! repeat is silenced: a different operation, or the same operation at a different price, is news
	//! and gets its own line.
	//! \param[in] operationA First earmark's operation name.
	//! \param[in] costA First earmark's cost.
	//! \param[in] operationB Second earmark's operation name.
	//! \param[in] costB Second earmark's cost.
	//! \return True only when both halves match.
	static bool IsSameReserve(string operationA, int costA, string operationB, int costB)
	{
		if (operationA != operationB)
			return false;

		return costA == costB;
	}
}
