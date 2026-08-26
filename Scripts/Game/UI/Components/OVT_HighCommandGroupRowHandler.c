//! One selectable row in the barracks purchase list - the OVT_LoadoutListEntryHandler shape.
class OVT_HighCommandGroupRowHandler : SCR_ButtonBaseComponent
{
	//! This row's position in OVT_HighCommandManagerComponent.GetEntryByIndex() - the value the
	//! purchase context re-selects by, never the widget's position in the container.
	int m_iIndex;

	//------------------------------------------------------------------------------------------------
	//! Fill in the row from an authored catalog entry. No server round trip - composition is read
	//! from the prefab source, never spawned (implementation.md §3.4).
	//! \param[in] index This entry's index into the catalog.
	//! \param[in] entry The authored entry.
	//! \param[in] memberCount The entry's member count, already resolved by the caller.
	void Populate(int index, notnull OVT_HighCommandGroupEntry entry, int memberCount)
	{
		m_iIndex = index;

		if (!m_wRoot) return;

		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("GroupName"));
		if (nameWidget)
			nameWidget.SetText(entry.m_sTitle);

		TextWidget membersWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("GroupMembers"));
		if (membersWidget)
			membersWidget.SetTextFormat("#OVT-HC_MemberCountFormat", memberCount.ToString());

		ImageWidget icon = ImageWidget.Cast(m_wRoot.FindAnyWidget("GroupIcon"));
		if (icon && entry.m_sMapIcon != "")
			icon.LoadImageFromSet(0, "{27F2439D610D02B3}UI/Imagesets/MilitarySymbol/ICO_Land.imageset", entry.m_sMapIcon);

		TextWidget vehicleTag = TextWidget.Cast(m_wRoot.FindAnyWidget("GroupVehicleTag"));
		if (vehicleTag)
		{
			if (!entry.m_sVehiclePrefab.IsEmpty())
				vehicleTag.SetText("#OVT-HC_VehicleIncluded");
			else
				vehicleTag.SetText("");
		}
	}
}
