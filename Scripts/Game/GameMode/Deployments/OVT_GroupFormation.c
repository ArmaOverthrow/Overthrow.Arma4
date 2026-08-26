//------------------------------------------------------------------------------------------------
//! WHAT SHAPE A GROUP WALKS IN.
//!
//! 🔴 WHY THIS EXISTS (author, 2026-08-26): a sabotage team marching from a forward base arrived
//! strung out - *"the squad leader runs far ahead and they dont travel as a group"* - and a team that
//! trickles onto an objective one man at a time loses to anything defending it.
//!
//! ⚠ THE WAYPOINT WAS NOT THE LEVER, AND CHANGING IT WOULD HAVE BEEN A TRAP. A march plan builds a
//! SINGLE move waypoint, so its EAIWaypointCompletionType decides only when that waypoint is marked
//! done - not how the men walk to it. Vanilla's AIWaypoint_Move.et ships `Any`; switching it to `All`
//! would not have closed the gap on the approach and WOULD have deadlocked the group whenever one man
//! never arrives - which the author reports is the common stuck case (a man left on the insertion
//! truck). Formation degrades gracefully instead: a straggler falls behind, and nothing stalls.
//!
//! ⚠ BOTH COMPONENTS, AND EVERY MOVE HANDLER. Copied from vanilla's own SCR_AISetGroupFormation
//! behaviour-tree node: AIFormationComponent takes one formation, AIGroupMovementComponent takes one
//! PER MOVE HANDLER and is walked until GetMoveHandlerAgentCount() answers -1. Setting only the
//! formation component leaves the movers on whatever they had.
//!
//! ⚠ THE NAME IS A STRING BY DESIGN. Both engine setters take the enum's NAME, not its value -
//! typename.EnumToString(SCR_EAIGroupFormation, ...) - which is exactly what vanilla does.
//------------------------------------------------------------------------------------------------
class OVT_GroupFormation
{
	//------------------------------------------------------------------------------------------------
	//! Puts a group into a travel formation.
	//!
	//! \param[in] group The group to shape. Null is a no-op.
	//! \param[in] formation An SCR_EAIGroupFormation value.
	//! \return True when at least one component accepted it.
	static bool Apply(SCR_AIGroup group, SCR_EAIGroupFormation formation)
	{
		if (!group)
			return false;

		string formationName = typename.EnumToString(SCR_EAIGroupFormation, formation);
		if (formationName.IsEmpty())
			return false;

		bool applied = false;

		AIFormationComponent formationComponent = group.GetFormationComponent();
		if (formationComponent && formationComponent.SetFormation(formationName))
			applied = true;

		AIGroupMovementComponent movement = AIGroupMovementComponent.Cast(group.FindComponent(AIGroupMovementComponent));
		if (movement)
		{
			// ⚠ EVERY HANDLER, NOT HANDLER 0. A group that has split - men in a vehicle and men on foot -
			// has more than one, and the ones left on the default would walk a different shape.
			int handlerId = 0;
			while (movement.GetMoveHandlerAgentCount(handlerId) != -1)
			{
				if (movement.SetFormationDefinition(handlerId, formationName))
					applied = true;

				handlerId++;
			}
		}

		return applied;
	}
}
