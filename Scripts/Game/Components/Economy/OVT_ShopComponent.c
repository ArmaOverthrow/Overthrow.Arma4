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
	OVT_ShopType m_ShopType;
	
	ref map<int,int> m_aInventory;
	ref array<ref OVT_ShopInventoryItem> m_aInventoryItems;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);	
		
		m_aInventory = new map<int,int>;
		m_aInventoryItems = new array<ref OVT_ShopInventoryItem>;
	}
	
	void AddToInventory(int id, int num)
	{
		OVT_Global.GetServer().AddToShopInventory(this, id, num);		
	}
	
	void TakeFromInventory(int id, int num)
	{		
		OVT_Global.GetServer().TakeFromShopInventory(this, id, num);
	}
	
	int GetStock(int id)
	{
		if(!m_aInventory.Contains(id)) return 0;
		return m_aInventory[id];
	}
	
	//RPC Methods
	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP inventory
		writer.Write(m_aInventory.Count(), 32); 
		for(int i; i<m_aInventory.Count(); i++)
		{
			writer.Write(m_aInventory.GetKey(i),32);
			writer.Write(m_aInventory.GetElement(i),32);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		//Recieve JIP inventory
		int length, num;
		RplId id;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(id,32)) return false;				
			if (!reader.Read(num, 32)) return false;
			m_aInventory[id] = num;
		}
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