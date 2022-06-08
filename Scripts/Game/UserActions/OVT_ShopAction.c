class OVT_ShopAction : ScriptedUserAction
{
	IEntity m_Register;
	
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Register = pOwnerEntity;		
	}
	
	OVT_ShopComponent GetShop()
	{
		IEntity shop = m_Register.GetParent();
		if(!shop) return null;
		
		return OVT_ShopComponent.Cast(shop.FindComponent(OVT_ShopComponent));
	}
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		if (!CanBePerformedScript(pUserEntity))
		 	return;
		SCR_InventoryStorageManagerComponent genericInventoryManager =  SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent( SCR_InventoryStorageManagerComponent ));
		if ( !genericInventoryManager )
			return;
		
		OVT_ShopComponent shop = GetShop();
		if (!shop)
			return;
		
		PerformActionInternal( shop, genericInventoryManager, pOwnerEntity, pUserEntity);
		
 	}
	
	protected void PerformActionInternal(OVT_ShopComponent shop, SCR_InventoryStorageManagerComponent manager, IEntity pOwnerEntity, IEntity pUserEntity)
	{
		OVT_UIManagerComponent uimanager = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!uimanager) return;
		
		OVT_ShopContext context = OVT_ShopContext.Cast(uimanager.GetContext(OVT_ShopContext));
		if(!context) return;
		
		context.SetShop(shop);
		
		uimanager.ShowContext(OVT_ShopContext);
	}
	
	override bool GetActionNameScript(out string outName)
	{
		return false;
	}
	
	
	override bool CanBePerformedScript(IEntity user)
 	{
		OVT_ShopComponent shop = GetShop();
		if (!shop)
			return false;
		
		Managed genericInventoryManager = user.FindComponent( SCR_InventoryStorageManagerComponent );
		if (!genericInventoryManager)
			return false;
		
		RplComponent genericRpl = RplComponent.Cast(user.FindComponent( RplComponent ));
		if (!genericRpl)
			return false;
		
		return genericRpl.IsOwner();
 	}
}