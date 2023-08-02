[EPF_ComponentSaveDataType(OVT_ResistanceFactionManager), BaseContainerProps()]
class OVT_ResistanceSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_ResistanceSaveData : EPF_ComponentSaveData
{
	ref array<ref OVT_CampData> m_Camps;
	string m_sPlayerFactionKey;
	bool m_bFOBDeployed = false;
	vector m_vFOBLocation;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_ResistanceFactionManager resistance = OVT_ResistanceFactionManager.Cast(component);
		
		m_Camps = new array<ref OVT_CampData>;
		m_sPlayerFactionKey = OVT_Global.GetConfig().m_sPlayerFaction;
		
		m_bFOBDeployed = resistance.m_bFOBDeployed;
		m_vFOBLocation = resistance.m_vFOBLocation;
		
		foreach(OVT_CampData fob : resistance.m_Camps)
		{
			fob.garrison.Clear();
			foreach(EntityID id : fob.garrisonEntities)
			{
				SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(id));
				if(!aigroup) continue;
				if(aigroup.GetAgentsCount() > 0)
				{
					ResourceName res = EPF_Utils.GetPrefabName(aigroup);
					fob.garrison.Insert(res);					
				}
			}	
			m_Camps.Insert(fob);
		}
		
		return EPF_EReadResult.OK;
	}

	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_ResistanceFactionManager resistance = OVT_ResistanceFactionManager.Cast(component);
		
		resistance.m_bFOBDeployed = m_bFOBDeployed;
		resistance.m_vFOBLocation = m_vFOBLocation;

		if (m_sPlayerFactionKey.IsEmpty())
		{
			Print("Player faction key is invalid, setting to FIA", LogLevel.WARNING);
			m_sPlayerFactionKey = "FIA";
		}

		OVT_Global.GetConfig().m_sPlayerFaction = m_sPlayerFactionKey;
		int playerFactionIndex = GetGame().GetFactionManager().GetFactionIndex(GetGame().GetFactionManager().GetFactionByKey(m_sPlayerFactionKey));
		OVT_Global.GetConfig().m_iPlayerFactionIndex = playerFactionIndex;

		foreach(OVT_CampData fob : m_Camps)
		{	
			fob.id = resistance.m_Camps.Count();			
			resistance.m_Camps.Insert(fob);
		}

		return EPF_EApplyResult.OK;
	}
}