enum OVT_StabilityModifierFlags
{	
	ACTIVE = 1,
	STACKABLE = 2	
};

[BaseContainerProps(), SCR_BaseContainerCustomTitleResourceName("name", true)]
class OVT_StabilityModifierConfig
{
	[Attribute()]
	string name;
	[Attribute()]
	string title;
	[Attribute("-5")]
	float baseEffect;
	[Attribute("1200")]
	int timeout;
	[Attribute("1", uiwidget: UIWidgets.Flags, "", "", ParamEnumArray.FromEnum(OVT_StabilityModifierFlags))]
	OVT_StabilityModifierFlags flags;
	[Attribute("", UIWidgets.Object)]
	ref OVT_StabilityModifier handler;
}

[BaseContainerProps(configRoot : true)]
class OVT_StabilityModifiersConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_StabilityModifierConfig> m_aStabilityModifiers;
		
}