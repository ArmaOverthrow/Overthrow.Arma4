[BaseContainerProps()]
class OVT_ResistanceFactionStruct : SCR_JsonApiStruct
{
	ref array<ref OVT_VehicleStruct> m_aPlaced = {};
	ref array<ref OVT_VectorStruct> m_aFOBs = {};
	
	override bool Serialize()
	{
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		
		foreach(EntityID id : rf.m_Placed)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(!ent) continue;
			OVT_VehicleStruct struct = new OVT_VehicleStruct();
			if(struct.Parse(ent))
				m_aPlaced.Insert(struct);
		}
		
		return true;
	}
	
	override bool Deserialize()
	{		
		OVT_ResistanceFactionManager rf = OVT_Global.GetResistanceFaction();
		foreach(OVT_VehicleStruct struct : m_aPlaced)
		{			
			IEntity ent = struct.Spawn();
			rf.m_Placed.Insert(ent.GetID());
		}
		
		foreach(OVT_VectorStruct struct : m_aFOBs)
		{	
			rf.m_FOBs.Insert(struct.m_vLoc);
		}
			
		return true;
	}
	
	void OVT_ResistanceFactionStruct()
	{
		RegV("m_aPlaced");
		RegV("m_aFOBs");
	}
}