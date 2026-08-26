//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Overthrow/Components", description: "Reacts to a truck's resource load: shows the vanilla crate stacks on the bed and closes the bed seats while loaded")]
class OVT_ResourceCargoBedComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Makes a loaded truck LOOK loaded, reusing the crate stacks vanilla already slots onto every
//! transport bed.
//!
//! Every vanilla truck cargo bed (M923A1_cargo.et, Ural4320_cargo.et and their variants) carries a
//! SlotManagerComponent with six slots named SupplyStorage_01..06, each pre-filled with
//! SupplyStack_Large_01_Vehicle.et. Conflict drives them through SCR_ResourceComponent, whose EOnInit
//! hides every stack whose container reads 0 - which is why an Overthrow truck shows a bare bed. This
//! component re-shows them from OUR ledger instead, so nothing in the data path touches SCR_Resource*.
//!
//! PRESENTATION ONLY, AND PURELY LOCAL. The crate count is derived from OVT_ResourceStoreComponent's
//! replicated packed contents, so every machine reaches the same answer on its own - no RPC, no
//! authority check, no new replicated state.
//!
//! VISIBILITY FLAGS ONLY - the simulation state is deliberately left as vanilla's EOnInit left it
//! (NONE). The stacks slot with MergePhysics, and toggling a merged body on a moving vehicle is not
//! worth the risk for decoration nobody can walk into.
//!
//! IT ALSO CLOSES THE BED SEATS. Vanilla's own supply gate on Get In is a player-only hotfix that
//! "does not block AI or Editor actions" (SCR_GetInUserAction.c:76); the modded action carries the
//! player half, and this carries the AI half through SetCompartmentsAccessibleForAI. AI boarding code
//! writes the same flag for its own reasons (SCR_AIGroup.c:2024), so a lapse is possible - it is
//! re-asserted on every contents change, which is the only moment it can start mattering.
//------------------------------------------------------------------------------------------------
class OVT_ResourceCargoBedComponent : OVT_Component
{
	//! Vanilla authors six; probing past that costs nothing and covers a modded bed.
	protected const int MAX_CRATE_SLOTS = 12;

	protected const int RESOLVE_RETRY_INTERVAL_MS = 1000;

	//! A truck with no supply slots (tanker, ammo, repair) must stop retrying rather than poll forever.
	protected const int MAX_RESOLVE_ATTEMPTS = 10;

	protected ref array<IEntity> m_aCrates;
	protected SCR_BaseCompartmentManagerComponent m_BedCompartments;
	protected OVT_ResourceStoreComponent m_Store;
	protected bool m_bSubscribed;
	protected int m_iAttempts;

	//! How many crates are currently shown, so an unchanged count skips the flag writes.
	protected int m_iShown = -1;

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The truck
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		// A throwaway ItemPreview icon: no crates and no callqueue entry.
		if (IsPreviewInstance(owner))
			return;

		// The bed is a slotted child and its SCR_ResourceComponent.EOnInit hides the stacks, so the
		// first look must happen after this frame or every crate is re-hidden behind us.
		GetGame().GetCallqueue().CallLater(Resolve, RESOLVE_RETRY_INTERVAL_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The truck
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Resolve);

		if (m_bSubscribed && m_Store)
			m_Store.GetOnContentsChanged().Remove(OnContentsChanged);

		m_bSubscribed = false;
		m_Store = null;
		m_aCrates = null;
		m_BedCompartments = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the store and the bed's crate stacks, then subscribes. Gives up after MAX_RESOLVE_ATTEMPTS
	//! so a truck family with no supply slots stops costing a timer.
	protected void Resolve()
	{
		m_iAttempts++;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!m_Store)
			m_Store = OVT_ResourceStoreComponent.Cast(owner.FindComponent(OVT_ResourceStoreComponent));

		if (!m_aCrates)
			m_aCrates = CollectCrates(owner);

