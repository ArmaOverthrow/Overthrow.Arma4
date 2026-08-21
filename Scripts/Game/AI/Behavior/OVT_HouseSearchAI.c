//------------------------------------------------------------------------------------------------
//! The RELAXED house search: vanilla's Search & Destroy machinery with the threat posture taken out.
//!
//! WHY THIS EXISTS. A deployment SEARCH waypoint (the town sweep) first shipped on vanilla's
//! AIWaypoint_SearchAndDestroy, and the men did exactly what was wanted - walked up, went inside, poked
//! the rooms, came out - but as soldiers under threat: crouched, weapons up, sprinting spot to spot.
//! Two vanilla choices cause that and neither is reachable from a flag:
//!   1. SCR_AISearchAndDestroyActivity.AssignInvestigationPositions() sends every soldier an Investigate
//!      with dangerous = TRUE, hard-coded, and SCR_AIMoveAndInvestigateBehavior then pins m_fThreat above
//!      VIGILANT_THRESHOLD for the whole action ("so we are not surprised by the enemy again").
//!   2. AI/BehaviorTrees/Chimera/Soldier/MoveAndInvestigate.bt UNCONDITIONALLY sets stance CROUCH, speed
//!      RUN and weapon raised near its root, before any threat test.
//! So the fix is one small class per layer, each reusing everything else from its vanilla parent:
//!   - OVT_AIStartHouseSearch  (waypoint tree node)  - adds OVT_AIHouseSearchActivity to the group directly,
//!     the way vanilla's goal reaction would, without needing a new goal-message type (those are indexed by
//!     an enum and registered per prefab on Group_Base.et, which we do not edit).
//!   - OVT_AIHouseSearchActivity (group activity)     - vanilla's grid, tile loading, fireteam assignment and
//!     holding time; only the per-soldier hand-out is overridden, to give OVT_AIHouseSearchBehavior.
//!   - OVT_AIHouseSearchBehavior (soldier behaviour)  - vanilla's investigate behaviour with dangerous=false
//!     and our own tree, AI/BehaviorTrees/Overthrow/Soldier/HouseSearch.bt: MoveAndInvestigate.bt with
//!     stance STAND, speed WALK and the weapon lowered. Everything threat-GATED in that tree is untouched,
//!     so a patrol that actually meets something still raises its weapon and reacts as vanilla would.
//! The waypoint prefab is Prefabs/AI/Waypoints/OVT_AIWaypoint_HouseSearch.et (a child of the S&D prefab
//! pointing at AI/BehaviorTrees/Overthrow/Waypoints/WP_HouseSearch.bt, which is WP_SearchAndDestroy.bt
//! with the goal-message node swapped for OVT_AIStartHouseSearch).
//!
//! ⚠ THE TWO .bt FILES ARE HAND-AUTHORED TEXT COPIES of the vanilla trees with three nodes changed. Their
//! GUIDs are hand-minted (A086847134FE94FF waypoint, 7ABD3B8D152B6DBA soldier). If either fails to load
//! the symptom is a group that reaches the house and stands still until the hold expires - open both in
//! Workbench's behaviour-tree editor once and resave if that is ever seen.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Per-soldier: walk to a spot in or around the house and have a look, unhurried.
//!
//! Same ports as the parent (SCR_AIGetInvestigateBehaviorParameters / SCR_AISetInvestigateBehaviorParameters
//! read them by name off a template instance), so the copied tree's parameter nodes keep working. The
//! only differences are the tree and that dangerous defaults to false - the parent's constructor already
//! leaves m_fThreat alone when it is, which is the whole posture change at this layer.
//------------------------------------------------------------------------------------------------
class OVT_AIHouseSearchBehavior : SCR_AIMoveAndInvestigateBehavior
{
	//! The relaxed soldier tree. Path carries the hand-minted GUID of AI/BehaviorTrees/Overthrow/Soldier/HouseSearch.bt.
	static const ResourceName HOUSE_SEARCH_TREE = "{7ABD3B8D152B6DBA}AI/BehaviorTrees/Overthrow/Soldier/HouseSearch.bt";

	//------------------------------------------------------------------------------------------------
	void OVT_AIHouseSearchBehavior(SCR_AIUtilityComponent utility, SCR_AIActivityBase groupActivity, vector pos, float priority = PRIORITY_BEHAVIOR_MOVE_AND_INVESTIGATE, float priorityLevel = PRIORITY_LEVEL_NORMAL, float radius = 10, bool isDangerous = false, EAIUnitType targetUnitType = EAIUnitType.UnitType_Infantry, float duration = 10.0)
	{
		m_sBehaviorTree = HOUSE_SEARCH_TREE;
	}
}

