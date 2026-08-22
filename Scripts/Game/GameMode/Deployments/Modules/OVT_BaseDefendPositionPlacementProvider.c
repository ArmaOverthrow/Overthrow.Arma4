//------------------------------------------------------------------------------------------------
//! Posts on the nearest base's authored DEFEND POSITIONS. Port of the legacy defense-position base
//! upgrade (deleted with this provider's config in virtualization/base-defense-migration Phase 4 -
//! line-exact citations to the original are preserved in that feature's implementation.md §3.3),
//! reading the list the base controller already builds at OVT_BaseControllerComponent.c:246-255.
//!
//! WHERE THE LIST COMES FROM, so nobody re-derives it here. FindSlots() sweeps baseRange around the
//! base marker and records the origin of every entity carrying an SCR_AISmartActionSentinelComponent.
//! Watchtowers are excluded from that sweep on purpose (`:239`) - they belong to the tower provider,
//! and a base that ran both would otherwise man each tower twice.
//!
//! NO HEADING IS ANSWERED. The list is positions only (the controller stores vectors, not
//! transforms), so occupants arrive on an identity rotation exactly as the legacy guards did. The
//! legacy class also gave each guard a DEFEND waypoint on the position; under virtualization that is
//! the CONFIG's job - Deployment_BaseDefensePositions.conf authors a DEFEND behaviour module and the
//! core builds the waypoints at registration.
//!
//! ⚠ THE `radius` ARGUMENT IS A REAL BOUND, NOT A FORMALITY. The base controller discovers these
//! within its own baseRange of the BASE MARKER, which is not the deployment's position: a deployment
//! created off-centre, or a marker that drifted, would otherwise be handed posts hundreds of metres
//! away and stand men at a base it is not part of. Both the base itself and every individual post are
//! range-checked against the deployment.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_BaseDefendPositionPlacementProvider : OVT_DeploymentPlacementProvider
{
	//------------------------------------------------------------------------------------------------
	//! Every defend position of the nearest base that lies within `radius` of the deployment.
	//! \param[in] deploymentPosition Centre of the search.
	//! \param[in] radius How far a post may be from the deployment.
	//! \param[in] factionIndex Unused - who holds the base is the config's business (the base-control
	//!            condition module), not the provider's.
	//! \return The posts, headingless. Empty when there is no base in range or it has none.
	override array<ref OVT_DeploymentPlacement> ResolvePlacements(vector deploymentPosition, float radius, int factionIndex)
	{
		array<ref OVT_DeploymentPlacement> placements = new array<ref OVT_DeploymentPlacement>();

		OVT_BaseControllerComponent controller = FindNearestBaseController(deploymentPosition, radius);
		if (!controller || !controller.m_aDefendPositions)
			return placements;

		foreach (vector post : controller.m_aDefendPositions)
		{
			if (vector.Distance(post, deploymentPosition) > radius)
				continue;

			placements.Insert(new OVT_DeploymentPlacement(post, vector.Zero));
		}

		return placements;
	}

	//------------------------------------------------------------------------------------------------
	override string GetProviderName()
	{
		return "base defend positions";
	}

	//------------------------------------------------------------------------------------------------
	//! The base controller nearest to a position, if it is close enough to be the deployment's own.
	//!
	//! Every dereference is guarded: this is legal to call on a config template in a world with no
	//! occupying faction manager, and "no base" is an ordinary answer rather than an error.
	//! \param[in] position The deployment position.
	//! \param[in] radius How far the base marker may be.
	//! \return The controller, or null.
	protected OVT_BaseControllerComponent FindNearestBaseController(vector position, float radius)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return null;

		OVT_BaseData nearest = occupying.GetNearestBase(position);
		if (!nearest)
			return null;

		if (vector.Distance(nearest.location, position) > radius)
			return null;

		IEntity marker = GetGame().GetWorld().FindEntityByID(nearest.entId);
		if (!marker)
			return null;

		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}
}
