//! Player-placed waypoint location type for the Overthrow map system
//!
//! TWO SLOTS, ONE TYPE. This emits up to two records per map open, from the two client-local waypoint
//! fields on OVT_JobManagerComponent: m_vCurrentWaypoint (written by OVT_JobsContext.ShowOnMap:109)
//! and m_vRecruitWaypoint (written by OVT_RecruitsContext.ShowOnMap:489). They used to be a single
//! shared field, so "show job on map" and "show recruit on map" clobbered each other; splitting them
//! is what lets both markers exist at once (implementation.md G2/K1).
//!
//! CLIENT-LOCAL. Neither field is replicated or persisted, so these markers exist only on the machine
//! whose player asked for them. Nothing here writes anything - it is pure presentation, and adds no
//! RPC (K10).
//!
//! EMPTINESS TEST. A slot holding "0 0 0" means "no waypoint set" and is skipped, matching legacy
//! (OVT_MapIcons.c:780 tested the vector's truthiness before drawing its "waypoint" icon). The test is
//! spelled out component-wise here rather than relying on vector-to-bool coercion.
//!
//! ALWAYS VISIBLE. The conf entry sets m_fVisibilityZoom 0 because legacy inserted a range of 0 for
//! this icon (OVT_MapIcons.c:790) - a waypoint you asked for should not vanish when you zoom out.
[BaseContainerProps(), OVT_MapLocationTypeTitle()]
class OVT_MapLocationWaypoint : OVT_MapLocationType
{
	//! Which slot a record came from. Local to this type - one writer, one reader (see OVT_MapDataKeys).
	protected const string DATA_KEY_SLOT = "waypointSlot";
	protected const string SLOT_JOB = "job";
	protected const string SLOT_RECRUIT = "recruit";

	[Attribute(defvalue: "Job Waypoint", desc: "Display name for the waypoint set from the Jobs menu. Swap to #OVT-Map_JobWaypoint once the localization exports are regenerated.", category: "Waypoints")]
	protected string m_sJobWaypointName;

	[Attribute(defvalue: "Recruit Waypoint", desc: "Display name for the waypoint set from the Recruits menu. Swap to #OVT-Map_RecruitWaypoint once the localization exports are regenerated.", category: "Waypoints")]
	protected string m_sRecruitWaypointName;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Icon tint for the job waypoint. The map icon atlas is white glyphs on transparent drawn over a light topographic map, so this must stay DARK - a light tint renders invisible.", category: "Waypoints")]
	protected ref Color m_JobWaypointColor;

	[Attribute(defvalue: "0 0 0 1", UIWidgets.ColorPicker, desc: "Icon tint for the recruit waypoint. Must be dark and clearly distinguishable from the job tint.", category: "Waypoints")]
	protected ref Color m_RecruitWaypointColor;

	//------------------------------------------------------------------------------------------------
	//! Emits up to two records - one per waypoint slot that is set.
	//! Returns cleanly when the job manager has not resolved yet; an empty map beats a script error.
	//! \param[out] locations Array to append this type's records to
	override void PopulateLocations(array<ref OVT_MapLocationData> locations)
	{
		// The base class does not cache the job manager, and this is its only map consumer, so it is
		// resolved per populate (once per map open) rather than adding an eighth cached manager.
		OVT_JobManagerComponent jobs = OVT_Global.GetJobs();
		if (!jobs)
			return;

		AddWaypoint(locations, jobs.m_vCurrentWaypoint, m_sJobWaypointName, SLOT_JOB);
		AddWaypoint(locations, jobs.m_vRecruitWaypoint, m_sRecruitWaypointName, SLOT_RECRUIT);
	}

	//------------------------------------------------------------------------------------------------
	//! Appends one record for a waypoint slot, skipping the slot when it is unset.
	//! \param[out] locations Array to append to
	//! \param[in] waypoint World position held by the slot
	//! \param[in] name Display name for this slot
	//! \param[in] slot Slot identifier stored on the record, used to pick the tint
	protected void AddWaypoint(array<ref OVT_MapLocationData> locations, vector waypoint, string name, string slot)
	{
		if (IsWaypointUnset(waypoint))
			return;

		OVT_MapLocationData locationData = new OVT_MapLocationData(waypoint, name, ClassName());
		locationData.SetDataString(DATA_KEY_SLOT, slot);

		locations.Insert(locationData);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] waypoint Slot value to test
	//! \return True when the slot is "0 0 0", i.e. no waypoint has been set
	protected bool IsWaypointUnset(vector waypoint)
	{
		return waypoint[0] == 0 && waypoint[1] == 0 && waypoint[2] == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Shared info panel: which menu placed this marker.
	//!
	//! The two slots share one icon quad and differ only by tint, so the panel is where a colour-blind
	//! player finds out which is which. The line names the menu rather than repeating the marker's own
	//! name (already the panel header) because that is the actionable half: the same menu is where the
	//! marker gets moved or replaced.
	//! \param[in] location The record being described
	//! \param[in] rowsContainer The shared panel's rows container
	override protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)
	{
		if (!location || !rowsContainer)
			return;

		string slot = location.GetDataString(DATA_KEY_SLOT, SLOT_JOB);
		if (slot == SLOT_RECRUIT)
			AddInfoRow(rowsContainer, "", "#OVT-Map_Waypoint_RecruitDesc");
		else
			AddInfoRow(rowsContainer, "", "#OVT-Map_Waypoint_JobDesc");
	}

	//------------------------------------------------------------------------------------------------
	//! Distinct tint per slot so a job waypoint and a recruit waypoint are told apart at a glance -
	//! they share one icon quad, so colour is the only differentiator on the marker itself.
	//! \param[in] location The record being drawn
	//! \return Configured tint for this record's slot
	override Color GetIconColor(OVT_MapLocationData location)
	{
		if (!location)
			return m_JobWaypointColor;

		string slot = location.GetDataString(DATA_KEY_SLOT, SLOT_JOB);
		if (slot == SLOT_RECRUIT)
			return m_RecruitWaypointColor;
		else
			return m_JobWaypointColor;
	}
}
