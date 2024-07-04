[BaseContainerProps(configRoot : true)]
class OVT_VehiclePricesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehiclePriceConfig> m_aPrices;		
}

class OVT_VehiclePriceConfig : ScriptAndConfig
{	
	[Attribute(desc: "(Optional) String to search in EntityCatalog prefab name, blank for all")]
	string m_sFind;
	
	[Attribute(desc: "(Optional) A specific prefab to use for this vehicle (not found in an EntityCatalog)", UIWidgets.ResourcePickerThumbnail, params: "et")]
	ResourceName prefab;
	
	[Attribute("50000", desc: "The cost of the vehicles found, will override any above this one")]
	int cost;
	
	[Attribute("5", desc: "Demand Multiplier")]
	int demand;
	
	[Attribute("1", desc: "Vehicle is illegal/lethal, will not be available at civilian vehicle stores")]
	bool illegal;
	
	[Attribute("0", desc: "If true, will not be available for sale anywhere")]
	bool hidden;
		
	[Attribute("0", UIWidgets.ComboBox, "Parking type", "", ParamEnumArray.FromEnum(OVT_ParkingType) )]
	OVT_ParkingType parking;
}