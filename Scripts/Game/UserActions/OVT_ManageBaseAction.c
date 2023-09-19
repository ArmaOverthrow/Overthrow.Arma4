class OVT_ManageBaseAction : ScriptedUserAction
{

	//---------------------------------------------------------
 	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 	{
		OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(pUserEntity.FindComponent(OVT_UIManagerComponent));
		if(!ui) return;

		OVT_BaseMenuContext context = OVT_BaseMenuContext.Cast(ui.GetContext(OVT_BaseMenuContext));
		if(!context) return;

		context.m_Base = OVT_BaseData.Get(pOwnerEntity.GetOrigin());
		context.ShowLayout();
 	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "#OVT-ManageBase";
		return true;
	}

	override bool CanBeShownScript(IEntity user)
	{
		return !EPF_Component<OVT_BaseControllerComponent>.Find(GetOwner()).IsOccupyingFaction();
	}

	override bool HasLocalEffectOnlyScript() { return true; };
}
