[BaseContainerProps(configRoot: true)]
class OVT_BaseBehaviorDeploymentModule : OVT_BaseDeploymentModule
{
	//------------------------------------------------------------------------------------------------
	//! The waypoint plan this behavior wants a group registered with, or null for "I have no opinion".
	//!
	//! ASKED BEFORE THE GROUP EXISTS. The virtualization core builds a group's waypoints from its plan
	//! at registration and owns them from then on, so a behavior that wants to shape where a group
	//! goes has to say so up front rather than bolting waypoints on after it spawns. Null is the
	//! honest answer for a behavior that is about something else entirely (reinforcement, capture),
	//! and the spawning module simply asks the next one.
	//! \param[in] groupPosition Where the group is about to be registered. A plan may be built around
	//!            it - a perimeter patrol starts on the group's own bearing to its centre.
	//! \return The plan, or null.
	OVT_VirtualWaypointPlan BuildVirtualPlan(vector groupPosition)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Common behavior functionality
	//------------------------------------------------------------------------------------------------
	protected array<SCR_AIGroup> GetManagedGroups()
	{
		array<SCR_AIGroup> groups = new array<SCR_AIGroup>;
		
		if (!m_ParentDeployment)
			return groups;
			
		// Get all spawning modules and extract their AI groups
		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			array<IEntity> entities = spawningModule.GetSpawnedEntities();
			foreach (IEntity entity : entities)
			{
				// Check if entity is an AI group or has AI group component
				SCR_AIGroup group = SCR_AIGroup.Cast(entity);
				if (group)
				{
					groups.Insert(group);
					continue;
				}
				
				// Check if entity is part of an AI group
				AIAgent agent = AIAgent.Cast(entity.FindComponent(AIAgent));
				if (agent && agent.GetParentGroup())
				{
					SCR_AIGroup parentGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
					if (parentGroup && !groups.Contains(parentGroup))
						groups.Insert(parentGroup);
				}
			}
		}
		
		return groups;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ApplyBehaviorToGroup(SCR_AIGroup group)
	{
		// Override in derived classes to implement specific behavior
	}
	
