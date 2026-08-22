//------------------------------------------------------------------------------------------------
//! TIER A cases - High Command's status mask and the map tag it chooses (the OVT_RecruitStatus
//! model). World-free by construction; see the suite header for the tier rule.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Each of the four measured facts sets exactly its own bit, with no cross-talk between them.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandStatus_DeriveSetsEachBitIndependently : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_HighCommandStatus.CONTACT != 1)
		{
			SetFailure("OVT_HighCommandStatus.CONTACT is not 1 - the status mask is a wire format and its bit values are fixed");
			return true;
		}

		if (OVT_HighCommandStatus.NO_AMMO != 2)
		{
			SetFailure("OVT_HighCommandStatus.NO_AMMO is not 2 - the status mask is a wire format and its bit values are fixed");
			return true;
		}

		if (OVT_HighCommandStatus.MOVING != 4)
		{
			SetFailure("OVT_HighCommandStatus.MOVING is not 4 - the status mask is a wire format and its bit values are fixed");
			return true;
		}

		if (OVT_HighCommandStatus.IN_VEHICLE != 8)
		{
			SetFailure("OVT_HighCommandStatus.IN_VEHICLE is not 8 - the status mask is a wire format and its bit values are fixed");
			return true;
		}

		// contact=false, anyAmmo=true, moving=false, mounted=false -> nothing set (NO_AMMO is the
		// inverse of anyAmmo, so a fully-supplied, stationary, dismounted, uncontacted group is 0).
		if (OVT_HighCommandStatus.Derive(false, true, false, false) != 0)
		{
			SetFailure("A quiet, supplied, stationary, dismounted group derived a non-zero status mask");
			return true;
		}

		// Contact alone.
		int contactOnly = OVT_HighCommandStatus.Derive(true, true, false, false);
		if (contactOnly != OVT_HighCommandStatus.CONTACT)
		{
			SetFailure("Contact alone derived mask %1, expected exactly CONTACT (%2)", contactOnly.ToString(), OVT_HighCommandStatus.CONTACT.ToString());
			return true;
		}

		// Out of ammo alone - anyAmmo=false sets NO_AMMO and nothing else.
		int noAmmoOnly = OVT_HighCommandStatus.Derive(false, false, false, false);
		if (noAmmoOnly != OVT_HighCommandStatus.NO_AMMO)
		{
			SetFailure("Out-of-ammo alone derived mask %1, expected exactly NO_AMMO (%2)", noAmmoOnly.ToString(), OVT_HighCommandStatus.NO_AMMO.ToString());
			return true;
		}

		// Moving alone.
		int movingOnly = OVT_HighCommandStatus.Derive(false, true, true, false);
		if (movingOnly != OVT_HighCommandStatus.MOVING)
		{
			SetFailure("Moving alone derived mask %1, expected exactly MOVING (%2)", movingOnly.ToString(), OVT_HighCommandStatus.MOVING.ToString());
			return true;
		}

		// Mounted alone.
		int mountedOnly = OVT_HighCommandStatus.Derive(false, true, false, true);
		if (mountedOnly != OVT_HighCommandStatus.IN_VEHICLE)
		{
			SetFailure("Mounted alone derived mask %1, expected exactly IN_VEHICLE (%2)", mountedOnly.ToString(), OVT_HighCommandStatus.IN_VEHICLE.ToString());
			return true;
		}

		// All four facts at once must produce all four bits and nothing else.
		int all = OVT_HighCommandStatus.Derive(true, false, true, true);
		int expectedAll = OVT_HighCommandStatus.CONTACT | OVT_HighCommandStatus.NO_AMMO | OVT_HighCommandStatus.MOVING | OVT_HighCommandStatus.IN_VEHICLE;
		if (all != expectedAll)
		{
			SetFailure("A group in contact, out of ammo, moving and mounted derived mask %1, expected %2 - a fact leaked into another's bit or two facts share a bit", all.ToString(), expectedAll.ToString());
			return true;
		}

		Print("High Command status: each of the four facts sets exactly its own bit, alone and combined");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Contact wins the map badge over an ammo warning; neither shows nothing.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]
class OVT_TEST_Logic_HighCommandStatus_TagIconPrefersContact : SCR_AutotestCaseBase
{
	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		if (OVT_HighCommandStatus.TagIcon(0) != "")
		{
			SetFailure("A group with no status flags at all was given a badge");
			return true;
		}

		// Contact AND out of ammo - contact must win.
		int contactAndNoAmmo = OVT_HighCommandStatus.Derive(true, false, false, false);
		if (OVT_HighCommandStatus.TagIcon(contactAndNoAmmo) != OVT_HighCommandStatus.TAG_CONTACT)
		{
			SetFailure("A group in contact AND out of ammo showed '%1', expected the contact tag to win", OVT_HighCommandStatus.TagIcon(contactAndNoAmmo));
			return true;
		}

		// Out of ammo alone - the ammo tag shows.
		int noAmmoOnly = OVT_HighCommandStatus.Derive(false, false, false, false);
		if (OVT_HighCommandStatus.TagIcon(noAmmoOnly) != OVT_HighCommandStatus.TAG_NO_AMMO)
		{
			SetFailure("A group out of ammo with no contact showed '%1', expected the ammo tag", OVT_HighCommandStatus.TagIcon(noAmmoOnly));
			return true;
		}

		// Neither - no badge.
		int quiet = OVT_HighCommandStatus.Derive(false, true, false, false);
		if (OVT_HighCommandStatus.TagIcon(quiet) != "")
		{
			SetFailure("A quiet, supplied group showed a badge ('%1')", OVT_HighCommandStatus.TagIcon(quiet));
			return true;
		}

		// The two real badges must be distinct pictures.
		if (OVT_HighCommandStatus.TagIcon(contactAndNoAmmo) == OVT_HighCommandStatus.TagIcon(noAmmoOnly))
		{
			SetFailure("The contact badge and the ammo badge resolve to the same quad - the one distinction this tag exists to draw");
			return true;
		}

		Print("High Command status tag: contact wins over an ammo warning, and neither shows nothing");
		return true;
	}
}
