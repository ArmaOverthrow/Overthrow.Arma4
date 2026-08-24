//------------------------------------------------------------------------------------------------
//! CREW-UP ON ALARM: when a battle starts near this base, its parked armour is handed off to a
//! mounted sortie deployment instead of sitting uncrewed for the rest of the fight.
//!
//! SHIPPED BY: the "Base Parked Armour" registry delta on Deployment_BaseParkedVehicles.conf
//! (occupying/vehicles Phase 5), alongside OVT_ParkedVehicleSpawningDeploymentModule.
//!
//! ==========================================================================================
//! 🔴 THE OWNERSHIP-TRANSFER CONTRACT, AND THE DOUBLE-DELETE HAZARD IT EXISTS TO AVOID.
//! ==========================================================================================
//! A parked vehicle and a mounted force are two DIFFERENT modules, each of which deletes what it
//! believes it owns at its own teardown. Handing the same Vehicle to both at once is a double free
//! waiting to happen; handing it to NEITHER (a window where the parked module has already let go and
//! the sortie has not yet taken hold) orphans it. This module's whole design is one rule: release
//! ONLY AFTER acceptance is confirmed.
//!
//!   1. resolve the parked vehicle through OVT_ParkedVehicleSpawningDeploymentModule
//!      .GetSpawnedEntities() - it is still owned there;
//!   2. stand up the sortie deployment and find its OVT_MountedForceSpawningDeploymentModule;
//!   3. call AdoptVehicle() on THAT module. Only once it returns true does the sortie own the hull;
//!   4. only THEN call OVT_ParkedVehicleSpawningDeploymentModule.ReleaseVehicleOwnership() - which
//!      removes the id from the parked module's own list, so its GetSpawnedEntities() stops reporting
//!      it and its teardown stops deleting it.
//!
//! If step 3 fails (no mounted module on the sortie config, or it already holds a hull), the sortie
//! deployment is torn down and step 4 never runs - the vehicle stays exactly where it was, still
//! parked, still owned by the module that has owned it all along. Nothing is ever released speculatively.
//!
//! ⚠ WHY RELEASE COMES LAST AND NOT FIRST. Releasing before AdoptVehicle() succeeds would leave a
//! window where NEITHER module's teardown would delete a hull that turned out to have nowhere to go.
//! Releasing before creating the sortie at all - "optimistically" - would do the same for every failure
//! path between here and AdoptVehicle(). The order in this file is the whole of the guarantee.
//!
//! ==========================================================================================
//! WHAT ENDS A SORTIE, AND WHETHER ANYTHING LEAKS IF THE BATTLE ENDS FIRST (see Phase 4's own
//! question, answered here for Phase 5 - "does this config need an analogous module to be torn down
//! correctly?").
//! ==========================================================================================
//! Unlike the QRF's mounted echelon - created and owned by the QRF controller for exactly the life of
//! ITS OWN battle - a sortie is created by THIS module, which lives on the base's parked-vehicle
//! deployment, a deployment with no reinforcement module and therefore (by that module's own header) no
//! collection path of its own; it persists for the campaign. Neither OVT_MobileCheckpointBehaviorDeploymentModule
//! nor the sortie's mounted module ever calls RequestDeploymentCollection() (Phase 4 confirmed the
//! former does this nowhere in the tree), and the sortie's config authors no reinforcement or condition
//! module, so nothing shipped ON THE SORTIE ITSELF can ever take it down.
//!
//! So THIS module polls: once it has sent a sortie, every update it asks the occupying faction manager
//! whether ANY battle is still active (m_bQRFActive). The faction runs at most one QRF at a time - the
//! same fact SendMountedEchelon relies on - so "no battle active" correctly means "the battle that
//! provoked this sortie is over", and the sortie is CollectDeployment()'d the moment that is true: paid
//! for out of the deployment POOL (see SendSortie's ledger note) and refunded to the same ledger, which
//! is exactly what makes CollectDeployment - not DeleteDeployment - the right teardown here (contrast
//! D6, where the QRF echelon is bought from the RESERVE and must never be collected).
//!
//! ⚠ THE HONEST GAP. The poll is coarse: two battles starting back to back with no gap between them
//! would leave the sortie uncollected until the NEXT gap, not the one that actually ended its own fight.
//! And the poll only runs because THIS module's OnUpdate runs, which requires the base's own
//! parked-vehicle deployment to still exist - if that base changes hands or its deployment is torn down
//! by something else while its sortie is still out, nothing is left watching, and the sortie's own
//! deployment has no other way home. Both are recorded rather than solved: the base deployment is
//! deliberately near-permanent (its own class header - "persists for the life of the campaign"), so the
//! second case is rare, and the first costs nothing but a delayed collection, never a leak.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_CrewUpOnAlarmBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	//! The QRF's own alarm range (OVT_QRFControllerComponent's QRF_RANGE), reused rather than a fresh
	//! number: a base outside the QRF's own notion of "near this battle" has no business crewing up for it.
	[Attribute(defvalue: "750", desc: "How close a battle has to start to this base before its parked armour crews up, in metres")]
	float m_fAlarmRadius;

	//! Resolved through OVT_DeploymentManagerComponent.FindConfigByName() - the same seam
	//! SendMountedEchelon uses for the QRF's own config.
	[Attribute(defvalue: "Base Armour Sortie", desc: "Registered name of the deployment config to stand up with the crewed hull")]
	string m_sSortieConfigName;

	//! ⚠ A BUDGET DEBITED AT CREATION, NOT THE VEHICLE'S PRICE PAID TWICE. The hull was already bought
	//! by OVT_ParkedVehicleSpawningDeploymentModule.m_iCostPerVehicle when it was parked; this is what
	//! crewing it and sending it out costs on top, debited from the deployment POOL at SendSortie's own
	//! create-then-debit choke point (T6.4's pattern, not the QRF's - see the class header's ledger note).
	[Attribute(defvalue: "60", desc: "Resources debited from the deployment pool when a sortie is sent")]
	int m_iSortieBudget;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE - none of it authored, none of it persisted (D9: nothing about a live mounted
	// force survives a load, and neither does the fact that one was ever sent).
	//------------------------------------------------------------------------------------------------

	//! The sortie this module has out, if any. EntityID has no meaningful "unset" test from script, so
	//! it is paired with m_bHasSortie rather than compared against a sentinel (the same pattern
	//! OVT_CompositionSpawningDeploymentModule.m_CompositionId uses).
	protected EntityID m_SortieDeploymentId;
	protected bool m_bHasSortie;

	//------------------------------------------------------------------------------------------------
	// The alarm
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribed on activation, removed on deactivation AND on cleanup (see OnCleanup's own note -
	//! Deactivate() is guarded by m_bActive and Cleanup() only calls it when active, so a module cleaned
	//! up without ever having activated needs its own unsubscribe or none ever runs).
	override void OnActivate()
	{
		super.OnActivate();

		SubscribeToBattles();
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE REMOVE CALL IS WRITTEN OUT HERE, NOT THROUGH A SHARED HELPER - deliberately, so that both
	//! cleanup paths carry their own unmistakable, independently-greppable unsubscribe rather than one
	//! call site the other merely reaches through.
	override void OnDeactivate()
	{
		OVT_OccupyingFactionManager occupyingOnDeactivate = OVT_Global.GetOccupyingFaction();
		if (occupyingOnDeactivate)
			occupyingOnDeactivate.m_OnBattleStarted.Remove(OnBattleStarted);

		super.OnDeactivate();
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ MUST ALSO UNSUBSCRIBE HERE, INDEPENDENTLY OF OnDeactivate(). Deactivate() is guarded by
	//! m_bActive and Cleanup() only calls it when active (OVT_BaseDeploymentModule.Cleanup()), so a
	//! module cleaned up WITHOUT ever having activated never reaches OnDeactivate() at all - and
	//! ScriptInvoker.Insert() does not de-duplicate, so a stale subscription left behind here would fire
	//! a re-activated module twice per battle.
	override protected void OnCleanup()
	{
		OVT_OccupyingFactionManager occupyingOnCleanup = OVT_Global.GetOccupyingFaction();
		if (occupyingOnCleanup)
			occupyingOnCleanup.m_OnBattleStarted.Remove(OnBattleStarted);

		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected void SubscribeToBattles()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return;

		occupying.m_OnBattleStarted.Insert(OnBattleStarted);
	}

	//------------------------------------------------------------------------------------------------
	//! A battle has started somewhere. Crews up and sorties this base's parked hull when it is close
	//! enough, and does nothing at all otherwise.
	//!
	//! ⚠ FIRES ONCE PER VEHICLE, STRUCTURALLY, NOT THROUGH A LATCH. Once a sortie has taken the hull,
	//! ReleaseVehicleOwnership() has removed it from the parked module's own list, so
	//! ResolveParkedVehicle() answers null on every later battle - inside the radius or not - until
	//! there is a vehicle here again, which this module never re-creates (occupying/vehicles G4/F8 does
	//! not apply here: a sortied hull is not "lost", it is on its own deployment now).
	//! ⚠ PUBLIC SO THAT THE INITIALISATION TIER CAN DRIVE ONE, exactly as
	//! OVT_QRFControllerComponent.IsFightingFit and SendMountedEchelon are - a fixture cannot wait for a
	//! deployment's real OnActivate() (a whole 8-12 s update interval after creation) just to prove the
	//! subscription plumbing, which is otherwise mechanical. Nothing in the campaign calls this directly;
	//! production reaches it only through the m_OnBattleStarted subscription made in OnActivate().
	//! \param[in] location Where the battle is - OVT_OccupyingFactionManager.m_vQRFLocation.
	void OnBattleStarted(vector location)
	{
		if (!m_ParentDeployment)
			return;

		if (vector.Distance(location, m_ParentDeployment.GetPosition()) > m_fAlarmRadius)
			return;

		OVT_ParkedVehicleSpawningDeploymentModule parked = ResolveParkedModule();
		if (!parked)
			return;

		Vehicle vehicle = ResolveParkedVehicle(parked);
		if (!vehicle)
			return;

		SendSortie(location, parked, vehicle);
	}

	//------------------------------------------------------------------------------------------------
	// Standing up the sortie
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Creates the sortie deployment, hands it the hull, and only then lets the parked module go. See
	//! the class header for why the order is the whole of the ownership-transfer contract.
	//! \param[in] target Where the sortie is headed - the battle location. The mounted module's own
	//! m_fLZStandoffDistance is what stops it short; this passes the raw battle position through.
	//! \param[in] parked The module still holding the vehicle.
	//! \param[in] vehicle The hull to crew.
	protected void SendSortie(vector target, notnull OVT_ParkedVehicleSpawningDeploymentModule parked, notnull Vehicle vehicle)
	{
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
		{
			RefuseSortie("there is no deployment manager to stand one up with");
			return;
		}

		OVT_DeploymentConfig config = deployments.FindConfigByName(m_sSortieConfigName);
		if (!config)
		{
			RefuseSortie("the deployment registry holds no config named '" + m_sSortieConfigName + "'");
			return;
		}

		int factionIndex = m_ParentDeployment.GetControllingFaction();
		vector source = m_ParentDeployment.GetPosition();

		// ⚠ ZERO INVESTED AT CREATION, on purpose - this is NOT the QRF's D6 rule. What the sortie
		// actually costs is debited explicitly below, AFTER AdoptVehicle() has succeeded, from the
		// deployment POOL rather than the reserve (contrast the echelon, funded from the QRF's own wave
		// budget). Stamping it here too would double-count it on the deployment's own snapshot.
		OVT_DeploymentComponent sortie = deployments.ForceCreateDeploymentFrom(config, target, factionIndex, source, 0);
		if (!sortie)
		{
			RefuseSortie("the deployment manager refused to create '" + m_sSortieConfigName + "' at " + target.ToString());
			return;
		}

		OVT_MountedForceSpawningDeploymentModule mounted = FindSortieMountedModule(sortie);
		if (!mounted || !mounted.AdoptVehicle(vehicle))
		{
			// The sortie exists but has no hull to crew. It must not sit on the map paid for and empty -
			// and the parked vehicle must not be released, because nothing has actually taken it.
			deployments.DeleteDeployment(sortie);
			RefuseSortie("'" + m_sSortieConfigName + "' authors no mounted force able to adopt the hull");
			return;
		}

		// ⚠ RELEASED ONLY NOW - see the class header. AdoptVehicle() above has already accepted the
		// hull, so the sortie owns it from this line on.
		parked.ReleaseVehicleOwnership(vehicle);

		// ⚠ CREATE-THEN-DEBIT, matching OVT_ObjectiveDirectorComponent.CreateObjectiveDeployment and
		// T6.4's hunter-killer dispatcher - debit only AFTER a successful create, never before, so a
		// refusal never burns the pool on nothing. Because this debits the POOL (not the reserve), the
		// sortie MAY be collected normally: the refund and the debit are the same ledger.
		deployments.SubtractFactionResources(factionIndex, m_iSortieBudget);

		m_SortieDeploymentId = sortie.GetOwner().GetID();
		m_bHasSortie = true;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Crew-up '%1': base at %2 crewed its parked vehicle and sortied it towards %3 for %4 resources",
			DescribeSelf(), source.ToString(), target.ToString(), m_iSortieBudget.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Says why no sortie was sent. VERBOSE: every one of these is an ordinary outcome (a config not yet
	//! authored while the tree is mid-build, a framework refusal) and this can be asked once per battle
	//! per base.
	//! \param[in] reason What refused it.
	protected void RefuseSortie(string reason)
	{
		OVT_DeploymentLog.Debug("[Overthrow] Crew-up '" + DescribeSelf() + "': no sortie sent - " + reason);
	}

	//------------------------------------------------------------------------------------------------
	// Resolving modules
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! \return This deployment's own parked-vehicle module, or null when its config authors none.
	protected OVT_ParkedVehicleSpawningDeploymentModule ResolveParkedModule()
	{
		if (!m_ParentDeployment)
			return null;

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_ParkedVehicleSpawningDeploymentModule parked = OVT_ParkedVehicleSpawningDeploymentModule.Cast(spawningModule);
			if (parked)
				return parked;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] parked The module to ask.
	//! \return The first live vehicle it still reports, or null when it has parked nothing (never
	//! parked, destroyed, stolen, or already handed off).
	protected Vehicle ResolveParkedVehicle(notnull OVT_ParkedVehicleSpawningDeploymentModule parked)
	{
		array<IEntity> entities = parked.GetSpawnedEntities();
		foreach (IEntity entity : entities)
		{
			Vehicle vehicle = Vehicle.Cast(entity);
			if (vehicle)
				return vehicle;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] sortie The freshly created sortie deployment.
	//! \return Its mounted force module - the RUNTIME clone, not the config's template (the same
	//! distinction OVT_DeploymentManagerComponent.ApplyMountedSourceOverride draws) - or null when its
	//! config authors none.
	protected OVT_MountedForceSpawningDeploymentModule FindSortieMountedModule(notnull OVT_DeploymentComponent sortie)
	{
		array<OVT_BaseSpawningDeploymentModule> spawningModules = sortie.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			OVT_MountedForceSpawningDeploymentModule mounted = OVT_MountedForceSpawningDeploymentModule.Cast(spawningModule);
			if (mounted)
				return mounted;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Ending the sortie - see the class header's "what ends a sortie" section.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Polled every update once a sortie is out. Collects it the moment no battle anywhere is active.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		TickSortieTeardown();
	}

	//------------------------------------------------------------------------------------------------
	protected void TickSortieTeardown()
	{
		if (!m_bHasSortie)
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (occupying && occupying.m_bQRFActive)
			return;

		CollectSortie();
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the sortie down through CollectDeployment(), not DeleteDeployment() - it was bought from
	//! the deployment pool (SendSortie's own debit), so the pool is the right ledger to refund it to.
	//! See the class header's ledger note for why this differs from the QRF echelon's rule.
	protected void CollectSortie()
	{
		m_bHasSortie = false;

		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		IEntity entity = world.FindEntityByID(m_SortieDeploymentId);
		if (!entity)
			return;

		OVT_DeploymentComponent sortie = OVT_DeploymentComponent.Cast(entity.FindComponent(OVT_DeploymentComponent));
		if (!sortie)
			return;

		deployments.CollectDeployment(sortie);

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Crew-up '%1': the battle is over - its sortie has been collected", DescribeSelf()));
	}

	//------------------------------------------------------------------------------------------------
	// Cloning
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute, no runtime state - a clone belongs to a different deployment and has sent
	//! nothing. Drop m_fAlarmRadius and every base crews up for a battle anywhere on the map, or never;
	//! drop m_sSortieConfigName and every crew-up module refuses silently forever; drop m_iSortieBudget
	//! and the sortie is debited the class default of 0, so the campaign fields free armour.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_CrewUpOnAlarmBehaviorDeploymentModule clone = new OVT_CrewUpOnAlarmBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fAlarmRadius = m_fAlarmRadius;
		clone.m_sSortieConfigName = m_sSortieConfigName;
		clone.m_iSortieBudget = m_iSortieBudget;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return This module's name for a log line, or a stand-in when none was authored.
	protected string DescribeSelf()
	{
		if (!m_sModuleName.IsEmpty())
			return m_sModuleName;

		return "crew-up on alarm";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while this module has a sortie out. Exposed for tests - nothing in this file needs it.
	bool HasSortieOut()
	{
		return m_bHasSortie;
	}

	//! \return The sortie deployment's EntityID, or a zero id when none is out. Exposed for tests, which
	//! need it to tear a fixture sortie down; nothing in this file needs it internally.
	EntityID GetSortieDeploymentId()
	{
		return m_SortieDeploymentId;
	}
}