	//------------------------------------------------------------------------------------------------
	protected void RemoveBehaviorFromGroup(SCR_AIGroup group)
	{
		// Override in derived classes to clean up behavior
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnActivate()
	{
		super.OnActivate();
		
		// Apply behavior to all current groups
		array<SCR_AIGroup> groups = GetManagedGroups();
		foreach (SCR_AIGroup group : groups)
		{
			ApplyBehaviorToGroup(group);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDeactivate()
	{
		super.OnDeactivate();
		
		// Remove behavior from all groups
		array<SCR_AIGroup> groups = GetManagedGroups();
		foreach (SCR_AIGroup group : groups)
		{
			RemoveBehaviorFromGroup(group);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);
		
		// A mission that has finished waits for the coast to clear before it leaves. See TickExfiltration.
		TickExfiltration();
		
		// Reapply behavior to any new groups that may have spawned
		array<SCR_AIGroup> groups = GetManagedGroups();
		foreach (SCR_AIGroup group : groups)
		{
			if (!HasBehaviorApplied(group))
				ApplyBehaviorToGroup(group);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Utility methods for AI group management
	//------------------------------------------------------------------------------------------------
	protected bool HasBehaviorApplied(SCR_AIGroup group)
	{
		// Override in derived classes to track which groups have behavior applied
		// Default implementation assumes behavior needs to be reapplied
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	// NO WAYPOINT HELPERS LIVE HERE ANY MORE.
	//
	// There used to be four - one that stripped a group's waypoints and three that built new ones - and
	// all four had zero callers by the time the virtualization core took ownership of waypoints. A
	// behavior module says what plan a group should be REGISTERED with (BuildVirtualPlan above); the
	// core turns that into waypoint entities, records them, and deletes them again on unregister. A
	// consumer that creates or removes a waypoint on a registered group corrupts the record the core
	// persists, so the helpers that made that easy to do are gone rather than merely unused.
	//------------------------------------------------------------------------------------------------
	
	//------------------------------------------------------------------------------------------------
	// "IS MY FORCE THERE, AND IS ANYBODY ELSE?" - the two questions a hold-a-place behavior asks
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! How many living members of this deployment's force are standing inside a circle.
	//!
	//! ⚠ IT COUNTS THROUGH THE VIRTUALIZATION RECORD, NEVER THROUGH AGENTS, and that is not an
	//! optimisation. Under Reforger 1.8 a perfectly alive DORMANT or spawn-queued group reports ZERO
	//! agents, so a behavior that counted bodies would decide its own force was dead every time the
	//! last player drove away - the exact mistake OVT_RadioTowerCaptureBehaviorDeploymentModule's
	//! header documents removing from the tower loop. GetAliveMemberCount() is answered off the survivor
	//! mask, which is true dormant or spawned, and that half of this method is untouched.
	//!
	//! ==========================================================================================
	//! 🔴 THE POSITION HALF WAS NOT TRUE BOTH WAYS, AND THE COMMENT ABOVE USED TO CLAIM IT WAS.
	//! ==========================================================================================
	//! It said "GetPosition() off the record". It is not: core resolves the group ENTITY and returns
	//! its origin whenever that entity exists, and an SCR_AIGroup is a MARKER created at the
	//! registration position that never follows its members. So for any force that is materialised and
	//! walking - which is every force a player is near, i.e. every force whose hold radius anybody ever
	//! observes - this measured where the men were REGISTERED, not where they are. A deployment whose
	//! force is registered at its own objective therefore passed its hold test from the first tick,
	//! forever, whatever the men were doing. The same one-line mistake raised forward bases with their
	//! parties a kilometre away on 2026-08-21; see OVT_VirtualGroupGeometry, which is where the
	//! materialised/dormant split now lives.
	//!
	//! WHOLE GROUPS, NOT INDIVIDUALS. One position per group, so a group counts its living members if
	//! the GROUP is inside the circle. For a hold radius measured in tens of metres against a squad that
	//! moves as one, that is the same answer - and it is now measured at the squad's centre of mass
	//! rather than at a marker, so "moves as one" is finally what is being asked about.
	//! \param[in] centre Where the circle is.
	//! \param[in] radius Its radius in metres. Non-positive counts nothing.
	//! \return Living members of this deployment's registered force inside the circle.
	protected int CountAliveRegisteredMembersWithin(vector centre, float radius)
	{
		if (radius <= 0)
			return 0;

		if (!m_ParentDeployment)
			return 0;

		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		if (!virtualization)
			return 0;

		array<int> handles = new array<int>();

		array<OVT_BaseSpawningDeploymentModule> spawningModules = m_ParentDeployment.GetSpawningModules();
		foreach (OVT_BaseSpawningDeploymentModule spawningModule : spawningModules)
		{
			if (spawningModule)
				spawningModule.CollectRegisteredHandles(handles);
		}

		int alive = 0;
		foreach (int handle : handles)
		{
			if (!virtualization.IsRegistered(handle))
				continue;

			int members = virtualization.GetAliveMemberCount(handle);
			if (members < 1)
				continue;

			if (!OVT_VirtualGroupGeometry.IsGroupWithin(virtualization, handle, centre, radius))
				continue;

			alive = alive + members;
		}

		return alive;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance to the nearest connected player.
	//!
	//! NOBODY CONNECTED ANSWERS float.MAX, matching OVT_BaseConditionDeploymentModule.GetPlayerProximity()
	//! - so "is an enemy standing on this place" reads false on an empty server rather than true, and a
	//! hold timer runs on a dedicated server nobody has joined yet.
	//!
	//! ⚠ PLAYERS ONLY, DELIBERATELY. "Armed resistance is present" could be asked of every AI agent in
	//! the world, and it would cost a sphere query on every behavior module of every deployment on the
	//! map every ten seconds. A player is what the ramp is being read by and interrupted by; a resistance
	//! patrol walking past is not what the design means by contested.
	//! \param[in] position The place to measure from.
	//! \return Metres to the nearest player, or float.MAX when nobody is connected.
	protected float NearestPlayerDistance(vector position)
	{
		float nearest = float.MAX;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return nearest;

		array<int> players = new array<int>();
		playerManager.GetPlayers(players);

		foreach (int playerId : players)
		{
			IEntity player = playerManager.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance < nearest)
				nearest = distance;
		}

		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	// COLLECTING THE DEPLOYMENT FROM A BEHAVIOR THAT HAS FINISHED ITS JOB
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Asks for this deployment to be taken down, NEXT FRAME.
	//!
	//! ⚠ THE ONE-FRAME DEFERRAL IS THE ENTIRE POINT AND IT IS NOT DEFENSIVE PADDING. DestroyDeployment()
	//! clears m_aActiveModules and then `delete GetOwner()`, and UpdateDeployment() is in the middle of
	//! a `foreach` over a local list of behavior modules when this is called - a list of WEAK references
	//! into the objects that were just thrown away. OVT_ReinforcementBehaviorDeploymentModule gets away
	//! with deleting inline only because every config authors it LAST among the behavior modules, which
	//! is a rule stated in three config headers and enforced by nothing. A behavior that finishes its
	//! mission is not last and cannot be: it has to run before the reinforcement module, or the
	//! reinforcement module rebuys the force in the same pass that decided the mission was over.
	//!
	//! ==========================================================================================
	//! 🔴 A MISSION THAT HAS DONE ITS JOB EXFILTRATES - IT DOES NOT STAND AT THE OBJECTIVE FOREVER,
	//! AND IT DOES NOT VANISH WHERE ANYBODY CAN SEE IT (author, 2026-08-21).
	//! ==========================================================================================
	//! *"If a mission is successful AND there's no players around, the team is collected and refunded.
	//! You don't leave them there. If they blow up something irl they are gonna get out of there. The
	//! player should return to see their stuff in flames and no-one around."*
	//!
	//! TWO CONDITIONS, AND THE SECOND ONE IS WHAT MAKES THE FIRST SAFE:
	//!   1. THE MISSION SUCCEEDED - which is what reaching this method means. Only a behaviour module
	//!      that has finished its own job calls it; there are three, and all three are MISSIONS (a base
	//!      repair detail, a base sabotage, a town harassment hold). Garrisons, patrols, tower guards
	//!      and base defences never come through here at all - they are meant to persist, and the one
	//!      other route to CollectDeployment (the reinforcement module's failed-conditions check) is
	//!      deliberately left alone. That is what keeps this rule out of the shared framework.
	//!   2. NOBODY IS WATCHING. A squad may not evaporate in front of a player, ever.
	//!
	//! ⚠ IT IS A POLL AND NOT A ONE-SHOT, AND THAT IS THE WHOLE DESIGN. A single check at completion
	//! time would strand the team permanently in exactly the case that matters most - the player who
	//! fought them and is standing there when the job finishes. Arming a latch and re-asking on every
	//! update means the team leaves the moment he does, and the player comes back to wreckage and an
	//! empty objective, which is the intended experience.
	//!
	//! ⚠ THERE IS NO DEADLINE, DELIBERATELY. A player who camps the objective keeps the team there, and
	//! that is correct rather than a hang: the alternative is men disappearing under observation, which
	//! is the one outcome the rule exists to forbid. The force is idle at the objective while it waits,
	//! which is precisely what it did before this method existed at all, so the waiting state costs
	//! nothing that was not already being paid.
	//!
	//! IDEMPOTENT. Called once per module instance; a second call while one is armed is ignored.
	//! \param[in] reason What finished, for the log line. Empty logs nothing.
	protected void RequestDeploymentCollection(string reason)
	{
		if (m_bCollectionRequested)
			return;

		m_bCollectionRequested = true;
		m_bExfilHoldLogged = false;

		if (reason != "")
			Print(string.Format("[Overthrow] Deployment collection requested: %1", reason), LogLevel.NORMAL);

		// Try immediately - the common case is a mission that finished with nobody near it, and making
		// that wait a whole update tick would be a needless delay.
		TickExfiltration();
	}

	//------------------------------------------------------------------------------------------------
	//! One look at whether an armed mission may leave yet.
	//!
	//! ⚠ THE RADIUS IS difficulty.baseCloseRange AND IT IS NOT A NEW NUMBER. 220 m is what this campaign
	//! already means by "a player is here" - OVT_AssetStarvedObjectiveAbort.IsPlayerAtAsset asks exactly
	//! this question with exactly this knob, through exactly this helper, and its own header records the
	//! rule that forbids inventing an objective-specific one. It is a PRESENCE test, not a line-of-sight
	//! test: a player behind a wall 100 m away has not seen anything, but he is close enough that the
	//! squad walking out of existence would be noticed one way or another, and presence is the thing the
	//! rest of the campaign can actually agree on.
	//!
	//! ⚠ MEASURED FROM THE DEPLOYMENT, NOT FROM THE GROUPS. A mission's men are spread over the
	//! objective and some of them may be a couple of hundred metres out on a flank; asking about the
	//! deployment position asks about the PLACE the player would be standing in, which is the question,
	//! and it cannot be gamed by one man having wandered off.
	protected void TickExfiltration()
	{
		if (!m_bCollectionRequested)
			return;

		if (!m_ParentDeployment)
			return;

		if (IsPlayerWatchingDeployment())
		{
			LogExfilHold();
			return;
		}

		// ⚠ THE ONE-FRAME DEFERRAL SURVIVES THE CHANGE AND MUST. Reached from OnUpdate, this is in the
		// middle of the deployment's own walk over its module list; DestroyDeployment() clears that list
		// and deletes the owning entity, so collecting inline would pull the ground out from under the
		// loop that called us. See CollectParentDeployment.
		GetGame().GetCallqueue().Remove(CollectParentDeployment);
		GetGame().GetCallqueue().CallLater(CollectParentDeployment, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when somebody is close enough that the team must not disappear.
	protected bool IsPlayerWatchingDeployment()
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		return OVT_WorldUtils.PlayerInRange(m_ParentDeployment.GetPosition(), difficulty.baseCloseRange);
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE that a finished mission is being kept in place because somebody is standing in it.
	//! Silent afterwards: this is polled on every update and a line per tick would bury the log.
	protected void LogExfilHold()
	{
		if (m_bExfilHoldLogged)
			return;

		m_bExfilHoldLogged = true;

		Print(string.Format("[Overthrow] Deployment '%1' has finished its job but a player is at the objective - the team holds until the coast is clear rather than vanishing in front of him",
			m_ParentDeployment.GetDeploymentName()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the deployment down, one frame after the behavior asked. Runs outside any module walk.
	//!
	//! ⚠ CollectDeployment, NOT DeleteDeployment (2026-08-20). Everything that reaches here has SUCCEEDED
	//! - it is the behaviour module's own "my job is done" - so the groups that are still at full
	//! strength are paid back for. A delete here wrote off men who had walked in, done the job and met no
	//! resistance at all, which is what the author raised during play-test. See CollectDeployment for
	//! which parts of the price come back and which do not.
	protected void CollectParentDeployment()
	{
		// ⚠ THE LATCH IS DROPPED ONLY WHEN THE COLLECTION ACTUALLY HAPPENS, and that changed with the
		// exfiltration rule. It used to be cleared unconditionally on the first line, which was harmless
		// when this ran one frame after the request and could not realistically fail - but now the latch
		// also means "this mission is waiting to leave", and clearing it on a transient null would
		// disarm the exfiltration for good and strand the team at the objective for the rest of the
		// campaign.
		if (!m_ParentDeployment)
			return;

		OVT_DeploymentManagerComponent manager = OVT_Global.GetDeploymentManager();
		if (!manager)
			return;

		m_bCollectionRequested = false;

		manager.CollectDeployment(m_ParentDeployment);
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE QUEUED COLLECTION MUST BE CANCELLED HERE. A deployment torn down by anything else - the
	//! objective director's reset, the reinforcement module's condition check, a Game Master - runs
	//! Cleanup() on every module, and a call still sitting in the queue would then fire into a module
	//! whose parent has already gone.
	override protected void OnCleanup()
	{
		GetGame().GetCallqueue().Remove(CollectParentDeployment);
		m_bCollectionRequested = false;

		super.OnCleanup();
	}

	//! True once a mission behaviour has declared its job done. It stays true while the team waits for
	//! the coast to clear, so this is now "an exfiltration is armed" rather than "a call is queued".
	protected bool m_bCollectionRequested;

	//! One hold message per armed exfiltration, not one per update tick.
	protected bool m_bExfilHoldLogged;

	//------------------------------------------------------------------------------------------------
	// Utility methods for position finding
	//------------------------------------------------------------------------------------------------
	protected vector GetRandomPositionInRadius(vector center, float radius)
	{
		float angle = Math.RandomFloat01() * Math.PI2;
		float distance = Math.RandomFloat(0, radius);
		
		vector offset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		return center + offset;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector GetPositionOnCircle(vector center, float radius, float angle)
	{
		vector offset = Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
		return center + offset;
	}
	
	//------------------------------------------------------------------------------------------------
	protected array<vector> GeneratePerimeterPositions(vector center, float radius, int count)
	{
		array<vector> positions = new array<vector>;
		
		for (int i = 0; i < count; i++)
		{
			float angle = (i / (float)count) * Math.PI2;
			vector position = GetPositionOnCircle(center, radius, angle);
			positions.Insert(position);
		}
		
		return positions;
	}
	
	//------------------------------------------------------------------------------------------------
	protected vector FindNearestDefendPosition(vector fromPosition)
	{
		if (!m_ParentDeployment)
			return fromPosition;
			
		// Look for nearby defend slots (like in the base upgrade system)
		float nearestDistance = float.MAX;
		vector nearestPosition = fromPosition;
		
		// Query for nearby sentinel components (guard positions)
		array<IEntity> entities = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(fromPosition, 200, null, FilterDefendEntities, EQueryEntitiesFlags.ALL);
		
		return nearestPosition;
	}
	
	//------------------------------------------------------------------------------------------------
	protected bool FilterDefendEntities(IEntity entity)
	{
		// Look for entities with sentinel components (guard positions)
		SCR_AISmartActionSentinelComponent sentinel = SCR_AISmartActionSentinelComponent.Cast(entity.FindComponent(SCR_AISmartActionSentinelComponent));
		return sentinel != null;
	}
}