//------------------------------------------------------------------------------------------------
//! One item inside a saved loadout, and its whole subtree.
//!
//! WHY THIS MIRRORS OVT_LoadoutItem INSTEAD OF BEING IT. OVT_LoadoutItem is a gameplay class that
//! carries a ref map of custom properties and a recursive ref array of children. Every vanilla save
//! class with a nested object graph writes itself field by field through SerializationSave /
//! SerializationLoad rather than leaving it to reflection - SCR_ScenarioFrameworkLayerSave (recursive
//! child layers), SCR_TaskSave (recursive child tasks), SCR_ScenarioFrameworkActionSave. Following
//! that shape means the payload is exactly what this file says it is, the recursion is explicit, and
//! the properties map travels as two parallel string arrays instead of as a map.
//!
//! The mirror also keeps the persistence format from being pinned to a gameplay class: adding a field
//! to OVT_LoadoutItem cannot silently change the save layout of every existing campaign.
//------------------------------------------------------------------------------------------------
class OVT_PersistedLoadoutItem
{
	string resourceName;
	int slotIndex;
	int storagePriority;

	//! EStoragePurpose ordinal. Stored as an int, like EOVTBaseType in OVT_PlaceableComponentSerializer.
	int storagePurpose;

	bool isEquipped;

	ref array<string> attachments = {};

	//! The custom properties map, flattened. Index-aligned with propertyValues.
	ref array<string> propertyKeys = {};
	ref array<string> propertyValues = {};

	ref array<ref OVT_PersistedLoadoutItem> children = {};

