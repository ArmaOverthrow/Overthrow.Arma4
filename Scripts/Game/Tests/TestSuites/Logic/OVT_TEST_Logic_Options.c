//------------------------------------------------------------------------------------------------
//! TIER A cases - the options registry's pure decisions: registration, normalization, default
//! precedence, the pending-override path and the scope permission table.
//!
//! Every subject is built with `new` and fed hand-written values. Nothing here reaches a manager, a
//! widget or the world. No case asserts that a shipped default equals a configured number.
//------------------------------------------------------------------------------------------------

//! Epsilon for a float comparison. A stored value round-trips through a string, so `==` is never
//! the right test.
const float OVT_TEST_OPTIONS_EPSILON = 0.001;

//------------------------------------------------------------------------------------------------
//! Registration is idempotent: the first definition for an id wins and a later one is refused.
//!
//! Fails if Register overwrites an existing id: the second definition's default would win and this
//! case would read "0" where it expects "1".
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_RegisterIsIdempotent : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();

		OVT_OptionDefinition first = OVT_OptionDefinition.MakeToggle("test.dupe", OVT_EOptionScope.LOCAL, true, "#Label", "#Desc");
		OVT_OptionDefinition second = OVT_OptionDefinition.MakeToggle("test.dupe", OVT_EOptionScope.WORLD, false, "#OtherLabel", "#OtherDesc");

		if (!registry.Register(first))
		{
			SetFailure("The first registration of an unused id was refused");
			return true;
		}

		if (registry.Register(second))
		{
			SetFailure("A second registration for an already-known id was accepted");
			return true;
		}

		OVT_OptionDefinition resolved = registry.GetDefinition("test.dupe");
		if (!resolved)
		{
			SetFailure("GetDefinition('test.dupe') returned null after a successful registration");
			return true;
		}

		if (resolved.m_sDefault != "1")
		{
			SetFailure("The first definition's default was '1' but the resolved default reads '%1'; the second registration must have overwritten it", resolved.m_sDefault);
			return true;
		}

		if (resolved.m_eScope != OVT_EOptionScope.LOCAL)
		{
			SetFailure("The resolved scope is not the first definition's LOCAL scope; the second registration must have overwritten it");
			return true;
		}

		if (registry.Register(null))
		{
			SetFailure("Register(null) was accepted");
			return true;
		}

		OVT_OptionDefinition blank = OVT_OptionDefinition.MakeToggle("", OVT_EOptionScope.LOCAL, true, "#Label", "#Desc");
		if (registry.Register(blank))
		{
			SetFailure("A definition with an empty id was accepted");
			return true;
		}

		Print("Options registry: a second registration for a known id is refused, null and an empty id are both refused, and the first definition's fields survive");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A `SLIDER` value below its minimum, above its maximum, or between two steps never leaves the
