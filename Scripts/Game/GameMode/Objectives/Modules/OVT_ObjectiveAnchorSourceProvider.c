//------------------------------------------------------------------------------------------------
//! THE FORWARD BASE FIRST, THE NEAREST HELD BASE OTHERWISE.
//!
//! An OVT_DeploymentSourceProvider for anything the objective director sends once its forward
//! operating base is standing: the garrison that reinforces it, and every later operation the design
//! wants launched from it rather than from the rear. Raising a forward base is supposed to SHORTEN the
//! occupying faction's supply line, and this is the one line of code that makes that true - without it
//! the base is scenery and every truck still drives from the same place it always did.
//!
//! ⚠ IT LIVES IN Objectives/Modules/, NOT IN Deployments/Modules/, AND THAT IS DELIBERATE. It names
//! the objective director, and `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/`
//! is empty - comments included - so that "the deployment framework does not know the director exists"
//! stays checkable by one command instead of by reading. A provider's directory has no bearing on how
//! a .conf authors it, so nothing else changes.
//!
//! ⚠ IT FALLS THROUGH RATHER THAN FAILING. No director, no objective, no forward base, or a forward
//! base that has just been torn down: every one of those degrades to the nearest base the faction
//! controls, which is exactly what the default provider answers. A garrison deployment that outlived
//! its forward base by one update therefore still resolves an origin and still delivers its force -
//! it just delivers it from further away. Answering false instead would strand the men, and the
//! insertion module's whole contract is that it never does that.
//!
//! CONTRACT POINTS 2 AND 3 (see OVT_DeploymentSourceProvider): nothing is cached across calls - the
//! forward base can be raised, starved or dismantled between two convergences - and the whole answer
//! costs one component resolve plus, in the fallback, one walk of the faction's base list.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ObjectiveAnchorSourceProvider : OVT_DeploymentSourceProvider
{
	//! How far the forward base may be from the deployment before it stops being a sensible origin for
	//! it. 0 (the default) means "no limit": the director only ever creates these at its own objective,
	//! and the forward base is by construction inside the band between that objective and the rear, so
	//! a limit here would only ever fire on a deployment somebody else created.
	//! How far from the forward base a living occupying group counts as ITS garrison, in metres.
	//!
	//! 250 m is the number the whole forward-base feature already agrees on - it is
	//! OVT_RaiseForwardBaseObjectiveOperation.AREA_RADIUS, the garrison sender's concurrency radius, and
	//! OVT_AssetStarvedObjectiveAbort's own m_fAreaRadius default. A .conf cannot reference a constant,
	//! so this is the fourth place the number is written down and it must not drift from the other three.
	[Attribute(defvalue: "250", desc: "How far from the forward base a living occupying group counts as its garrison, in metres. Used to decide whether the base is still manned enough to be an origin. 0 disables the manned test, restoring the pre-2026-08-21 behaviour")]
	float m_fGarrisonRadius;

	[Attribute(defvalue: "0", desc: "Ignore the forward operating base if it is further than this many metres from the deployment. 0 = no limit")]
	float m_fMaxForwardDistance;

	//! The fallback, held rather than constructed per call: a provider is a stateless answerer, so one
	//! instance answers every fallback for the life of the config.
	protected ref OVT_NearestControlledBaseSourceProvider m_Fallback;

	//------------------------------------------------------------------------------------------------
	void OVT_ObjectiveAnchorSourceProvider()
	{
		// ⚠ `new` DOES NOT APPLY [Attribute()] DEFVALUES, so the fallback's own limit is set here
		// explicitly rather than left to whatever a fresh instance happens to hold. 0 = no limit, which
		// is what a fallback wants: a distant base is still better than no insertion at all.
		m_Fallback = new OVT_NearestControlledBaseSourceProvider();
		m_Fallback.m_fMaxSourceDistance = 0;

		// ⚠ AND THE MANNED TEST'S RADIUS FOR THE SAME REASON. A provider built with `new` would otherwise
		// hold 0 here, which DISABLES the wiped-base refusal - so the guard would be live for every
		// authored config and silently absent for anything constructed in code. A rule that depends on how
		// its object was created is a landmine; this line removes the difference. An authored value still
		// wins, because attributes are applied after the constructor.
		m_fGarrisonRadius = 250;
	}

	//------------------------------------------------------------------------------------------------
	//! \param[in] deploymentPosition Where the force is going.
	//! \param[in] factionIndex The deployment's controlling faction.
	//! \param[out] sourcePosition The forward base, or the nearest controlled base.
	//! \return True when there is anywhere at all for the force to come from.
	override bool ResolveSource(vector deploymentPosition, int factionIndex, out vector sourcePosition)
	{
		sourcePosition = vector.Zero;

		if (ResolveForwardBase(deploymentPosition, sourcePosition))
			return true;

		return m_Fallback.ResolveSource(deploymentPosition, factionIndex, sourcePosition);
	}

	//------------------------------------------------------------------------------------------------
	//! The forward operating base, when one is standing and is close enough to be the origin.
	//! \param[in] deploymentPosition Where the force is going.
	//! \param[out] sourcePosition The forward base's position, written only when this returns true.
	//! \return True when the forward base is the answer.
	protected bool ResolveForwardBase(vector deploymentPosition, out vector sourcePosition)
	{
		// Contract point 4: legal to call on a config template with no campaign behind it.
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!director)
			return false;

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return false;

		vector forward = director.GetAssetPosition(OVT_ObjectiveDirectorComponent.ASSET_FOB);
		if (forward == vector.Zero)
			return false;

		if (m_fMaxForwardDistance > 0 && vector.Distance(forward, deploymentPosition) > m_fMaxForwardDistance)
			return false;

		// ==========================================================================================
		// 🔴 A WIPED FORWARD BASE IS NOT AN ORIGIN. "Should a wiped FOB be an insertion origin? No."
		// (author, 2026-08-21.)
		// ==========================================================================================
		// The author cleared a forward base's garrison and, standing in it, watched a fresh team sourced
		// FROM it. Men do not set out from a position the resistance has just taken off the faction, and
		// a base with nobody left in it is the worst possible place to conjure a force at: the ONLY
		// distance a force sourced here has to cover is zero, so it materialises exactly where the player
		// who just cleared it is standing.
		//
		// ⚠ WHAT "WIPED" MEANS HERE, PRECISELY, BECAUSE THREE DIFFERENT CONDITIONS COULD CLAIM THE WORD
		// AND THEY DO NOT FIRE TOGETHER:
		//   asset.up          says the STRUCTURE is standing. It was TRUE throughout the play-test - the
		//                     base was intact and undefended - so it cannot be the test.
		//   "cut off"         is OVT_AssetStarvedObjectiveAbort's compound of three inputs behind a
		//                     MINUTES-long counter. It is a decision about abandoning the objective and
		//                     it lags reality by design: the base in the play-test was declared cut off
		//                     seconds AFTER the team had already been sourced from it. Too slow.
		//   ZERO LIVING       is what was actually asked for and it is immediate. If no occupying group
		//   GROUPS AT IT      with a survivor is standing at the base, nothing is there to send anybody.
		// The third is the test, through the same primitive the starvation abort counts with, so the two
		// can never disagree about what "manned" means.
		//
		// ⚠ AND THE FALL-THROUGH IS BETTER THAN THE THING IT REPLACES, WHICH IS NOT OBVIOUS. Refusing here
		// hands the question to m_Fallback (nearest controlled base, no distance limit), and that origin
		// HAS a motor pool - so SourceProvidesTransport() below answers true for it and the force gets a
		// truck. A force sourced from the forward base always walks, because the base has no vehicles. So
		// the refusal does not sentence anybody to a two-kilometre march: it upgrades them from a march to
		// a drive. The only case that gets worse is a faction holding no base at all, and that force was
		// never being sent anyway.
		//
		// ⚠ IT IS SELF-HEALING AND DELIBERATELY SLIGHTLY CONSERVATIVE. A forward base that has been raised
		// but whose garrison has not ARRIVED yet also reads as unmanned, so the first garrison is sourced
		// from a real base and drives in - which is exactly the right answer, and is the shape the
		// garrison config wants (it is one of this provider's four consumers). The moment men are standing
		// there, the base is an origin again.
		if (m_fGarrisonRadius > 0 && OVT_ObjectiveDirectorComponent.CountAliveOccupyingGroupsAt(forward, m_fGarrisonRadius) < 1)
			return false;

		sourcePosition = forward;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! THE FORWARD BASE HAS NO MOTOR POOL, so anything setting out from it walks.
	//!
	//! ⚠ IT COMPARES THE POSITION RATHER THAN TRUSTING THE CALLER, because this provider answers TWO
	//! different origins and only one of them is on foot. An operation that resolved the FALLBACK - no
	//! forward base standing, or one torn down between two passes - set out from a real base with real
	//! vehicle spawns, and must still get its truck. Handing back a flat "no transport" for everything
	//! this provider answers would ground every insertion the objective director sends, including the
	//! 2.4 km opening drive from the rear.
	//!
	//! THE TOLERANCE IS WHAT MAKES THE COMPARISON HONEST. The origin travels out through
	//! ResolveSource()'s `out` parameter and back in here as a float vector, and the director's own
	//! record is the authority in between; an exact `==` on two float vectors is the kind of test that
	//! works until something rounds. A metre is far tighter than the distance between a forward base and
	//! any real base - they are hundreds of metres apart by construction - so nothing can be mistaken
	//! for the other.
	//! \param[in] sourcePosition The origin this provider resolved.
	//! \param[in] factionIndex The deployment's controlling faction. Unused: the forward base belongs to
	//!            the occupying faction and the director only ever has one.
	//! \return False when the resolved origin is the forward base, true for the fallback.
	override bool SourceProvidesTransport(vector sourcePosition, int factionIndex)
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!director)
			return true;

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return true;

		vector forward = director.GetAssetPosition(OVT_ObjectiveDirectorComponent.ASSET_FOB);
		if (forward == vector.Zero)
			return true;

		return vector.Distance(forward, sourcePosition) > FORWARD_BASE_MATCH_TOLERANCE_M;
	}

	//! How close a resolved origin has to be to the forward base to BE it. See SourceProvidesTransport.
	static const float FORWARD_BASE_MATCH_TOLERANCE_M = 1;

	//------------------------------------------------------------------------------------------------
	//! WHILE THE FORWARD BASE IS STANDING, OPERATIONS PRICED FROM THIS PROVIDER DO NOT BUDGET A TRUCK.
	//!
	//! This is what makes the forward base an actual ADVANTAGE rather than only a shorter drive: every
	//! operation the director sends while it stands is cheaper by m_iTruckCostOverride, and goes back to
	//! full price the moment a player finds the camp and dismantles it. Author intent, 2026-08-19: *"the
	//! whole point is that FOB gives them advantages until you find it and dismantle it"*.
	//!
	//! ⚠ IT RE-PRICES LIVE, AND THAT IS THE POINT RATHER THAN A HAZARD. Nothing persists a deployment's
	//! price and GetTotalResourceCost() recomputes on every ask, so raising the base drops the price of
	//! the next operation and dismantling it restores the price - including the objective reserve floor,
	//! which is explicitly built to be re-priced.
	//!
	//! ⚠ THE ONE IMPRECISION, STATED RATHER THAN HIDDEN. With m_fMaxForwardDistance authored above zero
	//! this can answer false while a particular deployment is far enough from the camp that
	//! ResolveSource() falls back to a real base and DOES spawn a truck. That is the benign direction
	//! named in the seam's header - the faction gets a truck it was not charged for, and nothing fails -
	//! and it cannot happen at all on the shipped configs, every one of which authors 0 (no limit).
	//! \return False while the forward base is up, true otherwise.
	override bool MayProvideTransport()
	{
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (!director)
			return true;

		if (!director.IsAssetUp(OVT_ObjectiveDirectorComponent.ASSET_FOB))
			return true;

		return director.GetAssetPosition(OVT_ObjectiveDirectorComponent.ASSET_FOB) == vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	override string GetProviderName()
	{
		return "objective forward base, then nearest controlled base";
	}
}
