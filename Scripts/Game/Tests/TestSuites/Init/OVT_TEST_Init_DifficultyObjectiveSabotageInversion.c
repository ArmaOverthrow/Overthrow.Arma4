//------------------------------------------------------------------------------------------------
//! TIER B - the five shipped difficulty presets load, and the sabotage gate scales BACKWARDS.
//!
//! WHAT IS BEING PINNED (occupying/counter-attacks G11 / D10). Every other difficulty knob in the
//! tree gets harder as the preset gets harder. objectiveSabotageMissionsRequired deliberately does
//! the opposite: it is the number of successful sabotage missions the occupying faction must land at
//! a base objective before it is allowed to counter-attack, so a HIGHER number means MORE warning.
//! Easy therefore demands six and Insane demands two, and the ordering
//! Easy >= Normal >= Hard >= Extreme >= Insane must hold with at least one strict step.
//!
//! WHY THAT NEEDS AN ASSERTION AT ALL. The inversion is authored in five .conf files, not computed.
//! It looks exactly like a typo to anyone editing the presets - the natural instinct while tuning
//! "Insane" is to raise every number in the file - and getting it backwards has no symptom a play-test
//! would catch: the counter-attack simply arrives sooner on Easy than on Insane, tens of in-game
//! minutes earlier, in a phase machine that legitimately takes that long anyway. The clamping half of
//! the rule is pinned in the cheap tier (OVT_TEST_Logic_ObjectiveScaling.c); the ORDERING can only be
//! seen where configs are loaded, which is here.
//!
//! WHY THIS TIER. m_aDifficultyPresets is populated by the config component at world load, so the
//! presets exist without a started campaign. Nothing here mutates anything - the case only reads the
//! loaded values back.
//!
//! PRESETS ARE FOUND BY NAME, NEVER BY INDEX. A world layer APPENDS to the prefab's preset list
//! rather than replacing it (OVT_OverthrowConfigComponent.c:159-162), so the test world's list is the
//! five shipped presets plus "Test World" and index 2 is not stable across worlds. A missing preset
//! fails loudly rather than passing vacuously.
//!
//! TEST WORLD IS EXCLUDED ON PURPOSE. Difficulty_TestWorld.conf authors none of the objective fields
//! and inherits every declared default; it is not part of the shipped difficulty ramp and asserting
//! an ordering against it would pin a default, not a design.
//!
//! PROVEN ABLE TO FAIL: Difficulty_Easy.conf's objectiveSabotageMissionsRequired was set to 1 - the
//! plausible "make Easy easier" edit, and the exact defect this case exists for. The tree recompiled
//! CLEAN (tools/compile-check.sh exit 0), which is the point: a backwards difficulty curve is not a
//! syntax error and nothing else in the tree would stop it shipping. The case then goes red on
//! "the sabotage gate must not INCREASE with difficulty: Easy requires 1 but the next preset
//! requires 5 - the field is inverted on purpose (easier = more warning)". Value restored, tree
//! recompiled clean.
//! A second fault - flattening all five to 4 - leaves the ordering valid but kills the strict step,
//! and the case then goes red on the "at least one strict step" assertion at the bottom, which is
//! what stops a uniformly-authored ramp from passing this case vacuously.
//! No polling, no timing, no async step anywhere in the case - no maxAttempts.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]
class OVT_TEST_Init_DifficultyObjectiveSabotageInversion : SCR_AutotestCaseBase
{
	//! The shipped ramp, hardest last. Difficulty_TestWorld.conf is deliberately absent.
	protected static const ref array<string> SHIPPED_PRESETS = {"Easy", "Normal", "Hard", "Extreme", "Insane"};

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
		{
			SetFailure("OVT_Global.GetConfig() is null");
			return true;
		}

		if (!config.m_aDifficultyPresets || config.m_aDifficultyPresets.IsEmpty())
		{
			SetFailure("The config component carries no difficulty presets, so the shipped ramp cannot be read");
			return true;
		}

		array<int> required = new array<int>();

		foreach (string presetName : SHIPPED_PRESETS)
		{
			OVT_DifficultySettings preset = FindPreset(config, presetName);
			if (!preset)
			{
				SetFailure("No shipped difficulty preset named '%1' is loaded - the ramp is incomplete", presetName);
				return true;
			}

			// An unauthored int field reads 0. Every shipped preset must author this one, or the
			// ordering below would be comparing defaults rather than a design.
			if (preset.objectiveSabotageMissionsRequired < 1)
			{
				SetFailure("Preset '%1' does not author objectiveSabotageMissionsRequired (reads %2)",
					presetName, preset.objectiveSabotageMissionsRequired.ToString());
				return true;
			}

			required.Insert(preset.objectiveSabotageMissionsRequired);
		}

		// --- THE INVERSION. Non-increasing from Easy to Insane, adjacent pair by adjacent pair, so the
		//     failure names the two presets that disagree rather than just the sequence.
		for (int i = 1; i < required.Count(); i++)
		{
			if (required[i] > required[i - 1])
			{
				SetFailure("the sabotage gate must not INCREASE with difficulty: %1 requires %2 but the next preset requires %3 - the field is inverted on purpose (easier = more warning)",
					SHIPPED_PRESETS[i - 1], required[i - 1].ToString(), required[i].ToString());
				return true;
			}
		}

		// --- AT LEAST ONE STRICT STEP, or a ramp authored flat everywhere would pass the loop above
		//     while scaling nothing at all.
		if (required[0] <= required[required.Count() - 1])
		{
			SetFailure("Easy requires %1 sabotage missions and Insane requires %2 - the ramp is flat or backwards, so difficulty scales nothing here",
				required[0].ToString(), required[required.Count() - 1].ToString());
			return true;
		}

		Print("Difficulty ramp: objectiveSabotageMissionsRequired is non-increasing Easy -> Insane (" + required[0].ToString() + " -> " + required[required.Count() - 1].ToString() + "), so an easier campaign gets more warning before the counter-attack");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds a loaded difficulty preset by its configured name.
	//! \param[in] config The Overthrow config component holding the preset list.
	//! \param[in] presetName Name to match exactly (OVT_DifficultySettings.name).
	//! \return The matching preset, or null when the list has no entry with that name.
	protected OVT_DifficultySettings FindPreset(OVT_OverthrowConfigComponent config, string presetName)
	{
		foreach (OVT_DifficultySettings preset : config.m_aDifficultyPresets)
		{
			if (preset && preset.name == presetName)
				return preset;
		}

		return null;
	}
}
