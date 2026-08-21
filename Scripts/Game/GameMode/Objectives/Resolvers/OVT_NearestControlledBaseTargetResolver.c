//------------------------------------------------------------------------------------------------
//! THE NEAREST BASE THE ASKING FACTION ACTUALLY HOLDS - where a supply line starts.
//!
//! The port of the director's nearest-controlled-base walk: it walks the faction's OWN base list and
//! take the nearest to the objective.
//!
//! 🔴 IT IS "NEAREST OF OURS", NEVER "NEAREST, THEN CHECK IF IT IS OURS", AND THE TWO ARE DIFFERENT
//! QUESTIONS. The second one answers "no source" whenever an enemy base happens to be closer, which is
//! precisely the map state a forward base exists for. That subtly different question broke the insertion
//! module's copy of this walk once already, and it is the reason this is one function rather than an
//! inline loop in two places.
//!
//! ⚠ IT IS THE SAME WALK OVT_NearestControlledBaseSourceProvider MAKES, deliberately: the whole point of
//! asking here is to predict what that provider will answer for the deployment about to be created. The
//! provider cannot be called directly - it resolves against a live deployment position and none exists
//! yet - so the agreement is kept by both walking the faction's own list.
//!
//! ⚠ ITS CONSUMER ARRIVES WITH THE FORWARD BASE (build phase 5), and the director's own copy of this
//! walk dies with it. Until then the two coexist, which is what the strangler does everywhere else.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_NearestControlledBaseTargetResolver : OVT_ObjectiveTargetResolver
{
	//------------------------------------------------------------------------------------------------
	//! \param[in] objective The objective "nearest" is measured to.
	//! \param[in] factionIndex The faction that must control the base.
	//! \param[out] positions Receives the one nearest held base, or nothing when the faction holds none.
	//! \return True when the faction holds at least one base.
	override bool Resolve(notnull OVT_ObjectiveInstance objective, int factionIndex, notnull array<vector> positions)
	{
		positions.Clear();

		if (!objective.IsLive())
			return false;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		array<OVT_BaseData> controlled = occupying.GetBasesControlledBy(factionIndex);
		if (!controlled || controlled.IsEmpty())
			return false;

		vector target = objective.GetTargetPosition();

		bool found = false;
		float best = 0;
		vector nearest = vector.Zero;

		foreach (OVT_BaseData base : controlled)
		{
			if (!base)
				continue;

			float distance = vector.Distance(base.location, target);

			if (found && distance >= best)
				continue;

			found = true;
			best = distance;
			nearest = base.location;
		}

		if (!found)
			return false;

		positions.Insert(nearest);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resolver's name, for the registry's validator and for debug output.
	override string GetResolverName()
	{
		return "NearestControlledBase";
	}
}
