//------------------------------------------------------------------------------------------------
//! Holder addressing: every storage holder is an RplId and nothing else.
//!
//! There is deliberately no second vocabulary here - no warehouse index, no per-manager id, no
//! EntityID on the wire. An EntityID names a different entity (or nothing) on the other machine; an
//! RplId is the only reference that means the same thing on both.
//------------------------------------------------------------------------------------------------
class OVT_StorageUtils
{
	//------------------------------------------------------------------------------------------------
	//! A networked holder reference back to this machine's copy of the entity.
	//! \param[in] id The holder id carried by the route.
	//! \return The entity, or null when the id resolves to nothing here (always a rejection, never a retry).
	static IEntity ResolveHolder(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl)
			return null;

		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! The storage component on an entity, if it has one.
	//! \param[in] e The candidate holder.
	//! \return Its OVT_StorageComponent, or null.
	//! The holder behind an entity a user action lives on.
	//!
	//! A truck's cargo bed is a SLOTTED CHILD with its own ActionsManagerComponent, and vanilla's real
	//! "door_rear" tailgate context lives there - so an action reachable from the bed is hosted on the
	//! bed while the storage is on the vehicle root. One hop up is enough (OVT_LootIntoVehicleAction:68).
	//! \param[in] e The action's owner.
	//! \return e when it holds the storage, otherwise its parent when THAT does, otherwise e unchanged.
	static IEntity ResolveStorageHolder(IEntity e)
	{
		if (!e)
			return e;

		if (OVT_ComponentFinder<OVT_StorageComponent>.Find(e))
			return e;

		IEntity parent = e.GetParent();
		if (parent && OVT_ComponentFinder<OVT_StorageComponent>.Find(parent))
			return parent;

		return e;
	}

	//------------------------------------------------------------------------------------------------
	static OVT_StorageComponent GetStorage(IEntity e)
	{
		return OVT_ComponentFinder<OVT_StorageComponent>.Find(e);
	}

