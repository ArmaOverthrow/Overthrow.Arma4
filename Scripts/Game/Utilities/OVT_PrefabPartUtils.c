//------------------------------------------------------------------------------------------------
//! Slot-declared parts: the sub-entities a prefab's OWN slots spawn with it.
//!
//! Vanilla builds gear variants by authoring a slot with a default Prefab rather than by making a
//! new model - Vest_SovietHarness_AR declares two pouches on its BaseLoadoutClothComponent, and
//! Rifle_SVD_PSO declares an Optic_PSO1 on its AttachmentSlotComponent. Those parts are separate
//! entities at runtime, so an inventory walk returns them alongside their holder.
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
	//! \param[in] item The candidate.
	//! \return True when respawning the holder's prefab would recreate this item.
	static bool IsDeclaredPart(IEntity item)
	{
		if (!item)
			return false;

		InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComp)
			return false;

		InventoryStorageSlot slot = itemComp.GetParentSlot();
		if (!slot)
			return false;

		// A storage slot is player-filled: its occupant is never recreated by respawning the holder.
		GenericComponent container = slot.GetParentContainer();
		if (!BaseLoadoutClothComponent.Cast(container) && !AttachmentSlotComponent.Cast(container))
			return false;

		IEntity holder = item.GetParent();
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
	//! Every prefab a prefab's own cloth and attachment slots spawn with it.
	//!
	//! Attachment slots hang off WeaponComponent rather than off the entity, so the search must go
	//! through component children - hence FindComponentSourcesOfClass with its child flag set.
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
		s_mDeclared.Insert(prefab, parts);

		if (prefab == "")
			return parts;

		IEntitySource source = SCR_BaseContainerTools.FindEntitySource(Resource.Load(prefab));
		if (!source)
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
