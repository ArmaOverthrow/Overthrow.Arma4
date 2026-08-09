//------------------------------------------------------------------------------------------------
//! TIER A cases - the tutorial framework's pure decisions.
//!
//! The tutorial pipeline is: an event happens, the matcher picks which entries it fires, the queue
//! decides which of those is shown next, the gate decides whether anything may be shown at all, and
//! the seen store decides whether it should ever be shown again. Every one of those five decisions
//! is a function of plain values, which is why they live in Scripts/Game/Data/ and Configuration/
//! rather than inside the manager and the HUD component (implementation plan, D-section).
//!
//! Every subject below is built with `new` and fed hand-written values. Nothing here touches a
//! component, a manager, a widget or the world - and per this directory's tier rule, neither does
//! any comment in this file name the accessors that would reach one.
//!
//! Two claims are worth calling out because they are the ones that would fail silently in play:
//!  - THE MATCHER RETURNS AN ENTRY ONCE, however many of its triggers matched. An entry declares
//!    several triggers as ALTERNATIVES; a naive per-trigger loop shows the same tip twice.
//!  - THE QUEUE DROPS AT THE CAP RATHER THAN EVICTING. Evicting the head would make which tip a
//!    player sees depend on arrival timing, and would silently discard something already waiting.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_TutorialTrigger.Matches() - event, then the optional threshold, then the optional filter.
//!
//! The two sentinels are the whole risk surface here. `m_iMinValue == 0` means NO THRESHOLD, not a
//! threshold of zero, and `m_sFilter == ""` means NO FILTER, not "the filter must be empty". A
//! hand-built trigger starts with exactly those two values (`new` applies no attribute defvalues),
//! so getting the sentinels wrong would make every default trigger either always-on or never-on.
//!
//! The threshold is INCLUSIVE - a tip bound to "spend at least 500" must fire on a purchase of
//! exactly 500 - and the filter is EXACT and CASE-SENSITIVE, because the strings it compares are
//! context class names and config keys where case carries meaning.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Tutorial_TriggerMatching : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// --- BARE TRIGGER: event alone decides, because both sentinels are at their "unset" value.
		OVT_TutorialTrigger bare = MakeTrigger(OVT_TutorialEvent.PLAYER_BUY, 0, "");

		if (!bare.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 0, "")))
		{
			SetResultFailure("A trigger with no threshold and no filter refused its own event");
			return true;
		}

		// A payload it declared no interest in must not change its mind either way.
		if (!bare.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 99999, "anything at all")))
		{
			SetResultFailure("A trigger with no threshold and no filter refused its own event when the event carried a payload");
			return true;
		}

		if (bare.Matches(MakeContext(OVT_TutorialEvent.PLAYER_SELL, 0, "")))
		{
			SetResultFailure("A PLAYER_BUY trigger matched a PLAYER_SELL event");
			return true;
		}

		// A null occurrence is not a match and is not a crash: the client-local hooks can produce one.
		if (bare.Matches(null))
		{
			SetResultFailure("A trigger matched a null event context");
			return true;
		}

		// --- THRESHOLD: rejects below, ACCEPTS EQUAL, accepts above.
		OVT_TutorialTrigger threshold = MakeTrigger(OVT_TutorialEvent.PLAYER_BUY, 500, "");

		if (threshold.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 499, "")))
		{
			SetResultFailure("A minimum of 500 matched a value of 499");
			return true;
		}

		if (!threshold.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 500, "")))
		{
			SetResultFailure("A minimum of 500 rejected a value of exactly 500; the threshold must be inclusive");
			return true;
		}

		if (!threshold.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 501, "")))
		{
			SetResultFailure("A minimum of 500 rejected a value of 501");
			return true;
		}

		if (threshold.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 0, "")))
		{
			SetResultFailure("A minimum of 500 matched an event carrying no value at all");
			return true;
		}

		// --- FILTER: exact, and case-sensitive.
		OVT_TutorialTrigger filtered = MakeTrigger(OVT_TutorialEvent.PLAYER_PLACE, 0, "OVT_Camp");

		if (!filtered.Matches(MakeContext(OVT_TutorialEvent.PLAYER_PLACE, 0, "OVT_Camp")))
		{
			SetResultFailure("A filter of 'OVT_Camp' rejected the exact string 'OVT_Camp'");
			return true;
		}

		if (filtered.Matches(MakeContext(OVT_TutorialEvent.PLAYER_PLACE, 0, "ovt_camp")))
		{
			SetResultFailure("A filter of 'OVT_Camp' matched 'ovt_camp'; the comparison must be case-sensitive");
			return true;
		}

		if (filtered.Matches(MakeContext(OVT_TutorialEvent.PLAYER_PLACE, 0, "OVT_CampSmall")))
		{
			SetResultFailure("A filter of 'OVT_Camp' matched 'OVT_CampSmall'; the comparison must be exact, not a prefix");
			return true;
		}

		if (filtered.Matches(MakeContext(OVT_TutorialEvent.PLAYER_PLACE, 0, "")))
		{
			SetResultFailure("A filter of 'OVT_Camp' matched an event carrying no filter");
			return true;
		}

		// A trigger with NO filter accepts an event that happens to carry one - the empty string is
		// the "do not care" sentinel, not a value to be matched against.
		OVT_TutorialTrigger unfiltered = MakeTrigger(OVT_TutorialEvent.PLAYER_PLACE, 0, "");

		if (!unfiltered.Matches(MakeContext(OVT_TutorialEvent.PLAYER_PLACE, 0, "OVT_Camp")))
		{
			SetResultFailure("A trigger with no filter rejected an event that carried one");
			return true;
		}

		// --- THE WRONG EVENT NEVER MATCHES, whatever the other two fields would have said.
		OVT_TutorialTrigger both = MakeTrigger(OVT_TutorialEvent.PLAYER_TRANSACTION, 100, "SHOP_GENERAL");

		if (!both.Matches(MakeContext(OVT_TutorialEvent.PLAYER_TRANSACTION, 100, "SHOP_GENERAL")))
		{
			SetResultFailure("A trigger with both a threshold and a filter rejected an event satisfying both");
			return true;
		}

		if (both.Matches(MakeContext(OVT_TutorialEvent.PLAYER_BUY, 100, "SHOP_GENERAL")))
		{
			SetResultFailure("A PLAYER_TRANSACTION trigger matched a PLAYER_BUY event that satisfied its threshold and filter");
			return true;
		}

		if (both.Matches(MakeContext(OVT_TutorialEvent.PLAYER_TRANSACTION, 100, "SHOP_GUNDEALER")))
		{
			SetResultFailure("A trigger with both conditions matched when only the threshold was satisfied");
			return true;
		}

		if (both.Matches(MakeContext(OVT_TutorialEvent.PLAYER_TRANSACTION, 99, "SHOP_GENERAL")))
		{
			SetResultFailure("A trigger with both conditions matched when only the filter was satisfied");
			return true;
		}

		Print("Tutorial triggers: 0 means no threshold and \"\" means no filter, a real threshold accepts an equal value, a real filter is exact and case-sensitive, and the wrong event never matches");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one trigger with every field set explicitly - `new` applies no attribute defvalues.
	//! \param[in] evt Event the trigger listens for.
	//! \param[in] minValue Threshold, or 0 for none.
	//! \param[in] filter Exact filter, or "" for none.
	//! \return The trigger.
	protected OVT_TutorialTrigger MakeTrigger(OVT_TutorialEvent evt, int minValue, string filter)
	{
		OVT_TutorialTrigger trigger = new OVT_TutorialTrigger();
		trigger.m_eEvent = evt;
		trigger.m_iMinValue = minValue;
		trigger.m_sFilter = filter;
		return trigger;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one event occurrence with every field set explicitly.
	//! \param[in] evt The event that occurred.
	//! \param[in] value Numeric payload.
	//! \param[in] filter String payload.
	//! \return The occurrence.
	protected OVT_TutorialEventContext MakeContext(OVT_TutorialEvent evt, int value, string filter)
	{
		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = evt;
		ctx.m_iPlayerId = 1;
		ctx.m_iValue = value;
		ctx.m_sFilter = filter;
		return ctx;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TutorialMatcher.FindMatches() - which entries an occurrence fires, and in what order.
//!
//! Four claims, each of which is a real defect if it goes the other way:
//!  - DISABLED ENTRIES NEVER FIRE. m_bEnabled 0 is the documented retirement path, and an id is
//!    never reused, so a retired entry that still fired would be unretirable.
//!  - AN ENTRY WITH TWO MATCHING TRIGGERS IS RETURNED ONCE. Triggers are alternatives.
//!  - ORDER IS PRIORITY-DESCENDING, THEN DECLARATION ORDER. The second half is what makes two
//!    entries at the default priority of 0 show in an author-controlled sequence.
//!  - NO MATCH IS AN EMPTY ARRAY, NEVER NULL. Every caller iterates the result immediately.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Tutorial_MatcherSelectsAndOrders : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TutorialEntryConfig> entries = new array<ref OVT_TutorialEntryConfig>();

		// Declaration order below IS the tie-break order the assertions depend on.
		// "low-a" and "low-b" share priority 0; "high" outranks both; "mid" sits between.
		entries.Insert(MakeEntry("low-a", 0, true, OVT_TutorialEvent.PLAYER_BUY, 0, ""));
		entries.Insert(MakeEntry("high", 100, true, OVT_TutorialEvent.PLAYER_BUY, 0, ""));
		entries.Insert(MakeEntry("low-b", 0, true, OVT_TutorialEvent.PLAYER_BUY, 0, ""));
		entries.Insert(MakeEntry("mid", 50, true, OVT_TutorialEvent.PLAYER_BUY, 0, ""));

		// Never fires: retired.
		entries.Insert(MakeEntry("retired", 1000, false, OVT_TutorialEvent.PLAYER_BUY, 0, ""));

		// Never fires for a buy: different event entirely.
		entries.Insert(MakeEntry("other-event", 1000, true, OVT_TutorialEvent.MAP_OPENED, 0, ""));

		// Never fires for a small buy: its threshold is not met.
		entries.Insert(MakeEntry("expensive", 1000, true, OVT_TutorialEvent.PLAYER_BUY, 5000, ""));

		// TWO triggers, BOTH of which match a plain buy. Must still be returned exactly once.
		OVT_TutorialEntryConfig twin = MakeEntry("twin", 25, true, OVT_TutorialEvent.PLAYER_BUY, 0, "");
		twin.m_aTriggers.Insert(MakeTrigger(OVT_TutorialEvent.PLAYER_BUY, 10, ""));
		entries.Insert(twin);

		array<string> ids = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_BUY, 100, ""), ids);

		array<string> expected = {"high", "mid", "twin", "low-a", "low-b"};

		if (ids.Count() != expected.Count())
		{
			SetResultFailure("FindMatches returned %1, expected %2",
				JoinIds(ids), JoinIds(expected));
			return true;
		}

		for (int i = 0; i < expected.Count(); i++)
		{
			if (ids.Get(i) != expected.Get(i))
			{
				SetResultFailure("FindMatches came out as %1, expected %2 (first difference at position %3)",
					JoinIds(ids), JoinIds(expected), i.ToString());
				return true;
			}
		}

		// --- THE RESULT BUFFER IS CLEARED, not appended to, on every call.
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_BUY, 100, ""), ids);

		if (ids.Count() != expected.Count())
		{
			SetResultFailure("A second FindMatches into the same buffer returned %1 ids, expected %2; the buffer was not cleared",
				ids.Count().ToString(), expected.Count().ToString());
			return true;
		}

		// --- NO MATCH: an empty array, never null.
		array<string> none = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_SPAWNED, 0, ""), none);

		if (!none)
		{
			SetResultFailure("FindMatches nulled the result array when nothing matched");
			return true;
		}

		if (none.Count() != 0)
		{
			SetResultFailure("An event nothing is bound to matched %1 entries (%2), expected none",
				none.Count().ToString(), JoinIds(none));
			return true;
		}

		// --- THE THRESHOLD ENTRY DOES FIRE once the event is big enough, which is what proves the
		// exclusion above was the threshold and not the entry being invisible to the matcher.
		array<string> big = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_BUY, 5000, ""), big);

		if (!big.Contains("expensive"))
		{
			SetResultFailure("A 5000 buy did not fire the entry with a 5000 threshold. Result: %1", JoinIds(big));
			return true;
		}

		if (big.Get(0) != "expensive")
		{
			SetResultFailure("The priority-1000 entry came out at position 0 as '%1', expected 'expensive'", big.Get(0));
			return true;
		}

		if (big.Contains("retired"))
		{
			SetResultFailure("A retired (m_bEnabled 0) entry fired");
			return true;
		}

		// --- DEGENERATE INPUTS are answered, not crashed on.
		array<string> nullEntries = new array<string>();
		OVT_TutorialMatcher.FindMatches(null, MakeContext(OVT_TutorialEvent.PLAYER_BUY, 1, ""), nullEntries);

		if (!nullEntries || nullEntries.Count() != 0)
		{
			SetResultFailure("A null entry list did not produce an empty result");
			return true;
		}

		array<string> nullCtx = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, null, nullCtx);

		if (!nullCtx || nullCtx.Count() != 0)
		{
			SetResultFailure("A null event context did not produce an empty result");
			return true;
		}

		Print("Tutorial matcher: enabled entries with any matching trigger, each returned exactly once, ordered by descending priority then declaration order, and no match gives an empty array");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one entry carrying exactly one trigger. Every field is set explicitly.
	//! \param[in] id Entry id.
	//! \param[in] priority Queue priority.
	//! \param[in] enabled Whether the entry may fire.
	//! \param[in] evt Event the single trigger listens for.
	//! \param[in] minValue Trigger threshold, or 0 for none.
	//! \param[in] filter Trigger filter, or "" for none.
	//! \return The entry.
	protected OVT_TutorialEntryConfig MakeEntry(string id, int priority, bool enabled, OVT_TutorialEvent evt, int minValue, string filter)
	{
		OVT_TutorialEntryConfig entry = new OVT_TutorialEntryConfig();
		entry.m_sId = id;
		entry.m_sTitle = "#OVT-Tutorial_Test_Title";
		entry.m_ePresentation = OVT_TutorialPresentation.NONMODAL;
		entry.m_iPriority = priority;
		entry.m_sFieldManualTitleKey = "";
		entry.m_bEnabled = enabled;

		entry.m_aPages = new array<ref OVT_TutorialPage>();
		OVT_TutorialPage page = new OVT_TutorialPage();
		page.m_sBody = "#OVT-Tutorial_Test_Body";
		page.m_sImage = ResourceName.Empty;
		entry.m_aPages.Insert(page);

		entry.m_aTriggers = new array<ref OVT_TutorialTrigger>();
		entry.m_aTriggers.Insert(MakeTrigger(evt, minValue, filter));

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] evt Event the trigger listens for.
	//! \param[in] minValue Threshold, or 0 for none.
	//! \param[in] filter Exact filter, or "" for none.
	//! \return The trigger.
	protected OVT_TutorialTrigger MakeTrigger(OVT_TutorialEvent evt, int minValue, string filter)
	{
		OVT_TutorialTrigger trigger = new OVT_TutorialTrigger();
		trigger.m_eEvent = evt;
		trigger.m_iMinValue = minValue;
		trigger.m_sFilter = filter;
		return trigger;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] evt The event that occurred.
	//! \param[in] value Numeric payload.
	//! \param[in] filter String payload.
	//! \return The occurrence.
	protected OVT_TutorialEventContext MakeContext(OVT_TutorialEvent evt, int value, string filter)
	{
		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = evt;
		ctx.m_iPlayerId = 1;
		ctx.m_iValue = value;
		ctx.m_sFilter = filter;
		return ctx;
	}

	//------------------------------------------------------------------------------------------------
	//! Renders an id list for a failure message, so a wrong order is readable at a glance.
	//! \param[in] ids The ids.
	//! \return A bracketed, comma-separated rendering.
	protected string JoinIds(array<string> ids)
	{
		if (!ids)
			return "<null>";

		string text = "[";

		for (int i = 0; i < ids.Count(); i++)
		{
			if (i > 0)
				text += ", ";

			text += ids.Get(i);
		}

		return text + "]";
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TutorialQueue - priority out, FIFO within a priority, duplicate-proof, and capped.
//!
//! The queue exists because a trigger can fire while a menu is open, while another popup is up or
//! while the player is dead, and the requirement is that the tip appears AFTER the blocker clears
//! rather than never. Everything below is about what happens to the backlog in the meantime.
//!
//! The cap claim is the one with teeth: an enqueue past MAX_QUEUE is DROPPED AND COUNTED, and the
//! item that was already waiting stays. Evicting to make room would make which tip a player sees
//! depend on arrival timing, and would discard something that has already earned its place.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Tutorial_QueueOrdering : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TutorialQueue queue = new OVT_TutorialQueue();

		// --- AN EMPTY QUEUE WRITES NOTHING TO THE OUT PARAM. The sentinel below must survive, or a
		// caller that ignores the return value would show a stale tip.
		string dequeued = "SENTINEL";

		if (queue.TryDequeue(dequeued))
		{
			SetResultFailure("TryDequeue on an empty queue reported success");
			return true;
		}

		if (dequeued != "SENTINEL")
		{
			SetResultFailure("TryDequeue on an empty queue overwrote the out param with '%1'", dequeued);
			return true;
		}

		if (queue.Count() != 0)
		{
			SetResultFailure("A fresh queue reported %1 entries, expected 0", queue.Count().ToString());
			return true;
		}

		// --- ORDER: two priority bands, arrival order preserved inside each.
		queue.Enqueue("low-first", 0);
		queue.Enqueue("high-first", 10);
		queue.Enqueue("low-second", 0);
		queue.Enqueue("high-second", 10);
		queue.Enqueue("mid", 5);

		if (queue.Count() != 5)
		{
			SetResultFailure("After five distinct enqueues the queue held %1 entries, expected 5", queue.Count().ToString());
			return true;
		}

		if (!queue.Contains("mid"))
		{
			SetResultFailure("Contains() did not find an id that had just been enqueued");
			return true;
		}

		if (queue.Contains("never-enqueued"))
		{
			SetResultFailure("Contains() found an id that was never enqueued");
			return true;
		}

		array<string> expected = {"high-first", "high-second", "mid", "low-first", "low-second"};

		for (int i = 0; i < expected.Count(); i++)
		{
			if (!queue.TryDequeue(dequeued))
			{
				SetResultFailure("TryDequeue failed at position %1 with %2 entries still expected",
					i.ToString(), (expected.Count() - i).ToString());
				return true;
			}

			if (dequeued != expected.Get(i))
			{
				SetResultFailure("Dequeue %1 gave '%2', expected '%3' (priority first, then arrival order within a priority)",
					i.ToString(), dequeued, expected.Get(i));
				return true;
			}
		}

		if (queue.Count() != 0)
		{
			SetResultFailure("After draining, the queue reported %1 entries, expected 0", queue.Count().ToString());
			return true;
		}

		// --- DUPLICATES ARE A NO-OP, at any priority. The second call must not re-queue the id and
		// must not move it: a repeated trigger cannot promote a tip past one already waiting.
		OVT_TutorialQueue dupes = new OVT_TutorialQueue();
		dupes.Enqueue("a", 0);
		dupes.Enqueue("b", 0);

		if (dupes.Enqueue("a", 0))
		{
			SetResultFailure("Enqueueing an already-queued id reported success");
			return true;
		}

		if (dupes.Enqueue("a", 999))
		{
			SetResultFailure("Enqueueing an already-queued id at a higher priority reported success");
			return true;
		}

		if (dupes.Count() != 2)
		{
			SetResultFailure("After two duplicate enqueues the queue held %1 entries, expected 2", dupes.Count().ToString());
			return true;
		}

		dupes.TryDequeue(dequeued);
		if (dequeued != "a")
		{
			SetResultFailure("The duplicate enqueue reordered the queue: head is '%1', expected 'a'", dequeued);
			return true;
		}

		// An empty id is refused outright rather than queued as a blank tip.
		if (dupes.Enqueue("", 0))
		{
			SetResultFailure("Enqueueing an empty id reported success");
			return true;
		}

		// --- THE CAP: fill it exactly, then push past it.
		OVT_TutorialQueue capped = new OVT_TutorialQueue();

		for (int f = 0; f < OVT_TutorialQueue.MAX_QUEUE; f++)
		{
			if (!capped.Enqueue("fill-" + f.ToString(), 0))
			{
				SetResultFailure("Filling to the cap failed at entry %1 of %2",
					f.ToString(), OVT_TutorialQueue.MAX_QUEUE.ToString());
				return true;
			}
		}

		if (capped.Count() != OVT_TutorialQueue.MAX_QUEUE)
		{
			SetResultFailure("A queue filled to the cap holds %1 entries, expected %2",
				capped.Count().ToString(), OVT_TutorialQueue.MAX_QUEUE.ToString());
			return true;
		}

		if (capped.GetDroppedCount() != 0)
		{
			SetResultFailure("Filling exactly to the cap already counted %1 drops, expected 0", capped.GetDroppedCount().ToString());
			return true;
		}

		// A high priority does not buy a way past the cap - it would otherwise evict by the back door.
		if (capped.Enqueue("overflow", 9999))
		{
			SetResultFailure("An enqueue past the cap reported success");
			return true;
		}

		if (capped.Count() != OVT_TutorialQueue.MAX_QUEUE)
		{
			SetResultFailure("After an over-cap enqueue the queue holds %1 entries, expected %2",
				capped.Count().ToString(), OVT_TutorialQueue.MAX_QUEUE.ToString());
			return true;
		}

		if (capped.Contains("overflow"))
		{
			SetResultFailure("The over-cap entry got into the queue anyway");
			return true;
		}

		if (!capped.Contains("fill-0"))
		{
			SetResultFailure("The over-cap enqueue displaced the entry that was already at the head");
			return true;
		}

		if (capped.GetDroppedCount() != 1)
		{
			SetResultFailure("One over-cap enqueue counted %1 drops, expected 1", capped.GetDroppedCount().ToString());
			return true;
		}

		capped.Enqueue("overflow-2", 0);

		if (capped.GetDroppedCount() != 2)
		{
			SetResultFailure("Two over-cap enqueues counted %1 drops, expected 2", capped.GetDroppedCount().ToString());
			return true;
		}

		// The head is still the FIRST thing that was queued, untouched by either overflow.
		capped.TryDequeue(dequeued);
		if (dequeued != "fill-0")
		{
			SetResultFailure("After two over-cap enqueues the head is '%1', expected the original 'fill-0'", dequeued);
			return true;
		}

		// --- CLEAR empties the queue and keeps the diagnostic.
		capped.Clear();

		if (capped.Count() != 0)
		{
			SetResultFailure("Clear() left %1 entries", capped.Count().ToString());
			return true;
		}

		if (capped.GetDroppedCount() != 2)
		{
			SetResultFailure("Clear() reset the drop count to %1; it is a session diagnostic, not queue state",
				capped.GetDroppedCount().ToString());
			return true;
		}

		Print("Tutorial queue: highest priority out first with FIFO inside a priority band, duplicates are a no-op, an empty dequeue writes nothing, and over-cap enqueues are dropped and counted without displacing anything");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TutorialGate.CanShowNow() - the four vetoes, each asserted in isolation.
//!
//! Isolation is the point of this case. A gate written as `return !tipsDisabled` with the rest
//! forgotten still passes an "all clear is true" check and still passes a "everything wrong is
//! false" check; only flipping ONE input at a time catches a missing clause.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Tutorial_GatePredicate : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		// Argument order: tipsDisabled, alreadyShowing, blockingUiOpen, playerAlive.

		// --- ALL CLEAR is the only true.
		if (!OVT_TutorialGate.CanShowNow(false, false, false, true))
		{
			SetResultFailure("The gate refused a popup with tips enabled, nothing showing, no blocking UI and the player alive");
			return true;
		}

		// --- EACH VETO ALONE, with the other three clear.
		if (OVT_TutorialGate.CanShowNow(true, false, false, true))
		{
			SetResultFailure("The gate allowed a popup while tips are disabled");
			return true;
		}

		if (OVT_TutorialGate.CanShowNow(false, true, false, true))
		{
			SetResultFailure("The gate allowed a second popup while one is already showing");
			return true;
		}

		if (OVT_TutorialGate.CanShowNow(false, false, true, true))
		{
			SetResultFailure("The gate allowed a popup while a blocking UI (menu, context or the map) is open");
			return true;
		}

		if (OVT_TutorialGate.CanShowNow(false, false, false, false))
		{
			SetResultFailure("The gate allowed a popup while the player is dead");
			return true;
		}

		// --- COMBINATIONS: any veto still vetoes, and everything wrong is still false.
		if (OVT_TutorialGate.CanShowNow(true, true, true, false))
		{
			SetResultFailure("The gate allowed a popup with all four conditions against it");
			return true;
		}

		if (OVT_TutorialGate.CanShowNow(false, true, true, true))
		{
			SetResultFailure("The gate allowed a popup while both showing and blocked");
			return true;
		}

		if (OVT_TutorialGate.CanShowNow(true, false, false, false))
		{
			SetResultFailure("The gate allowed a popup while tips are disabled and the player is dead");
			return true;
		}

		Print("Tutorial gate: shows only with tips enabled, nothing on screen, no blocking UI and the player alive - each of the four vetoes proven in isolation");

		SetResultSuccess();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_TutorialSeenStore - the set logic behind "a tip is shown at most once per machine, ever".
//!
//! This is the class that fixes the original defect: the old hinted-player set was reallocated on
//! every init, so the intro hint returned every single session. The rules that have to hold for the
//! replacement to be worth anything are all here - idempotent marking, exact-match lookup, a
//! lossless round trip through the plain-array shape the profile stores, a version mismatch that
//! CLEARS rather than half-loads, and a cap that refuses new ids rather than evicting old ones.
//!
//! Nothing in this case reaches a settings module: the store takes and returns plain arrays, which
//! is exactly the split that lets this tier assert it at all.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Tutorial_SeenStore : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		OVT_TutorialSeenStore store = new OVT_TutorialSeenStore();

		if (store.Count() != 0)
		{
			SetResultFailure("A fresh store remembers %1 ids, expected 0", store.Count().ToString());
			return true;
		}

		if (store.GetVersion() != OVT_TutorialSeenStore.CURRENT_VERSION)
		{
			SetResultFailure("A fresh store is at version %1, expected %2",
				store.GetVersion().ToString(), OVT_TutorialSeenStore.CURRENT_VERSION.ToString());
			return true;
		}

		if (store.HasSeen("economy-first-buy"))
		{
			SetResultFailure("A fresh store claims to have seen an id");
			return true;
		}

		// --- MARK, THEN LOOK UP. Exact match: a near miss is a bug to be seen, not to be smoothed over.
		store.MarkSeen("economy-first-buy");

		if (!store.HasSeen("economy-first-buy"))
		{
			SetResultFailure("An id that was just marked seen was not found");
			return true;
		}

		if (store.HasSeen("Economy-First-Buy"))
		{
			SetResultFailure("HasSeen matched a different casing; the lookup must be exact");
			return true;
		}

		if (store.HasSeen("economy-first"))
		{
			SetResultFailure("HasSeen matched a prefix of a stored id; the lookup must be exact");
			return true;
		}

		if (store.HasSeen(""))
		{
			SetResultFailure("HasSeen('') reported true");
			return true;
		}

		// --- IDEMPOTENT. The dismiss path can legitimately run twice (timer and explicit button).
		store.MarkSeen("economy-first-buy");
		store.MarkSeen("economy-first-buy");

		if (store.Count() != 1)
		{
			SetResultFailure("Marking the same id three times left %1 ids, expected 1", store.Count().ToString());
			return true;
		}

		// An empty id is refused rather than remembered as a blank.
		store.MarkSeen("");

		if (store.Count() != 1)
		{
			SetResultFailure("Marking an empty id left %1 ids, expected 1", store.Count().ToString());
			return true;
		}

		// --- ROUND TRIP through the plain-array shape the profile stores.
		store.MarkSeen("welcome-intro");
		store.MarkSeen("place-first-camp");

		array<string> written = new array<string>();
		store.WriteTo(written);

		if (written.Count() != 3)
		{
			SetResultFailure("WriteTo emitted %1 ids for a store of 3", written.Count().ToString());
			return true;
		}

		if (!written.Contains("economy-first-buy") || !written.Contains("welcome-intro") || !written.Contains("place-first-camp"))
		{
			SetResultFailure("WriteTo lost an id: got %1", JoinIds(written));
			return true;
		}

		// No duplication: three distinct ids in, three distinct ids out.
		for (int d = 0; d < written.Count(); d++)
		{
			for (int e = d + 1; e < written.Count(); e++)
			{
				if (written.Get(d) == written.Get(e))
				{
					SetResultFailure("WriteTo emitted '%1' twice: %2", written.Get(d), JoinIds(written));
					return true;
				}
			}
		}

		OVT_TutorialSeenStore reloaded = new OVT_TutorialSeenStore();
		reloaded.LoadFrom(written, OVT_TutorialSeenStore.CURRENT_VERSION);

		if (reloaded.Count() != 3)
		{
			SetResultFailure("A reloaded store holds %1 ids, expected 3", reloaded.Count().ToString());
			return true;
		}

		if (!reloaded.HasSeen("economy-first-buy") || !reloaded.HasSeen("welcome-intro") || !reloaded.HasSeen("place-first-camp"))
		{
			SetResultFailure("A reloaded store lost an id");
			return true;
		}

		// Writing the reloaded store back must produce the same list again - the round trip is stable,
		// so a profile that is loaded and saved repeatedly never drifts.
		array<string> rewritten = new array<string>();
		reloaded.WriteTo(rewritten);

		if (rewritten.Count() != written.Count())
		{
			SetResultFailure("A second round trip emitted %1 ids, expected %2", rewritten.Count().ToString(), written.Count().ToString());
			return true;
		}

		for (int r = 0; r < written.Count(); r++)
		{
			if (rewritten.Get(r) != written.Get(r))
			{
				SetResultFailure("The round trip is not stable: position %1 was '%2' and is now '%3'",
					r.ToString(), written.Get(r), rewritten.Get(r));
				return true;
			}
		}

		// LoadFrom REPLACES rather than merges, and a null list is an empty list, not a crash.
		OVT_TutorialSeenStore replaced = new OVT_TutorialSeenStore();
		replaced.MarkSeen("stale-id");
		replaced.LoadFrom(written, OVT_TutorialSeenStore.CURRENT_VERSION);

		if (replaced.HasSeen("stale-id"))
		{
			SetResultFailure("LoadFrom merged into the existing set instead of replacing it");
			return true;
		}

		OVT_TutorialSeenStore nulled = new OVT_TutorialSeenStore();
		nulled.MarkSeen("stale-id");
		nulled.LoadFrom(null, OVT_TutorialSeenStore.CURRENT_VERSION);

		if (nulled.Count() != 0)
		{
			SetResultFailure("LoadFrom(null) left %1 ids, expected 0", nulled.Count().ToString());
			return true;
		}

		// --- A VERSION MISMATCH CLEARS. Better one repeat of each tip than a half-trusted store.
		OVT_TutorialSeenStore mismatched = new OVT_TutorialSeenStore();
		mismatched.LoadFrom(written, OVT_TutorialSeenStore.CURRENT_VERSION + 1);

		if (mismatched.Count() != 0)
		{
			SetResultFailure("A store loaded at the wrong version kept %1 ids, expected 0", mismatched.Count().ToString());
			return true;
		}

		if (mismatched.GetVersion() != OVT_TutorialSeenStore.CURRENT_VERSION)
		{
			SetResultFailure("After a version mismatch the store is at version %1, expected to be rewritten at %2",
				mismatched.GetVersion().ToString(), OVT_TutorialSeenStore.CURRENT_VERSION.ToString());
			return true;
		}

		// It is usable afterwards rather than poisoned.
		mismatched.MarkSeen("after-reset");

		if (!mismatched.HasSeen("after-reset"))
		{
			SetResultFailure("A store cleared by a version mismatch would not accept a new id");
			return true;
		}

		// --- THE CAP REFUSES, IT DOES NOT EVICT.
		OVT_TutorialSeenStore capped = new OVT_TutorialSeenStore();

		for (int f = 0; f < OVT_TutorialSeenStore.MAX_SEEN; f++)
		{
			capped.MarkSeen("id-" + f.ToString());
		}

		if (capped.Count() != OVT_TutorialSeenStore.MAX_SEEN)
		{
			SetResultFailure("Filling to the cap left %1 ids, expected %2",
				capped.Count().ToString(), OVT_TutorialSeenStore.MAX_SEEN.ToString());
			return true;
		}

		if (capped.MarkSeen("one-too-many"))
		{
			SetResultFailure("MarkSeen past the cap reported success");
			return true;
		}

		if (capped.HasSeen("one-too-many"))
		{
			SetResultFailure("An id refused by the cap is remembered anyway");
			return true;
		}

		if (capped.Count() != OVT_TutorialSeenStore.MAX_SEEN)
		{
			SetResultFailure("After a refused id the store holds %1 ids, expected %2",
				capped.Count().ToString(), OVT_TutorialSeenStore.MAX_SEEN.ToString());
			return true;
		}

		// Nothing already remembered was dropped to make room - this is the claim that separates
		// "refuses" from "evicts", and evicting would re-show a tip the player already dismissed.
		if (!capped.HasSeen("id-0") || !capped.HasSeen("id-" + (OVT_TutorialSeenStore.MAX_SEEN - 1).ToString()))
		{
			SetResultFailure("The cap dropped an existing id to make room for a refused one");
			return true;
		}

		// A FULL store still answers a repeat mark of something it already knows, so a dismiss of an
		// already-seen entry is not reported as a failure.
		if (!capped.MarkSeen("id-0"))
		{
			SetResultFailure("Re-marking an id already present in a full store reported failure");
			return true;
		}

		Print("Tutorial seen store: exact-match lookup, idempotent marking, a stable lossless round trip through plain arrays, a version mismatch that clears and rewrites, and a 512 cap that refuses new ids without dropping old ones");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Renders an id list for a failure message.
	//! \param[in] ids The ids.
	//! \return A bracketed, comma-separated rendering.
	protected string JoinIds(array<string> ids)
	{
		if (!ids)
			return "<null>";

		string text = "[";

		for (int i = 0; i < ids.Count(); i++)
		{
			if (i > 0)
				text += ", ";

			text += ids.Get(i);
		}

		return text + "]";
	}
}

