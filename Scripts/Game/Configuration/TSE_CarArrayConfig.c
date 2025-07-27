[BaseContainerProps(configRoot : true)]
class TSE_CarArrayConfig : ScriptAndConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref TSE_CarArrayAndChance> m_aEntityPrefab;
	
	[Attribute("1.0", desc: "Periodic spawn interval in hours (for traffic simulation)")]
	float m_fPeriodicSpawnIntervalHours;
}