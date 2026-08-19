//------------------------------------------------------------------------------------------------
//! THE MODDER SEAM FOR ORIGIN. Answers "where does this deployment's force actually come FROM?" for
//! OVT_InsertionSpawningDeploymentModule.
//!
//! A config authors ONE of these inside its insertion module. One ships - the nearest base the
//! deployment's own faction controls - and a mod that wants a force to set out from a forward base, a
//! port, an airfield or a named marker writes a second and changes nothing else. That generality is
//! D7's instruction: the insertion module is general-purpose and has no idea what an objective is, so
//! everything place-specific about an insertion lives behind this one method.
//!
//! THE CONTRACT, and every implementation must hold to all four points:
//!   1. RETURN FALSE, NEVER A ZERO VECTOR. "There is nowhere for this force to come from" is a normal
//!      answer - a faction that has lost every base has no origin - and it must be distinguishable
//!      from "the origin is the world origin". The module's whole reason for existing is that it
//!      refuses to conjure a force out of thin air, and it can only refuse if it is told.
//!   2. ANSWER FROM THE WORLD, EVERY TIME. This is called again on every convergence pass and after
//!      every load, and the world moves under it: bases change hands, forward bases are torn down.
//!      Nothing may be cached across calls.
//!   3. BE CHEAP ENOUGH TO CALL FROM A CONVERGENCE PASS. Walking a faction's base list is fine;
//!      anything that queries the whole world is not.
//!   4. BE SAFE WITH NO WORLD STATE. It is legal to call this on a config template with no deployment
//!      behind it - that is what the initialisation tier does - so every manager lookup is guarded and
//!      an unresolvable one answers false rather than throwing.
//!
//! Shaped on OVT_DeploymentPlacementProvider next door, deliberately: two seams in one framework that
//! answer "where" should not be two different shapes.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_DeploymentSourceProvider
{
	//------------------------------------------------------------------------------------------------
	//! Where a deployment's force sets out from.
	//! \param[in] deploymentPosition Where the force is going - the deployment's own position.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[out] sourcePosition The origin, written only when this returns true.
	//! \return True when there is an origin. FALSE, never a zero vector, when there is not.
	bool ResolveSource(vector deploymentPosition, int factionIndex, out vector sourcePosition)
	{
		sourcePosition = vector.Zero;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Human-readable name for warnings and debug output.
	//! \return The provider's name.
	string GetProviderName()
	{
		return "none";
	}
}

//------------------------------------------------------------------------------------------------
//! THE DEFAULT: the nearest base the deployment's own faction controls.
//!
//! This is what "reinforcements arrive from somewhere real" means with no further authoring, and it
//! is the provider every shipped insertion config is expected to use until something better exists at
//! a given place.
//!
//! ⚠ CONTROLLED BY THE DEPLOYMENT'S FACTION, NOT "THE NEAREST BASE". OVT_InfantrySpawningDeploymentModule's
//! own GetNearestControlledBasePosition() asks for the nearest base and then checks whether that one
//! happens to be friendly - so a resistance-held base 200 m away makes a friendly base 800 m away
//! invisible and the whole registration is abandoned. This walks the faction's OWN base list, so a
//! contested map degrades to a longer drive rather than to no insertion at all.
//!
//! ⚠ IT MAY LEGITIMATELY ANSWER THE DEPLOYMENT'S OWN POSITION. A deployment created at a base that
//! faction holds resolves that base as its source, the separation is zero, and the insertion module's
//! walk threshold then sends the force in on foot from where it already is. That is correct, and it is
//! why the geometry clamps a zero-length line rather than treating it as an error.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_NearestControlledBaseSourceProvider : OVT_DeploymentSourceProvider
{
	//! Ignore bases further away than this. 0 (the default) means "no limit": a very distant base is
	//! still better than no insertion, and the insertion module's own walk threshold and the concurrency
	//! cap are what bound the cost. Set it on a config that would rather walk than drive across a map.
	[Attribute(defvalue: "0", desc: "Ignore controlled bases further than this many metres from the deployment. 0 = no limit")]
	float m_fMaxSourceDistance;

	//------------------------------------------------------------------------------------------------
	//! \param[in] deploymentPosition Where the force is going.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[out] sourcePosition The nearest controlled base's position.
	//! \return True when that faction controls a base within range.
	override bool ResolveSource(vector deploymentPosition, int factionIndex, out vector sourcePosition)
	{
		sourcePosition = vector.Zero;

		// Contract point 4: this is legal to call on a config template with nothing running behind it.
		OVT_OccupyingFactionManager bases = OVT_Global.GetOccupyingFaction();
		if (!bases)
			return false;

		array<OVT_BaseData> controlled = bases.GetBasesControlledBy(factionIndex);
		if (!controlled || controlled.IsEmpty())
			return false;

		bool found = false;
		float bestDistance = 0;

		foreach (OVT_BaseData base : controlled)
		{
			if (!base)
				continue;

			float distance = vector.Distance(base.location, deploymentPosition);

			if (m_fMaxSourceDistance > 0 && distance > m_fMaxSourceDistance)
				continue;

			if (found && distance >= bestDistance)
				continue;

			found = true;
			bestDistance = distance;
			sourcePosition = base.location;
		}

		if (!found)
			sourcePosition = vector.Zero;

		return found;
	}

	//------------------------------------------------------------------------------------------------
	override string GetProviderName()
	{
		return "nearest controlled base";
	}
}
