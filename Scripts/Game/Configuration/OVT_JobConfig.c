enum OVT_JobFlags
{	
	ACTIVE = 1,
	GLOBAL_UNIQUE = 2
};

//A config for a job that has start conditions and a number of stages
[BaseContainerProps(configRoot : true)]
class OVT_JobConfig
{
	[Attribute()]
	string m_sTitle;
	
	[Attribute()]
	string m_sDescription;
	
	[Attribute("0")]
	bool m_bBaseOnly;
	
	[Attribute("1")]
	bool m_bPublic;
	
	[Attribute("100")]
	int m_iReward;
	
	[Attribute("5")]
	int m_iRewardXP;
	
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Reward item prefabs", "et")]
	ref array<ResourceName> m_aRewardItems;
	
	[Attribute(defvalue: "0", desc:"Maximum number of times this job will spawn")]
	int m_iMaxTimes;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_JobCondition> m_aConditions;
	
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_JobStageConfig> m_aStages;
	
	[Attribute("1", uiwidget: UIWidgets.Flags, "", "", ParamEnumArray.FromEnum(OVT_JobFlags))]
	OVT_JobFlags flags;
}

class OVT_JobStageConfig : ScriptAndConfig
{
	[Attribute()]
	string m_sTitle;
	
	[Attribute()]
	string m_sDescription;
	
	[Attribute("0")]
	int m_iTimeout;
	
	[Attribute("", UIWidgets.Object)]
	ref OVT_JobStage m_Handler;
}