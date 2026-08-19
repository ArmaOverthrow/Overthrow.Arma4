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
	//! WHETHER A FORCE SETTING OUT FROM THIS ORIGIN CAN BE GIVEN A TRUCK AT ALL.
	//!
	//! ⚠ THIS IS A PROPERTY OF THE PLACE, NOT A DISTANCE, WHICH IS WHY IT IS NOT m_fWalkThresholdDistance.
	//! The threshold answers "is this hop short enough to walk"; this answers "is there anything here to
	//! drive". They are different questions and a config cannot express the second with the first: raising
	//! the threshold high enough to make a forward base walk would also make every insertion from a REAL
	//! base walk, including the 2.4 km opening drive that is exactly what the trucks are for.
	//!
	//! WHY IT EXISTS. The occupying faction's forward operating base is a field camp raised on a lattice
	//! point between the rear and the objective. It has no motor pool, no authored vehicle spawn and no
	//! guarantee of a road within a kilometre - so a truck spawned there does not just look wrong, it
	//! reliably strands: on the play-test that prompted this (2026-08-19) the transport stopped making
	//! progress 583 m short of its landing zone about a minute after setting off, the force walked in
	//! anyway, and a stranded truck was left beside the camp for the collector to sweep up. The convoy
	//! cost was spent to make the insertion slower and messier than the walk it fell back to.
	//!
	//! ⚠ IT TAKES THE RESOLVED POSITION RATHER THAN REMEMBERING THE LAST ONE, and that is contract point 2
	//! rather than an inconvenience. A provider is a stateless answerer; a "did my last call resolve the
	//! forward base" flag would be exactly the cache across calls that point forbids, and it would be
	//! read on a later pass than the one that set it.
	//!
	//! THE DEFAULT IS TRUE, so every existing provider and every mod's provider keeps today's behaviour
	//! without touching this. Only an origin that genuinely has no vehicles overrides it.
	//! \param[in] sourcePosition An origin this provider resolved.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \return True when a transport may be spawned at that origin.
	bool SourceProvidesTransport(vector sourcePosition, int factionIndex)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! THE SAME QUESTION AS SourceProvidesTransport, ASKED WITH NO CONTEXT, AT PRICING TIME.
	//!
	//! ⚠ IT IS A SEPARATE METHOD BECAUSE THE PRICING PATH HAS NOTHING TO PASS IT, and that is structural
	//! rather than an oversight. A deployment's price comes from GetTotalResourceCost() walking the
	//! CONFIG TEMPLATE's modules - there is no deployment, no position and no faction at that point, so
	//! the precise question cannot be asked. This one is answerable anyway, because a provider can look
	//! at the world and say "right now, everything I would resolve is on foot".
	//!
	//! THE TWO MUST AGREE, and the direction of any disagreement matters:
	//!   - FALSE HERE, TRUE THERE (priced for no truck, then given one) is BENIGN. The transport cost is
	//!     a budget line and nothing debits it at spawn time - see m_iTruckCostOverride - so the faction
	//!     simply got a truck cheaply. Nothing fails.
	//!   - TRUE HERE, FALSE THERE (priced for a truck, then walks) is the ORIGINAL DEFECT this pair was
	//!     written to fix: money charged for a vehicle that is never spawned.
	//! So an implementation that is unsure must answer TRUE here, never false: over-charging is a
	//! balance question and under-delivering is a bug.
	//!
	//! ⚠ ANSWER FROM THE WORLD, LIKE EVERYTHING ELSE ON THIS SEAM. The price is recomputed each time it
	//! is asked, so a forward base going up or being dismantled re-prices the very next pass. Nothing
	//! persists a deployment's price, so there is no stale figure to reconcile.
	//! \return True when an operation priced from this provider should budget for a transport.
	bool MayProvideTransport()
	{
		return true;
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
