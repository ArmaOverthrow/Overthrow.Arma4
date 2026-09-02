//------------------------------------------------------------------------------------------------
//! One rearmable unit of ammunition: one deficient gun magazine, or one reloadable rocket barrel.
//!
//! An empty m_sRes means the weapon's ammunition prefab could not be resolved. Such a line still
//! counts towards the total - it is simply never coverable, so it always lands in the money
//! remainder (R2's "rocket pods that have no magazine prefab stay money-only").
//------------------------------------------------------------------------------------------------
class OVT_RearmUnit : Managed
{
	//! Prefab ResourceName the unit is drawn from; "" means it can only be bought.
	string m_sRes;

	//! How many items this line needs.
	int m_iUnits;

	//------------------------------------------------------------------------------------------------
	//! \param[in] res Prefab ResourceName, or "".
	//! \param[in] units How many items the line needs.
	void OVT_RearmUnit(string res, int units)
	{
		m_sRes = res;
		m_iUnits = units;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared rules for the armed-vehicle re-arm action.
//!
//! One class answers, on both machines, the questions the user action and the server handler must
//! agree on: which of a vehicle's weapons are rearmable, whether any of them are missing
//! ammunition, what the rearm needs in ledger items, whether the vehicle is at a supply point, and
//! what the uncovered remainder costs. Weapon discovery walks the vehicle's SlotManagerComponent the
//! same way the vanilla editor context actions do (SCR_RefillMagazineContextAction for turret gun
//! magazines, SCR_BaseRocketPodsContextAction for rocket pods on a weapons rack), so anything the
//! game master's "Refill magazine" / "Rearm rocket pods" buttons would touch, this touches.
//!
//! LEDGER FIRST, MONEY SECOND. QuoteRearm is a pure READ: it builds the plan, counts what the
//! nearby ledgers hold and prorates the price over what they cannot cover. Nothing here takes,
//! spawns or writes except PerformRearm, which is server-only (BaseMagazineComponent.SetAmmoCount
//! may only be called on the master, and RocketEjectorMuzzleComponent.ReloadBarrel is ignored on
//! clients). Discovery, "is anything missing" and IsAtRearmSite are answerable on clients, which is
//! what lets the action show an honest disabled reason.
//------------------------------------------------------------------------------------------------
class OVT_VehicleRearmUtils
{
	//! Base price of a full re-arm, scaled by OVT_DifficultySettings.vehiclePriceMultiplier (the
	//! same replicated difficulty knob that prices the vehicles themselves).
	static const int REARM_BASE_COST = 1000;

	//! How far from the vehicle's origin a built supply structure is searched for. Wider than the
	//! improvised helipad's own 16 m heli parking box (OVT_ParkingComponent) because a Garage is a
	//! building rather than a landing box.
	static const float SITE_SEARCH_RADIUS = 20;

	//! How close to a deployed FOB counts as a supply point. The shipped placement distance
	//! (OVT_SleepService.MAX_FOB_PLACE_DIS, OVT_ItemLimitChecker, OVT_PlaceContext all hold 100).
	static const float FOB_SEARCH_RADIUS = 100;

	//! The OVT_BuildableComponent type strings authored on the two supply structures
	//! (HelipadImprovised_01/Helipad.et:14, Garage_E_02.et:5).
	static const string HELIPAD_BUILDABLE_TYPE = "Helipad";
	static const string GARAGE_BUILDABLE_TYPE = "VehicleGarage";

	//! Used only when the asking player's controller cannot be reached, so the authored
	//! OVT_StorageRequestComponent.m_fHolderRadius is unreadable. Same number that attribute defaults
	//! to; the authored value is the one that normally applies.
	static const float FALLBACK_HOLDER_RADIUS = 25;

	//! Whether the sphere query below has found a supply structure. Instance state because world
	//! queries report through member callbacks.
	protected bool m_bFoundSite;

	//------------------------------------------------------------------------------------------------
	//! What a full re-arm costs at the current difficulty, before any ledger coverage.
	//! \return The price in dollars, never negative.
	static int GetRearmCost()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(!config || !config.m_Difficulty) return REARM_BASE_COST;

		return Math.Round(REARM_BASE_COST * config.m_Difficulty.vehiclePriceMultiplier);
	}

	//------------------------------------------------------------------------------------------------
	//! Collect every rearmable weapon on a vehicle: gun magazines behind a muzzle (turret guns, gun
	//! pods) and rocket-pod weapons (anything with a rocket ejector muzzle).
	//!
	//! Walks each entity attached to the vehicle's slots - turrets and weapon racks alike - and then
	//! each weapon held by a WeaponSlotComponent on those entities. A weapon with a rocket ejector
	//! muzzle is a rocket pod; otherwise its current muzzle magazine, if any, is a refillable gun
	//! magazine. An absent weapons rack is NOT spawned in: a vehicle that never carried weapons has
	//! nothing to re-arm.
	//! \param[in] vehicle The vehicle to scan.
	//! \param[out] magazines Every refillable gun magazine found.
	//! \param[out] muzzles Index-aligned with magazines - the muzzle each was found behind, which is
	//! what GetDefaultMagazineOrProjectileName() must be asked (D2).
	//! \param[out] rocketWeapons Every rocket-pod weapon entity found.
	static void GetRearmableWeapons(IEntity vehicle, out array<BaseMagazineComponent> magazines, out array<BaseMuzzleComponent> muzzles, out array<IEntity> rocketWeapons)
	{
		magazines = {};
		muzzles = {};
		rocketWeapons = {};

		if(!vehicle) return;

		SlotManagerComponent slotManager = SlotManagerComponent.Cast(vehicle.FindComponent(SlotManagerComponent));
		if(!slotManager) return;

		array<EntitySlotInfo> slots = {};
		slotManager.GetSlotInfos(slots);

		foreach(EntitySlotInfo slot : slots)
		{
			IEntity attached = slot.GetAttachedEntity();
			if(!attached) continue;

			// The slotted entity may itself be a weapon (a pod attached directly to the airframe)
			CollectWeapon(attached, magazines, muzzles, rocketWeapons);

			// ...and/or a holder of weapons (a turret or a weapons rack)
			array<Managed> weaponSlots = {};
			attached.FindComponents(WeaponSlotComponent, weaponSlots);
			foreach(Managed comp : weaponSlots)
			{
				WeaponSlotComponent weaponSlot = WeaponSlotComponent.Cast(comp);
				if(!weaponSlot) continue;

				IEntity weapon = weaponSlot.GetWeaponEntity();
				if(!weapon) continue;

				CollectWeapon(weapon, magazines, muzzles, rocketWeapons);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle has any rearmable weapon at all - the "is this an armed vehicle"
	//! question, answered structurally rather than from prefab labels.
	//! \param[in] vehicle The vehicle to scan.
	//! \return True when there is at least one gun magazine or rocket pod.
	static bool IsArmed(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<BaseMuzzleComponent> muzzles;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, muzzles, rocketWeapons);

		return !magazines.IsEmpty() || !rocketWeapons.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle is missing any ammunition a re-arm would restore.
	//! \param[in] vehicle The vehicle to scan.
	//! \return True when any gun magazine is below capacity or any rocket barrel can be reloaded.
	static bool NeedsRearm(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<BaseMuzzleComponent> muzzles;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, muzzles, rocketWeapons);

		return AnyAmmoMissing(magazines, rocketWeapons);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether anything in an already-collected weapon set is missing ammunition. Split out so a
	//! caller that already paid for discovery does not walk the slots twice.
	//! \param[in] magazines Gun magazines from GetRearmableWeapons.
	//! \param[in] rocketWeapons Rocket-pod weapons from GetRearmableWeapons.
	//! \return True when any magazine is below capacity or any rocket barrel can be reloaded.
	static bool AnyAmmoMissing(array<BaseMagazineComponent> magazines, array<IEntity> rocketWeapons)
	{
		foreach(BaseMagazineComponent magazine : magazines)
		{
			if(magazine.GetAmmoCount() < magazine.GetMaxAmmoCount()) return true;
		}

		foreach(IEntity rocketWeapon : rocketWeapons)
		{
			if(CanReloadRocketPod(rocketWeapon)) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Turn a vehicle's deficient weapons into a list of (prefab, units) lines.
	//!
	//! One unit per deficient gun magazine, one per reloadable rocket barrel - the accounting unit is
	//! one ledger item (D1). A weapon whose prefab resolves empty still contributes its units to the
	//! total, so it is always part of the money remainder.
	//! \param[in] vehicle The vehicle to plan for.
	//! \param[out] plan Receives the lines. Allocated if null, cleared otherwise.
	//! \param[out] totalUnits Sum of every line's units.
	//! \return How many lines the plan has.
	static int BuildPlan(IEntity vehicle, out array<ref OVT_RearmUnit> plan, out int totalUnits)
	{
		if(!plan)
			plan = new array<ref OVT_RearmUnit>();

		plan.Clear();
		totalUnits = 0;

		array<BaseMagazineComponent> magazines;
		array<BaseMuzzleComponent> muzzles;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, muzzles, rocketWeapons);

		foreach(int i, BaseMagazineComponent magazine : magazines)
		{
			if(magazine.GetAmmoCount() >= magazine.GetMaxAmmoCount()) continue;

			string muzzleDefault = "";
			BaseMuzzleComponent muzzle = muzzles[i];
			if(muzzle) muzzleDefault = muzzle.GetDefaultMagazineOrProjectileName();

			string loaded = OVT_PrefabUtils.GetPrefabName(magazine.GetOwner());

			plan.Insert(new OVT_RearmUnit(OVT_VehicleRearmRules.ResolveAmmoPrefab(muzzleDefault, loaded, ""), 1));
			totalUnits += 1;
		}

		foreach(IEntity rocketWeapon : rocketWeapons)
		{
			int barrels = CountReloadableBarrels(rocketWeapon);
			if(barrels <= 0) continue;

			string rocketPrefab = "";
			SCR_RocketEjectorMuzzleComponent rocketMuzzle = SCR_RocketEjectorMuzzleComponent.Cast(rocketWeapon.FindComponent(SCR_RocketEjectorMuzzleComponent));
			if(rocketMuzzle) rocketPrefab = rocketMuzzle.GetDefaultRocketPrefab();

			plan.Insert(new OVT_RearmUnit(OVT_VehicleRearmRules.ResolveAmmoPrefab("", "", rocketPrefab), barrels));
			totalUnits += barrels;
		}

		return plan.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every ledger a rearm of this vehicle may draw on, own storage first.
	//!
	//! POSITION IS PRIORITY (OVT_WarehouseStockUtils.PrependStore): the crew's own load is burned
	//! before the neighbour's. The radius is the destination picker's authored one, so what the rearm
	//! can reach and what the player loaded from cannot drift.
	//!
	//! ⚠ THE VEHICLE IS ALWAYS INSIDE ITS OWN SEARCH SPHERE, so the collector has already returned its
	//! storage and PrependStore - which refuses to move a store it finds in the list - would be a
	//! no-op. It is pulled out first, with RemoveOrdered because everything after it keeps its query
	//! order. High Command never hit this: a recruitment tent's crate is not a registered warehouse.
	//! \param[in] vehicle The vehicle being rearmed.
	//! \param[in] playerId The asking player's runtime id.
	//! \param[out] stores Receives the stores. Allocated if null, cleared otherwise.
	//! \return How many stores were collected.
	static int CollectRearmStores(IEntity vehicle, int playerId, out array<OVT_StorageComponent> stores)
	{
		if(!stores)
			stores = new array<OVT_StorageComponent>();

		stores.Clear();

		if(!vehicle) return 0;

		OVT_StorageUtils.CollectStores(vehicle.GetOrigin(), ResolveHolderRadius(playerId), playerId, stores);

		// Same permission rule as every other holder - a vehicle locked to another player keeps its load.
		OVT_StorageComponent own = OVT_StorageUtils.GetStorage(vehicle);
		if(own && OVT_StorageUtils.PlayerMayDrawFrom(playerId, vehicle))
		{
			int at = stores.Find(own);
			if(at != -1) stores.RemoveOrdered(at);

			OVT_WarehouseStockUtils.PrependStore(stores, own);
		}

		return stores.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! What a rearm of this vehicle would need, cover and cost. A PURE READ - it takes nothing,
	//! spawns nothing and writes nothing, so both the display quote and the server handler can run
	//! it, and running it twice in one frame changes no state.
	//! \param[in] vehicle The vehicle being rearmed.
	//! \param[in] playerId The asking player's runtime id.
	//! \param[out] totalUnits Units the rearm needs; 0 when nothing is missing.
	//! \param[out] coveredUnits Units the reachable ledgers hold.
	//! \param[out] cost Money owed for the remainder; 0 when the ledgers cover everything.
	static void QuoteRearm(IEntity vehicle, int playerId, out int totalUnits, out int coveredUnits, out int cost)
	{
		totalUnits = 0;
		coveredUnits = 0;
		cost = 0;

		array<ref OVT_RearmUnit> plan = {};
		BuildPlan(vehicle, plan, totalUnits);
		if(totalUnits <= 0) return;

		array<OVT_StorageComponent> stores = {};
		CollectRearmStores(vehicle, playerId, stores);

		coveredUnits = CountCovered(plan, stores);
		cost = OVT_VehicleRearmRules.ProratedCost(GetRearmCost(), totalUnits - coveredUnits, totalUnits);
	}

	//------------------------------------------------------------------------------------------------
	//! How many of a plan's units the collected stores hold between them.
	//!
	//! ⚠ TWO LINES CAN NAME THE SAME PREFAB (a vehicle with two identical machine guns), and
	//! CountAvailable answers with the whole stock every time it is asked. The running claim is what
	//! makes this read model the sequential TakeUpTo the handler will run - without it the coverage
	//! would over-count and the player would be undercharged for ammunition they do not have.
	//! \param[in] plan Lines from BuildPlan.
	//! \param[in] stores Stores from CollectRearmStores.
	//! \return Units the stores can cover.
	static int CountCovered(notnull array<ref OVT_RearmUnit> plan, notnull array<OVT_StorageComponent> stores)
	{
		map<string, int> claimed = new map<string, int>();
		int covered = 0;

		foreach(OVT_RearmUnit unit : plan)
		{
			if(!unit || unit.m_sRes == "") continue;

			int already = 0;
			if(claimed.Contains(unit.m_sRes)) already = claimed.Get(unit.m_sRes);

			int free = OVT_WarehouseStockUtils.CountAvailable(stores, unit.m_sRes) - already;
			if(free <= 0) continue;

			int take = unit.m_iUnits;
			if(take > free) take = free;

			covered += take;
			claimed.Set(unit.m_sRes, already + take);
		}

		return covered;
	}

	//------------------------------------------------------------------------------------------------
	//! Fully restock every rearmable weapon on the vehicle. SERVER ONLY - on a client the magazine
	//! call is illegal and the barrel call is ignored.
	//!
	//! A rearm is always FULL, whatever paid for it: sourcing and pricing decide who pays, never how
	//! much ammunition appears.
	//! \param[in] vehicle The vehicle to re-arm.
	static void PerformRearm(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<BaseMuzzleComponent> muzzles;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, muzzles, rocketWeapons);

		foreach(BaseMagazineComponent magazine : magazines)
		{
			magazine.SetAmmoCount(magazine.GetMaxAmmoCount());
		}

		foreach(IEntity rocketWeapon : rocketWeapons)
		{
			ReloadRocketPod(rocketWeapon);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle is somewhere the resistance can SELL it ammunition.
	//!
	//! Two clauses, both answerable on client and server alike (base and FOB records replicate, the
	//! sphere query is local): within FOB_SEARCH_RADIUS of a deployed FOB, or within
	//! SITE_SEARCH_RADIUS of a built Helipad or Garage that is not ruined, at a base the occupying
	//! faction does not hold. Helipads and garages can only be BUILT at bases, but a base can change
	//! hands afterwards - hence the ownership check.
	//!
	//! ⚠ THIS GATE APPLIES ONLY WHEN cost > 0 (R1 as amended). A rearm the crew's own ledgers cover
	//! is free and performable anywhere in the world.
	//! \param[in] pos The vehicle's position.
	//! \return True when the missing ammunition may be bought here.
	bool IsAtRearmSite(vector pos)
	{
		if(IsNearDeployedFOB(pos)) return true;

		return IsAtSupplyStructure(pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Classify one weapon entity: rocket pod, gun magazine, or nothing rearmable.
	protected static void CollectWeapon(IEntity weapon, array<BaseMagazineComponent> magazines, array<BaseMuzzleComponent> muzzles, array<IEntity> rocketWeapons)
	{
		SCR_RocketEjectorMuzzleComponent rocketMuzzle = SCR_RocketEjectorMuzzleComponent.Cast(weapon.FindComponent(SCR_RocketEjectorMuzzleComponent));
		if(rocketMuzzle)
		{
			rocketWeapons.Insert(weapon);
			return;
		}

		MuzzleComponent muzzle = MuzzleComponent.Cast(weapon.FindComponent(MuzzleComponent));
		if(!muzzle) return;

		BaseMagazineComponent magazine = muzzle.GetMagazine();
		if(!magazine) return;

		magazines.Insert(magazine);
		muzzles.Insert(muzzle);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any barrel of a rocket pod accepts a reload. Same loop as
	//! SCR_BaseRocketPodsContextAction.CanReloadRocketPod.
	protected static bool CanReloadRocketPod(IEntity rocketPod)
	{
		return CountReloadableBarrels(rocketPod) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! How many of a rocket pod's barrels are empty - one plan unit each.
	protected static int CountReloadableBarrels(IEntity rocketPod)
	{
		SCR_RocketEjectorMuzzleComponent rocketMuzzle = SCR_RocketEjectorMuzzleComponent.Cast(rocketPod.FindComponent(SCR_RocketEjectorMuzzleComponent));
		if(!rocketMuzzle) return 0;

		int reloadable = 0;

		for(int i = 0, count = rocketMuzzle.GetBarrelsCount(); i < count; i++)
		{
			if(rocketMuzzle.CanReloadBarrel(i)) reloadable += 1;
		}

		return reloadable;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn the pod's default rocket into every empty barrel. Same loop as
	//! SCR_BaseRocketPodsContextAction.ReloadRocketPod. SERVER ONLY.
	protected static void ReloadRocketPod(IEntity rocketPod)
	{
		SCR_RocketEjectorMuzzleComponent rocketMuzzle = SCR_RocketEjectorMuzzleComponent.Cast(rocketPod.FindComponent(SCR_RocketEjectorMuzzleComponent));
		if(!rocketMuzzle) return;

		ResourceName defaultRocketName = rocketMuzzle.GetDefaultRocketPrefab();
		if(defaultRocketName.IsEmpty()) return;

		Resource resource = Resource.Load(defaultRocketName);
		if(!resource.IsValid()) return;

		for(int i = 0, count = rocketMuzzle.GetBarrelsCount(); i < count; i++)
		{
			if(!rocketMuzzle.CanReloadBarrel(i)) continue;

			IEntity rocket = GetGame().SpawnEntityPrefab(resource);
			rocketMuzzle.ReloadBarrel(i, rocket);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The destination picker's authored radius, read off the asking player's own controller so the
	//! rearm and the transfer screen cannot drift apart.
	//! \param[in] playerId The asking player's runtime id.
	//! \return The authored radius, or FALLBACK_HOLDER_RADIUS when the controller is unreachable.
	protected static float ResolveHolderRadius(int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if(players)
		{
			OVT_StorageRequestComponent requests = OVT_ComponentFinder<OVT_StorageRequestComponent>.Find(players.GetController(playerId));
			if(requests) return requests.GetHolderRadius();
		}

		return FALLBACK_HOLDER_RADIUS;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a deployed FOB is close enough to supply this position.
	protected bool IsNearDeployedFOB(vector pos)
	{
		OVT_ResistanceFactionManager resistance = OVT_Global.GetResistanceFaction();
		if(!resistance) return false;

		OVT_FOBData fob = resistance.GetNearestFOBData(pos);
		if(!fob) return false;

		return vector.Distance(fob.location, pos) < FOB_SEARCH_RADIUS;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a usable built helipad or garage at a resistance-held base is close enough.
	protected bool IsAtSupplyStructure(vector pos)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if(!occupyingFaction) return false;

		OVT_BaseData base = occupyingFaction.GetNearestBase(pos);
		if(!base || base.IsOccupyingFaction()) return false;

		BaseWorld world = GetGame().GetWorld();
		if(!world) return false;

		m_bFoundSite = false;
		world.QueryEntitiesBySphere(pos, SITE_SEARCH_RADIUS, FoundSiteCallback, FilterSiteCallback, EQueryEntitiesFlags.ALL);

		return m_bFoundSite;
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter: a built helipad or garage that is not a ruin.
	protected bool FilterSiteCallback(IEntity entity)
	{
		if(!entity) return false;

		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if(!buildable) return false;

		string type = buildable.GetBuildableType();
		if(type != HELIPAD_BUILDABLE_TYPE && type != GARAGE_BUILDABLE_TYPE) return false;

		return OVT_StructureDamage.IsUsable(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Query hit: one supply structure is enough, stop the query.
	protected bool FoundSiteCallback(IEntity entity)
	{
		m_bFoundSite = true;
		return false;
	}
}
