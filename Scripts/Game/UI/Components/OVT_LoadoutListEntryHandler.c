//! Handler for loadout list entry widgets in the loadouts UI
class OVT_LoadoutListEntryHandler : SCR_ButtonBaseComponent
{
	string m_sLoadoutName;
	int m_iIndex;
	
	//! Populate the entry with loadout data
	void Populate(string loadoutName, int index)
	{
		m_sLoadoutName = loadoutName;
		m_iIndex = index;
		
		// Set loadout name
		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("LoadoutName"));
		if (nameWidget)
		{
			nameWidget.SetText(loadoutName);
		}
		
		// Set loadout type info (personal vs officer template)
		TextWidget infoWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("LoadoutInfo"));
		if (infoWidget)
		{
			// TODO: Check if this is an officer template
			infoWidget.SetText("#OVT-Loadouts_PersonalLoadout");
		}
		
		// Set status
		TextWidget statusWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("LoadoutStatus"));
		if (statusWidget)
		{
			statusWidget.SetText("#OVT-Loadouts_Ready");
			statusWidget.SetColor(Color.Green);
		}
	}
}