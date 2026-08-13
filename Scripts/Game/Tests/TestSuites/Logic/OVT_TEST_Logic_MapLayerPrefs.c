//------------------------------------------------------------------------------------------------
//! TIER A cases - the map layer filter store's pure decisions.
//!
//! The map layer panel is overwhelmingly UI, and UI is outside this project's automated spine: no
//! widget tier, no input tier, no rendering tier. OVT_MapLayerPrefsStore is the one part of the
//! feature that is genuinely assertable, and it was split out of the settings plumbing for exactly
//! that reason - it makes no engine call, touches no widget and knows nothing about BaseContainer,
//! taking and returning plain arrays of strings.
//!
//! Two claims are worth calling out because they are the ones that would fail silently in play:
//!  - ABSENT MEANS VISIBLE. The record stores HIDDEN keys only, so a type or layer nobody has
//!    touched - including one added by a later version - draws. Invert that default and every marker
//!    on the map disappears for every player who has never opened the panel.
//!  - KEYS ARE NAMESPACE-PREFIXED. A canvas layer id and a location type class name are authored in
//!    different files by different hands and nothing stops them spelling the same word. Without the
//!    prefixes they would share one preference, and hiding a layer would hide a marker type.
//!
//! Nothing here reaches a settings module, a component, a widget or the world - every subject is
//! built with `new` and fed hand-written values, and per this directory's tier rule neither does any
//! comment in this file name the accessors that would reach one.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! OVT_MapLayerPrefsStore - the hidden-key set behind "everything is on until the player says
//! otherwise".
//!
//! The rules that have to hold for the store to be worth anything are all here: absent is visible,
//! hide and show are both idempotent, an empty key is refused rather than remembered as a blank, a
//! version mismatch CLEARS rather than half-loading, a null list is an empty list rather than a
//! crash, and the round trip through the plain-array shape the profile stores is lossless and leaves
//! nothing stale in a reused buffer.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_MapLayerPrefs_HiddenSet : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_MapLayerPrefsStore store = new OVT_MapLayerPrefsStore();

		if (store.Count() != 0)
		{
			SetFailure("A fresh store hides %1 keys, expected 0", store.Count().ToString());
			return true;
		}

		if (store.GetVersion() != OVT_MapLayerPrefsStore.CURRENT_VERSION)
		{
			SetFailure("A fresh store is at version %1, expected %2",
				store.GetVersion().ToString(), OVT_MapLayerPrefsStore.CURRENT_VERSION.ToString());
			return true;
		}

		string houses = OVT_MapLayerPrefsStore.TypeKey("OVT_MapLocationHouse");
		string territory = OVT_MapLayerPrefsStore.LayerKey("territory");

		// --- ABSENT MEANS VISIBLE. This is the default that makes "everything on" work: a player who
		// has never opened the panel sees exactly the map they saw before the feature existed, and a
		// type or layer added by a later version needs no migration to default on.
		if (!store.IsVisible(houses))
		{
			SetFailure("A key that was never hidden reported as hidden; absent must mean visible, or every marker vanishes for a player who has never opened the panel");
			return true;
		}

		if (!store.IsVisible(territory))
		{
			SetFailure("A layer key that was never hidden reported as hidden");
			return true;
		}

		if (!store.IsVisible("type:OVT_MapLocationNeverHeardOf"))
		{
			SetFailure("A key from a type this store has never seen reported as hidden; a later version must be able to add a type without a migration");
			return true;
		}

		// An empty key is not a thing that can be hidden, and asking about one is not a trap.
		if (!store.IsVisible(""))
		{
			SetFailure("IsVisible('') reported hidden");
			return true;
		}

		// --- HIDE, THEN LOOK UP. Exact match: a near miss is a bug to be seen, not smoothed over.
		store.SetHidden(houses, true);

		if (store.IsVisible(houses))
		{
			SetFailure("A key that was just hidden reported as visible");
			return true;
		}

		if (store.Count() != 1)
		{
			SetFailure("After hiding one key the store holds %1, expected 1", store.Count().ToString());
			return true;
		}

		// Hiding one thing hides ONLY that thing.
		if (!store.IsVisible(territory))
		{
			SetFailure("Hiding a location type also hid an unrelated canvas layer");
			return true;
		}

		if (!store.IsVisible("type:OVT_MapLocationHous"))
		{
			SetFailure("A prefix of a hidden key reported as hidden; the lookup must be exact");
			return true;
		}

		if (!store.IsVisible("TYPE:OVT_MAPLOCATIONHOUSE"))
		{
			SetFailure("A differently-cased key reported as hidden; the lookup must be case-sensitive");
			return true;
		}

		// --- IDEMPOTENT BOTH WAYS. The panel re-applies the same state whenever it is rebuilt, so
		// hiding something already hidden and showing something never hidden must both be no-ops.
		store.SetHidden(houses, true);
		store.SetHidden(houses, true);

		if (store.Count() != 1)
		{
			SetFailure("Hiding the same key three times left %1 keys, expected 1", store.Count().ToString());
			return true;
		}

		store.SetHidden(territory, false);

		if (store.Count() != 1)
		{
			SetFailure("Showing a key that was never hidden left %1 keys, expected 1", store.Count().ToString());
			return true;
		}

		if (!store.IsVisible(territory))
		{
			SetFailure("Showing a key that was never hidden made it hidden");
			return true;
		}

		store.SetHidden(houses, false);

		if (!store.IsVisible(houses))
		{
			SetFailure("A key that was shown again reported as hidden");
			return true;
		}

		if (store.Count() != 0)
		{
			SetFailure("After showing the only hidden key the store holds %1, expected 0", store.Count().ToString());
			return true;
		}

		store.SetHidden(houses, false);

		if (store.Count() != 0)
		{
			SetFailure("Showing an already-visible key left %1 keys, expected 0", store.Count().ToString());
			return true;
		}

		// --- AN EMPTY KEY IS REFUSED, not remembered as a blank row nobody can ever address again.
		store.SetHidden("", true);

		if (store.Count() != 0)
		{
			SetFailure("Hiding an empty key left %1 keys, expected 0", store.Count().ToString());
			return true;
		}

		// --- ROUND TRIP through the plain-array shape the profile stores.
		store.SetHidden(houses, true);
		store.SetHidden(territory, true);
		store.SetHidden(OVT_MapLayerPrefsStore.LayerKey("restricted"), true);

		// The buffer is deliberately REUSED and PRE-POPULATED: WriteTo must clear it, or a key the
		// player has since re-enabled would be written straight back to their profile.
		array<string> written = new array<string>();
		written.Insert("layer:stale-never-hidden");
		written.Insert("type:OVT_MapLocationStale");

		store.WriteTo(written);

		if (written.Count() != 3)
		{
			SetFailure("WriteTo emitted %1 keys for a store of 3: %2", written.Count().ToString(), JoinKeys(written));
			return true;
		}

		if (written.Contains("layer:stale-never-hidden") || written.Contains("type:OVT_MapLocationStale"))
		{
			SetFailure("WriteTo left a stale key in a reused buffer: %1", JoinKeys(written));
			return true;
		}

		if (!written.Contains(houses) || !written.Contains(territory) || !written.Contains("layer:restricted"))
		{
			SetFailure("WriteTo lost a hidden key: got %1", JoinKeys(written));
			return true;
		}

		for (int d = 0; d < written.Count(); d++)
		{
			for (int e = d + 1; e < written.Count(); e++)
			{
				if (written.Get(d) == written.Get(e))
				{
					SetFailure("WriteTo emitted '%1' twice: %2", written.Get(d), JoinKeys(written));
					return true;
				}
			}
		}

		OVT_MapLayerPrefsStore reloaded = new OVT_MapLayerPrefsStore();
		reloaded.LoadFrom(written, OVT_MapLayerPrefsStore.CURRENT_VERSION);

		if (reloaded.Count() != 3)
		{
			SetFailure("A reloaded store hides %1 keys, expected 3", reloaded.Count().ToString());
			return true;
		}

		if (reloaded.IsVisible(houses) || reloaded.IsVisible(territory) || reloaded.IsVisible("layer:restricted"))
		{
			SetFailure("A reloaded store lost a hidden key");
			return true;
		}

		// Everything else is still visible - a reload must not turn the map off.
		if (!reloaded.IsVisible("type:OVT_MapLocationTown"))
		{
			SetFailure("A reloaded store hid a key that was never in the list");
			return true;
		}

		// The round trip is stable, so a profile loaded and saved repeatedly never drifts.
		array<string> rewritten = new array<string>();
		reloaded.WriteTo(rewritten);

		if (rewritten.Count() != written.Count())
		{
			SetFailure("A second round trip emitted %1 keys, expected %2",
				rewritten.Count().ToString(), written.Count().ToString());
			return true;
		}

		for (int r = 0; r < written.Count(); r++)
		{
			if (rewritten.Get(r) != written.Get(r))
			{
				SetFailure("The round trip is not stable: position %1 was '%2' and is now '%3'",
					r.ToString(), written.Get(r), rewritten.Get(r));
				return true;
			}
		}

		// --- LoadFrom REPLACES rather than merges.
		OVT_MapLayerPrefsStore replaced = new OVT_MapLayerPrefsStore();
		replaced.SetHidden("layer:stale", true);
		replaced.LoadFrom(written, OVT_MapLayerPrefsStore.CURRENT_VERSION);

		if (!replaced.IsVisible("layer:stale"))
		{
			SetFailure("LoadFrom merged into the existing set instead of replacing it");
			return true;
		}

		// --- A NULL LIST IS AN EMPTY LIST, not a crash. The settings loader legitimately hands back a
		// null array for a member that has never been written.
		OVT_MapLayerPrefsStore nulled = new OVT_MapLayerPrefsStore();
		nulled.SetHidden("layer:stale", true);
		nulled.LoadFrom(null, OVT_MapLayerPrefsStore.CURRENT_VERSION);

		if (nulled.Count() != 0)
		{
			SetFailure("LoadFrom(null) left %1 keys, expected 0", nulled.Count().ToString());
			return true;
		}

		if (!nulled.IsVisible("layer:stale"))
		{
			SetFailure("LoadFrom(null) left a key hidden");
			return true;
		}

		// --- A VERSION MISMATCH DISCARDS. Better one re-toggle of each filter than a half-trusted
		// store, and the direction is safe: a cleared store shows the whole map.
		OVT_MapLayerPrefsStore mismatched = new OVT_MapLayerPrefsStore();
		mismatched.LoadFrom(written, OVT_MapLayerPrefsStore.CURRENT_VERSION + 1);

		if (mismatched.Count() != 0)
		{
			SetFailure("A store loaded at version %1 kept %2 keys, expected 0 - a mismatched version must discard rather than half-load",
				(OVT_MapLayerPrefsStore.CURRENT_VERSION + 1).ToString(), mismatched.Count().ToString());
			return true;
		}

		if (!mismatched.IsVisible(houses))
		{
			SetFailure("A store loaded at a mismatched version imported a hidden key anyway");
			return true;
		}

		// Version 0 is the shape a never-written profile block reports, and it must not be trusted
		// either.
		OVT_MapLayerPrefsStore zeroVersion = new OVT_MapLayerPrefsStore();
		zeroVersion.LoadFrom(written, 0);

		if (zeroVersion.Count() != 0)
		{
			SetFailure("A store loaded at version 0 kept %1 keys, expected 0", zeroVersion.Count().ToString());
			return true;
		}

		if (mismatched.GetVersion() != OVT_MapLayerPrefsStore.CURRENT_VERSION)
		{
			SetFailure("After a version mismatch the store is at version %1, expected to be rewritten at %2",
				mismatched.GetVersion().ToString(), OVT_MapLayerPrefsStore.CURRENT_VERSION.ToString());
			return true;
		}

		// It is usable afterwards rather than poisoned.
		mismatched.SetHidden(houses, true);

		if (mismatched.IsVisible(houses))
		{
			SetFailure("A store cleared by a version mismatch would not accept a new hidden key");
			return true;
		}

		// --- CLEAR turns everything back on.
		mismatched.Clear();

		if (mismatched.Count() != 0)
		{
			SetFailure("Clear() left %1 keys", mismatched.Count().ToString());
			return true;
		}

		if (!mismatched.IsVisible(houses))
		{
			SetFailure("Clear() left a key hidden");
			return true;
		}

		// --- THE CAP REFUSES, IT DOES NOT EVICT.
		OVT_MapLayerPrefsStore capped = new OVT_MapLayerPrefsStore();

		for (int f = 0; f < OVT_MapLayerPrefsStore.MAX_HIDDEN; f++)
		{
			capped.SetHidden("type:Filler" + f.ToString(), true);
		}

		if (capped.Count() != OVT_MapLayerPrefsStore.MAX_HIDDEN)
		{
			SetFailure("Filling to the cap left %1 keys, expected %2",
				capped.Count().ToString(), OVT_MapLayerPrefsStore.MAX_HIDDEN.ToString());
			return true;
		}

		capped.SetHidden("type:OneTooMany", true);

		if (capped.Count() != OVT_MapLayerPrefsStore.MAX_HIDDEN)
		{
			SetFailure("After an over-cap hide the store holds %1 keys, expected %2",
				capped.Count().ToString(), OVT_MapLayerPrefsStore.MAX_HIDDEN.ToString());
			return true;
		}

		if (!capped.IsVisible("type:OneTooMany"))
		{
			SetFailure("A key refused by the cap was hidden anyway");
			return true;
		}

		// Nothing already remembered was dropped to make room - evicting would silently re-show a
		// layer the player deliberately turned off.
		if (capped.IsVisible("type:Filler0") || capped.IsVisible("type:Filler" + (OVT_MapLayerPrefsStore.MAX_HIDDEN - 1).ToString()))
		{
			SetFailure("The cap dropped an existing key to make room for a refused one");
			return true;
		}

		// A FULL store still accepts showing something, which is the only mutation that can free room.
		capped.SetHidden("type:Filler0", false);

		if (capped.Count() != OVT_MapLayerPrefsStore.MAX_HIDDEN - 1)
		{
			SetFailure("Showing a key in a full store left %1 keys, expected %2",
				capped.Count().ToString(), (OVT_MapLayerPrefsStore.MAX_HIDDEN - 1).ToString());
			return true;
		}

		Print("Map layer prefs: absent means visible, hide and show are both idempotent, an empty key is refused, a mismatched version discards rather than half-loading, a null list is an empty list, and WriteTo clears a reused buffer before writing exactly the hidden set");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Renders a key list for a failure message, so a wrong set is readable at a glance.
	//! \param[in] keys The keys.
	//! \return A bracketed, comma-separated rendering.
	protected string JoinKeys(array<string> keys)
	{
		if (!keys)
			return "<null>";

		string text = "[";

		for (int i = 0; i < keys.Count(); i++)
		{
			if (i > 0)
				text += ", ";

			text += keys.Get(i);
		}

		return text + "]";
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_MapLayerPrefsStore.TypeKey / .LayerKey - two id spaces that cannot collide.
//!
//! The panel's rows come from sources that are authored independently: location type keys are script
//! CLASS NAMES, canvas layer keys are HAND-WRITTEN CONFIG STRINGS. Nothing stops both spelling the
//! same word, and if they did without prefixes they would share one stored preference - hiding the
//! territory overlay would also hide a marker type, and a player would have no way to separate them.
//!
//! The prefixes cost one string concat each. This case is what keeps them, and it asserts the
//! property directly (a key is never the bare name) as well as through the store, because a key
//! builder that quietly returns its argument is exactly the "simplification" a later reader would
//! make.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_MapLayerPrefs_KeyNamespaces : SCR_AutotestCaseBase
{
	//! One bare name deliberately used as BOTH a location type class name and a canvas layer id.
	protected static const string COLLIDING_NAME = "territory";

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		string typeKey = OVT_MapLayerPrefsStore.TypeKey(COLLIDING_NAME);
		string layerKey = OVT_MapLayerPrefsStore.LayerKey(COLLIDING_NAME);

		// --- NEITHER BUILDER MAY RETURN THE BARE NAME. An unprefixed key is what makes the two id
		// spaces overlap in the first place.
		if (typeKey == COLLIDING_NAME)
		{
			SetFailure("TypeKey('%1') returned the bare name; a preference key must be namespaced or a class name and a layer id can address the same record",
				COLLIDING_NAME);
			return true;
		}

		if (layerKey == COLLIDING_NAME)
		{
			SetFailure("LayerKey('%1') returned the bare name; a preference key must be namespaced or a layer id and a class name can address the same record",
				COLLIDING_NAME);
			return true;
		}

		if (typeKey != OVT_MapLayerPrefsStore.TYPE_PREFIX + COLLIDING_NAME)
		{
			SetFailure("TypeKey('%1') gave '%2', expected '%3'",
				COLLIDING_NAME, typeKey, OVT_MapLayerPrefsStore.TYPE_PREFIX + COLLIDING_NAME);
			return true;
		}

		if (layerKey != OVT_MapLayerPrefsStore.LAYER_PREFIX + COLLIDING_NAME)
		{
			SetFailure("LayerKey('%1') gave '%2', expected '%3'",
				COLLIDING_NAME, layerKey, OVT_MapLayerPrefsStore.LAYER_PREFIX + COLLIDING_NAME);
			return true;
		}

		if (typeKey == layerKey)
		{
			SetFailure("A location type and a canvas layer both named '%1' produced the same key '%2'",
				COLLIDING_NAME, typeKey);
			return true;
		}

		// --- AN EMPTY NAME PRODUCES AN EMPTY KEY, not a bare prefix that would hide everything
		// nameless under one shared record.
		if (OVT_MapLayerPrefsStore.TypeKey("") != "")
		{
			SetFailure("TypeKey('') gave '%1', expected an empty key", OVT_MapLayerPrefsStore.TypeKey(""));
			return true;
		}

		if (OVT_MapLayerPrefsStore.LayerKey("") != "")
		{
			SetFailure("LayerKey('') gave '%1', expected an empty key", OVT_MapLayerPrefsStore.LayerKey(""));
			return true;
		}

		// --- THROUGH THE STORE: hiding one does not hide the other, in either direction.
		OVT_MapLayerPrefsStore store = new OVT_MapLayerPrefsStore();
		store.SetHidden(layerKey, true);

		if (store.IsVisible(layerKey))
		{
			SetFailure("The layer key '%1' was hidden and reported visible", layerKey);
			return true;
		}

		if (!store.IsVisible(typeKey))
		{
			SetFailure("Hiding the canvas layer '%1' also hid the location type of the same name; the two id spaces must not share a preference",
				COLLIDING_NAME);
			return true;
		}

		if (store.Count() != 1)
		{
			SetFailure("Hiding one of two same-named keys left %1 keys, expected 1", store.Count().ToString());
			return true;
		}

		// The mirror direction, with a store of its own so neither result can be an artefact of the
		// order the two were hidden in.
		OVT_MapLayerPrefsStore mirror = new OVT_MapLayerPrefsStore();
		mirror.SetHidden(typeKey, true);

		if (mirror.IsVisible(typeKey))
		{
			SetFailure("The type key '%1' was hidden and reported visible", typeKey);
			return true;
		}

		if (!mirror.IsVisible(layerKey))
		{
			SetFailure("Hiding the location type '%1' also hid the canvas layer of the same name",
				COLLIDING_NAME);
			return true;
		}

		// --- BOTH HIDDEN AT ONCE ARE TWO RECORDS, not one. If the prefixes collapsed, this would be a
		// single entry and showing one would show both.
		OVT_MapLayerPrefsStore both = new OVT_MapLayerPrefsStore();
		both.SetHidden(typeKey, true);
		both.SetHidden(layerKey, true);

		if (both.Count() != 2)
		{
			SetFailure("Hiding a location type and a canvas layer that share the name '%1' produced %2 record(s), expected 2",
				COLLIDING_NAME, both.Count().ToString());
			return true;
		}

		both.SetHidden(layerKey, false);

		if (both.IsVisible(typeKey))
		{
			SetFailure("Showing the canvas layer '%1' also showed the location type of the same name",
				COLLIDING_NAME);
			return true;
		}

		if (!both.IsVisible(layerKey))
		{
			SetFailure("The canvas layer '%1' was shown again and reported hidden", COLLIDING_NAME);
			return true;
		}

		// --- DISTINCT NAMES IN THE SAME SPACE stay distinct too, which is what proves the separation
		// above came from the prefixes and not from the store refusing to hold two keys.
		string houses = OVT_MapLayerPrefsStore.TypeKey("OVT_MapLocationHouse");
		string towns = OVT_MapLayerPrefsStore.TypeKey("OVT_MapLocationTown");

		if (houses == towns)
		{
			SetFailure("Two different location types produced the same key '%1'", houses);
			return true;
		}

		OVT_MapLayerPrefsStore types = new OVT_MapLayerPrefsStore();
		types.SetHidden(houses, true);

		if (!types.IsVisible(towns))
		{
			SetFailure("Hiding one location type hid another");
			return true;
		}

		Print("Map layer prefs keys: TypeKey and LayerKey namespace their argument, an empty name gives an empty key, and a location type and a canvas layer sharing the bare name 'territory' hold two independent preferences");

		return true;
	}
}
