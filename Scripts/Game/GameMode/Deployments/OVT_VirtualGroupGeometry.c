//------------------------------------------------------------------------------------------------
//! WHERE A REGISTERED GROUP'S MEN ACTUALLY ARE - as opposed to where its record says they are.
//!
//! ==========================================================================================
//! 🔴 OVT_VirtualizationManagerComponent.GetPosition() IS NOT A POSITION FOR A GROUP THAT IS
//! WALKING, AND EVERY DISTANCE TEST IN THIS FRAMEWORK USED IT AS ONE.
//! ==========================================================================================
//! Core's GetPosition() resolves the group ENTITY and returns `group.GetOrigin()` whenever that
//! entity exists, falling back to the record only when it does not. That is exactly right for what
//! core needs it for - a dormant group IS its record, and virtual movement keeps the record current
//! through SetPosition(). It is exactly wrong for a MATERIALISED group, because an SCR_AIGroup is a
//! marker entity created at the registration position and it DOES NOT FOLLOW ITS MEMBERS. Core's own
//! SetPosition doc states the trap in one line: *"moving a group whose members exist teleports
//! nothing, it just relocates the record entity"* - the corollary being that reading it back gives
//! you the marker, not the men.
//!
//! ⚠ WHAT THAT COST (user play-test, 2026-08-21). A forward operating base is raised when its party
//! reaches the site on foot, gated at 80 m. The party is registered AT the site, so its marker sits
//! at the site, so the measured distance is ~0 forever - and the structure went up the instant the
//! transport stopped, with the men still a kilometre away walking. The user watched a FOB materialise
//! out of nothing after its team dismounted miles short. The same one-line mistake is in every
//! "is my force holding this place" test the deployment behaviours make.
//!
//! ⚠ AND THE OBVIOUS FIX IS ALSO WRONG, WHICH IS WHY THIS FILE EXISTS RATHER THAN AN AGENT LOOP AT
//! EACH CALL SITE. Counting or locating AGENTS answers "nobody is here" for a DORMANT or spawn-queued
//! group, which is perfectly alive and is the normal state of every group on a server with nobody
//! standing nearby - i.e. most servers, most of the time, and precisely when a forward base is
//! supposed to go up unobserved. Swapping the marker blind spot for the agent blind spot would turn
//! "raises too early, always" into "never raises unless a player is watching", which is worse.
//!
//! THE RULE, THEREFORE, IS THAT THE POSITION QUESTION IS ANSWERED DIFFERENTLY DEPENDING ON WHETHER
//! THE MEN EXIST, AND NOTHING ELSE CHANGES:
//!   MATERIALISED - ask the men. The marker and the record are both stale for a group that is
//!                  physically walking; only the bodies know where they are.
//!   DORMANT      - ask the record. It is the truth for an unspawned group and virtual movement
//!                  genuinely keeps it current (OVT_VirtualMovementManagerComponent calls SetPosition
//!                  as it advances a plan).
//! LIVENESS IS NOT PART OF THIS FILE'S JOB. Callers keep counting through the survivor mask exactly as
//! they did - that half was always right, and mixing the two questions is how the agent blind spot
//! gets reintroduced by somebody tidying up.
//------------------------------------------------------------------------------------------------
class OVT_VirtualGroupGeometry
{
	//! Below this, a reported centre of mass is treated as "the engine did not answer" rather than as
	//! the map origin. A group at <0,0,0> is not a real campaign position on any Overthrow world.
	static const float ORIGIN_EPSILON_M = 1;

	//------------------------------------------------------------------------------------------------
	//! WHERE THIS GROUP'S MEN ARE, honestly, whichever state it is in.
	//!
	//! THE FALLBACK CHAIN IS ORDERED BY HOW WELL EACH ANSWER DESCRIBES A SQUAD, not by convenience:
	//!   1. GetCenterOfMass() - the engine's own mean of the members. The right answer for "has the
	//!      party arrived": a point man fifty metres ahead of everyone else does not count as the
	//!      party arriving, and a straggler does not stop it.
	//!   2. GetLeaderEntity() - vanilla's own choice when it needs one point for a group, and it says
	//!      why in SCR_AIProcessFailedMovementResult:59: *"safer than obtaining position of the
	//!      group"*. Used when the engine gives no centre of mass.
	//!   3. the first materialised member, for a group whose leader slot is dead or unspawned.
	//!   4. core's GetPosition() - which for a group with no members IS the record, and is correct.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle A registered handle.
	//! \return A world position; vector.Zero only for a handle core does not know.
	static vector ResolveLivePosition(notnull OVT_VirtualizationManagerComponent virtualization, int handle)
	{
		SCR_AIGroup group = virtualization.GetGroup(handle);

		// No entity, or no men in it: the record is the truth and core already answers with it.
		if (!group || group.GetAgentsCount() == 0)
			return virtualization.GetPosition(handle);

		vector centre = group.GetCenterOfMass();
		if (centre.Length() > ORIGIN_EPSILON_M)
			return centre;

		IEntity leader = group.GetLeaderEntity();
		if (leader)
			return leader.GetOrigin();

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity member = agent.GetControlledEntity();
			if (member)
				return member.GetOrigin();
		}

		// Materialised by the agent count but nothing resolvable behind it - mid-teardown. The record
		// is stale but it is a real place, which beats vector.Zero.
		return virtualization.GetPosition(handle);
	}

	//------------------------------------------------------------------------------------------------
	//! The distance test every "is my force at this place" gate in the framework should be making.
	//! \param[in] virtualization The virtualization manager.
	//! \param[in] handle A registered handle.
	//! \param[in] centre The place being asked about.
	//! \param[in] radius How close counts, in metres. Non-positive is never inside.
	//! \return True when the group's men are within the radius.
	static bool IsGroupWithin(notnull OVT_VirtualizationManagerComponent virtualization, int handle, vector centre, float radius)
	{
		if (radius <= 0)
			return false;

		return vector.Distance(ResolveLivePosition(virtualization, handle), centre) <= radius;
	}
}
