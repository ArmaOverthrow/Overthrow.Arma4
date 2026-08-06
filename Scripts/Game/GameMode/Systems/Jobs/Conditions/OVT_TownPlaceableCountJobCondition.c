//------------------------------------------------------------------------------------------------
//! Passes only while a location still has FEWER than m_iMaxCount placeables of a given type standing.
//!
//! This is what stops a repeatable "go and place something" job from being offered forever: five
//! posters is enough propaganda for one town, one pirate radio is enough transmitter.
//!
//! THE RADIUS MATCHES THE PLACEMENT RULE. Both Poster and PirateRadio are m_bNearTown placeables,
//! and OVT_PlaceContext enforces that with `distance <= OVT_TownManagerComponent.GetTownRange(town)`.
//! Using the same range here means every placement that can satisfy the job is a placement this
//! condition can see - if the two ever disagreed, a job would complete without moving its own
//! counter and would be re-offered immediately, forever.
//!
//! COST. One QueryEntitiesBySphere per evaluation. Put this LAST in a job's condition list:
//! OVT_JobManagerComponent.JobShouldStart() breaks on the first false, so a cheap random/support
//! gate in front of it keeps the query off the 10 second offer loop's hot path in the common case.
//! (The manager also skips towns that already have this job running before conditions are consulted.)
//------------------------------------------------------------------------------------------------
class OVT_TownPlaceableCountJobCondition : OVT_JobCondition
{
	[Attribute("", UIWidgets.EditBox, "Name of the placeable to count (m_sName in placeables.conf, and m_sPlaceableType on its prefabs)")]
	string m_sPlaceableName;

	[Attribute("1", UIWidgets.EditBox, "Offer the job only while the location has fewer than this many of them")]
	int m_iMaxCount;

	[Attribute("0", UIWidgets.EditBox, "Search radius override in meters. 0 = the town's own size-based range, or the difficulty base range for base jobs")]
	float m_fRange;

	//! Accumulator for the sphere query below. Server-side and single threaded, same pattern as
	//! OVT_PlaceableItemJobStage and OVT_ItemLimitChecker
	protected int m_iCount;

	//------------------------------------------------------------------------------------------------
	//! \param[in] town The town context, or null for a base job.
	//! \param[in] base The base context, or null for a town job.
	//! \param[in] playerId Unused - the count is a property of the location, not of any player.
	//! \return True while the location is still below its quota of this placeable.
	override bool ShouldStart(OVT_TownData town, OVT_BaseData base, int playerId)
	{
		if(m_sPlaceableName.IsEmpty()) return false;
		if(m_iMaxCount < 1) return false;

		vector center;
		float range = m_fRange;

		if(town)
		{
			center = town.location;
			if(range <= 0) range = OVT_Global.GetTowns().GetTownRange(town);
		}
		else if(base)
		{
			center = base.location;
			if(range <= 0) range = OVT_Global.GetConfig().m_Difficulty.baseRange;
		}
		else
		{
			return false;
		}

		return CountPlaceables(center, range) < m_iMaxCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Counts standing placeables of m_sPlaceableName within range of a point.
	//! \param[in] center World position to search around.
	//! \param[in] range Search radius in meters.
	//! \return How many were found.
	protected int CountPlaceables(vector center, float range)
	{
		m_iCount = 0;

		GetGame().GetWorld().QueryEntitiesBySphere(center, range, CountPlaceableCallback, FilterPlaceableCallback, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);

		return m_iCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Query filter - passes only placeables whose type matches.
	protected bool FilterPlaceableCallback(IEntity entity)
	{
		if(!entity) return false;

		OVT_PlaceableComponent placeable = OVT_PlaceableComponent.Cast(entity.FindComponent(OVT_PlaceableComponent));
		if(!placeable) return false;

		return placeable.GetPlaceableType() == m_sPlaceableName;
	}

	//------------------------------------------------------------------------------------------------
	//! Query result - one more of them.
	protected bool CountPlaceableCallback(IEntity entity)
	{
		m_iCount++;
		return true;
	}
}
