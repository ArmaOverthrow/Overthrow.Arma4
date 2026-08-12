class OVT_CatchBusAction : ScriptedUserAction
{	
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;
		
		OVT_MapContext mapContext = OVT_MapContext.Cast(ui.GetContext(OVT_MapContext));
		if(!mapContext) return;

		// Opens the map and arms NOTHING. Bus eligibility is derived from the player's live position
		// each time a bus stop's info panel is built, so there is no mode to leave set - which is what
		// makes BUG-069 part 2 (a stale bus mode charging a fare on a later map open) unable to recur.
		mapContext.OpenMap();
 	}
	
	override bool HasLocalEffectOnlyScript() { return true; };
}