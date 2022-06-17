enum OVT_SupportModifierFlags
{	
	ACTIVE = 1,
	STACKABLE = 2	
};

[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("name")]
class OVT_SupportModifierConfig
{
	[Attribute()]
	string name;
	[Attribute()]
	string title;
	[Attribute("-5")]
	float baseEffect;
	[Attribute("1200")]
	int timeout;
	[Attribute("5")]
	int stackLimit;
	[Attribute("1", uiwidget: UIWidgets.Flags, "", "", ParamEnumArray.FromEnum(OVT_SupportModifierFlags))]
	OVT_SupportModifierFlags flags;
	[Attribute("", UIWidgets.Object)]
	ref OVT_SupportModifier handler;
}

[BaseContainerProps(configRoot : true)]
class OVT_SupportModifiersConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_SupportModifierConfig> m_aSupportModifiers;
		
}