//! registry.
//!
//! Fails if Normalize skips the clamp: SetValue("test.slider", "5000") would read back as 5000
//! instead of the definition's max of 100. Fails if Normalize skips the snap: a value of 27 with a
//! step of 25 from a min of 0 would read back as 27 instead of 25.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_SliderClampsAndSnaps : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();

		OVT_OptionDefinition def = OVT_OptionDefinition.MakeSlider("test.slider", OVT_EOptionScope.WORLD, 50, 0, 100, 25, "#Label", "#Desc");
		registry.Register(def);

		registry.SetValue("test.slider", "5000");
		if (Math.AbsFloat(registry.GetFloat("test.slider") - 100) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A value of 5000 against a max of 100 read back as %1, expected 100", registry.GetFloat("test.slider").ToString());
			return true;
		}

		registry.SetValue("test.slider", "-500");
		if (Math.AbsFloat(registry.GetFloat("test.slider") - 0) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A value of -500 against a min of 0 read back as %1, expected 0", registry.GetFloat("test.slider").ToString());
			return true;
		}

		registry.SetValue("test.slider", "27");
		if (Math.AbsFloat(registry.GetFloat("test.slider") - 25) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A value of 27 with a step of 25 from 0 read back as %1, expected the nearer step 25", registry.GetFloat("test.slider").ToString());
			return true;
		}

		registry.SetValue("test.slider", "38");
		if (Math.AbsFloat(registry.GetFloat("test.slider") - 50) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A value of 38 with a step of 25 from 0 read back as %1, expected the nearer step 50", registry.GetFloat("test.slider").ToString());
			return true;
		}

		registry.SetValue("test.slider", "75");
		if (Math.AbsFloat(registry.GetFloat("test.slider") - 75) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A value exactly on a step was moved off it: read back as %1, expected 75", registry.GetFloat("test.slider").ToString());
			return true;
		}

		// A step of zero or less must clamp only, never snap or divide by zero.
		OVT_OptionDefinition noStep = OVT_OptionDefinition.MakeSlider("test.slider.nostep", OVT_EOptionScope.WORLD, 10, 0, 100, 0, "#Label", "#Desc");
		registry.Register(noStep);

		registry.SetValue("test.slider.nostep", "63.4");
		if (Math.AbsFloat(registry.GetFloat("test.slider.nostep") - 63.4) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A zero step snapped or corrupted 63.4 into %1", registry.GetFloat("test.slider.nostep").ToString());
			return true;
		}

		registry.SetValue("test.slider.nostep", "500");
		if (Math.AbsFloat(registry.GetFloat("test.slider.nostep") - 100) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A zero-step slider failed to clamp 500 to its max of 100, read %1", registry.GetFloat("test.slider.nostep").ToString());
			return true;
		}

		Print("Options registry: a slider clamps to [min, max] and snaps to the nearest step from min, and a zero step clamps without snapping");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A `CHOICE` stores its key, never its index (D2). An undeclared key falls back to the default,
//! and reordering the declared list keeps a stored value readable.
//!
//! Fails if the registry ever stores an index: reordering `m_aChoiceKeys` in place after a value is
//! stored would then change what the stored value means, and this case would read the wrong key
//! back after the swap.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_ChoiceStoresKey : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		array<string> keys = new array<string>();
		keys.Insert("low");
		keys.Insert("medium");
		keys.Insert("high");

		array<string> labelKeys = new array<string>();
		labelKeys.Insert("#Low");
		labelKeys.Insert("#Medium");
		labelKeys.Insert("#High");

		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();
		OVT_OptionDefinition def = OVT_OptionDefinition.MakeChoice("test.choice", OVT_EOptionScope.FACTION, "medium", keys, labelKeys, "#Label", "#Desc");
		registry.Register(def);

		if (registry.GetChoice("test.choice") != "medium")
		{
			SetFailure("An unset choice read back '%1', expected the default 'medium'", registry.GetChoice("test.choice"));
			return true;
		}

		registry.SetValue("test.choice", "high");
		if (registry.GetChoice("test.choice") != "high")
		{
			SetFailure("Storing the declared key 'high' read back '%1'", registry.GetChoice("test.choice"));
			return true;
		}

		registry.SetValue("test.choice", "extreme");
		if (registry.GetChoice("test.choice") != "medium")
		{
			SetFailure("An undeclared key 'extreme' read back '%1', expected the fallback default 'medium'", registry.GetChoice("test.choice"));
			return true;
		}

		registry.SetValue("test.choice", "low");

		// Reorder the declared list in place. A key-based store must not care.
		keys.Clear();
		keys.Insert("high");
		keys.Insert("low");
		keys.Insert("medium");

		if (registry.GetChoice("test.choice") != "low")
		{
			SetFailure("Reordering the declared choice list changed the stored value to '%1', expected it to stay 'low'", registry.GetChoice("test.choice"));
			return true;
		}

		Print("Options registry: a choice stores its key, an undeclared key falls back to the default, and reordering the declared list leaves a stored value unchanged");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! An unset option reads its definition's default. A stored value beats the default. An id with no