	//------------------------------------------------------------------------------------------------
	//! The inventory manager that can actually see everything a holder is carrying.
	//!
	//! ⚠ THE ROOT-ONLY PATH MISSES TRUCK BEDS. A truck's cargo lives on ATTACHED CHILD entities, and
	//! only SCR_VehicleInventoryStorageManagerComponent registers those child storages
	//! (FillInitialStorages walks the slot manager). OVT_InventoryManagerComponent reads
	//! GetOwnedItems() off the root instead and misses them - the trap OVT_SellableItemScanner:77-106
	//! documents. Ask for the vehicle manager first, and only fall back to the plain one for boxes and
	//! buildings, which have no child storages to miss.
	//! \param[in] e The holder.
	//! \return The manager, or null when the holder has no inventory at all.
	static InventoryStorageManagerComponent GetInventoryManager(IEntity e)
	{
		if (!e)
			return null;

		SCR_VehicleInventoryStorageManagerComponent vehicle = SCR_VehicleInventoryStorageManagerComponent.Cast(e.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
		if (vehicle)
			return vehicle;

		return InventoryStorageManagerComponent.Cast(e.FindComponent(InventoryStorageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Moves one holder's entire ledger into another's. Server-side, zero spawns, one BumpMe each.
	//!
	//! For the two shipped flows that REPLACE a vehicle with another and then delete the original -
	//! FOB deploy and the vehicle upgrade. The destination takes everything at UNLIMITED capacity
	//! deliberately: the source is about to be destroyed, so clamping could only lose stock, and a
	//! destination spawned this frame has not resolved its own capacity yet anyway.
	//! \param[in] from The holder being emptied.
	//! \param[in] to The holder being filled.
	//! \return How many items arrived.
	static int MoveWholeLedger(IEntity from, IEntity to)
	{
		if (!Replication.IsServer() || !from || !to)
			return 0;

		OVT_StorageComponent source = GetStorage(from);
		OVT_StorageComponent dest = GetStorage(to);
		if (!source || !dest || source == dest)
			return 0;

		OVT_StorageLedger sourceLedger = source.GetLedger();
		OVT_StorageLedger destLedger = dest.GetLedger();
		if (!sourceLedger || !destLedger || sourceLedger.Total() <= 0)
			return 0;

		array<string> res = new array<string>();
		array<int> counts = new array<int>();
		sourceLedger.GetLines(res, counts);

		int moved = 0;

		for (int i = 0; i < res.Count(); i++)
		{
			int shortfall;
			moved += OVT_StorageRules.TransferLedgerLine(sourceLedger, destLedger, res[i], counts[i], OVT_StorageComponent.UNLIMITED_CAPACITY, shortfall);
		}

		source.PublishCount();
		dest.PublishCount();

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! An entity's networked name.
	//! \param[in] e The holder to name.
	//! \return Its RplId, or RplId.Invalid() when it is not replicated and therefore cannot be named.
	static RplId GetHolderId(IEntity e)
	{
		if (!e)
			return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(e.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}
}

//------------------------------------------------------------------------------------------------
//! Finds every usable storage holder in a radius.
//!
//! ONE INSTANCE PER CALL. The accumulator is a member of this object and is re-created by every
//! Run(), never shared and never static. OVT_InventoryManagerComponent.m_aContainerSearchResults
//! (:497) is a singleton that every concurrent search points at the caller's array in turn, so two
//! players searching at once fill each other's results - one of the defects this feature exists to
//! stop repeating. `new` one of these per call, on the server for validation and on the client for
//! the destination picker.
//------------------------------------------------------------------------------------------------
class OVT_StorageHolderQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<IEntity> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! Collects the holders around a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[out] results Receives the holders; cleared first, so a reused buffer never accumulates.
	//! \return How many holders were found.
	int Run(vector pos, float radius, out array<IEntity> results)
	{
		if (!results)
			results = new array<IEntity>();

		results.Clear();

		m_aResults = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterHolders, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		foreach (IEntity holder : m_aResults)
		{
			results.Insert(holder);
		}

		// Nothing outside Run() may reach into the accumulator, so it does not outlive the call.
		m_aResults = null;

		return results.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter: a holder is an entity with a storage component that actually holds something.
	//! Capacity 0 means "not a holder" (an illegal or armed vehicle, or an authored NONE), and those
	//! must never appear as a transfer destination.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterHolders(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		OVT_StorageComponent storage = OVT_StorageUtils.GetStorage(e);
		if (storage && storage.GetCapacity() != 0)
			m_aResults.Insert(e);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Finds the dead bodies and every loose item around a point, for the LOOT job.
//!
//! ONE INSTANCE PER CALL, for the same reason OVT_StorageHolderQuery is. The shipped
//! OVT_WorldUtils.GetNearbyBodiesAndWeapons keeps its accumulator in a STATIC
//! (OVT_WorldUtils.m_Bodies), so two players looting at once fill each other's lists - the B2 defect
//! this feature is not allowed to repeat, which is why the query is re-declared here rather than
//! reused.
//------------------------------------------------------------------------------------------------
class OVT_StorageLootQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<IEntity> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! Collects the lootables around a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[out] results Receives the bodies and weapons; cleared first.
	//! \return How many were found.
	int Run(vector pos, float radius, out array<IEntity> results)
	{
		if (!results)
			results = new array<IEntity>();

		results.Clear();

		m_aResults = new array<IEntity>();

		// ALL, matching the shipped OVT_WorldUtils query this replaces - a body can be sleeping and a
		// dropped weapon can be static, and neither may be missed because of a flag.
		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterLootables, EQueryEntitiesFlags.ALL);

		foreach (IEntity found : m_aResults)
		{
			results.Insert(found);
		}

		// Nothing outside Run() may reach into the accumulator, so it does not outlive the call.
		m_aResults = null;

		return results.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter: a dead character, or anything lying on the ground.
	//!
	//! ⚠ A LOOT RUN DELETES WHAT IT EMPTIES, so "destroyed damage manager" is NOT the body test any
	//! more - a ruined Overthrow building and a wrecked truck both report one, and neither may be
	//! deleted by a loot run. A body is a character; everything else is reached as a loose item.
	//!
	//! An item still slotted into a body, a container or a vehicle has a parent slot and is deliberately
	//! NOT queued: the job walks its owner's tree and would otherwise price the same entity twice.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterLootables(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		// A holder is never loot. It is emptied through the transfer screen, not destroyed.
		if (OVT_StorageUtils.GetStorage(e))
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(e);
		if (character)
		{
			DamageManagerComponent damage = DamageManagerComponent.Cast(e.FindComponent(DamageManagerComponent));
			if (damage && damage.IsDestroyed())
				m_aResults.Insert(e);

			return false;
		}

		// An ARMED mine or a placed charge is not litter, it is somebody's ordnance. A loot run DELETES
		// what it prices, so collecting one silently disarms the minefield the player just laid.
		if (IsEmplaced(e))
			return false;

		InventoryItemComponent item = InventoryItemComponent.Cast(e.FindComponent(InventoryItemComponent));
		if (item && !item.GetParentSlot())
			m_aResults.Insert(e);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an entity has been PUT somewhere rather than dropped there.
	//!
	//! This is vanilla's own vicinity-hiding rule, re-derived: SCR_MineInventoryItemComponent and
	//! SCR_DeployableInventoryItemInventoryComponent both answer ShouldHideInVicinity() from exactly
	//! these tests, but that event is protected and cannot be called from here.
	//!
	//! An UNARMED mine still in its box or lying loose is ordinary loot, and stays so.
	//! \param[in] e The candidate.
	//! \return True when it is emplaced and must be left alone.
	protected bool IsEmplaced(IEntity e)
	{
		SCR_BaseTriggerComponent trigger = SCR_BaseTriggerComponent.Cast(e.FindComponent(SCR_BaseTriggerComponent));
		if (trigger && trigger.IsActivated())
			return true;

		SCR_BaseDeployableInventoryItemComponent deployable = SCR_BaseDeployableInventoryItemComponent.Cast(e.FindComponent(SCR_BaseDeployableInventoryItemComponent));
		if (deployable && deployable.IsDeployed())
			return true;

		// A charge stuck to a wall carries no trigger until it is fuzed, and the engine locks it in
		// place instead - which is the test SCR_ExplosiveChargeInventoryItemComponent itself uses.
		if (e.FindComponent(SCR_ExplosiveChargeComponent))
		{
			InventoryItemComponent charge = InventoryItemComponent.Cast(e.FindComponent(InventoryItemComponent));
			if (charge && charge.IsLocked())
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Finds the PLAYER-PLACED containers around a point, for the FOB undeploy collection.
//!
//! The filter is the shipped one (OVT_InventoryManagerComponent.IsCollectableContainer): a vanilla
//! storage plus an OVT_PlaceableComponent or an OVT_BuildableComponent. It is deliberately NOT the
//! storage-holder filter OVT_StorageHolderQuery uses - that one would sweep a bystander's parked car
//! into the truck, which is not what undeploy has ever meant.
//!
//! ONE INSTANCE PER CALL, like every other query in this feature.
//------------------------------------------------------------------------------------------------
class OVT_StorageContainerQuery : Managed
{
	//! Per-instance, re-created by every Run(). The query callback is the only writer.
	protected ref array<IEntity> m_aResults;

	//------------------------------------------------------------------------------------------------
	//! Collects the containers around a point.
	//! \param[in] pos Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[out] results Receives the containers; cleared first.
	//! \return How many were found.
	int Run(vector pos, float radius, out array<IEntity> results)
	{
		if (!results)
			results = new array<IEntity>();

		results.Clear();

		m_aResults = new array<IEntity>();

		BaseWorld world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, radius, null, FilterContainers, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		foreach (IEntity found : m_aResults)
		{
			results.Insert(found);
		}

		// Nothing outside Run() may reach into the accumulator, so it does not outlive the call.
		m_aResults = null;

		return results.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter.
	//! \param[in] e The entity the query offered.
	//! \return Always false - the query runs to completion.
	protected bool FilterContainers(IEntity e)
	{
		if (!e || !m_aResults)
			return false;

		// A LEDGER IS A CONTAINER TOO. The one caller is the FOB undeploy sweep, and what it sweeps
		// must match what OVT_ResistanceFactionManager.CleanupFOBArea then DELETES - which is every
		// placeable and buildable in the radius, whether or not it has a vanilla inventory. A
		// recruitment tent's crate and a built warehouse both hold stock in a ledger only, and both
		// used to be destroyed with their contents.
		if (!e.FindComponent(UniversalInventoryStorageComponent) && !OVT_StorageUtils.GetStorage(e))
			return false;

		if (e.FindComponent(OVT_PlaceableComponent) || e.FindComponent(OVT_BuildableComponent))
			m_aResults.Insert(e);

		return false;
	}
}
