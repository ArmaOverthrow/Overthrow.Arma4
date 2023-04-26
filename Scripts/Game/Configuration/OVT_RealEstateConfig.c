[BaseContainerCustomTitleField("m_Name")]
class OVT_RealEstateConfig : ScriptAndConfig
{
	[Attribute()]
	string m_Name;
	
	[Attribute(desc: "Match one of these filters by resource name to designate a building belonging to this config", params: "et")]
	ref array<string> m_aResourceNameFilters;
	
	[Attribute(defvalue: "120000", desc: "Base sale price of this building type")]
	int m_BasePrice;
	
	[Attribute(defvalue: "4000", desc: "Base rent (per day) of this building type")]
	int m_BaseRent;
	
	[Attribute(defvalue: "0.1", desc: "Multiplies the price/rent by population and stability")]
	float m_DemandMultiplier;
	
	[Attribute()]
	bool m_IsWarehouse;
}