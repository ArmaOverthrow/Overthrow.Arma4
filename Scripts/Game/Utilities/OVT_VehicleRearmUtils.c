//------------------------------------------------------------------------------------------------
//! Shared rules for the stop-gap armed-helicopter re-arm action (superseded later by the
//! logistics epic).
//!
//! One class answers, on both machines, the questions the user action and the server handler must
//! agree on: which of a vehicle's weapons are rearmable, whether any of them are missing
//! ammunition, whether the vehicle is sitting on a built helipad at a resistance-held base, and
//! what a full re-arm costs. Weapon discovery walks the vehicle's SlotManagerComponent the same
//! way the vanilla editor context actions do (SCR_RefillMagazineContextAction for turret gun
//! magazines, SCR_BaseRocketPodsContextAction for rocket pods on a weapons rack), so anything the
//! game master's "Refill magazine" / "Rearm rocket pods" buttons would touch, this touches.
//!
//! The refill itself is server-only: BaseMagazineComponent.SetAmmoCount may only be called on the
//! master, and RocketEjectorMuzzleComponent.ReloadBarrel is ignored on clients. The discovery and
//! "is anything missing" checks are answerable on clients, which is what lets the action show an
//! honest disabled reason.
//------------------------------------------------------------------------------------------------
class OVT_VehicleRearmUtils
{
	//! Base price of a full re-arm, scaled by OVT_DifficultySettings.vehiclePriceMultiplier (the
	//! same replicated difficulty knob that prices the vehicles themselves).
	static const int REARM_BASE_COST = 1000;

	//! How far from the vehicle's origin a built helipad is searched for. The improvised helipad's
	//! own heli parking box is 16 m wide (OVT_ParkingComponent), so this covers landing anywhere on
	//! the pad plus a sloppy touchdown at its edge.
	static const float HELIPAD_SEARCH_RADIUS = 15;

	//! The OVT_BuildableComponent type string authored on the helipad prefab (Helipad.et).
	static const string HELIPAD_BUILDABLE_TYPE = "Helipad";

	//! Whether the sphere query below has found a built helipad. Instance state because world
	//! queries report through member callbacks.
	protected bool m_bFoundHelipad;

	//------------------------------------------------------------------------------------------------
	//! What a full re-arm costs at the current difficulty.
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
	//! magazine. An absent weapons rack is NOT spawned in: a helicopter that never carried weapons
	//! has nothing to re-arm.
	//! \param[in] vehicle The vehicle to scan.
	//! \param[out] magazines Every refillable gun magazine found.
	//! \param[out] rocketWeapons Every rocket-pod weapon entity found.
	static void GetRearmableWeapons(IEntity vehicle, out array<BaseMagazineComponent> magazines, out array<IEntity> rocketWeapons)
	{
		magazines = {};
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
			CollectWeapon(attached, magazines, rocketWeapons);

			// ...and/or a holder of weapons (a turret or a weapons rack)
			array<Managed> weaponSlots = {};
			attached.FindComponents(WeaponSlotComponent, weaponSlots);
			foreach(Managed comp : weaponSlots)
			{
				WeaponSlotComponent weaponSlot = WeaponSlotComponent.Cast(comp);
				if(!weaponSlot) continue;

				IEntity weapon = weaponSlot.GetWeaponEntity();
				if(!weapon) continue;

				CollectWeapon(weapon, magazines, rocketWeapons);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle has any rearmable weapon at all - the "is this an armed helicopter"
	//! question, answered structurally rather than from prefab labels.
	//! \param[in] vehicle The vehicle to scan.
	//! \return True when there is at least one gun magazine or rocket pod.
	static bool IsArmed(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, rocketWeapons);

		return !magazines.IsEmpty() || !rocketWeapons.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the vehicle is missing any ammunition a re-arm would restore.
	//! \param[in] vehicle The vehicle to scan.
	//! \return True when any gun magazine is below capacity or any rocket barrel can be reloaded.
	static bool NeedsRearm(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, rocketWeapons);

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
	//! Fully restock every rearmable weapon on the vehicle. SERVER ONLY - on a client the magazine
	//! call is illegal and the barrel call is ignored.
	//! \param[in] vehicle The vehicle to re-arm.
	static void PerformRearm(IEntity vehicle)
	{
		array<BaseMagazineComponent> magazines;
		array<IEntity> rocketWeapons;
		GetRearmableWeapons(vehicle, magazines, rocketWeapons);

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
	//! Whether the vehicle sits on a built helipad at a base the resistance holds.
	//!
	//! Two conditions, both answerable on client and server alike: a built structure whose
	//! OVT_BuildableComponent declares itself a helipad within HELIPAD_SEARCH_RADIUS of the
	//! position, and the nearest base not held by the occupying faction (the same ownership test
	//! OVT_MainMenuContextOverrideComponent applies to the helipad's own shop menu). Helipads can
	//! only be BUILT at bases, but the base can change hands afterwards - hence the second check.
	//! \param[in] pos The vehicle's position.
	//! \return True when a re-arm may be offered here.
	bool IsOnHelipadAtFriendlyBase(vector pos)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if(!occupyingFaction) return false;

		OVT_BaseData base = occupyingFaction.GetNearestBase(pos);
		if(!base || base.IsOccupyingFaction()) return false;

		BaseWorld world = GetGame().GetWorld();
		if(!world) return false;

		m_bFoundHelipad = false;
		world.QueryEntitiesBySphere(pos, HELIPAD_SEARCH_RADIUS, FoundHelipadCallback, FilterHelipadCallback, EQueryEntitiesFlags.ALL);

		return m_bFoundHelipad;
	}

	//------------------------------------------------------------------------------------------------
	//! Classify one weapon entity: rocket pod, gun magazine, or nothing rearmable.
	protected static void CollectWeapon(IEntity weapon, array<BaseMagazineComponent> magazines, array<IEntity> rocketWeapons)
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
		if(magazine) magazines.Insert(magazine);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any barrel of a rocket pod accepts a reload. Same loop as
	//! SCR_BaseRocketPodsContextAction.CanReloadRocketPod.
	protected static bool CanReloadRocketPod(IEntity rocketPod)
	{
		SCR_RocketEjectorMuzzleComponent rocketMuzzle = SCR_RocketEjectorMuzzleComponent.Cast(rocketPod.FindComponent(SCR_RocketEjectorMuzzleComponent));
		if(!rocketMuzzle) return false;

		for(int i = 0, count = rocketMuzzle.GetBarrelsCount(); i < count; i++)
		{
			if(rocketMuzzle.CanReloadBarrel(i)) return true;
		}

		return false;
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
	//! Query filter: only entities that are a built helipad.
	protected bool FilterHelipadCallback(IEntity entity)
	{
		if(!entity) return false;

		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if(!buildable) return false;

		return buildable.GetBuildableType() == HELIPAD_BUILDABLE_TYPE;
	}

	//------------------------------------------------------------------------------------------------
	//! Query hit: one helipad is enough, stop the query.
	protected bool FoundHelipadCallback(IEntity entity)
	{
		m_bFoundHelipad = true;
		return false;
	}
}
