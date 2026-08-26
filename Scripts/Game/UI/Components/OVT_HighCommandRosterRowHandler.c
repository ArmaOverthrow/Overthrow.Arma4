//------------------------------------------------------------------------------------------------
//! One row of the "Manage Groups" roster - the OVT_HighCommandGroupRowHandler shape, plus the
//! status badge and colour OVT_MapHighCommandLayer's marker also shows, so the roster and the map
//! never disagree about what a group is doing.
//------------------------------------------------------------------------------------------------
class OVT_HighCommandRosterRowHandler : SCR_ButtonBaseComponent
{
	//! This row's position in the roster's flat, owner-ordered list - what the context re-selects
	//! by, never the widget's position in the container (D16's reason).
	int m_iIndex;

	protected string m_sGroupId;

	//! Vanilla's NATO symbol atlas, for the contact badge - the OVT_MapHighCommandLayer constant.
	protected static const ResourceName SYMBOL_IMAGESET = "{8479B3B5347DF5CF}UI/Imagesets/MilitarySymbol/ID_D.imageset";

	//! Overthrow's own atlas, for the ammo badge.
	protected static const ResourceName STATUS_ICON_IMAGESET = "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset";

	//! No badge art exists for OVT_HighCommandStatus's two tags (Phase 7's note). Resolved onto the
	//! SAME shipped quads OVT_MapHighCommandLayer uses - an independent pair by design, not a shared
	//! accessor, so update both places when art arrives.
	protected static const string BADGE_CONTACT_QUAD = "Hostile_Land_Bcg";
	protected static const string BADGE_NO_AMMO_QUAD = "recruit_ammo_empty";

	//------------------------------------------------------------------------------------------------
	//! Fill in the row from an owned group's record.
	//! \param[in] record The group's record.
	//! \param[in] index This row's position in the roster's flat list.
	void Populate(notnull OVT_HighCommandRecord record, int index)
	{
		m_iIndex = index;
		m_sGroupId = record.m_sGroupId;

		if (!m_wRoot)
			return;

		OVT_HighCommandManagerComponent manager = OVT_HighCommandManagerComponent.GetInstance();
		OVT_HighCommandGroupEntry entry;
		if (manager)
			entry = manager.GetEntryByKey(record.m_sEntryKey);

		TextWidget nameWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("GroupName"));
		if (nameWidget)
		{
			if (entry && entry.m_sTitle != "")
				nameWidget.SetText(entry.m_sTitle);
			else
				nameWidget.SetText(record.m_sEntryKey);
		}

		TextWidget subtitleWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("GroupSubtitle"));
		if (subtitleWidget)
		{
			subtitleWidget.SetTextFormat(
				"#OVT-HC_Roster_RowSubtitle",
				record.m_iAliveMembers.ToString(),
				record.m_iTotalMembers.ToString(),
				WidgetManager.Translate(StanceLabel(record.m_iStance))
			);
		}

		ImageWidget icon = ImageWidget.Cast(m_wRoot.FindAnyWidget("GroupIcon"));
		if (icon)
		{
			string mapIcon = "Infantry_Friend";
			if (entry && entry.m_sMapIcon != "")
				mapIcon = entry.m_sMapIcon;

			icon.LoadImageFromSet(0, "{27F2439D610D02B3}UI/Imagesets/MilitarySymbol/ICO_Land.imageset", mapIcon);
		}

		TextWidget statusWidget = TextWidget.Cast(m_wRoot.FindAnyWidget("GroupStatus"));
		if (statusWidget)
		{
			statusWidget.SetText(StatusLabel(record));
			statusWidget.SetColor(StatusColor(record));
		}

		UpdateStatusIcon(record.m_iStatusFlags);
	}

	//------------------------------------------------------------------------------------------------
	//! Draw or hide the contact/no-ammo badge - the OVT_MapHighCommandLayer.UpdateTag shape, on a
	//! single always-present icon rather than the map's per-frame change-filtered map.
	//! \param[in] statusFlags OVT_HighCommandStatus mask.
	protected void UpdateStatusIcon(int statusFlags)
	{
		ImageWidget icon = ImageWidget.Cast(m_wRoot.FindAnyWidget("StatusIcon"));
		if (!icon)
			return;

		string quad = OVT_HighCommandStatus.TagIcon(statusFlags);
		if (quad.IsEmpty())
		{
			icon.SetVisible(false);
			return;
		}

		if (quad == OVT_HighCommandStatus.TAG_CONTACT)
		{
			icon.LoadImageFromSet(0, SYMBOL_IMAGESET, BADGE_CONTACT_QUAD);
			icon.SetColor(Color.FromRGBA(255, 70, 70, 255));
		}
		else
		{
			icon.LoadImageFromSet(0, STATUS_ICON_IMAGESET, BADGE_NO_AMMO_QUAD);
			icon.SetColor(Color.FromRGBA(255, 255, 255, 255));
		}

		icon.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] stance An OVT_EHighCommandStance value.
	//! \return Its localization key - the OVT_MapHighCommandLayer.StanceLabel wording, reused so the
	//! roster and the map info panel never name a stance differently. Also used by the roster's own
	//! details panel.
	static string StanceLabel(int stance)
	{
		if (stance == OVT_EHighCommandStance.PATROL)
			return "#OVT-HC_Stance_Patrol";

		if (stance == OVT_EHighCommandStance.ATTACK)
			return "#OVT-HC_Stance_Attack";

		return "#OVT-HC_Stance_Defend";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return Its localization key - contact first, the OVT_MapHighCommandLayer.StatusLabel order.
	//! Also used by the roster's own details panel.
	static string StatusLabel(notnull OVT_HighCommandRecord record)
	{
		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.CONTACT))
			return "#OVT-HC_Map_StatusContact";

		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.NO_AMMO))
			return "#OVT-HC_Map_StatusNoAmmo";

		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.MOVING))
			return "#OVT-HC_Map_StatusMoving";

		return "#OVT-HC_Map_StatusHolding";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] record The group's record.
	//! \return A colour for the status text - contact red, no ammo orange, otherwise white.
	static Color StatusColor(notnull OVT_HighCommandRecord record)
	{
		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.CONTACT))
			return Color.Red;

		if (OVT_HighCommandStatus.HasFlag(record.m_iStatusFlags, OVT_HighCommandStatus.NO_AMMO))
			return Color.Orange;

		return Color.White;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The group id this row was populated from.
	string GetGroupId()
	{
		return m_sGroupId;
	}
}
