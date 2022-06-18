enum OVT_ModifierFlags
{	
	ACTIVE = 1,
	STACKABLE = 2	
};

[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("name")]
class OVT_ModifierConfig 
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
	[Attribute("1", uiwidget: UIWidgets.Flags, "", "", ParamEnumArray.FromEnum(OVT_ModifierFlags))]
	OVT_ModifierFlags flags;
	[Attribute("", UIWidgets.Object)]
	ref OVT_Modifier handler;
}

[BaseContainerProps(configRoot : true)]
class OVT_ModifiersConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_ModifierConfig> m_aModifiers;
		
}