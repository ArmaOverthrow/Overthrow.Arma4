modded class OVT_UIManagerComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if(SCR_Global.IsEditMode()) return;
		
		Print("TSE_UIManagerComponent: Enhanced UI Manager initialized with faction resources and threat display");
	}
} 