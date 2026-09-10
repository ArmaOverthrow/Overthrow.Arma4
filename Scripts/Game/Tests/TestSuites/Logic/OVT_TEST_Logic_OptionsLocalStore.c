//------------------------------------------------------------------------------------------------
//! TIER A cases - the LOCAL option store's pure decisions.
//!
//! `OVT_OptionsLocalStore` makes no engine call, touches no widget and names no manager. Every
//! subject here is built with `new` and fed hand-written values.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! `OVT_OptionsLocalStore` - set, read, overwrite, remove and the empty-id refusal.
//!
//! Proven able to fail: each assertion was checked red against a store with `Set` returning `void`
//! and no cap or empty-id guard, then green after adding the guard clauses.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_OptionsLocalStore_SetAndRead : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsLocalStore store = new OVT_OptionsLocalStore();

		if (store.Count() != 0)
		{
			SetFailure("A fresh store holds %1 entries, expected 0", store.Count().ToString());
			return true;
		}

		if (store.Has("local.test") || store.Get("local.test") != "")
		{
			SetFailure("A fresh store already answers for an id nobody has set");
			return true;
		}

		// --- SET, THEN READ.
		if (!store.Set("local.test", "1"))
		{
			SetFailure("Set refused a well-formed id");
			return true;
		}

		if (!store.Has("local.test"))
		{
			SetFailure("Has answered false right after Set");
			return true;
		}

		if (store.Get("local.test") != "1")
		{
			SetFailure("Get returned '%1', expected '1'", store.Get("local.test"));
			return true;
		}

		if (store.Count() != 1)
		{
			SetFailure("After one Set the store holds %1 entries, expected 1", store.Count().ToString());
			return true;
		}

		// --- OVERWRITE keeps the count the same and replaces the value.
		if (!store.Set("local.test", "0"))
		{
			SetFailure("Set refused to overwrite an existing id");
			return true;
		}

		if (store.Get("local.test") != "0")
		{
			SetFailure("An overwrite left the old value '%1'", store.Get("local.test"));
			return true;
		}

		if (store.Count() != 1)
		{
			SetFailure("An overwrite changed the entry count to %1, expected 1", store.Count().ToString());
			return true;
		}

		// --- REMOVE is idempotent: removing twice is not an error and the second call reports it.
		if (!store.Remove("local.test"))
		{
			SetFailure("Remove reported false for an id that was present");
			return true;
		}

		if (store.Has("local.test") || store.Get("local.test") != "")
		{
			SetFailure("A removed id still answers");
			return true;
		}

		if (store.Remove("local.test"))
		{
			SetFailure("Remove reported true for an id that was already absent");
			return true;
		}

		if (store.Count() != 0)
		{
			SetFailure("After removing the only entry the store holds %1, expected 0", store.Count().ToString());
			return true;
		}

		// --- AN EMPTY ID IS REFUSED, not remembered as a blank entry nobody can ever address again.
		if (store.Set("", "x"))
		{
			SetFailure("Set accepted an empty id");
			return true;
		}

		if (store.Count() != 0)
		{
			SetFailure("Refusing an empty id still changed the entry count to %1", store.Count().ToString());
			return true;
		}

		if (store.GetVersion() != OVT_OptionsLocalStore.CURRENT_VERSION)
		{
			SetFailure("A fresh store is at version %1, expected %2",
				store.GetVersion().ToString(), OVT_OptionsLocalStore.CURRENT_VERSION.ToString());
			return true;
		}

		// --- THE CAP REFUSES A NEW ID, IT DOES NOT EVICT AN OLD ONE.
		OVT_OptionsLocalStore capped = new OVT_OptionsLocalStore();

		for (int i = 0; i < OVT_OptionsLocalStore.MAX_ENTRIES; i++)
		{
			capped.Set("local.filler" + i.ToString(), "v");
		}

		if (capped.Count() != OVT_OptionsLocalStore.MAX_ENTRIES)
		{
			SetFailure("Filling to the cap left %1 entries, expected %2",
				capped.Count().ToString(), OVT_OptionsLocalStore.MAX_ENTRIES.ToString());
			return true;
		}

		if (capped.Set("local.onetoomany", "v"))
		{
			SetFailure("Set accepted a new id past the cap");
			return true;
		}

		if (capped.Count() != OVT_OptionsLocalStore.MAX_ENTRIES)
		{
			SetFailure("An over-cap Set changed the entry count to %1, expected %2",
				capped.Count().ToString(), OVT_OptionsLocalStore.MAX_ENTRIES.ToString());
			return true;
		}

		// An overwrite of an existing id must still work while the store is full.
		if (!capped.Set("local.filler0", "changed"))
		{
			SetFailure("Set refused to overwrite an existing id while the store was full");
			return true;
		}

		if (capped.Get("local.filler0") != "changed")
		{
			SetFailure("An overwrite while full did not take effect");
			return true;
		}

		Print("Local option store: set, read, overwrite and remove are idempotent, an empty id is refused, and the cap refuses a new id without evicting an old one");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! `OVT_OptionsLocalStore.FromPairs` - a version mismatch clears rather than half-loads.
