//------------------------------------------------------------------------------------------------
//! TIER A cases - skill effects and player levelling maths.
//!
//! A skill effect is a pure function from a configured value onto one field of a player record, so
//! it can be exercised against a hand-built player with no manager and no world. The player record
//! is also where the level curve lives, which is pure arithmetic on the xp field.
//!
//! OVT_StaminaSkillEffect IS DELIBERATELY NOT ASSERTED. Its OnPlayerSpawn() resolves a stamina
//! component and then does nothing, with a comment saying the engine exposes no stamina parameters
//! to change - so it is intentionally inert. A case over it could only assert that nothing happens,
//! which would pass for the wrong reason the moment the effect is implemented, and it needs a
//! spawned character this tier does not have. It is recorded here and in findings.md instead.
//!
//! NO CASE IN THIS FILE PINS A BUG. The plan expected OVT_PlayerData.GetLevelProgress() to be
//! broken by integer division (Decision 10 / risk R7) and budgeted a pinning case for it. Measured
//! on 1.7.0.54 it is not broken - EnforceScript does not truncate a division between two ints in
//! these expressions - so the case asserts the correct fractional progress instead. Recorded in
//! findings.md under "Differs from assumptions".
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Each skill effect writes exactly the field it claims, and leaves the others alone.
//!
//! The "leaves the others alone" half is the point. Every effect receives the whole player record,
//! so an effect that reached one field too far would be invisible until a player happened to have
//! two skills at once - at which point the symptom would be a wrong shop price attributed to the
//! trade skill rather than to the stealth skill that actually caused it.
//!
//! Each effect gets a FRESH player so the four claims are independent, and the untouched fields are
//! compared against the record's own declared starting values (price and stealth multipliers 1,
//! diplomacy 0.1, no permissions).
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Skills_EffectsWriteOnlyTheirOwnField : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A fresh record starts at the neutral values every effect is measured against.
		OVT_PlayerData fresh = new OVT_PlayerData();
		if (!OVT_TEST_LogicFixture.FloatEquals(fresh.priceMultiplier, 1) || !OVT_TEST_LogicFixture.FloatEquals(fresh.stealthMultiplier, 1))
		{
			SetFailure("A fresh player record starts at priceMultiplier %1 / stealthMultiplier %2, expected 1 / 1",
				fresh.priceMultiplier.ToString(), fresh.stealthMultiplier.ToString());
			return true;
		}

		if (!OVT_TEST_LogicFixture.FloatEquals(fresh.diplomacy, 0.1))
		{
			SetFailure("A fresh player record starts at diplomacy %1, expected 0.1", fresh.diplomacy.ToString());
			return true;
		}

		// Trade discount -> priceMultiplier only.
		OVT_PlayerData trader = new OVT_PlayerData();
		OVT_TradeDiscountSkillEffect discount = new OVT_TradeDiscountSkillEffect();
		discount.m_fDiscount = 0.25;
		discount.OnPlayerData(trader);

		if (!OVT_TEST_LogicFixture.FloatEquals(trader.priceMultiplier, 0.75))
		{
			SetFailure("A 0.25 trade discount left priceMultiplier at %1, expected 0.75", trader.priceMultiplier.ToString());
			return true;
		}

		string traderSpill = FindSpill(trader, true, false, false, false);
		if (traderSpill != "")
		{
			SetFailure("The trade discount effect also wrote %1", traderSpill);
			return true;
		}

		// Stealth -> stealthMultiplier only.
		OVT_PlayerData sneak = new OVT_PlayerData();
		OVT_StealthSkillEffect stealth = new OVT_StealthSkillEffect();
		stealth.m_fDistanceMul = 0.4;
		stealth.m_fDetectionTimeMul = 0.3;
		stealth.m_fDisguiseBonus = 0.2;
		stealth.OnPlayerData(sneak);

		if (!OVT_TEST_LogicFixture.FloatEquals(sneak.stealthMultiplier, 0.6))
		{
			SetFailure("A 0.4 distance multiplier left stealthMultiplier at %1, expected 0.6", sneak.stealthMultiplier.ToString());
			return true;
		}

		string sneakSpill = FindSpill(sneak, false, true, false, false);
		if (sneakSpill != "")
		{
			SetFailure("The stealth effect also wrote %1", sneakSpill);
			return true;
		}

		// The other two stealth values are read back through their own accessors and are NOT put on
		// the player record at all - they are pulled by the detection code when it needs them.
		if (!OVT_TEST_LogicFixture.FloatEquals(stealth.GetDetectionTimeBonus(), 0.3) || !OVT_TEST_LogicFixture.FloatEquals(stealth.GetDisguiseBonus(), 0.2))
		{
			SetFailure("Stealth accessors returned detection %1 / disguise %2, expected 0.3 / 0.2",
				stealth.GetDetectionTimeBonus().ToString(), stealth.GetDisguiseBonus().ToString());
			return true;
		}

		// Support -> diplomacy only. Note this one ASSIGNS rather than adjusting, so it can lower
		// the starting 0.1 as well as raise it.
		OVT_PlayerData diplomat = new OVT_PlayerData();
		OVT_SupportSkillEffect support = new OVT_SupportSkillEffect();
		support.m_fSupportChance = 0.35;
		support.OnPlayerData(diplomat);

		if (!OVT_TEST_LogicFixture.FloatEquals(diplomat.diplomacy, 0.35))
		{
			SetFailure("A 0.35 support chance left diplomacy at %1, expected 0.35", diplomat.diplomacy.ToString());
			return true;
		}

		string diplomatSpill = FindSpill(diplomat, false, false, true, false);
		if (diplomatSpill != "")
		{
			SetFailure("The support effect also wrote %1", diplomatSpill);
			return true;
		}

		// Permission -> permissions only.
		OVT_PlayerData officer = new OVT_PlayerData();
		OVT_GivePermissionSkillEffect permission = new OVT_GivePermissionSkillEffect();
		permission.m_sPermission = "OVT_TEST_PERMISSION";
		permission.m_sDescription = "granted by a Tier A case";
		permission.OnPlayerData(officer);

		if (!officer.HasPermission("OVT_TEST_PERMISSION"))
		{
			SetFailure("The permission effect did not grant OVT_TEST_PERMISSION");
			return true;
		}

		string officerSpill = FindSpill(officer, false, false, false, true);
		if (officerSpill != "")
		{
			SetFailure("The permission effect also wrote %1", officerSpill);
			return true;
		}

		Print("Skill effects: trade -> priceMultiplier, stealth -> stealthMultiplier, support -> diplomacy, permission -> permissions; no cross-writes");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Names the first field that moved away from a fresh record's starting value and was NOT the
	//! field the effect under test is allowed to write.
	//! \param[in] player The record an effect was just applied to.
	//! \param[in] priceExpected True when priceMultiplier is allowed to have changed.
	//! \param[in] stealthExpected True when stealthMultiplier is allowed to have changed.
	//! \param[in] diplomacyExpected True when diplomacy is allowed to have changed.
	//! \param[in] permissionExpected True when permissions is allowed to have changed.
	//! \return The name of the first field written out of turn, or an empty string.
	protected string FindSpill(OVT_PlayerData player, bool priceExpected, bool stealthExpected, bool diplomacyExpected, bool permissionExpected)
	{
		if (!priceExpected && !OVT_TEST_LogicFixture.FloatEquals(player.priceMultiplier, 1))
			return "priceMultiplier";

		if (!stealthExpected && !OVT_TEST_LogicFixture.FloatEquals(player.stealthMultiplier, 1))
			return "stealthMultiplier";

		if (!diplomacyExpected && !OVT_TEST_LogicFixture.FloatEquals(player.diplomacy, 0.1))
			return "diplomacy";

		if (!permissionExpected && player.permissions.Count() != 0)
			return "permissions";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_GivePermissionSkillEffect is idempotent, and distinct permissions accumulate.
//!
//! Skill effects are re-applied every time a player's skills are recalculated, so a permission that
//! appended unconditionally would grow the player's permission array without bound for as long as
//! the session ran. OVT_PlayerData.GivePermission() guards against that; this case is what keeps
//! the guard.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Skills_GivePermissionIsIdempotent : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_PlayerData player = new OVT_PlayerData();

		OVT_GivePermissionSkillEffect effect = new OVT_GivePermissionSkillEffect();
		effect.m_sPermission = "OVT_TEST_PERMISSION";
		effect.m_sDescription = "granted by a Tier A case";

		if (player.HasPermission("OVT_TEST_PERMISSION"))
		{
			SetFailure("A fresh player record already holds OVT_TEST_PERMISSION");
			return true;
		}

		effect.OnPlayerData(player);
		effect.OnPlayerData(player);
		effect.OnPlayerData(player);

		if (!player.HasPermission("OVT_TEST_PERMISSION"))
		{
			SetFailure("OVT_TEST_PERMISSION was not granted at all");
			return true;
		}

		if (player.permissions.Count() != 1)
		{
			SetFailure("Applying the same permission effect three times left %1 permissions, expected 1", player.permissions.Count().ToString());
			return true;
		}

		// A different permission is a different entry, not a replacement.
		OVT_GivePermissionSkillEffect other = new OVT_GivePermissionSkillEffect();
		other.m_sPermission = "OVT_TEST_OTHER_PERMISSION";
		other.m_sDescription = "granted by a Tier A case";
		other.OnPlayerData(player);

		if (player.permissions.Count() != 2)
		{
			SetFailure("Granting a second, different permission left %1 permissions, expected 2", player.permissions.Count().ToString());
			return true;
		}

		if (!player.HasPermission("OVT_TEST_PERMISSION") || !player.HasPermission("OVT_TEST_OTHER_PERMISSION"))
		{
			SetFailure("One of the two granted permissions is missing after granting the second");
			return true;
		}

		if (player.HasPermission("OVT_TEST_NEVER_GRANTED"))
		{
			SetFailure("HasPermission() reported a permission that was never granted");
			return true;
		}

		Print("GivePermission: three applications leave one entry, a second permission adds a second entry");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_PlayerData level accessors - the xp curve, the level thresholds and the skill count.
//!
//! The curve is rawLevel = 1 + 0.1 * sqrt(xp), so a level costs quadratically more xp than the last
//! one, and GetLevelXP(n) is its inverse: the xp at which level n + 1 is reached.
//!
//! Every observed value is printed before the assertions run, so a single failing run shows the
//! whole picture in console.log rather than only the first mismatch.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Player_LevelAccessors : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_PlayerData player = new OVT_PlayerData();

		PrintFormat("Level curve: xp 0 -> raw %1 level %2, nextLevelXP %3",
			player.GetRawLevel().ToString(), player.GetLevel().ToString(), player.GetNextLevelXP().ToString());
		PrintFormat("Level thresholds: GetLevelXP(0) %1, GetLevelXP(1) %2, GetLevelXP(2) %3",
			player.GetLevelXP(0).ToString(), player.GetLevelXP(1).ToString(), player.GetLevelXP(2).ToString());

		// A brand-new player is level 1 with no xp.
		if (!OVT_TEST_LogicFixture.FloatEquals(player.GetRawLevel(), 1))
		{
			SetFailure("GetRawLevel() with 0 xp returned %1, expected 1", player.GetRawLevel().ToString());
			return true;
		}

		if (player.GetLevel() != 1)
		{
			SetFailure("GetLevel() with 0 xp returned %1, expected 1", player.GetLevel().ToString());
			return true;
		}

		// The thresholds are the inverse of the curve: level n + 1 is reached at (n / 0.1)^2 xp.
		if (player.GetLevelXP(0) != 0)
		{
			SetFailure("GetLevelXP(0) returned %1, expected 0", player.GetLevelXP(0).ToString());
			return true;
		}

		if (player.GetLevelXP(1) != 100)
		{
			SetFailure("GetLevelXP(1) returned %1, expected 100", player.GetLevelXP(1).ToString());
			return true;
		}

		if (player.GetLevelXP(2) != 400)
		{
			SetFailure("GetLevelXP(2) returned %1, expected 400", player.GetLevelXP(2).ToString());
			return true;
		}

		// GetNextLevelXP() is the threshold of the level the player is currently on.
		if (player.GetNextLevelXP() != 100)
		{
			SetFailure("GetNextLevelXP() at 0 xp returned %1, expected 100", player.GetNextLevelXP().ToString());
			return true;
		}

		// Partway into level 1: the raw level moves continuously, the integer level does not.
		player.xp = 150;
		if (player.GetLevel() != 2)
		{
			SetFailure("GetLevel() with 150 xp returned %1, expected 2", player.GetLevel().ToString());
			return true;
		}

		if (!OVT_TEST_LogicFixture.FloatEquals(player.GetRawLevel(), 1 + (0.1 * Math.Sqrt(150))))
		{
			SetFailure("GetRawLevel() with 150 xp returned %1, which is not 1 + 0.1 * sqrt(150)", player.GetRawLevel().ToString());
			return true;
		}

		if (player.GetNextLevelXP() != 400)
		{
			SetFailure("GetNextLevelXP() at 150 xp (level 2) returned %1, expected 400", player.GetNextLevelXP().ToString());
			return true;
		}

		player.xp = 500;
		if (player.GetLevel() != 3)
		{
			SetFailure("GetLevel() with 500 xp returned %1, expected 3", player.GetLevel().ToString());
			return true;
		}

		// CountSkills sums the LEVELS held across every skill, not the number of skills.
		if (player.CountSkills() != 0)
		{
			SetFailure("CountSkills() on a player with no skills returned %1, expected 0", player.CountSkills().ToString());
			return true;
		}

		player.skills.Insert("OVT_TEST_SKILL_A", 2);
		player.skills.Insert("OVT_TEST_SKILL_B", 3);

		if (player.CountSkills() != 5)
		{
			SetFailure("CountSkills() with skill levels 2 and 3 returned %1, expected 5", player.CountSkills().ToString());
			return true;
		}

		Print("Level accessors: curve, thresholds and skill count all as specified");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! OVT_PlayerData.GetLevelProgress() - fractional progress through the current level.
//!
//! The method locates the xp thresholds either side of the player's current level and returns how
//! far between them the player is, which is what the character sheet's progress bar draws.
//!
//! THIS CASE WAS PLANNED AS A BUG PIN AND IS NOT ONE. `int current / int total` returned into a
//! float looked certain to truncate to 0 for every xp inside a level. It does not on 1.7.0.54: the
//! division produces a real fraction (findings.md, Phase 5, "Differs from assumptions"). The case
//! therefore asserts the intended behaviour - and it is exactly the case that would catch the
//! truncation if a future engine or script change introduced it, because every row below would
//! collapse to 0.
//!
//! Expected values are DERIVED from the level thresholds rather than hardcoded, so a change to the
//! xp curve moves both sides of the comparison instead of silently invalidating the case. The one
//! independent claim is the ordering assertion at the end: progress must rise with xp inside a
//! level, which no arithmetic slip can satisfy accidentally.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_Player_LevelProgress_IsFractionalWithinTheLevel : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_PlayerData player = new OVT_PlayerData();

		// Sitting exactly on a level threshold: no progress into the new level yet.
		player.xp = 0;
		if (!OVT_TEST_LogicFixture.FloatEquals(player.GetLevelProgress(), 0))
		{
			SetFailure("GetLevelProgress() at 0 xp returned %1, expected 0", player.GetLevelProgress().ToString());
			return true;
		}

		// Halfway from the level 1 threshold (0 xp) to the level 2 threshold (100 xp).
		player.xp = 50;
		float halfway = ExpectedProgress(player);
		if (!OVT_TEST_LogicFixture.FloatEquals(player.GetLevelProgress(), halfway))
		{
			SetFailure("GetLevelProgress() at 50 xp returned %1, expected %2",
				player.GetLevelProgress().ToString(), halfway.ToString());
			return true;
		}

		if (!OVT_TEST_LogicFixture.FloatEquals(halfway, 0.5))
		{
			SetFailure("The level curve moved: 50 xp is now %1 of the way through level 1, not 0.5", halfway.ToString());
			return true;
		}

		// Well into level 2: 250 of the 300 xp between the 100 and 400 thresholds.
		player.xp = 350;
		float deepIntoLevel = ExpectedProgress(player);
		if (!OVT_TEST_LogicFixture.FloatEquals(player.GetLevelProgress(), deepIntoLevel))
		{
			SetFailure("GetLevelProgress() at 350 xp returned %1, expected %2",
				player.GetLevelProgress().ToString(), deepIntoLevel.ToString());
			return true;
		}

		// One xp short of the next level: almost, but not quite, 1.
		player.xp = 399;
		float almostThere = player.GetLevelProgress();
		if (!OVT_TEST_LogicFixture.FloatEquals(almostThere, ExpectedProgress(player)))
		{
			SetFailure("GetLevelProgress() at 399 xp returned %1, expected %2",
				almostThere.ToString(), ExpectedProgress(player).ToString());
			return true;
		}

		if (almostThere >= 1 || almostThere <= deepIntoLevel)
		{
			SetFailure("Progress did not rise with xp inside a level: 350 xp gave %1 and 399 xp gave %2, and the latter must be between the former and 1",
				deepIntoLevel.ToString(), almostThere.ToString());
			return true;
		}

		PrintFormat("Level progress: 50 xp -> %1, 350 xp -> %2, 399 xp -> %3",
			halfway.ToString(), deepIntoLevel.ToString(), almostThere.ToString());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Recomputes the expected progress from the record's own level thresholds, so the expectation
	//! follows a curve change instead of pinning yesterday's numbers.
	//! \param[in] player The record to measure.
	//! \return Fraction of the way from the current level's threshold to the next one.
	protected float ExpectedProgress(OVT_PlayerData player)
	{
		float from = player.GetLevelXP(player.GetLevel() - 1);
		float to = player.GetNextLevelXP();
		return (player.xp - from) / (to - from);
	}
}
