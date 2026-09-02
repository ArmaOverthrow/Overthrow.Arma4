//------------------------------------------------------------------------------------------------
//! Stateless prefab/resource inspection helpers: resolving the prefab an entity came from, and reading
//! UI info out of a prefab resource without spawning it.
//!
//! Split out of OVT_Global (see OVT_WorldUtils). OVT_Global keeps a one-line GetPrefabName forwarder;
//! the UI-info readers are called on this class directly.
class OVT_PrefabUtils : Managed
{
	//! Links of a prefab chain either ancestry walk will follow. A cycle here would hang its caller.
	static const int MAX_ANCESTRY_DEPTH = 16;

	//! Prefab -> hidden, written only from a read that reached a definite answer. See
	//! IsItemHiddenInInventory: an unreadable level is never memoised.
	protected static ref map<ResourceName, bool> s_mHiddenInInventory;

	//------------------------------------------------------------------------------------------------
	//! Gets the prefab an entity was spawned from, resolving prefab inheritance the way the engine does.
	//!
	//! Walks the container ancestry to the first container that actually came from a .et file, which is
	//! what makes this correct for nested/inherited prefabs where EntityPrefabData.GetPrefabName() can
	//! answer with an intermediate name. This is the vanilla replacement for the prefab-name utility the
	//! old persistence framework provided, and is deliberately identical to it.
	//! \param[in] entity The entity to look up.
	//! \return Its prefab resource name, or an empty resource name.
	static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity)
			return ResourceName.Empty;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return ResourceName.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(prefabData.GetPrefab());
	}
	static SCR_EditableVehicleUIInfo GetVehicleUIInfo(ResourceName res)
	{
		Resource holder = BaseContainerTools.LoadContainer(res);
		if (holder)
		{
			IEntitySource ent = holder.GetResource().ToEntitySource();
			for(int t=0; t<ent.GetComponentCount(); t++)
			{
				IEntityComponentSource comp = ent.GetComponent(t);
				if(comp.GetClassName() == "SCR_EditableVehicleComponent")
				{
					SCR_EditableVehicleUIInfo info;
					comp.Get("m_UIInfo",info);
					return info;
				}
			}
		}
		return null;
	}
	
	static SCR_EditableEntityUIInfo GetEditableUIInfo(ResourceName res)
	{
		Resource holder = BaseContainerTools.LoadContainer(res);
		if (holder)
		{
			IEntitySource ent = holder.GetResource().ToEntitySource();
			for(int t=0; t<ent.GetComponentCount(); t++)
			{
				IEntityComponentSource comp = ent.GetComponent(t);
				if(comp.GetClassName() == "SCR_EditableVehicleComponent")
				{
					SCR_EditableEntityUIInfo info;
					comp.Get("m_UIInfo",info);
					return info;
				}
			}
		}
		return null;
	}
	
	//! Reads an item prefab's display-name UIInfo, walking the prefab ancestry when the prefab itself
	//! does not carry one.
	//!
	//! The ancestry walk is load-bearing, not defensive: a delta that authors no InventoryItemComponent
	//! of its own (vanilla's whole `*_Dirty` clothing family, which only overrides a material) resolves
	//! no ItemDisplayName from its own source, and the UI would fall back to the raw ResourceName.
	//! \param[in] prefab The item prefab to read.
	//! \return Its display-name UIInfo, or null if no prefab in the chain declares one.
	static UIInfo GetItemUIInfo(ResourceName prefab)
	{
		ResourceName current = prefab;

		// Bounded: a prefab chain is a few links deep, and a cycle here would hang the UI thread.
		for(int depth = 0; depth < 16 && !current.IsEmpty(); depth++)
		{
			// The Resource is held in a local for the whole read - a temporary can be evicted out from
			// under the IEntitySource it produced, which reads back as a source with zero components.
			Resource holder = Resource.Load(current);
			IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(holder);
			if (!entitySource || entitySource.GetComponentCount() == 0)
				return null;

			UIInfo info = ReadItemDisplayName(entitySource);
			if (info && info.GetName() != "")
				return info;

			BaseContainer ancestor = entitySource.GetAncestor();
			if (!ancestor)
				return info;

			ResourceName parent = ancestor.GetResourceName();
			if (parent == current)
				return info;

			current = parent;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether an item would be invisible in a vanilla inventory, and so would be lost the moment it
	//! left a ledger for one - vanilla draws no slot for it (SCR_InventorySlotUI.c:103).
	//!
	//! FAILS OPEN. m_bVisible defaults to 1 (SCR_ItemAttributeCollection.c:16) and only 19 vanilla
	//! prefabs author it at all, so anything unreadable is treated as visible: making an arbitrary item
	//! permanently un-takeable off one bad prefab read is far worse than letting a hidden one through.
	//! \param[in] prefab The item prefab to test.
	//! \return True only when an authored m_bVisible 0 was found somewhere in the prefab chain.
	static bool IsItemHiddenInInventory(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		if (!s_mHiddenInInventory)
			s_mHiddenInInventory = new map<ResourceName, bool>();

		bool cached;
		if (s_mHiddenInInventory.Find(prefab, cached))
			return cached;

		bool authoredFound;
		bool authoredVisible;
		bool cacheable;
		if (!ReadAuthoredVisibility(prefab, authoredFound, authoredVisible, cacheable))
			return false;

		bool hidden = OVT_StorageRules.HiddenFromInventory(authoredFound, authoredVisible);
		if (cacheable)
			s_mHiddenInInventory.Insert(prefab, hidden);

		return hidden;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks a prefab chain looking for an authored m_bVisible.
	//!
	//! The walk is load-bearing for the same reason GetItemUIInfo's is: BaseContainer.Get reads one
	//! level, and the Box_25x137_M242_* family authors the flag on its _Base prefabs while the concrete
	//! variants are empty deltas.
	//! \param[in] prefab Where the walk starts.
	//! \param[out] authoredFound Whether any level authored the flag.
	//! \param[out] authoredVisible The authored value; meaningless when nothing was found.
	//! \param[out] cacheable False when any level read as unreadable - the answer may be right, but a
	//!        skipped level could have authored an override, so it must not be memoised.
	//! \return False when the chain could not be read to a conclusion - the caller must NOT cache that.
	protected static bool ReadAuthoredVisibility(ResourceName prefab, out bool authoredFound, out bool authoredVisible, out bool cacheable)
	{
		authoredFound = false;
		authoredVisible = true;
		cacheable = true;

		ResourceName current = prefab;
		bool anyLevelUnreadable = false;

		for(int depth = 0; depth < MAX_ANCESTRY_DEPTH && !current.IsEmpty(); depth++)
		{
			// The Resource is held in a local for the whole read - a temporary can be evicted out from
			// under the IEntitySource it produced, which reads back as a source with zero components.
			Resource holder = Resource.Load(current);
			IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(holder);
			if (!entitySource)
				return false;

			// Zero components is UNKNOWN, not "authors nothing": a resource that is not loaded reads
			// this way. Climbing is still worth trying, but nothing derived from here may be cached.
			if (entitySource.GetComponentCount() == 0)
			{
				anyLevelUnreadable = true;
			}
			else if (ReadItemVisibility(entitySource, authoredVisible))
			{
				authoredFound = true;
				cacheable = !anyLevelUnreadable;
				return true;
			}

			BaseContainer ancestor = entitySource.GetAncestor();
			if (!ancestor)
				break;

			ResourceName parent = ancestor.GetResourceName();
			if (parent == current)
				break;

			current = parent;
		}

		cacheable = !anyLevelUnreadable;
		return !anyLevelUnreadable;
	}

	//------------------------------------------------------------------------------------------------
	//! The single-source half of ReadAuthoredVisibility: reads m_bVisible off this source only.
	//! \param[in] entitySource The prefab source to read.
	//! \param[out] visible The authored value; untouched when nothing authored it.
	//! \return True when this level authored the flag.
	protected static bool ReadItemVisibility(notnull IEntitySource entitySource, out bool visible)
	{
		for(int nComponent, componentCount = entitySource.GetComponentCount(); nComponent < componentCount; nComponent++)
		{
			IEntityComponentSource componentSource = entitySource.GetComponent(nComponent);
			if (!componentSource) continue;

			typename componentType = componentSource.GetClassName().ToType();
			if(componentType == typename.Empty || !componentType.IsInherited(InventoryItemComponent)) continue;

			BaseContainer attributesContainer = componentSource.GetObject("Attributes");
			if (!attributesContainer) continue;

			if (attributesContainer.Get("m_bVisible", visible))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The clean prefab a cosmetic "dirty" variant is a delta over.
	//!
	//! Vanilla ships 17 `*_Dirty` clothing prefabs, each an inheritance delta over its clean sibling
	//! that overrides nothing but a material. They are what civilians wear, so they are most of what a
	//! loot run collects - and as ledger lines they are pure noise: a separate stack of the same
	//! garment, and one that declares no display name of its own. Stored gear comes out clean.
	//!
	//! Suffix-driven on purpose. The alternative - "a delta that only overrides materials" - would also
	//! collapse every deliberate colour variant, and those are distinct items.
	//! \param[in] prefab The prefab as spawned.
	//! \return Its clean ancestor, or the input unchanged when it is not a dirty variant.
	static ResourceName ResolveCleanVariant(ResourceName prefab)
	{
		ResourceName current = prefab;

		// Bounded: no vanilla chain stacks these, and a cycle here would hang the looting job.
		for(int depth = 0; depth < 4; depth++)
		{
			if (!IsDirtyVariant(current))
				return current;

			Resource holder = Resource.Load(current);
			IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(holder);
			if (!entitySource)
				return current;

			BaseContainer ancestor = entitySource.GetAncestor();
			if (!ancestor)
				return current;

			ResourceName parent = ancestor.GetResourceName();
			if (parent.IsEmpty() || parent == current)
				return current;

			current = parent;
		}

		return current;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a prefab's file stem ends in "_dirty", in any casing - vanilla uses both.
	//! \param[in] prefab The prefab to test.
	//! \return True for a cosmetic dirty variant.
	static bool IsDirtyVariant(ResourceName prefab)
	{
		string stem = prefab;

		int dot = stem.LastIndexOf(".");
		if(dot > 0) stem = stem.Substring(0, dot);

		stem.ToLower();
		return stem.EndsWith("_dirty");
	}

	//------------------------------------------------------------------------------------------------
	//! Last-resort readable name for a prefab that declares no display name anywhere in its chain.
	//! A raw ResourceName is a GUID plus a full path and is unreadable in a list; the file stem is not.
	//! \param[in] prefab The prefab to name.
	//! \return "Jacket Denim 01 strippedShirt dirty" for the denim jacket delta, never a GUID.
	static string PrettyPrefabName(ResourceName prefab)
	{
		string path = prefab;

		int brace = path.IndexOf("}");
		if(brace >= 0) path = path.Substring(brace + 1, path.Length() - brace - 1);

		int slash = path.LastIndexOf("/");
		if(slash >= 0) path = path.Substring(slash + 1, path.Length() - slash - 1);

		int dot = path.LastIndexOf(".");
		if(dot > 0) path = path.Substring(0, dot);

		path.Replace("_", " ");
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! The single-source half of GetItemUIInfo: reads ItemDisplayName off this source only, no ancestry.
	//! \param[in] entitySource The prefab source to read.
	//! \return Its display-name UIInfo, or null.
	protected static UIInfo ReadItemDisplayName(notnull IEntitySource entitySource)
	{
		for(int nComponent, componentCount = entitySource.GetComponentCount(); nComponent < componentCount; nComponent++)
		{
			IEntityComponentSource componentSource = entitySource.GetComponent(nComponent);

			typename componentType = componentSource.GetClassName().ToType();
			if(componentType == typename.Empty || !componentType.IsInherited(InventoryItemComponent)) continue;

			BaseContainer attributesContainer = componentSource.GetObject("Attributes");
			if (!attributesContainer) continue;

			BaseContainer itemDisplayNameContainer = attributesContainer.GetObject("ItemDisplayName");
			if (!itemDisplayNameContainer) continue;

			return UIInfo.Cast(BaseContainerTools.CreateInstanceFromContainer(itemDisplayNameContainer));
		}

		return null;
	}
}
