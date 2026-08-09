//------------------------------------------------------------------------------------------------
class OVT_MapMarkerManagerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Registry of every OVT_MapMarkerComponent in the world.
//!
//! Serves two map location types (bus stops and POIs) and gives map/fast-travel a single
//! GetNearestMarker API to build bus travel on.
//!
//! DELIBERATELY NOT REPLICATED. The world scan below runs on EVERY machine, outside any
//! Replication.IsServer() guard, because markers sit on static world entities that are identical
//! on every machine. This is the same arrangement OVT_EconomyManagerComponent uses for ports:
//! InitializePorts() runs from AfterInit() before the server guard, and m_aAllPorts is deliberately
//! absent from that component's RplSave. Adding replication here would cost bandwidth, introduce a
//! JIP ordering hazard and create a second source of truth for something the client can already see.
//!
//! Nothing here is persisted either - markers are discovered live at every session start, so a save
//! made before this component existed loads unchanged.
class OVT_MapMarkerManagerComponent: OVT_Component
{
	//------------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//------------------------------------------------------------------------------------------------

	//! Flat registry, all categories in one list. Elements are weak on purpose: the entity owns its
	//! component and unregisters it in OnDelete. (Vanilla's own component registries do the same,
	//! e.g. SCR_CampaignMilitaryBaseManager.m_aBases.)
	protected ref array<OVT_MapMarkerComponent> m_aMarkers;

	//------------------------------------------------------------------------------------------------
	// STATIC
	//------------------------------------------------------------------------------------------------

	static OVT_MapMarkerManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The singleton instance on the game mode, or null before the game mode exists
	static OVT_MapMarkerManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_MapMarkerManagerComponent.Cast(pGameMode.FindComponent(OVT_MapMarkerManagerComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \param[in] owner The game mode entity
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		s_Instance = this;
		m_aMarkers = new array<OVT_MapMarkerComponent>();

		if (SCR_Global.IsEditMode())
			return;

		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the one world scan, a frame after world init so streamed statics are present.
	//! \param[in] owner The game mode entity
	override void EOnInit(IEntity owner) //!EntityEvent.INIT
	{
		super.EOnInit(owner);

		if (!GetGame().InPlayMode())
			return;

		GetGame().GetCallqueue().CallLater(ScanWorldForMarkers, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Tears the registry down with the game mode.
	//!
	//! A world transition ("Continue" replaces the world in place) deletes the game mode entity and
	//! every world static in some order. Dropping the list and the cached instance here means a marker
	//! whose own OnDelete runs afterwards hits the guards in UnregisterMarker instead of walking a list
	//! that belongs to a world that no longer exists.
	//! \param[in] owner The game mode entity
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(ScanWorldForMarkers);

		m_aMarkers = null;

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// REGISTRATION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Adds a marker to the registry. Idempotent - registering the same marker twice is a no-op,
	//! which is what lets the world scan and component self-registration both run unconditionally.
	//! \param[in] marker The marker to register
	void RegisterMarker(OVT_MapMarkerComponent marker)
	{
		if (!marker)
			return;

		if (!m_aMarkers)
			m_aMarkers = new array<OVT_MapMarkerComponent>();

		if (m_aMarkers.Find(marker) != -1)
			return;

		m_aMarkers.Insert(marker);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a marker from the registry. Idempotent - unregistering something that was never
	//! registered is a no-op.
	//! \param[in] marker The marker to remove
	void UnregisterMarker(OVT_MapMarkerComponent marker)
	{
		if (!marker || !m_aMarkers)
			return;

		int index = m_aMarkers.Find(marker);
		if (index == -1)
			return;

		m_aMarkers.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	// QUERIES
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Every registered marker of one category.
	//! \param[in] category The category to filter on
	//! \return A new array, empty when nothing matches. Never null.
	array<OVT_MapMarkerComponent> GetMarkers(OVT_MapMarkerCategory category)
	{
		array<OVT_MapMarkerComponent> found = new array<OVT_MapMarkerComponent>();
		if (!m_aMarkers)
			return found;

		foreach (OVT_MapMarkerComponent marker : m_aMarkers)
		{
			if (!marker)
				continue;

			if (marker.GetCategory() != category)
				continue;

			found.Insert(marker);
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest marker of a category within a radius.
	//! \param[in] pos World position to measure from
	//! \param[in] category The category to filter on
	//! \param[in] maxDist Maximum distance in metres
	//! \return The nearest matching marker, or null when none is in range
	OVT_MapMarkerComponent GetNearestMarker(vector pos, OVT_MapMarkerCategory category, float maxDist)
	{
		if (!m_aMarkers)
			return null;

		OVT_MapMarkerComponent nearest = null;
		float nearestDist = maxDist;

		foreach (OVT_MapMarkerComponent marker : m_aMarkers)
		{
			if (!marker)
				continue;

			if (marker.GetCategory() != category)
				continue;

			IEntity owner = marker.GetOwner();
			if (!owner)
				continue;

			float dist = vector.Distance(owner.GetOrigin(), pos);
			if (dist > nearestDist)
				continue;

			nearestDist = dist;
			nearest = marker;
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many markers are registered, all categories
	int GetMarkerCount()
	{
		if (!m_aMarkers)
			return 0;

		return m_aMarkers.Count();
	}

	//------------------------------------------------------------------------------------------------
	// WORLD SCAN
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Finds and registers every world-placed marker.
	//!
	//! Runs on every machine - there is no Replication.IsServer() guard here and there must not be
	//! one. Shaped after OVT_EconomyManagerComponent.InitializePorts().
	//!
	//! The count is printed so it can be compared against a known world count during play-testing:
	//! a marker that never registers is otherwise a silent partial failure.
	protected void ScanWorldForMarkers()
	{
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckMarkerInit, FilterMarkerEntities, EQueryEntitiesFlags.STATIC);

		array<OVT_MapMarkerComponent> busStops = GetMarkers(OVT_MapMarkerCategory.BUS_STOP);
		array<OVT_MapMarkerComponent> pois = GetMarkers(OVT_MapMarkerCategory.POI);

		Print("[OVT_MapMarkerManagerComponent] World scan complete: " + GetMarkerCount() + " markers registered (" + busStops.Count() + " bus stops, " + pois.Count() + " POIs)", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Query callback used by ScanWorldForMarkers. Registers the marker on the entity.
	//! \param[in] entity The entity that passed the filter
	//! \return True to continue the query
	protected bool CheckMarkerInit(IEntity entity)
	{
		OVT_MapMarkerComponent marker = OVT_MapMarkerComponent.Cast(entity.FindComponent(OVT_MapMarkerComponent));
		if (marker)
			RegisterMarker(marker);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter used by ScanWorldForMarkers. Ensures the entity carries a marker component.
	//! \param[in] entity The entity to test
	//! \return True when the entity has a marker component, false otherwise
	protected bool FilterMarkerEntities(IEntity entity)
	{
		OVT_MapMarkerComponent marker = OVT_MapMarkerComponent.Cast(entity.FindComponent(OVT_MapMarkerComponent));
		if (marker) return true;

		return false;
	}
}
