//------------------------------------------------------------------------------------------------
//! TIER A cases - recruit status, starting with the one claim the record itself makes.
//!
//! WHAT IS HERE. Two subjects, both world-free. The first is the record's own starting value for the
//! inactive flag (Phase 1). The second is OVT_RecruitStatus (Phase 4): the packing of four measured
//! facts into the status mask that crosses the wire, the unpacking of that mask, and the map tag
//! chosen from it. The MEASURING half - weapon slots, muzzles, magazines, damage state - needs a live
//! body and is not reachable from this tier; it lives in the recruit manager and is play-test
//! territory. Phase 2's clustering selection is pinned in a file of its own.
//!
//! WHY THE DEFAULT IS WORTH A CASE AT ALL. "Every recruit starts in your squad" is the assumption
//! every other part of the feature is built on: the roster puts a new hire in the Active section, the
//! group transfer offers it to a friend's group, fast travel takes it along. If a hand-built record
//! ever started INACTIVE, every one of those would quietly do the opposite of what the player asked,
//! and nothing else in the tree asserts it. The value is a plain field initialiser, which IS applied
//! by `new` - unlike an [Attribute()] defvalue, which is not (see the house rules in the suite
//! header). This case is what keeps those two kinds of default from being confused.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 1, by deliberate fault + compile-check; running the suite is
//! the orchestrator's job, not this file's):
//!   OVT_RecruitData.m_bInactive's initialiser was flipped to `= true` and the tree recompiled. It
//!   compiled CLEAN - which is the whole point, since a wrong default is not a syntax error - and the
//!   first assertion below is then false by construction, so the case fails on "a freshly constructed
//!   recruit record is INACTIVE". The fault was reverted and the tree recompiled clean again.
//!   Deliberately NOT claimed: removing the initialiser altogether. A bool with no initialiser is
//!   false anyway, so that variant would pass and is not a valid fault for this claim.
//!   No maxAttempts: this is one `new` and a handful of comparisons, and it cannot flake.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! A recruit record is ACTIVE the moment it is constructed, and the flag is independent of liveness.
//!
//! The second half is the subtler claim: inactive (squad membership) and online (having a body) are
//! two different facts, and a record must be able to hold any combination of them. A future change
//! that made one derive from the other - "offline recruits are inactive", say - would pass the first
//! assertion and fail here.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitStatus_NewRecruitIsActive : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_RecruitData recruit = new OVT_RecruitData();

		if (recruit.m_bInactive)
		{
			SetFailure("A freshly constructed recruit record is INACTIVE - a new recruit must start in its owner's group");
			return true;
		}

		// The two flags are independent. Giving this record a body must not touch its membership,
		// and taking the body away must not park it.
		recruit.m_bIsOnline = true;
		if (recruit.m_bInactive)
		{
			SetFailure("Marking a recruit record ONLINE also marked it inactive - the two flags describe different things");
			return true;
		}

		recruit.m_bIsOnline = false;
		if (recruit.m_bInactive)
		{
			SetFailure("Marking a recruit record OFFLINE also marked it inactive - having no body is not the same as being parked");
			return true;
		}

		// And the flag is writable in both directions, which is what every later phase does to it.
		recruit.m_bInactive = true;
		if (!recruit.m_bInactive)
		{
			SetFailure("A recruit record refused to be marked inactive");
			return true;
		}

		recruit.m_bInactive = false;
		if (recruit.m_bInactive)
		{
			SetFailure("A recruit record refused to be marked active again");
			return true;
		}

		Print("Recruit record: constructed ACTIVE, membership independent of liveness, settable both ways");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Every one of the sixteen combinations of the four status facts survives packing into the mask.