//! definition at all reads an empty value.
//!
//! Fails if GetValue ignores the stored value and always returns the default: after SetValue moves
//! the option off its default, this case would still read the old default back.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_DefaultPrecedence : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();

		OVT_OptionDefinition toggle = OVT_OptionDefinition.MakeToggle("test.precedence.toggle", OVT_EOptionScope.LOCAL, true, "#Label", "#Desc");
		registry.Register(toggle);

		if (!registry.GetBool("test.precedence.toggle"))
		{
			SetFailure("An unset toggle with a true default read false");
			return true;
		}

		registry.SetValue("test.precedence.toggle", "0");
		if (registry.GetBool("test.precedence.toggle"))
		{
			SetFailure("A toggle explicitly set to '0' still read true; a stored value must beat the default");
			return true;
		}

		OVT_OptionDefinition slider = OVT_OptionDefinition.MakeSlider("test.precedence.slider", OVT_EOptionScope.LOCAL, 50, 0, 200, 0, "#Label", "#Desc");
		registry.Register(slider);

		if (Math.AbsFloat(registry.GetFloat("test.precedence.slider") - 50) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("An unset slider read %1, expected its own default of 50", registry.GetFloat("test.precedence.slider").ToString());
			return true;
		}

		registry.SetValue("test.precedence.slider", "120");
		if (Math.AbsFloat(registry.GetFloat("test.precedence.slider") - 120) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A slider set to 120 read %1, a stored value must beat the default", registry.GetFloat("test.precedence.slider").ToString());
			return true;
		}

		if (registry.HasDefinition("test.precedence.unknown"))
		{
			SetFailure("HasDefinition reported true for an id that was never registered");
			return true;
		}

		if (registry.GetValue("test.precedence.unknown") != "")
		{
			SetFailure("An unregistered id with no stored value read '%1', expected an empty string", registry.GetValue("test.precedence.unknown"));
			return true;
		}

		Print("Options registry: an unset option reads its own default, a stored value beats the default, and an unregistered id with no stored value reads empty");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A value stored before its registration arrives is applied and normalized once the registration
//! arrives (D5). Both orders (store-then-register, register-then-store) reach the same normalized
//! result.
//!
//! Fails if a pending value is left unclamped after Register runs: this case sets an out-of-range
//! value before the definition exists, and a normalization skipped at Register time would read the
//! raw out-of-range value back instead of the clamped one.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_PendingOverride : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();

		// --- ORDER A: store first, register second.
		registry.SetValue("test.pending.a", "9000");

		if (!registry.IsPending("test.pending.a"))
		{
			SetFailure("A value stored for an unregistered id was not marked pending");
			return true;
		}

		if (registry.HasDefinition("test.pending.a"))
		{
			SetFailure("HasDefinition reported true before any registration");
			return true;
		}

		OVT_OptionDefinition defA = OVT_OptionDefinition.MakeSlider("test.pending.a", OVT_EOptionScope.WORLD, 10, 0, 100, 0, "#Label", "#Desc");
		registry.Register(defA);

		if (registry.IsPending("test.pending.a"))
		{
			SetFailure("The pending mark survived Register");
			return true;
		}

		if (Math.AbsFloat(registry.GetFloat("test.pending.a") - 100) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A pending value of 9000 was not clamped to the arriving definition's max of 100; read %1", registry.GetFloat("test.pending.a").ToString());
			return true;
		}

		// --- ORDER B: register first, store second. The value must normalize immediately and never
		// be marked pending.
		OVT_OptionDefinition defB = OVT_OptionDefinition.MakeSlider("test.pending.b", OVT_EOptionScope.WORLD, 10, 0, 100, 0, "#Label", "#Desc");
		registry.Register(defB);

		registry.SetValue("test.pending.b", "-40");

		if (registry.IsPending("test.pending.b"))
		{
			SetFailure("A value stored after registration was marked pending");
			return true;
		}

		if (Math.AbsFloat(registry.GetFloat("test.pending.b") - 0) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("A value of -40 stored after registration was not clamped to 0; read %1", registry.GetFloat("test.pending.b").ToString());
			return true;
		}

		Print("Options registry: a value stored before its registration is clamped once the registration arrives, and a value stored after registration normalizes immediately in either order");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! `GetOverrides` reports only non-pending values that differ from their default. `GetPendingIds`
