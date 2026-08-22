//------------------------------------------------------------------------------------------------
//! Pure statics for High Command: cap arithmetic, stance validity, id minting, arrival and the
//! supporter draw-down (implementation.md §3.2). World-free - no manager, no entity, no world call
//! may ever appear here.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandRules
{
	//! THE one warehouse radius - shared by the purchase quote, the tent discount, the rearm tick
	//! and every client pre-check (D6, the BUG-070 lesson).
	static const float WAREHOUSE_RANGE = 150.0;

	//! Inside this of its destination a group is no longer "moving" - a derived fact, not tracked.
	static const float ARRIVAL_RADIUS = 25.0;

	static const int DEFAULT_MEMBER_CAP = 48;

	//! Radius of the ring a PATROL order walks around its destination (§3.12).
	static const float PATROL_RADIUS = 150.0;

	//! The entry key every CONVERTED recruit group carries. RESERVED: no authored catalog entry may
	//! use it, and nothing purchasable ever resolves to it - the purchase list reads the faction's
	//! own array by index, which this key is not in.
	static const string CONVERTED_ENTRY_KEY = "hc_converted";

	//------------------------------------------------------------------------------------------------
	//! Mints a stable, salted group id - the OVT_RecruitData id shape with an "hc" prefix.
	//! \param[in] ownerPersistentId The buying player.
	//! \param[in] unixTime Wall-clock seconds at mint time.
	//! \param[in] salt Caller-supplied tiebreaker; two different salts must never collide.
	//! \return "hc_<owner>_<unixtime>_<hex6-of-salt>".
	static string MintGroupId(string ownerPersistentId, int unixTime, int salt)
	{
		return string.Format("hc_%1_%2_%3", ownerPersistentId, unixTime.ToString(), SaltToHex6(salt));
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] stance A candidate OVT_EHighCommandStance ordinal.
	//! \return True when it names a real stance.
	static bool IsStanceValid(int stance)
	{
		return stance >= OVT_EHighCommandStance.DEFEND && stance <= OVT_EHighCommandStance.ATTACK;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a purchase or conversion of `incomingMembers` fits under a player's member cap.
	//! \param[in] currentMembers Members the owner already has.
	//! \param[in] incomingMembers Members the operation would add.
	//! \param[in] cap The owner's member cap; cap <= 0 means unlimited.
	//! \return True when the total fits, or the cap is unlimited.
	static bool FitsUnderCap(int currentMembers, int incomingMembers, int cap)
	{
		if (cap <= 0)
			return true;

		return currentMembers + incomingMembers <= cap;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] currentMembers Members the owner already has.
	//! \param[in] cap The owner's member cap; cap <= 0 means unlimited.
	//! \return int.MAX when unlimited, otherwise cap - currentMembers, never negative.
	static int RemainingCapacity(int currentMembers, int cap)
	{
		if (cap <= 0)
			return int.MAX;

		return Math.Max(0, cap - currentMembers);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a group is still travelling to its destination. SQUARED distance throughout -
	//! vector.Distance is not correctly rounded, so an exact-boundary decision would be a coin flip.
	//! \param[in] position Current position.
	//! \param[in] destination Ordered destination.
	//! \return True when further than ARRIVAL_RADIUS from the destination.
	static bool IsMoving(vector position, vector destination)
	{
		float radiusSq = ARRIVAL_RADIUS * ARRIVAL_RADIUS;
		return vector.DistanceSq(position, destination) > radiusSq;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one group's status is worth re-measuring on this sweep tick (R4).
	//!
	//! A group still TRAVELLING is measured on every tick, so its marker keeps up with it. A parked or
	//! arrived group is measured on one tick in idleEveryTicks, which is the cheap cadence the
	//! heartbeat has always run at. This decides only whether to READ - the change filter still gates
	//! the send, so a parked group whose flags have not changed produces no traffic at all.
	//! \param[in] position The group's last measured position.
	//! \param[in] destination Its ordered destination.
	//! \param[in] tickIndex A monotonically increasing sweep counter.
	//! \param[in] idleEveryTicks Ticks a parked group waits; <= 1 measures every tick.
	//! \return True when this group should be re-measured now.
	static bool IsStatusReadDue(vector position, vector destination, int tickIndex, int idleEveryTicks)
	{
		if (IsMoving(position, destination))
			return true;

		if (idleEveryTicks <= 1)
			return true;

		return (tickIndex % idleEveryTicks) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What a High Command purchase costs in total (R7).
	//!
	//! THE VEHICLE IS NOT FEE-MULTIPLIED. The recruit fee multiplier prices kitting a man out;
	//! a vehicle is bought outright, at the same full shop buy price any player would pay for it, so
	//! it is added to the crew total rather than folded into the gear subtotal.
	//! \param[in] crewTotal OVT_RecruitPurchaseRules.TotalPrice() for the members and their gear.
	//! \param[in] vehicleCost The vehicle's full shop buy price; 0 for a foot group.
	//! \return The amount the player is charged, never negative.
	static int PurchaseTotal(int crewTotal, int vehicleCost)
	{
		int total = crewTotal;

		if (vehicleCost > 0)
			total += vehicleCost;

		if (total < 0)
			return 0;

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] memberCount Members in the group.
	//! \param[in] supportersPerMember Difficulty-configured draw-down rate.
	//! \return memberCount * supportersPerMember, floored at 0.
	static int SupportersRequired(int memberCount, int supportersPerMember)
	{
		if (memberCount <= 0 || supportersPerMember <= 0)
			return 0;

		return memberCount * supportersPerMember;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] destination A candidate order destination.
	//! \return True when it is a real, finite, non-zero position.
	static bool IsDestinationLegal(vector destination)
	{
		if (destination == vector.Zero)
			return false;

		if (!IsFiniteComponent(destination[0]))
			return false;

		if (!IsFiniteComponent(destination[1]))
			return false;

		if (!IsFiniteComponent(destination[2]))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] value One vector component.
	//! \return False for NaN or +/-infinity.
	protected static bool IsFiniteComponent(float value)
	{
		if (value != value)
			return false;

		if (value >= float.INFINITY || value <= -float.INFINITY)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Aggregates one entry per deficient magazine into a per-resource count a warehouse can be asked
	//! for (the rearm tick's needed-magazine derivation). Pure: the world-touching walk that finds
	//! each deficient magazine and its resource name lives on the manager; this only sums what it is
	//! handed.
	//! \param[in] resourcesNeeded One resource name per deficient magazine found; duplicates expected.
	//! \param[out] needed Resource -> how many units are needed in total. Allocated if null, cleared
	//! otherwise. An empty resource name in the input contributes nothing.
	//! \return How many distinct resources are needed.
	static int AggregateResourceNeeds(notnull array<string> resourcesNeeded, out map<string, int> needed)
	{
		if (!needed)
			needed = new map<string, int>();

		needed.Clear();

		foreach (string resource : resourcesNeeded)
		{
			if (resource == "")
				continue;

			if (needed.Contains(resource))
				needed.Set(resource, needed.Get(resource) + 1);
			else
				needed.Set(resource, 1);
		}

		return needed.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Deterministically renders a salt as a 6-digit lowercase hex string, so two different salts
	//! always produce two different id suffixes.
	//! \param[in] salt Any int; negated when negative.
	//! \return A 6-character hex string.
	protected static string SaltToHex6(int salt)
	{
		string digits = "0123456789abcdef";
		string result = "";

		int value = salt;
		if (value < 0)
			value = -value;

		for (int i = 0; i < 6; i++)
		{
			int nibble = value % 16;
			result = digits.Get(nibble) + result;
			value = value / 16;
		}

		return result;
	}
}
