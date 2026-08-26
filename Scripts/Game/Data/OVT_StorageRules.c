//------------------------------------------------------------------------------------------------
//! Every storage decision that is a rule rather than a lookup, as pure statics.
//!
//! The callers do the lookups (economy catalogue, prefab data, positions) and hand the answers in,
//! so the Logic tier can assert the decisions without a world behind them.
//------------------------------------------------------------------------------------------------
class OVT_StorageRules
{
	//! What a registered, legal, non-truck vehicle holds when nothing overrides it.
	static const int DEFAULT_VEHICLE_CAPACITY = 300;

	//! Fraction of the import price the port pays back on export.
	static const float DEFAULT_EXPORT_RATIO = 0.5;

	//------------------------------------------------------------------------------------------------
	//! Resolves AUTO capacity for a holder.
	//!
	//! An unregistered vehicle returns 0 rather than a default: silently granting capacity to a
	//! prefab the economy does not know is the worse failure. The CALLER logs an ERROR once per
	//! prefab on that branch - a silent 0 is undiagnosable.
	//! \param[in] isVehicle False for boxes and buildings, which are unlimited.
	//! \param[in] isRegistered Whether the economy knows this vehicle prefab.
	//! \param[in] isLegalVehicle False for illegal or armed vehicles.
	//! \param[in] parking The registered vehicle's parking type.
	//! \param[in] defaultVehicleCapacity Capacity for an ordinary registered legal car.
	//! \return -1 unlimited, 0 none, otherwise the item cap.
	static int ResolveAutoCapacity(bool isVehicle, bool isRegistered, bool isLegalVehicle, OVT_ParkingType parking, int defaultVehicleCapacity)
	{
		if (!isVehicle)
			return -1;

		if (!isRegistered)
			return 0;

		if (!isLegalVehicle)
			return 0;

		if (parking == OVT_ParkingType.PARKING_TRUCK)
			return -1;

		return defaultVehicleCapacity;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a magazine is at full load. Part-used magazines never convert to a ledger line; they
	//! stay in the vanilla inventory.
	//!
	//! Strict equality, deliberately: a corrupt count above its own maximum is NOT full, and >= would
	//! call every item with an unknown maximum of -1 full.
	//! \param[in] ammoCount Rounds currently loaded.
	//! \param[in] maxAmmoCount Rounds when full.
	//! \return True only when the two are equal.
	static bool MagazineIsFull(int ammoCount, int maxAmmoCount)
	{
		return ammoCount == maxAmmoCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a clothing area is one of the three base garments a character always wears.
	//!
	//! There is no LoadoutShirtArea - the torso garment is LoadoutJacketArea. Compared as typenames,
	//! not as class-name strings: string comparison invites names that do not exist in vanilla.
	//! \param[in] areaType The cloth component's area typename.
	//! \return True for jacket, pants or boots; false for everything else, including null.
	static bool IsBaseClothingArea(typename areaType)
	{
		if (!areaType)
			return false;

		if (areaType == LoadoutJacketArea)
			return true;

		if (areaType == LoadoutPantsArea)
			return true;

		if (areaType == LoadoutBootsArea)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! What the port pays per item on export.
	//!
	//! Held strictly under the cheapest shop buy price so there is no shop-buy/port-sell loop. A
	//! negative minShopBuyPrice means "sold at no shop" (illegal items) and the ratio stands alone.
	//! \param[in] importPrice What import charges for this item.
	//! \param[in] ratio Fraction of the import price to pay, typically DEFAULT_EXPORT_RATIO.
	//! \param[in] minShopBuyPrice Cheapest shop buy price, or negative when sold nowhere.
	//! \return Dollars per item; never below 1.
	static int ExportUnitPrice(int importPrice, float ratio, int minShopBuyPrice)
	{
		int price = Math.Round(importPrice * ratio);

		if (minShopBuyPrice >= 0)
		{
			int ceiling = minShopBuyPrice - 1;
			if (price > ceiling)
				price = ceiling;
		}

		if (price < 1)
			price = 1;

		return price;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves one line from one ledger to another, and puts back whatever did not fit.
	//!
	//! THE ORDER IS THE POINT, and it is the one data-integrity rule in this feature that is pure
	//! enough to be asserted without a world:
	//!   1. clamp to what the source ACTUALLY holds right now - two players batching on one holder
	//!      must never drive a count negative (R9);
	//!   2. take from the source;
	//!   3. add to the destination, clamped by its capacity;
	//!   4. RETURN THE UN-ADDED REMAINDER TO THE SOURCE.
	//!
	//! The remainder goes back at unlimited capacity deliberately: those items were in the source one
	//! statement ago, so putting them back cannot mint anything, whereas re-clamping against a source
	//! whose capacity has since been retuned downwards would destroy them.
	//! \param[in] source The ledger being emptied.
	//! \param[in] dest The ledger being filled. May be the same object; the move is then a no-op.
	//! \param[in] res Prefab ResourceName.
	//! \param[in] qty How many were asked for.
	//! \param[in] destCapacity The destination holder's capacity; negative means unlimited.
	//! \param[out] shortfall How many of \a qty did not arrive, for the player's report.
	//! \return How many arrived at the destination.
	static int TransferLedgerLine(OVT_StorageLedger source, OVT_StorageLedger dest, string res, int qty, int destCapacity, out int shortfall)
	{
		shortfall = 0;

		if (!source || !dest || res == "" || qty <= 0)
		{
			if (qty > 0)
				shortfall = qty;

			return 0;
		}

		int want = qty;
		int held = source.Count(res);
		if (want > held)
			want = held;

		if (want <= 0)
		{
			shortfall = qty;
			return 0;
		}

		int taken = source.Take(res, want);
		int added = dest.Add(res, taken, destCapacity);

		int remainder = taken - added;
		if (remainder > 0)
			source.Add(res, remainder, -1);

		shortfall = qty - added;

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a holder is close enough to the player to be a transfer destination.
	//! \param[in] holderPos World position of the holder.
	//! \param[in] playerPos World position of the player.
	//! \param[in] radius Search radius in metres.
	//! \return True when the distance is within the radius, inclusive.
	static bool HolderIsInRange(vector holderPos, vector playerPos, float radius)
	{
		return vector.Distance(holderPos, playerPos) <= radius;
	}
}