//!
//! WHY A WHOLE MATRIX FOR SIXTEEN LINES OF BIT-OR. Because the mask is a WIRE FORMAT and a STORED
//! VALUE, not an internal detail: the server measures it, an owner-targeted RPC carries it, and the
//! map layer and the roster row both branch on it. The two ways that quietly goes wrong are a bit
//! written into the wrong slot (armed recruits render as wounded) and one fact leaking into another
//! (an unconscious recruit reported as unarmed). Both are invisible to the compiler and neither is
//! caught by testing one or two "interesting" combinations, so all sixteen are walked.
//!
//! The constants are pinned FIRST and by value. Everything below - and every consumer in the tree -
//! assumes 1/2/4/8, distinct and non-overlapping; if a future edit renumbered them, the loop below
//! would renumber with them and pass while every stored and in-flight mask in a live session meant
//! something else.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 4, by deliberate fault + compile-check):
//!   OVT_RecruitStatus.Derive()'s hasAmmo branch was changed to set ARMED instead of HAS_AMMO. The
//!   tree compiled CLEAN - which is the point, since a bit written into the wrong slot is not a
//!   syntax error - and combination 2 (has-ammo only) then derives 1 instead of 2, so the case fails
//!   on "combination 2 derived mask 1". The fault was reverted and the tree recompiled clean again.
//!   No maxAttempts: this is arithmetic on hand-built values with no clock, no RNG and no world.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitStatus_DeriveCoversEveryCombination : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_RecruitStatus.ARMED != 1)
		{
			SetFailure("OVT_RecruitStatus.ARMED is not 1 - the status mask is a wire and save format and its bit values are fixed");
			return true;
		}

		if (OVT_RecruitStatus.HAS_AMMO != 2)
		{
			SetFailure("OVT_RecruitStatus.HAS_AMMO is not 2 - the status mask is a wire and save format and its bit values are fixed");
			return true;
		}

		if (OVT_RecruitStatus.WOUNDED != 4)
		{
			SetFailure("OVT_RecruitStatus.WOUNDED is not 4 - the status mask is a wire and save format and its bit values are fixed");
			return true;
		}

		if (OVT_RecruitStatus.UNCONSCIOUS != 8)
		{
			SetFailure("OVT_RecruitStatus.UNCONSCIOUS is not 8 - the status mask is a wire and save format and its bit values are fixed");
			return true;
		}

		// Nothing measured means nothing set. Stated separately from the loop because "0 in, 0 out" is
		// the case every consumer relies on for a recruit the server has not described yet.
		if (OVT_RecruitStatus.Derive(false, false, false, false) != 0)
		{
			SetFailure("Deriving from four false facts produced a non-zero status mask");
			return true;
		}

		// Sixteen combinations, walked by treating the loop counter AS the expected mask: bit 0 of the
		// counter is the armed input and must come back as ARMED, bit 1 is has-ammo and must come back
		// as HAS_AMMO, and so on. Equality with the counter therefore asserts three things at once -
		// each fact set its own bit, no fact set another's, and no fact set a bit nobody asked for.
		for (int combination = 0; combination < 16; combination++)
		{
			bool armed = (combination & OVT_RecruitStatus.ARMED) != 0;
			bool hasAmmo = (combination & OVT_RecruitStatus.HAS_AMMO) != 0;
			bool wounded = (combination & OVT_RecruitStatus.WOUNDED) != 0;
			bool unconscious = (combination & OVT_RecruitStatus.UNCONSCIOUS) != 0;

			int flags = OVT_RecruitStatus.Derive(armed, hasAmmo, wounded, unconscious);

			if (flags != combination)
			{
				SetFailure("Status combination " + combination.ToString() + " derived mask " + flags.ToString() + " - a status fact set the wrong bit, another fact's bit, or a bit nobody asked for");
				return true;
			}
		}

		// The bits are independent BOTH WAYS, and this is the half the loop above cannot state on its
		// own: the mask that means "armed with ammunition" must not also claim the recruit is hurt.
		int healthyFighter = OVT_RecruitStatus.Derive(true, true, false, false);
		if (OVT_RecruitStatus.IsWounded(healthyFighter) || OVT_RecruitStatus.IsUnconscious(healthyFighter))
		{
			SetFailure("An armed, supplied, unhurt recruit derived a mask claiming it was wounded or unconscious");
			return true;
		}

		// And a casualty carries no weapon claim it was not given.
		int casualty = OVT_RecruitStatus.Derive(false, false, true, true);
		if (OVT_RecruitStatus.IsArmed(casualty) || OVT_RecruitStatus.HasAmmo(casualty))
		{
			SetFailure("An unarmed casualty derived a mask claiming it was armed or supplied");
			return true;
		}

		Print("Recruit status: all 16 fact combinations pack and survive; bit values 1/2/4/8 pinned");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The map tag has exactly three outcomes, and a weapon is what decides whether there is one at all.
