[EPF_ComponentSaveDataType(OVT_RealEstateManagerComponent), BaseContainerProps()]
class OVT_RealEstateSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_RealEstateSaveData : EPF_ComponentSaveData
{
	array<ref OVT_WarehouseData> m_aWarehouses;
	ref map<string, ref array<vector>> m_mOwned;
	ref map<string, ref array<vector>> m_mRented;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_RealEstateManagerComponent re = OVT_RealEstateManagerComponent.Cast(component);
		
		m_aWarehouses = re.m_aWarehouses;
		
		m_mOwned = new map<string, ref array<vector>>;
		
		for(int i=0; i<re.m_mOwned.Count(); i++)
		{		
			set<RplId> ownedArray = re.m_mOwned.GetElement(i);
			string playerId = re.m_mOwned.GetKey(i);
			m_mOwned[playerId] = new array<vector>;
			
			foreach(RplId id : ownedArray)
			{				
				m_mOwned[playerId].Insert(re.GetLocationFromId(id));
			}
		}
		
		m_mRented = new map<string, ref array<vector>>;
		
		for(int i=0; i<re.m_mRented.Count(); i++)
		{		
			set<RplId> ownedArray = re.m_mRented.GetElement(i);
			string playerId = re.m_mRented.GetKey(i);
			m_mRented[playerId] = new array<vector>;
			
			foreach(RplId id : ownedArray)
			{
				m_mRented[playerId].Insert(re.GetLocationFromId(id));
			}
		}
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_RealEstateManagerComponent re = OVT_RealEstateManagerComponent.Cast(component);
		
		re.m_aWarehouses = m_aWarehouses;
		
		for(int i=0; i<m_mOwned.Count(); i++)
		{
			array<vector> ownedArray = m_mOwned.GetElement(i);
			string playerId = m_mOwned.GetKey(i);
			
			foreach(vector pos : ownedArray)
			{
				IEntity building = re.GetNearestBuilding(pos, 5);
				if(!building) continue;
				RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
				if(!rpl) continue;
				re.DoSetOwnerPersistentId(playerId, rpl.Id());
			}
		}
		
		for(int i=0; i<m_mRented.Count(); i++)
		{
			array<vector> ownedArray = m_mRented.GetElement(i);
			string playerId = m_mRented.GetKey(i);
			
			foreach(vector pos : ownedArray)
			{
				IEntity building = re.GetNearestBuilding(pos, 5);
				if(!building) continue;
				RplComponent rpl = RplComponent.Cast(building.FindComponent(RplComponent));
				if(!rpl) continue;
				re.DoSetRenterPersistentId(playerId, rpl.Id());
			}
		}
				
		return EPF_EApplyResult.OK;
	}
	
	void ~OVT_RealEstateSaveData()
	{
		m_mOwned.Clear();
		m_mRented.Clear();
	}
}