		if (!m_Store || m_aCrates.IsEmpty())
		{
			if (m_iAttempts < MAX_RESOLVE_ATTEMPTS)
				GetGame().GetCallqueue().CallLater(Resolve, RESOLVE_RETRY_INTERVAL_MS);

			return;
		}

		m_Store.GetOnContentsChanged().Insert(OnContentsChanged);
		m_bSubscribed = true;

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] store This truck's own store
	protected void OnContentsChanged(OVT_ResourceStoreComponent store)
	{
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Walks the truck's slots for the bed, then the bed's SupplyStorage_NN slots for the stacks.
	//! \param[in] owner The truck
	//! \return Every crate entity found, in slot order; empty when this truck has no supply bed
	protected array<IEntity> CollectCrates(notnull IEntity owner)
	{
		array<IEntity> crates = new array<IEntity>();

		SlotManagerComponent truckSlots = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
		if (!truckSlots)
			return crates;

		array<EntitySlotInfo> slots = {};
		truckSlots.GetSlotInfos(slots);

		foreach (EntitySlotInfo slot : slots)
		{
			if (!slot)
				continue;

			IEntity bed = slot.GetAttachedEntity();
			if (!bed)
				continue;

			CollectCratesFromBed(bed, crates);
			if (crates.IsEmpty())
				continue;

			// Only the bed's own manager carries the flag; the cab's returns false, so the cab seats
			// are never touched.
			SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(bed.FindComponent(SCR_BaseCompartmentManagerComponent));
			if (compartments && compartments.BlockSuppliesIfOccupied())
				m_BedCompartments = compartments;

			return crates;
		}

		return crates;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] bed A candidate cargo bed entity
	//! \param[out] crates Filled with the bed's crate stacks, lowest slot number first
	protected void CollectCratesFromBed(notnull IEntity bed, notnull array<IEntity> crates)
	{
		SlotManagerComponent bedSlots = SlotManagerComponent.Cast(bed.FindComponent(SlotManagerComponent));
		if (!bedSlots)
			return;

		for (int i = 1; i <= MAX_CRATE_SLOTS; i++)
		{
			EntitySlotInfo slot = bedSlots.GetSlotByName(SlotName(i));
			if (!slot)
				continue;

			IEntity crate = slot.GetAttachedEntity();
			if (crate)
				crates.Insert(crate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] index 1-based slot number
	//! \return The vanilla slot name, zero-padded to two digits
	protected string SlotName(int index)
	{
		if (index < 10)
			return "SupplyStorage_0" + index.ToString();

		return "SupplyStorage_" + index.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the number of crates the current load earns and hides the rest.
	protected void Refresh()
	{
		if (!m_Store || !m_aCrates)
			return;

		int wanted = WantedCrateCount();
		if (wanted == m_iShown)
			return;

		for (int i = 0; i < m_aCrates.Count(); i++)
		{
			SetCrateVisible(m_aCrates[i], i < wanted);
		}

		if (m_BedCompartments)
			m_BedCompartments.SetCompartmentsAccessibleForAI(wanted == 0);

		m_iShown = wanted;
	}

	//------------------------------------------------------------------------------------------------
	//! Any load at all shows at least one crate, so a token delivery is still visible on the bed.
	//! \return How many crates the current load should show
	protected int WantedCrateCount()
	{
		int total = m_aCrates.Count();
		int used = m_Store.GetUsedLitres();
		if (used <= 0)
			return 0;

		int capacity = m_Store.GetCapacityLitres();
		if (capacity <= 0)
			return total;

		int wanted = Math.Ceil(used * total / (float)capacity);

		return Math.ClampInt(wanted, 1, total);
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors SCR_ResourceComponent.OnVisibilityChanged's flag half, minus the simulation change.
	//! \param[in] crate One crate stack entity
	//! \param[in] visible True to show it
	protected void SetCrateVisible(IEntity crate, bool visible)
	{
		if (!crate)
			return;

		if (visible)
		{
			crate.SetFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE, true);
			return;
		}

		crate.ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE, true);
	}
}
