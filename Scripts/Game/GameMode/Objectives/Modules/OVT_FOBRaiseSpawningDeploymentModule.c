//------------------------------------------------------------------------------------------------
//! THE INSERTION THAT BUILDS SOMETHING WHEN IT GETS THERE: a supply truck drives out from a base the
//! occupying faction holds, drops a party at the chosen site, and the forward operating base goes up.
//!
//! It is OVT_InsertionSpawningDeploymentModule with one job added, and everything about the drive -
//! the source provider, the truck, the crew's separate owner key, the landing zone, the stuck test,
//! the five roads to walking, the convoy slot, the waypoints - is inherited unchanged. What is added
//! here is the STRUCTURE, and the party this module registers is the forward base's free garrison:
//! they were registered with a plan that points at the deployment position, so once the truck lets
//! them out they walk onto the site and hold it.
//!
//! ==========================================================================================
//! 🔴 A RESTORED DEPLOYMENT RAISES NOTHING. THIS IS THE WHOLE REASON THE CLASS IS A DECISION (D11).
//! ==========================================================================================
//! The structure is a PERSISTENCE-TRACKED WORLD ENTITY. Vanilla persistence saves it and puts it back
//! before any deployment ticks, and the director's own serializer brings back the RECORD of it, so a
//! restored deployment that raised one anyway would give the campaign a SECOND forward base on every
//! single load - in a slightly different place each time, because the site is re-sampled. Ten loads
//! into a long campaign that is ten flagpoles in a field, every one of them findable, dismantleable
//! and persisted. This is R2 from virtualization/base-defense-migration restated for a bigger and far
//! more visible object, and the composition module next door carries the identical gate for the
//! identical reason.
//!
//! ⚠ THE GATE IS NOT ENOUGH ON ITS OWN AND THE LATCH IS NOT EITHER - BOTH ARE REQUIRED. The gate stops
//! the restored case; the latch (m_bRaiseAttempted) stops the live one, because a reinforcement rebuy
//! CLEARS the eliminated flags and re-runs the convergence, and a second arrival would otherwise
//! raise a second structure beside the first. DecideRaise() is where both live, as a pure function, so
//! the claim is assertable without a save, a truck or a world.
//! ==========================================================================================
//!
//! ⚠ IT DOES NOT DELETE THE STRUCTURE, EVER, AND THAT IS DELIBERATE (T7.10). The forward base outlives
//! its deployment: a reinforcement rebuy, a wipe, a collection by the objective condition module are
//! all things that end a DEPLOYMENT and none of them means the base is gone. There is exactly one
//! teardown path and it belongs to the director, which is the only thing that knows which of the three
//! exits happened and whether the resistance has earned the removal penalty. A cleanup-delete here
//! would also be a no-op in the one case it mattered - a restored deployment holds no structure id at
//! all, because that link is runtime-only.
//!
//! ⚠ THE STRUCTURE GOES AT THE DEPLOYMENT POSITION, NOT AT THE LANDING ZONE. The landing zone is where
//! the truck stopped, up to m_fLZStandoffDistance short of the site and snapped onto whatever road was
//! nearest; the site is the point the director sampled, traced and scored. Building at the drop point
//! would throw all of that away and put the base on a road.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_FOBRaiseSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule
{
	//! ⚠ NO DEFAULT, DELIBERATELY. The structure prefab is named in EXACTLY ONE place -
	//! Configs/Deployment/Deployment_ObjectiveFOB.conf - and a defvalue here would be a second answer
	//! that no config author could see and that the director's teardown (which reads the authored value
	//! off the config) would silently disagree with. Unauthored, this module logs a WARNING and builds
	//! nothing, which is a diagnosable failure rather than a structure nobody chose.
	[Attribute(desc: "The forward operating base structure. REQUIRED - it carries the dismantle action, so a prefab without one is a base the resistance can never remove", params: "et")]
	ResourceName m_rFOBPrefab;

	[Attribute(defvalue: "80", desc: "How close a registered group has to get to the deployment position, on the WALKING path, before the structure goes up. Ignored on the driving path, which raises on the truck's arrival")]
	float m_fRaiseOnFootRadius;

	//! The structure this module raised, once. EntityID rather than IEntity for the reason the
	//! composition module gives: the structure is a world entity a player can destroy, and a stale
	//! pointer is a crash where a stale id is a null lookup.
	protected EntityID m_StructureId;

	//! Whether m_StructureId holds anything. EntityID is a raw handle with no "unset" test from script.
	protected bool m_bHasStructure;

	//! Whether this module has already had its one attempt. ⚠ SEPARATE FROM m_bHasStructure ON PURPOSE:
	//! a structure a player has since dismantled must NOT be quietly rebuilt by the next convergence,
	//! and a rebuy that clears the eliminated flags must not buy a second one either.
	protected bool m_bRaiseAttempted;

	//------------------------------------------------------------------------------------------------
	//! What to do about the structure this pass.
	//!
	//! A PURE FUNCTION OF ITS ARGUMENTS, in the shape of
	//! OVT_CompositionSpawningDeploymentModule.DecideBuild() and for the same reason: the no-second-FOB
	//! claim is otherwise only assertable by driving a real save through a real objective with a real
	//! truck, and it is the single highest-consequence claim in the phase.
	//!
	//! FOUR GATES, IN ORDER, EACH A DIFFERENT QUESTION:
	//!   1. no deployment       - nothing to build around (a config template, the initialisation tier).
	//!                            FALSE, but nothing is latched: a template later cloned onto a real
	//!                            deployment must still be able to build.
	//!   2. already attempted   - one forward base per module, for the life of the deployment. FALSE.
	//!   3. restored from save  - D11. The structure is already standing, put back by vanilla
	//!                            persistence, and its record is already in the director. FALSE.
	//!   4. eliminated          - the party that was supposed to raise it is dead. FALSE, and NOT
	//!                            latched: the rebuy path clears the flag and the base is then owed.
	//! \param[in] hasDeployment Whether the module is attached to a live deployment.
	//! \param[in] alreadyAttempted Whether this module has already had its one attempt.
	//! \param[in] restoredFromSave Whether the parent deployment came back from a save point.
	//! \param[in] eliminated Whether this module or its deployment is flagged wiped out.
	//! \return True only when a structure should be raised right now.
	static bool DecideRaise(bool hasDeployment, bool alreadyAttempted, bool restoredFromSave, bool eliminated)
	{
		if (!hasDeployment)
			return false;

		if (alreadyAttempted)
			return false;

		if (restoredFromSave)
			return false;

		return !eliminated;
	}

	//------------------------------------------------------------------------------------------------
	//! THE DRIVING PATH. The truck has stopped at its landing zone and the party is on the ground.
	//! \param[in] lzPosition Where the transport actually stopped - NOT where the base goes.
	override protected void OnInsertionArrived(vector lzPosition)
	{
		super.OnInsertionArrived(lzPosition);

		TryRaiseStructure("its transport arrived");
	}

	//------------------------------------------------------------------------------------------------
	//! THE WALKING PATH, and it is not an afterthought.
	//!
	//! ⚠ FIVE INDEPENDENT THINGS DIVERT AN INSERTION ONTO FOOT and NONE of them reaches
	//! OnInsertionArrived(): a hop shorter than the walk threshold, a spent convoy cap, a vehicle prefab
	//! the faction does not have, a truck that stopped making progress, and a truck that was destroyed.
	//! Without this poll the forward base would simply never be raised in any of those five cases, the
	//! phase would sit until its timeout, and the objective would be blacklisted with a log line
	//! blaming the clock. The base class's arrival test only ever watches the TRUCK.
	//! \param[in] deltaTime Milliseconds since the last update of this module.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (m_bRaiseAttempted)
			return;

		if (GetInsertionState() == OVT_EInsertionState.DRIVING)
			return;

		if (!m_ParentDeployment)
			return;

		if (CountAliveRegisteredMembersAtSite() < 1)
			return;

		TryRaiseStructure("its party reached the site on foot");
	}

	//------------------------------------------------------------------------------------------------
	//! Living members of this module's own registered force standing at the site.
	//!
	//! ⚠ THE SURVIVOR MASK AND THE RECORD, NEVER AN AGENT COUNT. A dormant or spawn-queued group reports
	//! zero agents while being perfectly alive, so a poll that counted bodies would decide the party had
	//! never arrived on any server with nobody standing nearby - which is most of them, most of the
	//! time, and exactly when a forward base is supposed to go up unobserved.
	//! \return Living members within m_fRaiseOnFootRadius of the deployment position.
	protected int CountAliveRegisteredMembersAtSite()
	{
		if (m_fRaiseOnFootRadius <= 0)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		array<int> handles = new array<int>();
		CollectRegisteredHandles(handles);

		vector site = m_ParentDeployment.GetPosition();

		int alive = 0;
		foreach (int handle : handles)
		{
			if (!virtualization.IsRegistered(handle))
				continue;

			int members = virtualization.GetAliveMemberCount(handle);
			if (members < 1)
				continue;

			if (vector.Distance(virtualization.GetPosition(handle), site) > m_fRaiseOnFootRadius)
				continue;

			alive = alive + members;
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! Raises the forward operating base, once, if every gate allows it.
	//!
	//! ⚠ THE LATCH IS SET BEFORE THE SPAWN, NOT AFTER. A prefab that fails to load fails on every pass,
	//! and retrying it once per update for the rest of the phase would fill the log at six lines a
	//! minute with nothing changing. The failure is loud and terminal; the phase timeout is what ends
	//! the objective, with its own reason.
	//! \param[in] arrival How the party got there, for the log line.
	protected void TryRaiseStructure(string arrival)
	{
		bool hasDeployment = m_ParentDeployment != null;
		bool restoredFromSave = false;
		bool eliminated = m_bSpawnedUnitsEliminated;

		if (hasDeployment)
		{
			restoredFromSave = m_ParentDeployment.WasRestoredFromSave();
			if (m_ParentDeployment.GetSpawnedUnitsEliminated())
				eliminated = true;
		}

		if (!DecideRaise(hasDeployment, m_bRaiseAttempted, restoredFromSave, eliminated))
			return;

		m_bRaiseAttempted = true;

		if (m_rFOBPrefab.IsEmpty())
		{
			Print(string.Format("[Overthrow] Forward base '%1' has no structure prefab authored - nothing will be raised",
				DescribeSelf()), LogLevel.WARNING);
			return;
		}

		vector site = m_ParentDeployment.GetPosition();

		IEntity structure = OVT_WorldUtils.SpawnEntityPrefab(m_rFOBPrefab, site);
		if (!structure)
		{
			Print(string.Format("[Overthrow] Forward base '%1': the structure failed to spawn at %2",
				DescribeSelf(), site.ToString()), LogLevel.WARNING);
			return;
		}

		// Tracking is what puts the structure in save points. Without it the forward base vanishes on
		// the next load while the director's record still says it is standing, and the objective sits in
		// the forward-base phase pointing at nothing until its timeout.
		OVT_PersistenceTracking.Track(structure);

		// The structure carves a hole in the navmesh, and the AI that has to garrison it needs to be
		// able to walk around it from the moment it appears.
		OVT_NavmeshRebuild.RebuildNow(structure);

		m_StructureId = structure.GetID();
		m_bHasStructure = true;

		NotifyDirector(site);

		Print(string.Format("[Overthrow] Forward base '%1' raised at %2 - %3",
			DescribeSelf(), site.ToString(), arrival), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the objective director the forward base is standing.
	//!
	//! ⚠ NO NOTIFICATION IS SENT TO THE RESISTANCE, AND THAT IS AN EXPLICIT REQUIREMENT RATHER THAN AN
	//! OVERSIGHT (D12). The forward base is the one thing in the whole ramp that appears silently: the
	//! resistance is supposed to FIND it. Everything else the ramp does is announced or visible; this
	//! is the part a player is rewarded for noticing. If you are about to add a broadcast here because
	//! it seems inconsistent, it is the requirement, and the Intel epic is where surfacing it belongs.
	//! \param[in] site Where the structure stands.
	protected void NotifyDirector(vector site)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!director)
			return;

		director.OnFOBRaised(site, GetInsertionSource(), m_ParentDeployment.GetDeploymentName());
	}

	//------------------------------------------------------------------------------------------------
	//! Spends this module's one raise attempt WITHOUT building anything.
	//!
	//! ⚠ IT EXISTS SO THAT "THIS MODULE WILL NOT BUILD" CAN BE SAID WITHOUT SAYING "IT BUILT", and it
	//! has no production caller today - deliberately, and for the same reason
	//! OVT_BaseSabotageBehaviorDeploymentModule.AbortMission() does. Anything that later has to cancel
	//! an in-flight raise - a teardown, a reset, a Game Master - must have something to reach for that
	//! does NOT put a persisted flagpole in the world and does NOT tell the director a forward base is
	//! standing. Reaching for TryRaiseStructure() instead would do both.
	//!
	//! It is also what the initialisation-tier clone case uses to fire the latch, precisely because the
	//! raise path would spawn.
	//! \param[in] reason Why, for the log line.
	void AbandonRaise(string reason)
	{
		if (m_bRaiseAttempted)
			return;

		m_bRaiseAttempted = true;

		Print(string.Format("[Overthrow] Forward base '%1' will not be raised: %2", DescribeSelf(), reason), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The structure this module raised, or null when it never raised one / it is gone.
	IEntity GetStructure()
	{
		if (!m_bHasStructure)
			return null;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return null;

		return world.FindEntityByID(m_StructureId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this module has already had its one raise attempt.
	bool HasAttemptedRaise()
	{
		return m_bRaiseAttempted;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The prefab this module raises. Read by the director so the teardown can find a structure
	//!         whose runtime link did not survive a load.
	ResourceName GetFOBPrefab()
	{
		return m_rFOBPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY inherited attribute plus this module's own two.
	//!
	//! ⚠ CloneModule IS NOT CHAINED. It builds a fresh instance and copies BY HAND, so the thirteen
	//! lines from OVT_InfantrySpawningDeploymentModule AND the ten from
	//! OVT_InsertionSpawningDeploymentModule are repeated here verbatim - twenty-three inherited lines,
	//! and anything appended to either parent has to be appended here as well. A forgotten line does not
	//! warn, does not log and does not fail to parse: it ships the CLASS DEFAULT on every deployment,
	//! forever, which is how m_fMaxCruiseSpeed was lost on the vehicle module for a whole release.
	//!
	//! What a dropped line would cost HERE specifically: drop m_rFOBPrefab and the forward base is a
	//! phase that spends resources, drives a truck across the map and builds nothing, with one WARNING
	//! line to explain it; drop m_Source and the module registers nothing at all, silently; drop
	//! m_fRaiseOnFootRadius and it clones as 0, which disables the walking path - so the four insertion
	//! failures that divert onto foot each become an objective that times out.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_FOBRaiseSpawningDeploymentModule clone = new OVT_FOBRaiseSpawningDeploymentModule();

		// --- Inherited from OVT_InfantrySpawningDeploymentModule, all thirteen.
		clone.m_sModuleName = m_sModuleName;
		clone.m_sGroupType = m_sGroupType;
		clone.m_iMinGroupCount = m_iMinGroupCount;
		clone.m_iMaxGroupCount = m_iMaxGroupCount;
		clone.m_bScaleByTownSize = m_bScaleByTownSize;
		clone.m_fSpawnRadius = m_fSpawnRadius;
		clone.m_iCostPerGroup = m_iCostPerGroup;
		clone.m_bAllowReinforcement = m_bAllowReinforcement;
		clone.m_iReinforcementCost = m_iReinforcementCost;
		clone.m_bSpawnAtNearestBase = m_bSpawnAtNearestBase;
		clone.m_bReinforceFromNearestBase = m_bReinforceFromNearestBase;
		clone.m_eImportance = m_eImportance;
		clone.m_bSnapToRoad = m_bSnapToRoad;

		// --- Inherited from OVT_InsertionSpawningDeploymentModule, all ten.
		clone.m_Source = m_Source;
		clone.m_fWalkThresholdDistance = m_fWalkThresholdDistance;
		clone.m_sTruckVehicleType = m_sTruckVehicleType;
		clone.m_sTruckCrewGroup = m_sTruckCrewGroup;
		clone.m_fLZStandoffDistance = m_fLZStandoffDistance;
		clone.m_fStuckSpeedThreshold = m_fStuckSpeedThreshold;
		clone.m_iStuckTicks = m_iStuckTicks;
		clone.m_fArrivalRadius = m_fArrivalRadius;
		clone.m_iTruckCostOverride = m_iTruckCostOverride;
		clone.m_bWalkWhenInsertionRefused = m_bWalkWhenInsertionRefused;

		// --- This module's own two.
		clone.m_rFOBPrefab = m_rFOBPrefab;
		clone.m_fRaiseOnFootRadius = m_fRaiseOnFootRadius;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintFOBRaiseDebugInfo()
	{
		PrintInsertionDebugInfo();

		string raised = "no";
		if (m_bHasStructure)
			raised = "yes";

		string attempted = "no";
		if (m_bRaiseAttempted)
			attempted = "yes";

		Print(string.Format("  Forward base raised: %1  Attempted: %2  Prefab: %3", raised, attempted, m_rFOBPrefab));
	}
}
