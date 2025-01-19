#ifndef DISABLE_INVENTORY
modded class SCR_ItemAttributeCollection
{
    protected bool m_bWasVisible = true; // Tracks previous visibility state

    void SetVisible(bool val)
    {
        if (m_bVisible == val) return; // Prevent unnecessary updates

        m_bWasVisible = m_bVisible; // Store previous state before changing
        m_bVisible = val;
    }

    bool WasPreviouslyVisible()
    {
        return m_bWasVisible;
    }
}

modded class SCR_InventorySlotUI
{
    override void UpdateReferencedComponent(InventoryItemComponent pComponent, SCR_ItemAttributeCollection attributes = null)
    {
        SCR_ItemAttributeCollection tempAttrib = attributes;
        if (pComponent && pComponent.GetAttributes())
            tempAttrib = SCR_ItemAttributeCollection.Cast(pComponent.GetAttributes());

        bool wasVisible = false;
        if (tempAttrib)
        {
            IEntity itemOwner = pComponent.GetOwner();
            if (itemOwner)
            {
                OVT_PlayerOwnerComponent ownerComponent = OVT_PlayerOwnerComponent.Cast(itemOwner.FindComponent(OVT_PlayerOwnerComponent));
                if (ownerComponent && ownerComponent.IsLocked())
                {
                    string ownerUid = ownerComponent.GetPlayerOwnerUid();
                    string playerUid = OVT_Global.GetPlayers().GetPersistentIDFromControlledEntity(GetGame().GetPlayerController().GetControlledEntity());

                    if (ownerUid != playerUid)
                    {
                        wasVisible = tempAttrib.IsVisible();
                        tempAttrib.SetVisible(false); // Hide inventory slot for non-owners
                    }
                }
            }
        }

        super.UpdateReferencedComponent(pComponent, attributes);

        if (tempAttrib && wasVisible)
        {
            tempAttrib.SetVisible(true); // Restore visibility after UI update
        }
    }
}
#endif