//! reports the ids that never found a definition.
//!
//! Fails if GetOverrides includes a pending id: the pending id in this case carries a value that has
//! never been checked against a definition, so surfacing it to the serializer would persist a
//! meaningless string.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_OverridesExcludePending : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();

		OVT_OptionDefinition atDefault = OVT_OptionDefinition.MakeToggle("test.overrides.atdefault", OVT_EOptionScope.WORLD, true, "#Label", "#Desc");
		registry.Register(atDefault);

		OVT_OptionDefinition changed = OVT_OptionDefinition.MakeSlider("test.overrides.changed", OVT_EOptionScope.WORLD, 10, 0, 100, 0, "#Label", "#Desc");
		registry.Register(changed);
		registry.SetValue("test.overrides.changed", "80");

		registry.SetValue("test.overrides.pending", "unregistered-value");

		array<string> overrideIds = new array<string>();
		array<string> overrideValues = new array<string>();
		registry.GetOverrides(overrideIds, overrideValues);

		if (overrideIds.Contains("test.overrides.pending"))
		{
			SetFailure("GetOverrides listed a pending id with no definition");
			return true;
		}

		if (overrideIds.Contains("test.overrides.atdefault"))
		{
			SetFailure("GetOverrides listed an id still at its default value");
			return true;
		}

		if (!overrideIds.Contains("test.overrides.changed"))
		{
			SetFailure("GetOverrides omitted an id explicitly moved off its default");
			return true;
		}

		int index = overrideIds.Find("test.overrides.changed");
		if (Math.AbsFloat(overrideValues.Get(index).ToFloat() - 80) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("GetOverrides paired 'test.overrides.changed' with '%1', expected 80", overrideValues.Get(index));
			return true;
		}

		array<string> pendingIds = new array<string>();
		registry.GetPendingIds(pendingIds);

		if (!pendingIds.Contains("test.overrides.pending"))
		{
			SetFailure("GetPendingIds omitted a value that was never registered");
			return true;
		}

		if (pendingIds.Contains("test.overrides.changed") || pendingIds.Contains("test.overrides.atdefault"))
		{
			SetFailure("GetPendingIds listed a registered id");
			return true;
		}

		Print("Options registry: GetOverrides excludes a pending id and an at-default value, and GetPendingIds names exactly the ids with no definition");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The three static scope predicates answer the requirements' permission table: `LOCAL` needs
//! nothing special, `FACTION` needs the officer permission, `WORLD` needs the admin permission, and
//! both `FACTION` and `WORLD` are server-authoritative.
//!
//! Fails if any predicate mixes up `FACTION` and `WORLD`: an officer-only predicate that returned
//! true for `WORLD` would let a resistance officer edit an admin-scoped option.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_ScopePermissionTable : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_OptionsRegistry.ScopeIsServerAuthoritative(OVT_EOptionScope.LOCAL))
		{
			SetFailure("LOCAL was reported server-authoritative");
			return true;
		}

		if (!OVT_OptionsRegistry.ScopeIsServerAuthoritative(OVT_EOptionScope.FACTION))
		{
			SetFailure("FACTION was reported not server-authoritative");
			return true;
		}

		if (!OVT_OptionsRegistry.ScopeIsServerAuthoritative(OVT_EOptionScope.WORLD))
		{
			SetFailure("WORLD was reported not server-authoritative");
			return true;
		}

		if (OVT_OptionsRegistry.ScopeRequiresOfficer(OVT_EOptionScope.LOCAL))
		{
			SetFailure("LOCAL was reported to require the officer permission");
			return true;
		}

		if (!OVT_OptionsRegistry.ScopeRequiresOfficer(OVT_EOptionScope.FACTION))
		{
			SetFailure("FACTION was reported not to require the officer permission");
			return true;
		}

		if (OVT_OptionsRegistry.ScopeRequiresOfficer(OVT_EOptionScope.WORLD))
		{
			SetFailure("WORLD was reported to require the officer permission; an admin write must be checked as admin, not officer");
			return true;
		}

		if (OVT_OptionsRegistry.ScopeRequiresAdmin(OVT_EOptionScope.LOCAL))
		{
			SetFailure("LOCAL was reported to require the admin permission");
			return true;
		}

		if (OVT_OptionsRegistry.ScopeRequiresAdmin(OVT_EOptionScope.FACTION))
		{
			SetFailure("FACTION was reported to require the admin permission; an officer write must be checked as officer, not admin");
			return true;
		}

		if (!OVT_OptionsRegistry.ScopeRequiresAdmin(OVT_EOptionScope.WORLD))
		{
			SetFailure("WORLD was reported not to require the admin permission");
			return true;
		}

		Print("Options registry: the scope predicates answer LOCAL as unrestricted, FACTION as officer-only, and WORLD as admin-only, with FACTION and WORLD both server-authoritative");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Round trip of a bool, a float and a choice through the one string encoding (D1).
