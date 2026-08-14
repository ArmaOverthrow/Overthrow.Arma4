//------------------------------------------------------------------------------------------------
//! What a swap did. Returned by OVT_LoadoutSwap.Swap() and read by the caller to decide what to tell
//! the player.
//!
//! FOUR COUNTERS, NOT ONE SUCCESS FLAG, because "it worked" is not a single fact here: a kit is a
//! dozen or more separate entities and each of them can land in a different place. Every item that
//! this routine touched is accounted for in exactly one of them, which is what makes the item-count
//! invariant (plan Q3) checkable from the log alone.
//!
//!   m_iExchanged  - reached the other character, in the matching slot. The normal outcome.
//!   m_iRelocated  - reached the other character, but NOT in the matching slot (it would not fit the
//!                   slot, so it went into a container instead). Counts as moved; forces "partial",
//!                   because the player will otherwise be hunting for a jacket that is in a rucksack.
//!   m_iFailed     - stayed on the character it started on. Nothing was lost; the swap is just
//!                   incomplete for that item.
//!   m_iDropped    - ended up on the ground under the character it started on. The designed worst
//!                   case (plan D13) and the only one that needs the player to bend down.
//------------------------------------------------------------------------------------------------
class OVT_LoadoutSwapResult : Managed
{
	//! Whether the routine got past its guards and actually walked the two kits. False means nothing
	//! at all was touched, and all four counters are zero.
	bool m_bRan;

	//! Items that reached the other character in the matching slot.
	int m_iExchanged;

	//! Items that reached the other character but not in the matching slot.
	int m_iRelocated;

	//! Items that could not be exchanged and stayed where they were.
	int m_iFailed;

	//! Items that are on the ground.
	int m_iDropped;

