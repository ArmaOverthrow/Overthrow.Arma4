class OVT_QRFControllerComponentClass: OVT_ComponentClass
{
};
class OVT_QRFControllerComponent: OVT_Component
{
	//Last valid target zone found — fallback when the random search exhausts its attempts
	protected vector m_vGoodTargetPos = "0 0 0";

	[RplProp()]
	int m_iWinningFaction = -1;
	
	[RplProp()]
	int m_iPoints = 0;
			
	int m_iTimer = 120000;
	
	ref array<ref EntityID> m_Groups;
	
	protected const int UPDATE_FREQUENCY = 10000;
	const float QRF_RANGE = 750;
	const float QRF_DEPTH = 200;
	const float QRF_POINT_RANGE = 220;
	
	ref ScriptInvoker m_OnFinished = new ScriptInvoker();
	
	int m_iUsedResources = 0;
	
	OVT_OccupyingFactionManager m_OccupyingFaction;
	
	ref array<ResourceName> m_aSpawnQueue = {};
	ref array<vector> m_aSpawnPositions = {};
	ref array<vector> m_aSpawnTargets = {};

	//! Where each queued group is to stand once it lands - the siege ring (occupying/counter-attacks
	//! §3.9). EMPTY IN STANDARD MODE, and nothing reads it there.
	//!
	//! ⚠ THE FOURTH INDEX-PARALLEL ARRAY. m_aSpawnQueue / m_aSpawnPositions / m_aSpawnTargets and this
	//! one are addressed by the SAME index and SpawnFromQueue() takes index 0 off each of them. Adding
	//! an entry to one of the four without adding it to the others, or removing from three of the four,
	//! silently gives every later group somebody else's slot - and there is no symptom beyond groups
	//! standing in the wrong place.
	ref array<vector> m_aSpawnRingSlots = {};

	ref array<vector> m_Bases = {};
	int m_iResourcesLeft = 0;
	int m_iLZMax = 750;
	int m_iLZMin = 250;
	int m_iPreferredDirection = -1;
	int m_iDirectionVariance = 30;

	//! Which kind of battle this is (D14). ⚠ SET BY THE CALLER BEFORE Start(), following the existing
	//! SpawnQRFController -> configure -> Start() order that m_iLZMin and friends already use. Start()
	//! deliberately takes no arguments.
	//!
	//! ⚠ THE DEFAULT IS THE WHOLE SAFETY ARGUMENT: anything that spawns a controller and forgets to
	//! configure it gets a player-initiated battle, which is today's behaviour.
	OVT_EQRFMode m_eMode = OVT_EQRFMode.STANDARD;

	//! How far along a COUNTER_ATTACK battle is. Meaningless in STANDARD mode - IsEngaged() short-
	//! circuits on the mode before it ever looks at this.
	protected OVT_EQRFStage m_eStage = OVT_EQRFStage.SILENT_DEPLOY;

	//! What UpdateQRFTimer last broadcast, so the muster clock can be published on a cadence instead of
	//! on every tick. Negative means "nothing published yet"; see OVT_QRFSiege.ShouldPublishTimer.
	protected int m_iLastPublishedTimer = -1;

	//! Whether any tracked siege group has EVER been resolved to a live group entity.
	//!
	//! ==================================================================================================
	//! 🔴 THE ARMING LATCH FOR THE EARLY END. WITHOUT IT, "I CANNOT FIND IT" READS AS "IT IS DEAD".
	//! ==================================================================================================
	//! m_Groups holds EntityIDs, and CheckSiegeWipedOut resolves each one with FindEntityByID. A null
	//! answer there is AMBIGUOUS in exactly the way D16 says must never be trusted:
	//!
	//!   - the group really was deleted, which IS death; or
	//!   - THE ID NEVER RESOLVED IN THE FIRST PLACE. An entity that is not world-registered answers
	//!     GetID() with EntityID.INVALID - and EVERY unregistered entity shares that one value
	//!     (recorded in this tree at OVT_InactiveRecruitGroupComponent.c:76-83, where core's observer
	//!     map hit the same trap). SpawnFromQueue reads group.GetID() in the SAME FRAME as the spawn,
	//!     so an id that has not registered yet is a reading this component can genuinely take.
	//!
	//! Read the second case as death and the FIRST early-end tick of a perfectly healthy siege declares
	//! the whole force wiped out, jumps to BATTLE against men nobody has fought, and hands the
	//! resistance the objective for free with nothing in the log. This latch makes that impossible:
	//! nothing that has never been seen alive can be declared dead.
	//!
	//! ⚠ IT IS ARMED ONLY BY AN ACTUAL SUCCESSFUL RESOLUTION, never by "we spawned something". Arming it
	//! at the spawn site would re-open the very hazard it exists to close - a batch of unusable ids
	//! would arm it and then immediately read as a wipe.
	//!
	//! ⚠ IT DOES NOT MAKE THE EARLY END UNREACHABLE. A healthy siege resolves live groups on its first
	//! MUSTER-stage check, arms this, and fires normally the moment they are all down. The one case it
	//! gives up is a force wiped out before ANY early-end check ever saw it standing - which then simply
	//! waits out its clock. That is D16's own preferred failure: late, never early.
	protected bool m_bSiegeForceSeenAlive;

	//! How many passes the single-pass siege spend is allowed to make over the source list before it
	//! gives up. ⚠ A SAFETY NET, NOT A DESIGN BOUND - the budget is what really ends the loop, and a
	//! pass that allocates nothing breaks out immediately. This exists so that a misauthored
	//! baseResourceCost can never spin the server thread forever with nothing in the log.
	protected const int MAX_SIEGE_SPEND_PASSES = 64;

	//! How far in a rejected ring slot is pulled on each retry, and how many times, and the innermost
	//! radius it may ever reach. Walking IN rather than re-rolling the bearing keeps the ring even -
	//! a re-rolled bearing would put two groups on the same side of a coastal town.
	protected const float SIEGE_RING_INWARD_STEP = 15;
	protected const int SIEGE_RING_INWARD_ATTEMPTS = 8;
	protected const float SIEGE_RING_INWARD_FLOOR = 25;


