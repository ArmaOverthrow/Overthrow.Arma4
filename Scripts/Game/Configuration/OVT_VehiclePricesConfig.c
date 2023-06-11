[BaseContainerProps(configRoot : true)]
class OVT_VehiclePricesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehiclePriceConfig> m_aPrices;		
}

class OVT_VehiclePriceConfig : ScriptAndConfig
{	
	[Attribute(desc: "String to search in prefab name, blank for all")]
	string m_sFind;
	
	[Attribute("50", desc: "The cost of the vehicles found, will override any above this one")]
	int cost;
	
	[Attribute("5", desc: "Demand Multiplier")]
	int demand;
	
	[Attribute("1", desc: "Vehicle is illegal/lethal")]
	bool illegal;
}