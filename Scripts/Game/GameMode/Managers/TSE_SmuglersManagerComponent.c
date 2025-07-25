class TSE_SmuglersManagerComponentClass: OVT_ComponentClass
{
};

class TSE_PrefabItemChanceConfig : ScriptAndConfig
{
	[Attribute(desc: "Prefab of entity", UIWidgets.ResourcePickerThumbnail, params: "et")]
	ResourceName m_sEntityPrefab;
	
	[Attribute("50", desc: "Chance")]
	int chance;

	[Attribute("1", desc: "Minimum number to stock")]
	int minStock;
	
	[Attribute("10", desc: "Maximum number to stock")]
	int maxStock;
}