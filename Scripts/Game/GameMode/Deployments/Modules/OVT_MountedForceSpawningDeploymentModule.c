//------------------------------------------------------------------------------------------------
//! A MOUNTED FORCE: the live-insertion convoy, except the vehicle is a FIGHTING vehicle picked off
//! the faction's escalation ladder, and the force STAYS ABOARD when it gets there.
//!
//! 🔴 THE WALK FALLBACK IS STILL THE SPINE, INHERITED WHOLE AND NOT RE-DERIVED. Every one of the six
//! ways OVT_InsertionSpawningDeploymentModule ends up on foot still works here unchanged, and this
//! class adds a seventh (the vehicle is destroyed while it is being held). In all of them the force
//! exists, holds the plan it was registered with, and walks to the objective. Nothing below may turn
//! one of them into "men in a dead vehicle".
//!
//! WHAT CHANGES, AND IT IS ONLY THIS:
//!   1. THE VEHICLE IS LADDER-PICKED. GetVehiclePrefabFromFaction() resolves m_sVehicleRole against
//!      the live threat, the difficulty's vehicleThresholdScale and this module's own vehicle budget,
//!      instead of naming one prefab. A ladder that answers nothing falls back to m_sTruckVehicleType.
//!   2. ARRIVAL DOES NOT OPEN THE DOORS. CompleteInsertion() releases the convoy slot, enters HOLDING
//!      and leaves everybody seated. There is no return leg: the vehicle is the point.
//!   3. HOLDING IS TICKED. A destroyed vehicle or a dead crew drops the force onto the march.
//!
//! ==========================================================================================
//! 🔴 THE PASSENGERS ARE NEVER PUT BACK ON THE ORDINARY PROXIMITY RING WHILE THEY ARE ABOARD.
//! ==========================================================================================
//! This is a PROHIBITION, not a preference, and it is the only thing standing between a mounted force
//! and being pinned dormant inside its own battle.
//!
//! OVT_DeploymentManagerComponent.SuppressForcesAroundBattle() pins every not-yet-materialised
//! occupying-faction group inside a live battle's ring - and skips any handle whose resolved spawn
//! distance is STRICTLY WIDER than the world's global ring (:567). Riding crews and riding passengers
//! register at RIDING_SPAWN_DISTANCE (100 km), so they are already exempt, by that rule, with no
//! whitelist anywhere. Putting them back on the global ring throws the exemption away: the QRF's
//! mounted echelon would be pinned the moment it reached the battle it was sent to, and would
//! materialise nobody. The failure is completely silent - a deployment that exists, is paid for, and
//! delivers an empty vehicle.
//!
//! So: NO RING RESTORATION ANYWHERE ON THE ARRIVAL PATH. That is the entire mechanism, and it is
//! enough, because nothing re-runs the parent's ring sweep during a live hold: the sweep at the end of
//! EnsureGroups() is reached from activation (before any hold exists), from the UNDECIDED retry, and
//! from the records-restored fan-out - and NOT from the reinforcement rebuy, which calls the
//! convergence directly.
//!
//! ⚠ AND THE RECORDS-RESTORED CASE IS DELIBERATELY LEFT TO THE PARENT. A restore RE-CREATES the group
//! entities from core's own payload, so a force that was seated is not seated any more - the men who
//! were in the vehicle no longer exist. Putting those replacements back on the riding ring would
//! permanently materialise a squad that is standing in a field, one per deployment, for the rest of the
//! campaign. The parent's drop is the RIGHT answer there, and it is D9's answer: a restored mounted
//! deployment walks.
//!
//! ⚠ A REINFORCEMENT BOUGHT INTO A HOLD REGISTERS AT THE SOURCE, ON THE ORDINARY RING, AND WALKS IN -
//! and IsForceMounted() is deliberately NOT widened to say otherwise. It gates both registration seams
//! AND the seating pairing, and the pairing (AdoptPassenger) is written against DRIVING; widening only
//! the ring half would register replacements beside a parked vehicle, never seat them, and never let
//! them go dormant again - a permanently materialised squad standing in the road for the rest of the
//! campaign. Men who arrive after the vehicle has stopped are reinforcements on foot, which is what
//! they look like anyway.
//!
//! WHAT IT OWNS (all inherited, all still leaked if a path below forgets them):
//!   - the vehicle - deleted at teardown unless a player claimed it, or collected on the abandoned
//!     countdown. ⚠ EXCEPT one handed in by AdoptVehicle(): that hull belongs to whoever handed it
//!     over until they have released their own ownership (Phase 5's contract);
//!   - the crew's registration, under its own owner key, never counted as part of the force;
//!   - the waypoints - AIGroup.AddWaypoint() does NOT take ownership;
//!   - one slot of the faction's convoy cap. ⚠ A LEAKED RESERVATION IS PERMANENT. CompleteInsertion()
//!     below still calls ReleaseReservation(), and it must: a mounted force holds its vehicle for
//!     minutes or hours, and a slot held for that long is a slot the faction never drives on again.
//!   BORROWED: the passenger groups, the ladder, the difficulty scalar, the threat figure.
//!
//! ⚠ m_sTruckVehicleType MUST STAY AUTHORED even on a config that only cares about the role. The
//! parent's SpawnTruck() refuses on an empty one BEFORE it asks for a prefab, so an empty string is
//! not "use the ladder" - it is "never put a vehicle on the road at all".
//!
//! ⚠ THE PRICE IS A BUDGET, NOT A RECEIPT (D4). A deployment's cost is computed from the CONFIG
//! TEMPLATE, before any deployment exists and therefore before the faction is known, so no rung can be
//! resolved at pricing time. The module charges the authored m_iTruckCostOverride and then refuses any
//! rung dearer than it - which is what makes escalation cost money: a config that wants a BTR-70 has
//! to author at least what a BTR-70 costs.
//!
//! ⚠ NOTHING HERE IS PERSISTED (D9). A vehicle is not saved and a convoy is never resumed across a
//! load, so a restored mounted deployment walks its force in - the fallback, working.
//!
//! ⚠ THE HOLD CLOCK DOES NOT COLLECT THE DEPLOYMENT ITSELF. RequestDeploymentCollection() and the
//! exfiltration rule that makes it safe (nobody may watch a squad evaporate) live on
//! OVT_BaseBehaviorDeploymentModule, and a spawning module cannot reach them. m_iHoldTicks therefore
//! LATCHES - IsHoldExpired() - and the behaviour module riding on this deployment is what acts on it.
//! Duplicating the exfil rule here would be a second copy of a play-tested policy.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_MountedForceSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule
{
	[Attribute(defvalue: "armed", desc: "Ladder role in the faction VEHICLE registry, e.g. \"armed\". The highest rung unlocked by the live threat that also fits this module's vehicle budget is picked. Empty falls back to the named vehicle type")]
	string m_sVehicleRole;

	[Attribute(defvalue: "0", desc: "TRUE reproduces the plain insertion module exactly - the force is dropped at the landing zone and the vehicle drives home. For a config that wants a lift rather than a fighting vehicle")]
	bool m_bDismountOnArrival;

	[Attribute(defvalue: "0", desc: "Update ticks (about 10 s each) the force holds its vehicle at the drop point before this module reports the hold expired. 0 holds indefinitely")]
	int m_iHoldTicks;

	[Attribute(defvalue: "0", desc: "TRUE takes the vehicle handed in by AdoptVehicle() instead of spawning one. Used by the base armour sortie, which crews a hull the player could already see parked")]
	bool m_bAdoptExistingVehicle;

	[Attribute(defvalue: "1", desc: "TRUE puts the force on foot when the ladder answers no rung, instead of substituting the named transport type. A doctrine whose whole point is the fighting vehicle should not field an unarmed truck in its place")]
	bool m_bWalkWhenNoLadderRung;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE - none of it persisted, all of it re-derived. See the class header.
	//------------------------------------------------------------------------------------------------

	//! Where a RUNTIME caller says this force sets out from, overriding the authored provider (D7).
	//! vector.Zero means "nobody said", which is why it must be applied before the first convergence:
	//! the source decides both the anchor the force is registered at and the ring it is registered on,
	//! and neither can be changed afterwards without throwing away the survivor mask.
	protected vector m_vSourceOverride;

	//! A hull handed in by another module, taken instead of spawning one when m_bAdoptExistingVehicle.
	protected Vehicle m_AdoptedVehicle;

	//! Update ticks spent in HOLDING. Only counted while m_iHoldTicks is positive.
	protected int m_iHoldTicksElapsed;

	//! Latches "this force has held for as long as it was asked to". Read through IsHoldExpired() by
	//! the behaviour module that owns collection; nothing in this file acts on it.
	protected bool m_bHoldExpired;

	//! Latches the one line explaining that the ladder answered nothing, so a campaign whose threat is
	//! below every rung does not print a line per insertion.
	protected bool m_bLadderMissLogged;

	//! Which rung was actually picked, for the log and for a behaviour module that wants to know what
	//! it is driving. Empty until a vehicle has been resolved through the ladder.
	protected string m_sResolvedVehicleName;

	//------------------------------------------------------------------------------------------------
	// The vehicle
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! THE LADDER, resolved against the live threat rather than a name in a config.
	//!
	//! ⚠ IT FALLS BACK ON EVERY REFUSAL, and that is the whole design: the drive-vs-walk decision was
	//! already made by the time this is asked, so a ladder that answers nothing must not turn into a
	//! force that never leaves. An unauthored role, a faction with no rungs on it, a threat below every
	//! rung and a budget under the cheapest one are four different reasons for the same answer, and all
	//! four end in the parent's named-vehicle path.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \return The prefab, or an empty ResourceName - which is one of the roads to walking.
	override protected ResourceName GetVehiclePrefabFromFaction(int factionIndex)
	{
		if (m_sVehicleRole.IsEmpty())
			return super.GetVehiclePrefabFromFaction(factionIndex);

		float threat = ResolveLiveThreat();

		OVT_FactionVehicleEntry entry;
		if (ResolveLadderEntry(factionIndex, threat, entry) && !entry.m_sVehiclePrefab.IsEmpty())
		{
			m_sResolvedVehicleName = entry.m_sVehicleName;

			OVT_DeploymentLog.Debug(string.Format("[Overthrow] Mounted force '%1': role '%2' at threat %3 resolved to '%4'",
				DescribeSelf(), m_sVehicleRole, Math.Round(threat).ToString(), entry.m_sVehicleName));

			return entry.m_sVehiclePrefab;
		}

		LogLadderMiss(threat);

		// An empty name is one of the documented roads to walking (SpawnTruck refuses, and the parent
		// falls back to the march) - which is the honest answer for a doctrine that exists to field a
		// fighting vehicle: below the bottom rung the faction has nothing to send, so it sends nobody
		// in a vehicle rather than substituting an unarmed truck.
		if (m_bWalkWhenNoLadderRung)
			return ResourceName.Empty;

		return super.GetVehiclePrefabFromFaction(factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! One ladder query, null-safe at every step.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[in] threat The live threat figure to resolve against.
	//! \param[out] entry The picked rung. Untouched on a false return.
	//! \return True when a rung was picked.
	protected bool ResolveLadderEntry(int factionIndex, float threat, out OVT_FactionVehicleEntry entry)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return false;

		OVT_Faction faction = factions.GetOverthrowFactionByIndex(factionIndex);
		if (!faction)
			return false;

		faction.InitializeVehicleRegistry();

		// ⚠ THE BUDGET IS THE AUTHORED VEHICLE PRICE, and it is the reason escalation is not free. See
		// the class header: the price was charged from the config template long before the faction was
		// known, so this is a ceiling the pick has to fit under rather than a bill anybody pays now.
		return faction.ResolveVehicleForRole(m_sVehicleRole, threat, ResolveThresholdScale(), m_iTruckCostOverride, entry);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The occupying faction's live threat, or 0 when there is no manager to ask. A threat of 0
	//! still unlocks every rung authored at 0, so the bottom of the ladder is always reachable.
	protected float ResolveLiveThreat()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return 0;

		return occupying.GetThreatFloat();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The difficulty's ladder threshold scalar, or 1 when there is no preset to ask.
	//! OVT_VehicleLadderRules treats a non-positive scale as 1 as well, so both ends agree.
	protected float ResolveThresholdScale()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return 1;

		return difficulty.vehicleThresholdScale;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE that the ladder had nothing to offer, and then stops saying it.
	//!
	//! NORMAL rather than WARNING: this is an ordinary state of an early campaign, where the threat is
	//! under every rung. It is logged at all because "the enemy never fields armour" has to have an
	//! answer in the log that names the role and the number it was measured against.
	//! \param[in] threat The threat the ladder was asked at.
	protected void LogLadderMiss(float threat)
	{
		if (m_bLadderMissLogged)
			return;

		m_bLadderMissLogged = true;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Mounted force '%1': role '%2' answered no rung at threat %3 inside a budget of %4 - falling back to the named vehicle type",
			DescribeSelf(), m_sVehicleRole, Math.Round(threat).ToString(), m_iTruckCostOverride.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the handed-in hull when there is one, and otherwise spawns exactly as the parent does.
	//!
	//! ⚠ INERT UNTIL A CONFIG AUTHORS m_bAdoptExistingVehicle. With the flag false this is byte-for-byte
	//! the parent's behaviour, which is what makes Phase 5 a config change plus a behaviour module
	//! rather than a change to this file.
	//! \return True when there is a vehicle.
	override protected bool SpawnTruck()
	{
		if (!m_bAdoptExistingVehicle || !m_AdoptedVehicle)
			return super.SpawnTruck();

		m_Truck = m_AdoptedVehicle;

		// Where it already stands. There is no "home" for an adopted hull - it never set out from
		// anywhere - and the two readers of this fall back to the source anyway.
		m_vHome = m_Truck.GetOrigin();

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Mounted force '%1' adopted a vehicle already standing at %2 instead of spawning one",
			DescribeSelf(), m_vHome.ToString()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Arrival
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! ARRIVED, AND STILL ABOARD. The mounted answer to the parent's success path.
	//!
	//! Four things happen and one deliberately does not:
	//!   - the convoy slot is handed back. ⚠ NOT OPTIONAL. A mounted force holds its vehicle for the
	//!     rest of the deployment's life, and a slot held that long is one the faction never gets back;
	//!   - the state becomes HOLDING, which the parent's OnUpdate if-chain tests for nothing, so the
	//!     inherited dispatch does nothing at all with it;
	//!   - the arrival hook fires, with the state already set so a subclass reading it sees HOLDING;
	//!   - the drive's counters are zeroed, so a hold that later falls back to the march starts clean.
	//!   - NOBODY GETS OUT, and nobody goes back on the ordinary proximity ring. See the class header
	//!     for what that ring costs a force sitting in a battle.
	//!
	//! m_bDismountOnArrival reproduces the parent EXACTLY, by calling it, rather than by repeating what
	//! it does. There is one success path for a lift and it is the one that has been play-tested.
	override protected void CompleteInsertion()
	{
		if (m_bDismountOnArrival)
		{
			super.CompleteInsertion();
			return;
		}

		int delivered = m_aHandles.Count();

		ReleaseReservation();

		m_eState = OVT_EInsertionState.HOLDING;

		m_iHoldTicksElapsed = 0;
		m_bHoldExpired = false;
		m_iReturnTicksElapsed = 0;
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;
		m_iInsideRadiusTicks = 0;
		m_iStuckTicksElapsed = 0;
		m_bHaveLastTruckPosition = false;

		// 🔴 THE OUTBOUND ORDER MUST DIE HERE. Arrival is judged by this module's own radius test
		// (m_fArrivalRadius), NOT by the waypoint completing - so a force that has "arrived" is still
		// carrying a live MOVE order to a point it is merely near. The parent never had this problem
		// because its CompleteInsertion immediately replaces the order with the drive home; a mounted
		// force is issued nothing, so without this the crew spends the rest of the deployment trying to
		// inch onto the exact metre and the driver LEANS ON THE HORN FOREVER while it fails.
		//
		// Play-test, 2026-08-23: a QRF echelon found parked short of the battle sounding continuously.
		// Its behaviour module is a mobile checkpoint authored with m_iRelocateMinutes 0, so Relocate()
		// never fires and IssueDrive()'s DetachForeignWaypoints() - the workaround that had been hiding
		// this on the harassment config, which relocates every 4 minutes - never ran either.
		ClearOwnedWaypoints(ResolveCrewGroup());

		OnInsertionArrived(m_vLZ);

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Mounted force '%1' arrived at %2 with %3 group(s) still aboard; it holds its vehicle from here",
			DescribeSelf(), m_vLZ.ToString(), delivered.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! \return This force's crew group, or null when there is no registration or no manager to ask.
	protected SCR_AIGroup ResolveCrewGroup()
	{
		if (m_iCrewHandle == -1)
			return null;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return null;

		return virtualization.GetGroup(m_iCrewHandle);
	}

	//------------------------------------------------------------------------------------------------
	// The hold
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! ⚠ super.OnUpdate() FIRST AND UNCONDITIONALLY. It runs the abandoned-vehicle sweep (which only
	//! ever fires in WALKING and FINISHED, states its own dispatch does nothing for) and the UNDECIDED
	//! retry that is the only reason "a faction that has lost every base" is a retry rather than a
	//! permanent failure. Guarding it behind the state would switch both off.
	//! \param[in] deltaTime Milliseconds since the last update of this module.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (m_eState != OVT_EInsertionState.HOLDING)
			return;

		TickHold();
	}

	//------------------------------------------------------------------------------------------------
	//! One observation of a force holding its vehicle.
	//!
	//! The two tests are the drive's two, in the drive's order and for the drive's reason: losing the
	//! vehicle or the crew ends the hold wherever it happens, and it ends it on the march rather than
	//! in a wreck. Everything else about a parked mounted force belongs to a behaviour module.
	protected void TickHold()
	{
		if (!m_Truck || !IsTruckOperational())
		{
			DismountAndWalk("its vehicle was destroyed");
			return;
		}

		if (!IsCrewAlive())
		{
			DismountAndWalk("its vehicle lost its crew");
			return;
		}

		// ⚠ RE-ASSERTED EVERY TICK, exactly as the drive does it. A parked mounted force is not an idle
		// one - its gunner is manning a gun and a behaviour module may order it to relocate - and the
		// per-agent LOD system is free to push a crewman back towards max LOD at any moment, whatever
		// ring he is registered on. A crew that loses the pin is a materialised crew with no behaviour
		// tree: the gun stops, and the next relocate order is held and never executed.
		HoldRidersActive();
		NudgeCrewMaterialisation();

		TickHoldClock();
	}

	//------------------------------------------------------------------------------------------------
	//! Counts the hold out, once, when a config asked for a bounded one.
	//!
	//! ⚠ IT LATCHES AND DOES NOT COLLECT. See the class header: collection and the exfiltration rule
	//! that makes it safe belong to the behaviour modules, and a spawning module cannot reach either.
	protected void TickHoldClock()
	{
		if (m_iHoldTicks <= 0)
			return;

		if (m_bHoldExpired)
			return;

		m_iHoldTicksElapsed = m_iHoldTicksElapsed + 1;

		if (m_iHoldTicksElapsed < m_iHoldTicks)
			return;

		m_bHoldExpired = true;

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Mounted force '%1' has held its position for %2 update(s), which is what it was authored for",
			DescribeSelf(), m_iHoldTicksElapsed.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! THE CREW IS THE FORCE on every shipped mounted config (they carry no passenger groups), and the
	//! crew is registered under its OWN owner key - so no wipe is ever attributed to this module and
	//! the deployment would otherwise linger with nobody in it and be refunded as if intact.
	//!
	//! ⚠ Measured BEFORE super, because ReleaseConvoy() hands the crew registration back and
	//! IsCrewAlive() answers false for everything afterwards. A force that lost only its VEHICLE still
	//! has men, and must keep its deployment.
	//! \param[in] reason What ended the drive or the hold.
	override protected void DismountAndWalk(string reason)
	{
		bool crewLost = !IsCrewAlive();

		super.DismountAndWalk(reason);

		if (!crewLost || !m_aHandles.IsEmpty())
			return;

		SetSpawnedUnitsEliminated(true);

		if (m_ParentDeployment)
			m_ParentDeployment.CheckAllSpawningModulesEliminated();
	}

	//------------------------------------------------------------------------------------------------
	// The runtime seams
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Prefers a runtime source over the authored provider (D7).
	//!
	//! ⚠ The override wins only while nothing has been resolved yet, which is the same rule the parent
	//! applies to its own answer: an insertion that has begun keeps its source even if the base it came
	//! from changes hands mid-drive.
	//! \return True when there is an origin to work from.
	override protected bool EnsureSourceResolved()
	{
		if (m_vSource != vector.Zero)
			return true;

		if (m_vSourceOverride != vector.Zero)
		{
			m_vSource = m_vSourceOverride;
			m_bSourceWarned = false;
			return true;
		}

		return super.EnsureSourceResolved();
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE THIS FORCE COMES FROM, decided at runtime by the caller that stood the deployment up - the
	//! QRF echelon and the base armour sortie both need "come from THIS base", which is not a fact any
	//! authored provider knows.
	//!
	//! ⚠ IT MUST BE APPLIED BEFORE THE FIRST CONVERGENCE. The source decides both the anchor the force
	//! is registered at and the ring it is registered on, and neither can be changed afterwards without
	//! throwing away the survivor mask. Activation is not synchronous with creation - a deployment's
	//! modules are cloned and initialised on the creating call stack but only ACTIVATED at the end of
	//! its first 8-12 s update - so a caller that sets this immediately after ForceCreateDeployment has
	//! a whole update interval of margin. This warns rather than silently doing nothing if it is late.
	//! \param[in] source Where the force sets out from. vector.Zero clears the override.
	void SetSourceOverride(vector source)
	{
		if (m_vSource != vector.Zero && source != m_vSource)
		{
			Print(string.Format("[Overthrow] Mounted force '%1' was given a runtime source at %2 after it had already resolved one at %3 - the late one is ignored, because the force is registered against the first",
				DescribeSelf(), source.ToString(), m_vSource.ToString()), LogLevel.WARNING);
			return;
		}

		m_vSourceOverride = source;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands this module a vehicle that already exists instead of having it spawn one.
	//!
	//! ⚠ INERT UNLESS THE CONFIG AUTHORS m_bAdoptExistingVehicle - the hull is remembered and never
	//! used, so a caller that hands one to the wrong config leaves a vehicle standing rather than
	//! taking one over silently.
	//!
	//! ⚠ THE HANDOVER IS NOT A COPY. Once this module has taken a hull it will delete it at teardown
	//! like any other, so whoever handed it over must have released its own ownership first or the
	//! vehicle is deleted twice.
	//! \param[in] vehicle The hull to take. Null is refused.
	//! \return True when the hull was accepted.
	bool AdoptVehicle(Vehicle vehicle)
	{
		if (!vehicle)
			return false;

		if (m_Truck)
		{
			Print(string.Format("[Overthrow] Mounted force '%1' was handed a vehicle but already has one - the offer is refused rather than leaking the first",
				DescribeSelf()), LogLevel.WARNING);
			return false;
		}

		m_AdoptedVehicle = vehicle;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The vehicle this force is riding, or null. THE seam for a behaviour module that parks,
	//! relocates or fights with it.
	Vehicle GetMountedVehicle()
	{
		return m_Truck;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The hull handed in by AdoptVehicle(), or null. ⚠ NOT the same question as
	//! GetMountedVehicle(): an adopted hull is owned from the moment it is accepted, but only becomes
	//! m_Truck at the module's first SpawnTruck(), a whole update interval later.
	Vehicle GetAdoptedVehicle()
	{
		return m_AdoptedVehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the authored hold has run out. Always false when m_iHoldTicks is 0, which is an
	//! indefinite hold. The behaviour module that owns collection is what acts on this.
	bool IsHoldExpired()
	{
		return m_bHoldExpired;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The registry name of the rung actually picked, or empty when the ladder was never
	//! consulted or answered nothing.
	string GetResolvedVehicleName()
	{
		return m_sResolvedVehicleName;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The runtime source that was set, or vector.Zero when none was.
	vector GetSourceOverride()
	{
		return m_vSourceOverride;
	}

	//------------------------------------------------------------------------------------------------
	// Cloning
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! ALL TWENTY-EIGHT: the grandparent's thirteen, the parent's eleven and this module's four.
	//!
	//! ⚠ CloneModule is NOT chained - it builds a fresh instance and copies BY HAND - so every line of
	//! both ancestors is repeated here verbatim, and anything appended to either is appended here as
	//! well. A forgotten line does not warn, log or fail to parse: it ships the CLASS DEFAULT on every
	//! deployment, forever. This is the single highest-frequency defect in this module system.
	//!
	//! What a dropped line would cost among THIS module's four: drop m_sVehicleRole and every mounted
	//! force in the campaign silently reverts to a soft-skinned truck, so the escalation the whole
	//! feature exists for never happens; drop m_bDismountOnArrival and a config that wanted a lift gets
	//! a force that sits in its vehicle at the drop point forever; drop m_iHoldTicks and a bounded hold
	//! becomes an indefinite one, so a sweep never reports itself finished and its deployment is never
	//! collected; drop m_bAdoptExistingVehicle and the base armour sortie spawns a SECOND vehicle
	//! beside the one the player watched it crew. Among the inherited eleven, drop
	//! m_bTransportIsObserver and every mounted echelon drives on an unsimulated hull - which is to say
	//! it does not drive at all whenever nobody is watching, which is most of the time.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_MountedForceSpawningDeploymentModule clone = new OVT_MountedForceSpawningDeploymentModule();

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

		// --- Inherited from OVT_InsertionSpawningDeploymentModule, all eleven. The provider is shared
		//     rather than deep-copied, exactly as the parent shares it: a provider is a stateless answerer.
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
		clone.m_bTransportIsObserver = m_bTransportIsObserver;

		// --- This module's own four.
		clone.m_sVehicleRole = m_sVehicleRole;
		clone.m_bDismountOnArrival = m_bDismountOnArrival;
		clone.m_iHoldTicks = m_iHoldTicks;
		clone.m_bAdoptExistingVehicle = m_bAdoptExistingVehicle;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintMountedForceDebugInfo()
	{
		PrintInsertionDebugInfo();

		string resolved = m_sResolvedVehicleName;
		if (resolved.IsEmpty())
			resolved = "not resolved through the ladder";

		Print(string.Format("  Vehicle role: %1  Picked rung: %2", m_sVehicleRole, resolved));
		Print(string.Format("  Hold ticks: %1 of %2  Expired: %3",
			m_iHoldTicksElapsed.ToString(), m_iHoldTicks.ToString(), m_bHoldExpired.ToString()));
		Print(string.Format("  Runtime source override: %1", m_vSourceOverride.ToString()));
	}
}