//!
//! Proven able to fail: checked red against a `FromPairs` that adopted the arrays before checking
//! the version, then green after moving the version check first.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_OptionsLocalStore_VersionInvalidates : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> ids = {"local.a", "local.b"};
		array<string> values = {"1", "2"};

		// --- A MATCHING VERSION ADOPTS.
		OVT_OptionsLocalStore matched = new OVT_OptionsLocalStore();
		bool adopted = matched.FromPairs(OVT_OptionsLocalStore.CURRENT_VERSION, ids, values);

		if (!adopted)
		{
			SetFailure("FromPairs reported false for a matching version");
			return true;
		}

		if (matched.Count() != 2)
		{
			SetFailure("A matched load holds %1 entries, expected 2", matched.Count().ToString());
			return true;
		}

		if (matched.Get("local.a") != "1" || matched.Get("local.b") != "2")
		{
			SetFailure("A matched load lost or swapped a value");
			return true;
		}

		// --- A MISMATCHED VERSION CLEARS rather than importing values that may no longer mean what
		// they meant.
		OVT_OptionsLocalStore mismatched = new OVT_OptionsLocalStore();
		mismatched.Set("local.stale", "x");

		bool adoptedMismatch = mismatched.FromPairs(OVT_OptionsLocalStore.CURRENT_VERSION + 1, ids, values);

		if (adoptedMismatch)
		{
			SetFailure("FromPairs reported true for a mismatched version");
			return true;
		}

		if (mismatched.Count() != 0)
		{
			SetFailure("A version mismatch kept %1 entries, expected 0 - a mismatch must discard rather than half-load",
				mismatched.Count().ToString());
			return true;
		}

		if (mismatched.Has("local.stale"))
		{
			SetFailure("A version mismatch left a pre-existing entry behind instead of clearing it");
			return true;
		}

		if (mismatched.GetVersion() != OVT_OptionsLocalStore.CURRENT_VERSION)
		{
			SetFailure("After a version mismatch the store is at version %1, expected %2",
				mismatched.GetVersion().ToString(), OVT_OptionsLocalStore.CURRENT_VERSION.ToString());
			return true;
		}

		// Version 0 is the shape a never-written block could report, and it must not be trusted.
		OVT_OptionsLocalStore zeroVersion = new OVT_OptionsLocalStore();
		zeroVersion.FromPairs(0, ids, values);

		if (zeroVersion.Count() != 0)
		{
			SetFailure("A load at version 0 kept %1 entries, expected 0", zeroVersion.Count().ToString());
			return true;
		}

		// A store cleared by a mismatch is usable again, not poisoned.
		mismatched.Set("local.fresh", "y");

		if (mismatched.Get("local.fresh") != "y")
		{
			SetFailure("A store cleared by a version mismatch refused a fresh Set afterwards");
			return true;
		}

		// --- A NULL ARRAY CLEARS, IT DOES NOT CRASH. The settings loader legitimately hands back a
		// null array for a member that has never been written.
		OVT_OptionsLocalStore nulled = new OVT_OptionsLocalStore();
		nulled.Set("local.stale", "x");

		bool adoptedNull = nulled.FromPairs(OVT_OptionsLocalStore.CURRENT_VERSION, null, values);

		if (adoptedNull)
		{
			SetFailure("FromPairs reported true for a null id array");
			return true;
		}

		if (nulled.Count() != 0)
		{
			SetFailure("A null id array kept %1 entries, expected 0", nulled.Count().ToString());
			return true;
		}

		OVT_OptionsLocalStore nulledValues = new OVT_OptionsLocalStore();
		bool adoptedNullValues = nulledValues.FromPairs(OVT_OptionsLocalStore.CURRENT_VERSION, ids, null);

		if (adoptedNullValues)
		{
			SetFailure("FromPairs reported true for a null value array");
			return true;
		}

		// --- MISMATCHED LENGTHS CLEAR rather than pairing a stray id with the wrong value.
		array<string> shortValues = {"1"};

		OVT_OptionsLocalStore uneven = new OVT_OptionsLocalStore();
		bool adoptedUneven = uneven.FromPairs(OVT_OptionsLocalStore.CURRENT_VERSION, ids, shortValues);

		if (adoptedUneven)
		{
			SetFailure("FromPairs reported true for mismatched array lengths");
			return true;
		}

		if (uneven.Count() != 0)
		{
			SetFailure("A length mismatch kept %1 entries, expected 0", uneven.Count().ToString());
			return true;
		}

		Print("Local option store: a version mismatch, a null array and a length mismatch all clear the store and report the current version");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! `OVT_OptionsLocalStore.ToPairs` / `FromPairs` - the round trip is lossless and a reused buffer
