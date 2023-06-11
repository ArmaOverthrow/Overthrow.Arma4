class OVT_MainMenuContextOverrideComponentClass : OVT_ComponentClass
{}

class OVT_MainMenuContextOverrideComponent : OVT_Component
{
	[Attribute()]
	string m_ContextName;
	
	[Attribute()]
	string m_sDescription;
	
	[Attribute("5")]
	float m_fRange;
	
	[Attribute("0")]
	bool m_bMustOwnBase;
}