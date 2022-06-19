class OVT_CatchBusAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;
		
		OVT_MapContext.Cast(ui.GetContext(OVT_MapContext)).EnableBusTravel();
 	}
}