	override void OnPostInit(IEntity owner)
	{
		m_iPoints = 0;
		m_iWinningFaction = -1;		
		
		super.OnPostInit(owner);
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
					
		if(!Replication.IsServer()) return;
		
		GetGame().GetCallqueue().CallLater(CheckUpdateTimer, 1000, true, owner);		
		
		m_Groups = new array<ref EntityID>;		
		Replication.BumpMe();		
		
		GetGame().GetCallqueue().CallLater(CheckUpdatePoints, UPDATE_FREQUENCY, true, owner);		
	}
	
	//! ⚠ TAKES NO ARGUMENTS, DELIBERATELY. Everything this battle needs - the landing-zone band, the
	//! preferred direction, and from this feature the MODE - is written onto the component by the
	//! caller between SpawnQRFController() and here. Adding a parameter would give the mode two
	//! configuration orders to be wrong about.
	void Start()
	{
		SendTroops();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the shooting has actually started, as opposed to a battle object merely existing (D15).
	//!
	//! Three server-side gates read this and mean "is a battle being fought": the occupying faction's
	//! economy tick, deployment evaluation, and the objective town's civilian crowd. Under a 30-minute
	//! silent siege, answering those three with "a QRF exists" would empty the target town of
	//! civilians, freeze every deployment on the map and stall the faction's income for half an hour
	//! BEFORE the resistance is told anything - the loudest possible tell, and a dead world besides.
	//!
	//! ⚠ TRUE FROM CREATION FOR A STANDARD BATTLE, so player-initiated battles behave exactly as they
	//! do today. m_CurrentQRF keeps its own separate meaning - a battle object exists, no second one
	//! may start, the objective director stands down - and is NOT replaced by this.
	//! \return True when this battle is being fought.
	bool IsEngaged()
	{
		if(m_eMode == OVT_EQRFMode.STANDARD) return true;

		return m_eStage == OVT_EQRFStage.BATTLE;
	}

	//! How far along a counter-attack siege is. SILENT_DEPLOY for a standard battle, always, and
	//! meaningless there.
	OVT_EQRFStage GetStage()
	{
		return m_eStage;
	}

	//------------------------------------------------------------------------------------------------
	//! One second of the battle. Installed on a 1 000 ms repeating call in OnPostInit and driven by
	//! nothing else in normal play.
	//!
	//! ⚠ PUBLIC ONLY SO THAT THE INITIALISATION TIER CAN DRIVE EXACTLY ONE TICK, which is the same
	//! reason OVT_ObjectiveDirectorComponent.DirectorTick() is public. Nothing in the game calls it.
	void CheckUpdateTimer()
	{
		// THE SIEGE HAS ITS OWN CLOCK, AND IT IS NOT A 120-SECOND COUNTDOWN. Everything below this
		// branch is the standard battle, byte for byte as it has always been.
		if(m_eMode == OVT_EQRFMode.COUNTER_ATTACK)
		{
			TickSiegeTimer();
			return;
		}

		if(m_iTimer < 105000) //Wait 15 seconds for everything to despawn
			SpawnFromQueue();

		m_iTimer -= 1000;

		if(m_iTimer < 0)
		{
			m_iTimer = 0;
			return;
		}

		m_OccupyingFaction.UpdateQRFTimer(m_iTimer);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! One second of a counter-attack siege (§3.9's stage table), on the same 1 000 ms call the
	//! standard countdown uses.
	//!
	//! ⚠ THE STANDARD SPAWN CONDITION DOES NOT APPLY HERE. `m_iTimer < 105000` is a standard-mode
	//! expression of "wait fifteen seconds for everything to despawn"; in SILENT_DEPLOY the clock does
	//! not move at all, so it would be permanently false and nothing would ever spawn. The drain gets
	//! its own condition - and the wait is not wanted either, because in SILENT_DEPLOY nothing is being
	//! suppressed yet and there is nothing to wait for.
	protected void TickSiegeTimer()
	{
		if(m_eStage == OVT_EQRFStage.SILENT_DEPLOY)
		{
			// One group a second, exactly the cadence the standard path uses - it is the frame-load
			// spreader, not a dramatic device.
			if(m_aSpawnQueue.Count() > 0)
			{
				SpawnFromQueue();
				return;
			}

			// The last group is on the ground: the encirclement is complete.
			EnterMuster();
			return;
		}

		if(m_eStage == OVT_EQRFStage.MUSTER)
		{
			m_iTimer -= 1000;

			if(m_iTimer <= 0)
			{
				m_iTimer = 0;
				EnterBattle("the muster window ran out");
				return;
			}

			PublishSiegeTimer();
			return;
		}

		// BATTLE: nothing to do here. CheckUpdatePoints owns it from now on, exactly as it owns the
		// second half of a standard battle.
	}

	//------------------------------------------------------------------------------------------------
	//! Broadcasts the muster clock, but only when OVT_QRFSiege says it is worth a packet.
	//!
	//! ⚠ WHY THIS IS NOT JUST UpdateQRFTimer(). That method broadcasts a reliable RPC to EVERY client.
	//! On the standard 120-second countdown it fires 120 times; on a 30-minute muster it would fire
	//! 1 800 times to say things a minutes display cannot show.
	protected void PublishSiegeTimer()
	{
		if(!OVT_QRFSiege.ShouldPublishTimer(m_iTimer, m_iLastPublishedTimer)) return;

		m_iLastPublishedTimer = m_iTimer;
		m_OccupyingFaction.UpdateQRFTimer(m_iTimer);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! SILENT_DEPLOY -> MUSTER. The encirclement is complete, so the resistance is told.
	//!
	//! ⚠ THE REVEAL IS THE MANAGER'S, not this component's: it owns the replicated flag, the
	//! notification tags and the knowledge of whether this battle is for a base or a town.
	protected void EnterMuster()
	{
		m_eStage = OVT_EQRFStage.MUSTER;

		// The 120 000 the field was constructed with was never counted down; the muster window
		// replaces it outright.
		m_iTimer = OVT_QRFSiege.MUSTER_TIME_MS;
		m_iLastPublishedTimer = -1;

		Print("[Overthrow.QRFControllerComponent] Counter-attack: the encirclement is complete with " + m_Groups.Count().ToString() + " groups on the ring; muster window " + OVT_QRFSiege.MUSTER_TIME_MS.ToString() + " ms");

		m_OccupyingFaction.RevealQRF();

		// The negative sentinel above guarantees this one goes out whatever the cadence would say.
		PublishSiegeTimer();
	}

	//------------------------------------------------------------------------------------------------
	//! MUSTER -> BATTLE. The assault proper: orders change, the clock reads zero, scoring begins and
	//! the world stops living around the objective.
	//!
	//! ⚠ EVERY SIEGE PASSES THROUGH HERE. Scoring is gated on `m_iTimer <= 0`, which is unreachable in
	//! SILENT_DEPLOY (the clock is parked at its construction default) and in MUSTER (reaching zero IS
	//! this transition), so a siege cannot resolve without it. That is load-bearing: the civilian
	//! suppression is a PAIRED transition fired here and again at the finish, and a path that skipped
	//! this one would leave a town permanently empty.
	//! \param[in] reason Why the stage advanced, for the log.
	protected void EnterBattle(string reason)
	{
		if(m_eStage == OVT_EQRFStage.BATTLE) return;

		m_eStage = OVT_EQRFStage.BATTLE;
		m_iTimer = 0;

		// ⚠ PUBLISHED UNCONDITIONALLY, bypassing the cadence. This is the one broadcast that switches
		// every client's panel from a countdown to "#OVT-BattleProgress", and a rate limiter must not
		// be able to withhold it.
		m_iLastPublishedTimer = 0;
		m_OccupyingFaction.UpdateQRFTimer(0);
		Replication.BumpMe();

		IssueAssaultOrders();

		// The three world-suppression gates come on HERE and nowhere else.
		m_OccupyingFaction.OnQRFEngaged();

		Print("[Overthrow.QRFControllerComponent] Counter-attack: the assault has begun - " + reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Turns the ring inward: every surviving group drops what it was holding and attacks.
	//!
	//! ⚠ THE HELD WAYPOINT MUST COME OFF FIRST. AddWaypoint APPENDS, and a Defend waypoint is never
	//! completed, so a group handed SearchAndDestroy on top of it would defend its ring slot forever
	//! and the assault would simply never happen. The remove-then-add shape is vanilla's own
	//! (SCR_WaypointGroupCommand.SetWaypointForAIGroup); like vanilla it does NOT delete the waypoint
	//! entities, because a waypoint may be shared and today's battles already leave their completed
	//! ones standing.
	protected void IssueAssaultOrders()
	{
		BaseWorld world = GetGame().GetWorld();

		foreach(EntityID id : m_Groups)
		{
			IEntity groupEntity = world.FindEntityByID(id);
			if(!groupEntity) continue;

			SCR_AIGroup aigroup = SCR_AIGroup.Cast(groupEntity);
			if(!aigroup) continue;

			array<AIWaypoint> held = new array<AIWaypoint>;
			aigroup.GetWaypoints(held);
			foreach(AIWaypoint wp : held)
			{
				if(!wp) continue;
				aigroup.RemoveWaypoint(wp);
			}

			// Per group rather than once for the whole ring, so twelve groups converging do not all
			// walk at one blade of grass. GetTargetZone answers in a 50 m radius of the objective and
			// normally succeeds on its first roll.
			AddWaypoint(GetTargetZone(GetOwner().GetOrigin()), aigroup, "SearchAndDestroy");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! THE EARLY END: "if all of the spawned enemy is neutralised, scoring begins."
	//!
	//! Asked on CheckUpdatePoints' existing 10 s cadence, in MUSTER only. Wiping a siege out before its
	//! clock runs down ends the battle there; once BATTLE is entered, the unmodified scoring loop
	//! awards the resistance +5 a tick against an empty zone and the objective is theirs. That is the
	//! intended reward, and it needs no code of its own.
	//!
	//! ==============================================================================================
	//! 🔴 A GROUP WITH ZERO AGENTS AND A LIVE ENTITY COUNTS AS **ALIVE**. THIS IS NOT AN OVERSIGHT.
	//! ==============================================================================================
	//! "Zero agents means the group is dead" is a KNOWN-BAD prune in this engine: the AI spawn queue
	//! and dormancy can both legitimately report a group with no agents, and it is unfixed at HEAD.
	//! Siege groups are spawned live and never virtualised, so it should not arise here - but a false
	//! positive does not cost a little accuracy. It ends the muster window early, starts scoring
	//! against a force that is still standing, and hands the resistance the objective for free, with
	//! nothing in any log to explain it. The failure this leaves instead is the harmless one: a siege
	//! that had already been wiped out sits out the rest of its clock.
	protected void CheckSiegeWipedOut()
	{
		BaseWorld world = GetGame().GetWorld();

		int tracked = 0;
		int neutralised = 0;
		int resolvedNow = 0;

		foreach(EntityID id : m_Groups)
		{
			tracked++;

			IEntity groupEntity = world.FindEntityByID(id);
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(groupEntity);

			int agentCount = 0;
			int fitCount = 0;

			if(aigroup)
			{
				resolvedNow++;

				autoptr array<AIAgent> agents = new array<AIAgent>;
				aigroup.GetAgents(agents);

				agentCount = agents.Count();

				foreach(AIAgent agent : agents)
				{
					if(!agent) continue;

					if(IsFightingFit(agent.GetControlledEntity())) fitCount++;
				}
			}

			// 🔴 THE ZERO-AGENT RULE LIVES IN OVT_QRFSiege.GroupNeutralised, not here, so that it can
			// be asserted in the cheapest test tier instead of only being commented. A resolved group
			// with no agents comes back ALIVE. See the block above for why.
			if(OVT_QRFSiege.GroupNeutralised(aigroup != null, agentCount, fitCount)) neutralised++;
		}

		// 🔴 ARM THE LATCH FROM THIS TICK'S OWN OBSERVATION, BEFORE TESTING IT. A siege whose groups are
		// all standing on its first check arms here and can be judged from that same tick onward; the
		// later tick on which they are all gone finds resolvedNow == 0 but an already-armed latch, and
		// fires correctly. See m_bSiegeForceSeenAlive for the whole argument.
		if(resolvedNow > 0) m_bSiegeForceSeenAlive = true;

		// 🔴 NOTHING THAT WAS NEVER SEEN ALIVE MAY BE DECLARED DEAD. Every id being unresolvable is not
		// evidence of a wipe - it is exactly what a batch of not-yet-registered ids looks like, and
		// treating it as a wipe hands the resistance the objective for free on the first check.
		if(!m_bSiegeForceSeenAlive) return;

		// ⚠ AllNeutralised(0, 0) IS FALSE. Nothing tracked is not "everything is dead".
		if(!OVT_QRFSiege.AllNeutralised(tracked, neutralised)) return;

		EnterBattle("every one of the " + tracked.ToString() + " besieging groups was neutralised before the muster window ran out");
	}

	void KillAll()
	{
		BaseWorld world = GetGame().GetWorld();
		foreach(EntityID id : m_Groups)
		{
			IEntity group = world.FindEntityByID(id);
			if(!group) continue;
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
			if(!aigroup) continue;
			autoptr array<AIAgent> agents = new array<AIAgent>;
			aigroup.GetAgents(agents);
			foreach(AIAgent agent : agents)
			{
				DamageManagerComponent damageManager = DamageManagerComponent.Cast(agent.FindComponent(DamageManagerComponent));
				if (damageManager && damageManager.IsDamageHandlingEnabled())
					damageManager.SetHealthScaled(0);
			}
		}
	}

	
	//------------------------------------------------------------------------------------------------
	//! Whether a character is up and awake, and so counts towards zone control.
	//!
	//! Both questions, because they are different ones: GetLifeState() answers DEAD/INCAPACITATED, and
	//! IsUnconscious() additionally covers the wake-up animation, during which the life state has
	//! already flipped back to ALIVE (CharacterControllerComponent.c:262-267).
	//! \param[in] entity The character to test.
	//! \return True when it is alive and conscious.
	protected bool IsFightingFit(IEntity entity)
	{
		if(!entity) return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if(!controller) return false;

		if(controller.GetLifeState() != ECharacterLifeState.ALIVE) return false;

		return !controller.IsUnconscious();
	}

	//------------------------------------------------------------------------------------------------
	//! Ten seconds of the battle: the siege's early-end check, then zone scoring once the countdown has
	//! reached zero. Installed on a 10 000 ms repeating call in OnPostInit.
	//!
	//! ⚠ PUBLIC ONLY SO THAT THE INITIALISATION TIER CAN DRIVE EXACTLY ONE TICK. See CheckUpdateTimer.
	void CheckUpdatePoints()
	{
		BaseWorld world = GetGame().GetWorld();

		// THE SIEGE'S EARLY END, on this method's own 10 s cadence and in MUSTER only. It may set
		// m_iTimer to 0, in which case the scoring block below runs in this same call - which is
		// correct: the reward for wiping a siege out before it lands is that scoring starts at once.
		if(m_eMode == OVT_EQRFMode.COUNTER_ATTACK && m_eStage == OVT_EQRFStage.MUSTER)
			CheckSiegeWipedOut();

		if(m_iTimer <= 0)
		{
			int enemyNum = 0;
			int playerNum = 0;
			int recruitNum = 0;
			int enemyTotal = 0;

			PlayerManager mgr = GetGame().GetPlayerManager();

			//Hoisted out of the loop - these were resolved once per agent, every 10 seconds
			string occupyingKey = m_Config.m_sOccupyingFaction;
			string resistanceKey = m_Config.m_sPlayerFaction;

			array<AIAgent> groups();
			GetGame().GetAIWorld().GetAIAgents(groups);
			foreach(AIAgent group : groups)
			{
				if(!group) continue;
				IEntity entity = group.GetControlledEntity();
				if(!entity) continue;
				SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
				if(!character) continue;

				string factionKey = character.GetFactionKey();
				bool isResistance = factionKey == resistanceKey;
				if(!isResistance && factionKey != occupyingKey) continue;

				//A body on the ground holds no ground - dead and downed count for neither side
				if(!IsFightingFit(entity)) continue;

				float dist = vector.Distance(character.GetOrigin(),GetOwner().GetOrigin());

				if(isResistance)
				{
					//Recruits (and any other resistance AI) hold the zone alongside the players who
					//brought them - before this, an all-AI assault force scored nothing and lost by
					//default. Players are counted in their own loop below for XP, and a player-
					//controlled character can still surface as an agent, so don't count it twice.
					if(mgr.GetPlayerIdFromControlledEntity(entity) > 0) continue;
					if(dist < QRF_POINT_RANGE) recruitNum++;
					continue;
				}

				if(dist < QRF_POINT_RANGE)
				{
					enemyNum += 1;
				}
				if(dist < QRF_RANGE)
				{
					enemyTotal += 1;
				}
			}

			autoptr array<int> players = new array<int>;
			int numplayers = mgr.GetPlayers(players);

			if(numplayers > 0)
			{
				foreach(int playerID : players)
				{
					IEntity player = mgr.GetPlayerControlledEntity(playerID);
					if(!player) continue;
					//Same rule the AI gets: a corpse or a downed player neither holds the zone nor earns XP
					if(!IsFightingFit(player)) continue;
					float distance = vector.Distance(player.GetOrigin(), GetOwner().GetOrigin());
					if(distance < QRF_POINT_RANGE)
					{
						OVT_Global.GetSkills().GiveXP(playerID, 2);
						playerNum++;
					}
				}
			}
			
			//Zone control is a head count: every fighter the resistance has in the zone counts,
			//human or recruit
			int resistanceNum = playerNum + recruitNum;

			if(resistanceNum > 0 && enemyTotal == 0){
				//push towards resistance fast
				m_iPoints += 5;
			}else{
				if(resistanceNum == enemyNum)
				{
					//push towards zero
					if(m_iPoints > 0) m_iPoints--;
					if(m_iPoints < 0) m_iPoints++;
				}else{
					if(resistanceNum > enemyNum)
					{
						//push towards resistance
						m_iPoints++;
					}else{
						//push towards OF
						m_iPoints--;
					}
				}	
			}		
			
			int toWin = m_Config.m_Difficulty.QRFPointsToWin;
			
			if(m_iPoints > toWin) m_iPoints = toWin;
			if(m_iPoints < -toWin) m_iPoints = -toWin;
			
			m_OccupyingFaction.UpdateQRFPoints(m_iPoints);		
			
			if(m_iPoints > 0) m_iWinningFaction = OVT_Global.GetConfig().GetPlayerFactionIndex();
			if(m_iPoints < 0) m_iWinningFaction = OVT_Global.GetConfig().GetOccupyingFactionIndex();
			if(m_iPoints == 0) m_iWinningFaction = -1;
			
			if(m_iPoints >= toWin || m_iPoints <= -toWin)
			{
				//We have a winner		
				m_OnFinished.Invoke();
				GetGame().GetCallqueue().Remove(CheckUpdatePoints);
				GetGame().GetCallqueue().Remove(CheckUpdateTimer);
			}
		}
	}
	
	protected void SendTroops()
	{
		m_aSpawnQueue.Clear();
		m_aSpawnPositions.Clear();
		m_aSpawnTargets.Clear();
		// ⚠ THE FOURTH PARALLEL ARRAY - cleared here, filled after the spawn pass, and popped in
		// SpawnFromQueue alongside the other three. See its declaration.
		m_aSpawnRingSlots.Clear();

		vector qrfpos = GetOwner().GetOrigin();
		
		//Get valid bases to use for QRF
		
		foreach(OVT_BaseData data : m_OccupyingFaction.m_Bases)
		{			
			vector pos = data.location;
			float dist = vector.Distance(pos, qrfpos);
			
			if(!data.IsOccupyingFaction()) continue;
			if(dist < 20) continue; //QRF is for this base, ignore it
			
			m_Bases.Insert(pos);
		}

		// THE OCCUPYING FACTION'S FORWARD OPERATING BASE IS A WAVE SOURCE TOO (occupying/counter-attacks
		// T8.2), and it is inserted HERE - after the held-base loop and before the no-bases fallback - so
		// that a campaign whose only remaining foothold near the target is the forward base uses it
		// instead of the "troops out of the sea" placeholder below.
		//
		// The same >20 m rule the loop above applies: a source standing on the objective is not a source,
		// it is the thing being fought over.
		//
		// ⚠ m_Bases IS BUILT ONCE AND NEVER REFRESHED, so a forward base torn down mid-battle keeps
		// sending troops - exactly as a captured source base already does (:255). That staleness is
		// PRE-EXISTING and is deliberately left alone here: fixing it changes player-initiated battles
		// too and belongs to whoever owns this file next (D9's excluded list).
		OVT_ObjectiveDirectorComponent director = OVT_Global.GetObjectiveDirector();
		if(director && director.IsFOBUp())
		{
			vector fobpos = director.GetFOBPosition();
			if(vector.Distance(fobpos, qrfpos) >= 20)
			{
				m_Bases.Insert(fobpos);
				Print("[Overthrow.QRFControllerComponent] The occupying faction's forward operating base is a wave source: " + fobpos.ToString());
			}
		}

		if(m_Bases.Count() == 0)
		{
			Print("[Overthrow.QRFControllerComponent] Final Base Detected");
			//Temporary for when the OF has no bases left but this one
			//To-Do: organize an external force to come from the sea/air
			m_Bases.Insert(qrfpos + "250 0 100");
		}
		
		int resources = m_OccupyingFaction.m_iResources;
		if(resources <= 400) resources = 400; //Emergency resources (minimum size QRF)
		
		int max = OVT_Global.GetConfig().m_Difficulty.maxQRF;
		int numPlayersOnline = GetGame().GetPlayerManager().GetPlayerCount();
		m_vGoodTargetPos = "0 0 0";
		//Scale max QRF size by number of players online
		if(numPlayersOnline > 32)
		{
			max *= 6;
		}else if(numPlayersOnline > 24)
		{
			max *= 5;
		}else if(numPlayersOnline > 16)
		{
			max *= 4;
		}else if(numPlayersOnline > 8)
		{
			max *= 3;
		}else if(numPlayersOnline > 4)
		{
			max *= 2;
		}
		
		if(resources > max)
		{
			resources = max;
		}
		
		Print("[Overthrow.QRFControllerComponent] Allocated QRF Size: " + resources.ToString());

		m_iResourcesLeft = resources;
		SendWave();

		// THE RING IS COMPUTED ONCE, FROM THE FINAL QUEUE LENGTH, after the spawn pass has filled the
		// queue and before the first group spawns. Nothing can run between SendWave() returning and
		// this line: SpawnFromQueue is only ever reached from the 1 000 ms timer call, and a siege
		// schedules no follow-up wave, so the queue is final here and stays final.
		if(m_eMode == OVT_EQRFMode.COUNTER_ATTACK)
			BuildSiegeRing(qrfpos);
	}

	//------------------------------------------------------------------------------------------------
	//! One ring slot per queued group: an even circle around the objective at 100-150 m.
	//!
	//! ⚠ COMPUTED FROM THE FINAL QUEUE LENGTH, ALL AT ONCE. Assigning slots as groups spawn - with a
	//! count that grows by one each time - collapses the ring onto a handful of bearings, because
	//! "slot 0 of 1" and "slot 0 of 2" are the same direction.
	//!
	//! ⚠ THE HEIGHT LEAVES HERE AS THE OBJECTIVE'S OWN, exactly as GetLandingZone() keeps it for a
	//! landing zone. That is only safe because a ring slot has exactly one consumer - the "Defend"
	//! waypoint in SpawnFromQueue - and CreateWaypoint() puts every waypoint it makes on the terrain
	//! surface. Ring slots are NOT spawn positions (those come from GetLandingZone), so nothing is
	//! placed at this Y. Give a slot a second consumer and it must be ground-snapped here as well.
	//! \param[in] centre The objective being besieged.
	protected void BuildSiegeRing(vector centre)
	{
		m_aSpawnRingSlots.Clear();

		int count = m_aSpawnQueue.Count();
		if(count == 0)
		{
			Print("[Overthrow.QRFControllerComponent] Counter-attack: no groups were queued, so there is no ring to build");
			return;
		}

		for(int i = 0; i < count; i++)
		{
			float radius = Math.RandomFloatInclusive(OVT_QRFSiege.SIEGE_RING_MIN, OVT_QRFSiege.SIEGE_RING_MAX);
			vector slot = centre + OVT_QRFSiege.RingSlotOffset(i, count, radius);

			// A slot in the water is walked INWARD rather than re-rolled onto a new bearing: the
			// bearing is what makes the ring a ring, and a coastal town would otherwise end up with
			// every rejected group bunched on its landward side.
			int attempts = 0;
			while(OVT_WorldUtils.IsOceanAtPosition(slot) && attempts < SIEGE_RING_INWARD_ATTEMPTS)
			{
				attempts++;

				radius = radius - SIEGE_RING_INWARD_STEP;
				if(radius < SIEGE_RING_INWARD_FLOOR) radius = SIEGE_RING_INWARD_FLOOR;

				slot = centre + OVT_QRFSiege.RingSlotOffset(i, count, radius);
			}

			// Still wet after walking all the way in - a town on a spit. Fall back to the objective
			// itself, which is dry by definition, rather than ordering a group to stand in the sea.
			if(OVT_WorldUtils.IsOceanAtPosition(slot)) slot = centre;

			m_aSpawnRingSlots.Insert(slot);
		}

		Print("[Overthrow.QRFControllerComponent] Counter-attack: " + count.ToString() + " ring slots laid out around " + centre.ToString());
	}

	protected int SendWave()
	{
		int spent = 0;
		int allocate = Math.Floor(m_iResourcesLeft / m_Bases.Count());

		if(allocate > (16 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost))
		{
			allocate = 16 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
		}

		vector qrfpos = GetOwner().GetOrigin();

		// A SIEGE SPENDS ITS WHOLE BUDGET IN ONE PASS AND SCHEDULES NO FOLLOW-UP WAVE: the encirclement
		// has to be complete before anybody is told about it, and a wave arriving 4-8 minutes into the
		// muster window would be arriving after the announcement. The clamp above still bounds what one
		// SOURCE contributes per cycle; the cycling is what spends the rest.
		if(m_eMode == OVT_EQRFMode.COUNTER_ATTACK)
		{
			spent = SpendWholeBudgetInOnePass(qrfpos, allocate);
		}
		else
		{
			foreach(vector base : m_Bases)
			{
				if(m_iResourcesLeft <= 0) break;
				int allocated = 0;

				vector lz = GetLandingZone(base);
				vector target = GetTargetZone(qrfpos);
				int ii = 0;
				while(allocated < allocate && ii < 6)
				{
					ii++;
					allocated += SpawnTroops(lz, target);
				}
				spent += allocated;
				m_iResourcesLeft -= allocated;
				Print("[Overthrow.QRFControllerComponent] Sent troops from " + lz.ToString() + ": " + allocated.ToString());
			}

			if(m_iResourcesLeft > 0)
			{
				//leftover resources, schedule another wave
				GetGame().GetCallqueue().CallLater(SendWave, s_AIRandomGenerator.RandInt(240000, 480000));
			}
		}

		// ⚠ EVERYTHING BELOW RUNS EXACTLY ONCE PER PASS, IN BOTH MODES. The debit is the ONLY place a
		// battle takes resources out of the occupying faction's war chest; skipping it makes a
		// counter-attack free, and running it twice charges for one force twice (BUG-027's shape, in a
		// new place). The mode branch above therefore ends here, and neither side of it returns early.
		m_iUsedResources += spent;

		// QRFs are not free — debit the war chest by what this wave actually committed.
		// The emergency-resources floor can exceed the reserve, so clamp at zero rather
		// than letting the faction go negative.
		m_OccupyingFaction.m_iResources -= spent;
		if(m_OccupyingFaction.m_iResources < 0) m_OccupyingFaction.m_iResources = 0;

		Print("[Overthrow.QRFControllerComponent] Wave complete: " + spent.ToString());

		return spent;
	}

	//------------------------------------------------------------------------------------------------
	//! The counter-attack's spend: cycle the source list until the budget is gone, in one pass, with no
	//! follow-up wave.
	//!
	//! Each cycle gives every source the same bounded slice a standard wave would give it, so no single
	//! base empties itself into one battle; what changes is that the list is walked again, and again,
	//! until there is nothing left to spend.
	//!
	//! ⚠ BOUNDED THREE WAYS, AND ALL THREE ARE LOAD-BEARING:
	//!   1. THE BUDGET. `m_iResourcesLeft <= 0` is the intended exit and the one that fires in practice.
	//!   2. A ZERO-PROGRESS BREAK. A whole cycle that allocated nothing can never allocate anything on
	//!      the next one either - none of the inputs change - so it stops instead of spinning. This is
	//!      reachable today: a misauthored baseResourceCost of 0 makes the per-source slice 0, which
	//!      makes the inner `while(allocated < allocate)` false on entry, forever.
	//!   3. AN ITERATION COUNTER. The belt to (2)'s braces. This runs on the SERVER THREAD inside
	//!      Start(); a loop that cannot terminate here does not degrade the game, it hangs it, with
	//!      nothing in the log.
	//! \param[in] qrfpos The objective being attacked.
	//! \param[in] allocatePerSource The per-source slice SendWave computed, unchanged.
	//! \return What the pass actually committed, for the ONE debit in SendWave.
	protected int SpendWholeBudgetInOnePass(vector qrfpos, int allocatePerSource)
	{
		int spent = 0;
		int passes = 0;

		while(m_iResourcesLeft > 0 && passes < MAX_SIEGE_SPEND_PASSES)
		{
			passes++;

			int spentThisPass = 0;

			foreach(vector base : m_Bases)
			{
				if(m_iResourcesLeft <= 0) break;
				int allocated = 0;

				vector lz = GetLandingZone(base);
				vector target = GetTargetZone(qrfpos);
				int ii = 0;
				while(allocated < allocatePerSource && ii < 6)
				{
					ii++;
					allocated += SpawnTroops(lz, target);
				}
				spentThisPass += allocated;
				m_iResourcesLeft -= allocated;
				Print("[Overthrow.QRFControllerComponent] Counter-attack: sent troops from " + lz.ToString() + ": " + allocated.ToString());
			}

			spent += spentThisPass;

			if(spentThisPass <= 0)
			{
				Print("[Overthrow.QRFControllerComponent] Counter-attack: a whole cycle of the source list allocated nothing with " + m_iResourcesLeft.ToString() + " left in the budget - stopping rather than cycling again");
				break;
			}
		}

		if(passes >= MAX_SIEGE_SPEND_PASSES)
			Print("[Overthrow.QRFControllerComponent] Counter-attack: the spend hit its pass ceiling with " + m_iResourcesLeft.ToString() + " unspent. This is a safety net, not a design bound - check maxQRF against baseResourceCost.");

		return spent;
	}

	protected int SpawnTroops(vector pos, vector targetPos)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
						
		ResourceName res = faction.m_aGroupPrefabSlots.GetRandomElement();
		
		m_aSpawnQueue.Insert(res);
		m_aSpawnPositions.Insert(pos);
		m_aSpawnTargets.Insert(targetPos);
				
		int newres = 8 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	//Chris Schedule WP
	// Corrected waypoint creation and handling
	//------------------------------------------------------------------------------------------------
	//! THE ONE PLACE EVERY QRF WAYPOINT IS BORN, and therefore the one place the height is corrected.
	//!
	//! ⚠ EVERY CALLER USED TO HAND THIS A Y THAT WAS NOT THE GROUND. GetTargetZone() rolls a random
	//! point in a 50 m radius and never touches Y, so it carries the objective entity's own height;
	//! BuildSiegeRing() does the same at 100-150 m, where the error is far worse. A waypoint buried in
	//! a hillside or hanging in the air has a completion radius the group can never reach, so the
	//! Scout/SearchAndDestroy ladder stalls on its first entry and the siege's Defend ring is held at
	//! a spot nobody can stand on.
	//!
	//! Clamped HERE rather than at the call sites because both battle modes - the standard scheduled
	//! ladder, the siege's Defend orders and the BATTLE-transition assault orders - funnel through
	//! this method, and a future caller cannot forget it. This is the same GetSurfaceY() clamp the
	//! virtualization core applies when it spawns a waypoint entity
	//! (OVT_PatrolBehaviorDeploymentModule.SnapPlanPointsToGround).
	//!
	//! ⚠ NO VERTICAL OFFSET. OVT_SpawnPointComponent adds +0.5 because it is placing a physical body
	//! that must not start intersecting the terrain; a waypoint is a navigation target the AI resolves
	//! against the navmesh, so the bare surface height is what it wants.
	//! \param[in] waypointType The waypoint archetype to spawn. Unchanged by this clamp.
	//! \param[in] targetPos Where to put it. Its Y is replaced with the terrain surface.
	//! \return The spawned waypoint.
	protected AIWaypoint CreateWaypoint(string waypointType, vector targetPos) {
		BaseWorld world = GetGame().GetWorld();
		if (world)
			targetPos[1] = world.GetSurfaceY(targetPos[0], targetPos[2]);

	    switch (waypointType) {
	        case "SearchAndDestroy":
	            return OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos);
			case "DefendBase":
	            return OVT_Global.GetConfig().SpawnDefendBaseWaypoint(targetPos);
	        case "GetIn"://nearest
	            return OVT_Global.GetConfig().SpawnGetInWaypoint(targetPos);
	        case "GetOut":
	            return OVT_Global.GetConfig().SpawnGetOutWaypoint(targetPos);
	        case "Loiter":
	            return OVT_Global.GetConfig().SpawnLoiterWaypoint(targetPos);
	        case "Patrol":
	            return OVT_Global.GetConfig().SpawnBasicPatrolWaypoint(targetPos);
	        case "Cycle":
	            return OVT_Global.GetConfig().SpawnLoiterWaypoint(targetPos);
	        case "DefendBase":
	            return OVT_Global.GetConfig().SpawnBasicCycleWaypoint(targetPos);
	        case "Scout":
	            return OVT_Global.GetConfig().SpawnScoutWaypoint(targetPos);
	        case "Defend":
	            return OVT_Global.GetConfig().SpawnDefendWaypoint(targetPos);
	        default:
	            return OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos); // Default case, ensures return
	    }
		return OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(targetPos);
	}

	protected void AddWaypoint(vector targetPos, SCR_AIGroup aigroup, string waypointType)
	{
	    if (aigroup) // Check if the AI group still exists
	    {
	        AIWaypoint waypoint = CreateWaypoint(waypointType, targetPos);
	        aigroup.AddWaypoint(waypoint);
	    }
	}
		
	protected void ScheduleWaypoint(vector targetPos, float delay, SCR_AIGroup aigroup, string waypointType)
	{
	    // Correctly scheduling a waypoint addition without assignment
		GetGame().GetCallqueue().CallLater(AddWaypoint, delay * 1000, false, targetPos, aigroup, waypointType);
	}
	//----------------------------------------------------
	protected void SpawnFromQueue()
	{
		if(m_aSpawnQueue.Count() == 0) return;
		
		ResourceName res = m_aSpawnQueue[0];
		vector pos = m_aSpawnPositions[0];
		vector targetPos = m_aSpawnTargets[0];
		
		BaseWorld world = GetGame().GetWorld();
			
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;						
		spawnParams.Transform[3] = pos;
		IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(res), world, spawnParams);
				
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		m_Groups.Insert(group.GetID());
		// GM group registry. Index -1 on purpose: the base or town this QRF is answering for lives
		// on the occupying faction manager (m_iCurrentQRFBase / m_iCurrentQRFTown), not here, and
		// both are already replicated to every client.
		OVT_GMGroupRegistry.Tag(group, OVT_EGroupOrigin.QRF, -1, "QRF");

		if(m_eMode == OVT_EQRFMode.COUNTER_ATTACK)
		{
			// ONE Defend waypoint on this group's ring slot, instead of the Scout/Scout/SaD/SaD ladder
			// aimed at the objective. The group walks out from its landing zone, takes up its place in
			// the encirclement, and holds it until the muster window ends.
			//
			// ⚠ THE SLOT IS INDEX 0 OF THE FOURTH PARALLEL ARRAY, and it is popped below with the other
			// three. The fallback to targetPos only fires if the ring was never built, which would mean
			// the mode was set after Start() - the one configuration order this component forbids.
			vector slot = targetPos;
			if(m_aSpawnRingSlots.Count() > 0) slot = m_aSpawnRingSlots[0];

			// The same 5 s grace the standard ladder's first entry uses: a group needs a moment to
			// finish populating before it is worth giving it somewhere to be.
			ScheduleWaypoint(slot,5,aigroup,"Defend");
		}
		else
		{
			ScheduleWaypoint(targetPos,5,aigroup,"Scout");
			ScheduleWaypoint(targetPos,15,aigroup,"Scout");
			ScheduleWaypoint(targetPos,30,aigroup,"SearchAndDestroy");
			ScheduleWaypoint(targetPos,60,aigroup,"SearchAndDestroy");
		}

		m_aSpawnQueue.Remove(0);
		m_aSpawnPositions.Remove(0);
		m_aSpawnTargets.Remove(0);
		// ⚠ THE FOURTH ARRAY COMES OFF HERE TOO. Miss this and every later group is handed the slot of
		// the group before it - a ring that bunches tighter with every spawn, and no other symptom.
		// Guarded rather than unconditional because a standard battle never fills it.
		if(m_aSpawnRingSlots.Count() > 0) m_aSpawnRingSlots.Remove(0);
	}

	bool IsZeroVector(vector vec)
	{
    	return vec[0] == 0 && vec[1] == 0 && vec[2] == 0;
	}
	
	protected vector GetRandomDirection()
	{
		float angle = Math.RandomFloatInclusive(0, 359); // Random angle for azimuth
		if(m_iPreferredDirection > -1)
		{
			// Sample an offset around the preferred direction, then wrap — normalizing
			// min/max separately inverts the range when it straddles 0°/360°
			angle = m_iPreferredDirection + Math.RandomFloatInclusive(-m_iDirectionVariance, m_iDirectionVariance);
			if(angle < 0) angle += 360;
			if(angle >= 360) angle -= 360;
		}

		// In Arma: X = East, Z = South
		// For compass bearings: 0° = North, 90° = East, 180° = South, 270° = West
		// North = -Z, East = +X, South = +Z, West = -X
		// So we use sin for X and -cos for Z
		vector dir = {Math.Sin(angle * Math.DEG2RAD), 0, -Math.Cos(angle * Math.DEG2RAD)};
	    return dir.Normalized(); // Return a normalized direction vector
	}
	
	protected vector GetTargetZone(vector origin)
	{
	    origin = GetOwner().GetOrigin(); // Starting position
	    float searchRadius = 50.0; // Define the radius
	    int maxAttempts = 450; // Maximum attempts to find a valid position
	    int attempts = 0;
	    vector targetZone;
	    while (attempts < maxAttempts)
	    {
	        attempts++;
	
	        // Generate a random position within the radius
	        targetZone = s_AIRandomGenerator.GenerateRandomPointInRadius(0, searchRadius, origin);
	
	        // Check if the position is not in the ocean
	        if (!OVT_WorldUtils.IsOceanAtPosition(targetZone))
	        {
	            Print("[Debug] Found valid target zone: " + targetZone);
				m_vGoodTargetPos = targetZone;
	            return targetZone; // Return the valid position
	        }
	    }
		//Reuse any good qrf position if found
		if (!IsZeroVector(m_vGoodTargetPos)){return m_vGoodTargetPos;}
	    // If no valid position is found, return the original position as fallback
	    Print("[Debug] No valid target zone found. Returning origin as fallback.");
		//
	    return origin;
	}
	
	//! \param sourcePos Where the wave being placed is coming from - a held base, or the occupying
	//! faction's forward operating base. The ZERO VECTOR means "no known source", which falls back to
	//! the authored m_iPreferredDirection exactly as this method behaved before the source was passed.
	protected vector GetLandingZone(vector sourcePos)
	{
		// No caching here — SendWave calls this once per source base, and each wave
		// source is meant to get its own landing zone (BUG-031)
	    vector qrfpos = GetOwner().GetOrigin(); // Position of the QRF target (base being attacked)
	    Print("[Overthrow.QRFControllerComponent] QRF target position: " + qrfpos.ToString());

		// ⚠ THE BEARING IS TARGET -> SOURCE, NOT SOURCE -> TARGET, because `dir` below is used as
		// qrfpos + (dir * distance) and therefore points from the objective OUT TO the landing zone. A
		// wave has to land on the side of the objective its source is on; the inverse would put every
		// wave on the far side. OVT_QRFBearing's header carries the argument in full - it is the single
		// most likely defect in this change.
		bool fromSource = !IsZeroVector(sourcePos);
		int preferred = 0;
		if(fromSource)
		{
			preferred = OVT_QRFBearing.PreferredDegreesFromSource(sourcePos, qrfpos);
			Print("[Overthrow.QRFControllerComponent] Wave source " + sourcePos.ToString() + " lies on bearing " + preferred.ToString() + " from the target; landing zone biased there +/- " + m_iDirectionVariance.ToString());
		}

		// The authored path is untouched: with no source, GetRandomDirection() reads
		// m_iPreferredDirection and behaves exactly as it did before this feature.
		vector dir;
		if(fromSource)
			dir = OVT_QRFBearing.DirectionForDegrees(preferred + Math.RandomFloatInclusive(-m_iDirectionVariance, m_iDirectionVariance));
		else
			dir = GetRandomDirection(); // Get direction FROM which QRF should come
	    Print("[Overthrow.QRFControllerComponent] Direction vector: " + dir.ToString());

		float distance = Math.RandomFloatInclusive(m_iLZMin,m_iLZMax);
		Print("[Overthrow.QRFControllerComponent] Distance: " + distance.ToString());
	
	    vector checkpos = qrfpos + (dir * distance); 
		vector safepos = checkpos;
	
	    BaseWorld world = GetGame().GetWorld();
	
	    int maxAttempts = 450; // Maximum attempts to find a valid position
	    int attempts = 0;
		
	    while (attempts < maxAttempts)
	    {
	        attempts++;
				
	        // Ensure the position is not in the ocean
	        if (!OVT_WorldUtils.IsOceanAtPosition(checkpos))
	        {
				safepos = checkpos;
	            // Check for a clear landing zone (10x10x10)
	            vector mins = "-5 0 -5";
	            vector maxs = "5 10 5";
	            autoptr TraceBox trace = new TraceBox;
	            trace.Flags = TraceFlags.ENTS;
	            trace.Start = checkpos;
	            trace.Mins = mins;
	            trace.Maxs = maxs;
	            trace.Exclude = GetOwner();
	            float result = GetOwner().GetWorld().TracePosition(trace, null);
	            // TracePosition returns a negative value on overlap and sets TraceEnt to any
	            // blocking entity — the old '>= 0' accepted every candidate (BUG-031)
	            if (result > 0 && !trace.TraceEnt)
	            {
					Print("Found LZ: " + checkpos.ToString());
	                return checkpos;
	            }
	        }
	
	        // Randomize direction and try again — re-rolled the same way it was rolled the first time,
	        // so a source-derived search keeps searching the source's side of the objective rather than
	        // wandering onto the far side on its second attempt
	        if(fromSource)
	  		    dir = OVT_QRFBearing.DirectionForDegrees(preferred + Math.RandomFloatInclusive(-m_iDirectionVariance, m_iDirectionVariance));
	        else
	  		    dir = GetRandomDirection(); // Get a new random direction each time
			distance = Math.RandomFloatInclusive(m_iLZMin,m_iLZMax);
      		checkpos = qrfpos + (dir * distance); // Update check position
	    }	    
	    // Default to the last checked position if no better options were found
	    return safepos;
	}

		
}