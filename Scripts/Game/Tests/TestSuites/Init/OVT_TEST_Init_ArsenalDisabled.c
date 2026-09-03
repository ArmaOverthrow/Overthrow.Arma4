//------------------------------------------------------------------------------------------------
//! TIER B - the arsenal kill switch against a real vanilla arsenal box (GitHub issue #172).
//!
//! The modded SCR_ArsenalComponent must report disabled and list no stock on a prefab that carries
//! stock. The subject is the vanilla vehicle-gadget box, whose overwrite config holds the wrench
//! and the jerrycan. The case first proves that stock is there, so an empty list is the mod at
//! work and not an empty prefab.
//!
//! Straight-line: spawn, read, delete. No polling, no maxAttempts.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! A vanilla arsenal box with authored stock reports disabled, lists nothing, and registers none
//! of its stock with its arsenal storage manager.
//------------------------------------------------------------------------------------------------
[Test(suite: OVT_TEST_InitSuite, timeoutS: 60)]
class OVT_TEST_Init_ArsenalDisabled_AVehicleGadgetBoxListsNothing : SCR_AutotestCaseBase
{
	static const ResourceName SUBJECT_PREFAB = "{5F20C0063AD2D702}Prefabs/Props/Military/Arsenal/AmmoBoxes/FIA/AmmoBoxArsenal_VehicleGadget_FIA.et";

	protected IEntity m_Subject;

	//------------------------------------------------------------------------------------------------
	[TestStep(TestStage.Main)]
	bool Execute()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns || towns.m_Towns.Count() < 1)
		{
			SetFailure("No towns are registered - nowhere sensible to spawn the subject");
			return true;
		}

		vector origin = towns.m_Towns[0].location + Vector(700, 0, 900);
		m_Subject = OVT_Global.SpawnEntityPrefab(SUBJECT_PREFAB, origin);
		if (!m_Subject)
		{
			SetFailure("SpawnEntityPrefab() produced no entity for the vehicle-gadget arsenal box");
			return true;
		}

		string failure = Check(m_Subject);
		if (failure != "")
			SetFailure(failure);
		else
			Print("Arsenal kill switch: the vehicle-gadget box carries stock, reports disabled, and lists nothing");

		return CleanUp();
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] subject The spawned box.
	//! \return An empty string when every claim held, or the first that did not.
	protected string Check(notnull IEntity subject)
	{
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(subject.FindComponent(SCR_ArsenalComponent));
		if (!arsenal)
			return "The vehicle-gadget box has no SCR_ArsenalComponent - the vanilla prefab changed and this case no longer tests an arsenal";

		SCR_ArsenalItemListConfig stockConfig = arsenal.GetOverwriteArsenalConfig();
		if (!stockConfig)
			return "The vehicle-gadget box has no overwrite arsenal config - the case would be vacuous";

		array<ref SCR_ArsenalItemStandalone> stock;
		if (!stockConfig.GetArsenalItems(stock) || !stock || stock.IsEmpty())
			return "The vehicle-gadget box's overwrite config lists no items - the case would be vacuous";

		if (arsenal.IsArsenalEnabled())
			return "IsArsenalEnabled() returned true - the inventory menu would accept refunds and show the arsenal as live";

		if (arsenal.IsRefundEnabled())
			return "IsRefundEnabled() returned true - the server refund path would still delete deposited items";

		array<ResourceName> available = {};
		if (arsenal.GetAvailablePrefabs(available) || !available.IsEmpty())
			return string.Format("GetAvailablePrefabs() still lists %1 prefabs - the inventory menu would show them and every take spawns a copy", available.Count().ToString());

		array<SCR_ArsenalItem> filtered = {};
		if (arsenal.GetFilteredOverwriteArsenalItems(filtered) || !filtered.IsEmpty())
			return string.Format("GetFilteredOverwriteArsenalItems() still lists %1 items - the server request path would accept them", filtered.Count().ToString());

		SCR_ArsenalInventoryStorageManagerComponent storage = SCR_ArsenalInventoryStorageManagerComponent.Cast(subject.FindComponent(SCR_ArsenalInventoryStorageManagerComponent));
		if (!storage)
			return "The vehicle-gadget box has no SCR_ArsenalInventoryStorageManagerComponent - the vanilla prefab changed";

		foreach (SCR_ArsenalItemStandalone item : stock)
		{
			if (!item)
				continue;

			if (storage.IsPrefabInArsenalStorage(item.GetItemResourceName()))
				return string.Format("The arsenal storage manager registered '%1' as in stock - the inventory manager would treat it as available", item.GetItemResourceName());
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return Always true - the case is over either way.
	protected bool CleanUp()
	{
		if (m_Subject && !m_Subject.IsDeleted())
			delete m_Subject;

		m_Subject = null;

		return true;
	}
}
