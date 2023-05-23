[ComponentEditorProps(category: "Overthrow/Components/Economy", description: "")]
class OVT_ShopComponentClass: OVT_ComponentClass
{}

enum OVT_ShopType
{
	SHOP_GENERAL,
	SHOP_DRUG,
	SHOP_CLOTHES,
	SHOP_FOOD,
	SHOP_ELECTRONIC,
	SHOP_GUNDEALER,
	SHOP_VEHICLE
}

class OVT_ShopComponent: OVT_Component
{
	[Attribute("1", UIWidgets.ComboBox, "Shop type", "", ParamEnumArray.FromEnum(OVT_ShopType) )]
	int m_ShopType;
	int m_iTownId = -1;
	
	ref map<int,int> m_aInventory;
	ref array<ref OVT_ShopInventoryItem> m_aInventoryItems;
	ref array<int> m_aInventoryItemIds;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);	
		
		m_aInventory = new map<int,int>;
		m_aInventoryItems = new array<ref OVT_ShopInventoryItem>;
		m_aInventoryItemIds = new array<int>;
	}
	
	void AddToInventory(int id, int num)
	{
		OVT_Global.GetServer().AddToShopInventory(this, id, num);		
	}
	
	void TakeFromInventory(int id, int num)
	{		
		OVT_Global.GetServer().TakeFromShopInventory(this, id, num);
	}
	
	void HandleNPCSale(int id, int num)
	{
		//To-do: Give player money if they own the shop
		TakeFromInventory(id, num);
	}
	
	int GetStock(int id)
	{
		if(!m_aInventory.Contains(id)) return 0;
		return m_aInventory[id];
	}
		
	OVT_TownData GetTown()
	{
		return OVT_Global.GetTowns().GetTown(m_iTownId);
	}
	
	//RPC Methods
	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP inventory
		writer.WriteInt(m_aInventory.Count()); 
		for(int i=0; i<m_aInventory.Count(); i++)
		{
			writer.WriteInt(m_aInventory.GetKey(i));
			writer.WriteInt(m_aInventory.GetElement(i));
		}
		
		writer.WriteInt(m_iTownId);
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		//Recieve JIP inventory
		int length, num;
		RplId id;
		
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			if (!reader.ReadInt(id)) return false;				
			if (!reader.ReadInt(num)) return false;
			m_aInventory[id] = num;
		}
		if (!reader.ReadInt(m_iTownId)) return false;
		
		return true;
	}
	
	void StreamInventory(int id)
	{
		Rpc(RpcDo_SetInventory, id, m_aInventory[id]);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetInventory(int id, int amount)
	{
		m_aInventory[id] = amount;		
	}

	
	void ~OVT_ShopComponent()
	{
		if(m_aInventory)
		{
			m_aInventory.Clear();
			m_aInventory = null;
		}
		if(m_aInventoryItems)
		{
			m_aInventoryItems.Clear();
			m_aInventoryItems = null;
		}
	}
}