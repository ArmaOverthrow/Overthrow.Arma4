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
	
	[Attribute("0", desc:"")]
	bool m_bProcurement;
	
	int m_iTownId = -1;
	
	ref map<int,int> m_aInventory;

	//! Fired whenever one stock entry of this shop changes: (int resourceId, int newAmount).
	//!
	//! Fired on BOTH sides of the wire and exactly once per change:
	//! - StreamInventory() fires it where the mutation happened (server, and therefore a listen-server
	//!   host), because a broadcast Rpc does not execute on the caller;
	//! - RpcDo_SetInventory() fires it on every remote client as the new amount lands.
	//! The shop menu subscribes to this so buying the last unit of an item redraws the grid live
	//! instead of leaving a stale stock corner behind (Definition of Done F14).
	ref ScriptInvoker m_OnInventoryChanged = new ScriptInvoker();

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);	
		
		m_aInventory = new map<int,int>;
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
	
	//! Broadcasts one stock entry to every client. Server-side only: every mutation of m_aInventory
	//! (the buy path, the sell restock, the economy manager's restocks) ends here.
	//! \param[in] id Resource id whose stock changed.
	void StreamInventory(int id)
	{
		int amount = 0;
		if(m_aInventory.Contains(id)) amount = m_aInventory[id];

		Rpc(RpcDo_SetInventory, id, amount);

		// The broadcast does not run on the caller, so the host has to raise its own event here or a
		// listen-server player would never see the grid update the change they just made.
		if(m_OnInventoryChanged) m_OnInventoryChanged.Invoke(id, amount);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SetInventory(int id, int amount)
	{
		m_aInventory[id] = amount;

		if(m_OnInventoryChanged) m_OnInventoryChanged.Invoke(id, amount);
	}

	
}