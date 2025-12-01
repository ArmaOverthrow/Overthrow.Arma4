//------------------------------------------------------------------------------------------------
modded class SCR_InventoryMenuUI
{
	//------------------------------------------------------------------------------------------------
	// Override to fix NULL pointer crash in vanilla implementation
	override void HighlightAvailableStorages(SCR_InventorySlotUI itemSlot)
	{
		if (!itemSlot || !m_pActiveHoveredStorageUI || !m_aStorages)
			return;

		InventoryItemComponent itemComp = itemSlot.GetInventoryItemComponent();
		if (!itemComp)
			return;

		foreach (SCR_InventoryStorageBaseUI storageBaseUI : m_aStorages)
		{
			if (!storageBaseUI)
				continue;
			if (storageBaseUI.Type() == SCR_InventoryStorageLootUI)
				continue;
			if (storageBaseUI == m_pActiveHoveredStorageUI)
				continue;

			BaseInventoryStorageComponent contStorage = storageBaseUI.GetStorage();
			if (!contStorage)
				continue;

			storageBaseUI.SetStorageAsHighlighted(true);
		}
	}
}