//!
//! THE CLAIM THAT MATTERS IS THE FIRST ONE: an UNARMED recruit shows no tag whatever else is true of
//! it. Ammunition on somebody with nothing to fire it with is cargo, not capability, and a marker
//! that showed a magazine badge over an unarmed civilian-clothed recruit would be read by every
//! player as "this one is ready". The other two outcomes are the pair the feature exists for -
//! armed-and-supplied against armed-and-dry, which is the one status a player is expected to act on.
//!
//! WOUNDED AND UNCONSCIOUS ARE ASSERTED TO BE IRRELEVANT HERE, deliberately: they are not weapon
//! states and the marker layer expresses them another way, so a future edit that folded them into
//! this decision would change what an existing icon means without touching the icon.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 4, by deliberate fault + compile-check):
//!   OVT_RecruitStatus.TagIcon()'s unarmed branch was changed to return TAG_ARMED_EMPTY instead of
//!   TAG_NONE. The tree compiled CLEAN - a wrong-but-valid return is not a syntax error - and the
//!   first assertion below then sees "recruit_ammo_empty" where it requires an empty string, so the
//!   case fails on "An unarmed recruit was given a weapon tag". The fault was reverted and the tree
//!   recompiled clean again.
//!   Deliberately NOT claimed: swapping the two armed branches. That fault is caught too (the
//!   armed-with-ammo assertion below), but it is the same claim tested twice, not a second claim.
//!   No maxAttempts: three string comparisons on hand-built masks.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitStatus_TagIconChoosesByWeaponThenAmmo : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// Nothing known about a recruit is not a tag.
		if (OVT_RecruitStatus.TagIcon(0) != "")
		{
			SetFailure("A recruit with no status flags at all was given a weapon tag");
			return true;
		}

		// Unarmed, whatever else is true. Walked across every combination of the OTHER three facts so
		// that no single one of them can smuggle a tag in.
		for (int other = 0; other < 8; other++)
		{
			bool hasAmmo = (other & 1) != 0;
			bool wounded = (other & 2) != 0;
			bool unconscious = (other & 4) != 0;

			int unarmed = OVT_RecruitStatus.Derive(false, hasAmmo, wounded, unconscious);

			if (OVT_RecruitStatus.TagIcon(unarmed) != "")
			{
				SetFailure("An unarmed recruit was given a weapon tag (ammo=" + hasAmmo.ToString() + " wounded=" + wounded.ToString() + " unconscious=" + unconscious.ToString() + ") - ammunition without a weapon is cargo, not capability");
				return true;
			}
		}

		// Armed and supplied.
		int ready = OVT_RecruitStatus.Derive(true, true, false, false);
		if (OVT_RecruitStatus.TagIcon(ready) != "recruit_ammo")
		{
			SetFailure("An armed recruit with ammunition did not get the recruit_ammo tag, it got '" + OVT_RecruitStatus.TagIcon(ready) + "'");
			return true;
		}

		// Armed and dry - the status the player is meant to act on.
		int dry = OVT_RecruitStatus.Derive(true, false, false, false);
		if (OVT_RecruitStatus.TagIcon(dry) != "recruit_ammo_empty")
		{
			SetFailure("An armed recruit with no ammunition did not get the recruit_ammo_empty tag, it got '" + OVT_RecruitStatus.TagIcon(dry) + "'");
			return true;
		}

		// The two armed outcomes are different pictures. Asserted rather than assumed, because both
		// quad names are placeholders pointing at the same atlas today and a copy-paste that made them
		// literally equal would pass both assertions above.
		if (OVT_RecruitStatus.TagIcon(ready) == OVT_RecruitStatus.TagIcon(dry))
		{
			SetFailure("Armed-with-ammo and armed-out-of-ammo resolve to the same tag quad - the one distinction this tag exists to draw");
			return true;
		}

		// Being hurt does not change the weapon tag, in either direction.
		int woundedReady = OVT_RecruitStatus.Derive(true, true, true, true);
		if (OVT_RecruitStatus.TagIcon(woundedReady) != "recruit_ammo")
		{
			SetFailure("A wounded, unconscious but armed and supplied recruit changed weapon tag - wounded and unconscious are not weapon states");
			return true;
		}

		int woundedDry = OVT_RecruitStatus.Derive(true, false, true, true);
		if (OVT_RecruitStatus.TagIcon(woundedDry) != "recruit_ammo_empty")
		{
			SetFailure("A wounded, unconscious, armed and dry recruit changed weapon tag - wounded and unconscious are not weapon states");
			return true;
		}

		Print("Recruit status tag: none when unarmed (all 8 other combinations), recruit_ammo when supplied, recruit_ammo_empty when dry");

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Each predicate reads its own bit and only its own bit.
//!
//! WHY SEPARATELY FROM Derive(). The consumers of this class do not read masks the way the matrix
//! case does - the roster row and the marker layer never compare a mask to a number, they ask
//! IsArmed() / HasAmmo() / IsWounded() / IsUnconscious(). A predicate reading the wrong constant
//! would leave Derive() perfectly correct and every screen wrong, and the matrix case would still
//! pass.
//!
//! THE TWO EXTREMES ARE WHERE PREDICATES ACTUALLY BREAK. On a ZERO mask every predicate must say no,
//! which is what a client answers for a recruit the server has not described yet - a predicate that
//! got its polarity backwards would light up the entire roster on session start. On the FULL mask
//! every predicate must say yes, which is what catches a predicate testing against the whole mask
//! rather than its bit. Between those, each bit is set alone and the other three are required to
//! stay silent, which is what catches two predicates sharing a constant.
//!
//! PROVEN ABLE TO FAIL (recruit-ux Phase 4, by deliberate fault + compile-check):
//!   OVT_RecruitStatus.IsWounded() was changed to test the UNCONSCIOUS bit. The tree compiled CLEAN -
//!   two predicates sharing a constant is not a syntax error - and the wounded-only mask below then
//!   answers false, so the case fails on "The WOUNDED bit alone did not read as wounded". The isolation
//!   half catches the same fault from the other side (the unconscious-only mask reads as wounded). The
//!   fault was reverted and the tree recompiled clean again.
//!   No maxAttempts: bit tests on hand-built values.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_RecruitStatus_PredicatesReadOneBitEach : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		// A zero mask - what every consumer sees before the first status push arrives.
		if (OVT_RecruitStatus.IsArmed(0))
		{
			SetFailure("An empty status mask read as ARMED - an undescribed recruit must not look ready");
			return true;
		}

		if (OVT_RecruitStatus.HasAmmo(0))
		{
			SetFailure("An empty status mask read as having ammunition");
			return true;
		}

		if (OVT_RecruitStatus.IsWounded(0))
		{
			SetFailure("An empty status mask read as WOUNDED");
			return true;
		}

		if (OVT_RecruitStatus.IsUnconscious(0))
		{
			SetFailure("An empty status mask read as UNCONSCIOUS");
			return true;
		}

		// The full mask - every predicate must find its own bit inside a crowded value.
		int full = OVT_RecruitStatus.Derive(true, true, true, true);

		if (!OVT_RecruitStatus.IsArmed(full))
		{
			SetFailure("A full status mask did not read as ARMED");
			return true;
		}

		if (!OVT_RecruitStatus.HasAmmo(full))
		{
			SetFailure("A full status mask did not read as having ammunition");
			return true;
		}

		if (!OVT_RecruitStatus.IsWounded(full))
		{
			SetFailure("A full status mask did not read as WOUNDED");
			return true;
		}

		if (!OVT_RecruitStatus.IsUnconscious(full))
		{
			SetFailure("A full status mask did not read as UNCONSCIOUS");
			return true;
		}

		// One bit at a time: the predicate that owns it says yes, the other three say no.
		int armedOnly = OVT_RecruitStatus.ARMED;
		if (!OVT_RecruitStatus.IsArmed(armedOnly))
		{
			SetFailure("The ARMED bit alone did not read as armed");
			return true;
		}

		if (OVT_RecruitStatus.HasAmmo(armedOnly) || OVT_RecruitStatus.IsWounded(armedOnly) || OVT_RecruitStatus.IsUnconscious(armedOnly))
		{
			SetFailure("The ARMED bit alone also read as ammunition, wounded or unconscious - two predicates share a constant");
			return true;
		}

		int ammoOnly = OVT_RecruitStatus.HAS_AMMO;
		if (!OVT_RecruitStatus.HasAmmo(ammoOnly))
		{
			SetFailure("The HAS_AMMO bit alone did not read as having ammunition");
			return true;
		}

		if (OVT_RecruitStatus.IsArmed(ammoOnly) || OVT_RecruitStatus.IsWounded(ammoOnly) || OVT_RecruitStatus.IsUnconscious(ammoOnly))
		{
			SetFailure("The HAS_AMMO bit alone also read as armed, wounded or unconscious - two predicates share a constant");
			return true;
		}

		int woundedOnly = OVT_RecruitStatus.WOUNDED;
		if (!OVT_RecruitStatus.IsWounded(woundedOnly))
		{
			SetFailure("The WOUNDED bit alone did not read as wounded");
			return true;
		}

		if (OVT_RecruitStatus.IsArmed(woundedOnly) || OVT_RecruitStatus.HasAmmo(woundedOnly) || OVT_RecruitStatus.IsUnconscious(woundedOnly))
		{
			SetFailure("The WOUNDED bit alone also read as armed, supplied or unconscious - two predicates share a constant");
			return true;
		}

		int unconsciousOnly = OVT_RecruitStatus.UNCONSCIOUS;
		if (!OVT_RecruitStatus.IsUnconscious(unconsciousOnly))
		{
			SetFailure("The UNCONSCIOUS bit alone did not read as unconscious");
			return true;
		}

		if (OVT_RecruitStatus.IsArmed(unconsciousOnly) || OVT_RecruitStatus.HasAmmo(unconsciousOnly) || OVT_RecruitStatus.IsWounded(unconsciousOnly))
		{
			SetFailure("The UNCONSCIOUS bit alone also read as armed, supplied or wounded - two predicates share a constant");
			return true;
		}

		Print("Recruit status predicates: silent on an empty mask, each finds its own bit in a full one, none reads another's");

		return true;
	}
}
