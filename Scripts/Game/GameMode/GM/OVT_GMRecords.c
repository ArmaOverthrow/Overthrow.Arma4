//------------------------------------------------------------------------------------------------
//! The four per-entity record shapes carried by the Game Master snapshot fan.
//!
//! ONE SET OF CLASSES, USED ON BOTH SIDES. The server fills them in OVT_GMSnapshotBuilder, the fan
//! carries them FIELD BY FIELD as RPC parameters (never as objects and never as array<> parameters -
//! neither crosses the wire, and a hand-rolled bitstream is not an option because ScriptBitWriter
//! hard-crashes on first use from script), and the client rebuilds them into the identical classes
//! inside OVT_GMCampaignState. Two parallel shapes for "the server's base record" and "the client's
//! base record" would be two places for a field to be added and one place for it to be forgotten.
//!
//! THE JOIN KEYS ARE DELIBERATE (see the plan's 3.4):
//!  - groups and deployments are keyed by RplId, the only entity reference that means the same thing
//!    on both machines. A sibling with a GM selection reads the entity's RplComponent.Id() and looks
//!    the record up; POSITION IS THEREFORE NOT SENT for either - resolve the RplId and ask the entity.
//!  - bases are keyed by their POSITIONAL INDEX into OVT_OccupyingFactionManager.m_Bases, because
//!    clients already receive an index-aligned base list through that manager's JIP stream.
//!
//! They are plain Managed data with no behaviour on purpose: anything that looks like a query belongs
//! on OVT_GMCampaignState (which owns the arrays) or on the consumer.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! One occupying-faction base, aggregated over its upgrades.
//!
//! The aggregate exists so a consumer can draw a base without walking the upgrade records: the three
//! numbers here are exactly the sums of the OVT_GMBaseUpgradeRecord rows carrying the same base index,
//! except that m_iUpgrades counts ALL upgrades while only NON-EMPTY ones get a row (see the builder).
//! A base showing 12 upgrades and 4 rows is therefore correct, not truncated.
//------------------------------------------------------------------------------------------------
class OVT_GMBaseRecord : Managed
{
	//! Positional index into OVT_OccupyingFactionManager.m_Bases. THE JOIN KEY.
	int m_iBaseIndex;

	//! Sum of OVT_BaseUpgrade.GetResources() over every upgrade on the base.
	int m_iResources;

	//! Sum of OVT_BasePatrolUpgrade.GetNumGroups() over the upgrades that have groups at all.
	int m_iGroups;

	//! How many upgrades the base runs, non-empty or not.
	int m_iUpgrades;
}

//------------------------------------------------------------------------------------------------
//! One non-empty upgrade on one base: what it is, what it holds, what it fields.
//!
//! Positions are deliberately absent - OVT_BaseUpgradeData.pos exists and gm-map will want it for its
//! upgrade icon layer, but this feature has no renderer, so shipping it now would be speculative
//! payload. It is a two-field additive extension when a consumer actually needs it.
//------------------------------------------------------------------------------------------------
class OVT_GMBaseUpgradeRecord : Managed
{
	//! Positional index of the owning base into OVT_OccupyingFactionManager.m_Bases.
	int m_iBaseIndex;

	//! The upgrade's concrete ClassName() - OVT_BaseUpgradeSpecops, OVT_BaseUpgradeTownPatrol, ...
	//! Diagnostic text for a GM, never parsed.
	string m_sType;

	//! OVT_BaseUpgrade.GetResources() for this upgrade.
	int m_iResources;

	//! Live + banked groups for this upgrade, or 0 for an upgrade class that fields none.
	int m_iGroups;
}

//------------------------------------------------------------------------------------------------
//! One active deployment.
//!
//! NO THREAT LEVEL: OVT_DeploymentConfig carries only m_iMinimumThreatLevel, which is a SPAWN
//! PRECONDITION (the threat a deployment needs before it may be considered) and not a live per
//! deployment threat reading, so sending it would show a GM a constant that says nothing about the
//! deployment in front of them. OVT_DeploymentComponent.m_fThreatLevel does hold the area threat the
//! deployment was created with, but it is written once at creation and never recomputed, so it is a
//! historical value with the same problem. Neither is worth a wire field today; if gm-map wants
//! either it is an additive extension with a comment explaining which one it chose.
//------------------------------------------------------------------------------------------------
class OVT_GMDeploymentRecord : Managed
{
	//! The deployment entity's RplId. THE JOIN KEY - resolve it for position, prefab, anything else.
	RplId m_RplId;

	//! OVT_DeploymentComponent.GetDeploymentName() - the config's display name.
	string m_sName;

	//! Faction index that controls it (OVT_DeploymentComponent.GetControllingFaction()).
	int m_iFaction;

	//! Resources spent on it so far (OVT_DeploymentComponent.GetResourcesInvested()).
	int m_iResourcesInvested;

	//! Whether it is currently running (OVT_DeploymentComponent.IsDeploymentActive()).
	bool m_bActive;
}

//------------------------------------------------------------------------------------------------
//! One tagged AI group: why it exists.
//!
//! Sourced from OVT_GMGroupRegistry, which tags at spawn and never untags - so a record here is
//! always for a group whose entity resolved at build time, and a group that was never tagged is
//! simply ABSENT rather than wrong.
//------------------------------------------------------------------------------------------------
class OVT_GMGroupRecord : Managed
{
	//! The group entity's RplId. THE JOIN KEY.
	RplId m_RplId;

	//! An OVT_EGroupOrigin value, as int because that is what the wire carries.
	int m_iOriginType;

	//! Base index / town id / radio-tower id, or -1 where the origin has no meaningful index.
	int m_iOriginIndex;

	//! Free text naming the concrete producer - an upgrade's ClassName(), a deployment's config name.
	string m_sReason;
}