//!
//! Fails if DecodeBool treats an unrecognised string as true: this case decodes "yes" and a lenient
//! decoder would read it as true instead of false.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Options_ValueCodec : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_OptionsRegistry.EncodeBool(true) != "1")
		{
			SetFailure("EncodeBool(true) gave '%1', expected '1'", OVT_OptionsRegistry.EncodeBool(true));
			return true;
		}

		if (OVT_OptionsRegistry.EncodeBool(false) != "0")
		{
			SetFailure("EncodeBool(false) gave '%1', expected '0'", OVT_OptionsRegistry.EncodeBool(false));
			return true;
		}

		if (!OVT_OptionsRegistry.DecodeBool("1"))
		{
			SetFailure("DecodeBool('1') read false");
			return true;
		}

		if (!OVT_OptionsRegistry.DecodeBool("true"))
		{
			SetFailure("DecodeBool('true') read false");
			return true;
		}

		if (OVT_OptionsRegistry.DecodeBool("0"))
		{
			SetFailure("DecodeBool('0') read true");
			return true;
		}

		if (OVT_OptionsRegistry.DecodeBool(""))
		{
			SetFailure("DecodeBool('') read true");
			return true;
		}

		if (OVT_OptionsRegistry.DecodeBool("yes"))
		{
			SetFailure("DecodeBool('yes') read true; an unrecognised string must decode false");
			return true;
		}

		float decodedA = OVT_OptionsRegistry.DecodeFloat(OVT_OptionsRegistry.EncodeFloat(3.5));
		if (Math.AbsFloat(decodedA - 3.5) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("Encoding and decoding 3.5 gave %1", decodedA.ToString());
			return true;
		}

		float decodedB = OVT_OptionsRegistry.DecodeFloat(OVT_OptionsRegistry.EncodeFloat(600));
		if (Math.AbsFloat(decodedB - 600) > OVT_TEST_OPTIONS_EPSILON)
		{
			SetFailure("Encoding and decoding 600 gave %1", decodedB.ToString());
			return true;
		}

		OVT_OptionsRegistry registry = new OVT_OptionsRegistry();
		array<string> keys = new array<string>();
		keys.Insert("north");
		keys.Insert("south");

		OVT_OptionDefinition choice = OVT_OptionDefinition.MakeChoice("test.codec.choice", OVT_EOptionScope.LOCAL, "north", keys, keys, "#Label", "#Desc");
		registry.Register(choice);
		registry.SetValue("test.codec.choice", "south");

		if (registry.GetChoice("test.codec.choice") != "south")
		{
			SetFailure("A choice round trip through SetValue/GetChoice gave '%1', expected 'south'", registry.GetChoice("test.codec.choice"));
			return true;
		}

		Print("Options registry: bool, float and choice values all round trip through the string encoding, and an unrecognised bool string decodes false");

		return true;
	}
}
