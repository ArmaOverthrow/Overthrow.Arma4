//------------------------------------------------------------------------------------------------
//! One player's in-flight storage operation, and everything the engine needs to resume it after a
//! chunk boundary.
//!
//! EVERY FIELD IS PER-JOB. Nothing here is static and nothing is shared between players: the
//! singleton search accumulator on OVT_InventoryManagerComponent (:497) is the concurrency defect
//! this engine exists to replace, so reproducing its shape here would reproduce its bug.
//!
//! WORK IS ADDRESSED BY EntityID, NEVER BY IEntity. A sweep deletes the entities it enumerated and
//! anything else may delete them between two chunks; an EntityID that no longer resolves is a skip,
//! a dangling IEntity is a crash.
//------------------------------------------------------------------------------------------------
class OVT_StorageJob : Managed
{
	//! Who asked. Re-checked at every chunk: a job outlives its owner's disconnect by at most one.
	int m_iPlayerId;

	//! The checkout sequence this job answers, or OVT_StorageRequestComponent.SEQ_NONE for the
	//! single-shot verbs, which report through the progress channel instead.
	int m_iSeq;

	EOVT_StorageOp m_eOp;

	RplId m_SourceId;

	//! Equal to m_SourceId for every op but TO_HOLDER.
	RplId m_DestId;

	//! Parallel with m_aQty. Drained from the FRONT with RemoveOrdered - the client streamed them in
	//! cart order and a swap-with-last removal would silently reorder what the player asked for.
	ref array<string> m_aRes;

	ref array<int> m_aQty;

	//! Entity work list for the sweep, the clear and the loot, in the order they will be processed.
	//! COLLECT re-fills it per container rather than once per job.
	ref array<EntityID> m_aPending;

	//! Index into m_aPending. The line ops do not use it; they drain m_aRes instead.
	int m_iCursor;

	//! COLLECT only: the containers still to be drained into m_DestId, in the order they were found.
	ref array<EntityID> m_aHolders;

	//! COLLECT only: index into m_aHolders.
	int m_iHolderCursor;

	//! COLLECT only: whether m_aPending already holds the current container's work list.
	bool m_bHolderOpened;

	int m_iMoved;

	int m_iShortfall;

	int m_iEarned;

	//! Denominator for the progress bar, fixed when the job starts.
	int m_iTotalUnits;

	//! LOOT only: how far around the holder bodies and weapons are collected from.
	float m_fRadius;

	//! Localization key the progress HUD shows for the whole job.
	string m_sProgressKey;

	//! Started when this one finishes. sweepFirst chains TO_STORAGE -> TO_HOLDER; nothing else chains.
	ref OVT_StorageJob m_NextJob;

	//! Set when a streamed checkout arrives malformed. The refusal is deferred to Commit so that one
	//! bad line produces ONE refusal for the order rather than one per line.
	bool m_bMalformed;

	//------------------------------------------------------------------------------------------------
	void OVT_StorageJob()
	{
		m_aRes = new array<string>();
		m_aQty = new array<int>();
		m_aPending = new array<EntityID>();
		m_aHolders = new array<EntityID>();
		m_SourceId = RplId.Invalid();
		m_DestId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one cart line.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many are asked for.
	void AddLine(string res, int qty)
	{
		m_aRes.Insert(res);
		m_aQty.Insert(qty);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many cart lines are left.
	int LineCount()
	{
		return m_aRes.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the line currently being worked on, keeping the rest in the order the client sent them.
	//! RemoveOrdered, never Remove: array.Remove() is swap-with-last.
	void DropFrontLine()
	{
		if (m_aRes.IsEmpty())
			return;

		m_aRes.RemoveOrdered(0);
		m_aQty.RemoveOrdered(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Units still asked for across every remaining line. What an abort owes the shortfall.
	//! \return The sum of the outstanding quantities.
	int RemainingUnits()
	{
		int remaining = 0;
		foreach (int qty : m_aQty)
		{
			if (qty > 0)
				remaining += qty;
		}

		return remaining;
	}

	//------------------------------------------------------------------------------------------------
	//! Entity work items not yet reached. What an abort owes the shortfall.
	//! \return The count still ahead of the cursor.
	int RemainingPending()
	{
		int remaining = m_aPending.Count() - m_iCursor;
		if (remaining < 0)
			return 0;

		return remaining;
	}

	//------------------------------------------------------------------------------------------------
	//! How far along the job is, for the progress HUD.
	//! \return 0..1; 1 when the job has nothing to do at all.
	float Progress()
	{
		if (m_iTotalUnits <= 0)
			return 1;

		float done = Processed();

		return Math.Clamp(done / (float)m_iTotalUnits, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! How many units have been accounted for.
	//!
	//! The entity ops count the CURSOR rather than moved + shortfall, because a work item that is
	//! deliberately left alone (a part-used magazine, a container that still holds something) is
	//! neither, and the bar would stall short of the end without it.
	//! \return Work items or units already dealt with.
	int Processed()
	{
		// COLLECT counts CONTAINERS: m_aPending is refilled per container, so its cursor would run
		// back to zero every time the job moved on to the next one.
		if (m_eOp == EOVT_StorageOp.COLLECT)
			return m_iHolderCursor;

		if (!m_aPending.IsEmpty())
			return m_iCursor;

		return m_iMoved + m_iShortfall;
	}

	//------------------------------------------------------------------------------------------------
	//! COLLECT: moves on to the next container, dropping the work list of the one just drained.
	void AdvanceHolder()
	{
		m_iHolderCursor++;
		m_iCursor = 0;
		m_bHolderOpened = false;
		m_aPending.Clear();
	}
}
