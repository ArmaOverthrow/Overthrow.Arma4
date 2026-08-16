//------------------------------------------------------------------------------------------------
//! One declarative ambient spawn source (implementation.md §3.4, api.md §4).
//!
//! WHAT AN AMBIENT SOURCE IS. A position that should be surrounded by loose, disposable entities
//! (town civilians, parked vehicles) whenever anybody is near it, and by nothing at all when nobody
//! is. Ambient entities are NOT groups, so the engine's group lifecycle cannot drive them - the
//! virtualization manager owns the one tick core still has, and that tick asks the engine's
//! ObserversSystem rather than looping players.
//!
//! NOTHING AMBIENT IS EVER PERSISTED, BY CONSTRUCTION. A despawn discards everything: the entities
//! are deleted and the roll is forgotten, so the next approach RE-ROLLS from this config. Do not put
//! state on a subclass expecting it to survive a despawn, and never hand an ambient entity a job
//! that has to outlive one - transfer it out with
//! OVT_VirtualizationManagerComponent.ReleaseAmbientEntity() first, which is exactly what a
//! civilian-recruitment or vehicle-theft path does.
//!
//! THE MODDER SEAM. The five methods at the bottom are the extension point: subclass this, override
//! what you need, reference the subclass from a .conf, and no core code changes. Core ships ZERO
//! authored sources - `civilians` authors the content.
//!
//! SERVER ONLY. Only the authority ever constructs entities from one of these; the manager's server
//! guard is the gate, so nothing here re-checks it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sSourceName")]
class OVT_AmbientSpawnSourceConfig : ScriptAndConfig
{
	[Attribute(desc: "Unique name for this source type")]
	string m_sSourceName;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Prefabs this source may roll", params: "et")]
	ref array<ResourceName> m_aPrefabs;

	[Attribute(defvalue: "1", desc: "Minimum entities to spawn per activation")]
	int m_iMinCount;

	[Attribute(defvalue: "1", desc: "Maximum entities to spawn per activation (INCLUSIVE)")]
	int m_iMaxCount;

	[Attribute(defvalue: "100", desc: "Scatter radius around the source position")]
	float m_fRadius;

	[Attribute(defvalue: "-1", desc: "Spawn distance override in metres; -1 uses the configured global (virtualizationSpawnDistance). 0 means this source never materialises by proximity")]
	int m_iSpawnDistanceOverride;

	//------------------------------------------------------------------------------------------------
	void OVT_AmbientSpawnSourceConfig()
	{
		if (!m_aPrefabs)
			m_aPrefabs = new array<ResourceName>();
	}

	//------------------------------------------------------------------------------------------------
	// MODDER SEAM - override these in a subclass, no core change required
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Picks the prefab for ONE entity. Called once per entity, so a mixed source (three civilian
	//! bodies, one dog) needs no extra machinery - just weight the roll here.
	//! \return A prefab, or ResourceName.Empty when this source cannot produce one (the manager then
	//!         stops the activation rather than spawning nothing forever).
	ResourceName RollPrefab()
	{
		if (!m_aPrefabs || m_aPrefabs.IsEmpty())
			return ResourceName.Empty;

		if (m_aPrefabs.Count() == 1)
			return m_aPrefabs[0];

		// RandInt is MAX-EXCLUSIVE, so Count() is the right bound - Count() - 1 would never pick the
		// last prefab in the list.
		return m_aPrefabs[s_AIRandomGenerator.RandInt(0, m_aPrefabs.Count())];
	}

	//------------------------------------------------------------------------------------------------
	//! How many entities this source materialises for ONE activation. Rolled once when the source
	//! activates - never per tick - and then spent across ticks by the manager.
	//!
	//! ⚠ Goes through RollCountSafe because RandInt is max-exclusive AND RandInt(n, n) raises an
	//! engine error: a source authored with min == max would otherwise error every activation.
	//! \return An entity count in [m_iMinCount, m_iMaxCount], never negative.
	int RollCount()
	{
		int rolled = OVT_VirtualizationMath.RollCountSafe(m_iMinCount, m_iMaxCount);
		if (rolled < 0)
			return 0;

		return rolled;
	}

	//------------------------------------------------------------------------------------------------
	//! Where ONE entity goes. The default scatters uniformly inside the radius, keeps the point on the
	//! map and refuses ocean, using the tree's existing idiom (OVT_WorldUtils) so an ambient civilian
	//! cannot be dropped in the sea or off the terrain.
	//! \param[in] origin The source position.
	//! \param[in] radius The scatter radius.
	//! \return A world position with ground under it; the origin itself when no dry point was found.
	vector RollPosition(vector origin, float radius)
	{
		if (radius <= 0)
			return origin;

		return OVT_WorldUtils.GetRandomNonOceanPositionNear(origin, radius);
	}

	//------------------------------------------------------------------------------------------------
	//! Called immediately after each entity is spawned and recorded. THE dressing hook: clothes,
	//! loadouts, waypoints, flags, faction assignment.
	//!
	//! The entity is already owned by the source when this runs, so an override that deletes it must
	//! go through ReleaseAmbientEntity() first - otherwise the source's next despawn walks a dead ref
	//! (the manager prunes those, but the entity would still be counted until it did).
	//! \param[in] entity The freshly spawned entity.
	//! \param[in] sourcePosition The source's position (NOT the entity's - the scatter already moved it).
	void OnEntitySpawned(IEntity entity, vector sourcePosition)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Called for each still-live entity immediately before the source deletes it. Last chance to
	//! detach anything that points at it.
	//! \param[in] entity The entity about to be deleted.
	void OnEntityDespawning(IEntity entity)
	{
	}
}
