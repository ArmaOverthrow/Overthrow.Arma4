[ComponentEditorProps(category: "Overthrow/Components/Economy", description: "")]
class OVT_ShopComponentClass: OVT_ComponentClass
{}

enum OVT_ShopType
{
	SHOP_GENERAL,
	SHOP_DRUG,
	SHOP_CLOTHES,
	SHOP_FOOD,
	SHOP_ELECTRONIC
}

class OVT_ShopComponent: OVT_Component
{
	[Attribute("1", UIWidgets.ComboBox, "Shop type", "", ParamEnumArray.FromEnum(OVT_ShopType) )]
	OVT_ShopType m_ShopType;
	
	protected ref map<ResourceName,int> m_aInventory;
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);	
		
		m_aInventory = new map<ResourceName,int>;
	}
	
	void AddToInventory(ResourceName resource, int num)
	{
		if(!m_aInventory.Contains(resource))
		{
			m_aInventory[resource] = 0;
		}
		
		m_aInventory[resource] = m_aInventory[resource] + num;
	}
	
	void TakeFromInventory(ResourceName resource, int num)
	{
		if(!m_aInventory.Contains(resource)) return;		
		
		m_aInventory[resource] = m_aInventory[resource] - num;
		
		if(m_aInventory[resource] < 0) m_aInventory[resource] = 0;
	}
}