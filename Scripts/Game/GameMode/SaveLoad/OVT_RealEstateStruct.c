[BaseContainerProps()]
class OVT_RealEstateStruct : OVT_BaseSaveStruct
{
	protected ref array<ref OVT_RealEstatePlayerStruct> players = {};
		
	override bool Serialize()
	{
		players.Clear();
		
		map<string, OVT_RealEstatePlayerStruct> playerStructs = new map<string, OVT_RealEstatePlayerStruct>;
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		
		for(int i; i<re.m_mHomes.Count(); i++)
		{	
			OVT_RealEstatePlayerStruct struct = new OVT_RealEstatePlayerStruct();
			
			struct.id = re.m_mHomes.GetKey(i);
			struct.home = re.m_mHomes.GetElement(i);
			
			playerStructs[struct.id] = struct;
			players.Insert(struct);
		}
		
		for(int i; i<re.m_mOwned.Count(); i++)
		{
			string playerId = re.m_mOwned.GetKey(i);
			OVT_RealEstatePlayerStruct struct;
			
			if(!playerStructs.Contains(playerId))
			{
				struct = new OVT_RealEstatePlayerStruct();
				struct.id = playerId;
				players.Insert(struct);
			}else{
				struct = playerStructs[playerId];
			}
			
			foreach(RplId id : re.m_mOwned[playerId])
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
				IEntity ent = rpl.GetEntity();
				OVT_VectorStruct vec = new OVT_VectorStruct();
				vec.pos = ent.GetOrigin();
				struct.ownedProperty.Insert(vec);
			}
		}
		
		return true;
	}
	
	override bool Deserialize()
	{
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		
		foreach(OVT_RealEstatePlayerStruct struct : players)
		{
			re.m_mHomes[struct.id] = struct.home;
									
			re.m_mOwned[struct.id] = new set<RplId>;
			
			foreach(OVT_VectorStruct loc : struct.ownedProperty)
			{
				vector vec = loc.pos;
				IEntity ent = re.GetNearestBuilding(vec,2);
				if(ent)
				{
					RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
					re.m_mOwned[struct.id].Insert(rpl.Id());
				}
			}	
		}
		
		return true;
	}
	
	void OVT_RealEstateStruct()
	{
		RegV("players");
	}
}