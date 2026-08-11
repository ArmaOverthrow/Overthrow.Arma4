//------------------------------------------------------------------------------------------------
//! One influence relation between a source location and the town it reaches, as derived by the map
//! layer and consumed by its renderer.
//!
//! ONE RECORD PER (SOURCE, TARGET) PAIR, NOT PER MODIFIER. The campaign collapses every same-kind
//! source in range of a town into a single modifier record - a town inside three enemy towers'
//! reach carries exactly one NearbyRadioTowerNegative - but which of those towers is responsible is
//! the entire question the overlay exists to answer, so three sources produce three edges here.
//! Momentum behaves the same way: the server stops scanning at the first qualifying neighbour, while
//! every player-held town in range independently satisfies the condition and every one of them is
//! drawn.
//!
//! THE MODIFIER IS CARRIED AS DATA. Both the name and the resolved index are decided by
//! OVT_InfluenceRules at derivation time and stored here, so the renderer holds no modifier name and
//! no "this kind of source means that modifier" assumption. One source kind mapping to a different
//! modifier later is then a change in one function rather than a change in two files that have to be
//! kept in step.
//!
//! WORLD SPACE, NOT SCREEN SPACE. Both endpoints are world positions and stay that way: an edge
//! survives every pan and zoom, and only the projection and the dash tessellation are per-frame work.
//------------------------------------------------------------------------------------------------
class OVT_InfluenceEdge : Managed
{
	//! World position the influence is exerted FROM - the source location.
	vector m_vFrom;

	//! World position the influence is exerted ON - always a town.
	vector m_vTo;

	//! Faction index controlling the SOURCE. The edge is coloured by this, never by the target's
	//! faction, because the message is "who is exerting this".
	int m_iSourceFaction;

	//! Whether the campaign is currently applying the modifier this relation maps to.
	OVT_InfluenceEdgeState m_eState;

	//! What kind of location the source is.
	OVT_InfluenceSourceKind m_eKind;

	//! Support modifier this relation maps to, as authored in the modifier config. Empty when the
	//! (kind, polarity) pair maps to no modifier at all.
	string m_sModifierName;

	//! Index of that modifier in the support modifier config, or -1 when the name resolved to nothing.
	//! Kept alongside the name so a diagnostic can tell "the town does not have this modifier" apart
	//! from "this modifier could not be found", which look identical from the drawn result.
	int m_iModifierIndex;

	//------------------------------------------------------------------------------------------------
	//! \param[in] from World position of the source location.
	//! \param[in] to World position of the town being influenced.
	//! \param[in] sourceFaction Faction index controlling the source.
	//! \param[in] kind Kind of source location.
	//! \param[in] state Whether the campaign is applying the corresponding modifier.
	//! \param[in] modifierName Support modifier the relation maps to, or "".
	//! \param[in] modifierIndex Index of that modifier in the support config, or -1.
	void OVT_InfluenceEdge(vector from, vector to, int sourceFaction, OVT_InfluenceSourceKind kind, OVT_InfluenceEdgeState state, string modifierName, int modifierIndex)
	{
		m_vFrom = from;
		m_vTo = to;
		m_iSourceFaction = sourceFaction;
		m_eKind = kind;
		m_eState = state;
		m_sModifierName = modifierName;
		m_iModifierIndex = modifierIndex;
	}
}
