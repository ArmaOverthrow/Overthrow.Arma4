modded class SCR_ChimeraCharacter : ChimeraCharacter
{
	OVT_PlayerWantedComponent m_pWantedComponent;
	override Faction GetFaction()
	{
		if (!m_pFactionComponent)
			m_pFactionComponent = FactionAffiliationComponent.Cast(FindComponent(FactionAffiliationComponent));
		
		if (!m_pWantedComponent)
			m_pWantedComponent = OVT_PlayerWantedComponent.Cast(FindComponent(OVT_PlayerWantedComponent));
		
		if (m_pWantedComponent && m_pWantedComponent.GetWantedLevel() < 2)
			return null;
		
		if (m_pFactionComponent)
			return m_pFactionComponent.GetAffiliatedFaction();
		
		return null;
	}
}