//------------------------------------------------------------------------------------------------
//! The spawn-context filter selects EXACTLY ONE welcome, and an empty context selects NONE.
//!
//! Two welcome entries are bound to the same event, at the same priority, and differ only in their
//! filter: one for a player who was given a house, one for a player who was not. The property that
//! matters to a player is that they read one of them and never both, and that the one they read
//! describes the spawn they actually got.
//!
//! THE ASYMMETRY IS THE POINT, and it is the single most likely thing a future refactor breaks.
//! An empty filter on a TRIGGER means "no filter, fire for anything". An empty filter on the
//! EVENT CONTEXT is the opposite: it satisfies no filtered trigger at all, so it selects NEITHER
//! welcome rather than defaulting to one. That is why the value the client carries into this event
//! defaults to the house context and never to "": tidying that default to "" compiles, passes every
//! other case in this file, and silently suppresses BOTH welcomes for every player on every machine.
//! This case is what goes red on that day.
//!
//! Nothing here touches a component, a manager, a widget or the world - two entries and three event
//! occurrences, all built with `new` and hand-written values.
//!
//! PROVEN ABLE TO FAIL 2026-08-09: see the Proven-Red Table in
//! docs/features/new-player-experience/first-spawn/context.md.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome : SCR_AutotestCaseBase
{
	//! The two shipped welcome ids, and the two context values they are selected by. Written out as
	//! literals rather than read off a component, because this tier owns no component.
	protected static const string HOUSE_ID = "welcome-intro";
	protected static const string NOHOUSE_ID = "welcome-nohome";
	protected static const string HOUSE_CONTEXT = "house";
	protected static const string NOHOUSE_CONTEXT = "nohouse";

	//------------------------------------------------------------------------------------------------
	[Step(EStage.Main)]
	bool Execute()
	{
		array<ref OVT_TutorialEntryConfig> entries = new array<ref OVT_TutorialEntryConfig>();

		// Same event, same priority, different filter - the shipped shape exactly.
		entries.Insert(MakeEntry(HOUSE_ID, 100, true, OVT_TutorialEvent.PLAYER_SPAWNED, 0, HOUSE_CONTEXT));
		entries.Insert(MakeEntry(NOHOUSE_ID, 100, true, OVT_TutorialEvent.PLAYER_SPAWNED, 0, NOHOUSE_CONTEXT));

		// --- THE HOUSE CONTEXT SELECTS THE HOUSE WELCOME, AND ONLY IT.
		array<string> house = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_SPAWNED, 0, HOUSE_CONTEXT), house);

		if (house.Count() != 1)
		{
			SetResultFailure("A '%1' spawn context selected %2 welcome(s) %3, expected exactly one. A player must read one welcome, never two and never none.",
				HOUSE_CONTEXT, house.Count().ToString(), JoinIds(house));
			return true;
		}

		if (house.Get(0) != HOUSE_ID)
		{
			SetResultFailure("A '%1' spawn context selected '%2', expected '%3' - the player who was given a house would read the houseless page",
				HOUSE_CONTEXT, house.Get(0), HOUSE_ID);
			return true;
		}

		// --- THE HOUSELESS CONTEXT SELECTS THE OTHER ONE, AND ONLY IT.
		array<string> nohouse = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_SPAWNED, 0, NOHOUSE_CONTEXT), nohouse);

		if (nohouse.Count() != 1)
		{
			SetResultFailure("A '%1' spawn context selected %2 welcome(s) %3, expected exactly one",
				NOHOUSE_CONTEXT, nohouse.Count().ToString(), JoinIds(nohouse));
			return true;
		}

		if (nohouse.Get(0) != NOHOUSE_ID)
		{
			SetResultFailure("A '%1' spawn context selected '%2', expected '%3' - the player who spawned at a bus stop with no house and no car would read a page telling them they own a house and a car, which is the exact failure this feature exists to prevent",
				NOHOUSE_CONTEXT, nohouse.Get(0), NOHOUSE_ID);
			return true;
		}

		// --- AN EMPTY CONTEXT SELECTS NOTHING. The load-bearing asymmetry: "" on a TRIGGER means
		// "no filter", but "" on the CONTEXT satisfies no filtered trigger, so it suppresses BOTH.
		array<string> empty = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_SPAWNED, 0, ""), empty);

		if (empty.Count() != 0)
		{
			SetResultFailure("An EMPTY spawn context selected %1 welcome(s) %2, expected none. An empty context matches no filtered trigger, which is why the client-side default must be '%3' and never \"\": with \"\" the player sees NO welcome at all rather than the wrong page.",
				empty.Count().ToString(), JoinIds(empty), HOUSE_CONTEXT);
			return true;
		}

		// --- A NEAR MISS SELECTS NOTHING EITHER. A typo'd context is not silently rounded to a filter.
		array<string> typo = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.PLAYER_SPAWNED, 0, "House"), typo);

		if (typo.Count() != 0)
		{
			SetResultFailure("A spawn context of 'House' selected %1 welcome(s) %2; the filter comparison must be exact and case-sensitive",
				typo.Count().ToString(), JoinIds(typo));
			return true;
		}

		// --- A DIFFERENT EVENT CARRYING THE SAME FILTER SELECTS NOTHING, which is what proves the
		// selections above were the spawn event and not the filter string on its own.
		array<string> otherEvent = new array<string>();
		OVT_TutorialMatcher.FindMatches(entries, MakeContext(OVT_TutorialEvent.MAP_OPENED, 0, HOUSE_CONTEXT), otherEvent);

		if (otherEvent.Count() != 0)
		{
			SetResultFailure("A non-spawn event carrying the '%1' filter selected %2 welcome(s) %3, expected none",
				HOUSE_CONTEXT, otherEvent.Count().ToString(), JoinIds(otherEvent));
			return true;
		}

		Print("Tutorial spawn context: 'house' selects welcome-intro alone, 'nohouse' selects welcome-nohome alone, and an EMPTY context selects neither - which is why the client-side default is 'house' and never \"\"");

		SetResultSuccess();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one entry carrying exactly one trigger. Every field is set explicitly.
	//! \param[in] id Entry id.
	//! \param[in] priority Queue priority.
	//! \param[in] enabled Whether the entry may fire.
	//! \param[in] evt Event the single trigger listens for.
	//! \param[in] minValue Trigger threshold, or 0 for none.
	//! \param[in] filter Exact filter, or "" for none.
	//! \return The entry.
	protected OVT_TutorialEntryConfig MakeEntry(string id, int priority, bool enabled, OVT_TutorialEvent evt, int minValue, string filter)
	{
		OVT_TutorialEntryConfig entry = new OVT_TutorialEntryConfig();
		entry.m_sId = id;
		entry.m_sTitle = "#OVT-Tutorial_Test_Title";
		entry.m_ePresentation = OVT_TutorialPresentation.MODAL;
		entry.m_iPriority = priority;
		entry.m_sFieldManualTitleKey = "";
		entry.m_bEnabled = enabled;

		entry.m_aPages = new array<ref OVT_TutorialPage>();
		OVT_TutorialPage page = new OVT_TutorialPage();
		page.m_sBody = "#OVT-Tutorial_Test_Body";
		page.m_sImage = ResourceName.Empty;
		entry.m_aPages.Insert(page);

		entry.m_aTriggers = new array<ref OVT_TutorialTrigger>();
		OVT_TutorialTrigger trigger = new OVT_TutorialTrigger();
		trigger.m_eEvent = evt;
		trigger.m_iMinValue = minValue;
		trigger.m_sFilter = filter;
		entry.m_aTriggers.Insert(trigger);

		return entry;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] evt The event that occurred.
	//! \param[in] value Numeric payload.
	//! \param[in] filter String payload - the spawn context, for PLAYER_SPAWNED.
	//! \return The occurrence.
	protected OVT_TutorialEventContext MakeContext(OVT_TutorialEvent evt, int value, string filter)
	{
		OVT_TutorialEventContext ctx = new OVT_TutorialEventContext();
		ctx.m_eEvent = evt;
		ctx.m_iPlayerId = 1;
		ctx.m_iValue = value;
		ctx.m_sFilter = filter;
		return ctx;
	}

	//------------------------------------------------------------------------------------------------
	//! Renders an id list for a failure message.
	//! \param[in] ids The ids.
	//! \return A bracketed, comma-separated rendering.
	protected string JoinIds(array<string> ids)
	{
		if (!ids)
			return "<null>";

		string text = "[";

		for (int i = 0; i < ids.Count(); i++)
		{
			if (i > 0)
				text += ", ";

			text += ids.Get(i);
		}

		return text + "]";
	}
}
