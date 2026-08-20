//------------------------------------------------------------------------------------------------
//! The port Import screen: one mode, a priced import list, and the occupied vehicle as the only
//! destination. All widget work belongs to OVT_TransferContext.
//------------------------------------------------------------------------------------------------
class OVT_PortContext : OVT_TransferContext
{
	//! OVT_VehicleRequestComponent.IMPORT_MAX_QUANTITY (:64) is protected, so the cap is mirrored
	//! here; the server rejects anything above it.
	protected const int IMPORT_MAX_QUANTITY = 100;

	//! Prefab -> localized display name. UIInfo resolution loads and scans a prefab container, which is
	//! far too expensive to repeat per row per sort comparison; names never change.
	protected ref map<string, string> m_mDisplayNames = new map<string, string>();

	//------------------------------------------------------------------------------------------------
	override void BuildModes(out array<int> modes, out array<string> labelKeys)
	{
		modes.Clear();
		labelKeys.Clear();

		modes.Insert(0);
		labelKeys.Insert("#OVT-Import");
	}

	//------------------------------------------------------------------------------------------------
	override void BuildEntries(int mode, OVT_TransferListModel model)
	{
		if(!model || !m_Economy) return;

		array<ResourceName> prefabs = new array<ResourceName>();
		CollectImportables(prefabs);

		foreach(ResourceName prefab : prefabs)
		{
			int id = m_Economy.GetInventoryId(prefab);

			OVT_TransferEntry entry = new OVT_TransferEntry();
			entry.m_sId = prefab;
			entry.m_sDisplayName = ResolveDisplayName(prefab);
			entry.m_eImageKind = EOVT_TransferImageKind.PREFAB;
			entry.m_sImage = prefab;
			entry.m_iValue = m_Economy.GetPrice(id);
			entry.m_eValueKind = EOVT_TransferValueKind.PRICE;
			entry.m_iMaxQuantity = IMPORT_MAX_QUANTITY;
			entry.m_iCategoryId = ResolveCategory(prefab, id);
			entry.m_bEnabled = true;
			entry.m_sDisabledReasonKey = "";

			model.Add(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The shop browse category for a prefab. An unregistered prefab would resolve to id 0,
	//! i.e. some other item's category, so it is filed under OTHER instead.
	protected int ResolveCategory(ResourceName prefab, int id)
	{
		if(!m_Economy.IsRegisteredResource(prefab)) return OVT_ShopCategory.OTHER;

		return m_Economy.GetItemCategory(id);
	}

	//------------------------------------------------------------------------------------------------
	override string GetCategoryLabelKey(int categoryId)
	{
		OVT_ShopCategory category = categoryId;
		return OVT_ShopCategoryHelper.GetLabelKey(category);
	}

	//------------------------------------------------------------------------------------------------
	override void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)
	{
		name = entry.m_sDisplayName;
		value = FormatValue(entry.m_iValue, entry.m_eValueKind);
		body = "";

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(entry.m_sImage);
		if(info) body = info.GetDescription();
	}

	//------------------------------------------------------------------------------------------------
	//! Import buys; there is no "Add all" for something the player pays for by the unit.
	override bool IsAddAllAllowed(int mode)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void BuildDestinations(out array<ref OVT_TransferDestination> dests)
	{
		dests.Clear();

		IEntity vehicle = GetOccupiedVehicle();
		if(!vehicle) return;

		OVT_TransferDestination dest = new OVT_TransferDestination();
		dest.m_sId = "vehicle";
		dest.m_sLabel = "#OVT-Transfer_DestinationVehicle";
		dest.m_Entity = vehicle;

		dests.Insert(dest);
	}

	//------------------------------------------------------------------------------------------------
	override string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!dest || !dest.m_Entity) return "#OVT-Transfer_NoVehicle";

		if(m_Economy && m_Cart.TotalValue() > m_Economy.GetPlayerMoney(m_sPlayerID))
			return "#OVT-CannotAfford";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! One existing ImportToVehicle per line - no batching, no new RPC (D10). The port has no change
	//! invoker, so the one coalesced refresh is what repaints money and prices.
	override void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)
	{
		if(!lines || !dest || !dest.m_Entity) return;
		if(!m_Economy) return;

		OVT_VehicleRequestComponent vehicles = OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get();
		if(!vehicles) return;

		foreach(OVT_TransferCartLine line : lines)
		{
			if(line.m_iQuantity <= 0) continue;

			int id = m_Economy.GetInventoryId(line.m_sId);
			vehicles.ImportToVehicle(id, line.m_iQuantity, dest.m_Entity);
		}

		ScheduleRefresh();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The vehicle the player occupies, or null.
	protected IEntity GetOccupiedVehicle()
	{
		if(!m_Owner) return null;

		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_Owner.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment) return null;

		return compartment.GetVehicle();
	}

