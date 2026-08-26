//------------------------------------------------------------------------------------------------
//! THE OBJECTIVE ITSELF - one position, the place the machine committed to.
//!
//! The implicit `m_Objective.position` that the harassment and sabotage senders used to carry inline.
//! Answers exactly one entry, so the caller's dedup-then-next-candidate walk has nothing to walk on
//! to: an already-served objective is a refusal, not a second try somewhere else.
//!
//! ⚠ IT ALSO CARRIES SABOTAGE'S TWO REFUSALS, AS AN OPTIONAL GUARD, AND THAT IS NOT A GRAB-BAG.
//! The hard-coded sabotage sender refused in three ways before it created anything, and two of them were
//! statements about the DESTINATION rather than about the operation:
//!
//!   1. "the objective position must still be a real base" - a restored payload naming a base that has
//!      since gone, or an objective committed at an arbitrary position, must not buy a team that then
//!      strips whichever base happened to be nearest on the map. The shipped radius is 100 m and it is
//!      not defensive padding: selection copies base.location verbatim, so in a live campaign the test
//!      passes by metres and only a fixture or a stale restore can fail it.
//!   2. "somebody took it back while the ramp was running" - a base the asking faction now holds is not
//!      a target. The objective is over on the next control-change reselect; sending a team to strip our
//!      own base in the meantime is not.
//!
//! Both are "is there anywhere to send this", which is this seam's whole question, so they live here
//! rather than as two more attributes on the operation module. ⚠ AND THE ANSWER IS THE BASE'S OWN
//! LOCATION, NOT THE OBJECTIVE POSITION, exactly as the sender created at base.location.
//!
//! ⚠ THE GUARD IS OFF BY DEFAULT (0 m) AND THE HARASSMENT LADDER AUTHORS IT OFF. A town objective's
//! destination is the town, and asking it to find a base would refuse every harassment operation ever
//! sent.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_ObjectiveSelfTargetResolver : OVT_ObjectiveTargetResolver
{
	[Attribute(defvalue: "0", desc: "When above zero, answer NOTHING unless a base the asking faction does NOT hold sits within this many metres of the objective - and answer that base's own position rather than the objective's. Ports the hard-coded sabotage sender's two refusals, whose radius was 100 m. 0 = answer the objective position unconditionally, which is what a town ramp wants")]
	float m_fRequireEnemyHeldBaseWithin;

	//------------------------------------------------------------------------------------------------
	//! \param[in] objective The objective the operation belongs to.
	//! \param[in] factionIndex The faction running the operation.
	//! \param[out] positions Receives the one destination, or nothing.
	//! \return True when there is somewhere to send this.
	override bool Resolve(notnull OVT_ObjectiveInstance objective, int factionIndex, notnull array<vector> positions)
	{
		positions.Clear();

		if (!objective.IsLive())
			return false;

		vector target = objective.GetTargetPosition();

		if (m_fRequireEnemyHeldBaseWithin <= 0)
		{
			positions.Insert(target);
			return true;
		}

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		OVT_BaseData base = occupying.GetNearestBase(target);
		if (!base)
			return false;

		if (vector.Distance(base.location, target) > m_fRequireEnemyHeldBaseWithin)
			return false;

		// Somebody took it back while the ramp was running. See the class header.
		if (base.faction == factionIndex)
			return false;

		positions.Insert(base.location);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resolver's name, for the registry's validator and for debug output.
	override string GetResolverName()
	{
		return "ObjectiveSelf";
	}
}
