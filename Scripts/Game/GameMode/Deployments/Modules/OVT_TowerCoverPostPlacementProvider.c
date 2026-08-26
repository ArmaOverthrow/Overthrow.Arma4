//------------------------------------------------------------------------------------------------
//! Posts on the OPEN-AIR WALKWAY of every watchtower near the deployment. Port of the legacy tower-
//! guard base upgrade's tower query, post arithmetic and sentinel pick (that class was deleted with
//! this provider's config in virtualization/base-defense-migration Phase 4; line-exact citations to
//! the original are preserved in that feature's implementation.md §3.3).
//!
//! ================== WHY THE WALKWAY AND NOT THE AUTHORED COVER POST ======================
//! Carried verbatim from the legacy class's own comment, because it is the entire reason the
//! offset exists and a future reader WILL try to "fix" it back to the sentinel's own position:
//!
//!   ActionOffset is in the tower's local space (see vanilla SCR_AIGetSmartActionSentinelParams).
//!   The guard stands on the open-air walkway ringing the cabin, NOT on the authored CoverPost
//!   inside it: the cabin's window glass blinds AI perception traces (the wanted system's LOS trace
//!   included) and obstructs the muzzle, so a guard in the cabin never sees or engages.
//!
//! WALKWAY_OFFSET is tower-LOCAL and applied before the tower's rotation, exactly as legacy did:
//!   post = towerTransform[3] + (sentinel.GetActionOffset() + WALKWAY_OFFSET).Multiply3(towerTransform)
//! =========================================================================================
//!
//! NO HEADING IS ANSWERED (angles are vector.Zero, i.e. an identity rotation), which is what the
//! legacy teleport used - it built Math3D.MatrixIdentity4 and set only the translation. A CoverPost
//! sentinel's authored look direction points INTO the cabin's firing slit, so adopting it would face
//! the guard at the glass this offset exists to get him out from behind. AI turns to face what it
//! perceives, so the initial heading costs nothing.
//!
//! ⚠ THE GUARD GETS NO SMART ACTION, ON PURPOSE. The legacy class reserved nothing and neither does
//! this: it reads the sentinel to find out WHERE the post is and never occupies it. That is why the
//! legacy class's ReleaseAction() housekeeping is deliberately not ported - there is no reservation to
//! leak. IsActionAccessible() is still consulted, because an inaccessible post is the engine's own
//! signal that something else is standing there.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TowerCoverPostPlacementProvider : OVT_DeploymentPlacementProvider
{
	//! Tower-local offset from the cabin CoverPost to the open-air walkway ringing it.
	protected const vector WALKWAY_OFFSET = "0 0 1.5";

	//! The tag every usable cover post on a watchtower carries.
	protected const string COVER_POST_TAG = "CoverPost";

	//! Towers found by the in-flight query. Instance state rather than a local because
	//! QueryEntitiesBySphere calls back into a method, and it is rebuilt at the start of every
	//! resolve - the contract forbids caching across calls.
	protected ref array<IEntity> m_aFoundTowers;

	//------------------------------------------------------------------------------------------------
	//! One post per watchtower within `radius` that still has an accessible cover post.
	//! \param[in] deploymentPosition Centre of the search.
	//! \param[in] radius How far to look for towers.
	//! \param[in] factionIndex Unused - a tower belongs to whoever holds the ground under it.
	//! \return The walkway posts. Empty when there is no tower nearby, which is the usual answer.
	override array<ref OVT_DeploymentPlacement> ResolvePlacements(vector deploymentPosition, float radius, int factionIndex)
	{
		array<ref OVT_DeploymentPlacement> placements = new array<ref OVT_DeploymentPlacement>();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return placements;

		m_aFoundTowers = new array<IEntity>();
		world.QueryEntitiesBySphere(deploymentPosition, radius, AddTower, FilterTower, EQueryEntitiesFlags.ALL);

		foreach (IEntity tower : m_aFoundTowers)
		{
			if (!tower)
				continue;

			SCR_AISmartActionSentinelComponent sentinel = FindCoverPost(tower);
			if (!sentinel)
				continue;

			vector mat[4];
			tower.GetWorldTransform(mat);

			vector post = mat[3] + (sentinel.GetActionOffset() + WALKWAY_OFFSET).Multiply3(mat);

			placements.Insert(new OVT_DeploymentPlacement(post, vector.Zero));
		}

		// Do not hold the entities past the call - a destroyed tower is replaced by its ruin and this
		// array would keep a stale pointer alive until the next resolve.
		m_aFoundTowers = null;

		return placements;
	}

	//------------------------------------------------------------------------------------------------
	override string GetProviderName()
	{
		return "tower cover posts";
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity Candidate from the sphere query.
	//! \return True for a destructible building whose map descriptor says it is a watchtower.
	protected bool FilterTower(IEntity entity)
	{
		if (!entity)
			return false;

		// The legacy class matched on the class name string and nothing else claims MDT_TOWER among
		// destructible buildings, so the same pair of tests is kept rather than widened.
		if (entity.ClassName() != "SCR_DestructibleBuildingEntity")
			return false;

		SCR_MapDescriptorComponent descriptor = SCR_MapDescriptorComponent.Cast(entity.FindComponent(SCR_MapDescriptorComponent));
		if (!descriptor)
			return false;

		return descriptor.GetBaseType() == EMapDescriptorType.MDT_TOWER;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] entity A tower that passed the filter.
	//! \return True, to keep the query running.
	protected bool AddTower(IEntity entity)
	{
		m_aFoundTowers.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The first accessible "CoverPost" sentinel on a tower.
	//!
	//! FIRST, NOT NEAREST OR BEST: a watchtower has one cabin, and the legacy class took the first
	//! match for the same reason. Accessibility is consulted because a reserved post means somebody
	//! else is already up there.
	//! \param[in] tower The tower entity.
	//! \return The sentinel, or null when the tower has none free.
	protected SCR_AISmartActionSentinelComponent FindCoverPost(notnull IEntity tower)
	{
		array<Managed> components = {};
		tower.FindComponents(SCR_AISmartActionSentinelComponent, components);

		foreach (Managed candidate : components)
		{
			SCR_AISmartActionSentinelComponent sentinel = SCR_AISmartActionSentinelComponent.Cast(candidate);
			if (!sentinel)
				continue;

			array<string> tags = {};
			sentinel.GetTags(tags);

			if (tags.Contains(COVER_POST_TAG) && sentinel.IsActionAccessible())
				return sentinel;
		}

		return null;
	}
}
