//------------------------------------------------------------------------------------------------
//! What stage of its journey an insertion is at.
//!
//! ⚠ WALKING IS AN OUTCOME, NOT AN ERROR STATE, and five different things lead to it: the hop was too
//! short to be worth a truck, no convoy slot was free, no transport could be put on the road, the
//! transport stopped making progress, and the transport was destroyed. Four of those are failures of
//! the drive and none of them is a failure of the INSERTION - in every one of them the force exists,
//! holds the plan it was registered with, and reaches the objective on foot.
//------------------------------------------------------------------------------------------------
enum OVT_EInsertionState
{
	//! Nothing decided yet. Re-entered on every convergence until an origin can be resolved, so a
	//! faction that currently holds no base is not a permanent failure - it is a retry.
	UNDECIDED,

	//! On foot. The terminal state of every path that is not a live convoy.
	WALKING,

	//! A truck is on the road with the force aboard.
	DRIVING,

	//! The force is down; the empty truck is going home.
	RETURNING,

	//! The convoy is over and everything it owned has been handed back.
	FINISHED
}

//------------------------------------------------------------------------------------------------
//! LIVE INSERTION: registers a deployment's force at a place it could plausibly have come FROM, puts
//! it in a truck, drives that truck down real roads to a landing zone short of the objective, drops
//! it, and sends the truck home.
//!
//! ==========================================================================================
//! THE WALK FALLBACK IS THE SPINE OF THIS FILE, NOT ITS ERROR HANDLING.
//! ==========================================================================================
//!
//! Read the class in this order and it makes sense; read it as "a convoy, with some error paths" and
//! it does not. The force is registered FIRST, with a plan that already points at the objective, and
//! is registered whether or not any of the driving works. Everything about the truck is an
//! OPTIMISATION laid on top of a march that would have happened anyway. Five things can take the
//! truck away - a hop too short to bother with, a convoy cap already spent, a missing vehicle prefab,
//! a truck that stopped making progress and a truck that was destroyed - and all five land in exactly
//! the same place: the men are on the ground, they hold the plan they were registered with, and they
//! walk. A path that leaves men in a dead truck is a defect, not a degradation.
//!
//! WHY IT SUBCLASSES THE INFANTRY MODULE rather than sitting beside it. OVT_ReinforcementBehaviorDeploymentModule
//! .GetMissingUnitsCount() casts to OVT_InfantrySpawningDeploymentModule and answers 0 for anything
//! else, so a sibling class would silently never be rebought after a wipe. The same reasoning
//! base-defense-migration recorded in its D6.
//!
//! IT HAS NO IDEA WHAT AN OBJECTIVE IS, AND THAT IS D7. Nothing here resolves, references or names the
//! component that owns a faction's long-term intent; the only place-specific question it asks -
//! "where does this force come from?" - is asked through OVT_DeploymentSourceProvider, and the
//! concurrency cap it respects lives on the deployment manager. A future config that wants a supply
//! run, a reinforcement convoy or a resistance-side insertion authors a provider and a .conf and
//! changes nothing else.
//!
//! WHAT IT OWNS AND WHAT IT BORROWS, because the split is the whole reason the teardown is careful:
//!   OWNED, and leaked if this module forgets them:
//!     - the truck (a plain world entity, deleted at teardown unless a player has made it theirs, and
//!       collected on a bounded countdown when it is abandoned short of one - see TickAbandonedTruck,
//!       which exists because the forward base's deployment never reaches a teardown at all);
//!     - the crew's registration (a SEPARATE owner key from the passengers' - see EnsureCrew);
//!     - two AIWaypoint entities, one out and one home. ⚠ AIGroup.AddWaypoint() does NOT take
//!       ownership; a leaked waypoint per insertion compounds over a campaign;
//!     - one slot of the deployment manager's per-faction convoy cap. ⚠ A leaked reservation is
//!       PERMANENT: nothing can hand it back, and enough of them stop the faction driving for the
//!       rest of the campaign. Every exit path below releases.
//!   BORROWED from the base class, and never torn down here:
//!     - the passenger groups. They are the deployment's actual force, registered under the base
//!       class's owner key, reclaimed by it and released by its OnCleanup.
//!
//! ⚠ A LIVE CONVOY NEEDS TWO STAMPS AND NOT ONE, AND THE SECOND ONE IS NEW (2026-08-21). The crew has
//! to EXIST wherever the players are - that is the 100 km riding ring, RIDING_SPAWN_DISTANCE - and it
//! also has to be RUNNING, which no ring can deliver: the per-agent LOD system switches an agent's
//! behaviour tree off entirely at max LOD, roughly a kilometre from the nearest observer. A crew with
//! only the first stamp is a materialised driver asleep at the wheel, and a convoy that "never left its
//! spawn point" with a perfectly alive crew, which is exactly what a play-test produced from 2.4 km
//! away. OVT_MountedGroupActivation is the second stamp; HoldRidersActive() applies it and
//! ReleaseRidersActive() hands it back, from ReleaseConvoy, on every exit path this file has.
//!
//! //! ⚠ A CONVOY IS NEVER RESUMED ACROSS A LOAD. Vehicles are not persisted, so a save taken mid-drive
//! comes back with the force somewhere along a road and no truck under it. Re-spawning a truck at the
//! source and expecting men who are three kilometres away to board it is not a restore, it is a
//! second insertion. A restored deployment therefore walks, always - which is the fallback, working.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_InsertionSpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
{
	//! WHERE THE FORCE COMES FROM - the modder seam. Left unauthored, this module refuses to register
	//! anything at all, which is deliberate: "reinforcements arrive from somewhere real" is the rule it
	//! exists to enforce, and a null provider means nobody has said where that is.
	[Attribute(desc: "Where this deployment's force sets out from. Ship OVT_NearestControlledBaseSourceProvider unless a config has a better answer. REQUIRED - with none, this module registers nothing")]
	ref OVT_DeploymentSourceProvider m_Source;

	[Attribute(defvalue: "400", desc: "Below this separation between the source and the objective, no truck is spawned at all - the force simply walks. 0 disables the rule and always sends a truck")]
	float m_fWalkThresholdDistance;

	[Attribute(defvalue: "truck", desc: "Vehicle type name from the faction VEHICLE registry")]
	string m_sTruckVehicleType;

	[Attribute(defvalue: "truck_crew", desc: "Group type name from the faction GROUP registry, used to crew the transport")]
	string m_sTruckCrewGroup;

	[Attribute(defvalue: "300", desc: "How far short of the objective the transport stops, in metres. Clamped: a standoff longer than the whole journey drops the force at the source rather than behind it")]
	float m_fLZStandoffDistance;

	[Attribute(defvalue: "1", desc: "Below this ground speed in m/s the transport is not making progress")]
	float m_fStuckSpeedThreshold;

	//! ⚠ A TICK HERE IS ONE DEPLOYMENT UPDATE, i.e. roughly TEN SECONDS - so the default is about a
	//! minute of complete immobility. It is deliberately generous, because the cost of calling a convoy
	//! stuck is a force dumped in open country, and because a truck legitimately stands still while its
	//! crew materialises through the AI spawn queue and boards.
	[Attribute(defvalue: "6", desc: "Consecutive update ticks (about 10 s each) below the speed threshold before the force dismounts and walks. 0 disables the stuck test entirely")]
	int m_iStuckTicks;

	[Attribute(defvalue: "40", desc: "How close to the landing zone counts as arrived, in metres")]
	float m_fArrivalRadius;

	//! ⚠ CHARGED WHETHER OR NOT A TRUCK EVER REACHES A ROAD, and it has to be. A deployment's price is
	//! computed from the CONFIG TEMPLATE before the deployment exists, months of campaign time before
	//! this module knows whether the hop is long enough, whether a slot is free or whether the faction
	//! even has a prefab for the vehicle. It is a budgeted cost, not a receipt.
	//!
	//! ⚠ WITH EXACTLY ONE EXCEPTION, ADDED 2026-08-19: an origin that has NO VEHICLES AT ALL is not
	//! charged for one. That is knowable at pricing time - it is a property of the place, and the source
	//! provider can be asked about the world without a deployment - where none of the other five reasons
	//! an insertion ends up walking are. See GetResourceCost() and
	//! OVT_DeploymentSourceProvider.MayProvideTransport(). In practice this is the occupying faction's
	//! forward operating base: while it stands, every operation launched from it is cheaper by this
	//! amount, which is the advantage the base is supposed to confer.
	[Attribute(defvalue: "40", desc: "Added to this module's resource cost to cover the transport. Budgeted at creation time, so it is charged even on deployments that end up walking - EXCEPT where the source provider reports the origin has no vehicles at all, e.g. a forward operating base")]
	int m_iTruckCostOverride;

	//! What to do when the faction's convoy cap is already spent. TRUE walks the force in now; FALSE
	//! leaves the whole insertion undecided and tries again on the next convergence, which is right for
	//! a config where arriving by truck matters more than arriving soon.
	[Attribute(defvalue: "1", desc: "When every convoy slot is taken: TRUE walks the force in immediately, FALSE waits for a slot and retries on the next update")]
	bool m_bWalkWhenInsertionRefused;

	//! HOW SLOW A TRANSPORT INSIDE THE ARRIVAL RADIUS HAS TO BE BEFORE ITS DOORS OPEN, in m/s.
	//!
	//! ⚠ WHY THERE IS A SPEED CONDITION AT ALL. Disembarking puts a passenger beside the truck carrying
	//! the truck's own velocity; on a transport braking hard into its drop point that throws the force
	//! across the road and injures it. The prefab's friction was raised on 2026-08-19 to stop trucks
	//! getting stuck, which worked and made them brake much harder, and the injuries appeared with it -
	//! but the drop was only ever safe by luck, so this gates on the truck being stopped rather than on
	//! any particular tuning of the driving.
	//!
	//! ⚠ IT IS MEASURED AGAINST A REAL SPEEDOMETER, AND IT WAS NOT ALWAYS. Until 2026-08-19 the only
	//! speed this module had was SpeedFromTravel's - two origins one ~10 s deployment update apart - and
	//! feeding an AVERAGE OVER THE WHOLE TICK to an "has it stopped yet" question is what made the drop
	//! late: the tick in which a transport brakes from road speed to a halt still averages several m/s,
	//! so the earliest a stationary truck could possibly be recognised was the NEXT tick, and any creep
	//! at all pushed it out to the settle grace. The author play-tested it as "the truck stops and they
	//! sit there for twenty seconds", which is exactly two ticks. TruckGroundSpeed() now reads the
	//! transport's actual velocity for this test; the tick average is still what the STALL test uses,
	//! where being immune to spinning wheels is the entire point. Read the two together and do not swap
	//! them back.
	//!
	//! WHY 0.5 m/s SURVIVED THE CHANGE OF MEASUREMENT UNCHANGED, which is not obvious - the old number
	//! was justified partly BY the averaging ("far stricter than it looks"), and that argument is gone:
	//!   - AS AN INSTANTANEOUS READING IT IS STILL COMFORTABLY SAFE. 0.5 m/s is a third of the 1.4 m/s
	//!     of a walking man, and a walking pace is the speed at which being put on the ground is
	//!     harmless. What the passenger inherits is the truck's velocity at the instant the doors open,
	//!     which is precisely what is now being read, so the threshold and the hazard are finally the
	//!     same quantity.
	//!   - IT DOES NOT NEED TO ABSORB MEASUREMENT NOISE ANY MORE. A parked vehicle's physics velocity is
	//!     ~0, not the 0.1-0.2 m/s that suspension settle and waypoint shuffling used to show up as in a
	//!     10 s position delta, so the headroom that argument bought is now spare rather than spent.
	//!   - IT IS STILL HALF m_fStuckSpeedThreshold's DEFAULT, so a truck this test calls "still moving"
	//!     is one the stall test agrees is moving, and the two knobs cannot disagree about a creeping
	//!     transport. That relationship was worth preserving on its own.
	//!
	//! ⚠ THE REMAINING LATENCY IS THE TICK, NOT THIS NUMBER, and it is deliberately left alone. The drop
	//! can still only happen on a deployment update, so a truck that stops just after one waits up to ten
	//! seconds - average five. Closing that would mean a CallLater inside this module, and the header of
	//! TickAbandonedTruck() sets out at length why this module does not own one: it is thrown away on
	//! seventeen release paths and a queued call holding a pointer into it would have to be removed on
	//! every one of them. Five seconds is not worth a dangling-pointer class of bug.
	//!
	//! A const rather than an attribute for the reason RETURN_TIMEOUT_TICKS is one: nobody tunes "how
	//! stopped is stopped", and one more authored field is one more thing CloneModule can drop.
	static const float ARRIVAL_SETTLE_SPEED_MS = 0.5;

	//! Where a crew is registered relative to its truck, in metres along X - the same offset the
	//! vehicle module uses, and for the same reason: close enough to board, not inside the geometry.
	static const float CREW_SPAWN_OFFSET_M = 5;

	//! Where passengers are registered relative to the truck. Further out than the crew so the two
	//! groups do not materialise inside one another.
	static const float PASSENGER_SPAWN_OFFSET_M = 9;

	//! AI spawn-budget tier for the transport crew. NEVER leave a registration unstamped: an unstamped
	//! group inherits vanilla's LOW tier, is capped at half the AI budget and is evicted first - which
	//! for a crew means a truck with nobody in it.
	//!
	//! ⚠ HIGH SINCE 2026-08-21, AND IT WAS NORMAL. The tier is not only a budget cap - the engine's spawn
	//! queue DRAINS IN IMPORTANCE ORDER (SCR_AIGroup.RequestSpawn's own doc, :2668-2670), so at NORMAL a
	//! transport crew queues level with every scripted patrol, town garrison and remnant in a live
	//! campaign and is served in turn with them.
	//!
	//! TWO MEN WHOSE ABSENCE COSTS A WHOLE INSERTION ARE NOT LEVEL WITH A GARRISON RIFLEMAN. A garrison
	//! that materialises a minute late is a garrison that materialised; a crew that materialises a minute
	//! late is a convoy that has already been written off as driverless, a force dumped at its source,
	//! and a truck abandoned on a vehicle spawn the next insertion needs. HIGH (capRatio 0.90, vanilla's
	//! "base defenders" tier - SCR_EAISpawnImportance) puts them ahead of the crowd they were queuing
	//! behind and costs at most two men of headroom per live convoy, which the per-faction convoy cap
	//! already bounds.
	//!
	//! ⚠ IT IS NOT OFFERED AS THE FIX FOR THE UNMATERIALISED CREWS OF 2026-08-21, and must not be read
	//! as one. Whether the queue was SLOW (ordering, which this changes) or NEVER DISPATCHING (which this
	//! would not touch) is the exact thing the instrumentation in modded SCR_AIGroup was added to settle.
	//! This is defensible on its own terms either way; it is not evidence and it is not a diagnosis.
	static const SCR_EAISpawnImportance CREW_IMPORTANCE = SCR_EAISpawnImportance.HIGH;

	//! How many update ticks a crew is given to MATERIALISE before the insertion gives up and walks.
	//!
	//! ==========================================================================================
	//! 🔴 A CREW THAT HAS NOT SPAWNED YET IS NOT A CREW THAT REFUSES TO DRIVE, AND UNTIL 2026-08-21
	//! THIS FILE COUNTED THEM ON THE SAME CLOCK.
	//! ==========================================================================================
	//! *"You understand that spawning AI groups do not spawn the members on the same frame usually?"*
	//! (author, 2026-08-21.) Member spawning is asynchronous: a registration hands ChimeraAIWorld a
	//! request, the queue re-validates and dispatches it in importance order over following frames, and
	//! the men appear when it gets to them. Zero members shortly after registering is the NORMAL state
	//! of a group, not a fault.
	//!
	//! The uncrewed budget (m_iStuckTicks, 6 ticks, ~60 s) was written for "a man is standing beside the
	//! truck and will not get in" and was being spent on "the queue has not reached us yet". Those want
	//! completely different deadlines, and 60 s is not obviously generous for the second one on a live
	//! campaign - it is a guess that a play-test has now failed three times.
	//!
	//! 18 ticks is ~3 real minutes. Deliberately far beyond anything a healthy queue needs, because the
	//! cost of being too SHORT is the visible one the author keeps reporting (every insertion after the
	//! first walks), while the cost of being too long is a force standing at its own base for two extra
	//! minutes, 1.4 km from anybody, before doing exactly what it would have done anyway. It is still a
	//! bound: nothing waits forever, and the outcome remains the walk fallback, which is the system
	//! working.
	//!
	//! ⚠ IT IS SEPARATE FROM m_iStuckTicks ON PURPOSE and must not be folded back into it. See
	//! m_iUnmaterialisedTicksElapsed - this file's own header already makes exactly this argument for
	//! why the stall clock and the uncrewed clock are two counters, and this is the third question in
	//! that family: "is anyone there yet" is not "is anyone driving" is not "is it moving".
	static const int CREW_MATERIALISE_TICKS = 18;

	//! ALWAYS MATERIALISED. Both the crew and, while they are riding, the passengers register at this
	//! ring. It is not a luxury and it is not tunable:
	//!   - a DORMANT crew holding a route plan is walked in a straight line across the map by the
	//!     movement tick while its truck stays parked, and materialises kilometres from its own
	//!     vehicle (OVT_VehicleSpawningDeploymentModule's header records the same finding);
	//!   - a DORMANT passenger group cannot be seated, because there is nobody to seat. A truck driving
	//!     through country with no player near it would arrive empty and the whole insertion would be
	//!     theatre.
	//! The passengers are dropped back to the global ring the moment they are on the ground - see
	//! RestoreGlobalSpawnRing(), which is the one place in this file that writes to a core record.
	//!
	//! ==========================================================================================
	//! 🔴 THIS RING IS NECESSARY AND IS NOT SUFFICIENT, AND BELIEVING OTHERWISE COST A PLAY-TEST.
	//! ==========================================================================================
	//! A ring answers "do these men exist". It says NOTHING about whether anything is running inside
	//! them. The per-agent LOD system deactivates an agent outright at max LOD - roughly a kilometre
	//! from the nearest observer, and independent of every spawn ring - so a crew registered here is a
	//! materialised driver in a materialised truck with no behaviour tree, holding the move waypoint
	//! this module gave him and never executing it. That is precisely what a convoy that "never left
	//! its spawn point" while IsCrewAlive() answers true looks like, and it is what the author saw from
	//! 2.4 km away on 2026-08-21.
	//!
	//! ⚠ DO NOT RAISE THIS NUMBER IN RESPONSE TO A CONVOY THAT WOULD NOT MOVE. It is already 100 km;
	//! there is no larger ring. The second gate is OVT_MountedGroupActivation, which is stamped on the
	//! crew by HoldRidersActive() and released on every exit path - read that file's header before
	//! touching this constant.
	static const int RIDING_SPAWN_DISTANCE = 100000;

	//! Mirrors OVT_VirtualizationManagerComponent's own m_fDespawnHysteresis default. It is protected
	//! there with no getter, and this module needs it only to re-stamp ONE transient policy change on
	//! groups it put on a truck; a server that dialled the core value elsewhere gets a slightly
	//! different anti-thrash band on those groups until they are next re-registered, which is
	//! invisible. Not worth widening a frozen API for.
	static const float DESPAWN_HYSTERESIS = 1.15;

	//! Appended to the module name to key the CREW's registration. ⚠ THE CREW MUST NOT SHARE THE
	//! PASSENGERS' OWNER KEY: the base class rebuilds its handle list from FindGroupsByOwner on every
	//! convergence, so a crew under the same key would be reclaimed as a passenger, counted towards the
	//! force size, driven off in its own truck and released by the wrong teardown.
	static const string CREW_KEY_SUFFIX = "crew";

	//! Separates the deployment-scoped part of a crew key from the per-insertion serial appended to it.
	//! See GetCrewOwnerKey() for why the serial exists at all.
	static const string CREW_INSTANCE_MARK = "i";

	//! How many update ticks an empty truck gets to reach home before it is released where it stands.
	//! A const rather than an attribute: nobody tunes "how long before we stop caring about an empty
	//! truck", and one more authored field is one more thing CloneModule can drop.
	static const int RETURN_TIMEOUT_TICKS = 60;

	//! How many update ticks a truck this module ABANDONED is left standing before it is collected.
	//!
	//! ⚠ TWICE RETURN_TIMEOUT_TICKS, DELIBERATELY, RATHER THAN A NUMBER SOMEBODY LIKED. That constant is
	//! this file's existing answer to "how long before we stop caring about an empty truck" (~10 real
	//! minutes at the deployment's 10 s update); an ABANDONED one gets twice as long because it may
	//! still have men's kit in it, it is somewhere a player might plausibly walk, and it is the one the
	//! header calls a landmark and a lootable. Deriving it keeps the two in a stated relationship instead
	//! of letting them drift.
	//!
	//! 120 ticks is ~20 real minutes (16-24 with the deployment's 0.8-1.2x update stagger). Long enough
	//! that a player who watches a convoy stall and drives to it from the nearest town - typically 1-3 km,
	//! so 3-8 minutes - arrives with time to spare, and the proximity hold then keeps it for as long as he
	//! is there. Short enough that a route which strands a truck per insertion carries at most a couple at
	//! a time instead of every one it ever stranded: the forward-base deployment STANDS INDEFINITELY, so
	//! before this its truck was never collected at all, and the garrison runs behind it drive the same
	//! road to the same place (user play-test, 2026-08-19).
	//!
	//! ==========================================================================================
	//! ⚠ SUPERSEDED AS THE LIVE BUDGET ON 2026-08-21 - SEE STUCK_TRUCK_TIMEOUT_TICKS. THE REASONING
	//! ABOVE IS KEPT BECAUSE IT IS WHAT THE NEW RULE OVERRULED, AND A READER NEEDS TO KNOW THAT.
	//! ==========================================================================================
	//! Nothing arms a countdown with this any more. It remains declared as the module's statement of
	//! what a NON-failed-drive abandonment would be worth, and it is the realistic limit the Logic case
	//! OVT_TEST_Logic_ObjectiveInsertion_CollectsAbandonedTransportsOnlyWhenDue exercises the pure
	//! predicate against - that case asserts IsAbandonedTruckCollectable, not this module's choice of
	//! budget, and is untouched.
	static const int ABANDONED_TRUCK_TIMEOUT_TICKS = RETURN_TIMEOUT_TICKS * 2;

	//! How many update ticks a transport ABANDONED BY A FAILED DRIVE is left standing before it is
	//! collected, once nobody can see it go.
	//!
	//! ==========================================================================================
	//! 🔴 A STUCK TRUCK IS NOT A LANDMARK, IT IS THE NEXT CONVOY'S ROADBLOCK (author, 2026-08-21).
	//! ==========================================================================================
	//! *"We do want to clean up the truck though. In my opinion if an insertion gets stuck and there's no
	//! players around anywhere we can just delete the truck and its crew to minimize pile ups on that
	//! route."*
	//!
	//! ABANDONED_TRUCK_TIMEOUT_TICKS above was written around the idea that a stranded transport is worth
	//! something as scenery and as loot, and gave it twenty real minutes. What that misses is that every
	//! insertion from a given base drives the SAME ROAD to the same objective, so a transport stranded on
	//! that road is still there when the next one comes down it - and the next one now has to path around
	//! it, which is one of the things that strands trucks. Failures compound into a queue of wrecks on one
	//! route. ArmAbandonedTruck's own header already recorded the same compounding at the vehicle spawn
	//! end of the road; this is that argument applied to the rest of it.
	//!
	//! ONE TICK, NOT ZERO, AND THE DIFFERENCE IS NOT COSMETIC. A non-positive budget means "never collect"
	//! to OVT_InsertionGeometry.IsAbandonedTruckCollectable - deliberately, it is that function's
	//! documented off-switch - so 1 is the smallest budget that means "as soon as possible". In practice
	//! that is the very next deployment update, i.e. within ten seconds of the drive being written off.
	//!
	//! ⚠ THE PLAYER HOLD IS UNCHANGED AND IS STILL ABSOLUTE. This shortens the DELAY; it does not touch
	//! the rule that a transport is never taken away while somebody is within
	//! ABANDONED_TRUCK_PLAYER_RADIUS_M of it, and that hold still never expires. Both of those are
	//! asserted by the shipped Logic case and neither assertion was altered - the change here is which
	//! number this module hands the predicate, not what the predicate does.
	//!
	//! ⚠ AND THE COST, STATED RATHER THAN DISCOVERED LATER. The old twenty minutes also protected a case
	//! the 320 m test cannot see: a player who watches a convoy stall from a kilometre away and drives
	//! out to loot it. He is far outside the radius, so the transport is now gone before he arrives. That
	//! is a real loss and it is the price of the pile-up fix; if it matters more than the roadblocks,
	//! the knob is ABANDONED_TRUCK_PLAYER_RADIUS_M rather than this one, because the question is "who
	//! counts as watching", not "how long do we wait".
	static const int STUCK_TRUCK_TIMEOUT_TICKS = 1;

	//! How close a live player has to be for an abandoned truck to be left exactly where it is.
	//!
	//! 320 m is the framework's existing "a player would notice that" distance -
	//! OVT_NoPlayersNearbyConditionDeploymentModule's default, itself the legacy baseCloseRange (220) plus
	//! 100. The same number is used here for the mirror-image reason: that module refuses to make things
	//! APPEAR inside it, and this one refuses to make things DISAPPEAR inside it.
	static const int ABANDONED_TRUCK_PLAYER_RADIUS_M = 320;

	//! How far the source may be from a base marker for that base's authored vehicle spawns to be
	//! treated as ITS spawns. 250 m is OVT_DeploymentManagerComponent.BASE_CLASSIFICATION_RADIUS - the
	//! radius the rest of the framework uses to decide a position IS a base, and the same one
	//! OVT_RoadSlotOverwatchPlacementProvider asks the base controller at. The shipped provider answers
	//! a base marker's exact location, so this is only really a guard against a MODDED provider (a
	//! forward base, a port) borrowing the markers of a base it merely stands near.
	static const float SOURCE_BASE_RADIUS_M = 250;

	//! HOW CLOSE TO ITS OWN SPAWN A STALLED TRANSPORT COUNTS AS "never left", in metres.
	//!
	//! 30 m is chosen against the two things it has to separate, not as a round number. It must be
	//! comfortably WIDER than MARKER_CLEARANCE_M (6) plus a truck's own length, so a transport that
	//! rolled a few metres and stalled still counts as blocking the marker it is fouling. And it must be
	//! far TIGHTER than any distance at which a truck is worth keeping as a landmark - a convoy that
	//! made it out of the compound and stalled on the road is hundreds of metres away and is left alone.
	//! Nothing in between is a case that occurs: a truck either fails to get going at all or gets going.
	static const float ABANDONED_AT_SPAWN_RADIUS_M = 30;

	//! How much room a transport needs at an authored marker before that marker counts as free. Wider
	//! than the vehicle manager's car-sized 3 m default because the thing being placed is a truck, and
	//! because these markers are SHARED with the vehicle-patrol deployments - a patrol vehicle sitting
	//! on one is the normal case this exists to skip past.
	static const float MARKER_CLEARANCE_M = 6;

	//! How long the fallback march plan pauses at the objective before walking to it again. Long enough
	//! to be silent, short enough that a group displaced by a fight walks back. Mirrors the shape of
	//! the parked-recruit hold loop.
	static const float MARCH_HOLD_SECONDS = 3600;

	//------------------------------------------------------------------------------------------------
	// RUNTIME STATE - none of it persisted, all of it re-derived. See the class header.
	//------------------------------------------------------------------------------------------------

	protected OVT_EInsertionState m_eState;

	//! Where the force sets out from, resolved once through m_Source and then reused for the whole
	//! insertion so a base changing hands mid-drive cannot move the landing zone under the convoy.
	protected vector m_vSource;

	//! Where the truck is going. Decided before anything is registered, because the stuck fallback and
	//! the arrival test both measure against it.
	protected vector m_vLZ;

	//! WHERE THE TRANSPORT GOES HOME TO: the exact spot it was spawned on, not the base.
	//!
	//! ⚠ THIS IS NOT m_vSource AND THE DIFFERENCE IS THE WHOLE REASON IT EXISTS. m_vSource is the base's
	//! own position - the middle of it, wherever the controller sits - and the return leg used to drive
	//! at that, which meant a truck that had set out from a designer-placed vehicle spawn on the access
	//! road came back and tried to park in the centre of the compound, through whatever is standing
	//! there (author play-test, 2026-08-19). ResolveTruckSpawn() has already answered "where does a
	//! vehicle belong at this base" properly - an authored OVT_VehiclePatrolSpawn if there is a free one,
	//! the nearest road if there is not - so the return leg simply reuses that answer instead of asking a
	//! worse question.
	//!
	//! Zero until a transport has been spawned, which is exactly when the return leg can first run; the
	//! two readers fall back to m_vSource anyway, so a walk-path insertion that never had a truck cannot
	//! be sent to the origin of the map.
	protected vector m_vHome;

	protected Vehicle m_Truck;

	protected int m_iCrewHandle;

	//! THIS INSERTION'S OWN crew owner key, minted once on first use and never recomputed. See
	//! GetCrewOwnerKey() - the whole point is that no other insertion, ever, can compose this string.
	protected string m_sCrewOwnerKey;

	//! Session-wide serial handed to crew keys, one per insertion that ever asks for one.
	//!
	//! A plain static rather than anything cleverer: it never has to survive a load (a convoy is never
	//! resumed across one - see the class header - so no restored module asks for a crew key at all),
	//! it never has to be dense, and it only has to be different from the last one.
	static int s_iCrewKeySerial;

	protected bool m_bReserved;

	protected int m_iStuckTicksElapsed;

	//! Consecutive update ticks the transport has been INSIDE the arrival radius without having settled
	//! enough to open its doors. Reset the moment it is outside again.
	//!
	//! ⚠ A SECOND COUNTER RATHER THAN A READING OF m_iStuckTicksElapsed, and it has to be. That one is
	//! reset by AdvanceStuckTicks on any observation of movement, which is the correct rule for "has the
	//! road AI given up" and the wrong one for "how long have we been waiting for this thing to stop":
	//! a truck jittering fast-slow-fast on its landing zone would reset the stall counter forever and the
	//! wait would never end, which is the one outcome the settle grace exists to make impossible. This
	//! one only ever counts up while the truck is at the landing zone. It borrows the stall test's
	//! BUDGET (m_iStuckTicks) rather than adding a second authored number - see IsSettleGraceExpired.
	protected int m_iInsideRadiusTicks;

	//! Consecutive update ticks the transport has been standing on the road WITH NOBODY DRIVING IT.
	//!
	//! ==========================================================================================
	//! ⚠ WHY A TRUCK WITH NO DRIVER IS NOT A STUCK TRUCK, AND WHY THE OLD MESSAGE WAS A LIE.
	//! ==========================================================================================
	//! The stall test measures ground covered and concludes "the road AI has given up". That conclusion
	//! requires there to BE a road AI: a transport whose crew has not materialised, has not boarded, or
	//! has materialised into max LOD with its behaviour tree switched off is not failing to drive, it is
	//! failing to have a driver, and those two want different messages and different remedies. Counting
	//! them on the same clock is how the log came to say "its transport NEVER LEFT ITS SPAWN POINT - its
	//! transport stopped making progress 1994 m SHORT OF THE LANDING ZONE" in one sentence, which reads
	//! as a contradiction because it is one.
	//!
	//! ⚠ IT BORROWS m_iStuckTicks AS ITS BUDGET rather than adding a second authored number, exactly as
	//! the settle grace does. "How long do we wait for nothing at all to happen" is one question with one
	//! right answer, and a config that disables the stall test (0) disables this with it - which is
	//! correct: an author who has switched off "give up on this convoy" has switched off both reasons to.
	protected int m_iUncrewedTicksElapsed;

	//! Consecutive update ticks the transport's crew has had NO MEN IN IT AT ALL.
	//!
	//! ==========================================================================================
	//! ⚠ THE THIRD COUNTER, AND IT EXISTS FOR EXACTLY THE REASON THE SECOND ONE DOES.
	//! ==========================================================================================
	//! m_iUncrewedTicksElapsed's own header argues that "the road AI has given up" and "there is no
	//! driver" are different failures wanting different messages, and that counting them on one clock
	//! produced a log line that contradicted itself. This is the same argument one step further back:
	//! "nobody has spawned yet" is not "nobody will drive". AI group members are produced by
	//! ChimeraAIWorld's queue over following frames, so an empty crew immediately after registration is
	//! the ordinary state of a new group - and charging it against a 60 s "he will not get in the truck"
	//! budget wrote off three insertions in a row on 2026-08-21.
	//!
	//! Its budget is CREW_MATERIALISE_TICKS, which is much larger, and the two are mutually exclusive by
	//! construction: on any tick either the crew has men (the uncrewed clock runs) or it does not (this
	//! one runs). Neither ever runs while somebody is at the wheel.
	protected int m_iUnmaterialisedTicksElapsed;

	protected vector m_vLastTruckPosition;

	//! False until the truck has been observed once. Without it the first tick would measure a speed
	//! against a zero vector - a "distance" of tens of thousands of metres, or, if it were clamped, a
	//! free stuck tick out of a limit that may only be three.
	protected bool m_bHaveLastTruckPosition;

	protected int m_iReturnTicksElapsed;

	//! Whether m_Truck is a transport this module walked away from and is now only holding until it can
	//! be collected. Set at exactly one place - ReleaseConvoy() with deleteTruck false - and cleared the
	//! moment there is no truck to hold.
	//!
	//! ⚠ AN EXPLICIT FLAG RATHER THAN "m_Truck is set and we are not driving". The implicit test is true
	//! for one tick during states it has no business firing in, and the cost of being wrong is a truck
	//! deleted out from under a convoy that was still using it.
	protected bool m_bTruckAbandoned;

	//! Update ticks since the abandonment. Counts UP to STUCK_TRUCK_TIMEOUT_TICKS, which is 1 - so on
	//! every abandonment this module makes, the only thing between the transport and collection is the
	//! player-proximity hold.
	protected int m_iAbandonedTicksElapsed;

	//! Latches the one VERBOSE line explaining that an overdue transport is being kept because somebody
	//! is standing near it, so "why is that truck still there" has an answer in the log without one line
	//! every ten seconds for as long as a player camps beside it.
	protected bool m_bAbandonedHoldLogged;

	//! Every waypoint this module spawned. AIGroup.AddWaypoint() does NOT take ownership, so these are
	//! deleted by hand on every exit. Entity handles null themselves out when the engine deletes one.
	protected ref array<AIWaypoint> m_aOwnedWaypoints;

	//! Group ENTITY id -> whether that group is the CREW (true) or a PASSENGER (false). Keyed on the
	//! entity because that is all the engine's per-member callback gives us, and rebuilt from scratch
	//! every session because a restored group's entity id is a new one.
	protected ref map<ref EntityID, bool> m_mRiderIsCrew;

	//! Latches the "nowhere to come from" warning so a faction that has lost every base does not fill
	//! the log at one line per module per update.
	protected bool m_bSourceWarned;


	//------------------------------------------------------------------------------------------------
	void OVT_InsertionSpawningDeploymentModule()
	{
		m_eState = OVT_EInsertionState.UNDECIDED;
		m_iCrewHandle = -1;
		m_aOwnedWaypoints = new array<AIWaypoint>();
		m_mRiderIsCrew = new map<ref EntityID, bool>();
	}

	//------------------------------------------------------------------------------------------------
	//! The force, plus the transport it is budgeted to arrive in - UNLESS ITS ORIGIN HAS NO VEHICLES.
	//!
	//! ⚠ THE PROVIDER IS ASKED, NOT THE DISTANCE, and it is asked through MayProvideTransport() rather
	//! than SourceProvidesTransport() because there is no position to hand it here: a price is computed
	//! from the config TEMPLATE, before any deployment exists. See the seam's own header for why the two
	//! methods are separate and which way a disagreement is allowed to fall.
	//!
	//! Everything else about m_iTruckCostOverride is unchanged and still deliberate: it is a BUDGET, not
	//! a receipt, so an insertion that is priced for a truck and then walks for any of the other five
	//! reasons - a short hop, a refused convoy slot, a missing prefab, a stuck truck, a destroyed one -
	//! still pays for it. Only "this origin has no motor pool" is knowable at pricing time, and only that
	//! one is discounted.
	override int GetResourceCost()
	{
		if (m_Source && !m_Source.MayProvideTransport())
			return super.GetResourceCost();

		return super.GetResourceCost() + m_iTruckCostOverride;
	}

	//------------------------------------------------------------------------------------------------
	// The convergence
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings this insertion up to what it wants: a decision, a transport if one is warranted, and the
	//! force itself. ALWAYS SAFE TO CALL, any number of times, in any order.
	//!
	//! ⚠ THE ORDER OF THE THREE STEPS IS LOAD-BEARING.
	//!   1. DECIDE FIRST, because the decision picks both the anchor the force is registered AT and the
	//!      proximity ring it is registered ON, and neither can be changed after the fact without
	//!      re-registering - which would throw away the survivor mask.
	//!   2. THE TRANSPORT SECOND, before the force exists. Passengers are registered beside the truck
	//!      so they can be seated the instant they materialise; with no truck yet there is nothing to
	//!      register beside. This is also what makes the failure ordering right - a truck that cannot
	//!      be spawned diverts to WALKING while the force is still unregistered, so it is then
	//!      registered spread around its source on the global ring, exactly as a walking force should
	//!      be.
	//!   3. THE FORCE LAST, through the base class, which reclaims before it registers and therefore
	//!      converges rather than stacking a second force on a restored one.
	//!
	//! ⚠ THIS IS NOT A PERIODIC METHOD, AND KNOWING THAT IS LOAD-BEARING. Nothing in the framework ticks
	//! it: it is reached from the deployment's ONE activation and from the manager's records-restored
	//! fan-out, and that is all (grep `EnsureGroups()` - there is no third caller). Everything this
	//! module has to keep doing while a convoy runs - watching for arrival, the stall and uncrewed
	//! tests, holding the crew's AI awake - therefore lives in OnUpdate(), and the ONE case that needs
	//! the convergence itself to run again (an insertion with nowhere yet to come from) is re-driven
	//! from there explicitly. Writing a retry here and assuming something would call it again is the
	//! mistake this note exists to stop.
	//!
	//! ⚠ SEATING IS NOT ON THAT LIST ANY MORE, AND ITS ABSENCE IS DELIBERATE (2026-08-21). "Seating late
	//! arrivals" used to be an OnUpdate job and is now an ARRIVAL job - OnRiderAgentAdded seats each man
	//! as the spawn queue produces him. The convergence still runs one boarding sweep, precisely because
	//! it is not periodic; see BoardEveryone() for what a periodic one did to a convoy at a gate.
	override void EnsureGroups()
	{
		if (!m_ParentDeployment)
			return;

		if (m_eState == OVT_EInsertionState.UNDECIDED)
		{
			DecideInsertion();

			// ⚠ STILL UNDECIDED MEANS REGISTER NOTHING, AND IT HAS TO BE SAID EXPLICITLY. There are two
			// ways to get here and they look identical from below: there is nowhere for the force to
			// come from, or the config would rather wait for a convoy slot than walk. Falling through
			// would put the force on the ground AT ITS SOURCE with a march plan - i.e. it would walk -
			// which is exactly the decision that has not been made yet. (The first case happens to be
			// safe anyway, because ResolveSpawnAnchor refuses with no source; the second is not, and
			// relying on the accident would be a trap for whoever changes the anchor.)
			if (m_eState == OVT_EInsertionState.UNDECIDED)
				return;
		}

		if (m_eState == OVT_EInsertionState.DRIVING)
			EnsureConvoy();

		super.EnsureGroups();

		if (m_eState == OVT_EInsertionState.DRIVING)
		{
			// ⚠ THE ONLY SEATING SWEEP LEFT, AND IT IS SAFE HERE BECAUSE THIS METHOD IS NOT PERIODIC.
			// See EnsureGroups' own header: nothing ticks it - it is reached from the deployment's ONE
			// activation and from the records-restored fan-out, and that is all. So this fires when the
			// convoy is committed and essentially never again, which is exactly the "only when the team
			// spawns" contract. The per-tick sweep that used to sit in TickDrive is gone; see
			// BoardEveryone() for what it cost and what replaced it.
			BoardEveryone();

			// Every path that seats also pins, so the two can never drift apart. Reached on the
			// records-restored fan-out as well as on activation, and idempotent on both.
			HoldRidersActive();
			return;
		}

		// EVERYTHING THAT IS NOT RIDING BELONGS ON THE ORDINARY PROXIMITY RING, and this is the sweep
		// that guarantees it however the state got here - including the force of a deployment restored
		// mid-drive, whose records came back out of the save still carrying the riding ring. Run
		// unconditionally rather than behind a latch on purpose: RestoreGlobalSpawnRing() answers
		// immediately for a group that is already on the global ring, which is every group on almost
		// every pass, and a latch here has to be cleared correctly on five different paths to be
		// right - one missed clear is a squad materialised for the rest of the campaign.
		DropPassengersToGlobalRing();
	}

	//------------------------------------------------------------------------------------------------
	//! Chooses between a march and a convoy, once, and commits to a source and a landing zone.
	//!
	//! ⚠ EVERY BRANCH THAT IS NOT "DRIVE" IS A BRANCH THAT STILL DELIVERS THE FORCE. The only outcome
	//! that registers nothing at all is "there is nowhere for this force to come from", and that one
	//! stays UNDECIDED rather than settling, so a faction that takes a base back later gets its
	//! insertion then.
	protected void DecideInsertion()
	{
		if (!EnsureSourceResolved())
			return;

		// A convoy is state that lives only in this session's memory: a truck, a crew, two waypoints
		// and a reservation. None of it is in the save, so there is nothing to resume - see the class
		// header.
		if (m_ParentDeployment.WasRestoredFromSave())
		{
			EnterWalking("it came back from a save point, and a convoy is never resumed across a load");
			return;
		}

		vector target = m_ParentDeployment.GetPosition();
		float separation = vector.Distance(m_vSource, target);

		if (OVT_InsertionGeometry.ShouldWalk(separation, m_fWalkThresholdDistance))
		{
			int separationMetres = Math.Round(separation);
			int thresholdMetres = Math.Round(m_fWalkThresholdDistance);

			EnterWalking(string.Format("the objective is %1 m away, inside the %2 m walk threshold",
				separationMetres.ToString(), thresholdMetres.ToString()));
			return;
		}

		// ⚠ THE SIXTH WAY TO END UP WALKING, AND THE ONLY ONE THAT IS ABOUT THE PLACE RATHER THAN THE
		// DISTANCE. Asked AFTER the threshold so a short hop still reports the reason a reader expects,
		// and BEFORE the convoy slot so an origin with no vehicles never claims one - a reservation
		// taken here would be held for the length of a march and refuse a real convoy that could have
		// used it. See OVT_DeploymentSourceProvider.SourceProvidesTransport() for why a place can
		// refuse: a forward operating base is a field camp with no motor pool and often no road, and
		// the truck it was given reliably stranded within a minute of setting off.
		if (m_Source && !m_Source.SourceProvidesTransport(m_vSource, m_ParentDeployment.GetControllingFaction()))
		{
			EnterWalking(string.Format("its origin (%1) has no transport to give it", m_Source.GetProviderName()));
			return;
		}

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
		{
			EnterWalking("the deployment framework could not be resolved, so no convoy slot could be claimed");
			return;
		}

		if (!manager.TryReserveInsertion(m_ParentDeployment.GetControllingFaction()))
		{
			if (m_bWalkWhenInsertionRefused)
			{
				EnterWalking(string.Format("all %1 of the faction's convoy slots are taken",
					manager.GetMaxConcurrentInsertions().ToString()));
				return;
			}

			// The config would rather wait for a truck than arrive on foot. Nothing is registered this
			// pass and the next convergence asks again.
			return;
		}

		m_bReserved = true;

		// Decided BEFORE anything is registered, so a config with an impossible standoff fails here
		// rather than three kilometres down a road, and so the stuck fallback has somewhere to measure
		// against from the very first tick.
		m_vLZ = ResolveLandingZone(m_vSource, target);

		m_eState = OVT_EInsertionState.DRIVING;
		m_iStuckTicksElapsed = 0;
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;
		m_iInsideRadiusTicks = 0;
		m_bHaveLastTruckPosition = false;
		m_iReturnTicksElapsed = 0;

		// ⚠ THE DISTANCE QUOTED IS THE DRIVE, NOT THE SEPARATION, AND IT USED TO BE THE WRONG ONE. This
		// line said "driving <separation> m ... to a landing zone at <LZ>" while the landing zone sits
		// m_fLZStandoffDistance SHORT of the objective, so it over-reported the journey by the standoff -
		// 2418 m claimed for a 2070 m drive on 2026-08-21 - and every later line that measures a shortfall
		// against the LZ then disagreed with it by exactly that gap. A reader chasing a convoy that stopped
		// short should not have to work out which of two numbers the log meant.
		int driveMetres = Math.Round(vector.Distance(m_vSource, m_vLZ));
		int standoffMetres = Math.Round(vector.Distance(m_vLZ, target));

		Print(string.Format("[Overthrow] Insertion '%1': driving %2 m from %3 to a landing zone at %4, %5 m short of the objective",
			DescribeSelf(), driveMetres.ToString(), m_vSource.ToString(), m_vLZ.ToString(),
			standoffMetres.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves and caches the origin. Never overwrites one that is already set: an insertion that has
	//! begun keeps its source even if the base it came from changes hands mid-drive.
	//! \return True when there is an origin to work from.
	protected bool EnsureSourceResolved()
	{
		if (m_vSource != vector.Zero)
			return true;

		if (!m_ParentDeployment)
			return false;

		if (!m_Source)
		{
			WarnNoSource("no source provider is authored on it");
			return false;
		}

		vector resolved;
		if (!m_Source.ResolveSource(m_ParentDeployment.GetPosition(), m_ParentDeployment.GetControllingFaction(), resolved))
		{
			WarnNoSource(string.Format("its source provider (%1) found nowhere for the force to come from", m_Source.GetProviderName()));
			return false;
		}

		m_vSource = resolved;
		m_bSourceWarned = false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Says once, and then stops saying, that this insertion has nowhere to start from.
	//!
	//! THIS IS THE ONE FAILURE THAT REGISTERS NOTHING, and it is the rule the module exists to enforce:
	//! a force that appears at its objective out of thin air is exactly what live insertion replaces.
	//! It is logged at WARNING and named, because "the enemy never reinforces town X" has to have an
	//! explanation in the log rather than a repro.
	//! \param[in] reason Why there is no origin.
	protected void WarnNoSource(string reason)
	{
		if (m_bSourceWarned)
			return;

		m_bSourceWarned = true;

		Print(string.Format("[Overthrow] Insertion '%1' will register nothing: %2. Nothing is spawned from thin air; this is retried on every update",
			DescribeSelf(), reason), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! The landing zone: a point on the source->objective line, snapped onto a road where there is one.
	//!
	//! THE SNAP IS A PREFERENCE, NOT A REQUIREMENT. A truck dropped on a road is a truck that can drive
	//! away again; a truck dropped on the raw line point may be in a field. But the raw point is always
	//! reachable-ish and the road search is bounded, so no road within range simply uses the line
	//! point, surface-clamped.
	//! \param[in] source Where the convoy starts.
	//! \param[in] target The objective.
	//! \return A landing zone at ground height.
	protected vector ResolveLandingZone(vector source, vector target)
	{
		vector point = OVT_InsertionGeometry.LZPointOnLine(source, target, m_fLZStandoffDistance);

		vector roadPosition;
		vector roadAngles;
		if (OVT_WorldUtils.FindNearestRoadSpawn(point, OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, roadPosition, roadAngles))
			return roadPosition;

		// The geometry interpolates Y between two endpoints and knows nothing about the ground between
		// them; over 300 m of hillside that is metres out, and a landing zone under the terrain is one
		// a truck can never arrive at.
		BaseWorld world = GetGame().GetWorld();
		if (world)
			point[1] = world.GetSurfaceY(point[0], point[2]);

		return point;
	}

	//------------------------------------------------------------------------------------------------
	// The convoy
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Brings the transport up to what a drive needs: a truck, a crew, and an order to go.
	//!
	//! Each of the two things it can fail to build diverts to the march, and the divert happens BEFORE
	//! the force is registered (see EnsureGroups' step ordering), so the force is then registered as a
	//! walking force rather than as a stranded one.
	protected void EnsureConvoy()
	{
		if (m_bSpawnedUnitsEliminated || m_ParentDeployment.GetSpawnedUnitsEliminated())
		{
			// There is nobody left to carry. This is not a fallback - walking a force that does not
			// exist is meaningless - so the convoy is simply wound up.
			ReleaseConvoy("the force it was carrying has been wiped out", true);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
		}

		if (!m_Truck && !SpawnTruck())
		{
			FallBackToWalking("no transport could be put on the road");
			return;
		}

		if (m_iCrewHandle == -1 && !EnsureCrew())
		{
			FallBackToWalking("no crew could be registered for the transport");
			return;
		}

		IssueDriveOrder();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the truck in the world at the source.
	//! \return True when there is a truck.
	protected bool SpawnTruck()
	{
		if (m_sTruckVehicleType.IsEmpty())
		{
			Print(string.Format("[Overthrow] Insertion '%1' has no transport type authored", DescribeSelf()), LogLevel.WARNING);
			return false;
		}

		ResourceName prefab = GetVehiclePrefabFromFaction(m_ParentDeployment.GetControllingFaction());
		if (prefab.IsEmpty())
			return false;

		vector spawnPosition;
		vector spawnAngles;
		ResolveTruckSpawn(spawnPosition, spawnAngles);

		m_Truck = Vehicle.Cast(SpawnEntity(prefab, spawnPosition, spawnAngles));
		if (!m_Truck)
		{
			Print(string.Format("[Overthrow] Insertion '%1': transport '%2' failed to spawn at %3",
				DescribeSelf(), m_sTruckVehicleType, spawnPosition.ToString()), LogLevel.WARNING);
			return false;
		}

		// Recorded HERE rather than re-derived when the truck turns for home, and that is deliberate: by
		// then the marker it left from may well be occupied - quite possibly by this very deployment's
		// next convoy - and ResolveAuthoredTruckSpawn() would hand back a different spot or none at all.
		// Where it actually started is a fact, and the only honest answer to "go back where you came from".
		m_vHome = spawnPosition;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE THE TRUCK APPEARS: an authored marker at the source base if there is a free one, and the
	//! nearest road if there is not.
	//!
	//! ⚠ AN AUTHORED MARKER BEATS THE ROAD SNAP, ALWAYS, AND THAT IS THE WHOLE POINT OF THE MARKERS. The
	//! road snap answers "somewhere a vehicle could drive away from" with no knowledge of what is
	//! standing there: at a base it lands on the access road, which may be inside the wire, across a
	//! gate or nose-first into a wall. A designer who placed an OVT_VehiclePatrolSpawn has already
	//! answered the question properly, INCLUDING THE FACING - that is what the arrow in Workbench is.
	//!
	//! Everything below the marker branch is byte-for-byte the behaviour that shipped before markers
	//! were consulted, and it is reached on all four of: no base near the source, a base with no
	//! markers authored, every marker occupied, and no vehicle manager to ask about occupancy.
	//! \param[out] position Where to put the truck.
	//! \param[out] angles How to point it.
	protected void ResolveTruckSpawn(out vector position, out vector angles)
	{
		if (ResolveAuthoredTruckSpawn(position, angles))
			return;

		position = m_vSource;
		angles = vector.Zero;

		vector roadPosition;
		vector roadAngles;
		if (OVT_WorldUtils.FindNearestRoadSpawn(m_vSource, OVT_WorldUtils.ROAD_SPAWN_MAX_DISTANCE, roadPosition, roadAngles))
		{
			position = roadPosition;
			angles = roadAngles;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The authored answer, when the source base has one that is free.
	//!
	//! ⚠ NO SECOND WORLD QUERY FOR THE MARKERS. The base controller already swept baseRange for them at
	//! base init and cached them; this reads that cache through CollectVehiclePatrolSpawns(). The only
	//! query it runs is the occupancy test, once per candidate.
	//!
	//! THE CHOICE IS DETERMINISTIC - nearest free marker to the source, ties to the lower index - and
	//! deliberately NOT the base controller's GetRandomVehiclePatrolSpawn(). See
	//! OVT_InsertionGeometry.ChooseSpawnMarker, which is where the decidable part of this lives so it
	//! can be asserted without a world.
	//! \param[out] position The chosen marker's position.
	//! \param[out] angles The chosen marker's OWN facing - not the road's, not zero.
	//! \return True when a free authored marker was found and written out.
	protected bool ResolveAuthoredTruckSpawn(out vector position, out vector angles)
	{
		OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.FindNearestBaseControllerWithin(m_vSource, SOURCE_BASE_RADIUS_M);
		if (!baseController)
			return false;

		array<IEntity> markers = {};
		baseController.CollectVehiclePatrolSpawns(markers);

		if (markers.IsEmpty())
			return false;

		array<vector> positions = {};
		array<bool> blocked = {};

		OVT_VehicleManagerComponent vehicles = OVT_Global.GetVehicles();

		foreach (IEntity marker : markers)
		{
			vector markerPosition = marker.GetOrigin();
			positions.Insert(markerPosition);

			// No vehicle manager is not a reason to refuse an authored spot - it is a reason not to know
			// whether it is taken. ChooseSpawnMarker treats an unanswered index as free.
			if (!vehicles)
				continue;

			blocked.Insert(vehicles.IsSpotBlockedByVehicle(markerPosition, MARKER_CLEARANCE_M));
		}

		int chosen = OVT_InsertionGeometry.ChooseSpawnMarker(positions, blocked, m_vSource);
		if (chosen == -1)
			return false;

		position = positions[chosen];

		// YAW ONLY, through the shared helper. SpawnEntity()'s rotation parameter is in
		// Math3D.AnglesToMatrix order - "(yaw, pitch, roll)" - which is NOT the "(pitch, yaw, roll)"
		// that IEntity.GetAngles() returns. Passing GetAngles() straight through puts the marker's yaw
		// in the matrix's pitch slot and the transport spawns ON ITS NOSE. This code did exactly that
		// for a few hours on 2026-08-19; main was being fixed for the same class of bug at the time
		// (ed14ba9d, "OF vehicles would spawn flipped").
		//
		// GetUprightSpawnRotation() is the one place that conversion lives - do not hand-roll a second
		// copy, because a second copy is how this comes back. GetYawPitchRoll()[0] IS the yaw, so the
		// reader never has to work out which slot they are in; pitch and roll are discarded outright,
		// since an authored marker can carry a few degrees of terrain pitch (Eden's steepest is 4.7 deg)
		// and nothing about a heading should be able to tip a truck.
		angles = GetUprightSpawnRotation(markers[chosen].GetYawPitchRoll()[0]);

		Print(string.Format("[Overthrow] Insertion '%1': transport spawning on authored vehicle spawn %2 of %3 at %4",
			DescribeSelf(), (chosen + 1).ToString(), markers.Count().ToString(), position.ToString()), LogLevel.VERBOSE);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers the transport crew under its OWN owner key, beside the truck, always materialised AND
	//! always running.
	//!
	//! ⚠ TWO STAMPS, NOT ONE, AND THE SECOND ONE IS THE ONE PEOPLE FORGET. RIDING_SPAWN_DISTANCE makes
	//! the men EXIST from anywhere on the map; OVT_MountedGroupActivation makes their behaviour trees
	//! RUN from anywhere on the map. A crew with only the first is a driver asleep at the wheel - see
	//! that file's header, and see RIDING_SPAWN_DISTANCE for the play-test that proved it.
	//!
	//! ⚠ A NULL PLAN, DELIBERATELY, AND NOT THE BEHAVIOUR MODULES' ONE. ResolveVirtualPlan() asks the
	//! deployment's behaviour modules what a group of this deployment should do, and for the FORCE that
	//! is exactly right - but the crew is not the force. A harassment plan handed to the crew would
	//! send the truck into the town centre and hold it there. The crew's orders are issued by this
	//! module as ordinary waypoints it owns and deletes.
	//! \return True when there is a crew.
	protected bool EnsureCrew()
	{
		if (m_sTruckCrewGroup.IsEmpty())
		{
			Print(string.Format("[Overthrow] Insertion '%1' has no transport crew type authored", DescribeSelf()), LogLevel.WARNING);
			return false;
		}

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return false;

		string crewKey = GetCrewOwnerKey();
		if (crewKey.IsEmpty())
			return false;

		// Reclaim before registering, exactly as everything else in this framework does: a convergence
		// that ran twice, or one that follows a records-restored fan-out, must not put a second crew in
		// a truck that already has one.
		//
		// ⚠ AND IT CAN ONLY EVER FIND THIS INSERTION'S OWN CREW, which is a property of the KEY and not
		// of this loop - see GetCrewOwnerKey() for the serial that guarantees it and for the play-test
		// that made it necessary. Read this as "reclaim my crew", never as "reclaim a crew".
		array<int> found = virtualization.FindGroupsByOwner(OWNER_SYSTEM, crewKey);
		foreach (int foundHandle : found)
		{
			if (!virtualization.IsRegistered(foundHandle))
				continue;

			// ⚠ NEVER ADOPT A CREW THAT CAN NEVER BE REPOPULATED. A group with no men whose refill seam
			// reports COMPLETE is a husk: the engine's spawn queue books every request against it as
			// satisfied and drops it, so it will sit at 0 materialised for the rest of the campaign
			// while IsCrewAlive() answers true off the survivor mask - a truck that never gets a driver
			// and a force that walks, every single time. Inheriting one is strictly worse than the
			// double crew the reclaim exists to prevent, because a husk has nobody in it to be doubled.
			// Hand it back and register a fresh one below.
			if (IsCrewHusk(virtualization, foundHandle))
			{
				Print(string.Format("[Overthrow] Insertion '%1': crew handle %2 has no men and cannot be refilled - unregistering it and crewing the transport fresh",
					DescribeSelf(), foundHandle.ToString()), LogLevel.WARNING);

				virtualization.UnregisterGroup(foundHandle);
				continue;
			}

			m_iCrewHandle = foundHandle;
			break;
		}

		if (m_iCrewHandle == -1)
		{
			string factionKey = ResolveFactionKey(m_ParentDeployment.GetControllingFaction());
			if (factionKey.IsEmpty())
			{
				Print(string.Format("[Overthrow] Insertion '%1': faction index %2 resolves to no faction key, cannot crew the transport",
					DescribeSelf(), m_ParentDeployment.GetControllingFaction().ToString()), LogLevel.WARNING);
				return false;
			}

			vector crewPosition = m_Truck.GetOrigin() + Vector(CREW_SPAWN_OFFSET_M, 0, 0);

			m_iCrewHandle = virtualization.RegisterGroup(OWNER_SYSTEM, crewKey, factionKey, m_sTruckCrewGroup,
				crewPosition, null, RIDING_SPAWN_DISTANCE, CREW_IMPORTANCE);

			if (m_iCrewHandle == -1)
			{
				Print(string.Format("[Overthrow] Insertion '%1': registration of transport crew '%2' (%3) was refused",
					DescribeSelf(), m_sTruckCrewGroup, factionKey), LogLevel.WARNING);
				return false;
			}
		}

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (crew)
		{
			TagForGameMaster(crew);
			PairRider(crew, true);

			// Whoever is already standing. Everyone who arrives after this is pinned by
			// OnRiderAgentAdded as the spawn queue produces him, and the whole crew is re-pinned on
			// every drive tick - a crew fills PROGRESSIVELY, so one pass over it is never enough.
			OVT_MountedGroupActivation.HoldGroupActive(crew);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! IS THIS REGISTERED CREW AN UNREPOPULATABLE CORPSE?
	//!
	//! Two conditions, and BOTH are needed or the answer is a lie:
	//!   NOBODY IS STANDING     - a crew that has men is obviously fine, and a crew that is merely
	//!                            DORMANT also has none. Dormancy is not the question.
	//!   THE REFILL SEAM IS DONE - SCR_AIGroup.IsExpandComplete() is what the engine's spawn queue asks
	//!                            to decide "at capacity, drop this request" versus "transient failure,
	//!                            retry". TRUE with nobody in the group means every future request is
	//!                            dropped on arrival: for a core-owned group that is the survivor mask
	//!                            and the per-activation slot list out of agreement (modded
	//!                            SCR_AIGroup.ExpandOneMember), and for a vanilla one it is dormant
	//!                            counts saying the roster is spent.
	//!
	//! ⚠ A DORMANT CREW IS NOT A HUSK AND MUST NOT BE CALLED ONE. A group core has despawned reports
	//! IsExpandComplete() against a CLEARED slot list, so it answers "still has slots to fill" - which is
	//! exactly right, and is why this test does not need a dormancy clause of its own.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle A registered crew handle.
	//! \return True when the handle names a group that will never have men in it again.
	protected bool IsCrewHusk(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return true;

		if (group.GetAgentsCount() > 0)
			return false;

		return group.IsExpandComplete();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the crew a MOVE order to the landing zone, once.
	//!
	//! Idempotent by construction: it does nothing while this module already owns a waypoint, so the
	//! convergence can run every ten seconds for the whole drive without stacking orders.
	protected void IssueDriveOrder()
	{
		if (!m_aOwnedWaypoints.IsEmpty())
			return;

		IssueCrewMove(m_vLZ);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces whatever the crew was doing with a single MOVE.
	//!
	//! ⚠ EVERY WAYPOINT SPAWNED HERE IS REMEMBERED AND DELETED BY HAND. AIGroup.AddWaypoint() does not
	//! take ownership - a waypoint is an ordinary world entity - so the alternative is one leaked
	//! entity per leg per insertion, forever.
	//! \param[in] destination Where the truck should go.
	protected void IssueCrewMove(vector destination)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		ClearOwnedWaypoints(crew);

		AIWaypoint waypoint = config.SpawnMoveWaypoint(destination);
		if (!waypoint)
		{
			Print(string.Format("[Overthrow] Insertion '%1': a move waypoint could not be spawned, so the transport has no orders",
				DescribeSelf()), LogLevel.WARNING);
			return;
		}

		m_aOwnedWaypoints.Insert(waypoint);
		crew.AddWaypoint(waypoint);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes this module's waypoints off a group and deletes them.
	//! \param[in] crew The group holding them; null is legal (the group may already be gone).
	protected void ClearOwnedWaypoints(SCR_AIGroup crew)
	{
		foreach (AIWaypoint waypoint : m_aOwnedWaypoints)
		{
			if (!waypoint)
				continue;

			if (crew)
				crew.RemoveWaypoint(waypoint);

			SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}

		m_aOwnedWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	// The drive
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		// ⚠ BEFORE THE STATE DISPATCH, BECAUSE EVERY BRANCH BELOW RETURNS. A truck is only ever abandoned
		// in WALKING or FINISHED - states the dispatch does nothing for - so a sweep placed inside it
		// would never run at all. It is a no-op on every other path (the flag is false).
		TickAbandonedTruck();

		// ⚠ THE RETRY, AND THE ONLY PLACE IT CAN LIVE. EnsureGroups() is called once, at activation - see
		// its header - so an insertion that could not resolve an origin on that one pass would otherwise
		// stay undecided and register nothing for the rest of the campaign. "A faction that has lost
		// every base is not a permanent failure, it is a retry" is only true because of this line, and so
		// is "the config would rather wait for a convoy slot than walk". Both states are transient and
		// rare; a settled insertion never reaches this.
		if (m_eState == OVT_EInsertionState.UNDECIDED)
		{
			EnsureGroups();
			return;
		}

		if (m_eState == OVT_EInsertionState.DRIVING)
		{
			TickDrive(deltaTime);
			return;
		}

		if (m_eState == OVT_EInsertionState.RETURNING)
			TickReturn(deltaTime);
	}

	//------------------------------------------------------------------------------------------------
	//! One observation of a convoy on its way out.
	//!
	//! THE ORDER OF THE TESTS IS THE ORDER OF THEIR SEVERITY, and it matters. Losing the truck or the
	//! crew ends the drive wherever it happens to be; BEING AT THE LANDING ZONE beats being stuck (a
	//! truck standing still on its own landing zone is finished, not stranded - see
	//! OVT_InsertionGeometry.IsStuck); and only then is progress judged.
	//!
	//! ⚠ ARRIVING IS TWO CONDITIONS NOW, PLACE AND STILLNESS, and the second one is why the whole
	//! inside-the-radius case is a self-contained branch. Passengers are teleported out carrying the
	//! truck's velocity, so a force disembarked from a transport that is still braking is a force thrown
	//! across the road; the drop therefore waits for the truck to stop. A wait needs a bound, the bound
	//! is the settle grace, and both ends of it complete the insertion properly - see the branch.
	//! \param[in] deltaTime Milliseconds since the last update of this module.
	protected void TickDrive(int deltaTime)
	{
		if (!m_Truck || !IsTruckOperational())
		{
			DismountAndWalk("its transport was destroyed");
			return;
		}

		if (!IsCrewAlive())
		{
			DismountAndWalk("its transport lost its crew");
			return;
		}

		// ⚠ BEFORE ANY TEST, EVERY TICK. A crew fills progressively through the engine's spawn queue and
		// the LOD system is free to push a man back towards max LOD at any moment, so the pin is a
		// re-assert rather than a one-shot - and it has to land BEFORE the stall accounting below, or a
		// crewman who arrived this tick would be measured as "not driving" in the same tick he was
		// pinned. See OVT_MountedGroupActivation.
		HoldRidersActive();

		// And the gate BEFORE the pin: men who do not exist cannot be held awake. A no-op on every tick
		// of every convoy that has a crew - see NudgeCrewMaterialisation for the pop-in clause it exists
		// to get past.
		NudgeCrewMaterialisation();

		vector truckPosition = m_Truck.GetOrigin();
		float distanceToLZ = vector.Distance(truckPosition, m_vLZ);

		// ⚠ THE FIRST OBSERVATION IS NOT A MEASUREMENT AND MUST NOT COST A STALL. There is no previous
		// position to compare against, so the honest answer is "no reading yet" rather than "no speed" -
		// and with a stall limit that may be as low as three, a free stall on the very tick the convoy is
		// still boarding is a quarter of the budget spent on nothing.
		//
		// ⚠ NO READING IS READ AS STOPPED BY THE ARRIVAL TEST BELOW, and that is right rather than
		// merely convenient: the only way to be inside the arrival radius with no previous observation is
		// to have been spawned there, which happens whenever the standoff swallows the whole journey and
		// the landing zone collapses onto the source. That truck has not moved, and it must still arrive
		// on its first tick exactly as it did before there was a speed condition.
		//
		// ⚠ deltaTime IS THE NOMINAL 10 s, NOT THE REAL INTERVAL. The deployment's update timer is
		// deliberately staggered by 0.8-1.2x, and its caller passes the constant rather than the elapsed
		// time, so this speed carries up to 20% of error. That is fine for the only two questions asked of
		// it - "did this thing move at all" and "has it stopped yet" - and would not be for anything finer.
		float speed = 0;
		if (m_bHaveLastTruckPosition)
			speed = OVT_InsertionGeometry.SpeedFromTravel(m_vLastTruckPosition, truckPosition, deltaTime / 1000.0);

		// ⚠ EVERYTHING INSIDE THE RADIUS IS THE ARRIVAL PATH'S BUSINESS AND NOTHING ELSE'S. This branch
		// returns, so the stall test below is never even asked about a transport at its landing zone -
		// which keeps IsStuck's arrival exemption exactly as strong as it was, and stops the very
		// stationary-for-N-ticks counter that a SETTLING truck trips from reading it as stranded. There are
		// only two ways out of here and both are CompleteInsertion(): it settled, or it was given the whole
		// settle grace to settle and did not. Nothing waits forever.
		if (OVT_InsertionGeometry.IsInsideArrivalRadius(distanceToLZ, m_fArrivalRadius))
		{
			m_iInsideRadiusTicks = m_iInsideRadiusTicks + 1;

			// ⚠ THE LIVE READING, NOT THE TICK AVERAGE, AND ONLY HERE. `speed` above is the distance the
			// transport covered over the last ~10 s divided by 10, which cannot answer "has it stopped
			// YET" - the tick containing the braking averages road speed, so a stationary truck could
			// never be recognised before the following tick and the force sat inside it for twenty
			// seconds (author play-test, 2026-08-19). The stall test below still uses `speed`, on
			// purpose: there the question is "is this thing getting anywhere", and an average of two
			// origins is the only reading that a truck spinning its wheels against a wall cannot fool.
			if (OVT_InsertionGeometry.HasArrived(distanceToLZ, m_fArrivalRadius, TruckGroundSpeed(speed), ARRIVAL_SETTLE_SPEED_MS))
			{
				CompleteInsertion();
				return;
			}

			if (OVT_InsertionGeometry.IsSettleGraceExpired(m_iInsideRadiusTicks, m_iStuckTicks))
			{
				int settleTicks = m_iInsideRadiusTicks;

				Print(string.Format("[Overthrow] Insertion '%1': its transport reached the landing zone but never came to a stop in %2 update(s); dropping the force anyway",
					DescribeSelf(), settleTicks.ToString()), LogLevel.NORMAL);

				CompleteInsertion();
				return;
			}

			// ⚠ STILL BRAKING - AND WHAT KEEPS EVERYONE ABOARD IS THIS `return`, NOT A SEATING SWEEP.
			// This branch used to end with SeatEveryone() under the comment "keep everyone aboard while
			// it does", which read as though the sweep were what held the force in the truck. It was
			// not: nothing in this module takes anybody OUT until CompleteInsertion(), so simply not
			// completing is the whole mechanism. The sweep was a redundant re-assert, and it carried the
			// gate hazard (see EvictHijackers) into the one place a reader would least expect it - the
			// landing zone is road-snapped and only m_fLZStandoffDistance short of an enemy base, so a
			// gate beside it is an ordinary thing rather than a hypothetical.
			m_vLastTruckPosition = truckPosition;
			m_bHaveLastTruckPosition = true;

			return;
		}

		m_iInsideRadiusTicks = 0;

		// ==========================================================================================
		// ⚠ THE STALL CLOCK ONLY RUNS WHILE THERE IS SOMEBODY TO STALL. See m_iUncrewedTicksElapsed for
		// why an uncrewed transport is a different failure from a stuck one, and why saying so out loud
		// is worth a second counter.
		// ==========================================================================================
		if (!CrewIsAtTheWheel())
		{
			m_iStuckTicksElapsed = 0;

			// ==========================================================================================
			// 🔴 TWO CLOCKS, AND WHICH ONE RUNS DEPENDS ON WHETHER THERE IS ANYBODY THERE AT ALL.
			// ==========================================================================================
			// See m_iUnmaterialisedTicksElapsed. An AI group's members are produced by ChimeraAIWorld's
			// spawn queue over following frames, not on the frame it was registered, so an empty crew is
			// the ordinary state of a new group and NOT a transport that has failed to get a driver.
			// Charging it against the 60 s "he will not get in the truck" budget is what wrote off every
			// insertion after the first on 2026-08-21.
			if (CrewMaterialisedCount() == 0)
			{
				m_iUnmaterialisedTicksElapsed = m_iUnmaterialisedTicksElapsed + 1;

				// ⚠ ONE LINE PER TICK, ON PURPOSE, AND IT IS THE MEASUREMENT THAT SHOULD HAVE BEEN TAKEN
				// THREE ROUNDS AGO. A single line at the end of the window cannot tell "the queue is slow
				// and the count is climbing" from "the count was flat at zero the whole time", and those
				// are different bugs with different fixes. It is bounded by CREW_MATERIALISE_TICKS and only
				// ever appears while an insertion is actually failing to fill its cab.
				Print(string.Format("[Overthrow] Insertion '%1': waiting for its crew, update %2 of %3 - %4",
					DescribeSelf(), m_iUnmaterialisedTicksElapsed.ToString(), ResolveMaterialiseTicks().ToString(),
					DescribeCrewFill()), LogLevel.NORMAL);

				if (OVT_InsertionGeometry.IsUncrewedGraceExpired(m_iUnmaterialisedTicksElapsed, ResolveMaterialiseTicks()))
				{
					Print(string.Format("[Overthrow] Insertion '%1': its crew never materialised in %2 update(s) - %3",
						DescribeSelf(), m_iUnmaterialisedTicksElapsed.ToString(), DescribeCrewLiveness()), LogLevel.WARNING);

					DismountAndWalk("its transport's crew never materialised");
					return;
				}
			}
			else
			{
				m_iUncrewedTicksElapsed = m_iUncrewedTicksElapsed + 1;

				// BOUNDED BY THE SAME BUDGET, so an insertion whose crew turns up and then will not board
				// is still written off in about a minute and still walks - the fallback, working - rather
				// than standing beside a driverless truck for the rest of the campaign. A disabled stall
				// budget disables this too.
				if (OVT_InsertionGeometry.IsUncrewedGraceExpired(m_iUncrewedTicksElapsed, m_iStuckTicks))
				{
					Print(string.Format("[Overthrow] Insertion '%1': its crew is on the ground but nobody took the wheel in %2 update(s) - %3",
						DescribeSelf(), m_iUncrewedTicksElapsed.ToString(), DescribeCrewLiveness()), LogLevel.NORMAL);

					DismountAndWalk("its transport never got a driver");
					return;
				}
			}

			// Keep the observation fresh so the tick the crew DOES board is measured against where the
			// truck is now, not against wherever it was when it was last driven.
			//
			// ⚠ AND NO RE-SEAT HERE EITHER, THOUGH IT IS THE MOST TEMPTING PLACE FOR ONE. "Nobody is in
			// the driver's seat" looks like the unambiguous licence to put somebody back in it - but
			// SCR_AISelectDoorOperatorAgent scores PILOT (+200) above TURRET (0), so on a crew whose only
			// spare man is a gunner, or whose co-driver is dead, THE DRIVER is the one it sends to open
			// the gate. A re-seat here would teleport exactly that man back into his seat mid-task, over
			// and over, which is the jam this whole change exists to remove.
			m_vLastTruckPosition = truckPosition;
			m_bHaveLastTruckPosition = true;

			return;
		}

		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;

		if (m_bHaveLastTruckPosition)
		{
			m_iStuckTicksElapsed = OVT_InsertionGeometry.AdvanceStuckTicks(speed, m_fStuckSpeedThreshold, m_iStuckTicksElapsed);

			if (OVT_InsertionGeometry.IsStuck(speed, m_fStuckSpeedThreshold, m_iStuckTicksElapsed, m_iStuckTicks, distanceToLZ, m_fArrivalRadius))
			{
				int shortfallMetres = Math.Round(distanceToLZ);

				// ⚠ THE STATE OF THE CREW GOES IN THE LOG ON ITS OWN LINE, ALWAYS, AND THIS IS THE LINE
				// THAT DID NOT EXIST. "its transport never left its spawn point" was true and useless:
				// it could equally have meant an unmaterialised crew, a crew whose AI the LOD system had
				// switched off, a driver who never boarded, or a transport genuinely wedged against a
				// wall, and separating those took a play-test, a log dive and a read of vanilla's LOD
				// notes. One line here answers it. It is a SEPARATE line rather than part of the reason
				// string on purpose: the reason is repeated by three downstream messages and none of
				// them wants a paragraph.
				Print(string.Format("[Overthrow] Insertion '%1': its transport stalled at %2 - %3",
					DescribeSelf(), truckPosition.ToString(), DescribeCrewLiveness()), LogLevel.NORMAL);

				DismountAndWalk(string.Format("its transport stopped making progress %1 m short of the landing zone",
					shortfallMetres.ToString()));
				return;
			}
		}

		m_vLastTruckPosition = truckPosition;
		m_bHaveLastTruckPosition = true;

		// The only thing this module still polices on a moving truck: a member of the FORCE at the
		// wheel. Never anybody on foot - see EvictHijackers().
		EvictHijackers();
	}

	//------------------------------------------------------------------------------------------------
	//! HOW FAST THE TRANSPORT IS GOING RIGHT NOW, in m/s - a speedometer, not a tick average.
	//!
	//! ⚠ THIS EXISTS FOR THE ARRIVAL TEST AND NOTHING ELSE, and the distinction is the fix it was written
	//! for. "Is it safe to open the doors" is a question about the velocity a passenger would inherit at
	//! this instant, so it wants an instantaneous reading; "is this convoy getting anywhere" is a question
	//! about ground covered, so it wants SpeedFromTravel's average and would be actively wrong with this -
	//! a truck spinning its wheels against a wall reports plenty of velocity while going nowhere, which is
	//! the exact case the stall test's header says it must not be fooled by. Two questions, two readings.
	//!
	//! THE FALLBACK IS THE CALLER'S OWN AVERAGE rather than zero, and it has to be that way round. A
	//! transport with no physics object is one the engine is not simulating - most likely mid-despawn -
	//! and answering "stopped" for it would open the doors on whatever the tick average says is still a
	//! moving vehicle. Handing back the average restores exactly the pre-2026-08-19 behaviour for that
	//! case: later, but never unsafe, and still bounded by the settle grace.
	//! \param[in] fallbackSpeed The tick-average speed to use when the transport has no physics.
	//! \return Metres per second.
	protected float TruckGroundSpeed(float fallbackSpeed)
	{
		if (!m_Truck)
			return fallbackSpeed;

		Physics physics = m_Truck.GetPhysics();
		if (!physics)
			return fallbackSpeed;

		return physics.GetVelocity().Length();
	}

	//------------------------------------------------------------------------------------------------
	// The two liveness gates - see RIDING_SPAWN_DISTANCE and OVT_MountedGroupActivation
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Keeps the transport CREW out of max LOD, so its behaviour tree keeps running whatever the
	//! distance to the nearest observer.
	//!
	//! ==========================================================================================
	//! ⚠ THE CREW ONLY, AND THE PASSENGERS' OMISSION IS A DECISION RATHER THAN AN OVERSIGHT.
	//! ==========================================================================================
	//! The rule the author asked for is that a vehicle drives the road network and is ACTIVE, and it is
	//! the crew that drives. A pinned passenger is worse than a wasted one:
	//!   - HE HAS A PLAN AND IT POINTS AT THE OBJECTIVE. The force is deliberately registered holding
	//!     the behaviour module's plan for the whole ride - that is what lets every failure path just
	//!     open the doors and walk away - so an ACTIVE passenger is an AI with a live move order sitting
	//!     next to an empty seat. SeatRider's header records what that produced on 2026-08-20: a squad
	//!     leader took the wheel and drove the convoy to the objective instead of the landing zone.
	//!     Waking the whole squad up would make that race harder, not easier, and the eviction guard
	//!     only runs on the ~10 s update.
	//!   - HE MAY REACT TO A FIGHT HE CANNOT SEE THE POINT OF. Cargo at max LOD is inert; cargo with a
	//!     behaviour tree perceives, evaluates and can decide to get out.
	//!   - AND HE COSTS SIMULATION FOR NOTHING. A squad plus a crew, per live convoy, on a map where
	//!     nobody is within a kilometre of any of them.
	//! Passengers still ride ALWAYS-MATERIALISED (RIDING_SPAWN_DISTANCE), which is what seating them
	//! needs; they simply do not need to be awake to be carried.
	//!
	//! ⚠ AND IT IS A RE-ASSERT, NOT A ONE-SHOT. Called from the drive tick, the return tick and the
	//! convergence, because a core-registered group fills progressively through the AI spawn queue and
	//! the man who arrives on the third dispatch was not there to be pinned on the first.
	protected void HoldRidersActive()
	{
		if (m_iCrewHandle == -1)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		OVT_MountedGroupActivation.HoldGroupActive(virtualization.GetGroup(m_iCrewHandle));
	}

	//------------------------------------------------------------------------------------------------
	//! ASKS THE ENGINE, DIRECTLY, FOR A CREW THAT IS REGISTERED AND HAS NOBODY IN IT.
	//!
	//! ==========================================================================================
	//! 🔴 A 100 km RING IS A STATEMENT OF INTENT, AND THE ONLY THING THAT ACTS ON IT CAN REFUSE.
	//! ==========================================================================================
	//! RIDING_SPAWN_DISTANCE does not spawn anybody. What spawns a ProximityDriven group is
	//! SCR_AIGroup.LifecycleTick, once a second, and between "an observer is inside the ring" and "ask
	//! the queue for a man" that tick has a POP-IN CLAUSE (:3036-3044): an observer within
	//! m_fVeryNearBlockDistance - 150 m, and NOT settable through the -1 core passes
	//! SetLifecyclePolicy - makes it return having enqueued nothing, so men never appear in somebody's
	//! face. It is skipped only for an observer who arrived SUDDENLY, judged by this tick's
	//! inside-the-ring bit against last tick's.
	//!
	//! ⚠ A GROUP ON A 100 km RING IS PERMANENTLY INSIDE ITS OWN RING, so that bit is true from the first
	//! tick and the arrival is always judged gradual. The escape hatch is unreachable, and the result is
	//! that ANY observer within 150 m of where the crew was registered - a player at the motor pool, or
	//! a Game Master free camera, which is an observer too (SCR_DefenderSpawnerComponent:604) - stops
	//! that crew materialising for as long as he stands there, silently. A truck at its spawn with 2 of 2
	//! alive in the mask and 0 materialised is what that looks like from the log, and it is what a
	//! play-test produced twice in a row on 2026-08-21 after the first insertion of the session worked.
	//!
	//! WHY ForceSpawn IS THE RIGHT LEVER AND NOT A WORKAROUND. Core's ForceSpawn is
	//! SCR_AIGroup.RequestSpawn() with observerRange 0 - it goes into the same importance-ordered,
	//! budget-respecting queue as everything else, it is subject to the same AI cap, and it simply does
	//! not come from the lifecycle tick, so neither the pop-in clause nor the dispatch-time observer
	//! re-check applies to it. "This crew must exist wherever anybody is standing" is precisely what the
	//! registration already said; this is the API that says it to the engine.
	//!
	//! ⚠ IT IS ALSO WHAT MAKES THE DIAGNOSIS UNNECESSARY. Several different engine-side refusals produce
	//! the identical "0 materialised" line - the pop-in block, a request dropped at dispatch, a queue
	//! that never got round to it - and a fix aimed at one of them would be a guess. A direct request
	//! every tick answers all three without needing to know which.
	//!
	//! CHEAP AND IDEMPOTENT BY CONSTRUCTION: it returns on the first line for a crew that has men, which
	//! is every tick of every convoy that is working, and a request against a group already at capacity
	//! is dropped by the queue's own IsExpandComplete test.
	//!
	//! ⚠ THE ONE SIDE EFFECT, NAMED SO NOBODY REDISCOVERS IT: RequestSpawn also re-runs
	//! AddVehiclesStatic / AddWaypointsStatic / AddWaypointsDynamic from the group PREFAB
	//! (SCR_AIGroup.c:2706-2709), and a prefab naming a waypoint that does not exist in this world adds
	//! a NULL one - which is where the "Group contains null waypoints!" warnings in every Overthrow log
	//! come from. It is not introduced here: the engine's own LifecycleTick already calls RequestSpawn
	//! ONCE A SECOND for any empty in-range group, so this adds the same thing at a tenth of the rate,
	//! and only in the state where the tick has stopped calling it at all.
	//!
	//! ⚠ IT LOGS NOTHING OF ITS OWN, AND IT USED TO. The latched line it printed said "this crew has
	//! nobody in it" on the FIRST empty tick - which, now that it is understood that member spawning is
	//! asynchronous, is the ordinary state of every new group and was pure alarm. The per-tick trend line
	//! in TickDrive supersedes it completely and says strictly more.
	//!
	//! ⚠ AND THE HONEST ACCOUNTING OF WHAT THIS IS WORTH, measured rather than argued (play-test
	//! 2026-08-21, three insertions): it FIRED on all three failing crews and NONE of them materialised,
	//! so it is not the fix and is not claimed to be. It is also not the churn: SCR_AIGroup.LifecycleTick
	//! enqueues the identical request for the identical group ONCE PER SECOND while it is empty and in
	//! range, and this adds one per ten-second update - about a tenth of what vanilla is already doing to
	//! that queue on its own. It is kept because it is the only caller left on any path where the
	//! lifecycle tick returns early, and because a tenth of an existing cost is not a reason to remove a
	//! guarantee.
	protected void NudgeCrewMaterialisation()
	{
		if (m_iCrewHandle == -1)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		if (!virtualization.IsRegistered(m_iCrewHandle))
			return;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew || crew.GetAgentsCount() > 0)
			return;

		virtualization.ForceSpawn(m_iCrewHandle);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands EVERY rider back to the LOD system - the crew and the force alike, whatever this module
	//! thought their roles were.
	//!
	//! ⚠ WIDER THAN THE PIN, DELIBERATELY. AllowMaxLOD on a man who was never pinned is a no-op, so
	//! releasing everybody costs nothing and makes a role misclassification unable to strand a pin.
	//! "Pin narrowly, release widely" - see OVT_MountedGroupActivation's header.
	//!
	//! ⚠ THE ONE CALLER IS ReleaseConvoy(), AND THAT IS THE POINT. That method is this file's single
	//! audited teardown - every non-arrival exit reaches it and so does the successful one, by way of
	//! TickReturn - so there is exactly one release to keep correct rather than five. The ordinary
	//! proximity ring is the backstop underneath it: a group returned to it goes dormant, dormancy
	//! DELETES the member entities, and a pin cannot outlive the man carrying it.
	protected void ReleaseRidersActive()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		if (m_iCrewHandle != -1)
			OVT_MountedGroupActivation.ReleaseGroupActive(virtualization.GetGroup(m_iCrewHandle));

		foreach (int handle : m_aHandles)
		{
			OVT_MountedGroupActivation.ReleaseGroupActive(virtualization.GetGroup(handle));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! IS ANYBODY DRIVING THIS THING: a crewman with his AI running, in THIS transport's PILOT seat.
	//!
	//! ==========================================================================================
	//! ⚠ THE DRIVER'S SEAT SPECIFICALLY, NOT "ANY CREWMAN ABOARD". This was the looser test for a few
	//! hours on 2026-08-21 and the looser test is wrong, for a reason that only shows up once you know
	//! what vanilla does at a gate.
	//! ==========================================================================================
	//! SCR_AISelectDoorOperatorAgent picks a group member to get out and open a gate, and it scores
	//! CARGO (+400) above PILOT (+200): the man who leaves is the CO-DRIVER, and the driver stays where
	//! he is. So during a gate operation "any crewman aboard" and "somebody is driving" happen to agree -
	//! but they disagree in the case the counter actually exists for. A crew that materialised and never
	//! boarded properly can easily have a man in a CARGO seat and nobody at the wheel; that convoy is
	//! not being driven and the loose test would call it driven forever, which is the unbounded hang the
	//! uncrewed counter was added to make impossible.
	//!
	//! ⚠ THE PRICE, STATED PLAINLY: a truck whose DRIVER is the one out opening a gate reads as "no
	//! driver" for the duration. That happens when the picker has no better candidate - a two-man crew
	//! whose co-driver is dead, or one whose only other man is in a turret (scored 0, "gunner should be
	//! last to leave"). It costs uncrewed ticks, it is bounded by the same ~60 s budget as everything
	//! else here, and if the gate really takes longer than that the outcome is the walk fallback with a
	//! full liveness line in the log - not a hang. The alternative, deferring the bound whenever any
	//! crewman is on foot near the truck, cannot tell a man opening a gate from a man who failed to
	//! board and is standing beside it forever, and that ambiguity is exactly what the bound exists to
	//! refuse.
	//!
	//! ⚠ IsAIActivated() IS ASKED OF THE AGENT, NOT INFERRED FROM DISTANCE. It is the same bit vanilla's
	//! own HasHeldMember() consults, and it is the only honest answer to "is there a behaviour tree
	//! running in there".
	//!
	//! THE SEAT IS TESTED BY CAST, not by comparing GetType(), because PilotCompartmentSlot.Cast() is
	//! how the rest of the engine asks this question (SCR_ChimeraCharacter, SCR_BaseGameMode,
	//! CharacterCamera3rdPersonVehicle and SCR_AIDecoTestCanGroupDriveVehicle all do exactly this). And
	//! it is checked against OUR transport through GetVehicle(), which walks up to the root vehicle -
	//! a crewman who has climbed into somebody else's truck is not driving ours.
	//! \return True when the transport has a working driver in its driver's seat.
	protected bool CrewIsAtTheWheel()
	{
		if (!m_Truck)
			return false;

		if (m_iCrewHandle == -1)
			return false;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return false;

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return false;

		array<AIAgent> agents = {};
		crew.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			if (!agent.IsAIActivated())
				continue;

			IEntity character = agent.GetControlledEntity();
			if (!character)
				continue;

			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
			if (!access)
				continue;

			BaseCompartmentSlot slot = access.GetCompartment();
			if (!slot)
				continue;

			if (!PilotCompartmentSlot.Cast(slot))
				continue;

			if (slot.GetVehicle() != m_Truck)
				continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE LINE THAT SAYS WHY A CONVOY IS NOT MOVING, covering both gates and the distance that drives
	//! the second one.
	//!
	//! WHAT EACH SHAPE MEANS, because the whole value of the line is that a reader does not have to
	//! guess:
	//!   "0 materialised"                   - the group exists but has no men: a spawn-ring, AI-budget
	//!                                        or spawn-queue problem. Check the ring it reports.
	//!   "2 materialised, 0 AI-active"      - the men exist and nothing is running in them. That is the
	//!                                        LOD gate; OVT_MountedGroupActivation is not holding, or is
	//!                                        not being called. Compare the worst LOD against its max.
	//!   "2 materialised, 2 AI-active"      - both gates are open and the convoy is genuinely stuck.
	//!                                        Now it is a driving, navmesh or vehicle-tuning question.
	//! \return A compact human-readable description of the crew's liveness.
	//! The materialisation deadline AS IT APPLIES TO THIS CONFIG.
	//!
	//! ⚠ IT INHERITS m_iStuckTicks' OFF-SWITCH WITHOUT INHERITING ITS VALUE, and both halves are
	//! deliberate. The value has to be its own (see CREW_MATERIALISE_TICKS - waiting for a spawn queue
	//! and waiting for a man to open a door are not the same duration), but the OFF-SWITCH has to be
	//! shared, because this file already states the rule for the sibling counter: *"an author who has
	//! switched off 'give up on this convoy' has switched off both reasons to"*. A config with
	//! m_iStuckTicks 0 would otherwise still walk its force after three minutes, which is precisely the
	//! give-up it asked not to have.
	//! \return CREW_MATERIALISE_TICKS, or 0 when this config has disabled giving up altogether.
	protected int ResolveMaterialiseTicks()
	{
		if (m_iStuckTicks <= 0)
			return 0;

		return CREW_MATERIALISE_TICKS;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many of this crew's men are standing in the world right now.
	protected int CrewMaterialisedCount()
	{
		if (m_iCrewHandle == -1)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		return OVT_MountedGroupActivation.MaterialisedCount(virtualization.GetGroup(m_iCrewHandle));
	}

	//------------------------------------------------------------------------------------------------
	//! THE PER-TICK TREND LINE: how full the cab is right now and what the engine's spawn queue has done
	//! about it, short enough to print once every ten seconds without burying the log.
	//!
	//! ⚠ DELIBERATELY NOT DescribeCrewLiveness(). That one is a paragraph and is right for the ONE line
	//! at the end of a window; eighteen of them would be unreadable. What a trend needs is the two
	//! numbers that move - how many men exist, and how many times the queue has actually dispatched this
	//! group - because "0, 0, 0, 0" and "0, 1, 1, 2" are different bugs and no snapshot can tell them
	//! apart.
	//! \return A compact fill state.
	protected string DescribeCrewFill()
	{
		if (m_iCrewHandle == -1)
			return "no crew is registered";

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return "the virtualization manager could not be resolved";

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
		if (!crew)
			return string.Format("crew handle %1 has no group entity", m_iCrewHandle.ToString());

		return string.Format("crew handle %1: %2 of %3 materialised, %4 alive in the mask; %5",
			m_iCrewHandle.ToString(),
			OVT_MountedGroupActivation.MaterialisedCount(crew).ToString(),
			virtualization.GetMemberCount(m_iCrewHandle).ToString(),
			virtualization.GetAliveMemberCount(m_iCrewHandle).ToString(),
			crew.GetOVTSpawnQueueDiagnostic());
	}

	//------------------------------------------------------------------------------------------------
	protected string DescribeCrewLiveness()
	{
		if (m_iCrewHandle == -1)
			return "no crew is registered";

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return "the virtualization manager could not be resolved";

		if (!virtualization.IsRegistered(m_iCrewHandle))
			return string.Format("crew handle %1 is no longer registered", m_iCrewHandle.ToString());

		SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);

		string policy = "no group entity";
		if (crew)
			policy = typename.EnumToString(SCR_EAIGroupLifecyclePolicy, crew.GetLifecyclePolicy());

		string nearest = "no live players";
		if (m_Truck)
		{
			float nearestDistance = OVT_MountedGroupActivation.NearestPlayerDistance(m_Truck.GetOrigin());
			if (nearestDistance >= 0)
			{
				int nearestMetres = Math.Round(nearestDistance);
				nearest = nearestMetres.ToString() + " m";
			}
		}

		int ring = virtualization.GetSpawnDistance(m_iCrewHandle);

		// ⚠ THE WHY-CLAUSE IS ONLY ADDED WHEN THERE IS NOTHING TO SEE, and that is the point: a crew with
		// men in it does not need three sentences about the spawn queue, and a crew with none is useless
		// without them. See OVT_MountedGroupActivation.DescribeSpawnState for how to read it.
		string why = "";
		if (OVT_MountedGroupActivation.MaterialisedCount(crew) == 0)
		{
			why = string.Format(" [%1; %2]",
				OVT_MountedGroupActivation.DescribeSpawnState(crew, ring),
				OVT_MountedGroupActivation.DescribeAiBudget(crew));
		}

		return string.Format("crew handle %1: %2 of %3 alive in the mask, %4%5; lifecycle %6 on a %7 m ring, nearest player %8",
			m_iCrewHandle.ToString(),
			virtualization.GetAliveMemberCount(m_iCrewHandle).ToString(),
			virtualization.GetMemberCount(m_iCrewHandle).ToString(),
			OVT_MountedGroupActivation.DescribeActivation(crew),
			why,
			policy,
			ring.ToString(),
			nearest);
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE HOME IS for a transport that has delivered: the spot it was spawned on.
	//!
	//! THE FALLBACK IS THE SOURCE, and it is a real case rather than defensive padding - a module can be
	//! RETURNING without ever having spawned a truck only if somebody changes the state machine, but
	//! m_vHome is also zero for the whole of every walk-path insertion, and a zero vector is the origin
	//! of the map rather than an obviously-wrong value. The base is where this used to drive to, so the
	//! fallback is precisely the old behaviour: worse, never absurd.
	//! \return The spawn spot when there is one, otherwise the source base.
	protected vector HomePosition()
	{
		if (m_vHome == vector.Zero)
			return m_vSource;

		return m_vHome;
	}

	//------------------------------------------------------------------------------------------------
	//! One observation of an empty truck on its way home.
	//! \param[in] deltaTime Milliseconds since the last update of this module.
	protected void TickReturn(int deltaTime)
	{
		if (!m_Truck || !IsTruckOperational())
		{
			ReleaseConvoy("its transport was destroyed on the way home", false);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
		}

		// ⚠ THE RETURN LEG IS STILL A DRIVE. The force is on the ground and back on the ordinary ring,
		// but the crew is doing exactly what it was doing on the way out and needs exactly the same
		// pin - an empty truck whose driver has gone to sleep at max LOD never gets home, holds its
		// spawn marker out of service and is eventually collected by the abandoned-transport sweep for
		// a reason nobody would guess. This is why the pin follows the RIDE and not the DRIVING state.
		HoldRidersActive();
		NudgeCrewMaterialisation();

		// ⚠ THE RADIUS ALONE, NOT THE SPEED-AWARE ARRIVAL TEST. Nobody gets out here - the truck is empty
		// and is about to be deleted - so there is nothing to throw about, and a transport that rolls
		// through its own spawn at walking pace has got home. Bounded anyway by RETURN_TIMEOUT_TICKS below.
		if (OVT_InsertionGeometry.IsInsideArrivalRadius(vector.Distance(m_Truck.GetOrigin(), HomePosition()), m_fArrivalRadius))
		{
			ReleaseConvoy("its transport is home", true);
			m_eState = OVT_EInsertionState.FINISHED;
			return;
		}

		// ==========================================================================================
		// 🔴 THE CREW CAN GET OUT AND WALK HOME, AND UNTIL 2026-08-21 NOTHING HERE NOTICED FOR TEN
		// MINUTES. IT IS VANILLA DOING IT, ON PURPOSE, AND THE LOD PIN IS WHAT LET IT START.
		// ==========================================================================================
		// SCR_AIVehicleCombatActivity is the group activity that runs when a group with a non-static
		// vehicle perceives a dangerous enough target cluster (:13-49). Its first branch is
		// `if (!vehicle.HasWeapon())` - a transport truck - and it then sends EVERY member of the crew
		// fireteams a dismount message (:164-176), plus every cargo member (:222-232). That is vanilla's
		// "get out of the soft-skin, it is a coffin" reaction and it is correct behaviour; what it leaves
		// behind is a truck standing in the road with its engine running and a crew that still holds the
		// MOVE order this module gave them, which they then execute ON FOOT. The author watched exactly
		// that: *"driver crew started walking home and left the truck there running"*.
		//
		// ⚠ AND IT IS NEW BECAUSE OF OVT_MountedGroupActivation. A crew at max LOD has no behaviour tree,
		// so it PERCEIVES NOTHING and no combat activity can ever evaluate for it - the whole reaction was
		// unreachable for an unobserved convoy before the pin existed. Pinning crews below max LOD is what
		// makes convoys drive at all, and it also switched their perception on. The two arrived together.
		//
		// ⚠ AND THERE IS A SECOND VANILLA DOOR ONTO THE SAME SYMPTOM, which this one test also covers.
		// SCR_AILeaveStaticVehicles dismounts a group from any vehicle that answers false to CanMove()
		// (:104-109), and that is SCR_AIUtils.VehicleCanMove - `GetMovementDamage() < 1` plus a not-on-fire
		// test (:264-293). An IMMOBILISED truck is therefore abandoned by its crew while
		// IsTruckOperational() above still calls it fine, because that one asks IsDestroyed() and a
		// truck with dead wheels is not destroyed. Both doors present identically from here - nobody at
		// the wheel - and both want the same answer, so neither is tested for separately.
		//
		// WHAT IT COST BEFORE THIS TEST. Nothing in TickReturn asked about the crew at all, so an
		// abandoned return leg sat here until RETURN_TIMEOUT_TICKS - about ten real minutes - with two
		// LOD-PINNED, registered, materialised men walking across the map and a truck standing wherever
		// they left it. Per insertion. This is the same budget the outbound leg gives the same question
		// (see m_iUncrewedTicksElapsed), so both legs now write a driverless transport off in about a
		// minute and hand its crew back.
		//
		// ⚠ THE TRUCK IS LEFT STANDING RATHER THAN DELETED, unlike the timeout below, and deliberately:
		// this one fires while a player may well be looking at it (a fight is the usual reason the crew
		// bailed out), so it goes onto the same bounded, player-aware collection countdown that a truck
		// stranded on the way OUT gets. See ArmAbandonedTruck and TickAbandonedTruck.
		if (!CrewIsAtTheWheel())
		{
			m_iUncrewedTicksElapsed = m_iUncrewedTicksElapsed + 1;

			if (OVT_InsertionGeometry.IsUncrewedGraceExpired(m_iUncrewedTicksElapsed, m_iStuckTicks))
			{
				Print(string.Format("[Overthrow] Insertion '%1': nobody is driving its empty transport home after %2 update(s) - %3",
					DescribeSelf(), m_iUncrewedTicksElapsed.ToString(), DescribeCrewLiveness()), LogLevel.NORMAL);

				ReleaseConvoy("nobody is driving its transport home - the crew left it", false);
				m_eState = OVT_EInsertionState.FINISHED;
				return;
			}
		}
		else
		{
			m_iUncrewedTicksElapsed = 0;
		}

		m_iReturnTicksElapsed++;
		if (m_iReturnTicksElapsed < RETURN_TIMEOUT_TICKS)
			return;

		// A truck that cannot find its way home is not worth watching for the rest of the campaign.
		// It is deleted where it stands, subject to the same player veto as any other teardown.
		ReleaseConvoy("its transport did not get home in time", true);
		m_eState = OVT_EInsertionState.FINISHED;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE OBSERVATION OF A TRANSPORT THIS MODULE WALKED AWAY FROM.
	//!
	//! ==========================================================================================
	//! WHY IT IS A COUNTER ON THIS MODULE AND NOT A TIMER. Deleting a world entity N minutes from now is
	//! the classic CallLater, and a CallLater here would be a genuine leak: the module is thrown away
	//! whenever its deployment ends, on every one of the seventeen release paths audited for this file,
	//! and a queued call holding a pointer into it would have to be removed on all of them - one missed
	//! removal is a callback into a dead module. A plain int ticked from OnUpdate() cannot do that:
	//!   - IT SURVIVES NOTHING. No serializer touches it and none should. A save taken while a truck is
	//!     waiting to be collected comes back with no truck at all (vehicles are not persisted here) and
	//!     no countdown, which is the same answer by a shorter route.
	//!   - IT IS CANCELLED BY THE TRUCK GOING AWAY, whatever took it - a Game Master, a teardown, the
	//!     ordinary ReleaseTruck() - because an entity handle nulls itself and this reads m_Truck fresh
	//!     every tick rather than capturing anything.
	//!   - IT CANNOT FIRE AGAINST A STALE HANDLE, because it only runs from a live update of a live
	//!     module, and the module and the counter die together.
	//!   - IT ADDS NO EXIT TO THE RELEASE AUDIT. It calls ReleaseTruck() and nothing else: the convoy
	//!     slot, the crew registration and the owned waypoints were all handed back by the ReleaseConvoy
	//!     that armed this in the first place, and ReleaseTruck() touches none of them.
	//! ==========================================================================================
	//!
	//! ⚠ THE OWNERSHIP VETO IS UNCHANGED AND IS STILL THE LAST WORD. This routes through ReleaseTruck(),
	//! so a transport a player owns or is sitting in is left standing and FORGOTTEN - stealing one still
	//! makes it yours permanently, which is a requirement rather than an accident.
	protected void TickAbandonedTruck()
	{
		if (!m_bTruckAbandoned)
			return;

		if (!m_Truck)
		{
			DisarmAbandonedTruck();
			return;
		}

		m_iAbandonedTicksElapsed = m_iAbandonedTicksElapsed + 1;

		// Asked every tick rather than only at the deadline. It is a handful of distance checks against
		// the connected players once per ten seconds per abandoned truck, and keeping the whole decision
		// in one pure call is worth more than saving them.
		bool playerNearby = OVT_WorldUtils.PlayerInRange(m_Truck.GetOrigin(), ABANDONED_TRUCK_PLAYER_RADIUS_M);

		if (!OVT_InsertionGeometry.IsAbandonedTruckCollectable(m_iAbandonedTicksElapsed, STUCK_TRUCK_TIMEOUT_TICKS, playerNearby))
		{
			LogAbandonedHold(playerNearby);
			return;
		}

		vector where = m_Truck.GetOrigin();

		// Read before the release, which nulls the handle. ReleaseTruck() disarms this countdown on BOTH
		// of its branches - the deletion and the ownership veto - so there is nothing left to clear here
		// and a vetoed truck is never counted towards again. Its own NORMAL line explains the veto, so
		// this one must only claim a collection that happened.
		if (!ReleaseTruck())
			return;

		Print(string.Format("[Overthrow] Insertion '%1': its abandoned transport at %2 was collected after %3 update(s) - nobody was within %4 m of it, and a transport left on this road is the next convoy's obstacle",
			DescribeSelf(), where.ToString(), m_iAbandonedTicksElapsed.ToString(),
			ABANDONED_TRUCK_PLAYER_RADIUS_M.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the collection countdown on a transport that has just been left standing.
	//!
	//! ⚠ THE CREW IS ALREADY GONE BY THE TIME THIS RUNS, AND NOTHING HERE SHOULD TRY TO REMOVE IT AGAIN.
	//! The author's rule for a stuck insertion is "delete the truck AND its crew", and the second half is
	//! satisfied before the first: this is reached only from ReleaseConvoy()'s else branch, and
	//! ReleaseConvoy stands the crew down and unregisters it UNCONDITIONALLY - the deleteTruck flag
	//! selects what happens to the VEHICLE and nothing else. Read the ordering there: ReleaseRidersActive
	//! hands back the LOD pin, StandDownCrew empties the group (which is what steers UnregisterGroup onto
	//! its despawn-and-delete branch instead of retiring the men in place), UnregisterGroup drops the
	//! record, and the owner-key sweep catches any straggler. All of that has happened several lines
	//! above this call. A second release here would be dead code that looked load-bearing.
	//!
	//! What is left to decide is therefore only the transport, and since 2026-08-21 the answer is "as
	//! soon as nobody can see it go" - see STUCK_TRUCK_TIMEOUT_TICKS.
	//! \param[in] reason Why it was left, for the log line.
	protected void ArmAbandonedTruck(string reason)
	{
		if (!m_Truck)
			return;

		if (m_bTruckAbandoned)
			return;

		// ==========================================================================================
		// 🔴 A TRANSPORT THAT NEVER LEFT ITS OWN MOTOR POOL IS NOT A LANDMARK, IT IS AN OBSTRUCTION -
		// AND LEAVING IT THERE IS SELF-REINFORCING.
		//
		// The countdown below is written for a truck stranded OUT IN THE WORLD, where it is worth
		// something as scenery and as loot. A truck that stalled on the spot it spawned on is the
		// opposite: it is sitting on an authored OVT_VehiclePatrolSpawn at a friendly base, and
		// ResolveAuthoredTruckSpawn() skips any marker with a vehicle within MARKER_CLEARANCE_M. So it
		// takes that marker out of service - and when the last free marker goes, ChooseSpawnMarker()
		// answers -1 and the next insertion falls back to the road snap, which the method's own header
		// admits "may land on the access road, inside the wire, across a gate or nose-first into a
		// wall". That truck stalls too, and abandons itself somewhere worse. Each failure makes the
		// next one likelier.
		//
		// The author walked into the middle of exactly that loop (2026-08-20): "I also just found an
		// abandoned truck at the vehicle spawn point of chotain. then a specops harassment team spawned
		// in a new truck nearby but triggered the 'stuck' gate shortly after".
		//
		// ⚠ THE 20-MINUTE TIMEOUT CANNOT SAVE IT, AND NEITHER CAN THE PROXIMITY HOLD - THE HOLD IS THE
		// PROBLEM. Collection also requires no player within ABANDONED_TRUCK_PLAYER_RADIUS_M, which is
		// 320 m: at a base a player visits, that covers the whole compound, so the obstruction is held
		// in place for as long as anyone is around to be inconvenienced by it.
		//
		// ⚠ RELEASE, NOT DELETE, so the ownership veto still has the last word. If a player has claimed
		// it, ReleaseTruck() leaves it standing and forgets it - stealing one still makes it yours
		// permanently - and it nulls the handle either way, which is why nothing is armed afterwards.
		// ==========================================================================================
		if (m_vHome != vector.Zero && vector.Distance(m_Truck.GetOrigin(), m_vHome) <= ABANDONED_AT_SPAWN_RADIUS_M)
		{
			Print(string.Format("[Overthrow] Insertion '%1': its transport never left its spawn point - %2. Collecting it immediately, without even waiting for the coast to clear, because it is standing on a vehicle spawn the next insertion needs",
				DescribeSelf(), reason), LogLevel.NORMAL);

			ReleaseTruck();
			return;
		}

		m_bTruckAbandoned = true;
		m_iAbandonedTicksElapsed = 0;
		m_bAbandonedHoldLogged = false;

		// ⚠ NORMAL, NOT VERBOSE, AND THE LEVEL IS THE WHOLE POINT OF THE LINE. "That truck never
		// despawns" was reported as a defect on 2026-08-21 against a truck that was in fact on a
		// perfectly healthy countdown - and the log said NOTHING at all, because this line was filtered
		// out. A reader has to be able to tell "it is waiting for the coast to clear" apart from
		// "nothing is watching it" without attaching a debugger, and one line per abandoned truck is not
		// noise.
		Print(string.Format("[Overthrow] Insertion '%1': its transport is left standing at %2 - %3. It will be collected on the next update once nobody is within %4 m of it",
			DescribeSelf(), m_Truck.GetOrigin().ToString(), reason,
			ABANDONED_TRUCK_PLAYER_RADIUS_M.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops counting. Safe to call when nothing was ever armed.
	protected void DisarmAbandonedTruck()
	{
		m_bTruckAbandoned = false;
		m_iAbandonedTicksElapsed = 0;
		m_bAbandonedHoldLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE that an overdue transport is being kept because somebody is standing near it.
	//! \param[in] playerNearby Whether that is in fact why it is being kept.
	protected void LogAbandonedHold(bool playerNearby)
	{
		if (m_bAbandonedHoldLogged)
			return;

		if (!playerNearby)
			return;

		if (m_iAbandonedTicksElapsed < STUCK_TRUCK_TIMEOUT_TICKS)
			return;

		m_bAbandonedHoldLogged = true;

		// NORMAL for the same reason ArmAbandonedTruck's line is: this is the answer to "why is that
		// truck still standing there", it is latched to exactly one line per abandoned transport, and a
		// reader who cannot see it concludes the countdown is broken.
		Print(string.Format("[Overthrow] Insertion '%1': its abandoned transport at %2 is overdue for collection but a player is within %3 m, so it stays until nobody is",
			DescribeSelf(), m_Truck.GetOrigin().ToString(), ABANDONED_TRUCK_PLAYER_RADIUS_M.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! THE SUCCESS PATH: the force is down at the landing zone and the truck goes home.
	//!
	//! The force needs no new orders. It has held a plan pointing at the objective since the moment it
	//! was registered - the behaviour module's, or this module's fallback march - which is precisely
	//! why every failure path below can simply open the doors and walk away.
	protected void CompleteInsertion()
	{
		int delivered = m_aHandles.Count();

		DisembarkPassengers();
		DropPassengersToGlobalRing();

		// The slot is about trucks driving TOWARDS an objective. The empty one going home does not hold
		// the next insertion up.
		ReleaseReservation();

		OnInsertionArrived(m_vLZ);

		m_eState = OVT_EInsertionState.RETURNING;
		m_iReturnTicksElapsed = 0;

		// The return leg has its own uncrewed test now (see TickReturn), and it must start from zero
		// rather than from whatever the outbound drive left behind - a convoy that spent five ticks
		// waiting for its crew to board and then delivered perfectly must not arrive on the return leg
		// one tick from being written off.
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;

		IssueCrewMove(HomePosition());

		Print(string.Format("[Overthrow] Insertion '%1' delivered %2 group(s) at %3; its transport is going home",
			DescribeSelf(), delivered.ToString(), m_vLZ.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! THE HOOK A SUBCLASS OVERRIDES to do something at the drop point. Empty here on purpose: this
	//! module delivers a force and nothing else.
	//! \param[in] lzPosition Where the transport actually stopped - NOT the objective.
	protected void OnInsertionArrived(vector lzPosition)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! THE FALLBACK, taken when a drive that had already started cannot continue.
	//!
	//! Everything the force needs it already has. It is registered, it is alive, it holds a plan that
	//! points at the objective, and the only thing between it and that plan is a door. So: open the
	//! door, put the men back on the ordinary proximity ring, hand every borrowed thing back, and let
	//! them walk. Logged at NORMAL rather than WARNING - this is the system working.
	//! \param[in] reason What ended the drive, for the log line.
	protected void DismountAndWalk(string reason)
	{
		DisembarkPassengers();

		// The truck is NOT deleted here, AND IT IS NOT LEFT FOREVER EITHER. A stuck one is a landmark and
		// a lootable and a destroyed one is already a wreck, so neither is taken away on the spot - but
		// "released with everything else when the deployment ends" was only ever true of a deployment that
		// ENDS. The forward base's stands for as long as the base does, so on the one route that reliably
		// strands trucks they simply accumulated (user play-test, 2026-08-19). ReleaseConvoy() arms a
		// bounded collection countdown instead; see TickAbandonedTruck().
		ReleaseConvoy(reason, false);

		EnterWalking(reason);

		// Immediately, rather than waiting for the next convergence: the men are on the ground and
		// there is no reason for them to be an always-materialised squad for another ten seconds.
		DropPassengersToGlobalRing();
	}

	//------------------------------------------------------------------------------------------------
	//! Diverts an insertion that has not yet started driving onto the march.
	//! \param[in] reason Why there is no convoy, for the log line.
	protected void FallBackToWalking(string reason)
	{
		ReleaseConvoy(reason, true);
		EnterWalking(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Settles this insertion into "on foot".
	//! \param[in] reason Why, for the log line.
	protected void EnterWalking(string reason)
	{
		m_eState = OVT_EInsertionState.WALKING;

		Print(string.Format("[Overthrow] Insertion '%1' is on foot: %2", DescribeSelf(), reason), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Teardown - see the class header's owned/borrowed split
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Hands back everything this module borrowed for a convoy. IDEMPOTENT and safe on a module that
	//! never had one.
	//!
	//! ⚠ THIS IS THE ONLY PLACE THE RESERVATION IS RELEASED OUTSIDE CompleteInsertion(), AND EVERY
	//! NON-ARRIVAL EXIT MUST REACH IT. The manager's counter cannot be recovered by anything else - it
	//! is not persisted, nothing sweeps it, and a slot lost to a convoy that ended quietly is a slot
	//! the faction never gets back.
	//! \param[in] reason What ended the convoy, for the log line. Empty logs nothing.
	//! \param[in] deleteTruck Whether the transport should be taken away as well.
	protected void ReleaseConvoy(string reason, bool deleteTruck)
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();

		SCR_AIGroup crew;
		if (virtualization && m_iCrewHandle != -1)
			crew = virtualization.GetGroup(m_iCrewHandle);

		ClearOwnedWaypoints(crew);

		// ⚠ BEFORE THE UNREGISTER AND BEFORE UnsubscribeRiders(), because both of those take away the
		// handles this reads. It is the single release point for the activation pin - see
		// ReleaseRidersActive() for why there is exactly one and why it is wider than the pin.
		ReleaseRidersActive();

		UnsubscribeRiders();

		if (virtualization && m_iCrewHandle != -1)
		{
			// ⚠ TAKE THE MEN OUT AND PUT THEM AWAY BEFORE HANDING THE REGISTRATION BACK. See
			// StandDownCrew() - without this the truck is deleted a few lines below with a live crew
			// inside it, and the registration is retired around men who never despawn.
			StandDownCrew(virtualization, m_iCrewHandle);

			// UnregisterGroup respects held members: a crewman still sitting in the truck retires the
			// group in place rather than having it deleted out from under him. StandDownCrew() has
			// already emptied the group, so the ordinary branch - despawn and delete - is the one taken.
			virtualization.UnregisterGroup(m_iCrewHandle);
		}

		m_iCrewHandle = -1;

		// ⚠ AND THEN SWEEP THE KEY, WHICH IS NEW AND IS THE PRICE OF THE PER-INSERTION KEY.
		//
		// A crew key is now unique to ONE insertion (see GetCrewOwnerKey), which is what stops the next
		// one inheriting these men - and the flip side is that NOTHING WILL EVER LOOK UNDER THIS KEY
		// AGAIN once this module is gone. Under the old shared key a record this module lost track of
		// was at least reclaimable by the next insertion of the same config; now it would be a
		// registered, permanently-materialised two-man group with no owner for the rest of the campaign.
		//
		// The one way to lose track of it is a handle this module has already dropped: OnVirtualGroupWiped
		// sets m_iCrewHandle to -1 as it hands the wipe on, and any future path that clears it early
		// would do the same. Asking the registry rather than trusting the field costs one map lookup on a
		// teardown and closes the whole class.
		if (virtualization && !m_sCrewOwnerKey.IsEmpty())
		{
			array<int> strays = virtualization.FindGroupsByOwner(OWNER_SYSTEM, m_sCrewOwnerKey);
			foreach (int strayHandle : strays)
			{
				if (!virtualization.IsRegistered(strayHandle))
					continue;

				Print(string.Format("[Overthrow] Insertion '%1': crew handle %2 was still registered under this insertion's key at teardown - releasing it",
					DescribeSelf(), strayHandle.ToString()), LogLevel.WARNING);

				StandDownCrew(virtualization, strayHandle);

				virtualization.UnregisterGroup(strayHandle);
			}
		}

		if (deleteTruck)
		{
			ReleaseTruck();
		}
		else
		{
			// ⚠ THE ONE PLACE THE COLLECTION COUNTDOWN IS ARMED, and it is here rather than in
			// DismountAndWalk() so that BOTH paths which leave a transport standing are covered by one
			// line: a stranded truck mid-drive, and one destroyed on its way home. See TickAbandonedTruck()
			// for why a counter and not a timer, and STUCK_TRUCK_TIMEOUT_TICKS for why the delay is now
			// one tick and what that is trying to stop happening on the road.
			ArmAbandonedTruck(reason);
		}

		ReleaseReservation();

		m_bHaveLastTruckPosition = false;
		m_iStuckTicksElapsed = 0;
		m_iUncrewedTicksElapsed = 0;
		m_iUnmaterialisedTicksElapsed = 0;
		m_iInsideRadiusTicks = 0;

		if (!reason.IsEmpty())
			Print(string.Format("[Overthrow] Insertion '%1': convoy stood down - %2", DescribeSelf(), reason), LogLevel.VERBOSE);
	}

	//------------------------------------------------------------------------------------------------
	//! WINDS THE TRANSPORT CREW UP FOR GOOD: out of the truck, off the map, ready to be unregistered.
	//!
	//! ==========================================================================================
	//! 🔴 WHY THIS EXISTS - TWO FAULTS THAT ONLY APPEAR TOGETHER, BOTH SEEN ON 2026-08-21.
	//! ==========================================================================================
	//! FAULT 1: A TRANSPORT IS DELETED UNDER ITS OWN CREW. A deployment is collected the moment its
	//! force has done its job - CollectDeployment() settles the accounts and leaves the delivered force
	//! alive and working, which is correct - but the TRANSPORT may still be halfway home. Collection
	//! reaches OnCleanup -> ReleaseConvoy(deleteTruck: true) -> ReleaseTruck(), and TruckDeletionVeto()
	//! protects a vehicle a PLAYER owns or is riding in and says nothing at all about the men this
	//! module put in it. The truck therefore evaporated around a seated, driving crew (author play-test:
	//! *"the truck did drive away from the LZ I saw it, both crew were seated. But a bit later I saw them
	//! on foot. I can't find their truck anywhere."*).
	//!
	//! ⚠ AND IT CANNOT BE FIXED BY LETTING THE TRUCK FINISH ITS JOURNEY, WHICH WAS THE FIRST INSTINCT.
	//! OVT_DeploymentComponent.DestroyDeployment() removes the update timer, cleans up every module and
	//! deletes the owning entity on one call stack - so a collected deployment's modules are NEVER
	//! TICKED AGAIN. There is nothing left to drive the transport home, nothing left to notice it
	//! arrived, and nothing left to collect it if it does not: the abandoned-transport countdown is
	//! module state and dies with the module. A transport left standing at collection is litter forever.
	//! Deleting it at teardown is therefore right; deleting it WITH PEOPLE IN IT is the bug.
	//!
	//! FAULT 2: THE REGISTRATION IS RETIRED AROUND LIVE MEN, AND THAT ONE IS THIS FEATURE'S OWN DOING.
	//! UnregisterGroup() asks HasHeldMember(), which is true for any AI-ACTIVATED member, and retires
	//! such a group in place instead of deleting it - a sound rule, written when a crew far from any
	//! player was at max LOD and therefore NOT activated, so the branch fired only for a crew somebody
	//! was actually using. OVT_MountedGroupActivation pins crews below max LOD so they will drive, which
	//! makes them permanently AI-activated, which makes the retire-in-place branch fire EVERY TIME. A
	//! retired group keeps its members, keeps its 100 km ring, and its lifecycle tick therefore never
	//! despawns them: two men materialised for the rest of the campaign, per insertion, wandering
	//! wherever their truck was taken from under them. That is precisely the leak the riding ring's own
	//! header warns about, arriving by a door nobody was watching.
	//!
	//! Both are closed by the same three lines, in this order:
	//!   1. HAND BACK THE LOD PIN so nothing this module did keeps the men awake after it is finished
	//!      with them (ReleaseRidersActive has already done it; this is the belt to that braces);
	//!   2. GET THEM OUT OF THE TRUCK, so the vehicle deletion below cannot take them with it and so a
	//!      transport that is being LEFT standing is not left with corpses of a group in its seats;
	//!   3. DESPAWN THE MEMBERS, which empties the group and steers UnregisterGroup onto its ordinary
	//!      despawn-and-delete branch instead of retire-in-place.
	//!
	//! ⚠ THIS IS A RESTORATION, NOT A NEW POLICY. Before the LOD pin, a crew being unregistered at
	//! teardown was almost always unheld and was despawned and deleted by core exactly like this. All
	//! this does is stop the pin changing the answer.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The crew registration to wind up. Passed in rather than read off m_iCrewHandle
	//!            because the teardown sweep in ReleaseConvoy() stands down crews this module has
	//!            already stopped tracking - see the sweep for why one can exist.
	protected void StandDownCrew(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		if (handle == -1)
			return;

		SCR_AIGroup crew = virtualization.GetGroup(handle);
		if (!crew)
			return;

		OVT_MountedGroupActivation.ReleaseGroupActive(crew);

		array<AIAgent> agents = {};
		crew.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			DisembarkAgent(agent);
		}

		if (crew.GetAgentsCount() > 0)
			crew.DespawnMembers();
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the faction's convoy slot back. Idempotent: only a claim that was actually made is
	//! released, and the manager floors its own counter at zero regardless.
	protected void ReleaseReservation()
	{
		if (!m_bReserved)
			return;

		m_bReserved = false;

		if (!m_ParentDeployment)
			return;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return;

		manager.ReleaseInsertion(m_ParentDeployment.GetControllingFaction());
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the truck away - unless somebody has made it theirs.
	//!
	//! The two vetoes are the ones OVT_VehicleSpawningDeploymentModule settled on, for the same
	//! reasons: a player owner uid is stamped the moment a player drives an unowned vehicle, and an
	//! occupant that is a player or a player's recruit covers the passenger seat the claim path never
	//! fires for.
	//!
	//! ⚠ A VETO FORGETS THE TRUCK RATHER THAN DEFERRING IT. m_Truck is nulled on both branches, so a
	//! vehicle a player has made his is never looked at again by this module - not by the teardown, and
	//! not by the abandoned-transport countdown, which is disarmed here for the same reason. Stealing one
	//! makes it yours permanently.
	//! \return True when the vehicle was actually deleted, false when a veto left it standing. Every
	//!         caller but the abandoned-transport sweep ignores it; that one needs it so its log line
	//!         cannot claim to have collected a truck a player had just claimed.
	protected bool ReleaseTruck()
	{
		if (!m_Truck)
			return false;

		string veto = TruckDeletionVeto(m_Truck);
		if (veto != "")
		{
			Print(string.Format("[Overthrow] Insertion '%1': transport left standing - %2", DescribeSelf(), veto), LogLevel.NORMAL);
			m_Truck = null;
			DisarmAbandonedTruck();
			return false;
		}

		// ⚠ THE LAST LINE OF DEFENCE AGAINST DELETING A VEHICLE OVER ITS OCCUPANTS, and it is deliberately
		// here rather than only at the one call site that got it wrong. TruckDeletionVeto() above answers
		// "is this somebody else's property" - a player owner, a player or a player's recruit riding in
		// it - and that is a question about OWNERSHIP, not about safety. It has nothing to say about the
		// men this module itself put in the cab, which is exactly how a transport came to be deleted
		// around its own seated crew mid-return on 2026-08-21. Every AI still aboard is put on the ground
		// first, so the class of fault becomes impossible rather than merely unlikely on the path that
		// was noticed. On the ordinary teardown path StandDownCrew() has already emptied the crew and
		// this finds nobody - it costs one compartment walk on a vehicle that is about to stop existing.
		EvacuateAiOccupants(m_Truck);

		SCR_EntityHelper.DeleteEntityAndChildren(m_Truck);
		m_Truck = null;
		DisarmAbandonedTruck();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts every AI still aboard on the ground, immediately before the vehicle stops existing.
	//!
	//! DELIBERATELY NOT A VETO. Refusing to delete a transport because some stray AI wandered into it
	//! would leave the vehicle standing forever - the module that owns it is about to be destroyed and
	//! nothing else will ever look at it again (see StandDownCrew for why a collected deployment is
	//! never ticked). Getting the man out and proceeding is the answer that leaves neither a stranded
	//! vehicle nor a deleted passenger.
	//!
	//! ⚠ PLAYERS AND PLAYER RECRUITS NEVER REACH HERE. TruckDeletionVeto() has already refused the
	//! deletion outright for both, so this only ever sees faction AI.
	//! \param[in] vehicle The transport about to be deleted.
	protected void EvacuateAiOccupants(notnull Vehicle vehicle)
	{
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!compartments)
			return;

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);

		int evacuated = 0;

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			IEntity occupant = slot.GetOccupant();
			if (!occupant)
				continue;

			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(
				occupant.FindComponent(CompartmentAccessComponent)
			);

			if (!access || !access.IsInCompartment())
				continue;

			access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true);
			evacuated = evacuated + 1;
		}

		if (evacuated > 0)
		{
			Print(string.Format("[Overthrow] Insertion '%1': %2 AI occupant(s) put on the ground before their transport was removed",
				DescribeSelf(), evacuated.ToString()), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ IT IS A TEST OF OWNERSHIP AND NOT OF SAFETY, AND THE DIFFERENCE MATTERED ONCE. It answers "has
	//! somebody else a claim on this vehicle" - a player owner, a player aboard, a player's recruit
	//! aboard - and it deliberately says NOTHING about this module's own crew, because a crew we are
	//! winding up is not a claim. That blind spot let a transport be deleted around its own seated crew
	//! on 2026-08-21; the fix is NOT to add a fourth veto here (which would leave the vehicle standing
	//! forever - see EvacuateAiOccupants) but to empty the vehicle first. Keep the two jobs apart.
	//! \param[in] vehicle The transport to judge.
	//! \return An empty string when it is safe to delete, or the reason it is not.
	protected string TruckDeletionVeto(notnull Vehicle vehicle)
	{
		OVT_PlayerOwnerComponent owner = OVT_PlayerOwnerComponent.Cast(vehicle.FindComponent(OVT_PlayerOwnerComponent));
		if (owner && !owner.GetPlayerOwnerUid().IsEmpty())
			return "a player owns it";

		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!compartments)
			return "";

		PlayerManager players = GetGame().GetPlayerManager();

		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			IEntity occupant = slot.GetOccupant();
			if (!occupant)
				continue;

			if (players && players.GetPlayerIdFromControlledEntity(occupant) > 0)
				return "a player is riding in it";

			OVT_PlayerOwnerComponent occupantOwner = OVT_PlayerOwnerComponent.Cast(
				occupant.FindComponent(OVT_PlayerOwnerComponent)
			);

			if (occupantOwner && !occupantOwner.GetPlayerOwnerUid().IsEmpty())
				return "a player's recruit is riding in it";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The deployment is over.
	//!
	//! ⚠ THE CONVOY IS WOUND UP BEFORE super, AND THE ORDER IS NOT COSMETIC. The base class releases
	//! the PASSENGERS - its own owner key - and knows nothing about the crew's separate registration,
	//! the truck, the waypoints or the reservation. Running it first would clear m_aHandles and leave
	//! this module tearing down against an empty list.
	override protected void OnCleanup()
	{
		ReleaseConvoy("the deployment is over", true);
		m_eState = OVT_EInsertionState.FINISHED;

		super.OnCleanup();
	}

	//------------------------------------------------------------------------------------------------
	//! One registered group has been wiped out.
	//!
	//! ⚠ THE CREW IS NOT PART OF THE FORCE. It is registered under a different owner key, is not in
	//! m_aHandles and must never count towards this module's elimination - a deployment whose truck
	//! crew was shot has not lost its infantry. So the crew's wipe is intercepted here and the base
	//! class never sees it; everything else is passed straight through.
	//! \param[in] handle The wiped group's registry handle.
	override void OnVirtualGroupWiped(int handle)
	{
		if (m_iCrewHandle != -1 && handle == m_iCrewHandle)
		{
			m_iCrewHandle = -1;

			if (m_eState == OVT_EInsertionState.DRIVING)
			{
				DismountAndWalk("its transport crew was killed");
				return;
			}

			if (m_eState == OVT_EInsertionState.RETURNING)
			{
				ReleaseConvoy("its transport crew was killed on the way home", true);
				m_eState = OVT_EInsertionState.FINISHED;
			}

			return;
		}

		super.OnVirtualGroupWiped(handle);
	}

	//------------------------------------------------------------------------------------------------
	// The registration seams
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! WHERE THE FORCE IS REGISTERED: at its source, always, whatever the config says about nearest
	//! bases.
	//!
	//! ⚠ RETURNING FALSE ABORTS THE REGISTRATION, AND THAT IS THE POINT. This is the one guarantee the
	//! module exists to make: a force with nowhere to have come from does not appear at its objective.
	//! The base class's own fromNearestBase flag is deliberately ignored - the provider is a strictly
	//! better answer to the same question (see OVT_NearestControlledBaseSourceProvider's header for why
	//! the base class's version goes blind on a contested map).
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[in] fromNearestBase Ignored; the provider is the authority here.
	//! \param[out] anchor The resolved origin.
	//! \return False when there is nowhere for the force to come from.
	override protected bool ResolveSpawnAnchor(int factionIndex, bool fromNearestBase, out vector anchor)
	{
		anchor = vector.Zero;

		if (!EnsureSourceResolved())
			return false;

		anchor = m_vSource;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE ONE GROUP GOES: beside the truck while there is one to board, and on the base class's ring
	//! roll otherwise.
	//! \param[in] anchor The batch anchor - the resolved source.
	//! \param[in] index Position within this batch.
	//! \return The world position to register at.
	override protected vector ResolveSpawnPosition(vector anchor, int index)
	{
		if (IsForceMounted())
			return m_Truck.GetOrigin() + Vector(PASSENGER_SPAWN_OFFSET_M, 0, 0);

		return super.ResolveSpawnPosition(anchor, index);
	}

	//------------------------------------------------------------------------------------------------
	//! IS THERE A VEHICLE UNDER THIS FORCE RIGHT NOW - the one question both registration seams and the
	//! ring sweep should be asking.
	//!
	//! ⚠ IT IS ABOUT THE RIDE AND NOT ABOUT THE ENUM, DELIBERATELY. "m_eState == DRIVING" was a proxy
	//! for "mounted" that happens to be right for the FORCE and wrong for anything else that rides: the
	//! crew is mounted in RETURNING too, and a DRIVING module whose transport has just been destroyed is
	//! not mounted at all until the next tick notices. Asking about the truck makes those cases answer
	//! correctly without anyone having to remember to add a state to a list.
	//!
	//! The FORCE specifically is only ever aboard on the way out - CompleteInsertion and DismountAndWalk
	//! both open the doors and put it back on the ordinary ring before the state moves on - so the
	//! DRIVING term stays. What it no longer does is stand in for "there is a vehicle".
	//! \return True while the force is riding a transport that exists.
	protected bool IsForceMounted()
	{
		return m_eState == OVT_EInsertionState.DRIVING && m_Truck;
	}

	//------------------------------------------------------------------------------------------------
	//! THE RING: always-materialised while riding, ordinary otherwise. See RIDING_SPAWN_DISTANCE for
	//! why a dormant passenger cannot be seated - and for why a ring alone was never the whole answer.
	//! \return The spawnDistanceOverride to register with.
	override protected int ResolveRegistrationSpawnDistance()
	{
		if (IsForceMounted())
			return RIDING_SPAWN_DISTANCE;

		return super.ResolveRegistrationSpawnDistance();
	}

	//------------------------------------------------------------------------------------------------
	//! THE PLAN: whatever the behaviour modules want, and failing that, a march onto the objective.
	//!
	//! ⚠ THE FALLBACK IS WHAT MAKES THE WALK FALLBACK REAL RATHER THAN NOMINAL. A group registered with
	//! no plan is a garrison, and a garrison registered at the SOURCE is a force that stands at the
	//! base it set out from for the rest of the campaign - which is exactly the "men left behind"
	//! outcome every other line in this file is written to prevent. A deployment with an opinionated
	//! behaviour module never reaches this; one without it still arrives.
	//!
	//! It CYCLES, with a long pause, for the same reason the parked-recruit hold loop does: the group
	//! walks to the objective, stands there quietly, and walks back to it if a fight displaced it.
	//! \param[in] groupPosition Where the group is about to be registered.
	//! \return The plan. Never null once there is a deployment to march towards.
	override protected OVT_VirtualWaypointPlan ResolveVirtualPlan(vector groupPosition)
	{
		OVT_VirtualWaypointPlan plan = super.ResolveVirtualPlan(groupPosition);
		if (plan)
			return plan;

		if (!m_ParentDeployment)
			return null;

		array<vector> stops = {};
		stops.Insert(m_ParentDeployment.GetPosition());

		return OVT_VirtualPlanFactory.BuildRoutePlan(stops, MARCH_HOLD_SECONDS, false, groupPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! A newly registered group is a passenger. Subscribe it and seat it if the truck is waiting.
	//! \param[in] handle The new group's registry handle.
	//! \param[in] position Where it was registered.
	override protected void OnGroupRegistered(int handle, vector position)
	{
		super.OnGroupRegistered(handle, position);

		AdoptPassenger(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! A group re-found in the registry is a passenger too.
	//!
	//! ⚠ A RECLAIMED GROUP IS A NEW ENTITY AFTER A LOAD - core re-creates it from its own payload with
	//! a fresh EntityID - so anything subscribed on the old one is gone and the pairing has to be made
	//! again here, which is the same reason the base class re-tags for the Game Master at this point.
	//! \param[in] handle The reclaimed group's registry handle.
	override protected void OnGroupReclaimed(int handle)
	{
		super.OnGroupReclaimed(handle);

		AdoptPassenger(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! Binds one passenger group to the ride, if there is one.
	//! \param[in] handle The group's registry handle.
	protected void AdoptPassenger(int handle)
	{
		if (m_eState != OVT_EInsertionState.DRIVING)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return;

		PairRider(group, false);

		if (m_Truck)
			SeatExistingRiders(group, false);
	}

	//------------------------------------------------------------------------------------------------
	// Seating
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Records what a group is riding as, and subscribes its per-member arrival callback.
	//!
	//! ⚠ PER MEMBER, NOT PER GROUP. A group's whole-group init event fires only when the group fills
	//! COMPLETELY, which under AI budget pressure may never happen, and a core-registered group fills
	//! progressively through the engine's spawn queue - so a whole-group hook would frequently seat
	//! nobody at all.
	//!
	//! ⚠ REMOVE THEN INSERT. ScriptInvoker does not de-duplicate and a convergence runs many times per
	//! session; a plain Insert would try to seat every arriving man once per pass that had ever run.
	//! \param[in] group The rider.
	//! \param[in] isCrew True for the transport crew, false for a passenger.
	protected void PairRider(notnull SCR_AIGroup group, bool isCrew)
	{
		m_mRiderIsCrew.Set(group.GetID(), isCrew);

		group.GetOnAgentAdded().Remove(OnRiderAgentAdded);
		group.GetOnAgentAdded().Insert(OnRiderAgentAdded);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops listening for every rider's members, so no invoker keeps a pointer into a module the
	//! deployment is about to throw away.
	protected void UnsubscribeRiders()
	{
		BaseWorld world = GetGame().GetWorld();

		for (int i = 0; i < m_mRiderIsCrew.Count(); i++)
		{
			if (!world)
				break;

			SCR_AIGroup group = SCR_AIGroup.Cast(world.FindEntityByID(m_mRiderIsCrew.GetKey(i)));
			if (group)
				group.GetOnAgentAdded().Remove(OnRiderAgentAdded);
		}

		m_mRiderIsCrew.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! One rider has arrived in the world. Put him in the truck.
	//!
	//! ⚠ ONE PARAMETER. SCR_AIGroup's own doc comment claims the invoker passes (group, agent); the
	//! actual invocation passes the AGENT alone, and the group is recovered from it.
	//! \param[in] agent The arriving member.
	protected void OnRiderAgentAdded(AIAgent agent)
	{
		if (!agent)
			return;

		// ⚠ RETURNING COUNTS NOW, AND IT DID NOT USED TO. A crewman who materialises on the way home is
		// as much a driver as one who materialises on the way out, and refusing him here left the empty
		// truck's replacement crew unpinned and unseated.
		if (m_eState != OVT_EInsertionState.DRIVING && m_eState != OVT_EInsertionState.RETURNING)
			return;

		AIGroup parent = agent.GetParentGroup();
		if (!parent)
			return;

		EntityID groupId = parent.GetID();
		if (!m_mRiderIsCrew.Contains(groupId))
			return;

		bool isCrew = m_mRiderIsCrew.Get(groupId);

		// THE PIN FIRST AND UNCONDITIONALLY FOR A CREWMAN, before any test that can bail out. It is what
		// makes him drive; losing it to a transport that has just gone would leave a live crew asleep
		// while ReleaseConvoy is still a tick away. Releasing it again there is free.
		if (isCrew)
			OVT_MountedGroupActivation.HoldAgentActive(agent);

		if (!m_Truck)
		{
			m_mRiderIsCrew.Remove(groupId);
			return;
		}

		// Passengers are only ever seated on the way OUT. The way home is the empty truck's leg, and
		// putting a member of a force that has already been delivered back into it would carry him away
		// from the objective he was dropped for.
		if (!isCrew && m_eState != OVT_EInsertionState.DRIVING)
			return;

		SeatRider(m_Truck, agent, isCrew);
	}

	//------------------------------------------------------------------------------------------------
	//! BOARDING: seats everybody who is already on their feet - the crew, then the force.
	//!
	//! ==========================================================================================
	//! 🔴 THIS IS A BOARDING SWEEP AND NOT A TICK SWEEP, AND THE DIFFERENCE IS A JAMMED CONVOY.
	//! ==========================================================================================
	//! Until 2026-08-21 this ran on EVERY drive update under the comment "anybody who materialised, fell
	//! out or was pulled out mid-drive gets back aboard". That is a sweep with no way of telling a man
	//! who FELL out from a man who GOT out on purpose - and vanilla AI gets a man out on purpose all the
	//! time. SCR_AISelectDoorOperatorAgent sends a crewman to open a gate the convoy has to drive
	//! through, and this sweep teleported him back into the cab before he reached it, every ten seconds,
	//! forever. The gate never opened, the truck never moved, and the crew was materialised and fully
	//! AI-active throughout - so it read as "genuinely stuck" in the log and in the liveness diagnostic
	//! (user play-test, 2026-08-21: "the co-driver gets out to open it, but we detect that and force
	//! them back in").
	//!
	//! ⚠ THE GATE CASE IS NEW BECAUSE THE LOD FIX MADE IT REAL. SCR_AISelectDoorOperatorAgent has a
	//! TELEKINESIS branch it takes when the chosen man is at max LOD - a distant convoy used to open
	//! gates with nobody getting out at all. Now that crews are pinned below max LOD so they will drive
	//! (see OVT_MountedGroupActivation), they walk to gates like anyone else, and this sweep started
	//! fighting them. The two changes belong to one another.
	//!
	//! ⚠ SO WHERE DOES SEATING HAPPEN NOW: on ARRIVAL IN THE WORLD, never on a clock.
	//!   - OnRiderAgentAdded() seats each man as the AI spawn queue produces him - that IS "when the
	//!     team spawns", and it is per-member rather than per-group for the reason PairRider records;
	//!   - AdoptPassenger() seats a whole group at the moment it is committed to the ride;
	//!   - this method seats whoever is already standing when the convergence commits the convoy.
	//! All three are events. None of them can fire on a man in the middle of a task, because a man in
	//! the middle of a task is not being spawned or adopted.
	//!
	//! ⚠ AND THE HALF THAT IS KNOWINGLY GIVEN UP, because it must not be re-added by accident: a man
	//! who is thrown, dragged or otherwise DISPLACED from a moving truck is no longer put back. He is
	//! left standing on the road holding the plan he was registered with, and he walks to the objective
	//! from wherever he landed. That is the author's explicit trade (2026-08-21) and it is the right way
	//! round: a squad member left behind is visible, recoverable and arrives late, whereas a convoy
	//! permanently jammed at a gate is invisible and arrives never. Anyone tempted to restore the sweep
	//! must first find a test that separates "displaced and idle" from "out of the truck doing a job" -
	//! and note that proximity does not do it, because the gate is a few metres from the truck.
	protected void BoardEveryone()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		if (m_iCrewHandle != -1)
		{
			SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
			if (crew)
				SeatExistingRiders(crew, true);
		}

		foreach (int handle : m_aHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (group)
				SeatExistingRiders(group, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! THE ONE THING STILL POLICED ON A MOVING TRUCK: a member of the FORCE sitting in the driver's seat.
	//!
	//! ==========================================================================================
	//! ⚠ WHY THIS SURVIVED THE SWEEP'S REMOVAL WHEN NOTHING ELSE DID.
	//! ==========================================================================================
	//! The hijack it prevents is documented at length on SeatRider(): the force rides holding the
	//! behaviour module's plan, which points at the OBJECTIVE, so a squad leader who finds the pilot
	//! seat empty takes it and drives the convoy past its landing zone (author play-test, 2026-08-20).
	//! Removing the tick sweep outright would have handed that bug straight back - and worse than
	//! before, because the pilot seat is now reliably empty at exactly one moment: while the driver is
	//! out opening a gate. Gate opens, hijacker drives off, and the crew is left standing at the gate.
	//!
	//! ⚠ IT CANNOT FIGHT A MAN PERFORMING A TASK, AND THAT IS THE WHOLE DESIGN CONSTRAINT IT WAS BUILT
	//! TO SATISFY. Two properties, both structural rather than heuristic:
	//!   1. IT ONLY EVER LOOKS AT MEN WHO ARE INSIDE A COMPARTMENT. Anybody on foot - which is what
	//!      every gate operator, every displaced man and every casualty is - is invisible to it. It has
	//!      no code path that puts a man INTO a truck he is not already in.
	//!   2. IT ONLY EVER LOOKS AT THE FORCE, NEVER AT THE CREW. m_aHandles is the passengers; the crew
	//!      is under its own owner key and is not iterated here at all. A crewman in the driver's seat
	//!      is a crewman doing his job.
	//! A passenger sitting in the pilot compartment is never "out of the truck doing a job" - he is in
	//! the truck doing the wrong one - so there is no task to interrupt.
	//!
	//! Delegates to EvictPassengerFromPilotSeat(), which re-checks the compartment type and that the
	//! seat belongs to THIS transport, moves him by exactly the ordinary passenger rule, and refuses to
	//! put a man on a moving road when there is no free seat to move him to.
	protected void EvictHijackers()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);

			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;

				IEntity character = agent.GetControlledEntity();
				if (!character)
					continue;

				CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
				if (!access)
					continue;

				// ⚠ THE GUARD THAT MAKES THIS SAFE. A man on foot is somebody else's business - the
				// spawn hook's, or nobody's.
				if (!access.IsInCompartment())
					continue;

				EvictPassengerFromPilotSeat(m_Truck, agent, access);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Seats every member a group already has.
	//! \param[in] group The rider.
	//! \param[in] isCrew True for the transport crew.
	protected void SeatExistingRiders(notnull SCR_AIGroup group, bool isCrew)
	{
		if (!m_Truck)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			SeatRider(m_Truck, agent, isCrew);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one man in a seat.
	//!
	//! CREW TAKE THE DRIVER'S SEAT, THEN THE GUN, THEN THE CO-DRIVER'S, THEN ANY CARGO, applied one man
	//! at a time so the first arrival finds the pilot compartment free and takes it. On every transport
	//! shipped today there is no gun, so in practice that reads driver then co-driver.
	//! PASSENGERS TAKE CARGO AND NEVER THE CO-DRIVER'S SEAT: a passenger in the driver's seat is a
	//! passenger who drives the truck somewhere nobody asked for, and a passenger in a turret is a
	//! passenger who starts a firefight the convoy exists to avoid.
	//!
	//! ==========================================================================================
	//! ⚠ WHY THE CO-DRIVER'S SEAT IS RESERVED, AND WHY IT DID NOT USED TO NEED TO BE. Until
	//! 2026-08-19 crew fell through to a plain "any free cargo seat" and it USUALLY worked, because
	//! vanilla's own picker returns the first free cargo compartment and the front seats are authored
	//! first. What broke it is that this module seats men AS THEY MATERIALISE, one at a time, through
	//! the AI spawn queue - so the order they arrive in is the order the engine happens to produce them,
	//! not crew-then-force. Two passengers materialising ahead of the second crewman took both cab
	//! seats and put him in the back of the truck, which is what the author saw EVERY time.
	//!
	//! It matters beyond tidiness: the co-driver is the man who gets out to open gates, so a crew
	//! sitting in the cargo bed is a convoy that stops at the first closed gate on its route. The seat
	//! is therefore claimed explicitly rather than competed for, which also makes the outcome
	//! independent of materialisation order - the race is not won, it is removed.
	//! ==========================================================================================
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man.
	//! \param[in] isCrew True for the transport crew.
	//! \return True when he was seated.
	protected bool SeatRider(notnull Vehicle vehicle, AIAgent agent, bool isCrew)
	{
		if (!agent)
			return false;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return false;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return false;

		// Already aboard something - this truck, or one he was ordered into. Leave him there.
		//
		// ⚠ AND NOTE WHAT IS NOT HERE: no "not aboard anything, so put him in" clause on a timer. Since
		// 2026-08-21 this method is only ever reached with a man who has just materialised or a group
		// that has just been adopted, so "he is on foot" always means "he has not boarded yet" and never
		// "he got out to open a gate". The periodic caller that made those two indistinguishable is
		// gone - see BoardEveryone().
		//
		// ⚠ WITH ONE EXCEPTION, AND IT IS A REAL ONE: A PASSENGER WHO HAS TAKEN THE WHEEL. Vanilla AI
		// will board and DRIVE a vehicle to satisfy a move order, and the force is riding with exactly
		// such an order live - the behaviour module's plan, pointing at the objective, which this module
		// deliberately never clears (it is what lets every failure path just open the doors and walk
		// away). So a squad leader who materialises beside an empty driver's seat can take it and then
		// drive the convoy to the OBJECTIVE instead of to the landing zone, which is precisely what the
		// author saw on 2026-08-20. Leaving him there because "he is already in a compartment" is how a
		// one-line early return became a hijacked convoy.
		if (access.IsInCompartment())
		{
			if (isCrew)
				return false;

			return EvictPassengerFromPilotSeat(vehicle, agent, access);
		}

		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		if (!compartmentManager)
			return false;

		array<BaseCompartmentSlot> cargo = {};
		int cabCount = CollectCargoSlots(vehicle, cargo);

		if (isCrew)
		{
			if (FillCompartment(compartmentManager, agent, ECompartmentType.PILOT))
				return true;

			// ⚠ THE GUNNER BEFORE THE CO-DRIVER, AND THIS ORDER WAS DELIBERATELY REVERSED ON 2026-08-20.
			// It briefly ran the other way on the argument that "a gunner is a fighting role and this
			// convoy exists to avoid fights". The author's instruction overrides it, and the reason is
			// forward-looking rather than about today's trucks: "insertion teams in future deployment
			// configs may have a weapon... so yes we should force driver + gunner (fallback to co-driver
			// when no gunner position)". A transport that HAS a weapon is one that was chosen for a route
			// where it might be needed, and leaving its gun unmanned to keep a man free for gates is the
			// wrong trade on that vehicle.
			//
			// ⚠ NOTE FOR ANYONE MATCHING THIS AGAINST THE PREFABS: in Enfusion a gunner is a TURRET
			// compartment, NOT a front-cabin seat. The Ural's cab is a pilot slot plus two CARGO slots
			// (`Passenger_r01` at pivot `codriver_01_idle` and `Passenger_m01` at `codriver_02_idle`), so
			// an unarmed truck has no turret at all and this line simply falls through - which is exactly
			// the authored "fallback to co-driver when no gunner position".
			if (FillCompartment(compartmentManager, agent, ECompartmentType.TURRET))
				return true;

			// The co-driver's seat: the fallback on every transport that carries no weapon, which is
			// every one shipped today. He is still the man who gets out to open gates.
			if (cabCount > 0 && FillSlot(vehicle, agent, cargo[0]))
				return true;

			return FillCompartment(compartmentManager, agent, ECompartmentType.CARGO);
		}

		return SeatPassengerInCargo(vehicle, agent, compartmentManager, cargo, cabCount);
	}

	//------------------------------------------------------------------------------------------------
	//! WHERE A MEMBER OF THE FORCE GOES: the back of the truck, then the leftover cab, then anywhere.
	//!
	//! Split out of SeatRider so the eviction path below seats a hijacker by exactly the same rule as an
	//! ordinary passenger, rather than growing a second, subtly different preference order.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man.
	//! \param[in] compartmentManager The transport's own compartment manager, for the last resort.
	//! \param[in] cargo Every cargo slot, cab first, from CollectCargoSlots().
	//! \param[in] cabCount How many leading entries of `cargo` are cab seats.
	//! \return True when he was seated.
	protected bool SeatPassengerInCargo(notnull Vehicle vehicle, AIAgent agent, SCR_BaseCompartmentManagerComponent compartmentManager, notnull array<BaseCompartmentSlot> cargo, int cabCount)
	{
		// THE BACK OF THE TRUCK FIRST: everything past the cab, in authored order. On a transport with a
		// bed this is where the whole force ends up, which is both what it should look like and what
		// keeps the cab free for the men whose job needs them able to get out of it.
		for (int i = cabCount; i < cargo.Count(); i++)
		{
			if (FillSlot(vehicle, agent, cargo[i]))
				return true;
		}

		// Then the cab, MINUS the co-driver's seat - the leftover middle seat on a Ural, or every seat on
		// a vehicle that has no bed at all, which is the ordinary case for a car and not a fallback.
		for (int i = 1; i < cabCount; i++)
		{
			if (FillSlot(vehicle, agent, cargo[i]))
				return true;
		}

		// ⚠ THE RESERVATION IS A PREFERENCE, NOT A RULE, AND THIS LAST RESORT IS THE POINT OF IT. A force
		// that exactly fills every other seat would otherwise leave a man standing in the open at the
		// source base for the sake of one seat nobody is going to use - by the time any passenger reaches
		// this line the crew that wanted it has already been seated, or there is no crew at all.
		if (!compartmentManager)
			return false;

		return FillCompartment(compartmentManager, agent, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	//! GETS A MEMBER OF THE FORCE OUT OF THE DRIVER'S SEAT OF OUR OWN TRUCK.
	//!
	//! ⚠ IT IS SCOPED THREE WAYS AND EVERY ONE OF THEM MATTERS. Only a PASSENGER (the caller checks),
	//! only the PILOT compartment, and only OUR truck. A man in a turret is not steering anything; a man
	//! driving some other vehicle entirely is somebody else's business and moving him would be this
	//! module reaching outside its own convoy. The narrow scope is what makes a per-tick sweep safe.
	//!
	//! ⚠ IT MOVES HIM RATHER THAN THROWING HIM OUT, and that is not a nicety. The truck is at road speed
	//! by the time anybody notices, and DisembarkAgent() teleports - a man put on the ground here arrives
	//! carrying the truck's velocity, which is the exact injury the arrival speed gate was added to stop.
	//! FillSlot() re-seats him inside the same vehicle instead, so he never leaves it.
	//!
	//! ⚠ IF THERE IS NOWHERE TO PUT HIM HE STAYS AT THE WHEEL, deliberately. A full truck with a
	//! passenger driving is a bad state; a passenger dumped on a moving road is a worse one, and a truck
	//! with nobody driving at all is worse still. The log line is the answer to that case - it is rare
	//! enough to be worth reading and there is no safe automatic recovery.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man who has taken the wheel.
	//! \param[in] access His compartment access component.
	//! \return True when he was moved.
	protected bool EvictPassengerFromPilotSeat(notnull Vehicle vehicle, AIAgent agent, notnull CompartmentAccessComponent access)
	{
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return false;

		if (slot.GetType() != ECompartmentType.PILOT)
			return false;

		// Somebody else's vehicle. Not ours to police.
		if (slot.GetVehicle() != vehicle)
			return false;

		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(
			vehicle.FindComponent(SCR_BaseCompartmentManagerComponent)
		);

		array<BaseCompartmentSlot> cargo = {};
		int cabCount = CollectCargoSlots(vehicle, cargo);

		if (SeatPassengerInCargo(vehicle, agent, compartmentManager, cargo, cabCount))
		{
			Print(string.Format("[Overthrow] Insertion '%1': a member of the force had taken the driver's seat - moved to a passenger seat before it could drive the convoy to the objective instead of the landing zone",
				DescribeSelf()), LogLevel.NORMAL);

			return true;
		}

		Print(string.Format("[Overthrow] Insertion '%1': a member of the force is driving and there is NO free passenger seat to move him to - leaving him at the wheel, because putting a man on a moving road is worse. The convoy may drive to the objective rather than the landing zone",
			DescribeSelf()), LogLevel.WARNING);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY CARGO SEAT ON A TRANSPORT, IN AUTHORED ORDER, cab first - so index 0 is the co-driver.
	//!
	//! ⚠ THE CHILD WALK IS NOT OPTIONAL AND IS NOT A REFINEMENT. BaseCompartmentManagerComponent's
	//! GetCompartments() is NOT recursive - vanilla's own SCR_CompartmentAccessComponent carries a
	//! "ToDo: Remove once GetCompartments is recursive" beside exactly this loop - and on a covered truck
	//! the bed benches live in a child entity. Reading only the vehicle's own compartments would find the
	//! two cab seats and nothing else, so the force would be turned away from a truck with eight free
	//! seats in the back.
	//!
	//! ORDER IS THE WHOLE CONTRACT HERE, AND SO IS THE SPLIT POINT THIS RETURNS. The vehicle's own
	//! compartments come first, children after, which is the order vanilla's picker scans in and the
	//! order the prefabs are authored in. On a covered truck that boundary is exactly the cab/bed line:
	//! the Ural4320 authors its two cab seats on the vehicle (`Passenger_r01` at pivot `codriver_01_idle`
	//! and `Passenger_m01` at `codriver_02_idle`) and hangs the bed benches off its `Cargo` child slot.
	//! So the returned count is "how many of these are cab seats", index 0 is the co-driver's, and
	//! everything from the count onwards is the back of the truck - all without reading a seat name or a
	//! pivot transform. A name match would be per-vehicle authoring this module has no business knowing.
	//!
	//! ⚠ ON A VEHICLE WITH NO CHILD SEATING - a UAZ, a car - the count equals the total and there is no
	//! "back". That is not a degenerate case to guard, it is the correct answer: the callers' bed-first
	//! preference simply finds nothing and falls through to the cab, which is where those seats are.
	//! \param[in] vehicle The transport.
	//! \param[out] cargo Filled with its cargo compartments; cleared first.
	//! \return How many leading entries belong to the vehicle itself, i.e. the cab.
	protected int CollectCargoSlots(notnull Vehicle vehicle, notnull array<BaseCompartmentSlot> cargo)
	{
		cargo.Clear();

		AppendOwnCargoSlots(vehicle, cargo);

		int cabCount = cargo.Count();

		IEntity child = vehicle.GetChildren();
		while (child)
		{
			AppendCargoSlots(child, cargo);
			child = child.GetSibling();
		}

		return cabCount;
	}

	//------------------------------------------------------------------------------------------------
	//! One entity's own cargo compartments, then its children's - the same shape, and the same order, as
	//! vanilla's FindFreeAndAccessibleCompartment, which recurses by calling itself on each child.
	//! \param[in] entity The entity whose compartment manager to read.
	//! \param[out] cargo Appended to.
	protected void AppendCargoSlots(notnull IEntity entity, notnull array<BaseCompartmentSlot> cargo)
	{
		AppendOwnCargoSlots(entity, cargo);

		IEntity child = entity.GetChildren();
		while (child)
		{
			AppendCargoSlots(child, cargo);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The cargo compartments of ONE entity, ignoring its children.
	//! \param[in] entity The entity whose compartment manager to read.
	//! \param[out] cargo Appended to.
	protected void AppendOwnCargoSlots(notnull IEntity entity, notnull array<BaseCompartmentSlot> cargo)
	{
		BaseCompartmentManagerComponent manager = BaseCompartmentManagerComponent.Cast(
			entity.FindComponent(BaseCompartmentManagerComponent)
		);

		if (!manager)
			return;

		array<BaseCompartmentSlot> slots = {};
		manager.GetCompartments(slots);

		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;

			if (slot.GetType() != ECompartmentType.CARGO)
				continue;

			cargo.Insert(slot);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts one man in ONE NAMED SEAT, or refuses.
	//!
	//! No occupancy or accessibility test of its own: MoveInVehicle() re-checks the slot is free, is
	//! accessible, is not get-in-locked for this character and is of the type asked for, and answers
	//! false when any of that fails. Duplicating those checks here would be a second copy to drift.
	//!
	//! ⚠ THE VEHICLE PASSED IS THE TRANSPORT, EVEN WHEN THE SLOT BELONGS TO A CHILD. That is safe by
	//! construction rather than by luck: with a custom slot, MoveInVehicle never looks the compartment up
	//! from the vehicle - it validates the slot it was handed and then addresses the RPC to
	//! slot.GetOwner(). The argument only has to be non-null.
	//! \param[in] vehicle The transport.
	//! \param[in] agent The man.
	//! \param[in] slot The seat.
	//! \return True when he got it.
	protected bool FillSlot(notnull Vehicle vehicle, AIAgent agent, BaseCompartmentSlot slot)
	{
		if (!agent || !slot)
			return false;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		return access.MoveInVehicle(vehicle, ECompartmentType.CARGO, false, slot);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] compartmentManager The transport's compartments.
	//! \param[in] agent The man.
	//! \param[in] type Which kind of seat to try.
	//! \return True when he got one.
	protected bool FillCompartment(SCR_BaseCompartmentManagerComponent compartmentManager, AIAgent agent, ECompartmentType type)
	{
		if (!agent || !agent.GetControlledEntity())
			return false;

		IEntity character = agent.GetControlledEntity();

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;

		IEntity vehicle = compartmentManager.GetOwner();
		if (!vehicle)
			return false;

		return access.MoveInVehicle(vehicle, type);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the doors: every member of the FORCE gets out of the truck, wherever the truck is.
	//!
	//! THE CREW IS DELIBERATELY LEFT ABOARD. It still has a truck to drive home, and on the paths where
	//! it does not - a wreck, a wipe - it is unregistered moments later and core retires it in place.
	//! Teleporting men out of a vehicle that is about to be released would only scatter them.
	protected void DisembarkPassengers()
	{
		if (!m_Truck)
			return;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			SCR_AIGroup group = virtualization.GetGroup(handle);
			if (!group)
				continue;

			array<AIAgent> agents = {};
			group.GetAgents(agents);

			foreach (AIAgent agent : agents)
			{
				DisembarkAgent(agent);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gets one man out.
	//!
	//! TELEPORT RATHER THAN THE ANIMATED EXIT, which is the call vanilla's own AI makes when it needs a
	//! man out of a vehicle now: an animated dismount is interruptible, takes seconds per man and can
	//! leave a whole squad half-out when the vehicle is destroyed under them.
	//! \param[in] agent The man to put on the ground.
	protected void DisembarkAgent(AIAgent agent)
	{
		if (!agent)
			return;

		IEntity character = agent.GetControlledEntity();
		if (!character)
			return;

		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
		if (!access)
			return;

		if (!access.IsInCompartment())
			return;

		access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.LEAVE_OPEN, true);
	}

	//------------------------------------------------------------------------------------------------
	// The riding ring
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Puts the whole force back on the ordinary proximity ring.
	protected void DropPassengersToGlobalRing()
	{
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return;

		foreach (int handle : m_aHandles)
		{
			RestoreGlobalSpawnRing(virtualization, handle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Puts ONE group back on the ordinary proximity ring, in the record and in the engine.
	//!
	//! ================== THE ONE PLACE THIS FILE WRITES TO A CORE RECORD ======================
	//!
	//! ⚠ AND IT HAS TO. A group is registered with its ring, the ring is stamped onto the engine
	//! lifecycle policy at registration, and the registry has no setter for it - the virtualization
	//! core's API is frozen for this feature and its own re-stamp is protected. Re-registering instead
	//! would throw away the survivor mask, which is the whole point of the registry.
	//!
	//! ⚠ AND BOTH HALVES ARE NEEDED. The engine policy is what decides whether these men despawn when
	//! the last player leaves; the RECORD is what a save writes and what a load re-stamps. Fixing only
	//! the policy delivers a force that behaves correctly this session and comes back permanently
	//! materialised, forever, on the next load - one squad of the AI budget spent on men nobody can
	//! see, per insertion, for the rest of the campaign.
	//!
	//! Everything about the values is taken from core rather than assumed: the ring comes back out of
	//! GetSpawnDistance() (which resolves the record we just corrected against the server's configured
	//! global) and the anti-thrash band from core's own arithmetic. Only the hysteresis multiplier is a
	//! local constant - see DESPAWN_HYSTERESIS.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle The group to put back.
	protected void RestoreGlobalSpawnRing(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		OVT_VirtualGroupRecord record = virtualization.GetRecord(handle);
		if (record)
		{
			if (record.m_iSpawnDistanceOverride == SPAWN_DISTANCE_GLOBAL)
				return;

			record.m_iSpawnDistanceOverride = SPAWN_DISTANCE_GLOBAL;
		}

		SCR_AIGroup group = virtualization.GetGroup(handle);
		if (!group)
			return;

		int spawnDistance = virtualization.GetSpawnDistance(handle);
		if (spawnDistance <= 0)
		{
			// core expresses "never materialise by proximity" as the Manual policy, because
			// SetLifecyclePolicy ignores non-positive distances and a ProximityDriven group stamped
			// with 0 would silently keep vanilla's own defaults.
			group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.Manual);
			return;
		}

		int despawnDistance = OVT_VirtualizationMath.ResolveDespawnDistance(spawnDistance, DESPAWN_HYSTERESIS);

		group.SetLifecyclePolicy(SCR_EAIGroupLifecyclePolicy.ProximityDriven, spawnDistance, despawnDistance, -1);
	}

	//------------------------------------------------------------------------------------------------
	// Queries
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! The force, plus the transport and its crew while they exist.
	override array<IEntity> GetSpawnedEntities()
	{
		array<IEntity> entities = super.GetSpawnedEntities();

		if (m_Truck)
			entities.Insert(m_Truck);

		if (m_iCrewHandle != -1)
		{
			OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
			if (virtualization)
			{
				SCR_AIGroup crew = virtualization.GetGroup(m_iCrewHandle);
				if (crew)
					entities.Insert(crew);
			}
		}

		return entities;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the transport is still a working vehicle.
	//! ⚠ IT ASKS "IS IT A WRECK", NOT "CAN IT DRIVE", AND THE GAP IS REAL BUT COVERED ELSEWHERE. A truck
	//! whose wheels or engine are dead answers IsDestroyed() false and passes this test, while vanilla's
	//! own SCR_AIUtils.VehicleCanMove (`GetMovementDamage() < 1`) calls it unusable and
	//! SCR_AILeaveStaticVehicles walks its crew out of it. Widening this test would be a second way to
	//! reach the same conclusion: the uncrewed test on BOTH legs already writes an immobilised transport
	//! off within about a minute of its crew leaving, with an honest log line saying nobody is driving it.
	protected bool IsTruckOperational()
	{
		if (!m_Truck)
			return false;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(m_Truck.FindComponent(SCR_DamageManagerComponent));
		if (damageManager && damageManager.IsDestroyed())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ ASKED OF THE SURVIVOR MASK, NEVER OF AN AGENT COUNT. A dormant or spawn-queued group reports
	//! zero agents while being perfectly alive, and a convoy that dismounted its passengers every time
	//! its crew went briefly dormant would never deliver anybody.
	//! \return Whether the transport still has a crew.
	protected bool IsCrewAlive()
	{
		if (m_iCrewHandle == -1)
			return false;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return false;

		if (!virtualization.IsRegistered(m_iCrewHandle))
			return false;

		return virtualization.GetAliveMemberCount(m_iCrewHandle) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The owner key the CREW is registered under - UNIQUE TO THIS ONE INSERTION, not to its deployment
	//! and not to its config. See CREW_KEY_SUFFIX for why it must also differ from the passengers'.
	//!
	//! ==========================================================================================
	//! 🔴 A CREW IS NEVER INHERITED. "It shouldn't matter anyway, just spawn a new crew." (author,
	//! 2026-08-21.)
	//! ==========================================================================================
	//! A crew exists to drive ONE truck. Every insertion spawns its own truck, so there is no reading
	//! of "reuse the last crew" that is ever the right answer - the men it would inherit are somewhere
	//! else entirely, usually walking home from the last drop, and the new truck sits at its spawn with
	//! nobody in it until the uncrewed grace runs out and the whole force walks.
	//!
	//! ⚠ AND THE DEPLOYMENT KEY IS NOT ENOUGH TO PREVENT THAT, WHICH IS THE POINT OF THE SERIAL. The
	//! deployment half comes from OVT_DeploymentVirtualKey.DeriveKey(configName, x, z) plus a collision
	//! ORDINAL, and that ordinal is PROBED AGAINST LIVE DEPLOYMENTS (OVT_DeploymentManagerComponent
	//! .NextKeyOrdinal) - deliberately, so it is self-healing across saves. The consequence is that the
	//! ordinal is FREED when a deployment ends: an objective that sends 'Objective Sabotage' to the same
	//! base twice, the second time after the first has been collected, composes byte-for-byte the same
	//! deployment key, hence the same crew key, hence a reclaim that adopts whatever the first one left
	//! behind. That is not a hypothetical shape - it is the ordinary rhythm of an objective plan.
	//!
	//! ⚠ THE RECLAIM IN EnsureCrew() IS NOT DELETED, AND MUST NOT BE. It is what makes the convergence
	//! idempotent WITHIN one insertion (see its own comment). Scoping the key to the instance keeps that
	//! property while making a cross-insertion match impossible to compose, which is the only change
	//! wanted here.
	//!
	//! ⚠ NOTHING IS MINTED UNTIL THERE IS A DEPLOYMENT TO KEY AGAINST. BuildOwnerKey answers empty before
	//! the parent deployment has a virtual key; caching that - or burning a serial on it - would give
	//! this insertion a key that never matches the one its own crew was registered under.
	//! \return The key, or an empty string when there is no deployment to key against.
	protected string GetCrewOwnerKey()
	{
		if (!m_sCrewOwnerKey.IsEmpty())
			return m_sCrewOwnerKey;

		string deploymentScoped = BuildOwnerKey(m_sModuleName + CREW_KEY_SUFFIX);
		if (deploymentScoped.IsEmpty())
			return "";

		s_iCrewKeySerial = s_iCrewKeySerial + 1;

		m_sCrewOwnerKey = deploymentScoped + CREW_INSTANCE_MARK + s_iCrewKeySerial.ToString();

		return m_sCrewOwnerKey;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle prefab for this module's transport type.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \return The prefab, or an empty ResourceName - which is one of the five roads to walking.
	protected ResourceName GetVehiclePrefabFromFaction(int factionIndex)
	{
		OVT_OverthrowFactionManager factions = OVT_Global.GetFactions();
		if (!factions)
			return "";

		OVT_Faction faction = factions.GetOverthrowFactionByIndex(factionIndex);
		if (!faction)
		{
			Print(string.Format("[Overthrow] Insertion '%1': faction index %2 resolves to no faction",
				DescribeSelf(), factionIndex.ToString()), LogLevel.WARNING);
			return "";
		}

		faction.InitializeVehicleRegistry();

		ResourceName prefab = faction.GetVehiclePrefabByName(m_sTruckVehicleType);
		if (prefab.IsEmpty())
		{
			Print(string.Format("[Overthrow] Insertion '%1': transport type '%2' is not in faction '%3's registry",
				DescribeSelf(), m_sTruckVehicleType, faction.GetFactionKey()), LogLevel.WARNING);
		}

		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! How this insertion names itself in a log line: the deployment, then the module within it.
	//! \return "<deployment name>/<module name>".
	protected string DescribeSelf()
	{
		string deploymentName = "unknown deployment";
		if (m_ParentDeployment)
			deploymentName = m_ParentDeployment.GetDeploymentName();

		if (m_sModuleName.IsEmpty())
			return deploymentName;

		return deploymentName + "/" + m_sModuleName;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What stage of its journey this insertion is at.
	OVT_EInsertionState GetInsertionState()
	{
		return m_eState;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where the force set out from; vector.Zero before an origin has been resolved.
	vector GetInsertionSource()
	{
		return m_vSource;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Where the transport is headed; only meaningful once the state is DRIVING.
	vector GetLandingZone()
	{
		return m_vLZ;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this insertion currently holds one of its faction's convoy slots.
	bool HoldsInsertionReservation()
	{
		return m_bReserved;
	}

	//------------------------------------------------------------------------------------------------
	// Cloning
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! EVERY inherited attribute plus this module's own ten.
	//!
	//! ⚠ CloneModule IS NOT CHAINED - it builds a fresh instance and copies BY HAND - so the thirteen
	//! lines from OVT_InfantrySpawningDeploymentModule have to be repeated here verbatim, and anything
	//! appended there has to be appended here as well. A forgotten line does not warn, does not log and
	//! does not fail to parse: it ships the CLASS DEFAULT on every deployment, forever. That is how
	//! m_fMaxCruiseSpeed was lost on the vehicle module for a whole release.
	//!
	//! What a dropped line would cost here specifically: drop m_Source and the module registers NOTHING
	//! AT ALL, silently, because it refuses to spawn a force from thin air; drop m_sTruckVehicleType
	//! and every insertion in the campaign walks; drop m_fArrivalRadius and every convoy drives past
	//! its landing zone until the stuck test catches it, which it never will while the truck is moving.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_InsertionSpawningDeploymentModule clone = new OVT_InsertionSpawningDeploymentModule();

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

		// --- This module's own ten. The provider is shared rather than deep-copied, exactly as the
		//     placed-infantry module shares its placement provider: a provider is a stateless answerer.
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

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintInsertionDebugInfo()
	{
		Print(string.Format("Insertion Module: %1", DescribeSelf()));
		Print(string.Format("  State: %1", typename.EnumToString(OVT_EInsertionState, m_eState)));
		Print(string.Format("  Source: %1  LZ: %2", m_vSource.ToString(), m_vLZ.ToString()));
		string reservation = "no";
		if (m_bReserved)
			reservation = "yes";
		Print(string.Format("  Holds a convoy slot: %1", reservation));
		Print(string.Format("  Crew handle: %1  Stuck ticks: %2  Return ticks: %3",
			m_iCrewHandle.ToString(), m_iStuckTicksElapsed.ToString(), m_iReturnTicksElapsed.ToString()));
		string crewKey = m_sCrewOwnerKey;
		if (crewKey.IsEmpty())
			crewKey = "not minted - this insertion has never asked for a crew";
		Print(string.Format("  Crew owner key: %1", crewKey));
		string abandoned = "no";
		if (m_bTruckAbandoned)
			abandoned = "yes";
		Print(string.Format("  Transport abandoned: %1  Ticks towards collection: %2 of %3",
			abandoned, m_iAbandonedTicksElapsed.ToString(), STUCK_TRUCK_TIMEOUT_TICKS.ToString()));
		Print(string.Format("  Owned waypoints: %1  Riders paired: %2",
			m_aOwnedWaypoints.Count().ToString(), m_mRiderIsCrew.Count().ToString()));
		Print(string.Format("  Uncrewed ticks: %1 of %2  At the wheel: %3",
			m_iUncrewedTicksElapsed.ToString(), m_iStuckTicks.ToString(), CrewIsAtTheWheel().ToString()));

		// The same line the stall path prints, on demand. See DescribeCrewLiveness() for how to read it.
		Print(string.Format("  Crew liveness: %1", DescribeCrewLiveness()));
	}
}
