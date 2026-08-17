//------------------------------------------------------------------------------------------------
//! Posts on every curated sniper marker (an entity carrying OVT_SniperPositionComponent) near the
//! deployment. Port of the legacy sniper-position base upgrade's marker query and placement (that
//! class was deleted with this provider's config in virtualization/base-defense-migration Phase 4;
//! line-exact citations to the original are preserved in that feature's implementation.md §3.3).
//!
//! ROTATION IS PRESERVED, AND THAT IS THE POINT OF A CURATED MARKER. A level designer places one of
//! these facing the approach it covers; the team is teleported onto the marker's own world transform
//! so the sniper looks down that line from the first frame instead of spinning to whatever heading
//! the engine happened to spawn him with. This is the one shipped provider that answers a heading -
//! the tower and defend-position providers deliberately answer none.
//!
//! ================== THE PER-MARKER THREAT GATE IS PART OF THE PROVIDER ======================
//! Each marker authors its own OVT_SniperPositionComponent.m_iMinimumThreat and is skipped while the
//! occupying faction's threat is below it - carried verbatim from the legacy upgrade's own gate. That
//! is a PER-POSITION escalation and the deployment framework has no equivalent: a config's
//! m_iMinimumThreatLevel gates the whole deployment, all-or-nothing. Filtering here is what keeps the
//! designers' authored progression - the exposed forward positions only get manned once the campaign
//! is hot - and it re-evaluates on every convergence, so a base grows sniper teams as threat rises
//! without anything else changing.
//!
//! A world with no occupying faction manager reads threat as 0, which mans only the markers authored
//! with m_iMinimumThreat 0. That is the safe direction to fail: too few snipers, never too many.
//! ===========================================================================================
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_SniperMarkerPlacementProvider : OVT_DeploymentPlacementProvider
{
	//! Markers found by the in-flight query. Rebuilt at the start of every resolve; never cached
	//! across calls, because a marker can be deleted with the composition it stood on.
	protected ref array<IEntity> m_aFoundMarkers;

	//------------------------------------------------------------------------------------------------
	//! One post per sniper marker within `radius` whose own threat gate the campaign has passed.
	//! \param[in] deploymentPosition Centre of the search.
	//! \param[in] radius How far to look for markers.
	//! \param[in] factionIndex Unused - a curated marker belongs to whoever holds the base.
	//! \return The marker transforms, rotation included. Empty when nothing qualifies.
	override array<ref OVT_DeploymentPlacement> ResolvePlacements(vector deploymentPosition, float radius, int factionIndex)
	{
		array<ref OVT_DeploymentPlacement> placements = new array<ref OVT_DeploymentPlacement>();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return placements;

		float threat = GetOccupyingThreat();

		m_aFoundMarkers = new array<IEntity>();
		world.QueryEntitiesBySphere(deploymentPosition, radius, AddMarker, FilterMarker, EQueryEntitiesFlags.ALL);

		foreach (IEntity marker : m_aFoundMarkers)
		{
			if (!marker)
				continue;

			OVT_SniperPositionComponent position = OVT_SniperPositionComponent.Cast(marker.FindComponent(OVT_SniperPositionComponent));
			if (!position)
				continue;

			if (threat < position.m_iMinimumThreat)
				continue;

			placements.Insert(new OVT_DeploymentPlacement(marker.GetOrigin(), marker.GetAngles()));
		}

		m_aFoundMarkers = null;

		return placements;
	}

	//------------------------------------------------------------------------------------------------
	override string GetProviderName()
	{
		return "sniper markers";
	}

	//------------------------------------------------------------------------------------------------
	//! The occupying faction's current threat, or 0 when there is no manager to ask.
	//!
	//! Guarded rather than assumed: a provider is legal to call off a config template in a world with
	//! no campaign running at all, and 0 mans exactly the always-on markers.
	//! \return The threat level.
	protected float GetOccupyingThreat()
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return 0;

		return occupying.m_iThreat;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from the sphere query.
	//! \return True when it is a curated sniper marker.
	protected bool FilterMarker(IEntity entity)
	{
		if (!entity)
			return false;

		return entity.FindComponent(OVT_SniperPositionComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A marker that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddMarker(IEntity entity)
	{
		m_aFoundMarkers.Insert(entity);
		return true;
	}
}
