[EntityEditorProps(category: "Overthrow/Deployments", description: "Deployment instance component")]
class OVT_DeploymentComponentClass : OVT_ComponentClass
{
}

class OVT_DeploymentComponent : OVT_Component
{
	[Attribute()]
	ref OVT_DeploymentConfig m_DeploymentConfig;
	
	protected ref array<ref OVT_BaseDeploymentModule> m_aActiveModules;
	protected int m_iControllingFaction;
	protected float m_fThreatLevel;
	protected int m_iResourcesInvested;
	protected bool m_bActive;
	protected vector m_vPosition;
	protected bool m_bSpawnedUnitsEliminated; // Flag to track if spawned units have been eliminated

	//! The deployment's virtualization identity: "<config name>@<round(x)>_<round(z)>[#<ordinal>]".
	//! Derived ONCE (EnsureVirtualKey) and persisted from then on, because it is what the groups this
	//! deployment registers are tagged with and therefore the only way to find them again after a
	//! load. Empty until first use, and empty on a deployment restored from a version 1 payload.
	protected string m_sVirtualKey;

	//! Whether this deployment arrived from a save point rather than from the evaluator.
	//!
	//! RUNTIME ONLY - SET IN ApplyPersistedDeployment, NEVER WRITTEN TO A PAYLOAD. It answers exactly
	//! one question, D7's: may my static-content modules BUILD anything? A restored deployment's
	//! compositions, checkpoints and parked vehicles are world entities that vanilla persistence has
	//! already brought back (OVT_PersistenceTracking / the vehicle manager), and their slot claims came
	//! back with the base controller's m_aSlotsFilled. A module that rebuilt on restore would give the
	//! base one more bunker per load, in a different slot each time, forever.
	//!
	//! It is deliberately NOT persisted: it describes how THIS session's copy of the deployment came
	//! into being, and a deployment that is saved again is restored again, so the answer is re-derived
	//! every time it matters. Persisting it would also be a serializer field, and the serializer's
	//! version and field order are frozen for this feature.
	protected bool m_bRestoredFromSave;

	static const int UPDATE_FREQUENCY = 10000; // 10 seconds
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
			
