class OVT_ManageCampAction : ScriptedUserAction
{

	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;
		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		vector location = pOwnerEntity.GetOrigin();
		
		OVT_CampData nearestCamp = rf.GetNearestCampData(location);
		if(!nearestCamp) return;
		
		float dist = vector.Distance(nearestCamp.location, location);
		
		if(dist < 15)  // Slightly larger radius than base action
		{			
			OVT_CampMenuContext context = OVT_CampMenuContext.Cast(ui.GetContext(OVT_CampMenuContext));
			if(!context) return;
		
			context.m_Camp = nearestCamp;
			context.ShowLayout();
			return;
		}
 	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-ManageCamp";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		// Check if we're near a camp
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		if(!rf) return false;
		
		vector location = GetOwner().GetOrigin();
		OVT_CampData nearestCamp = rf.GetNearestCampData(location);
		if(!nearestCamp) return false;
		
		float dist = vector.Distance(nearestCamp.location, location);
		if(dist > 15) return false;
		
		// Only show for camp owner (could be modified to allow officers too)
		string playerID = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(SCR_PlayerController.GetLocalPlayerId());
		return nearestCamp.owner == playerID;
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}