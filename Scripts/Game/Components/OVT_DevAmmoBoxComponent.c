//! Fills this box's storage from script shortly after init. The prefab-side
//! InitialInventoryItems mechanism does not populate plain storage boxes (verified on
//! OVT_AmmoBox_Dev even with TargetPurpose set), so dev/test boxes stock themselves the same
//! way OVT_CompositionSpawningDeploymentModule.FillAmmoBoxes stocks base ammo caches.
[ComponentEditorProps(category: "Overthrow", description: "Fills this storage with configured items on init (dev/test convenience)")]
class OVT_DevAmmoBoxComponentClass : ScriptComponentClass
{
}

class OVT_DevAmmoBoxComponent : ScriptComponent
{
	[Attribute(uiwidget: UIWidgets.ResourceAssignArray, desc: "Item prefabs to spawn into this box (one entry per item)", params: "et")]
	ref array<ResourceName> m_aItems;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		// The storage stack is not reliably ready inside INIT; fill shortly after
		GetGame().GetCallqueue().CallLater(Fill, 1000, false, owner);
	}

	protected void Fill(IEntity owner)
	{
		if (!m_aItems)
			return;

		SCR_InventoryStorageManagerComponent storage = SCR_InventoryStorageManagerComponent.Cast(owner.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!storage)
			return;

		// A box restored from a save already has its contents - don't stack a fresh fill on top
		array<IEntity> existing = {};
		storage.GetItems(existing);
		if (!existing.IsEmpty())
			return;

		foreach (ResourceName res : m_aItems)
		{
			if (res == "")
				continue;
			storage.TrySpawnPrefabToStorage(res);
		}
	}
}
