class OVT_GunDealerAction : ScriptedUserAction
{
	IEntity m_Dealer;
	
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Dealer = pOwnerEntity;		
	}
	
	OVT_ShopComponent GetShop()
	{		
		return OVT_ShopComponent.Cast(m_Dealer.FindComponent(OVT_ShopComponent));
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
	
	override bool HasLocalEffectOnlyScript() { return true; };
}