//------------------------------------------------------------------------------------------------
//! Parks faction vehicles in a base's parking spots. Direct port of the legacy parked-vehicles base
//! upgrade's BuyCar/BuyTruck, which were the same method twice. That class is gone; its reasons are
//! preserved here and in implementation.md 3.3 of the base-defense-migration feature.
//!
//! SHIPPED BY: Deployment_BaseParkedVehicles.conf (truck x1, PARKING_TRUCK, priority 10).
//!
//! ⚠ THAT CONFIG AUTHORS NO REINFORCEMENT MODULE, AND THAT IS WHAT KEEPS IT ALIVE. The
//! m_bDeleteOnConditionFail path lives INSIDE OVT_ReinforcementBehaviorDeploymentModule
//! .CheckReinforcement(), so a deployment with no reinforcement module has no collection path at all.
//! Correct here - there is nothing to rebuy and a parked truck the player stole must not be replaced -
//! but it means the deployment persists for the life of the campaign whatever its conditions say.
//!
//! ================== IT DOES NOT SUBCLASS THE INFANTRY MODULE (D6) =======================
//! It registers NO groups. There is no crew, no waypoint plan, no owner key, no survivor mask and
//! nothing for the reinforcement module to rebuy - a parked truck is set dressing that the player can
//! steal, and stealing it is the point. Inheriting group machinery it never uses would be worse than
//! not having it, so this is the one new spawning module that sits directly on
//! OVT_BaseSpawningDeploymentModule.
//! =======================================================================================
//!
//! ================== A RESTORED DEPLOYMENT PARKS NOTHING (D7) ============================
//! Vehicles are persisted by the vehicle manager, not by this module, and they come back from the
//! save before any deployment ticks. A restored deployment that parked its trucks again would add a
//! truck per load, forever - the same failure the composition module's gate exists to stop.
//! =======================================================================================
//!
//! ⚠ SPAWNS ONCE, AND NEVER REPLACES. A destroyed or stolen vehicle is not re-bought: legacy did not
//! either (m_Cars/m_Trucks were never pruned, so their counts never dropped below the target and
//! BuyCar was never reached a second time), and a base that quietly regrew its motor pool every time
//! the player took a truck would be an infinite vehicle source.
//!
//! ================== THE LADDER, OPT-IN (occupying/vehicles Phase 5, G8) =================
//! m_sVehicleRole resolves a rung off the live threat exactly as the mounted module does, with
//! m_iCostPerVehicle as the budget. EMPTY IS BYTE-IDENTICAL to this module's behaviour before the role
//! existed - ResolveVehiclePrefab() falls straight through to its own original, unedited body.
//!
//! ⚠ A PARKED VEHICLE MAY BE HANDED OFF WHILE IT STANDS UNCREWED (see ReleaseVehicleOwnership). The
//! hull stays owned by THIS module - and is deleted by this module's own teardown - until something
//! else takes it over; a caller that resolves a vehicle through GetSpawnedEntities() and crews it
//! without releasing ownership here leaves two modules both believing they own it, and both delete it
//! at teardown.
//! =======================================================================================
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_ParkedVehicleSpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "truck", desc: "Vehicle type name from the faction's VEHICLE registry. Consulted only when m_sVehicleRole is empty or the ladder answers nothing")]
	string m_sVehicleType;

	//! Ladder role in the faction VEHICLE registry (occupying/vehicles G8), e.g. "armed". EMPTY IS
	//! BYTE-IDENTICAL TO TODAY: ResolveVehiclePrefab() delegates straight to the unchanged
	//! ResolveNamedVehiclePrefab() body when this is empty, exactly as before this field existed. A
	//! non-empty role is resolved against the live threat with m_iCostPerVehicle as the per-vehicle
	//! budget - the same "budget, not receipt" rule the mounted module charges by (D4): a base that
	//! wants to park a BTR-70 has to author m_iCostPerVehicle >= 120. A miss falls back to
	//! m_sVehicleType, same as an unauthored role.
	[Attribute(defvalue: "", desc: "Ladder role in the faction VEHICLE registry, e.g. \"armed\". Empty parks the named vehicle type exactly as before this field existed")]
	string m_sVehicleRole;

	[Attribute(defvalue: "1", desc: "How many to park")]
	int m_iVehicleCount;

	[Attribute(defvalue: "90", desc: "Resource cost per vehicle")]
	int m_iCostPerVehicle;

	//! Which kind of parking spot to ask an OVT_ParkingComponent for. NOT derivable from the registry
	//! name - a spot is sized by the building, and asking for a car spot for a Ural finds none at all.
	//! Legacy hard-coded PARKING_CAR in BuyCar and PARKING_TRUCK in BuyTruck; this is that choice,
	//! authored.
	[Attribute(defvalue: "1", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(OVT_ParkingType), desc: "Parking spot size to request")]
	OVT_ParkingType m_eParkingType;

	//! Whether the one parking pass has already run. See the class header - there is no second pass.
	protected bool m_bParked;

	//------------------------------------------------------------------------------------------------
	override int GetResourceCost()
	{
		return m_iVehicleCount * m_iCostPerVehicle;
	}

	//------------------------------------------------------------------------------------------------
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_ParkedVehicleSpawningDeploymentModule clone = new OVT_ParkedVehicleSpawningDeploymentModule();

		// EVERY attribute. A forgotten one ships the class default in silence - drop m_eParkingType and
		// every truck asks for a car spot and never finds one, so the base parks nothing and says
		// nothing about it. Drop m_sVehicleRole and every armour delta silently reverts to parking
		// m_sVehicleType instead - the escalation Phase 5 exists for never happens, with nothing in the
		// log to say why.
		clone.m_sModuleName = m_sModuleName;
		clone.m_sVehicleType = m_sVehicleType;
		clone.m_sVehicleRole = m_sVehicleRole;
		clone.m_iVehicleCount = m_iVehicleCount;
		clone.m_iCostPerVehicle = m_iCostPerVehicle;
		clone.m_eParkingType = m_eParkingType;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();

		EnsureGroups();
	}

	//------------------------------------------------------------------------------------------------
	//! Parks the vehicles, once. Named EnsureGroups because that is the framework's convergence entry
	//! point on every spawning module - this one has no groups to converge, only a one-shot to guard.
	override void EnsureGroups()
	{
		if (m_bParked)
			return;

		if (!m_ParentDeployment)
			return;

		if (m_ParentDeployment.WasRestoredFromSave())
		{
			// Latch it, so a later reinforcement or re-activation cannot park a second motor pool on top
			// of the one the save restored.
			m_bParked = true;
			return;
		}

		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
			return;

		OVT_BaseControllerComponent controller = FindNearestBaseController();
		if (!controller || !controller.m_Parking || controller.m_Parking.IsEmpty())
		{
			// NOT latched: the base controller may not have run FindParking() yet, and the next
			// convergence is a free retry.
			return;
		}

		ResourceName prefab = ResolveVehiclePrefab();
		if (prefab == ResourceName.Empty)
		{
			Print(string.Format("[Overthrow] Deployment '%1': vehicle registry has no entry named '%2', nothing to park",
				m_ParentDeployment.GetDeploymentName(), m_sVehicleType), LogLevel.WARNING);
			m_bParked = true;
			return;
		}

		m_bParked = true;

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();
		if (!vehicles)
			return;

		int parked = 0;
		for (int i = 0; i < m_iVehicleCount; i++)
		{
			if (ParkOne(controller, vehicles, prefab))
				parked++;
		}

		Print(string.Format("[Overthrow] Deployment '%1' parked %2/%3 '%4'",
			m_ParentDeployment.GetDeploymentName(), parked.ToString(), m_iVehicleCount.ToString(), m_sVehicleType), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Every vehicle this module parked that still exists.
	//! \return The live vehicles; empty when none were parked or all are gone.
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = new array<IEntity>;

		if (!m_aSpawnedEntities)
			return entities;

		foreach (EntityID id : m_aSpawnedEntities)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (entity)
				entities.Insert(entity);
		}

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands ownership of a parked vehicle to whoever is crewing it (occupying/vehicles Phase 5's
	//! ownership-transfer contract - see the class-less note in context.md and
	//! OVT_CrewUpOnAlarmBehaviorDeploymentModule's header).
	//!
	//! ⚠ THE VEHICLE IS REMOVED FROM m_aSpawnedEntities, NOT DELETED. That is the whole of the contract:
	//! this module's own teardown only ever deletes entities still IN that array (see the base class's
	//! OnCleanup), and GetSpawnedEntities() only ever reports entities still in it - so the moment this
	//! runs, this module stops claiming the vehicle in both directions at once. Skipping this call and
	//! deleting the hull through some other path would leave BOTH modules still listing it; skipping it
	//! and leaving the hull alone would have BOTH modules delete it at their own teardown - the
	//! double-delete this method exists to prevent.
	//!
	//! IDEMPOTENT. array.RemoveItem() on an id that is not present is a no-op, so a caller that is not
	//! sure whether this already ran may call it again safely.
	//! \param[in] vehicle The hull being taken over. Ignored (not an error) when this module never
	//! parked it - a caller that resolved the vehicle through this module's own GetSpawnedEntities()
	//! cannot hand in one it does not own.
	void ReleaseVehicleOwnership(notnull Vehicle vehicle)
	{
		if (!m_aSpawnedEntities)
			return;

		// array.Remove() is swap-with-last (unordered); RemoveItem() shares that shape. The order
		// parked vehicles were spawned in has no meaning - unlike the survivor-mask arrays elsewhere in
		// this tree - so that is fine here.
		m_aSpawnedEntities.RemoveItem(vehicle.GetID());
	}

	//------------------------------------------------------------------------------------------------
	//! Registers an already-spawned vehicle as parked by this module, bypassing the building/parking-spot
	//! search in EnsureGroups().
	//!
	//! ⚠ PUBLIC SO THAT THE INITIALISATION TIER CAN DRIVE ONE, exactly as
	//! OVT_QRFControllerComponent.IsFightingFit and SendMountedEchelon are - a fixture that needs "this
	//! base already has a parked vehicle" would otherwise need a real base controller with real parking
	//! spots and a whole 8-12 s update interval for EnsureGroups() to run at all. Not called from
	//! anywhere in the campaign.
	//! \param[in] vehicle The entity to register. Not required to be armed or even a Vehicle - this
	//! mirrors ParkOne()'s own Insert(), which asks nothing of what it is handed either.
	void PlantParkedVehicle(notnull IEntity vehicle)
	{
		if (!m_aSpawnedEntities)
			m_aSpawnedEntities = new array<ref EntityID>;

		m_aSpawnedEntities.Insert(vehicle.GetID());
		m_bParked = true;
	}

	//------------------------------------------------------------------------------------------------
	//! One vehicle into one random parking building. Port of the legacy upgrade's BuyTruck body.
	//! \param[in] controller The base controller holding the parking list.
	//! \param[in] vehicles The vehicle manager, which owns spot resolution and vehicle registration.
	//! \param[in] prefab The vehicle prefab to park.
	//! \return True when a vehicle now stands in a spot.
	protected bool ParkOne(notnull OVT_BaseControllerComponent controller, notnull OVT_VehicleManagerComponent vehicles, ResourceName prefab)
	{
		EntityID buildingId = controller.m_Parking.GetRandomElement();
		IEntity building = GetGame().GetWorld().FindEntityByID(buildingId);
		if (!building)
			return false;

		vector spot[4];
		if (!vehicles.GetParkingSpot(building, spot, m_eParkingType))
			return false;

		IEntity vehicle = vehicles.SpawnVehicleMatrix(prefab, spot);
		if (!vehicle)
			return false;

		if (m_aSpawnedEntities)
			m_aSpawnedEntities.Insert(vehicle.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab behind this module's authored registry name or ladder role, for the deployment's own
	//! faction.
	//!
	//! ⚠ EMPTY ROLE IS BYTE-IDENTICAL TO BEFORE m_sVehicleRole EXISTED (G8): this delegates straight to
	//! ResolveNamedVehiclePrefab(), the method's own original body, unchanged. A role is tried FIRST and
	//! only when authored; a ladder miss (unauthored faction, threat under every rung, budget under the
	//! cheapest) falls back to the same named-vehicle path every other config already takes.
	//! \return The prefab, or ResourceName.Empty.
	protected ResourceName ResolveVehiclePrefab()
	{
		if (!m_sVehicleRole.IsEmpty())
		{
			ResourceName ladderPrefab = ResolveLadderPrefab();
			if (ladderPrefab != ResourceName.Empty)
				return ladderPrefab;
		}

		return ResolveNamedVehiclePrefab();
	}

	//------------------------------------------------------------------------------------------------
	//! THE LADDER, resolved against the live threat rather than a name in a config - the same query the
	//! mounted module's GetVehiclePrefabFromFaction() asks, with m_iCostPerVehicle as the budget instead
	//! of m_iTruckCostOverride (D4: a price is a budget, not a receipt, computed off the config template
	//! before any deployment - and therefore any faction - exists).
	//! \return The picked rung's prefab, or ResourceName.Empty - one of the roads to the named fallback.
	protected ResourceName ResolveLadderPrefab()
	{
		if (!m_ParentDeployment)
			return ResourceName.Empty;

		string key = ResolveFactionKey(m_ParentDeployment.GetControllingFaction());
		if (key.IsEmpty())
			return ResourceName.Empty;

		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return ResourceName.Empty;

		OVT_Faction faction = factions.GetOverthrowFactionByKey(key);
		if (!faction)
			return ResourceName.Empty;

		faction.InitializeVehicleRegistry();

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		float scale = 1;
		if (difficulty)
			scale = difficulty.vehicleThresholdScale;

		float threat = 0;
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying)
			threat = occupying.GetThreatFloat();

		OVT_FactionVehicleEntry entry;
		if (!faction.ResolveVehicleForRole(m_sVehicleRole, threat, scale, m_iCostPerVehicle, entry))
			return ResourceName.Empty;

		if (entry.m_sVehiclePrefab.IsEmpty())
			return ResourceName.Empty;

		Print(string.Format("[Overthrow] Deployment '%1': role '%2' at threat %3 resolved to '%4'",
			m_ParentDeployment.GetDeploymentName(), m_sVehicleRole, Math.Round(threat).ToString(), entry.m_sVehicleName), LogLevel.NORMAL);

		return entry.m_sVehiclePrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! THE ORIGINAL BODY, UNCHANGED (G8) - today's m_sVehicleType path, byte-identical to before
	//! m_sVehicleRole existed.
	//!
	//! Deliberately NOT through OVT_OverthrowFactionManager.GetOverthrowFactionByIndex(), which
	//! dereferences GetFactionByIndex(index) unguarded and would VME on a stale index - the base
	//! class's ResolveFactionKey() is the guarded route and every deployment module uses it.
	//! \return The prefab, or ResourceName.Empty.
	protected ResourceName ResolveNamedVehiclePrefab()
	{
		if (m_sVehicleType.IsEmpty() || !m_ParentDeployment)
			return ResourceName.Empty;

		string key = ResolveFactionKey(m_ParentDeployment.GetControllingFaction());
		if (key.IsEmpty())
			return ResourceName.Empty;

		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return ResourceName.Empty;

		OVT_Faction faction = factions.GetOverthrowFactionByKey(key);
		if (!faction)
			return ResourceName.Empty;

		return faction.GetVehiclePrefabByName(m_sVehicleType);
	}

	//------------------------------------------------------------------------------------------------
	//! The base controller whose parking this module may use.
	//! \return The controller, or null.
	protected OVT_BaseControllerComponent FindNearestBaseController()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		OVT_BaseData nearest = occupying.GetNearestBase(GetDeploymentPosition());
		if (!nearest)
			return null;

		IEntity marker = GetGame().GetWorld().FindEntityByID(nearest.entId);
		if (!marker)
			return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Parked Vehicle Module: %1", m_sModuleName));
		Print(string.Format("  Vehicle Type: %1 x%2", m_sVehicleType, m_iVehicleCount));
		Print(string.Format("  Parking Type: %1", typename.EnumToString(OVT_ParkingType, m_eParkingType)));
		Print(string.Format("  Live vehicles: %1", GetSpawnedEntities().Count()));
	}
}
