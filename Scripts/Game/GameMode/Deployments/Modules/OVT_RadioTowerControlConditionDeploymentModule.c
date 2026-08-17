//------------------------------------------------------------------------------------------------
//! Radio tower control condition for deployments: "is the nearest radio tower still held by the
//! faction this deployment belongs to?"
//!
//! A STRUCTURAL COPY of OVT_BaseControlConditionDeploymentModule against GetNearestRadioTower()
//! instead of GetNearestBase(). It is deliberately the same shape - same two evaluations, same two
//! attributes, same hand-written clone - because that class is the reviewed precedent for a
//! "location X still belongs to us" gate and a tower is the same question asked about a different
//! kind of location.
//!
//! IT DOES TWO DIFFERENT JOBS THROUGH TWO METHODS, and both matter:
//!   - EvaluateStaticCondition() gates CREATION. The evaluator asks it before a deployment exists,
//!     with a candidate position, so a tower already in resistance hands never gets a garrison.
//!   - EvaluateCondition() gates the RUNTIME. The reinforcement module asks every one of its checks,
//!     and with m_bDeleteOnConditionFail authored the deployment deletes itself when the answer
//!     turns false. That is the whole of the "a captured tower's garrison deployment is collected"
//!     path - there is no code anywhere that watches for a tower changing hands.
//!
//! THE LOOP THIS CLOSES. Garrison wiped -> OVT_RadioTowerCaptureBehaviorDeploymentModule flips the
//! tower to the resistance -> this condition starts answering false -> the reinforcement module
//! deletes the deployment. If the occupying faction ever retakes the tower, the 30 s evaluator
//! creates a new deployment from scratch. Nothing is bespoke and nothing is persisted.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_RadioTowerControlConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "300", desc: "Maximum distance from the deployment to a radio tower for this condition to consider it. Matches the 300 m ring GetLocationTypeAtPosition uses to classify a position as RADIO_TOWER")]
	float m_fMaxDistance;

	[Attribute(defvalue: "true", desc: "If true, condition passes when the faction controls the tower. If false, passes when the faction does NOT control it")]
	bool m_bRequireControl;

	//! The tower the last evaluation resolved, for debugging and for whoever wants to ask.
	//!
	//! DELIBERATELY NOT A STRONG REF, exactly as the base-control module holds its OVT_BaseData: the
	//! occupying faction manager's m_RadioTowers array owns every tower record for the campaign's
	//! whole life, and holding a second owning reference here would keep a record alive past the only
	//! list that is supposed to define which towers exist.
	protected OVT_RadioTowerData m_NearestTower;

	//------------------------------------------------------------------------------------------------
	//! Runtime gate: does this deployment's faction still hold the tower it was created for?
	//! \return True when the tower's control matches what this module was authored to require.
	override bool EvaluateCondition()
	{
		if (!m_ParentDeployment)
			return false;

		vector deploymentPos = m_ParentDeployment.GetPosition();
		int factionIndex = m_ParentDeployment.GetControllingFaction();

		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;

		m_NearestTower = occupyingFaction.GetNearestRadioTower(deploymentPos);
		if (!m_NearestTower)
			return false;

		// Out of range is a FAILED condition, not a passed one - a deployment that has somehow ended
		// up nowhere near a tower is not a tower garrison and should be collected.
		float distance = vector.Distance(deploymentPos, m_NearestTower.location);
		if (distance > m_fMaxDistance)
			return false;

		bool isControlled = (m_NearestTower.faction == factionIndex);

		return (isControlled == m_bRequireControl);
	}

	//------------------------------------------------------------------------------------------------
	//! Creation gate: asked by the deployment evaluator about a candidate position, before any
	//! deployment exists.
	//! \param[in] position The candidate position.
	//! \param[in] factionIndex The faction the evaluator is deploying for.
	//! \param[in] threatLevel The candidate's scored threat. Unused - a tower is worth garrisoning
	//!            whether or not anything has happened near it.
	//! \return True when the tower's control matches what this module was authored to require.
	override bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;

		OVT_RadioTowerData nearestTower = occupyingFaction.GetNearestRadioTower(position);
		if (!nearestTower)
			return false;

		float distance = vector.Distance(position, nearestTower.location);
		if (distance > m_fMaxDistance)
			return false;

		bool isControlled = (nearestTower.faction == factionIndex);

		return (isControlled == m_bRequireControl);
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here. A module is cloned out of its config template for each
	//! deployment and CloneModule copies by hand, so a forgotten attribute silently ships the class
	//! default instead of the authored value - that is how m_fMaxCruiseSpeed was lost on the vehicle
	//! module for a whole release. The resolved tower is NOT copied: it is a cache of the last
	//! evaluation, and a clone has not evaluated anything yet.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_RadioTowerControlConditionDeploymentModule clone = new OVT_RadioTowerControlConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxDistance = m_fMaxDistance;
		clone.m_bRequireControl = m_bRequireControl;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The tower the last EvaluateCondition() resolved, or null when it has not run or found
	//!         nothing.
	OVT_RadioTowerData GetNearestRadioTower()
	{
		return m_NearestTower;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Radio Tower Control Condition Module: %1", m_sModuleName));
		Print(string.Format("  Max Distance: %1m", m_fMaxDistance));
		string requireControl = "No";
		if (m_bRequireControl)
			requireControl = "Yes";
		Print(string.Format("  Require Control: %1", requireControl));

		if (m_NearestTower)
		{
			Print(string.Format("  Tower Faction: %1", m_NearestTower.faction));
		}
	}
}