	//------------------------------------------------------------------------------------------------
	//! Everything that changed hands, however it got there.
	int GetMovedCount()
	{
		return m_iExchanged + m_iRelocated;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether every item this routine touched ended up in the matching slot on the other character.
	//!
	//! Two naked characters swapping nothing is COMPLETE, not failed: the routine ran, and afterwards
	//! each of them is wearing exactly what the other was.
	bool IsComplete()
	{
		if (!m_bRan) return false;

		return m_iRelocated == 0 && m_iFailed == 0 && m_iDropped == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether something changed hands but not everything did.
	bool IsPartial()
	{
		if (!m_bRan) return false;
		if (IsComplete()) return false;

		return GetMovedCount() > 0;
	}
}

//------------------------------------------------------------------------------------------------
//! One entry in the rollback journal: an item that HAS ALREADY BEEN MOVED, and everything needed to
//! put it back.
//!
//! Entries are appended only AFTER the move that they describe succeeded, so at rollback time an
//! entry's item is in exactly one of two states, and which one decides who is allowed to move it:
//!
//!   - it has a parent slot  -> it is sitting in the OTHER character's storage, so m_AwayManager is
//!     the manager that owns that storage and therefore the only one allowed to take it out again
//!     (see the instigator rule in the OVT_LoadoutSwap header).
//!   - it has no parent slot -> it was detached and is lying in the world, so there is no removal to
//!     authorise and m_HomeManager can simply take it back.
//------------------------------------------------------------------------------------------------
class OVT_LoadoutSwapStep : Managed
{
	//! The item that was moved.
	IEntity m_Item;

	//! The storage it came out of.
	BaseInventoryStorageComponent m_HomeStorage;

	//! The slot id it came out of, or -1 when it had none.
	int m_iHomeSlotId;

	//! The storage manager that owns m_HomeStorage.
	InventoryStorageManagerComponent m_HomeManager;

	//! The storage manager on the other side of the exchange.
	InventoryStorageManagerComponent m_AwayManager;
}

//------------------------------------------------------------------------------------------------
//! One slot class to exchange: what character A has there, what character B has there, and where
//! each side's slot is.
//!
//! "Slot class" is the pairing model, and it is deliberately structural rather than item-based: two
//! items are a MATCHED PAIR when they occupy the same NAMED PLACE on their respective characters -
//! the same loadout area (jacket against jacket) or the same weapon slot index (shoulder against
//! shoulder). Nothing about the items themselves is compared, so a pistol never tries to pair with a
//! rifle and a helmet never tries to pair with a backpack.
//!
//! Either item may be null; both null is not a unit at all and is never built.
//------------------------------------------------------------------------------------------------
class OVT_LoadoutSwapUnit : Managed
{
	//! Human-readable name of the slot class, for the WARNING log only.
	string m_sLabel;

	//! What character A has in this slot class, or null.
	IEntity m_ItemA;

	//! What character B has in this slot class, or null.
	IEntity m_ItemB;

	//! Character A's storage for this slot class. Never null in a built unit.
	BaseInventoryStorageComponent m_StorageA;

	//! Character A's slot id within m_StorageA, or -1 for "wherever it fits".
	int m_iSlotA;

	//! Character B's storage for this slot class. Never null in a built unit.
	BaseInventoryStorageComponent m_StorageB;

	//! Character B's slot id within m_StorageB, or -1 for "wherever it fits".
	int m_iSlotB;
}

//------------------------------------------------------------------------------------------------
//! Exchanges two characters' entire kit by MOVING THE REAL ITEM ENTITIES between their inventories.
//!
//! ! THE SAFETY PROPERTY IS STRUCTURAL, NOT PROCEDURAL (plan decision D13). This routine has no way
//! to create an item and no way to destroy one: it only ever asks the engine to move entities that
//! already exist. That is not a rule the code follows carefully, it is a rule the code cannot break,
//! and it is enforced from outside by the grep proof in the plan's Q9 - the four engine identifiers
//! that could create or destroy an item must never appear in this file. The worst outcome reachable
//! from here is therefore an item lying on the ground under one of the two characters. A missing or
//! duplicated item is not reachable at all.
//!
//! WHY IT DOES NOT BUILD ON THE LOADOUT CAPTURE/APPLY ENGINE. That path describes a kit as a list of
//! prefab names and then rebuilds it, which means a swap through it would be "destroy 30 items,
//! create 30 items" - and the items are not interchangeable (a magazine is half empty, a weapon has
//! attachments, a rucksack has somebody's loot in it). Moving the entities keeps all of that by
//! construction and needs no equivalence rules at all.
//!
//! ---------------------------------------------------------------------------------------------
//! THE INSTIGATOR RULE, which is the single most important thing in this file
//! ---------------------------------------------------------------------------------------------
//!
//! SCR_InventoryStorageManagerComponent.ShouldForbidRemoveByInstigator (:555) refuses to let one
//! character's storage manager REMOVE an item from a LIVING character's storage - that is vanilla's
//! "you cannot loot someone who is still alive" rule, and the only exemption is consumables (which is
//! why a medic can push a tourniquet into a patient: SCR_TourniquetStorageComponent.AddTourniquetToSlot
//! removes from the MEDIC's own storage and only INSERTS into the patient's).
//!
//! Both characters in a loadout swap are alive, so every single move here would silently return false
//! if it were instigated by the wrong manager. The rule this file follows everywhere, without
//! exception:
//!
//!   A REMOVAL IS ALWAYS INSTIGATED BY THE MANAGER THAT OWNS THE STORAGE BEING EMPTIED.
//!   AN INSERTION MAY BE INSTIGATED BY EITHER, BECAUSE NOTHING IS BEING REMOVED.
//!
//! So: character A's item is moved out of A by A's manager (into B's storage - the insert half is not
//! gated), and character B's item is moved out of B by B's manager. An item lying unparented in the
//! world belongs to nobody and can be inserted by whichever manager owns the destination.
//!
//! This is also why TrySwapItemStorages, which necessarily removes from BOTH sides at once, is tried
//! but never relied on: whichever manager is asked, one of its two removals is a cross-character one.
//! It is attempted first because when it IS permitted it is the only genuinely atomic primitive
//! available, and the three-step fallback behind it needs no cooperation from the rule at all.
//!
//! ---------------------------------------------------------------------------------------------
//! THE ALGORITHM
//! ---------------------------------------------------------------------------------------------
//!
//!  1. ENUMERATE BOTH SIDES INTO ARRAYS, THEN MUTATE. Never walk a storage while changing it - the
//!     in-repo pattern is OVT_LoadoutManagerComponent.EmptyStorageManagerIntoBox (:752-768), and the
//!     reason is that every move here changes the slot contents of the thing being iterated.
//!  2. MATCHED PAIRS FIRST, one slot class at a time, outermost clothing first (see
//!     BuildClothingAreaOrder for why the order is load-bearing).
//!  3. UNMATCHED SINGLES are a plain move into the counterpart's slot, retried without a slot id,
//!     mirroring OVT_InventoryManagerComponent.TransferItemWithFallback (:310-325).
//!  4. EVERY COMPLETED MOVE IS JOURNALLED. A step that cannot finish rewinds only its own moves and
//!     the routine carries on with the next slot class; one garment that will not fit does not cost
//!     the player the other eleven.
//!  5. IF EVEN THE ROLLBACK FAILS the item is detached to the world under the character. That is the
//!     end of the fallback chain, and it is recoverable by bending down.
//!
//! ---------------------------------------------------------------------------------------------
//! WHAT IS DELIBERATELY NOT SWAPPED
//! ---------------------------------------------------------------------------------------------
//!
//!  - THE IDENTITY, SALINE AND TOURNIQUET STORAGES (Character_Base.et:99-131). Dog tags are identity,
//!    and an applied tourniquet or saline bag is a treatment on a specific limb of a specific person.
//!    They are excluded for free: the clothing pass is driven by loadout AREAS, and none of those
//!    storages has one.
//!  - THE EQUIPMENT STORAGE (binoculars, wristwatch - Character_Base.et:68). Same free exclusion, and
//!    left out on purpose: it is a sibling component of the medical storages above and the value of
//!    swapping a wristwatch does not justify going anywhere near them.
//!  - THE HAND SLOT AND THE HAND WEAPON SLOT. Putting something into the other character's hands
//!    means changing weapon selection, and BaseWeaponManagerComponent.SelectWeapon is explicitly NOT
//!    synchronised (BaseWeaponManagerComponent.c:35), so a server-side selection would be a lie on
//!    every client. A weapon HELD from a shoulder slot still swaps - it lives in that shoulder slot
//!    and is merely selected - and both characters simply end up with nothing selected, which is the
//!    state the plan asks for.
//!  - CONTAINER CONTENTS, INDIVIDUALLY. A rucksack, vest or jacket is moved as one entity and
//!    everything inside it travels with it. Walking the contents as well would be strictly more risk
//!    for exactly zero extra coverage.
//!
//! SERVER ONLY. Inventory operations are server-authoritative in this project; the guard and its
//! message shape are OVT_InventoryManagerComponent.TransferStorage's (:130-136).
//!
//! Not reachable by the automated spine: it needs two live characters with real kit in a real world,
//! which the test world does not have. Its play-test is the item-count invariant (plan Q3, step 9).
//------------------------------------------------------------------------------------------------
class OVT_LoadoutSwap : Managed
{
	//! Character A, the side whose items are moved by m_ManagerA.
	protected IEntity m_CharacterA;

	//! Character B.
	protected IEntity m_CharacterB;

	//! A's storage manager. Base type on purpose: the concrete
	//! SCR_InventoryStorageManagerComponent adds script-level gates (IsAnimationReady(),
	//! IsInventoryLocked()) that describe a PLAYER OPERATING A MENU, which is not what this is. The
	//! base-class methods are the engine ones and carry no such gate.
	protected InventoryStorageManagerComponent m_ManagerA;

	//! B's storage manager.
	protected InventoryStorageManagerComponent m_ManagerB;

	//! A's worn-clothing storage, as a storage.
	protected BaseInventoryStorageComponent m_LoadoutStorageA;

	//! B's worn-clothing storage, as a storage.
	protected BaseInventoryStorageComponent m_LoadoutStorageB;

	//! A's worn-clothing storage, as a loadout storage (for the per-area queries).
	protected EquipedLoadoutStorageComponent m_LoadoutA;

	//! B's worn-clothing storage, as a loadout storage.
	protected EquipedLoadoutStorageComponent m_LoadoutB;

	//! Moves already made, newest last. See OVT_LoadoutSwapStep.
	protected ref array<ref OVT_LoadoutSwapStep> m_aJournal;

	//! The tally handed back to the caller.
	protected ref OVT_LoadoutSwapResult m_Result;

	//------------------------------------------------------------------------------------------------
	//! Exchange two characters' kit. THE ONLY ENTRY POINT.
	//!
	//! Static so callers cannot hold a half-finished swap, and internally an instance so the run's
	//! state (two managers, the journal, the tally) does not have to be threaded through a dozen
	//! parameters. A swap is a single synchronous operation - there is nothing to keep afterwards.
	//! \param[in] a One character. Order does not matter; the routine is symmetric.
	//! \param[in] b The other character.
	//! \return What happened. Never null; m_bRan false means nothing was touched.
	static OVT_LoadoutSwapResult Swap(IEntity a, IEntity b)
	{
		OVT_LoadoutSwap run = new OVT_LoadoutSwap();

		return run.Run(a, b);
	}

	//------------------------------------------------------------------------------------------------
	//! The run itself. See the class header for the algorithm.
	//! \param[in] a One character.
	//! \param[in] b The other character.
	//! \return What happened.
	protected OVT_LoadoutSwapResult Run(IEntity a, IEntity b)
	{
		m_Result = new OVT_LoadoutSwapResult();

		// SERVER-SIDE ONLY: all inventory operations must happen on server
		if (!Replication.IsServer())
		{
			Print("[Overthrow] OVT_LoadoutSwap: Attempted to run on client - inventory operations are server-side only!", LogLevel.WARNING);
			return m_Result;
		}

		if (!a || !b)
		{
			Print("[Overthrow] OVT_LoadoutSwap: Refused - one of the two characters does not exist", LogLevel.WARNING);
			return m_Result;
		}

		// Swapping a character with itself would pair every item with itself, and the three-step path
		// would then detach an item and try to put it back into the slot it is being moved out of.
		if (a == b)
		{
			Print("[Overthrow] OVT_LoadoutSwap: Refused - both sides are the same character", LogLevel.WARNING);
			return m_Result;
		}

		if (!Resolve(a, b))
			return m_Result;

		m_aJournal = new array<ref OVT_LoadoutSwapStep>();

		// Step 1 of the algorithm: everything is read BEFORE anything is moved.
		array<ref OVT_LoadoutSwapUnit> units = new array<ref OVT_LoadoutSwapUnit>();
		CollectClothingUnits(units);
		CollectWeaponUnits(units);
		CollectCarriedUnits(units);

		m_Result.m_bRan = true;

		foreach (OVT_LoadoutSwapUnit unit : units)
		{
			if (!unit)
				continue;

			ExecuteUnit(unit);
		}

		RebuildQuickSlots(m_CharacterA);
		RebuildQuickSlots(m_CharacterB);

		Print(string.Format("[Overthrow] Loadout swap finished: %1 exchanged, %2 relocated, %3 failed, %4 dropped",
			m_Result.m_iExchanged, m_Result.m_iRelocated, m_Result.m_iFailed, m_Result.m_iDropped), LogLevel.NORMAL);

		return m_Result;
	}

	//-----------------------------------------------------------------------
	// RESOLUTION
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Look up everything the run needs from both characters, up front.
	//!
	//! All-or-nothing: a character with no storage manager or no loadout storage is not something this
	//! routine could half-swap safely, so it refuses before touching anything.
	//! \param[in] a One character.
	//! \param[in] b The other character.
	//! \return True when both sides are usable.
	protected bool Resolve(notnull IEntity a, notnull IEntity b)
	{
		m_CharacterA = a;
		m_CharacterB = b;

		m_ManagerA = InventoryStorageManagerComponent.Cast(a.FindComponent(InventoryStorageManagerComponent));
		m_ManagerB = InventoryStorageManagerComponent.Cast(b.FindComponent(InventoryStorageManagerComponent));

		if (!m_ManagerA || !m_ManagerB)
		{
			Print("[Overthrow] OVT_LoadoutSwap: Refused - one of the two characters has no inventory manager", LogLevel.WARNING);
			return false;
		}

		// Base type on purpose: a character's concrete storage is
		// SCR_CharacterInventoryStorageComponent, which inherits from this one. Same resolution as
		// OVT_SellableItemScanner.IsWornCloth (:249).
		m_LoadoutA = EquipedLoadoutStorageComponent.Cast(a.FindComponent(EquipedLoadoutStorageComponent));
		m_LoadoutB = EquipedLoadoutStorageComponent.Cast(b.FindComponent(EquipedLoadoutStorageComponent));

		if (!m_LoadoutA || !m_LoadoutB)
		{
			Print("[Overthrow] OVT_LoadoutSwap: Refused - one of the two characters has no loadout storage", LogLevel.WARNING);
			return false;
		}

		m_LoadoutStorageA = BaseInventoryStorageComponent.Cast(m_LoadoutA);
		m_LoadoutStorageB = BaseInventoryStorageComponent.Cast(m_LoadoutB);

		if (!m_LoadoutStorageA || !m_LoadoutStorageB)
		{
			Print("[Overthrow] OVT_LoadoutSwap: Refused - a loadout storage is not an inventory storage", LogLevel.WARNING);
			return false;
		}

		return true;
	}

	//-----------------------------------------------------------------------
	// ENUMERATION
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The wearable loadout areas, OUTERMOST FIRST, which is the order the clothing pass uses.
	//!
	//! ORDER IS LOAD-BEARING, and this is risk R6. A garment can BLOCK another area while it is worn:
	//! BaseLoadoutClothComponent.GetBlockedSlots feeds SCR_CharacterInventoryStorageComponent's
	//! m_aBlockedSlots (:698-725) and IsAreaBlocked (:730) then refuses that area. A plate carrier
	//! blocking the jacket area is the everyday case. Taking the outer layers off first means that by
	//! the time an inner area is exchanged, whatever was blocking it has already left the character -
	//! the same order a person would use.
	//!
	//! LoadoutHandwearSlotArea, not "LoadoutHandwearArea". The area typename list in
	//! OVT_InventoryManagerComponent.LootBodyItems (:812-820) compares against the string
	//! "LoadoutHandwearArea", which is not a class that exists (scripts/Game/generated/Loadout/), so
	//! that one entry has never matched anything. Types are used here rather than strings precisely so
	//! that kind of typo cannot survive the compiler.
	//! \param[out] areas Receives the area typenames in exchange order.
	protected void BuildClothingAreaOrder(out array<typename> areas)
	{
		areas.Insert(LoadoutBackpackArea);
		areas.Insert(LoadoutVestArea);
		areas.Insert(LoadoutArmoredVestSlotArea);
		areas.Insert(LoadoutJacketArea);
		areas.Insert(LoadoutPantsArea);
		areas.Insert(LoadoutBootsArea);
		areas.Insert(LoadoutHeadCoverArea);
		areas.Insert(LoadoutHandwearSlotArea);
	}

	//------------------------------------------------------------------------------------------------
	//! One unit per wearable area that either character has something in.
	//!
	//! GetClothFromArea is vanilla's own "what is actually worn in this area" query
	//! (SCR_CharacterInventoryStorageComponent.c:800-805 uses it for exactly this question), which is
	//! what makes it safe against the BUG-083 trap: a spare uniform in a rucksack has the same
	//! BaseLoadoutClothComponent and the same area type as the worn one, and this returns only the
	//! entity actually attached to the loadout slot. GetItems(PURPOSE_DEPOSIT) is not used anywhere in
	//! this file for the same reason - the character's loadout storage carries the DEPOSIT bit
	//! (Character_Base.et:179, StoragePurpose 0x9) and so a DEPOSIT query answers with worn gear.
	//! \param[out] units Receives one unit per occupied area.
	protected void CollectClothingUnits(notnull array<ref OVT_LoadoutSwapUnit> units)
	{
		array<typename> areas = new array<typename>();
		BuildClothingAreaOrder(areas);

		foreach (typename area : areas)
		{
			IEntity itemA = m_LoadoutA.GetClothFromArea(area);
			IEntity itemB = m_LoadoutB.GetClothFromArea(area);

			if (!itemA && !itemB)
				continue;

			OVT_LoadoutSwapUnit unit = new OVT_LoadoutSwapUnit();
			unit.m_sLabel = area.ToString();
			unit.m_ItemA = itemA;
			unit.m_ItemB = itemB;

			// A character that does not have the area at all still gets a destination: its loadout
			// storage with "wherever it fits", which is how vanilla equips clothing anyway.
			unit.m_StorageA = m_LoadoutStorageA;
			unit.m_iSlotA = -1;

			LoadoutSlotInfo slotA = m_LoadoutA.GetSlotFromArea(area);
			if (slotA && slotA.GetStorage())
			{
				unit.m_StorageA = slotA.GetStorage();
				unit.m_iSlotA = slotA.GetID();
			}

			unit.m_StorageB = m_LoadoutStorageB;
			unit.m_iSlotB = -1;

			LoadoutSlotInfo slotB = m_LoadoutB.GetSlotFromArea(area);
			if (slotB && slotB.GetStorage())
			{
				unit.m_StorageB = slotB.GetStorage();
				unit.m_iSlotB = slotB.GetID();
			}

			units.Insert(unit);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One unit per weapon slot index that either character has a weapon in.
	//!
	//! WEAPON SLOTS, NOT THE WEAPON IN HANDS. Reading only the selected weapon loses everything slung
	//! and holstered, which is BUG-044; the slot walk is the same one
	//! OVT_LoadoutManagerComponent.ExtractEquippedItems (:1075-1079) and
	//! OVT_RecruitManagerComponent.ReadRecruitStatus already use.
	//!
	//! GetWeaponSlotIndex() is both the pairing key and the destination slot id - vanilla passes it
	//! straight to TryInsertItemInStorage against the weapon storage (SCR_EquipWeaponAction.c:143), so
	//! shoulder pairs with shoulder and holster with holster on any two characters built from
	//! Character_Base.
	//!
	//! CharacterHandWeaponSlotComponent is skipped: see "what is deliberately not swapped" in the
	//! class header.
	//! \param[out] units Receives one unit per occupied weapon slot.
	protected void CollectWeaponUnits(notnull array<ref OVT_LoadoutSwapUnit> units)
	{
		BaseWeaponManagerComponent weaponsA = BaseWeaponManagerComponent.Cast(m_CharacterA.FindComponent(BaseWeaponManagerComponent));
		BaseWeaponManagerComponent weaponsB = BaseWeaponManagerComponent.Cast(m_CharacterB.FindComponent(BaseWeaponManagerComponent));

		if (!weaponsA || !weaponsB)
			return;

		// Same lookup as SCR_CharacterInventoryStorageComponent.GetWeaponStorage (:159-165).
		BaseInventoryStorageComponent storageA = BaseInventoryStorageComponent.Cast(m_CharacterA.FindComponent(EquipedWeaponStorageComponent));
		BaseInventoryStorageComponent storageB = BaseInventoryStorageComponent.Cast(m_CharacterB.FindComponent(EquipedWeaponStorageComponent));

		if (!storageA || !storageB)
			return;

		array<WeaponSlotComponent> slotsA = new array<WeaponSlotComponent>();
		weaponsA.GetWeaponsSlots(slotsA);

		array<WeaponSlotComponent> slotsB = new array<WeaponSlotComponent>();
		weaponsB.GetWeaponsSlots(slotsB);

		array<int> paired = new array<int>();

		foreach (WeaponSlotComponent slotA : slotsA)
		{
			if (!slotA)
				continue;

			if (CharacterHandWeaponSlotComponent.Cast(slotA))
				continue;

			int index = slotA.GetWeaponSlotIndex();
			WeaponSlotComponent slotB = FindWeaponSlot(slotsB, index);

			IEntity itemA = slotA.GetWeaponEntity();

			IEntity itemB;
			if (slotB)
				itemB = slotB.GetWeaponEntity();

			paired.Insert(index);

			if (!itemA && !itemB)
				continue;

			OVT_LoadoutSwapUnit unit = new OVT_LoadoutSwapUnit();
			unit.m_sLabel = "WeaponSlot" + index.ToString();
			unit.m_ItemA = itemA;
			unit.m_ItemB = itemB;
			unit.m_StorageA = storageA;
			unit.m_iSlotA = index;
			unit.m_StorageB = storageB;
			unit.m_iSlotB = -1;

			if (slotB)
				unit.m_iSlotB = index;

			units.Insert(unit);
		}

		// Slot indices B has and A does not: still worth moving, just with no matching destination.
		foreach (WeaponSlotComponent slotB : slotsB)
		{
			if (!slotB)
				continue;

			if (CharacterHandWeaponSlotComponent.Cast(slotB))
				continue;

			int index = slotB.GetWeaponSlotIndex();
			if (paired.Contains(index))
				continue;

			IEntity itemB = slotB.GetWeaponEntity();
			if (!itemB)
				continue;

			OVT_LoadoutSwapUnit unit = new OVT_LoadoutSwapUnit();
			unit.m_sLabel = "WeaponSlot" + index.ToString();
			unit.m_ItemB = itemB;
			unit.m_StorageA = storageA;
			unit.m_iSlotA = -1;
			unit.m_StorageB = storageB;
			unit.m_iSlotB = index;

			units.Insert(unit);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The weapon slot with a given index, or null.
	//! \param[in] slots The slots to search.
	//! \param[in] index The weapon slot index to find.
	//! \return The slot, or null.
	protected WeaponSlotComponent FindWeaponSlot(notnull array<WeaponSlotComponent> slots, int index)
	{
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot)
				continue;

			if (slot.GetWeaponSlotIndex() == index)
				return slot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Items sitting in a container that belongs to the CHARACTER ITSELF rather than to a garment.
	//!
	//! ON A STOCK REFORGER CHARACTER THIS FINDS NOTHING, and that is the point of writing it out
	//! rather than assuming it. Every carried item lives inside a rucksack, vest or pocket - i.e.
	//! inside a garment - and a garment is moved as one entity with its contents inside it, so those
	//! items are exchanged for free by the clothing pass. This pass exists for the case the clothing
	//! pass genuinely cannot reach: a container component mounted directly on the character by a mod
	//! or by a future Overthrow prefab. Without it such items would silently stay behind.
	//!
	//! Three exclusions, each for a different reason:
	//!  - storages owned by another entity are a garment's or a weapon's own storage, and travel with
	//!    their owner.
	//!  - non-universal storages are the loadout, weapon, identity, medical and equipment storages,
	//!    all of which are handled (or deliberately excluded) elsewhere.
	//!  - the hand slot storage is universal-derived and holds what the character is HOLDING, which is
	//!    left alone (see the class header).
	//! \param[out] units Receives one single-sided unit per loose item.
	protected void CollectCarriedUnits(notnull array<ref OVT_LoadoutSwapUnit> units)
	{
		CollectCarriedUnitsFrom(units, m_CharacterA, m_ManagerA, true);
		CollectCarriedUnitsFrom(units, m_CharacterB, m_ManagerB, false);
	}

	//------------------------------------------------------------------------------------------------
	//! One side of CollectCarriedUnits.
	//! \param[out] units Receives the units.
	//! \param[in] character The character to walk.
	//! \param[in] manager That character's storage manager.
	//! \param[in] isSideA True when this is character A.
	protected void CollectCarriedUnitsFrom(notnull array<ref OVT_LoadoutSwapUnit> units, notnull IEntity character, notnull InventoryStorageManagerComponent manager, bool isSideA)
	{
		array<BaseInventoryStorageComponent> storages = new array<BaseInventoryStorageComponent>();
		manager.GetStorages(storages, EStoragePurpose.PURPOSE_ANY);

		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (!storage)
				continue;

			if (storage.GetOwner() != character)
				continue;

			if (!BaseUniversalInventoryStorageComponent.Cast(storage))
				continue;

			if (SCR_HandSlotStorageComponent.Cast(storage))
				continue;

			// Collected first, moved later: this storage is about to be emptied.
			array<IEntity> loose = new array<IEntity>();

			for (int slotIndex = 0, slotsCount = storage.GetSlotsCount(); slotIndex < slotsCount; slotIndex++)
			{
				IEntity item = storage.Get(slotIndex);
				if (!item)
					continue;

				loose.Insert(item);
			}

			foreach (IEntity item : loose)
			{
				OVT_LoadoutSwapUnit unit = new OVT_LoadoutSwapUnit();
				unit.m_sLabel = "Carried";
				unit.m_StorageA = m_LoadoutStorageA;
				unit.m_iSlotA = -1;
				unit.m_StorageB = m_LoadoutStorageB;
				unit.m_iSlotB = -1;

				// A loose item has no counterpart slot, so it is always an unmatched single and the
				// destination is resolved at move time from the recipient's own storages.
				if (isSideA)
				{
					unit.m_ItemA = item;
					unit.m_StorageA = storage;
					unit.m_iSlotA = -1;
					unit.m_StorageB = null;
				}
				else
				{
					unit.m_ItemB = item;
					unit.m_StorageB = storage;
					unit.m_iSlotB = -1;
					unit.m_StorageA = null;
				}

				units.Insert(unit);
			}
		}
	}

	//-----------------------------------------------------------------------
	// EXECUTION
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Exchange one slot class.
	//! \param[in] unit The slot class to exchange.
	protected void ExecuteUnit(notnull OVT_LoadoutSwapUnit unit)
	{
		if (unit.m_ItemA && unit.m_ItemB)
		{
			ExchangePair(unit);
			return;
		}

		if (unit.m_ItemA)
		{
			MoveSingle(unit, unit.m_ItemA, m_ManagerA, m_ManagerB, unit.m_StorageB, unit.m_iSlotB);
			return;
		}

		if (unit.m_ItemB)
			MoveSingle(unit, unit.m_ItemB, m_ManagerB, m_ManagerA, unit.m_StorageA, unit.m_iSlotA);
	}

	//------------------------------------------------------------------------------------------------
	//! Both sides occupied: exchange the two items.
	//!
	//! Two paths, in order of preference:
	//!
	//!  1. TrySwapItemStorages, the one primitive that does this in a single engine call and is
	//!     therefore atomic as far as script is concerned. Asked of both managers because either one
	//!     may be the one the instigator rule permits (class header). CanSwapItemStorages is checked
	//!     first so the common refusal costs a query rather than an attempt.
	//!  2. A three-step exchange in which every removal is instigated by the manager that owns the
	//!     storage being emptied, which needs no permission from the instigator rule at all:
	//!       a. take A's item out of its slot - it is now unparented, and journalled;
	//!       b. move B's item into the slot A's item just left, using B's manager;
	//!       c. put A's item into the slot B's item just left, using B's manager (it is unparented, so
	//!          this is an insert and either manager may do it).
	//!     Failure at (a) changes nothing. Failure at (b) rewinds (a). Failure at (c) walks a fallback
	//!     chain - B's containers, then back onto A, then the ground - and does NOT rewind (b),
	//!     because B's item has already crossed successfully and undoing a good move to tidy up a bad
	//!     one is more moves, more failure surface, and no better a state.
	//! \param[in] unit The slot class, with both items present.
	protected void ExchangePair(notnull OVT_LoadoutSwapUnit unit)
	{
		IEntity itemA = unit.m_ItemA;
		IEntity itemB = unit.m_ItemB;

		if (TryAtomicSwap(itemA, itemB))
		{
			m_Result.m_iExchanged += 2;
			return;
		}

		BaseInventoryStorageComponent homeA = ResolveParentStorage(itemA, unit.m_StorageA);
		int homeSlotA = ResolveParentSlotId(itemA, unit.m_iSlotA);

		BaseInventoryStorageComponent homeB = ResolveParentStorage(itemB, unit.m_StorageB);
		int homeSlotB = ResolveParentSlotId(itemB, unit.m_iSlotB);

		if (!homeA || !homeB)
		{
			m_Result.m_iFailed += 2;
			LogItem(unit, itemA, "could not be exchanged: a slot could not be resolved");
			return;
		}

		int mark = m_aJournal.Count();

		// (a) A's item leaves its slot. Removal from A's storage, instigated by A's manager.
		if (!m_ManagerA.TryRemoveItemFromStorage(itemA, homeA))
		{
			m_Result.m_iFailed += 2;
			LogItem(unit, itemA, "could not be taken out of its slot");
			return;
		}

		Journal(itemA, homeA, homeSlotA, m_ManagerA, m_ManagerB);

		// (b) B's item takes its place. Removal from B's storage, instigated by B's manager.
		if (!MoveInto(m_ManagerB, itemB, homeA, homeSlotA))
		{
			LogItem(unit, itemB, "would not fit the other character");

			// The counters are a PARTITION: an item the rewind could not put back is already counted
			// as dropped, so only the ones that really did stay put are counted as failed. itemB never
			// moved and is always one of them.
			int dropped = Rewind(mark);
			m_Result.m_iFailed += 2 - dropped;
			return;
		}

		Journal(itemB, homeB, homeSlotB, m_ManagerB, m_ManagerA);

		// (c) A's item is unparented, so this is an insert and needs no removal permission.
		if (m_ManagerB.TryInsertItemInStorage(itemA, homeB, homeSlotB))
		{
			m_Result.m_iExchanged += 2;
			return;
		}

		if (homeSlotB != -1 && m_ManagerB.TryInsertItemInStorage(itemA, homeB, -1))
		{
			m_Result.m_iExchanged++;
			m_Result.m_iRelocated++;
			LogItem(unit, itemA, "did not fit its matching slot and went elsewhere on the recipient");
			return;
		}

		// R6's designed fallback: a garment the recipient cannot wear goes into their containers.
		if (m_ManagerB.TryInsertItem(itemA, EStoragePurpose.PURPOSE_ANY))
		{
			m_Result.m_iExchanged++;
			m_Result.m_iRelocated++;
			LogItem(unit, itemA, "could not be worn by the recipient and was stowed in their containers");
			return;
		}

		// It will not go onto the recipient at all. Give it back rather than leave it lying about.
		if (m_ManagerA.TryInsertItemInStorage(itemA, homeA, homeSlotA) || m_ManagerA.TryInsertItem(itemA, EStoragePurpose.PURPOSE_ANY))
		{
			m_Result.m_iExchanged++;
			m_Result.m_iFailed++;
			LogItem(unit, itemA, "could not be given to the recipient and was returned to its owner");
			return;
		}

		// End of the chain. It is already unparented, so it is already where it is going to be.
		m_Result.m_iExchanged++;
		m_Result.m_iDropped++;
		LogDrop(unit, itemA);
	}

	//------------------------------------------------------------------------------------------------
	//! Only one side occupied: move that item across.
	//!
	//! Nothing is journalled here: TryMoveItemToStorage either completes or leaves the item exactly
	//! where it was, so a failure has nothing to undo.
	//! \param[in] unit The slot class, for the log line.
	//! \param[in] item The item to move.
	//! \param[in] ownerManager The manager that owns the storage the item is in. THE INSTIGATOR RULE.
	//! \param[in] recipientManager The other character's manager.
	//! \param[in] destStorage Where it should go, or null for "wherever it fits on the recipient".
	//! \param[in] destSlotId The slot it should go into, or -1.
	protected void MoveSingle(notnull OVT_LoadoutSwapUnit unit, notnull IEntity item, notnull InventoryStorageManagerComponent ownerManager, notnull InventoryStorageManagerComponent recipientManager, BaseInventoryStorageComponent destStorage, int destSlotId)
	{
		BaseInventoryStorageComponent destination = destStorage;
		if (!destination)
			destination = recipientManager.FindStorageForItem(item, EStoragePurpose.PURPOSE_ANY);

		if (!destination)
		{
			m_Result.m_iFailed++;
			LogItem(unit, item, "could not be exchanged: the recipient has nowhere to put it");
			return;
		}

		if (ownerManager.TryMoveItemToStorage(item, destination, destSlotId))
		{
			m_Result.m_iExchanged++;
			return;
		}

		// The TransferItemWithFallback retry (OVT_InventoryManagerComponent.c:310-325): the named slot
		// is a preference, not a requirement.
		if (destSlotId != -1 && ownerManager.TryMoveItemToStorage(item, destination, -1))
		{
			m_Result.m_iRelocated++;
			LogItem(unit, item, "did not fit its matching slot and went elsewhere on the recipient");
			return;
		}

		BaseInventoryStorageComponent fallback = recipientManager.FindStorageForItem(item, EStoragePurpose.PURPOSE_ANY);
		if (fallback && fallback != destination && ownerManager.TryMoveItemToStorage(item, fallback, -1))
		{
			m_Result.m_iRelocated++;
			LogItem(unit, item, "could not be worn by the recipient and was stowed in their containers");
			return;
		}

		m_Result.m_iFailed++;
		LogItem(unit, item, "could not be moved to the other character");
	}

	//------------------------------------------------------------------------------------------------
	//! The atomic exchange, if the engine will allow it from either side.
	//!
	//! Both managers are asked because TrySwapItemStorages necessarily removes from BOTH characters,
	//! so whichever one is asked, one of the two removals is a cross-character one and may be refused
	//! (class header, the instigator rule). Asking the other costs one query and one attempt, and when
	//! it works it is the safest operation available here.
	//! \param[in] itemA Character A's item.
	//! \param[in] itemB Character B's item.
	//! \return True when the two items changed places.
	protected bool TryAtomicSwap(notnull IEntity itemA, notnull IEntity itemB)
	{
		if (m_ManagerA.CanSwapItemStorages(itemA, itemB) && m_ManagerA.TrySwapItemStorages(itemA, itemB))
			return true;

		if (m_ManagerB.CanSwapItemStorages(itemA, itemB) && m_ManagerB.TrySwapItemStorages(itemA, itemB))
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Move an item that is currently in a slot into another storage, retrying without the slot id.
	//! \param[in] ownerManager The manager that owns the storage the item is in.
	//! \param[in] item The item to move.
	//! \param[in] storage The destination storage.
	//! \param[in] slotId The preferred destination slot, or -1.
	//! \return True when it moved.
	protected bool MoveInto(notnull InventoryStorageManagerComponent ownerManager, notnull IEntity item, notnull BaseInventoryStorageComponent storage, int slotId)
	{
		if (ownerManager.TryMoveItemToStorage(item, storage, slotId))
			return true;

		if (slotId == -1)
			return false;

		return ownerManager.TryMoveItemToStorage(item, storage, -1);
	}

	//-----------------------------------------------------------------------
	// JOURNAL AND ROLLBACK
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Record a move that has already happened, so it can be undone.
	//! \param[in] item The item that moved.
	//! \param[in] homeStorage Where it came from.
	//! \param[in] homeSlotId The slot it came from, or -1.
	//! \param[in] homeManager The manager that owns homeStorage.
	//! \param[in] awayManager The manager on the other side of the exchange.
	protected void Journal(notnull IEntity item, notnull BaseInventoryStorageComponent homeStorage, int homeSlotId, notnull InventoryStorageManagerComponent homeManager, notnull InventoryStorageManagerComponent awayManager)
	{
		OVT_LoadoutSwapStep step = new OVT_LoadoutSwapStep();
		step.m_Item = item;
		step.m_HomeStorage = homeStorage;
		step.m_iHomeSlotId = homeSlotId;
		step.m_HomeManager = homeManager;
		step.m_AwayManager = awayManager;

		m_aJournal.Insert(step);
	}

	//------------------------------------------------------------------------------------------------
	//! Undo every journalled move from `mark` onwards, newest first.
	//!
	//! Newest first because the moves were ordered - the second one went into the space the first one
	//! made - so undoing them in the other order would ask an item to go back into a slot that is
	//! still occupied.
	//!
	//! WHICH MANAGER UNDOES A MOVE is decided per entry, from where the item is NOW, and it is the
	//! instigator rule again (class header): an item sitting in the other character's storage can only
	//! be taken out of it by that character's manager, while an item lying unparented in the world can
	//! be picked up by either.
	//! \param[in] mark The journal length to rewind back to.
	//! \return How many items the rewind could not put back and had to leave in the world. Already
	//!         counted in m_iDropped; returned so the caller does not also count them as failed.
	protected int Rewind(int mark)
	{
		int dropped = 0;

		for (int i = m_aJournal.Count() - 1; i >= mark; i--)
		{
			OVT_LoadoutSwapStep step = m_aJournal[i];
			if (!step || !step.m_Item || !step.m_HomeStorage)
				continue;

			bool restored = false;

			if (HasParentSlot(step.m_Item))
			{
				restored = MoveInto(step.m_AwayManager, step.m_Item, step.m_HomeStorage, step.m_iHomeSlotId);
			}
			else
			{
				restored = step.m_HomeManager.TryInsertItemInStorage(step.m_Item, step.m_HomeStorage, step.m_iHomeSlotId);

				if (!restored && step.m_iHomeSlotId != -1)
					restored = step.m_HomeManager.TryInsertItemInStorage(step.m_Item, step.m_HomeStorage, -1);

				if (!restored)
					restored = step.m_HomeManager.TryInsertItem(step.m_Item, EStoragePurpose.PURPOSE_ANY);
			}

			if (restored)
				continue;

			// The rollback itself failed. Detaching is the end of the chain and it is recoverable:
			// the item exists, it is in the world, and it is under the character it belongs to.
			DetachToWorld(step);
			m_Result.m_iDropped++;
			dropped++;
		}

		if (m_aJournal.Count() > mark)
			m_aJournal.Resize(mark);

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Last resort: leave the item lying in the world rather than half-placed.
	//!
	//! TryRemoveItemFromStorage with no new parent IS the world drop - the engine documents the
	//! manager going null on a storage as "item drop in world"
	//! (BaseInventoryStorageComponent.c:155-158), and it is how vanilla's own AI drops a weapon
	//! (SCR_AIDropWeapon.c:45). An item that is already unparented needs no call at all; it is already
	//! there.
	//! \param[in] step The journal entry whose rollback failed.
	protected void DetachToWorld(notnull OVT_LoadoutSwapStep step)
	{
		if (!step.m_Item)
			return;

		InventoryStorageSlot parent = GetParentSlot(step.m_Item);
		if (parent && parent.GetStorage())
		{
			BaseInventoryStorageComponent holding = parent.GetStorage();

			if (!step.m_AwayManager.TryRemoveItemFromStorage(step.m_Item, holding))
				step.m_HomeManager.TryRemoveItemFromStorage(step.m_Item, holding);
		}

		Print(string.Format("[Overthrow] Loadout swap left %1 on the ground: it could not be put back and could not be given away",
			OVT_Global.GetPrefabName(step.m_Item)), LogLevel.WARNING);
	}

	//-----------------------------------------------------------------------
	// SMALL HELPERS
	//-----------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The slot an item is currently attached to, or null when it is loose in the world.
	//! \param[in] item The item to ask about.
	//! \return Its parent slot, or null.
	protected InventoryStorageSlot GetParentSlot(IEntity item)
	{
		if (!item)
			return null;

		InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComp)
			return null;

		return itemComp.GetParentSlot();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an item is attached to something rather than lying in the world.
	//! \param[in] item The item to ask about.
	//! \return True when it has a parent slot.
	protected bool HasParentSlot(IEntity item)
	{
		return GetParentSlot(item) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Where an item actually is, preferring its live parent slot over the slot class it was found in.
	//!
	//! The two normally agree. They can differ when a garment sits in a slot that is not the one its
	//! area query reported, and the live answer is the one a rollback has to use.
	//! \param[in] item The item.
	//! \param[in] fallback The unit's storage for that side.
	//! \return The storage the item is in, or the fallback.
	protected BaseInventoryStorageComponent ResolveParentStorage(IEntity item, BaseInventoryStorageComponent fallback)
	{
		InventoryStorageSlot slot = GetParentSlot(item);
		if (slot && slot.GetStorage())
			return slot.GetStorage();

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! The slot id an item actually occupies, or the unit's if it has none.
	//! \param[in] item The item.
	//! \param[in] fallback The unit's slot id for that side.
	//! \return The live slot id, or the fallback.
	protected int ResolveParentSlotId(IEntity item, int fallback)
	{
		InventoryStorageSlot slot = GetParentSlot(item);
		if (slot && slot.GetStorage())
			return slot.GetID();

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Put every carried weapon back into a quick slot on a character.
	//!
	//! QUICK SLOT STATE IS PER CHARACTER AND DOES NOT FOLLOW THE ITEM
	//! (SCR_CharacterInventoryStorageComponent.c:378, and :768-773 clears the slot when an item leaves
	//! an inventory), so after a swap both characters have holes where their old weapons were. Vanilla
	//! refills them on the machine that CONTROLS the character, through the m_OnItemAddedInvoker
	//! subscription made in InitAsPlayer (:1374) - which means a remote player's client fixes itself
	//! and this call is the belt to that braces, correct on a listen host and harmless everywhere
	//! else. StoreItemToQuickSlot is idempotent: an item already in a slot keeps it.
	//! \param[in] character The character to refill.
	protected void RebuildQuickSlots(IEntity character)
	{
		if (!character)
			return;

		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!storage)
			return;

		BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!weapons)
			return;

		array<WeaponSlotComponent> slots = new array<WeaponSlotComponent>();
		weapons.GetWeaponsSlots(slots);

		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot)
				continue;

			IEntity weapon = slot.GetWeaponEntity();
			if (!weapon)
				continue;

			storage.StoreItemToQuickSlot(weapon);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Log one item that did not do what was asked of it.
	//!
	//! ALWAYS WARNING, ALWAYS WITH THE PREFAB NAME (task T7.7). A partial swap that says nothing is
	//! the one outcome nobody can diagnose afterwards: the player reports "some of my gear did not
	//! swap" and there is no way to find out which item or why. The slot class is in the line too, so
	//! a blocked-area failure (risk R6) reads as a run of failures in one area rather than as noise.
	//! \param[in] unit The slot class being exchanged.
	//! \param[in] item The item concerned.
	//! \param[in] what What happened to it, in plain words.
	protected void LogItem(notnull OVT_LoadoutSwapUnit unit, IEntity item, string what)
	{
		Print(string.Format("[Overthrow] Loadout swap [%1]: %2 %3", unit.m_sLabel, OVT_Global.GetPrefabName(item), what), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Log one item that ended up on the ground.
	//! \param[in] unit The slot class being exchanged.
	//! \param[in] item The item concerned.
	protected void LogDrop(notnull OVT_LoadoutSwapUnit unit, IEntity item)
	{
		Print(string.Format("[Overthrow] Loadout swap [%1]: %2 is on the ground - neither character would take it", unit.m_sLabel, OVT_Global.GetPrefabName(item)), LogLevel.WARNING);
	}
}
