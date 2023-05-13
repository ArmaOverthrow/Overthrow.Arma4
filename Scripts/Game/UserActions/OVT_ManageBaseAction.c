class OVT_ManageBaseAction : ScriptedUserAction
{
	protected bool m_bIsBase;
	protected OVT_BaseData m_BaseData;
	
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		if(SCR_Global.IsEditMode()) return;
		OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(pOwnerEntity.GetOrigin());
		float dist = vector.Distance(base.location, pOwnerEntity.GetOrigin());
		if(dist < 5) {
			m_bIsBase = true;
			m_BaseData = base;
		}		
	}
	
	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity) 
 	{
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;
		
		if(m_bIsBase)
		{
			OVT_BaseMenuContext context = OVT_BaseMenuContext.Cast(ui.GetContext(OVT_BaseMenuContext));
			if(!context) return;
		
			context.m_Base = m_BaseData;
			context.ShowLayout();
		}else{
			OVT_FOBData fob = OVT_Global.GetResistanceFaction().GetNearestFOBData(pOwnerEntity.GetOrigin());
			OVT_FOBMenuContext context = OVT_FOBMenuContext.Cast(ui.GetContext(OVT_FOBMenuContext));
			if(!context) return;
		
			context.m_FOB = fob;
			context.ShowLayout();
		}
 	}
	
	override bool GetActionNameScript(out string outName)
	{
		if(m_bIsBase)
		{
			outName = "#OVT-ManageBase";
		}else{
			outName = "#OVT-ManageFOB";
		}
		
		return true;
	}
}