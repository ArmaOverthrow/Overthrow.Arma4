modded class SCR_ArsenalComponent : ScriptComponent
{	
	[Attribute("0")]
	bool m_bDontReinsert;
		
	protected override void OnItemRemoved(IEntity entity, BaseInventoryStorageComponent storage)
	{
		if(m_bDontReinsert) return;
		
		// Only refill root inventory items, ignore attachments and magazines on weapons
		if (SCR_Global.IsEditMode() || !entity || storage != m_StorageComponent || m_bIsClearingInventory || !entity || entity.IsDeleted() || !entity.GetPrefabData() || GetOwner().IsDeleted())
		{
			return;
		}
		
		InsertItem(Resource.Load(entity.GetPrefabData().GetPrefabName()));
	}
	
	
};