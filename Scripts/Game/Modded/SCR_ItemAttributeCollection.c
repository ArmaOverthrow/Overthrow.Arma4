/* Credit to Integrity Gaming Group <integritygaming.org> */
#ifndef DISABLE_INVENTORY
modded class SCR_ItemAttributeCollection
{
	void SetVisible(bool val)
	{
		m_bVisible = val;
	}
}

modded class SCR_InventorySlotUI
{
	override void UpdateReferencedComponent(InventoryItemComponent pComponent, SCR_ItemAttributeCollection attributes = null)
	{
		SCR_ItemAttributeCollection tempAttrib = attributes;
		if (pComponent && pComponent.GetAttributes())
			tempAttrib = SCR_ItemAttributeCollection.Cast(pComponent.GetAttributes());

		bool revertVisible = false;
		if (tempAttrib) {
			OVT_PlayerOwnerComponent comp = OVT_PlayerOwnerComponent.Cast(pComponent.GetOwner().FindComponent(OVT_PlayerOwnerComponent));
			if (comp && comp.IsLocked()) {
				// Check if the current player is the owner
				string ownerUid = comp.GetPlayerOwnerUid();
				bool isOwner = false;
				
				if (ownerUid != "") {
					IEntity playerEntity = SCR_PlayerController.GetLocalControlledEntity();
					if (playerEntity) {
						string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(playerEntity);
						isOwner = (ownerUid == playerUid);
					}
				}
				
				// Only hide the item if it's locked AND the player is NOT the owner
				if (!isOwner) {
					revertVisible = tempAttrib.IsVisible(pComponent);
					tempAttrib.SetVisible(false);
				}
			}
		}

		super.UpdateReferencedComponent(pComponent, attributes);

		if (tempAttrib && revertVisible) {
			tempAttrib.SetVisible(true);
		}
	}
}
#endif
