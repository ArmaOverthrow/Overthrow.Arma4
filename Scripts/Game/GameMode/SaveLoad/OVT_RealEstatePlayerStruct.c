[BaseContainerProps()]
class OVT_RealEstatePlayerStruct : SCR_JsonApiStruct
{
	string m_sPlayerId;
	vector m_vHome;
	ref array<ref OVT_VectorStruct> m_aOwned = {};
		
	void OVT_RealEstatePlayerStruct()
	{
		RegV("m_sPlayerId");
		RegV("m_vHome");
		RegV("m_aOwned");
	}
}

class OVT_VectorStruct : SCR_JsonApiStruct
{
	vector m_vLoc;
	
	void OVT_VectorStruct()
	{
		RegV("m_vLoc");
	}
}