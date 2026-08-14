//------------------------------------------------------------------------------------------------
//! WHAT a recruit's status flags MEAN and WHICH map tag they choose - expressed as PURE FUNCTIONS
//! with no world, no manager and no engine access.
//!
//! WHY THIS IS A SEPARATE, STATIC-ONLY CLASS. Reading whether a recruit is armed, has ammunition,
//! is wounded or is unconscious needs a live body: weapon slots, muzzles, an inventory manager, a
//! damage manager. None of that is reachable from the automated spine. So the reading stays in
//! OVT_RecruitManagerComponent.ReadRecruitStatus() (world-bound, server-only) and the DECIDING -
//! packing four booleans into a mask, unpacking the mask again, and choosing the icon a marker
//! shows - lives here where Tier A (OVT_TEST_LogicSuite) can pin it.
//! NOTHING IN THIS FILE MAY EVER GROW A MANAGER LOOKUP OR AN ENTITY DEREFERENCE, or that coverage
//! silently dies.
//!
//! WHY THE FLAGS EXIST AT ALL RATHER THAN FOUR BOOLS ON THE RECORD. Status is computed on the
//! SERVER and pushed to the owning client every STATUS_SYNC_INTERVAL_MS (decision D11), because a
//! parked recruit is by definition somewhere its owner is not: its entity is not streamed to that
//! client, so a client-side read would render every distant recruit as unarmed. One int is one
//! already-proven RPC parameter type, and one source keeps the map tag and the roster row from
//! disagreeing with each other.
//!
//! THE BITS ARE INDEPENDENT AND Derive() ENFORCES NOTHING BETWEEN THEM. A mask carrying HAS_AMMO
//! without ARMED is a legal value and means exactly what it says: this character is carrying
//! ammunition but nothing to fire it with. Collapsing that at derivation time would throw away a
//! fact the caller measured; it is TagIcon() - the one consumer that has to pick a single picture -
//! that decides such a recruit shows no weapon tag at all.
//------------------------------------------------------------------------------------------------
class OVT_RecruitStatus
{
	//! The recruit is carrying a weapon in at least one weapon slot (in hands, slung or holstered).
	static const int ARMED = 1;

	//! The recruit can put rounds downrange: something is loaded, or a compatible magazine is in
	//! its inventory. See ReadRecruitStatus() for the exact measurement.
	static const int HAS_AMMO = 2;

	//! The recruit's damage state is worse than undamaged.
	static const int WOUNDED = 4;

	//! The recruit is unconscious - it has a body, but that body is doing nothing.
	static const int UNCONSCIOUS = 8;

	//! No weapon tag is drawn. An unarmed recruit's marker is the bare base image.
	static const string TAG_NONE = "";

	//! Armed and able to shoot.
	static const string TAG_ARMED = "recruit_ammo";

	//! Armed but dry - the one status a player is expected to act on, which is why it gets its own
	//! quad rather than being folded into "armed".
	static const string TAG_ARMED_EMPTY = "recruit_ammo_empty";

	//------------------------------------------------------------------------------------------------
	//! Pack four measured facts into the status mask that crosses the wire.
	//!
	//! Deliberately does not sanity-check its inputs against each other (see the class header): each
	//! parameter sets exactly its own bit and nothing else, so a caller measuring an unusual
	//! combination gets that combination back, and this function has no opinion the reader has to
	//! know about.
	//! \param[in] armed Whether any weapon slot holds a weapon.
	//! \param[in] hasAmmo Whether any of those weapons can be fired or reloaded.
	//! \param[in] wounded Whether the damage state is worse than undamaged.
	//! \param[in] unconscious Whether the character is unconscious.
	//! \return The status mask - 0 when nothing is true.
	static int Derive(bool armed, bool hasAmmo, bool wounded, bool unconscious)
	{
		int flags = 0;

		if (armed)
			flags |= ARMED;

		if (hasAmmo)
			flags |= HAS_AMMO;

		if (wounded)
			flags |= WOUNDED;

		if (unconscious)
			flags |= UNCONSCIOUS;

		return flags;
	}

	//------------------------------------------------------------------------------------------------
	//! Which imageset quad a marker's tag should show for this status.
	//!
	//! THREE OUTCOMES, NOT FOUR. Ammunition is only a fact worth drawing about somebody who can use
	//! it, so an unarmed recruit shows no tag whatever else is true of it - HAS_AMMO on an unarmed
	//! recruit describes cargo, not capability. Wounded and unconscious are deliberately absent from
	//! this decision: they are not weapon states, and the marker layer expresses them another way.
	//!
	//! An empty string is a real answer, not a failure. The caller hides the tag widget on it (see
	//! the map layer), which is why nothing here ever returns a "blank" quad name.
	//! \param[in] flags A mask as built by Derive().
	//! \return An imageset quad name, or "" when no tag should be drawn.
	static string TagIcon(int flags)
	{
		if (!IsArmed(flags))
			return TAG_NONE;

		if (HasAmmo(flags))
			return TAG_ARMED;

		return TAG_ARMED_EMPTY;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the mask says the recruit is carrying a weapon.
	//! \param[in] flags A mask as built by Derive().
	//! \return True when ARMED is set.
	static bool IsArmed(int flags)
	{
		return (flags & ARMED) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the mask says the recruit can shoot or reload.
	//! \param[in] flags A mask as built by Derive().
	//! \return True when HAS_AMMO is set.
	static bool HasAmmo(int flags)
	{
		return (flags & HAS_AMMO) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the mask says the recruit is hurt.
	//! \param[in] flags A mask as built by Derive().
	//! \return True when WOUNDED is set.
	static bool IsWounded(int flags)
	{
		return (flags & WOUNDED) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the mask says the recruit is unconscious.
	//! \param[in] flags A mask as built by Derive().
	//! \return True when UNCONSCIOUS is set.
	static bool IsUnconscious(int flags)
	{
		return (flags & UNCONSCIOUS) != 0;
	}
}
