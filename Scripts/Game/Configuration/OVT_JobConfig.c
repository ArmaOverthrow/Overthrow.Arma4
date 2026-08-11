enum OVT_JobFlags
{	
	ACTIVE = 1,
	GLOBAL_UNIQUE = 2
};

//A config for a job that has start conditions and a number of stages
[BaseContainerProps(configRoot : true)]
class OVT_JobConfig
{
	[Attribute(desc: "Stable identity used by the save format. NEVER change a shipped id.")]
	//! Stable unique identity, short lowercase-kebab, e.g. "raise-support". IMMUTABLE ONCE SHIPPED:
	//! it is the key the save format writes for the job board and for both lifetime counter maps, so
	//! changing it on a shipped config orphans every saved record naming it, and reusing a retired id
	//! silently reattaches old records to a different job.
	//!
	//! This is deliberately NOT m_sTitle (decision D1). m_sTitle is a localization key rendered in the
	//! Jobs menu and in the completion hint; renaming it is a legitimate content edit and must never
	//! break a save. Identity and presentation are two concerns and this field is the identity one.
	string m_sId;

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
	
	[Attribute(defvalue: "0", desc:"Maximum number of times this job will spawn for a player")]
	int m_iMaxTimesPlayer;
	
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