//------------------------------------------------------------------------------------------------
//! Group side: vanilla's Search & Destroy activity, handing out OVT_AIHouseSearchBehavior instead.
//!
//! The parent owns the grid over the waypoint's completion radius, the navmesh tile loading, the
//! fireteam bookkeeping, OnChildBehaviorFinished (which casts to SCR_AIMoveAndInvestigateBehavior - our
//! subclass passes - and hands the next spot out) and the holding-time failure. Only the hand-out is
//! replaced: the parent broadcasts an Investigate MESSAGE whose vanilla reaction builds the vanilla
//! behaviour; this builds ours and adds it to the soldier's utility directly, which is exactly what the
//! reaction would have done with it.
//------------------------------------------------------------------------------------------------
class OVT_AIHouseSearchActivity : SCR_AISearchAndDestroyActivity
{
	//! How long one soldier lingers at one spot once there, in seconds (the parent's is 10). Longer reads
	//! as "having a look round the room" rather than "clearing it"; the parent jitters it ±20 %.
	static const float SPOT_DWELL_SECONDS = 15;

	//------------------------------------------------------------------------------------------------
	void OVT_AIHouseSearchActivity(SCR_AIGroupUtilityComponent utility, AIWaypoint relatedWaypoint, vector pos, IEntity ent, EMovementType movementType = EMovementType.WALK, bool useVehicles = false, float priority = PRIORITY_ACTIVITY_SEEK_AND_DESTROY, float priorityLevel = PRIORITY_LEVEL_NORMAL)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the next available spot to every member of the fireteam as a relaxed house-search behaviour.
	//!
	//! Mirrors the parent's bookkeeping exactly (assigned list, available list, refill when drained) so
	//! OnChildBehaviorFinished keeps recognising the positions it hands back.
	//! \param[in] ft The fireteam to task.
	override protected void AssignInvestigationPositions(SCR_AIGroupFireteam ft)
	{
		if (!ft || !m_aAvailablePositions || m_aAvailablePositions.IsEmpty())
			return;

		vector spot = m_aAvailablePositions[0];

		array<AIAgent> agents = {};
		ft.GetMembers(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
			if (!utility)
				continue;

			// The same "cancel previous investigations" the vanilla reaction performs.
			utility.SetStateAllActionsOfType(SCR_AIMoveAndInvestigateBehavior, EAIActionState.FAILED);

			OVT_AIHouseSearchBehavior behavior = new OVT_AIHouseSearchBehavior(utility, this, spot,
				SCR_AIActionBase.PRIORITY_BEHAVIOR_MOVE_AND_INVESTIGATE, m_fPriorityLevel.m_Value,
				radius: m_fBehaviorRadius, isDangerous: false, targetUnitType: EAIUnitType.UnitType_Infantry, duration: SPOT_DWELL_SECONDS);
			utility.AddAction(behavior);
		}

		m_aAssignedPositions.Insert(spot);
		m_aAvailablePositions.Remove(0);

		if (!m_aAvailablePositions.IsIndexValid(0))
			m_aAvailablePositions.Copy(m_aCorrectedPositions);
	}
}

//------------------------------------------------------------------------------------------------
//! Waypoint-tree node: starts the relaxed house search on the group running the tree.
//!
//! Sits where WP_SearchAndDestroy.bt has "SCR_AI Send Goal Message_ Seek And Destroy". That node
//! broadcasts a SeekAndDestroy goal message to the group itself, whose registered reaction
//! (SCR_AIGoalReaction_SeekAndDestroy) fails any running move activity and adds the vanilla S&D
//! activity. A new MESSAGE would need a new goal-message type and a reaction registered on every group
//! prefab, so this node does the reaction's two lines itself with our activity instead.
//! Extends SCR_AISendMessageGenerated only for its protected helpers (related waypoint); it sends nothing.
//------------------------------------------------------------------------------------------------
class OVT_AIStartHouseSearch : SCR_AISendMessageGenerated
{
	[Attribute("")]
	float m_fPriorityLevel;

	protected static ref TStringArray s_aVarsIn = {
		"PriorityLevel"
	};
	override TStringArray GetVariablesIn() { return s_aVarsIn; }

	//------------------------------------------------------------------------------------------------
	override ENodeResult EOnTaskSimulate(AIAgent owner, float dt)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(owner);
		if (!group)
		{
			SCR_AgentMustBeAIGroup(this, owner);
			return ENodeResult.FAIL;
		}

		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!utility)
			return ENodeResult.FAIL;

		AIWaypoint waypoint = GetRelatedWaypoint(owner);
		if (!waypoint)
			return ENodeResult.FAIL;

		float priorityLevel;
		if (!GetVariableIn("PriorityLevel", priorityLevel))
			priorityLevel = m_fPriorityLevel;

		OVT_AIHouseSearchActivity activity = new OVT_AIHouseSearchActivity(utility, waypoint, waypoint.GetOrigin(), null, priorityLevel: priorityLevel);

		utility.SetStateAllActionsOfType(SCR_AIMoveActivity, EAIActionState.FAILED);
		utility.AddAction(activity);

		return ENodeResult.SUCCESS;
	}

	//------------------------------------------------------------------------------------------------
	static override bool VisibleInPalette() { return true; }

	//------------------------------------------------------------------------------------------------
	protected static override string GetOnHoverDescription()
	{
		return "Overthrow: starts the relaxed house search (OVT_AIHouseSearchActivity) on this group for the current waypoint";
	}

	//------------------------------------------------------------------------------------------------
	override string GetNodeMiddleText()
	{
		return string.Format("m_fPriorityLevel: %1\n", m_fPriorityLevel);
	}
}
