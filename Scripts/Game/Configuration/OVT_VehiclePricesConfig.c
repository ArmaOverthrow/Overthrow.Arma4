[BaseContainerProps(configRoot : true)]
class OVT_VehiclePricesConfig
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref OVT_VehiclePriceConfig> m_aPrices;		
}

[BaseContainerProps(), OVT_VehiclePriceConfigEntry()]
class OVT_VehiclePriceConfig
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

//~ Custom title to show config
class OVT_VehiclePriceConfigEntry : BaseContainerCustomTitle
{
	//------------------------------------------------------------------------------------------------
	override bool _WB_GetCustomTitle(BaseContainer source, out string title)	
	{	
		bool hidden;
		if (!source.Get("hidden", hidden))
			return false;
		if(hidden)
		{
			title = "Hide   ~   ";
		}

		ResourceName path;
		if (!source.Get("prefab", path))
			return false;
		
		if (!path.IsEmpty())
		{
			title = title + FilePath.StripPath(path.GetPath());
		}
		
		string find;
		if (!source.Get("m_sFind", find))
			return false;
		
		if(find != "")
		{
			title = title + "'" + find + "'";
		}else if(path.IsEmpty()){
			title = title + "Default";
		}

		if(hidden){
			return true;
		}

		int cost;
		if (!source.Get("cost", cost))
			return false;
		
		title = title + " = " + cost.ToString();

		OVT_ParkingType parking;
		if (!source.Get("parking", parking))
			return false;
		
		title = title + " [" + typename.EnumToString(OVT_ParkingType, parking) + "]";

		bool illegal;
		if (!source.Get("illegal", illegal))
			return false;
		
		if(illegal)
		{
			title = title + " **Illegal**";
		}
		
		return true; 
	}
};