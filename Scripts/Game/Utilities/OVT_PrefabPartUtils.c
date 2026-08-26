//------------------------------------------------------------------------------------------------
//! Slot-declared parts: the sub-entities a prefab's OWN slots spawn with it.
//!
//! Vanilla builds gear variants by authoring a slot with a default Prefab rather than by making a
//! new model - Vest_SovietHarness_AR declares two pouches on its BaseLoadoutClothComponent,
//! Rifle_SVD_PSO declares an Optic_PSO1 on its AttachmentSlotComponent, and Scabbard_Bayonet_6Kh4
//! declares its Bayonet_6Kh4 twice over, on an EquipmentStorageComponent InitialStorageSlot and on a
//! BaseSlotComponent AttachType. Those parts are separate entities at runtime, so an inventory walk
//! returns them alongside their holder.
//!
//! They must never become ledger lines: respawning the holder's prefab recreates them, so crediting
//! one mints an item on every withdrawal, and many of them (the harness belt dummies) carry no
//! InventoryItemComponent at all and so have no name to show.
//!
//! The test is "does the HOLDER's prefab declare THIS prefab in that slot", not "is this a cloth
//! part" - an optic the player mounted on a rifle whose prefab declares none is still ordinary loot,
//! and a part swapped for a different one is credited because the respawn will not bring it back.
class OVT_PrefabPartUtils : Managed
{
	//! Declared part prefabs per holder prefab. Read once per prefab; a prefab with none caches empty.
	protected static ref map<string, ref array<string>> s_mDeclared;

