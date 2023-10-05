[
    EPF_PersistentScriptedStateSettings(OVT_VirtualizedGroupData),
    EDF_DbName.Automatic()
]
class OVT_VirtualizedGroupSaveData : EPF_ScriptedStateSaveData
{
    int id;					//
	vector pos;				//Current position
	ResourceName prefab; 	//Group prefab
	ref array<vector> waypoints = new array<vector>; //the current waypoints for this group
	OVT_GroupOrder order;	//Current order for this group (determines what type of waypoints are spawned)
	bool isTrigger = false; //Group can spawn nearby virtualized AI like a player can
	float altitude = 0;		//Current altitude above land
	OVT_GroupSpeed speed;	//Travel speed (when virtualized)
	bool cycleWaypoints = true;
	int currentWaypoint = 0;
}

[EPF_ComponentSaveDataType(OVT_VirtualizationManagerComponent), BaseContainerProps()]
class OVT_VirtualizationSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_VirtualizationSaveData : EPF_ComponentSaveData
{
	ref array<string> m_aGroups;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_VirtualizationManagerComponent virt = OVT_VirtualizationManagerComponent.Cast(component);
		
		m_aGroups = new array<string>;
		virt.GetGroupsIdArray(m_aGroups);
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_VirtualizationManagerComponent virt = OVT_VirtualizationManagerComponent.Cast(component);
			
		virt.LoadGroupsIdArray(m_aGroups);
				
		return EPF_EApplyResult.OK;
	}
}