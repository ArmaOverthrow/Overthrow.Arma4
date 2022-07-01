[BaseContainerProps()]
class OVT_RealEstatePlayerStruct : SCR_JsonApiStruct
{
	string id;
	vector home;
	ref array<ref OVT_VectorStruct> ownedProperty = {};
		
	void OVT_RealEstatePlayerStruct()
	{
		RegV("id");
		RegV("home");
		RegV("ownedProperty");
	}
}

class OVT_VectorStruct : SCR_JsonApiStruct
{
	vector pos;
	
	void OVT_VectorStruct()
	{
		RegV("pos");
	}
}