	//------------------------------------------------------------------------------------------------
	//! Copies a live item and everything inside it into this record.
	//! \param[in] item The live loadout item.
	void ReadFrom(notnull OVT_LoadoutItem item)
	{
		resourceName = item.m_sResourceName;
		slotIndex = item.m_iSlotIndex;
		storagePriority = item.m_iStoragePriority;
		storagePurpose = item.m_eStoragePurpose;
		isEquipped = item.m_bIsEquipped;

		if (item.m_aAttachments)
		{
			foreach (string attachment : item.m_aAttachments)
			{
				attachments.Insert(attachment);
			}
		}

		if (item.m_mProperties)
		{
			for (int i = 0; i < item.m_mProperties.Count(); i++)
			{
				string propertyKey = item.m_mProperties.GetKey(i);
				if (propertyKey == "")
					continue;

				propertyKeys.Insert(propertyKey);
				propertyValues.Insert(item.m_mProperties.GetElement(i));
			}
		}

		if (item.m_aChildItems)
		{
			foreach (OVT_LoadoutItem childItem : item.m_aChildItems)
			{
				if (!childItem)
					continue;

				OVT_PersistedLoadoutItem childRecord = new OVT_PersistedLoadoutItem();
				childRecord.ReadFrom(childItem);
				children.Insert(childRecord);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds a live item (and its subtree) from this record.
	//! \return A new loadout item.
	OVT_LoadoutItem Build()
	{
		OVT_LoadoutItem item = new OVT_LoadoutItem();

		item.m_sResourceName = resourceName;
		item.m_iSlotIndex = slotIndex;
		item.m_iStoragePriority = storagePriority;
		item.m_eStoragePurpose = storagePurpose;
		item.m_bIsEquipped = isEquipped;

		if (attachments)
		{
			foreach (string attachment : attachments)
			{
				item.AddAttachment(attachment);
			}
		}

		if (propertyKeys && propertyValues)
		{
			int propertyCount = propertyKeys.Count();
			if (propertyValues.Count() < propertyCount)
				propertyCount = propertyValues.Count();

			for (int i = 0; i < propertyCount; i++)
			{
				item.SetProperty(propertyKeys[i], propertyValues[i]);
			}
		}

		if (children)
		{
			foreach (OVT_PersistedLoadoutItem childRecord : children)
			{
				if (!childRecord)
					continue;

				OVT_LoadoutItem childItem = childRecord.Build();
				item.AddChildItem(childItem);
			}
		}

		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes this item. Positional: read order below must match exactly.
	//! \param[in] context Save context to write into.
	//! \return True when written.
	bool SerializationSave(SaveContext context)
	{
		context.Write(resourceName);
		context.Write(slotIndex);
		context.Write(storagePriority);
		context.Write(storagePurpose);
		context.Write(isEquipped);
		context.Write(attachments);
		context.Write(propertyKeys);
		context.Write(propertyValues);
		context.Write(children);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads this item back, in the order SerializationSave wrote it.
	//! \param[in] context Load context to read from.
	//! \return True when read.
	bool SerializationLoad(LoadContext context)
	{
		context.Read(resourceName);
		context.Read(slotIndex);
		context.Read(storagePriority);
		context.Read(storagePurpose);
		context.Read(isEquipped);
		context.Read(attachments);
		context.Read(propertyKeys);
		context.Read(propertyValues);
		context.Read(children);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! One saved loadout, contents included.
//!
//! WHY THE CONTENTS TRAVEL HERE AND NOT IN A PersistentState. A PersistentState declared in the conf
//! is a SINGLETON per class - the engine constructs exactly one and tracks it. Loadouts are per
//! player and variable in number, so modelling them as engine states would mean N script-owned
//! instances registered with StartTracking(), each with its own id that something has to remember and
//! load back by hand. That is precisely the design the old persistence framework had (an index of ids
//! in the manager plus one database row per loadout, fetched on demand by id), and it is why loadout
//! contents never actually came back: the id index and the rows were two halves that could disagree,
//! and the
//! "find a loadout whose id we lost" fallback was never implemented.
//!
//! Writing every loadout in the same record as the index that names them removes the second half
//! entirely - they cannot disagree, because there is only one thing. Vanilla does the same for
//! variable-count script data: SCR_TaskSystemSerializer writes array<ref SCR_TaskSave>.
//!
//! METADATA IS DELIBERATELY PARTIAL: only the usage count is carried, matching the manager's own JIP
//! payload (OVT_LoadoutManagerComponent.RplSave). Category, version, favourite and tags exist on
//! OVT_LoadoutMetadata but nothing in the mod ever writes them, so persisting them would only
//! preserve defaults.
//------------------------------------------------------------------------------------------------
class OVT_PersistedLoadout
{
	//! Cache key, "<persistentId>_<loadoutName>". Stored rather than rebuilt so the key a save was
	//! written with survives a change to how keys are formatted.
	string key;

	string loadoutName;
	string playerId;
	string description;
	int createdTimestamp;
	int lastUsedTimestamp;
	bool isTemplate;
	bool isOfficerTemplate;
	int usageCount;

	ref array<string> quickSlotItems = {};
	ref array<ref OVT_PersistedLoadoutItem> items = {};

	//------------------------------------------------------------------------------------------------
	//! Copies a live loadout into this record.
	//! \param[in] cacheKey The key the loadout is cached under.
	//! \param[in] loadout The live loadout.
	void ReadFrom(string cacheKey, notnull OVT_PlayerLoadout loadout)
	{
		key = cacheKey;
		loadoutName = loadout.m_sLoadoutName;
		playerId = loadout.m_sPlayerId;
		description = loadout.m_sDescription;
		createdTimestamp = loadout.m_iCreatedTimestamp;
		lastUsedTimestamp = loadout.m_iLastUsedTimestamp;
		isTemplate = loadout.m_bIsTemplate;
		isOfficerTemplate = loadout.m_bIsOfficerTemplate;

		if (loadout.m_Metadata)
			usageCount = loadout.m_Metadata.m_iUsageCount;

		if (loadout.m_aQuickSlotItems)
		{
			foreach (string quickSlotItem : loadout.m_aQuickSlotItems)
			{
				quickSlotItems.Insert(quickSlotItem);
			}
		}

		if (loadout.m_aItems)
		{
			foreach (OVT_LoadoutItem item : loadout.m_aItems)
			{
				if (!item)
					continue;

				OVT_PersistedLoadoutItem itemRecord = new OVT_PersistedLoadoutItem();
				itemRecord.ReadFrom(item);
				items.Insert(itemRecord);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Writes this loadout. Positional: read order below must match exactly.
	//! \param[in] context Save context to write into.
	//! \return True when written.
	bool SerializationSave(SaveContext context)
	{
		context.Write(key);
		context.Write(loadoutName);
		context.Write(playerId);
		context.Write(description);
		context.Write(createdTimestamp);
		context.Write(lastUsedTimestamp);
		context.Write(isTemplate);
		context.Write(isOfficerTemplate);
		context.Write(usageCount);
		context.Write(quickSlotItems);
		context.Write(items);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads this loadout back, in the order SerializationSave wrote it.
	//! \param[in] context Load context to read from.
	//! \return True when read.
	bool SerializationLoad(LoadContext context)
	{
		context.Read(key);
		context.Read(loadoutName);
		context.Read(playerId);
		context.Read(description);
		context.Read(createdTimestamp);
		context.Read(lastUsedTimestamp);
		context.Read(isTemplate);
		context.Read(isOfficerTemplate);
		context.Read(usageCount);
		context.Read(quickSlotItems);
		context.Read(items);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Persists OVT_LoadoutManagerComponent: the index of saved loadouts AND their contents.
//!
//! BINDING. Listed in the ComponentSerializers block of the game-mode configuration in
//! Configs/Systems/Persistence/Overthrow.conf.
//!
//! WHAT A LOADOUT IS MADE OF, AND WHERE EACH HALF LIVES NOW:
//!
//!   1. THE INDEX - "<persistentId>_<loadoutName>" to the storage id of that loadout. The manager
//!      owns it, GetAvailableLoadouts() reads it, and it is shipped to clients by the component's own
//!      RplSave/RplLoad. Carried here as two parallel string arrays.
//!
//!   2. THE CONTENTS - every item, attachment, nested container and quick slot. Carried here as one
//!      record per loadout since version 2 (see OVT_PersistedLoadout for why they are not a separate
//!      PersistentState).
//!
//! VERSION 2 IS ADDITIVE. The three version-1 fields keep their positions and a version-1 payload is
//! still read correctly - it simply restores names without contents, which is what a version-1 save
//! actually contained. Binary contexts are POSITIONAL, so the records are only read when the version
//! says they were written.
//!
//! POST-LOAD. No RPC: loadout availability is queried per player through the manager, not pushed.
//!
//! IDEMPOTENT ON A LIVE SESSION. Deserialize also runs when saved data is re-applied to a running
//! campaign (OVT_PersistenceManagerComponent.ReapplyLatestSaveData). The index apply is a
//! clear-and-refill of one map; the contents apply fills the EXISTING OVT_PlayerLoadout in place when
//! one is already cached under the same key, so nothing holding a reference to it is invalidated and
//! a second pass produces the same cache.
//!
//! FORMAT. Binary contexts are POSITIONAL: write order must equal read order. Version first.
//------------------------------------------------------------------------------------------------
class OVT_LoadoutManagerSerializer : ScriptedComponentSerializer
{
	//------------------------------------------------------------------------------------------------
	//! \return The component class this serializer is responsible for.
	override static typename GetTargetType()
	{
		return OVT_LoadoutManagerComponent;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the loadout index and every cached loadout's contents.
	//! \param[in] owner The game mode entity owning the loadout manager.
	//! \param[in] component The loadout manager being saved.
	//! \param[in] context Save context to write into.
	//! \return OK, or ERROR when the component is not an Overthrow loadout manager.
	override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
	{
		OVT_LoadoutManagerComponent loadouts = OVT_LoadoutManagerComponent.Cast(component);
		if (!loadouts)
			return ESerializeResult.ERROR;

		context.WriteValue("version", 2);

		array<string> loadoutKeys = new array<string>();
		array<string> loadoutIds = new array<string>();

		map<string, string> mapping = loadouts.GetLoadoutIdMapping();
		if (mapping)
		{
			for (int i = 0; i < mapping.Count(); i++)
			{
				string key = mapping.GetKey(i);
				string id = mapping.GetElement(i);

				if (key == "" || id == "")
					continue;

				loadoutKeys.Insert(key);
				loadoutIds.Insert(id);
			}
		}

		context.Write(loadoutKeys);
		context.Write(loadoutIds);

		array<ref OVT_PersistedLoadout> records = new array<ref OVT_PersistedLoadout>();

		map<string, ref OVT_PlayerLoadout> active = loadouts.GetActiveLoadouts();
		if (active)
		{
			for (int i = 0; i < active.Count(); i++)
			{
				string cacheKey = active.GetKey(i);
				OVT_PlayerLoadout loadout = active.GetElement(i);

				if (cacheKey == "")
					continue;

				if (!loadout)
					continue;

				// A loadout with no owner cannot be given back to anybody on load.
				if (loadout.m_sPlayerId == "")
					continue;

				OVT_PersistedLoadout record = new OVT_PersistedLoadout();
				record.ReadFrom(cacheKey, loadout);
				records.Insert(record);
			}
		}

		context.Write(records);

		return ESerializeResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the index and the loadout contents back and hands both to the manager to apply.
	//! \param[in] owner The game mode entity owning the loadout manager.
	//! \param[in] component The loadout manager being loaded.
	//! \param[in] context Load context to read from.
	//! \return True when the payload was consumed.
	override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
	{
		OVT_LoadoutManagerComponent loadouts = OVT_LoadoutManagerComponent.Cast(component);
		if (!loadouts)
			return false;

		// See OVT_TownManagerSerializer.Deserialize(). Without this guard an absent payload would clear
		// the whole loadout index.
		int version;
		context.ReadValue("version", version);
		if (version < 1)
			return true;

		array<string> loadoutKeys = new array<string>();
		context.Read(loadoutKeys);

		array<string> loadoutIds = new array<string>();
		context.Read(loadoutIds);

		loadouts.ApplyPersistedLoadoutIds(loadoutKeys, loadoutIds);

		// Version 1 saves stop here - they never carried contents. Reading further would consume
		// whatever happens to follow in a positional binary payload.
		if (version < 2)
			return true;

		array<ref OVT_PersistedLoadout> records = new array<ref OVT_PersistedLoadout>();
		context.Read(records);

		loadouts.ApplyPersistedLoadouts(records);

		return true;
	}
}
