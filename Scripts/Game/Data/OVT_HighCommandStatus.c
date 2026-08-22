//------------------------------------------------------------------------------------------------
//! High Command's status flags and map-tag choice, as pure functions (the OVT_RecruitStatus
//! shape). The MEASURING half - contact, ammo, movement, mounted - needs a live group and lives on
//! the manager; the DECIDING half lives here so Logic-tier coverage can pin it.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandStatus
{
	//! The group has a perceived enemy contact.
	static const int CONTACT = 1;

	//! No member of the group can put rounds downrange.
	static const int NO_AMMO = 2;

	//! Further than OVT_HighCommandRules.ARRIVAL_RADIUS from its destination.
	static const int MOVING = 4;

	//! At least one member is mounted in the group's vehicle.
	static const int IN_VEHICLE = 8;

	//! No tag drawn.
	static const string TAG_NONE = "";

	//! Contact takes priority over every other tag.
	static const string TAG_CONTACT = "hc_contact";

	//! Shown only when there is no contact to report.
	static const string TAG_NO_AMMO = "hc_no_ammo";

	//------------------------------------------------------------------------------------------------
	//! Packs four measured facts into the status mask that crosses the wire. Each parameter sets
	//! exactly its own bit; nothing here sanity-checks one fact against another.
	//! \param[in] contact Any member has a perceived enemy contact.
	//! \param[in] anyAmmo Any member can fire or reload.
	//! \param[in] moving OVT_HighCommandRules.IsMoving() for the group's position and destination.
	//! \param[in] mounted Any member is mounted.
	//! \return The status mask - 0 when nothing is true.
	static int Derive(bool contact, bool anyAmmo, bool moving, bool mounted)
	{
		int flags = 0;

		if (contact)
			flags |= CONTACT;

		if (!anyAmmo)
			flags |= NO_AMMO;

		if (moving)
			flags |= MOVING;

		if (mounted)
			flags |= IN_VEHICLE;

		return flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Which map badge a group's status should show. Contact always wins over an ammo badge.
	//! \param[in] flags A mask as built by Derive().
	//! \return An imageset quad name, or "" when nothing should be drawn.
	static string TagIcon(int flags)
	{
		if (HasFlag(flags, CONTACT))
			return TAG_CONTACT;

		if (HasFlag(flags, NO_AMMO))
			return TAG_NO_AMMO;

		return TAG_NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] flags A mask as built by Derive().
	//! \param[in] bit One of this class's bit constants.
	//! \return True when that bit is set.
	static bool HasFlag(int flags, int bit)
	{
		return (flags & bit) != 0;
	}
}
