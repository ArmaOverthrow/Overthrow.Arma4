[EntityEditorProps(category: "Overthrow/Managers", description: "Ambient town civilians, registered on the virtualization ambient seam")]
class OVT_CivilianAmbienceManagerComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Town civilian ambience - one ambient spawn source per town (implementation.md §3.2, §3.5).
//!
//! WHAT THIS REPLACES. Overthrow's oldest ad-hoc spawner: a per-town repeating 10 s job on
//! OVT_TownControllerComponent that polled proximity itself and, on a hit, built population x 0.1
//! one-man groups IN A SINGLE FRAME (a 283-population city: 28 groups and 140 waypoint entities at
//! once). It never pruned the dead, never deleted a waypoint, and despawned every town's crowd
//! whenever a QRF started anywhere. All of that is gone; this manager registers a source per town and
//! core's tick owns proximity, the anti-thrash band, the once-per-activation roll and the ≤3 entities
//! per tick fill.
//!
//! WHY A MANAGER AND NOT MORE CONTROLLER CODE (decision D5):
//!  - THE RECRUIT CHOKEPOINT NEEDS ONE GLOBAL ENTRY POINT. OVT_RecruitManagerComponent holds a
//!    character and must reach "whoever owns this civilian" without knowing about towns at all.
//!  - THE TOWN CONTROLLER RUNS ON CLIENTS TOO (its flag material is a deliberately local visual), and
//!    server-only ambient state has no business living there.
//!  - ONE AUTHORITATIVE TEARDOWN. The controller had no OnDelete at all; this has one, and every
//!    source it registered goes down with it.
//!
//! SERVER ONLY. The guard sits before any collection is allocated, so a client's copy of this
//! component holds nothing and every entry point below fails closed. Nothing here replicates, nothing
//! here is persisted (decision D9) - a despawn discards the crowd and the next approach re-rolls it.
//------------------------------------------------------------------------------------------------
class OVT_CivilianAmbienceManagerComponent : OVT_Component
{
	//------------------------------------------------------------------------------------------------
	// CONSTANTS
	//------------------------------------------------------------------------------------------------

	//! The authored source this manager binds per town. Must match m_sSourceName in
	//! Configs/Civilians/CivilianAmbience.conf exactly - the registry lookup is case-sensitive.
	static const string TOWN_SOURCE_NAME = "town_civilians";

	//! The authored PARKED VEHICLE source this manager binds per town (Phase 5). Same registry, same
	//! case-sensitive lookup. A registry without it simply produces towns with no parked cars - the
	//! civilian crowd is unaffected, which is what makes this phase droppable at runtime as well as in
	//! the plan.
	static const string TOWN_VEHICLE_SOURCE_NAME = "town_vehicles";

	//! Owner-key prefix handed to core, so a source can be recognised in a log without a handle.
	static const string OWNER_KEY_PREFIX = "town:";

	//! Owner-key prefix for the parked vehicle source, so the two sources of one town are told apart in
	//! core's logs.
	static const string VEHICLE_OWNER_KEY_PREFIX = "townveh:";

	//------------------------------------------------------------------------------------------------
	// MEMBER VARIABLES
	//------------------------------------------------------------------------------------------------

	//! townId -> that town's runtime source config. Allocated on the SERVER ONLY; null everywhere
	//! else, which is what makes every entry point below fail closed on a client.
	protected ref map<int, ref OVT_TownCivilianSourceConfig> m_mSources;

	//! townId -> the handle core gave us for that town's civilian source. A town present in m_mSources
	//! but ABSENT here is suspended (a QRF is being fought in it).
	protected ref map<int, int> m_mHandles;

	//! townId -> that town's runtime PARKED VEHICLE source config (Phase 5). Parallel to m_mSources in
	//! every respect: same lifetime, same activation, same suppression.
	protected ref map<int, ref OVT_TownVehicleSourceConfig> m_mVehicleSources;

	//! townId -> the handle core gave us for that town's vehicle source. A town present in
	//! m_mVehicleSources but ABSENT here is suspended, exactly as for civilians.
	protected ref map<int, int> m_mVehicleHandles;

