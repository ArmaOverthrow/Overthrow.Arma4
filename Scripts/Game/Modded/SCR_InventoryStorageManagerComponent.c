//------------------------------------------------------------------------------------------------
//! Null-guard for a vanilla 1.7 landmine in SCR_InventoryStorageManagerComponent.OnItemAdded.
//!
//! Vanilla's body ends with `if (storageOwner == m_Storage.GetWeaponStorage())` (1.7.0.54, :539),
//! and m_Storage is only ever assigned for owners that carry a CharacterInventoryStorageComponent
//! (EOnInit, :2141). On any OTHER owner the callback is therefore guaranteed to throw a VME the
//! moment an item lands - and Overthrow inserts items through non-character managers on purpose:
//! OVT_CompositionSpawningDeploymentModule.FillAmmoBoxes stocks base-composition ammo boxes through
//! the box's own manager (seen live: NULL pointer during the opening resource distribution on a new
//! campaign, back when base upgrades still spent the occupying faction's reserve directly).
modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Characters take the vanilla path untouched. A manager with no character storage gets
	//! vanilla's body minus the broken line: the bandage census still applies; the weapon-storage
	//! comparison below it can, by definition, never match when there is no weapon storage.
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		if (m_Storage)
		{
			super.OnItemAdded(storageOwner, item);
			return;
		}

		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		if (consumable && consumable.GetConsumableType() == SCR_EConsumableType.BANDAGE)
			m_iHealthEquipment++;
	}
}
