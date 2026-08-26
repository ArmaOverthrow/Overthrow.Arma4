//------------------------------------------------------------------------------------------------
//! One exact place a deployment can stand a group: a world position plus the direction it faces.
//!
//! WHY A CLASS AND NOT A BARE VECTOR. The sniper markers carry an authored heading and the whole
//! point of manning them is that the team looks where the marker points; a plain array<vector> would
//! throw that away and put two snipers on a ridge facing whatever the engine felt like. Positions
//! that genuinely have no meaningful heading (a tower walkway, a defend-position sentinel) answer
//! vector.Zero angles and are teleported onto an identity matrix, which is exactly what the legacy
//! tower guard did.
//------------------------------------------------------------------------------------------------
class OVT_DeploymentPlacement
{
	//! World position of the post.
	vector m_vPosition;

	//! Yaw/pitch/roll the occupant should face, in the engine's Math3D.AnglesToMatrix order.
	//! vector.Zero means "no authored heading" and produces an identity rotation.
	vector m_vAngles;

	//------------------------------------------------------------------------------------------------
	//! \param[in] position World position.
	//! \param[in] angles Facing, or vector.Zero for none.
	void OVT_DeploymentPlacement(vector position, vector angles)
	{
		m_vPosition = position;
		m_vAngles = angles;
	}

	//------------------------------------------------------------------------------------------------
	//! The post as a world transform, ready for BaseGameEntity.Teleport().
	//! \param[out] outMat The transform. Rotation from m_vAngles, translation from m_vPosition.
	void GetTransform(out vector outMat[4])
	{
		Math3D.AnglesToMatrix(m_vAngles, outMat);
		outMat[3] = m_vPosition;
	}
}

//------------------------------------------------------------------------------------------------
//! THE MODDER SEAM FOR EXACT PLACEMENT. Answers "where, precisely, should this deployment stand its
//! groups?" for OVT_PlacedInfantrySpawningDeploymentModule.
//!
//! A config authors ONE of these inside its placement module; three ship (tower walkways, curated
//! sniper markers, a base's defend positions) and a mod that wants men on a rooftop, a bridge or a
//! bunker firing step writes a fourth and changes nothing else. That generality is D2's instruction,
//! not a speculative extra: the legacy system had two hard-coded copies of the same mechanism and no
//! way to add a third without another base-upgrade class.
//!
//! THE CONTRACT, and every implementation must hold to all four points:
//!   1. NEVER RETURN NULL. An empty array means "nothing qualifies here", which is a normal answer -
//!      most deployments stand nowhere near a tower or a marker. Null would make every caller guard.
//!   2. ANSWER FROM THE WORLD, EVERY TIME. This is called again on every re-materialisation and after
//!      every load, and the world moves under it: towers are destroyed and replaced by ruins, markers
//!      are deleted, a base's slots are re-discovered on load. Nothing may be cached across calls.
//!   3. BE CHEAP ENOUGH TO CALL FROM A CONVERGENCE PASS. A sphere query per call is fine; anything
//!      that walks the whole world is not.
//!   4. BE SAFE WITH NO WORLD STATE. It is legal to call this on a config template with no deployment
//!      behind it (that is what the Init tier does), so every manager lookup is guarded and an
//!      unresolvable one answers an empty list rather than throwing.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class OVT_DeploymentPlacementProvider
{
	//------------------------------------------------------------------------------------------------
	//! Every post this provider can offer around a deployment.
	//! \param[in] deploymentPosition Centre of the search - the deployment's own position.
	//! \param[in] radius How far to look.
	//! \param[in] factionIndex The deployment's controlling faction, for providers that care.
	//! \return The posts, in a stable-as-the-world-allows order. NEVER null; empty means none.
	array<ref OVT_DeploymentPlacement> ResolvePlacements(vector deploymentPosition, float radius, int factionIndex)
	{
		return new array<ref OVT_DeploymentPlacement>();
	}

	//------------------------------------------------------------------------------------------------
	//! Human-readable name for warnings and debug output.
	//! \return The provider's name.
	string GetProviderName()
	{
		return "none";
	}
}
