//------------------------------------------------------------------------------------------------
//! Per-site-type tuning for the territory overlay, one record per location type.
//!
//! This is the whole reason adding a site type is a CONFIG edit plus one collector method and never a
//! change to the solver or the emitter: the collector asks for the record by id, and everything that
//! decides how much ground that type wins lives here.
//!
//! THE REACH CAP IS GONE and weights are purely RELATIVE. Ground is classified square by square against
//! every site at once, so a weight decides where a boundary sits between two types - it does not buy a
//! radius, and there is no longer anywhere to configure one.
//!
//! Ids in use today are "town", "base", "radiotower" and "fob". A record whose id matches no collector
//! is simply never asked for, and a collector whose id has no record collects nothing - both are quiet,
//! recoverable states rather than errors.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_TerritorySiteConfig
{
	[Attribute("town", UIWidgets.Auto, "Site type id this record configures: town / base / radiotower / fob. Must match the id the collector asks for.")]
	string m_sId;

	[Attribute("1.0", UIWidgets.Auto, "Multiplicative influence weight. A point belongs to the site minimising dist / weight, so 2 reaches twice as far against an equal rival. Order the types bases > towns > FOBs.")]
	float m_fWeight;

	[Attribute("1", UIWidgets.Auto, "When false this type contributes no sites at all, which is the cheapest way to isolate one type while tuning.")]
	bool m_bEnabled;
}
