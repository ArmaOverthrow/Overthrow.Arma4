class OVT_ComponentClass: ScriptComponentClass
{
	
}

class OVT_Component: ScriptComponent
{
	protected OVT_OverthrowConfigComponent m_Config;
	protected TimeAndWeatherManagerEntity m_Time;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Config = OVT_Global.GetConfig();
		ChimeraWorld world = GetOwner().GetWorld();
		m_Time = world.GetTimeAndWeatherManager();
	}
	
	protected string GetGUID(ResourceName prefab)
	{
		int index = prefab.IndexOf("}");
		if (index == -1) return ResourceName.Empty;
		return prefab.Substring(1, index - 1);
	}

	RplComponent GetRpl()
	{
		return RplComponent.Cast(GetOwner().FindComponent(RplComponent));
	}
}