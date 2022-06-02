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
		
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		m_Time = GetGame().GetTimeAndWeatherManager();
	}
}