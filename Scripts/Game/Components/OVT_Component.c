class OVT_ComponentClass: ScriptComponentClass
{}

class OVT_Component: ScriptComponent
{
	event void OnInit(IEntity owner);
	protected OVT_OverthrowConfigComponent m_Config;
	
	override void EOnInit(IEntity owner)
	{
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		
		OnInit(owner);		
		
	}
}