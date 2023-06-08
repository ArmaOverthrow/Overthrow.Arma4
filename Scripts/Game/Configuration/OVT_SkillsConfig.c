[BaseContainerProps(configRoot : true)]
class OVT_SkillsConfig
{	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_SkillConfig> m_aSkills;
}

class OVT_SkillConfig : ScriptAndConfig
{
	[Attribute()]
	string m_sKey;
	
	[Attribute()]
	ref SCR_UIInfo m_UIInfo;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_SkillLevelConfig> m_aLevels;
}

class OVT_SkillLevelConfig : ScriptAndConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_SkillEffect> m_aEffects;
}

class OVT_SkillEffect : ScriptAndConfig
{	
	void SetupSkillServer(ChimeraCharacter character, string persId)
	{
	
	}
	
	void SetupSkillLocal(ChimeraCharacter character)
	{
	
	}
	
	void SetDescriptionTo(TextWidget widget)
	{
		widget.SetText("");
	}
}