//! is never left with a stale entry.
//!
//! Proven able to fail: checked red against a `ToPairs` that did not clear its output arrays first,
//! then green after adding the clear.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_OptionsLocalStore_RoundTrip : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsLocalStore store = new OVT_OptionsLocalStore();
		store.Set("local.a", "1");
		store.Set("local.b", "hello");
		store.Set("local.c", "3.5");

		// The buffers are deliberately REUSED and PRE-POPULATED: ToPairs must clear them, or a value
		// the player has since removed would still be written back to the profile.
		array<string> ids = new array<string>();
		ids.Insert("local.stale-id");

		array<string> values = new array<string>();
		values.Insert("stale-value");

		store.ToPairs(ids, values);

		if (ids.Count() != 3 || values.Count() != 3)
		{
			SetFailure("ToPairs emitted %1 ids and %2 values for a store of 3", ids.Count().ToString(), values.Count().ToString());
			return true;
		}

		if (ids.Contains("local.stale-id") || values.Contains("stale-value"))
		{
			SetFailure("ToPairs left a stale pair in a reused buffer");
			return true;
		}

		// Each id lines up with its own value.
		for (int i = 0; i < ids.Count(); i++)
		{
			string id = ids[i];
			string value = values[i];

			if (id == "local.a" && value != "1")
			{
				SetFailure("local.a paired with '%1', expected '1'", value);
				return true;
			}

			if (id == "local.b" && value != "hello")
			{
				SetFailure("local.b paired with '%1', expected 'hello'", value);
				return true;
			}

			if (id == "local.c" && value != "3.5")
			{
				SetFailure("local.c paired with '%1', expected '3.5'", value);
				return true;
			}
		}

		// --- THE ROUND TRIP IS LOSSLESS.
		OVT_OptionsLocalStore reloaded = new OVT_OptionsLocalStore();
		bool adopted = reloaded.FromPairs(OVT_OptionsLocalStore.CURRENT_VERSION, ids, values);

		if (!adopted)
		{
			SetFailure("FromPairs reported false for a round trip at the current version");
			return true;
		}

		if (reloaded.Count() != 3)
		{
			SetFailure("A reloaded store holds %1 entries, expected 3", reloaded.Count().ToString());
			return true;
		}

		if (reloaded.Get("local.a") != "1" || reloaded.Get("local.b") != "hello" || reloaded.Get("local.c") != "3.5")
		{
			SetFailure("A reloaded store lost or altered a value");
			return true;
		}

		// A second pass through the same reused buffers stays clean and stable.
		reloaded.ToPairs(ids, values);

		if (ids.Count() != 3 || values.Count() != 3)
		{
			SetFailure("A second ToPairs into the same buffers left %1 ids and %2 values, expected 3 and 3",
				ids.Count().ToString(), values.Count().ToString());
			return true;
		}

		Print("Local option store: the pair round trip is lossless and ToPairs clears a reused buffer before writing");

		return true;
	}
}
