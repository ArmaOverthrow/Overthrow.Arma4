class OVT_ComponentClass: ScriptComponentClass
{}

class OVT_Component: ScriptComponent
{
	protected OVT_OverthrowConfigComponent m_Config;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
	}
}