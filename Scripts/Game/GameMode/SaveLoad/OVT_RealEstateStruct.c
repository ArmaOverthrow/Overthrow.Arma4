[BaseContainerProps()]
class OVT_RealEstateStruct : SCR_JsonApiStruct
{
	protected ref array<ref OVT_RealEstatePlayerStruct> m_aPlayerStructs = {};
		
	override bool Serialize()
	{
		map<string, OVT_RealEstatePlayerStruct> players = new map<string, OVT_RealEstatePlayerStruct>;
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		
		for(int i; i<re.m_mHomes.Count(); i++)
		{	
			OVT_RealEstatePlayerStruct struct = new OVT_RealEstatePlayerStruct();
			
			struct.m_sPlayerId = re.m_mHomes.GetKey(i);
			struct.m_vHome = re.m_mHomes.GetElement(i);
			
			players[struct.m_sPlayerId] = struct;
			m_aPlayerStructs.Insert(struct);
		}
		
		for(int i; i<re.m_mOwned.Count(); i++)
		{
			string playerId = re.m_mOwned.GetKey(i);
			OVT_RealEstatePlayerStruct struct;
			
			if(!players.Contains(playerId))
			{
				struct = new OVT_RealEstatePlayerStruct();
				struct.m_sPlayerId = playerId;
				m_aPlayerStructs.Insert(struct);
			}else{
				struct = players[playerId];
			}
			
			foreach(RplId id : re.m_mOwned[playerId])
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				IEntity ent = rpl.GetEntity();
				OVT_VectorStruct vec = new OVT_VectorStruct();
				vec.m_vLoc = ent.GetOrigin();
				struct.m_aOwned.Insert(vec);
			}
		}
		
		return true;
	}
	
	override bool Deserialize()
	{
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		
		foreach(OVT_RealEstatePlayerStruct struct : m_aPlayerStructs)
		{
			re.m_mHomes[struct.m_sPlayerId] = struct.m_vHome;
									
			re.m_mOwned[struct.m_sPlayerId] = new set<RplId>;
			
			foreach(OVT_VectorStruct loc : struct.m_aOwned)
			{
				vector vec = loc.m_vLoc;
				IEntity ent = re.GetNearestBuilding(vec,2);
				if(ent)
				{
					RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
					re.m_mOwned[struct.m_sPlayerId].Insert(rpl.Id());
				}
			}	
		}
		
		return true;
	}
	
	void OVT_RealEstateStruct()
	{
		RegV("m_aPlayerStructs");
	}
}