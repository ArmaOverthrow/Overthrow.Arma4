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
	
	ref map<ResourceName,int> m_aInventory;
	ref array<ref OVT_ShopInventoryItem> m_aInventoryItems;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);	
		
		m_aInventory = new map<ResourceName,int>;
		m_aInventoryItems = new array<ref OVT_ShopInventoryItem>;
	}
	
	void AddToInventory(ResourceName resource, int num)
	{
		Rpc(RpcAsk_AddToInventory, resource, num);
	}
	
	void TakeFromInventory(ResourceName resource, int num)
	{
		Rpc(RpcAsk_TakeFromInventory, resource, num);
	}
	
	int GetStock(ResourceName resource)
	{
		if(!m_aInventory.Contains(resource)) return 0;
		return m_aInventory[resource];
	}
	
	//RPC Methods
	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP inventory
		writer.Write(m_aInventory.Count(), 32); 
		for(int i; i<m_aInventory.Count(); i++)
		{
			string key = m_aInventory.GetKey(i);			
			writer.Write(key.Length(),32);
			writer.Write(key, 8 * key.Length());
			writer.Write(m_aInventory[key],32);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{
		//Recieve JIP inventory
		int length, keylength, num;
		string key;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			if (!reader.Read(keylength, 32)) return false;
			key = "";
			if (!reader.Read(key, keylength)) return false;			
			if (!reader.Read(num, 32)) return false;
			m_aInventory[key] = num;
		}
		return true;
	}
	
	protected void StreamInventory(ResourceName resource)
	{
		Rpc(RpcDo_SetInventory, resource, m_aInventory[resource]);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetInventory(ResourceName resource, int amount)
	{
		m_aInventory[resource] = amount;		
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_AddToInventory(ResourceName resource, int num)
	{
		if(!m_aInventory.Contains(resource))
		{
			m_aInventory[resource] = 0;
		}		
		m_aInventory[resource] = m_aInventory[resource] + num;
		StreamInventory(resource);
	}
	
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TakeFromInventory(ResourceName resource, int num)
	{
		if(!m_aInventory.Contains(resource)) return;		
		m_aInventory[resource] = m_aInventory[resource] - num;		
		if(m_aInventory[resource] < 0) m_aInventory[resource] = 0;
		StreamInventory(resource);
	}
}