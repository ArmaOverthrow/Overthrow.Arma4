//------------------------------------------------------------------------------------------------
//! Produces the candidate entity list for a sale, for BOTH the client (what the menu offers) and the
//! server (what it will actually delete).
//!
//! Deliberately a plain static utility rather than a component: the whole point is that there is ONE
//! implementation of "what can this player sell", called verbatim from both sides of the authority
//! boundary. If the client and the server ever asked the question differently, the menu would offer
//! rows the server refuses (or worse, the reverse). Any remaining disagreement is then a state race -
//! the item moved between drawing the card and clicking Sell - which the server resolves by selling
//! fewer items and crediting less.
//!
//! "Unequipped" is decided PER ITEM by IsEquipped(). EStoragePurpose.PURPOSE_DEPOSIT is only the
//! cheap first pass that narrows which storages the engine walks - it is an optimisation hint, NOT
//! the guarantee.
//!
//! The original design (implementation plan D13) trusted the mask alone. Play-testing on 1.7.0 proved
//! that wrong (BUG-083: the Sell list offered worn clothing, and deleting a worn container silently
//! deleted its contents). The reason is in the character prefab itself: Character_Base.et:176 declares
//! SCR_CharacterInventoryStorageComponent with "StoragePurpose 0x9", i.e. PURPOSE_DEPOSIT |
//! PURPOSE_LOADOUT_PROXY. The character's loadout storage therefore MATCHES a PURPOSE_DEPOSIT query,
//! and GetItems happily returns the uniform, vest and backpack sitting in its LoadoutSlotInfo slots.
//! (Vanilla's SCR_ArsenalManagerComponent.c:1219 does use the same mask, but for loadout VALIDATION -
//! "does this player carry a blacklisted prefab" - where including worn gear is the point. It was
//! never a "safe to delete" partition.)
//!
//! Accepted consequence, unchanged: a slung or holstered weapon is NOT sellable - it lives in a
//! weapon-proxy storage - and the player unslings it into their pack first.
//!
//! This class is not Logic-tier testable (it needs a spawned character with a loadout); its decisions
//! are, through OVT_ShopSellRules, which takes the equipped classification as a plain bool.
//------------------------------------------------------------------------------------------------
class OVT_SellableItemScanner
{
	//------------------------------------------------------------------------------------------------
	//! Every item a character is carrying in containers - backpack, vest and jacket pockets, and the
	//! contents of anything stowed inside them.
	//!
	//! Two passes, and only the second one is a guarantee:
	//!
	//! 1. PURPOSE_DEPOSIT narrows the storages the engine walks. It drops the ones that opt out of the
	//!    DEPOSIT bit - the weapon storage (Character_Base.et:307, StoragePurpose 0x2) and the
	//!    equipment/gadget storage (:67, 0x40) both clear it - but it keeps everything that never
	//!    overrode the default, and it keeps the character's own loadout storage, which carries the
	//!    DEPOSIT bit alongside the loadout one (:176, StoragePurpose 0x9 = DEPOSIT | LOADOUT_PROXY).
	//!    Worn clothing, the dog tags in the identity storage and a gadget held in the hand slot all
	//!    survive this pass. That is BUG-083.
	//! 2. IsEquipped() is then applied to every survivor, and is what actually keeps the shirt on the
	//!    player's back. Everything worn, held, slung, holstered or slotted is dropped here; container
	//!    content - including a spare uniform carried inside a pack - survives.
	//!
	//! The result is built into a fresh list rather than removed in place: array removal semantics
	//! (index shuffling) are exactly the kind of detail that turns an under-sell into a delete.
	//! \param[in] character The character to scan. May be null.
	//! \param[out] items Receives the carried items. Allocated if null, cleared otherwise.
	static void CollectUnequippedItems(IEntity character, out array<IEntity> items)
	{
		if(!items) items = new array<IEntity>;
		items.Clear();

		if(!character) return;

		InventoryStorageManagerComponent inventory = GetCharacterInventory(character);
		if(!inventory) return;

		array<IEntity> candidates = new array<IEntity>;
		inventory.GetItems(candidates, EStoragePurpose.PURPOSE_DEPOSIT);

		foreach(IEntity item : candidates)
		{
			if(!item) continue;
			if(IsEquipped(item, character)) continue;

			items.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every item in a vehicle's registered cargo storages.
	//!
	//! Uses the vehicle's own storage manager, which registers cargo storages living on ATTACHED CHILD
	//! entities as well as on the vehicle root - the same call OVT_UnloadStorageAction already relies
	//! on. OVT_InventoryManagerComponent's transfer path instead reads GetOwnedItems() off the root
	//! only and misses trucks whose bed is a separate entity; do not copy that path here.
	//!
	//! Crew personal inventories are never touched, because a crew member's inventory is not a
	//! registered cargo storage of the vehicle.
	//!
	//! Deliberately NOT given the BUG-083 per-item filter: a vehicle has no loadout storage, no
	//! LoadoutSlotInfo slots and nothing "worn", so there is no equipped/unequipped distinction to
	//! draw. Running IsEquipped() here would only mis-classify legitimate cargo - anything sitting in
	//! a storage the classifier does not recognise (a weapon rack, an ammo rack) would fail safe to
	//! "equipped" and quietly stop being sellable.
	//! \param[in] vehicle The vehicle to scan. May be null.
	//! \param[out] items Receives the cargo items. Allocated if null, cleared otherwise.
	static void CollectCargoItems(IEntity vehicle, out array<IEntity> items)
	{
		if(!items) items = new array<IEntity>;
		items.Clear();

		if(!vehicle) return;

		SCR_VehicleInventoryStorageManagerComponent vehicleStorage = GetVehicleCargoStorage(vehicle);
		if(!vehicleStorage) return;

		vehicleStorage.GetItems(items);
	}

	//------------------------------------------------------------------------------------------------
	//! Per-item equipped classification. This is the guarantee behind both collectors, and the answer
	//! for callers that enumerate more broadly (anything using PURPOSE_ANY, or a single item picked
	//! out of the world).
	//!
	//! There is no IsInLoadout() helper in vanilla, so this is a hand-walked chain, ordered
	//! most-specific-first. Every branch has a vanilla precedent or a prefab fact behind it:
	//!
	//! - in hands / in the left-hand gadget slot   SCR_CharacterInventoryStorageComponent.c:788
	//! - worn cloth, asked of the loadout storage  SCR_CharacterInventoryStorageComponent.c:804
	//!   (GetClothFromArea is how vanilla itself answers "is this exact item already worn")
	//! - LoadoutSlotInfo slot                      Campaign_FIA_Player.et:31-40 declares the worn
	//!   Hat/Jacket/Pants/Boots (and Vest/ArmoredVest/Back elsewhere) as LoadoutSlotInfo
	//! - EquipmentStorageSlot slot                 SCR_BlockUnequipItemHintUIInfo.c:15,
	//!   SCR_GadgetManagerComponent.c:723; also covers SCR_SalineBagStorageSlot and
	//!   SCR_TourniquetStorageSlot, which derive from SCR_EquipmentStorageSlot
	//! - hand-slot storage                         SCR_HandSlotStorageComponent derives from
	//!   SCR_UniversalInventoryStorageComponent, so a held gadget WOULD pass the container test below
	//!   if it were not caught first
	//! - weapon-proxy storage                      SCR_CharacterInventoryStorageComponent.c:388 is the
	//!   vanilla "is this weapon slung/holstered" test, EquipedWeaponStorageComponent on the slot's
	//!   storage. Tested through its base so any equipped-weapon storage counts
	//! - loadout storage                           belt and braces for BUG-083: anything whose slot
	//!   belongs to the character's own loadout storage is worn, whatever the slot class turns out to
	//!   be. This is the storage with StoragePurpose 0x9 that leaks worn gear into a DEPOSIT query
	//! - universal storage                         backpack, vest and pocket content, and anything
	//!   nested inside it - the ONLY unequipped case, and the reason a spare uniform in a pack is
	//!   still sellable while the worn one is not
	//!
	//! Note on the deliberate absence of a purpose bitmask test: the prefabs prove EStoragePurpose is a
	//! flag set at engine level (0x1 DEPOSIT, 0x2 WEAPON_PROXY, 0x9 = both DEPOSIT and LOADOUT_PROXY),
	//! but generated/InventorySystem/EStoragePurpose.c declares the enum with NO explicit values, so
	//! script-side the constants may well be plain ordinals 0..7. The first two entries coincide either
	//! way (ordinal 1 == bit 0x1, ordinal 2 == bit 0x2), which is why the existing
	//! "GetPurpose() & PURPOSE_DEPOSIT" idiom elsewhere in Overthrow works. From GADGET_PROXY onwards
	//! ordinal and bit diverge (ordinal 3 vs bit 0x4, LOADOUT_PROXY ordinal 4 vs bit 0x8), so a test
	//! for the loadout bit could silently read a different flag. Concrete class casts cannot drift that
	//! way, so the classification uses them instead - which also means this code does not have to be
	//! right about which bit is which.
	//!
	//! Fails safe: anything it cannot classify is reported as equipped, so an unknown item under-sells
	//! rather than being destroyed for free.
	//! \param[in] item The item to classify.
	//! \param[in] character Optional owning character; when given, the item in hands and the item worn
	//! in each loadout area count as equipped. Omit it to classify a plain item with no character
	//! context - the slot and storage branches still apply.
	//! \return True when the item is worn, held, slung, holstered or otherwise not container content.
	static bool IsEquipped(IEntity item, IEntity character = null)
	{
		if(!item) return false;

		if(character)
		{
			if(IsInHands(character, item)) return true;
			if(IsWornCloth(character, item)) return true;
		}

		InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		// Not an inventory item at all, so it cannot be occupying anyone's slot.
		if(!itemComp) return false;

		InventoryStorageSlot slot = itemComp.GetParentSlot();
		// No parent slot at all: loose in the world, not equipped by anyone.
		if(!slot) return false;

		// Worn on the body (uniform, jacket, pants, boots, headgear, vest, backpack).
		if(LoadoutSlotInfo.Cast(slot)) return true;

		// Gadget / equipment slot, including applied saline bags and tourniquets.
		if(EquipmentStorageSlot.Cast(slot)) return true;

		// Gadget currently held in the hand slot.
		if(SCR_HandSlotStorageSlot.Cast(slot)) return true;

		BaseInventoryStorageComponent storage = slot.GetStorage();
		// Slotted somewhere, but the slot will not say where: fail safe.
		if(!storage) return true;

		// Slung or holstered weapon.
		if(BaseEquipedWeaponStorageComponent.Cast(storage)) return true;

		// A slot of the character's own loadout storage: worn, whatever the slot class is.
		if(EquipedLoadoutStorageComponent.Cast(storage)) return true;

		// Hand-slot storage is universal-derived, so it has to be excluded before the container test.
		if(SCR_HandSlotStorageComponent.Cast(storage)) return true;

		// Container content: backpack, vest, jacket and pants pockets, and anything nested in them.
		if(BaseUniversalInventoryStorageComponent.Cast(storage)) return false;

		// Unrecognised storage - a magazine well, an attachment rail, something modded. Fail safe.
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this exact entity is an item the character currently holds.
	//!
	//! Both hands count. The right-hand item is the classic "current item in hands"; the left hand
	//! holds gadgets (map, compass, radio) and is read through the input context, the same pair of
	//! tests vanilla uses at SCR_CharacterInventoryStorageComponent.c:788.
	//! \param[in] character The character to ask.
	//! \param[in] item The item to compare against.
	//! \return True when the character is holding that item.
	static bool IsInHands(IEntity character, IEntity item)
	{
		if(!character || !item) return false;

		CharacterControllerComponent controller = GetCharacterController(character);
		if(!controller) return false;

		if(controller.GetCurrentItemInHands() == item) return true;

		CharacterInputContext input = controller.GetInputContext();
		if(input && input.GetLeftHandGadgetEntity() == item) return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this exact entity is the cloth the character is currently WEARING in its loadout area.
	//!
	//! Asks the loadout storage rather than the item, which is what makes it safe for the case that
	//! matters: a spare uniform in a backpack has the same BaseLoadoutClothComponent and the same
	//! area type as the worn one, but GetClothFromArea returns only the entity actually attached to
	//! the loadout slot, so the spare still sells. Vanilla uses this exact comparison to stop a player
	//! re-equipping what they already wear (SCR_CharacterInventoryStorageComponent.c:800-805).
	//! \param[in] character The character to ask.
	//! \param[in] item The item to compare against.
	//! \return True when the item is the cloth worn in its own loadout area.
	static bool IsWornCloth(IEntity character, IEntity item)
	{
		if(!character || !item) return false;

		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
		if(!cloth) return false;

		LoadoutAreaType area = cloth.GetAreaType();
		if(!area) return false;

		// Base type on purpose: the character's concrete storage is
		// SCR_CharacterInventoryStorageComponent, which inherits from this.
		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		if(!loadout) return false;

		return loadout.GetClothFromArea(area.Type()) == item;
	}

	//------------------------------------------------------------------------------------------------
	//! The storage manager that owns a character's containers.
	//!
	//! Resolved through the base type so the SAME manager instance that enumerated the items is the
	//! one asked to delete them; the character's concrete manager is
	//! SCR_InventoryStorageManagerComponent, which inherits from this.
	//! \param[in] character The character to look up. May be null.
	//! \return The manager, or null.
	static InventoryStorageManagerComponent GetCharacterInventory(IEntity character)
	{
		if(!character) return null;
		return InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle's cargo storage manager.
	//! \param[in] vehicle The vehicle to look up. May be null.
	//! \return The manager, or null when the vehicle has no cargo storage at all.
	static SCR_VehicleInventoryStorageManagerComponent GetVehicleCargoStorage(IEntity vehicle)
	{
		if(!vehicle) return null;
		return SCR_VehicleInventoryStorageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Character controller lookup that works for both ChimeraCharacter and any other entity that
	//! carries the component directly.
	//! \param[in] character The entity to look up.
	//! \return The controller, or null.
	protected static CharacterControllerComponent GetCharacterController(IEntity character)
	{
		if(!character) return null;

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if(chimera)
		{
			CharacterControllerComponent chimeraController = chimera.GetCharacterController();
			if(chimeraController) return chimeraController;
		}

		return CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
	}
}