	//------------------------------------------------------------------------------------------------
	//! Lists every prefab this player may import, de-duplicated. Same two branches and same membership
	//! rules as the pre-transfer-UI port screen - only the drawing moved out.
	//! \param[out] prefabs Receives the importable prefabs. Cleared first.
	protected void CollectImportables(out array<ResourceName> prefabs)
	{
		if(!prefabs) return;

		prefabs.Clear();

		// Trade L5 unlocks the extended catalogue anywhere; a resistance-held port area (closest
		// town or base to the port, whichever is nearer) unlocks it for everyone at that port.
		bool illegalImports = m_PlayerData && m_PlayerData.HasPermission("IllegalImports");
		if(!illegalImports && m_Owner)
			illegalImports = m_Economy.ResistanceControlsNearestPort(m_Owner.GetOrigin());

		if(illegalImports)
		{
			array<ResourceName> resources();
			m_Economy.GetAllNonOccupyingFactionItems(resources);
			foreach(ResourceName prefab : resources)
			{
				if(prefabs.Contains(prefab)) continue;
				prefabs.Insert(prefab);
			}
			return;
		}

		// Occupying-faction items must be excluded here too: RpcAsk_ImportToVehicle rejects them
		// outright, so listing them offers a row that can only ever do nothing (BUG-033, BUG-102).
		int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

		foreach(OVT_ShopInventoryConfig shop : m_Economy.m_ShopConfig.m_aShopConfigs)
		{
			if(shop.type == OVT_ShopType.SHOP_VEHICLE) continue;

			foreach(OVT_ShopInventoryItem item : shop.m_aInventoryItems)
			{
				array<SCR_EntityCatalogEntry> entries();
				m_Economy.FindInventoryItems(item.m_eItemType, item.m_eItemMode, item.m_sFind, entries, item.m_bIncludeSupportStationItems);

				foreach(SCR_EntityCatalogEntry entry : entries)
				{
					ResourceName prefab = entry.GetPrefab();
					if(prefabs.Contains(prefab)) continue;

					// GetInventoryId is a bare map index: an unregistered prefab resolves to id 0, i.e.
					// some other item's faction. Only ask about registered ones.
					if(m_Economy.IsRegisteredResource(prefab))
					{
						int id = m_Economy.GetInventoryId(prefab);
						if(m_Economy.ItemIsFromFaction(id, occupyingFactionIndex)) continue;
					}

					prefabs.Insert(prefab);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localized display name for a prefab, memoised for the life of this context. Translated, not
	//! raw: the sort must order by what the player reads, not by "#AR-Item_..." keys.
	//! \param[in] res The prefab to resolve.
	//! \return A non-empty, sortable name.
	protected string ResolveDisplayName(ResourceName res)
	{
		if(m_mDisplayNames.Contains(res)) return m_mDisplayNames[res];

		string name = "";

		UIInfo info = OVT_PrefabUtils.GetItemUIInfo(res);
		if(info) name = info.GetName();

		if(name == "") name = res;

		string translated = WidgetManager.Translate(name);
		if(translated != "") name = translated;

		m_mDisplayNames[res] = name;
		return name;
	}
}