		m_aActiveModules = new array<ref OVT_BaseDeploymentModule>;
		m_vPosition = owner.GetOrigin();
	}
	
	//------------------------------------------------------------------------------------------------
	void InitializeDeployment(OVT_DeploymentConfig config, int factionIndex)
	{
		if (!config)
			return;
			
		m_DeploymentConfig = config;
		m_iControllingFaction = factionIndex;
		
		// Register with manager
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
			manager.RegisterDeployment(this);
		
		// Create module instances from config
		foreach (OVT_BaseDeploymentModule moduleTemplate : config.m_aModules)
		{
			OVT_BaseDeploymentModule module = OVT_BaseDeploymentModule.Cast(moduleTemplate.CloneModule());
			if (module)
			{
				AddModule(module);
			}
		}
		
		// If this deployment was loaded with spawned units eliminated, set all spawning modules as eliminated
		if (m_bSpawnedUnitsEliminated)
		{
			array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
			foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
			{
				spawningModule.SetSpawnedUnitsEliminated(true);
				OVT_DeploymentLog.Debug(string.Format("Set spawning module as eliminated on load for deployment '%1'", GetDeploymentName()));
			}
		}
		
		// Start update loop
		float mul = s_AIRandomGenerator.RandFloatXY(0.8, 1.2);
		int frequency = (int)((float)UPDATE_FREQUENCY * mul); //Stagger these updates
		GetGame().GetCallqueue().CallLater(UpdateDeployment, frequency, true);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Brings a deployment respawned from a save point back to life.
	//!
	//! Called from OVT_DeploymentComponentSerializer.Deserialize(), which is a pure codec - every
	//! side effect of restoring a deployment lives here.
	//!
	//! ORDER MATTERS. The scalars, and m_bSpawnedUnitsEliminated in particular, are written BEFORE
	//! InitializeDeployment() because that method reads the flag to decide whether the modules it has
	//! just cloned start out eliminated. EPF's OVT_DeploymentComponentSaveData.ApplyTo() set it
	//! afterwards, so a deployment whose force had already been wiped out came back with a fresh one
	//! waiting to spawn.
	//!
	//! IDEMPOTENT. m_DeploymentConfig is only ever set by InitializeDeployment(), so it doubles as
	//! the "already built" flag: a second application - which is what
	//! OVT_PersistenceManagerComponent.ReapplyLatestSaveData() does to a live session - refreshes the
	//! scalars and returns without cloning a second set of modules, registering with the manager
	//! twice or starting a second update loop.
	//!
	//! SPAWNS NOTHING DIRECTLY. The deployment's units are virtualized: the spawning modules create
	//! them when a player comes into range and delete them again when nobody is near, which is
	//! exactly why the units themselves are not persisted.
	//! \param[in] configName Name of the OVT_DeploymentConfig this deployment was running.
	//! \param[in] factionIndex Faction that owns the deployment.
	//! \param[in] threatLevel Threat level it was created at.
	//! \param[in] resourcesInvested Resources spent on it so far.
	//! \param[in] spawnedUnitsEliminated Whether its force had already been wiped out.
	//! \param[in] virtualKey The virtualization key its registered groups are tagged with. EMPTY for a
	//!            version 1 payload, which was written before deployments carried one.
	//! \param[in] seededAtGameStart Whether this deployment was put down by the free-at-game-start pass.
	//!            FALSE for a version 1 or 2 payload, which is the safe reading - it only ever relaxes
	//!            two rules for a founding force (see WasSeededAtGameStart).
	void ApplyPersistedDeployment(string configName, int factionIndex, float threatLevel, int resourcesInvested, bool spawnedUnitsEliminated, string virtualKey, bool seededAtGameStart = false)
	{
		if (!Replication.IsServer())
			return;

		if (!m_aActiveModules)
			m_aActiveModules = new array<ref OVT_BaseDeploymentModule>;

		// D7's gate, set BEFORE anything else and on every branch including the already-running one:
		// this deployment's static content came out of the save, so no module may build any of it.
		m_bRestoredFromSave = true;

		m_iControllingFaction = factionIndex;
		m_fThreatLevel = threatLevel;
		m_iResourcesInvested = resourcesInvested;
		m_bSpawnedUnitsEliminated = spawnedUnitsEliminated;
		m_bSeededAtGameStart = seededAtGameStart;

		// The SAVED key wins over anything this session could derive: it is the string the groups in
		// the registry are actually tagged with, and re-deriving it would only agree by luck once the
		// marker has been restored a metre off. An EMPTY key means a version 1 payload - leave the
		// field alone and let EnsureVirtualKey() derive one from the restored marker on first use,
		// which is exactly the pre-feature-save migration path (D6).
		if (!virtualKey.IsEmpty())
			m_sVirtualKey = virtualKey;

		if (m_DeploymentConfig)
		{
			// Already running. Only the wipe-out flag can have moved on the modules, so put that back
			// in step and leave everything else alone.
			if (m_bSpawnedUnitsEliminated)
			{
				array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
				foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
				{
					spawningModule.SetSpawnedUnitsEliminated(true);
				}
			}

			return;
		}

		if (configName.IsEmpty())
		{
			Print("[Overthrow] A saved deployment has no configuration name and cannot be restored", LogLevel.WARNING);
			return;
		}

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager || !manager.m_DeploymentRegistry)
		{
			Print(string.Format("[Overthrow] Cannot restore deployment '%1' - there is no deployment registry", configName), LogLevel.ERROR);
			return;
		}

		OVT_DeploymentConfig config = manager.m_DeploymentRegistry.FindConfigByName(configName);
		if (!config)
		{
			// An authored config that has been renamed or removed since the save. Dropping the
			// deployment is the honest outcome; EvaluateDeployments() will spend the faction's
			// resources on something that still exists.
			Print(string.Format("[Overthrow] Saved deployment '%1' no longer exists in the registry - it will not be restored", configName), LogLevel.WARNING);
			return;
		}

		InitializeDeployment(config, factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	void ActivateDeployment()
	{
		if (m_bActive)
			return;
			
		m_bActive = true;
		
		// Activate modules in the correct order: spawning → behavior → condition
		
		// First: Activate spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			module.Activate();
		}
		
		// Second: Activate behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			module.Activate();
		}
		
		// Third: Activate condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			module.Activate();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! THERE IS NO "TURN THE WHOLE DEPLOYMENT BACK OFF" METHOD ANY MORE, AND THERE MUST NOT BE ONE.
	//!
	//! One used to run every 10 s the moment the last player walked outside 1750 m, and it deleted every
	//! soldier this deployment had put in the world. That is the ad-hoc virtualization this framework
	//! was migrated off: a patrol shot down to one man came back at full strength as soon as you left
	//! and returned, because the whole force was rebuilt from the config rather than remembered.
	//!
	//! What replaced it is the engine's own observer-driven group lifecycle, which materialises and
	//! despawns registered groups without deleting the records behind them. The teardown that DOES
	//! still exist is DestroyDeployment() -> module Cleanup() -> UnregisterGroup(), reached only when a
	//! deployment is genuinely over.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	protected void UpdateDeployment()
	{
		if (!Replication.IsServer())
			return;

		// AN EMPTY SERVER TICKS NOBODY (author, 2026-08-23). The deployment MANAGER's guard only ever
		// blocked CREATION - see EvaluateDeployments() - so every deployment already standing kept
		// running its modules on an empty server: reinforcement rebuys spending whatever was left in the
		// pool, patrol behaviours, condition modules, teardown polls. Nothing could be watching any of
		// it, because nothing materialises without an observer.
		//
		// ⚠ EVERY TIMER IN HERE IS EITHER WORLD-TIME OR A TICK COUNT, so a skipped stretch is safe both
		// ways: a world-time comparison (a reinforcement cooldown, an activation delay) simply reads as
		// long expired when players return, and a tick counter (the insertion module's stuck and
		// arrival counters) is paused rather than banked.
		PlayerManager players = GetGame().GetPlayerManager();
		if (players && players.GetPlayerCount() == 0)
			return;

		// Safety check to prevent crashes during cleanup
		if (!m_aActiveModules)
			return;
			
		// Update modules in order: spawning → behavior → condition
		
		// First: Update spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (module)
				module.Update(UPDATE_FREQUENCY);
		}
		
		// Second: Update behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			if (module)
				module.Update(UPDATE_FREQUENCY);
		}
		
		// Third: Update condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			if (module)
				module.Update(UPDATE_FREQUENCY);
		}
		
		// ACTIVATE ONCE, AND NEVER TOGGLE. Where this used to poll player proximity and flip the whole
		// deployment on and off, its groups are now registered with the virtualization core and the
		// engine decides on its own when to put men on the ground near an observer and when to take
		// them away again - without ever forgetting who died. Activation is what starts the modules
		// ticking, so it happens as soon as the deployment does and stays.
		if (!m_bActive)
			ActivateDeployment();
	}

	//------------------------------------------------------------------------------------------------
	void ReinforceDeployment(int additionalResources)
	{
		m_iResourcesInvested += additionalResources;
		
		// Distribute resources to spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		if (spawningModules.IsEmpty())
			return;
			
		int resourcesPerModule = additionalResources / spawningModules.Count();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			// TODO: Implement reinforcement logic
		}
	}

	//------------------------------------------------------------------------------------------------
	// Virtualization
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The deployment's virtualization key, derived on first use and remembered from then on.
	//!
	//! DERIVE ONCE, NOT EVERY TIME. The key is built from the marker's position, and a key that
	//! silently changed under a moved marker would make every group this deployment registered
	//! unreachable - the next reclaim would find nothing and register the whole force a second time on
	//! top of the one already standing there. Deriving once and persisting the answer (serializer
	//! version 2) is what makes the key immune to that.
	//!
	//! Collisions - the same config on the same rounded spot - are separated by an ordinal the
	//! deployment manager hands out at derivation time.
	//! \return The key; never empty once there is a marker to derive it from.
	string EnsureVirtualKey()
	{
		if (!m_sVirtualKey.IsEmpty())
			return m_sVirtualKey;

		string configName;
		if (m_DeploymentConfig)
			configName = m_DeploymentConfig.m_sDeploymentName;

		// The marker's live origin, falling back to the position captured at OnPostInit for the window
		// where the owner is not resolvable.
		vector position = m_vPosition;
		IEntity marker = GetOwner();
		if (marker)
			position = marker.GetOrigin();

		string baseKey = OVT_DeploymentVirtualKey.DeriveKey(configName, position[0], position[2]);

		int ordinal = 0;
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
			ordinal = manager.NextKeyOrdinal(baseKey);

		m_sVirtualKey = OVT_DeploymentVirtualKey.Disambiguate(baseKey, ordinal);

		return m_sVirtualKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Brings every spawning module's registered groups up to the count it wants, without ever
	//! spawning anything twice.
	//!
	//! ALWAYS SAFE TO CALL, in any order and any number of times: each module reclaims what it already
	//! owns before it registers anything, so activation, a restore and a reinforcement all converge on
	//! the same set of groups. That is why the fan-out is written as "converge to wanted" rather than
	//! "spawn wanted".
	void EnsureGroups()
	{
		if (!Replication.IsServer())
			return;

		if (!m_aActiveModules)
			return;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (module)
				module.EnsureGroups();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tells every spawning module that one registered group has been wiped out.
	//!
	//! Fanned out from the deployment manager, which is the ONE subscriber to the virtualization
	//! registry's wipe event: modules are cloned config objects with no stable lifetime, so a
	//! subscription held by one is a dangling call waiting for the deployment to be deleted (D7).
	//! A module that does not own the handle ignores it.
	//! \param[in] handle The wiped group's registry handle.
	void OnVirtualGroupWiped(int handle)
	{
		if (!Replication.IsServer())
			return;

		if (!m_aActiveModules)
			return;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (module)
				module.OnVirtualGroupWiped(handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	void DestroyDeployment()
	{
		// Stop update loop
		GetGame().GetCallqueue().Remove(UpdateDeployment);
		
		// Cleanup modules in reverse order: condition → behavior → spawning
		
		// First: Cleanup condition modules
		array<OVT_BaseConditionDeploymentModule> conditionModules = GetConditionModules();
		foreach (OVT_BaseConditionDeploymentModule module : conditionModules)
		{
			module.Cleanup();
		}
		
		// Second: Cleanup behavior modules
		array<OVT_BaseBehaviorDeploymentModule> behaviorModules = GetBehaviorModules();
		foreach (OVT_BaseBehaviorDeploymentModule module : behaviorModules)
		{
			module.Cleanup();
		}
		
		// Third: Cleanup spawning modules
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			module.Cleanup();
		}
		
		m_aActiveModules.Clear();
		
		// Unregister from manager
		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (manager)
			manager.UnregisterDeployment(this);
			
		// Delete the entity
		delete GetOwner();
	}
	
	//------------------------------------------------------------------------------------------------
	// Module management
	//------------------------------------------------------------------------------------------------
	void AddModule(OVT_BaseDeploymentModule module)
	{
		if (!module)
			return;
			
		m_aActiveModules.Insert(module);
		module.Initialize(this);
	}
	
	//------------------------------------------------------------------------------------------------
	void RemoveModule(typename moduleType)
	{
		for (int i = m_aActiveModules.Count() - 1; i >= 0; i--)
		{
			if (m_aActiveModules[i].Type() == moduleType)
			{
				m_aActiveModules[i].Cleanup();
				m_aActiveModules.Remove(i);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	OVT_BaseDeploymentModule GetModule(typename moduleType)
	{
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			if (module.Type() == moduleType)
				return module;
		}
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	// Module type queries
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseConditionDeploymentModule> GetConditionModules()
	{
		array<OVT_BaseConditionDeploymentModule> modules = new array<OVT_BaseConditionDeploymentModule>;
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(module);
			if (conditionModule)
				modules.Insert(conditionModule);
		}
		return modules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseSpawningDeploymentModule> GetSpawningModules()
	{
		array<OVT_BaseSpawningDeploymentModule> modules = new array<OVT_BaseSpawningDeploymentModule>;
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			OVT_BaseSpawningDeploymentModule spawningModule = OVT_BaseSpawningDeploymentModule.Cast(module);
			if (spawningModule)
				modules.Insert(spawningModule);
		}
		return modules;
	}
	
	//------------------------------------------------------------------------------------------------
	array<OVT_BaseBehaviorDeploymentModule> GetBehaviorModules()
	{
		array<OVT_BaseBehaviorDeploymentModule> modules = new array<OVT_BaseBehaviorDeploymentModule>;
		foreach (OVT_BaseDeploymentModule module : m_aActiveModules)
		{
			OVT_BaseBehaviorDeploymentModule behaviorModule = OVT_BaseBehaviorDeploymentModule.Cast(module);
			if (behaviorModule)
				modules.Insert(behaviorModule);
		}
		return modules;
	}
	
	//------------------------------------------------------------------------------------------------
	// Used by deployment manager to check if deployment should be created
	static bool CheckDeploymentConditions(OVT_DeploymentConfig config, vector position, int factionIndex, float threatLevel)
	{
		if (!config)
			return false;
			
		// Check threat level requirement
		if (config.m_iMinimumThreatLevel > 0 && threatLevel < config.m_iMinimumThreatLevel)
			return false;
		
		// Check module-specific conditions
		foreach (OVT_BaseDeploymentModule moduleTemplate : config.m_aModules)
		{
			OVT_BaseConditionDeploymentModule conditionModule = OVT_BaseConditionDeploymentModule.Cast(moduleTemplate);
			if (conditionModule && !conditionModule.EvaluateStaticCondition(position, factionIndex, threatLevel))
				return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	protected static OVT_FactionType GetFactionType(Faction faction)
	{
		// Determine faction type based on faction key
		string factionKey = faction.GetFactionKey();
		
		if (factionKey == OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey())
			return OVT_FactionType.OCCUPYING_FACTION;
		else if (factionKey == OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey())
			return OVT_FactionType.RESISTANCE_FACTION;
		else if (factionKey == OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey())
			return OVT_FactionType.SUPPORTING_FACTION;
		
		return 0;
	}
	
	//------------------------------------------------------------------------------------------------
	// Getters
	//------------------------------------------------------------------------------------------------
	int GetControllingFaction() { return m_iControllingFaction; }
	float GetThreatLevel() { return m_fThreatLevel; }
	int GetResourcesInvested() { return m_iResourcesInvested; }
	bool IsDeploymentActive() { return m_bActive; }
	vector GetPosition() { return GetOwner().GetOrigin(); }
	//! Which way this deployment's marker was created facing, in degrees.
	//!
	//! ⚠ GetYawPitchRoll()[0], NOT GetAngles()[0]. The two engine angle APIs use different orders and
	//! GetAngles() puts PITCH in slot 0, so the wrong one answers ~0 on every marker on flat ground and
	//! looks like it works. See OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation.
	//!
	//! ZERO IS THE ANSWER FOR ALMOST EVERY DEPLOYMENT and means "unrotated", not "unset" - see
	//! OVT_DeploymentManagerComponent.CreateDeployment for which caller passes a heading and why.
	float GetYaw() { return GetOwner().GetYawPitchRoll()[0]; }
	OVT_DeploymentConfig GetConfig() { return m_DeploymentConfig; }
	//! The virtualization key AS IT STANDS - empty until EnsureVirtualKey() has derived one. Read this
	//! when the answer "not derived yet" matters (the serializer, a collision probe); call
	//! EnsureVirtualKey() when a key is actually needed.
	string GetVirtualKey() { return m_sVirtualKey; }
	//! Whether this deployment came from a save point (D7). Read by any spawning module that puts
	//! ENTITIES rather than groups in the world: false means "you own building this", true means
	//! "vanilla persistence has already brought yours back - build nothing". Never persisted; see the
	//! member's header.
	bool WasRestoredFromSave() { return m_bRestoredFromSave; }
	string GetDeploymentName()
	{ 
		if (m_DeploymentConfig)
			return m_DeploymentConfig.m_sDeploymentName;
		return "Unknown Deployment";
	}
	
	//------------------------------------------------------------------------------------------------
	void SetThreatLevel(float threat) { m_fThreatLevel = threat; }
	//! Absolute set (not an increment - see ReinforceDeployment for that). Stamped at creation time.
	void SetResourcesInvested(int resources) { m_iResourcesInvested = resources; }
	void SetControllingFaction(int factionIndex) { m_iControllingFaction = factionIndex; }
	
	//------------------------------------------------------------------------------------------------
	void CheckAllSpawningModulesEliminated()
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = GetSpawningModules();
		if (spawningModules.IsEmpty())
		{
			// No spawning modules, so nothing to eliminate
			m_bSpawnedUnitsEliminated = false;
			return;
		}
		
		// Check if ALL spawning modules have been eliminated
		bool allEliminated = true;
		foreach (OVT_BaseSpawningDeploymentModule module : spawningModules)
		{
			if (!module.AreSpawnedUnitsEliminated())
			{
				allEliminated = false;
				break;
			}
		}
		
		bool previousState = m_bSpawnedUnitsEliminated;
		m_bSpawnedUnitsEliminated = allEliminated;		
	}
	
	//------------------------------------------------------------------------------------------------
	bool GetSpawnedUnitsEliminated() { return m_bSpawnedUnitsEliminated; }
	void SetSpawnedUnitsEliminated(bool eliminated) { m_bSpawnedUnitsEliminated = eliminated; }

	//------------------------------------------------------------------------------------------------
	//! WHETHER THIS DEPLOYMENT CAME FROM THE FREE-AT-GAME-START PASS rather than from a purchase.
	//!
	//! WHAT READS IT: OVT_InfantrySpawningDeploymentModule.EnsureGroups(), to decide whether to honour
	//! m_bSpawnAtNearestBase. Author's rule, 2026-08-20: *"they shouldnt spawn at nearest base at game
	//! start (if free at game start = true they should spawn in the town)"*. A campaign that opens with
	//! every town's patrol walking in from a base opens with every town empty, which is not what a map
	//! the occupying faction has held for years should look like; a patrol BOUGHT mid-campaign is
	//! reinforcement arriving from somewhere real, and should travel.
	//!
	//! ⚠ IT IS NOT config.m_bFreeAtGameStart AND CANNOT BE DERIVED FROM IT. That flag is a property of
	//! the CONFIG, and the same config is bought again by the evaluator later in the campaign - when the
	//! force SHOULD come from a base. This is a property of THIS INSTANCE: how it came into being.
	//!
	//! ⚠ NOT PERSISTED, deliberately. On load, spawning modules RECLAIM their groups by owner key rather
	//! than registering new ones, so nothing re-reads this for a restored deployment; a save taken after
	//! game start therefore has nothing to remember. The cost of being wrong is one patrol walking in
	//! from a base after a load rather than appearing in its town, which is the same outcome as the
	//! evaluator having bought it.
	//! \return True when the free-at-game-start pass created this deployment.
	bool WasSeededAtGameStart() { return m_bSeededAtGameStart; }
	void SetSeededAtGameStart(bool seeded) { m_bSeededAtGameStart = seeded; }

	protected bool m_bSeededAtGameStart;
}