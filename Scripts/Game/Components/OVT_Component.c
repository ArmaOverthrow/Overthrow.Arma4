class OVT_ComponentClass: ScriptComponentClass
{
	
}

class OVT_Component: ScriptComponent
{
	protected TimeAndWeatherManagerEntity m_Time;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Time = GetGame().GetTimeAndWeatherManager();
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