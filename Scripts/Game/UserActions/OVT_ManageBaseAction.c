class OVT_ManageBaseAction : ScriptedUserAction
{

	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;
		
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		vector location = pOwnerEntity.GetOrigin();
		
		OVT_BaseData nearestBase = of.GetNearestBase(location);
		float dist = vector.Distance(nearestBase.location, location);
		
		if(dist < 10)
		{
			OVT_BaseMenuContext context = OVT_BaseMenuContext.Cast(ui.GetContext(OVT_BaseMenuContext));
			if(!context) return;
		
			context.m_Base = nearestBase;
			context.ShowLayout();
			return;
		}
		
		OVT_FOBData nearestFOB = rf.GetNearestFOBData(location);
		dist = vector.Distance(nearestFOB.location, location);
		
		if(dist < 10)
		{			
			OVT_FOBMenuContext context = OVT_FOBMenuContext.Cast(ui.GetContext(OVT_FOBMenuContext));
			if(!context) return;
		
			context.m_FOB = nearestFOB;
			context.ShowLayout();
			return;
		}
		
 	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-ManageBase";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		OVT_BaseControllerComponent baseController = EPF_Component<OVT_BaseControllerComponent>.Find(GetOwner());
		if (!baseController)
		{
			return false;
		}
		return !baseController.IsOccupyingFaction();
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}
