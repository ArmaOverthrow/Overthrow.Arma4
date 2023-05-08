class OVT_WarehouseStruct : OVT_BaseSaveStruct
{
	int id;
	vector location;
	string owner;
	bool isPrivate;
	bool isLinked;
	ref array<ref OVT_InventoryStruct> inventory = {};
	
	void OVT_WarehouseStruct()
	{
		RegV("id");
		RegV("location");
		RegV("owner");
		RegV("isPrivate");
		RegV("isLinked");
		RegV("inventory");
	}
}