	//! The town whose crowd is suppressed by the QRF being fought in it, or -1. At most one, because at
	//! most one QRF exists at a time.
	protected int m_iSuppressedTownId = -1;

	//------------------------------------------------------------------------------------------------
	// STATIC
	//------------------------------------------------------------------------------------------------

	static OVT_CivilianAmbienceManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The manager on the game mode, or null before the game mode exists / when the component
	//! is not on the prefab.
	//!
	//! ⚠ RE-RESOLVES ACROSS A WORLD, exactly like the virtualization manager's accessor: a static
	//! outlives a campaign, so a cached instance is only trusted while it still belongs to the game
	//! mode that exists NOW. The second campaign of a session can never be handed the first one's
	//! sources.
	static OVT_CivilianAmbienceManagerComponent GetInstance()
	{
		if (!GetGame())
			return s_Instance;

		BaseGameMode gameMode = GetGame().GetGameMode();

		// No game mode yet (very early init, or a world without one): answer with whatever OnPostInit
		// claimed rather than dropping a perfectly good instance.
		if (!gameMode)
			return s_Instance;

		IEntity gameModeEntity = gameMode;
		if (s_Instance && s_Instance.GetOwner() != gameModeEntity)
			s_Instance = null;

		if (!s_Instance)
			s_Instance = OVT_CivilianAmbienceManagerComponent.Cast(gameMode.FindComponent(OVT_CivilianAmbienceManagerComponent));

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server guard FIRST, allocation second (the virtualization manager's shape) - a client that
	//! allocated the maps would look initialised and silently accept activations that can never reach
	//! the authority.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		s_Instance = this;

		if (!Replication.IsServer())
			return;

		m_mSources = new map<int, ref OVT_TownCivilianSourceConfig>();
		m_mHandles = new map<int, int>();
		m_mVehicleSources = new map<int, ref OVT_TownVehicleSourceConfig>();
		m_mVehicleHandles = new map<int, int>();
	}

	//------------------------------------------------------------------------------------------------
	//! Called from OVT_OverthrowGameMode.EOnInit, after the virtualization manager's Init.
	//!
	//! Deliberately does no registering: towns are activated by OVT_TownManagerComponent, which runs
	//! later, and a source registered before its town exists would have no live population to read.
	//! \param[in] owner The game mode entity.
	void Init(IEntity owner)
	{
		if (!Replication.IsServer())
			return;

		Print("[Overthrow] CivilianAmbienceManager initialized", LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! Called from OVT_OverthrowGameMode.DoStartGame(), after the virtualization manager's.
	//!
	//! ⚠ EVERY TOWN IS ALREADY ACTIVATED BY THIS POINT. The town manager's PostGameStart() runs far
	//! earlier in DoStartGame() and calls ActivateTown() on every controller, so this is a reporting
	//! point, not a registration point. Registration is deliberately NOT retried here: doing it twice
	//! would give a town two crowds.
	void PostGameStart()
	{
		if (!Replication.IsServer())
			return;

		int sourceCount = 0;
		if (m_mHandles)
			sourceCount = m_mHandles.Count();

		Print(string.Format("[Overthrow] CivilianAmbience: %1 town civilian source(s) registered - nothing spawns until an observer is inside the spawn ring",
			sourceCount.ToString()), LogLevel.VERBOSE);

		// QRF locality (D6). Subscribed HERE rather than in Init() because DoStartGame() runs the
		// occupying faction manager's PostGameStart() before this one, so the invoker is guaranteed to
		// exist - and this is the one start path a restored campaign shares with a new one.
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_OnQRFTownChanged)
			occupying.m_OnQRFTownChanged.Insert(OnQRFTownChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Restart hygiene. A second campaign in the same session must inherit nothing: no per-town
	//! sources pointing at the previous world's towns, and no stale s_Instance.
	//!
	//! ⚠ DELIBERATELY DESTROYS NOTHING, mirroring core's own ClearAmbientState() decision verbatim.
	//! Ambient civilians are transient by construction and the world being torn down takes them
	//! anyway, while deleting entities from inside a teardown is how a shutdown turns into an error
	//! cascade. It also cannot leak: the virtualization manager's own OnDelete clears its ambient
	//! source map in the same teardown, which releases every instance registered from here.
	//!
	//! THE DESTRUCTIVE PATH IS DeactivateTown(), which a live town uses when its crowd really must go
	//! (its controller being deleted mid-session, and Phase 3's QRF suppression).
	override void OnDelete(IEntity owner)
	{
		// The QRF subscription is the one thing here that points at ANOTHER component, so it is the one
		// thing that has to be undone: a second campaign's manager must not find the first one's
		// callback still on an invoker that outlived it.
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_OnQRFTownChanged)
			occupying.m_OnQRFTownChanged.Remove(OnQRFTownChanged);

		m_iSuppressedTownId = -1;

		if (m_mHandles)
			m_mHandles.Clear();

		if (m_mSources)
			m_mSources.Clear();

		if (m_mVehicleHandles)
			m_mVehicleHandles.Clear();

		if (m_mVehicleSources)
			m_mVehicleSources.Clear();

		if (s_Instance == this)
			s_Instance = null;

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// TOWN ACTIVATION
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Gives one town an ambient civilian source. Called by OVT_TownControllerComponent.ActivateTown().
	//!
	//! REGISTRATION SPAWNS NOTHING - the source is dormant until core's tick finds an observer inside
	//! its spawn ring - which is exactly what makes it safe to do this for all 20 Eden towns at
	//! campaign start.
	//!
	//! Idempotent by town id: a controller activated twice gets one crowd, not two.
	//! \param[in] controller The town's controller, for its authored range. May be null.
	//! \param[in] town The town data. Its POPULATION IS NOT READ HERE - it is read live on every roll.
	//! \param[in] townId Index into the town manager's town list.
	//! \return The core ambient handle, or -1 when nothing was registered.
	int ActivateTown(OVT_TownControllerComponent controller, OVT_TownData town, int townId)
	{
		if (!Replication.IsServer())
			return -1;

		if (!m_mSources || !m_mHandles)
			return -1;

		if (!town || townId < 0)
			return -1;

		if (m_mHandles.Contains(townId))
			return m_mHandles.Get(townId);

		// A town SUSPENDED by a QRF has no handle but still has its source. Building a second one here
		// would give it two crowds the moment the battle ended.
		if (m_mSources.Contains(townId))
			return -1;

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (!virt)
		{
			Print("[Overthrow] CivilianAmbience: no virtualization manager - town civilians cannot be registered", LogLevel.WARNING);
			return -1;
		}

		// Parked cars are a SEPARATE source registered at the same moment (Phase 5). Done before the
		// civilian source and not gated on it: a registry that authors one but not the other must still
		// give a town whatever it did author, and a town with no `town_vehicles` entry is simply a town
		// with no parked cars.
		ActivateTownVehicles(virt, controller, town, townId);

		OVT_CivilianAmbienceConfig template = OVT_CivilianAmbienceConfig.Cast(virt.FindAmbientSourceConfig(TOWN_SOURCE_NAME));
		if (!template)
		{
			Print(string.Format("[Overthrow] CivilianAmbience: the ambient registry has no '%1' source of type OVT_CivilianAmbienceConfig - is m_AmbientRegistry pointed at Configs/Civilians/CivilianAmbience.conf? No town will have civilians",
				TOWN_SOURCE_NAME), LogLevel.WARNING);
			return -1;
		}

		OVT_TownCivilianSourceConfig source = BuildTownSource(template, controller, town, townId);

		int handle = virt.RegisterAmbientSource(source, town.location, OWNER_KEY_PREFIX + townId.ToString());
		if (handle == -1)
			return -1;

		m_mSources.Set(townId, source);
		m_mHandles.Set(townId, handle);

		return handle;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives one town an ambient PARKED VEHICLE source (Phase 5, T5.2).
	//!
	//! Same contract as the civilian source in every respect that matters: registration spawns nothing,
	//! it is idempotent by town id, and a town suspended by a QRF keeps its source object while losing
	//! its handle.
	//!
	//! Silent when the registry has no `town_vehicles` entry - the phase is droppable, and a registry
	//! that predates it must not fill the log with warnings on every town.
	//! \param[in] virt The virtualization manager (already resolved by the caller).
	//! \param[in] controller The town's controller, for its authored range. May be null.
	//! \param[in] town The town data.
	//! \param[in] townId Index into the town manager's town list.
	//! \return The core ambient handle, or -1 when nothing was registered.
	protected int ActivateTownVehicles(notnull OVT_VirtualizationManagerComponent virt, OVT_TownControllerComponent controller, notnull OVT_TownData town, int townId)
	{
		if (!m_mVehicleSources || !m_mVehicleHandles)
			return -1;

		if (m_mVehicleHandles.Contains(townId))
			return m_mVehicleHandles.Get(townId);

		if (m_mVehicleSources.Contains(townId))
			return -1;

		OVT_TownVehicleAmbienceConfig template = OVT_TownVehicleAmbienceConfig.Cast(virt.FindAmbientSourceConfig(TOWN_VEHICLE_SOURCE_NAME));
		if (!template)
			return -1;

		OVT_TownVehicleSourceConfig source = BuildTownVehicleSource(template, controller, town, townId);

		int handle = virt.RegisterAmbientSource(source, town.location, VEHICLE_OWNER_KEY_PREFIX + townId.ToString());
		if (handle == -1)
			return -1;

		m_mVehicleSources.Set(townId, source);
		m_mVehicleHandles.Set(townId, handle);

		return handle;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds ONE town's runtime parked-vehicle source from the authored template.
	//!
	//! Only the base-class fields core reads directly are set (decision D5) - the counts, the search
	//! ranges and the prefab pool are all read through the template at roll time. The radius is THIS
	//! TOWN'S range, for the same reason the civilian source's is: a shared radius would park a hamlet's
	//! cars in the next valley.
	//!
	//! Public because it is the seam the Init-tier case asserts.
	//! \param[in] template The authored declaration.
	//! \param[in] controller The town's controller, for its authored range. May be null.
	//! \param[in] town The town data.
	//! \param[in] townId Index into the town manager's town list.
	//! \return The bound instance, never null.
	OVT_TownVehicleSourceConfig BuildTownVehicleSource(notnull OVT_TownVehicleAmbienceConfig template, OVT_TownControllerComponent controller, notnull OVT_TownData town, int townId)
	{
		OVT_TownVehicleSourceConfig source = new OVT_TownVehicleSourceConfig();

		source.m_sSourceName = template.m_sSourceName;
		source.m_iMinCount = template.m_iMinCount;
		source.m_iMaxCount = template.m_iMaxCount;
		source.m_iSpawnDistanceOverride = template.m_iSpawnDistanceOverride;

		float radius = 0;
		if (controller)
			radius = controller.m_iTownRange;

		if (radius <= 0)
			radius = template.m_fRadius;

		source.m_fRadius = radius;
		source.SetTownLocation(town.location);
		source.Bind(template, townId, town.size);

		return source;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one town's parked cars down and forgets them.
	//!
	//! Unregistering deletes them through the normal despawn hook - which is also where an OCCUPIED car
	//! saves itself by being claimed instead of deleted.
	//! \param[in] townId The town to deactivate.
	//! \return True when a source was removed.
	protected bool DeactivateTownVehicles(int townId)
	{
		if (!m_mVehicleHandles)
			return false;

		int handle;
		if (!m_mVehicleHandles.Find(townId, handle))
		{
			// A suspended town has no handle but still has its source, and forgetting a town must forget
			// both halves.
			if (m_mVehicleSources)
				m_mVehicleSources.Remove(townId);

			return false;
		}

		m_mVehicleHandles.Remove(townId);

		if (m_mVehicleSources)
			m_mVehicleSources.Remove(townId);

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (virt)
			virt.UnregisterAmbientSource(handle);

		return true;
	}

	//! \param[in] townId The town to ask about.
	//! \return That town's runtime parked-vehicle source, or null when the town has none.
	OVT_TownVehicleSourceConfig GetTownVehicleSource(int townId)
	{
		if (!m_mVehicleSources)
			return null;

		return m_mVehicleSources.Get(townId);
	}

	//! \param[in] townId The town to ask about.
	//! \return The core ambient handle for that town's parked cars, or -1 when it has none.
	int GetTownVehicleHandle(int townId)
	{
		if (!m_mVehicleHandles)
			return -1;

		int handle;
		if (!m_mVehicleHandles.Find(townId, handle))
			return -1;

		return handle;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one town's crowd down and forgets it.
	//!
	//! Unregistering deletes the live civilians through the normal despawn hook - member characters,
	//! waypoints and group entities all - so there is no second teardown path to keep in step.
	//! Idempotent; safe on a client and safe after the virtualization manager has gone.
	//! \param[in] townId The town to deactivate.
	//! \return True when a source was removed.
	bool DeactivateTown(int townId)
	{
		// Both of this town's sources go together, and the cars go FIRST: they are the half that can
		// save itself (an occupied car claims itself out of the despawn), so nothing about the civilian
		// teardown should be able to interrupt it.
		DeactivateTownVehicles(townId);

		if (!m_mHandles)
			return false;

		int handle;
		if (!m_mHandles.Find(townId, handle))
			return false;

		m_mHandles.Remove(townId);

		if (m_mSources)
			m_mSources.Remove(townId);

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (virt)
			virt.UnregisterAmbientSource(handle);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes EVERY town's crowd down. Not used by the teardown path (see OnDelete) - it exists for a
	//! caller that really wants every town emptied while the campaign is still running.
	//! \return How many towns were deactivated.
	int DeactivateAllTowns()
	{
		if (!m_mHandles)
			return 0;

		// The keys are snapshotted first: DeactivateTown() removes from the very map being walked.
		array<int> townIds = new array<int>();
		for (int i = 0; i < m_mHandles.Count(); i++)
		{
			townIds.Insert(m_mHandles.GetKey(i));
		}

		int removed = 0;
		foreach (int townId : townIds)
		{
			if (DeactivateTown(townId))
				removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds ONE town's runtime source config from the authored template.
	//!
	//! ⚠ ONLY THE BASE-CLASS FIELDS CORE READS DIRECTLY ARE SET HERE (implementation.md §3.3).
	//! Everything else - the type pool, the archetypes, the population rate, the placement mix - is
	//! read THROUGH the template at roll time, so a template that gains an attribute later cannot go
	//! missing from a per-town instance. There is deliberately no field-copy method to drift.
	//!
	//! Public because it is the seam the Init-tier case asserts: "an instance built from the authored
	//! template exposes the template's numbers plus this town's radius".
	//! \param[in] template The authored declaration.
	//! \param[in] controller The town's controller, for its authored range. Null falls back to the
	//!        template's radius.
	//! \param[in] town The town data.
	//! \param[in] townId Index into the town manager's town list.
	//! \return The bound instance, never null.
	OVT_TownCivilianSourceConfig BuildTownSource(notnull OVT_CivilianAmbienceConfig template, OVT_TownControllerComponent controller, notnull OVT_TownData town, int townId)
	{
		OVT_TownCivilianSourceConfig source = new OVT_TownCivilianSourceConfig();

		source.m_sSourceName = template.m_sSourceName;
		source.m_iMinCount = template.m_iMinCount;
		source.m_iMaxCount = template.m_iMaxCount;
		source.m_iSpawnDistanceOverride = template.m_iSpawnDistanceOverride;

		// The scatter radius is THIS TOWN'S range, not the template's: an Eden city and a hamlet are
		// authored with very different ranges and a shared radius would put village civilians in the
		// next valley.
		float radius = 0;
		if (controller)
			radius = controller.m_iTownRange;

		if (radius <= 0)
			radius = template.m_fRadius;

		source.m_fRadius = radius;
		source.SetTownLocation(town.location);

		// The town's own curation list (Phase 4). Null/empty is "no restriction", so a town that authors
		// nothing behaves exactly as it did before the attribute existed.
		array<string> allowedNames;
		if (controller)
			allowedNames = controller.m_aCivilianTypes;

		array<ref OVT_CivilianTypeConfig> allowed = ResolveAllowedTypes(template, town.size, allowedNames);

		// A town that curated itself into nothing is a mis-authored world, and it is silent in play (an
		// empty town looks like an unlucky roll), so it is said out loud once at activation.
		if (allowed.IsEmpty())
			Print(string.Format("[Overthrow] CivilianAmbience: town %1 allows NO civilian type - its size permits none of the names in its m_aCivilianTypes list, so it will never have a crowd",
				townId.ToString()), LogLevel.WARNING);

		source.Bind(template, townId, town.size, allowed);

		return source;
	}

	//------------------------------------------------------------------------------------------------
	//! Which of the template's civilian types this settlement may roll.
	//!
	//! Two filters, both in the world-free helper: the type's own minimum settlement size, and the
	//! explicit per-town allow-list authored on OVT_TownControllerComponent.m_aCivilianTypes. Null or
	//! empty means "no restriction", never "nothing allowed" - every un-authored town carries one.
	//! \param[in] template The authored declaration.
	//! \param[in] townSize This settlement's size.
	//! \param[in] allowedNames The town's own allow-list, or null for "whatever the size permits".
	//! \return The eligible types, never null.
	array<ref OVT_CivilianTypeConfig> ResolveAllowedTypes(notnull OVT_CivilianAmbienceConfig template, OVT_TownSize townSize, array<string> allowedNames)
	{
		array<ref OVT_CivilianTypeConfig> allowed = new array<ref OVT_CivilianTypeConfig>();
		if (!template.m_aTypes)
			return allowed;

		foreach (OVT_CivilianTypeConfig type : template.m_aTypes)
		{
			if (!type)
				continue;

			if (!OVT_CivilianAmbienceMath.TypeAllowed(type.m_eMinTownSize, townSize, allowedNames, type.m_sTypeName))
				continue;

			allowed.Insert(type);
		}

		return allowed;
	}

	//------------------------------------------------------------------------------------------------
	// QRF LOCALITY (decision D6)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! ONE subscription, dispatching by town id.
	//!
	//! WHAT THIS RESTORES, AND WHAT IT DELIBERATELY DOES NOT. The retired spawner suppressed EVERY
	//! town's crowd whenever a QRF ran anywhere on the map (`&& !m_CurrentQRF`), which was a
	//! performance shortcut, not a design: a battle in one valley emptied the streets three towns away.
	//! Only the town actually under attack is suppressed now, and BASE QRFs are not published at all -
	//! they happen at bases, not in towns (D6).
	//!
	//! ⚠ SUPPRESSION IS OFF BY DEFAULT (decision D13, user amendment 2026-08-17). Civilians STAY during
	//! a town QRF unless the operator sets `despawnCiviliansDuringQRF` - players recruit civilians
	//! specifically to fight the QRF, and despawning them was a performance shortcut, not a design.
	//! The wiring exists in full either way, because a heavy server needs the AI budget back.
	//!
	//! ⚠ THE END-OF-BATTLE RESTORE DOES NOT CONSULT THE FLAG. An operator can edit the config file
	//! between the QRF starting and finishing, so what gets restored is whatever this manager actually
	//! suspended - tracked in m_iSuppressedTownId. Reading the flag at both ends would leave a town
	//! permanently empty (flag turned off mid-battle) or double-register one (flag turned on).
	//!
	//! ⚠ AT MOST ONE TOWN IS EVER SUPPRESSED, because at most one QRF exists at a time
	//! (StartTownQRF returns early while m_CurrentQRF is set). The single field below is therefore the
	//! whole state, and the "restore the previous one first" branch is belt and braces for a future
	//! that allows two.
	//! \param[in] townId The town now under QRF attack, or -1 when no town is.
	protected void OnQRFTownChanged(int townId)
	{
		if (!Replication.IsServer())
			return;

		if (townId >= 0 && townId == m_iSuppressedTownId)
			return;

		// Whatever WE suspended comes back, whatever the flag says now.
		if (m_iSuppressedTownId >= 0)
		{
			ResumeTown(m_iSuppressedTownId);
			m_iSuppressedTownId = -1;
		}

		if (townId < 0)
			return;

		if (!ShouldSuppressDuringQRF())
			return;

		if (SuspendTown(townId))
			m_iSuppressedTownId = townId;
	}

	//------------------------------------------------------------------------------------------------
	//! Does this server want a town's crowd gone while the town is being fought over?
	//! \return The operator's setting; false (civilians stay) when there is no config to ask.
	protected bool ShouldSuppressDuringQRF()
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config || !config.m_ConfigFile)
			return false;

		return config.m_ConfigFile.despawnCiviliansDuringQRF;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one town's crowd off the map WITHOUT forgetting how to rebuild it.
	//!
	//! Unregistering is the mechanism (it is what a despawn already is): core deletes the live
	//! civilians through the ordinary OnEntityDespawning hook - members, waypoints and husks - and the
	//! source object stays in m_mSources with its template, radius, allowed types and location intact.
	//! There is deliberately no "suppressed" flag inside the roll path: a source that is not registered
	//! cannot be ticked, so suppression cannot be half-applied.
	//! \param[in] townId The town to suppress.
	//! \return True when a registered source was suspended.
	bool SuspendTown(int townId)
	{
		if (!m_mHandles)
			return false;

		// The parked cars are suppressed with the crowd - a battle in the streets is exactly when a
		// server that asked for the AI budget back wants the scenery gone too. An OCCUPIED car still
		// claims itself out of the despawn hook, so a player who drove one into the fight keeps it.
		SuspendTownVehicles(townId);

		int handle;
		if (!m_mHandles.Find(townId, handle))
			return false;

		m_mHandles.Remove(townId);

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (virt)
			virt.UnregisterAmbientSource(handle);

		Print(string.Format("[Overthrow] CivilianAmbience: town %1 suppressed - its crowd is despawned for the duration of the battle", townId.ToString()), LogLevel.VERBOSE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a suspended town's source back on core's tick.
	//!
	//! THE CROWD IS NOT RESTORED, IT IS RE-ROLLED - the source comes back empty and the next approach
	//! rolls a fresh count against the town's LIVE population, which after a battle is not the number
	//! it was before one. That is the ambient contract (nothing survives a despawn) and it is also the
	//! honest outcome.
	//! \param[in] townId The town to bring back.
	//! \return True when a suspended source was re-registered.
	bool ResumeTown(int townId)
	{
		ResumeTownVehicles(townId);

		if (!m_mSources || !m_mHandles)
			return false;

		if (m_mHandles.Contains(townId))
			return false;

		OVT_TownCivilianSourceConfig source = m_mSources.Get(townId);
		if (!source)
			return false;

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (!virt)
			return false;

		int handle = virt.RegisterAmbientSource(source, source.GetTownLocation(), OWNER_KEY_PREFIX + townId.ToString());
		if (handle == -1)
			return false;

		m_mHandles.Set(townId, handle);

		Print(string.Format("[Overthrow] CivilianAmbience: town %1 un-suppressed - the next approach rolls a fresh crowd", townId.ToString()), LogLevel.VERBOSE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes one town's parked cars off the map without forgetting how to rebuild them.
	//! \param[in] townId The town to suppress.
	//! \return True when a registered vehicle source was suspended.
	protected bool SuspendTownVehicles(int townId)
	{
		if (!m_mVehicleHandles)
			return false;

		int handle;
		if (!m_mVehicleHandles.Find(townId, handle))
			return false;

		m_mVehicleHandles.Remove(townId);

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (virt)
			virt.UnregisterAmbientSource(handle);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a suspended town's parked cars back on core's tick. They are RE-ROLLED, not restored - the
	//! same ambient contract the crowd follows.
	//! \param[in] townId The town to bring back.
	//! \return True when a suspended vehicle source was re-registered.
	protected bool ResumeTownVehicles(int townId)
	{
		if (!m_mVehicleSources || !m_mVehicleHandles)
			return false;

		if (m_mVehicleHandles.Contains(townId))
			return false;

		OVT_TownVehicleSourceConfig source = m_mVehicleSources.Get(townId);
		if (!source)
			return false;

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (!virt)
			return false;

		int handle = virt.RegisterAmbientSource(source, source.GetTownLocation(), VEHICLE_OWNER_KEY_PREFIX + townId.ToString());
		if (handle == -1)
			return false;

		m_mVehicleHandles.Set(townId, handle);

		return true;
	}

	//! \return The town whose crowd is currently suppressed by a QRF, or -1 when none is.
	int GetSuppressedTownId()
	{
		return m_iSuppressedTownId;
	}

	//------------------------------------------------------------------------------------------------
	// RECRUITMENT
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Takes one recruited civilian out of ambient ownership, and cleans up what it leaves behind.
	//!
	//! THE ONE ENTRY POINT the recruit manager needs. It is called from RecruitCivilian() AFTER the
	//! recruit record exists and BEFORE the recruit is moved into its owner's group, which is the only
	//! window where both halves are true: a refused recruit has not released anything, and the group
	//! resolution still works (once the agent is reparented, the civilian's ambient group is no longer
	//! its parent and there is nothing left to find).
	//!
	//! ⚠ OnEntityPruned NEVER FIRES FOR A RELEASED ENTITY, so the crowd's leftovers - the ambient
	//! route's waypoints, and the group husk the recruit is about to leave - are cleaned up explicitly
	//! by the source, not by a hook.
	//!
	//! A SAFE NO-OP for null, for a character with no agent or no group, and for a character that was
	//! never ambient at all (a tent recruit) - core re-verifies the claim against the source's own
	//! entity list before honouring it.
	//! \param[in] character The freshly recruited civilian.
	//! \return True when the character was an ambient civilian and has been released.
	bool ReleaseRecruitedCivilian(SCR_ChimeraCharacter character)
	{
		if (!character)
			return false;

		if (!m_mSources)
			return false;

		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		if (!virt)
			return false;

		AIControlComponent aiControl = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (!aiControl)
			return false;

		AIAgent agent = aiControl.GetAIAgent();
		if (!agent)
			return false;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return false;

		if (!virt.ReleaseAmbientEntity(group))
			return false;

		// Which town's crowd owned it is not worth a second index: this runs once per recruit, and a
		// campaign has ~20 towns.
		EntityID groupId = group.GetID();
		for (int i = 0; i < m_mSources.Count(); i++)
		{
			OVT_TownCivilianSourceConfig source = m_mSources.GetElement(i);
			if (!source)
				continue;

			if (source.ReleaseCivilian(groupId))
				break;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// GETTERS
	//------------------------------------------------------------------------------------------------

	//! \return How many towns currently have a registered civilian source.
	int GetActiveTownCount()
	{
		if (!m_mHandles)
			return 0;

		return m_mHandles.Count();
	}

	//! \param[in] townId The town to ask about.
	//! \return That town's runtime source config, or null when the town has none.
	OVT_TownCivilianSourceConfig GetTownSource(int townId)
	{
		if (!m_mSources)
			return null;

		return m_mSources.Get(townId);
	}

	//! \param[in] townId The town to ask about.
	//! \return The core ambient handle for that town's civilians, or -1 when it has none.
	int GetTownHandle(int townId)
	{
		if (!m_mHandles)
			return -1;

		int handle;
		if (!m_mHandles.Find(townId, handle))
			return -1;

		return handle;
	}
}