	//------------------------------------------------------------------------------------------------
	//! Whether an item is a part its holder's own prefab declares.
	//!
	//! Ordered cheapest-first: only an item sitting in a cloth or attachment slot reaches the prefab
	//! read, so container content - the overwhelming majority of every sweep - costs two casts.
	//!
	//! A part with NO InventoryItemComponent has no parent slot to read, so it falls through to the
	//! prefab test alone: nothing without an item component can be storage CONTENT, which makes "an
	//! entity child of a holder that declares its prefab" the whole answer for it.
	//! \param[in] item The candidate.
	//! \return True when respawning the holder's prefab would recreate this item.
	static bool IsDeclaredPart(IEntity item)
	{
		if (!item)
			return false;

		IEntity holder = item.GetParent();

		InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (itemComp)
		{
			InventoryStorageSlot slot = itemComp.GetParentSlot();
			if (!slot)
				return false;

			// A CARGO slot is player-filled: its occupant is never recreated by respawning the holder.
			// An EquipmentStorageComponent slot is not - it is authored with its prefab and an
			// AllowedItemTypes list, which is how a scabbard carries its bayonet.
			GenericComponent container = slot.GetParentContainer();
			if (!BaseLoadoutClothComponent.Cast(container) && !AttachmentSlotComponent.Cast(container)
				&& !BaseEquipmentStorageComponent.Cast(container))
				return false;

			// The SLOT'S OWNER, not the entity parent. A garment worn on a body reparents its slotted
			// parts to the character, so GetParent() answers "the soldier" and the prefab read then
			// looks for a scabbard among a character's declared parts and finds nothing.
			// Only a storage component exposes GetOwner(); a cloth or attachment slot falls back to the
			// entity parent, which is correct while the garment is not being worn.
			BaseInventoryStorageComponent slotStorage = slot.GetStorage();
			if (slotStorage && slotStorage.GetOwner())
				holder = slotStorage.GetOwner();
		}

		if (!holder)
			return false;

		ResourceName itemPrefab = OVT_PrefabUtils.GetPrefabName(item);
		if (itemPrefab == "")
			return false;

		return GetDeclaredParts(OVT_PrefabUtils.GetPrefabName(holder)).Find(itemPrefab) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The declared parts currently attached to a holder, as entities.
	//!
	//! Entity children, not storage content: a slotted part is a child of the entity that holds it.
	//! \param[in] holder The entity to look under.
	//! \param[out] parts Receives every child that is a declared part of it.
	static void CollectAttachedParts(IEntity holder, out array<IEntity> parts)
	{
		if (!parts)
			parts = new array<IEntity>();

		if (!holder)
			return;

		IEntity child = holder.GetChildren();
		while (child)
		{
			if (IsDeclaredPart(child))
				parts.Insert(child);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every prefab a prefab's own slots spawn with it.
	//!
	//! Four spellings, because vanilla uses four: BaseLoadoutClothComponent.Slots,
	//! AttachmentSlotComponent.AttachmentSlot, BaseEquipmentStorageComponent.InitialStorageSlots and
	//! BaseSlotComponent.AttachType. Attachment slots hang off WeaponComponent rather than off the
	//! entity, so the search must go through component children - hence FindComponentSourcesOfClass
	//! with its child flag set.
	//! \param[in] prefab The holder's prefab.
	//! \return The declared prefabs; never null, and owned by the cache - do not modify it.
	static array<string> GetDeclaredParts(ResourceName prefab)
	{
		if (!s_mDeclared)
			s_mDeclared = new map<string, ref array<string>>();

		array<string> cached = s_mDeclared.Get(prefab);
		if (cached)
			return cached;

		array<string> parts = new array<string>();

		if (prefab == "")
			return parts;

		IEntitySource source = SCR_BaseContainerTools.FindEntitySource(Resource.Load(prefab));
		// An UNLOADED resource answers with a source that has ZERO components rather than with null.
		// Caching that would memoise "declares nothing" for the rest of the session, which is silent
		// and permanent: the guard would go on crediting the parts it exists to drop.
		if (!source || source.GetComponentCount() == 0)
			return parts;

		array<IEntityComponentSource> found = new array<IEntityComponentSource>();

		SCR_BaseContainerTools.FindComponentSourcesOfClass(source, BaseLoadoutClothComponent, true, found);
		foreach (IEntityComponentSource cloth : found)
		{
			BaseContainerList slots = cloth.GetObjectArray("Slots");
			if (!slots)
				continue;

			for (int i = 0, count = slots.Count(); i < count; i++)
			{
				InsertSlotPrefab(slots.Get(i), parts);
			}
		}

		SCR_BaseContainerTools.FindComponentSourcesOfClass(source, AttachmentSlotComponent, true, found);
		foreach (IEntityComponentSource attachment : found)
		{
			InsertSlotPrefab(attachment.GetObject("AttachmentSlot"), parts);
		}

		SCR_BaseContainerTools.FindComponentSourcesOfClass(source, BaseEquipmentStorageComponent, true, found);
		foreach (IEntityComponentSource equipment : found)
		{
			BaseContainerList slots = equipment.GetObjectArray("InitialStorageSlots");
			if (!slots)
				continue;

			for (int i = 0, count = slots.Count(); i < count; i++)
			{
				InsertSlotPrefab(slots.Get(i), parts);
			}
		}

		// A scabbard declares its bayonet on BOTH an equipment slot and a BaseSlotComponent AttachType.
		// Which one holds it at runtime is vanilla's business; either spelling makes it a declared part.
		SCR_BaseContainerTools.FindComponentSourcesOfClass(source, BaseSlotComponent, true, found);
		foreach (IEntityComponentSource attach : found)
		{
			InsertSlotPrefab(attach.GetObject("AttachType"), parts);
		}

		s_mDeclared.Insert(prefab, parts);

		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one slot container's default Prefab into the list.
	//! \param[in] slot The slot container, or null.
	//! \param[out] parts The list being built.
	protected static void InsertSlotPrefab(BaseContainer slot, out array<string> parts)
	{
		if (!slot)
			return;

		ResourceName prefab;
		if (!slot.Get("Prefab", prefab))
			return;

		if (prefab == "" || parts.Find(prefab) != -1)
			return;

		parts.Insert(prefab);
	}
}
