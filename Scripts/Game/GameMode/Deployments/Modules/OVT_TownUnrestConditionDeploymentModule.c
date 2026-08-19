//------------------------------------------------------------------------------------------------
//! Town unrest condition for deployments: "is there a town this position's radio tower is talking to
//! that we still hold but are visibly losing?"
//!
//! WHAT IT ANSWERS, IN ONE SENTENCE: at least one town within radio range of the nearest tower is big
//! enough to matter, is still flying the occupying faction's flag, and has more than half its
//! population supporting the resistance.
//!
//! ===========================================================================================
//! WHY THIS IS A SEPARATE MODULE FROM THE TOWER-CONTROL ONE, when both gate the same deployment.
//! One module, one question, is the framework's rule and it earns its keep here: the two conditions
//! have DIFFERENT LIFETIMES. "The tower is not ours" is the thing that ends the operation - the
//! recapture flips it, the control condition turns false, the reinforcement module collects the
//! deployment - and it therefore has to be re-asked forever. "The town is in unrest" is the thing
//! that STARTED the operation, and re-asking it forever is actively wrong (see below). Folding them
//! into one class would force one lifetime onto both.
//! ===========================================================================================
//!
//! ⚠ THE RUNTIME EVALUATION DELIBERATELY DOES NOT RE-ASK THE QUESTION, which is the opposite of
//! every other condition module in the tree and is the decision most likely to look like a bug:
//!   - Support is a live, noisy number that the operation itself does not move. A team inserted at
//!     51 % and re-checked at 49 % would be deleted in flight, having already been paid for, with the
//!     tower still in enemy hands - and then re-created the moment support ticked back up. That is a
//!     resource leak with a 30 s period, not a gate.
//!   - There is no risk of a deployment that never ends, because it is not the only condition on the
//!     config. OVT_RadioTowerControlConditionDeploymentModule (m_bRequireControl 0) is still asked
//!     every reinforcement check and still collects the deployment the moment the tower is ours,
//!     which is the success path and the only ending this operation ever wanted.
//!   - m_iMaxInstances on the config is what bounds the failure path - a team that cannot take the
//!     tower ties up one slot, not the whole faction's budget.
//! EvaluateCondition() is therefore inherited (always true) rather than overridden, and this comment
//! is the reason. If a future config needs the runtime gate, add an attribute for it - do not make it
//! unconditional.
//!
//! ⚠ IT IS ANCHORED ON THE TOWER, NOT ON THE TOWN, because the deployment is. The config authors
//! m_iAllowedLocationTypes RADIO_TOWER, so every candidate position the evaluator offers is a tower;
//! the towns are what make that tower worth taking back. Reading it the other way round - a TOWN
//! candidate that goes looking for a tower - would put the specops team's spawn radius, its hold
//! radius and the recapture module's own m_fMaxDistance around the town square instead of the mast.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_TownUnrestConditionDeploymentModule : OVT_BaseConditionDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "300", desc: "Maximum distance from the candidate position to a radio tower for this condition to consider it. Matches the 300 m ring GetLocationTypeAtPosition uses to classify a position as RADIO_TOWER")]
	float m_fMaxDistance;

	//! ⚠ EXCEEDED, NOT MET. A town sitting at exactly this number does not qualify: the design brief
	//! is "over 50 % support", and a >= here would fire the operation on a town that is precisely
	//! evenly split, which is the one state that has not actually turned against the faction yet.
	[Attribute(defvalue: "50", desc: "A town must EXCEED this resistance-support percentage to justify the operation. 50 = the design's 'over half the town'")]
	int m_iMinSupportPercent;

	//! Authored as the raw OVT_TownSize integer rather than an enum attribute, matching how
	//! OVT_ObjectiveConditionDeploymentModule authors its phase numbers.
	[Attribute(defvalue: "2", desc: "Smallest town size that counts, as OVT_TownSize: 1 VILLAGE, 2 TOWN, 3 CITY, 4 CAPITAL. The default of 2 is the design's 'a town or city, not a village'")]
	int m_iMinTownSize;

	//------------------------------------------------------------------------------------------------
	//! Creation gate: asked by the deployment evaluator about a candidate tower position, before any
	//! deployment exists.
	//! \param[in] position The candidate position - a radio tower, per the config's location types.
	//! \param[in] factionIndex The faction the evaluator is deploying for.
	//! \param[in] threatLevel The candidate's scored threat. Unused: a town's politics are the reason
	//!            for this operation, and whether anything has been shot near the mast is not.
	//! \return True when at least one qualifying town is in this tower's influence.
	override bool EvaluateStaticCondition(vector position, int factionIndex, float threatLevel)
	{
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;

		OVT_RadioTowerData nearestTower = occupyingFaction.GetNearestRadioTower(position);
		if (!nearestTower)
			return false;

		if (vector.Distance(position, nearestTower.location) > m_fMaxDistance)
			return false;

		return HasUnrestInRange(nearestTower.location, factionIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether any town this tower reaches is one the faction still holds and is visibly losing.
	//!
	//! ⚠ THE RANGE IS THE CAMPAIGN'S OWN radioTowerRange AND IS READ FRESH, not an attribute of this
	//! module. A tower's reach is a difficulty setting that the whole support/stability economy is
	//! computed from - OVT_OccupyingFactionManager.GetRadioTowersAffecting() reads exactly this number
	//! - and a second, separately authored copy here would let the deployment fire on towns the tower
	//! is not actually influencing. One number, one meaning.
	//!
	//! ⚠ IsProximitySource IS THE SAME PREDICATE THE MODIFIER SYSTEM USES, called with the arguments
	//! the other way round. It is a plain symmetric distance test, so "is this town in the tower's
	//! range" and "is this tower in the town's range" are the same question - but it is called through
	//! the shared rule rather than open-coded so that a future change to what "in range" means (an
	//! inclusive boundary, a falloff) moves this with it.
	//! \param[in] towerPosition The mast.
	//! \param[in] factionIndex The faction the deployment belongs to - the one that must still hold
	//!            the town for it to be worth defending politically.
	//! \return True on the first qualifying town.
	protected bool HasUnrestInRange(vector towerPosition, int factionIndex)
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		float range = difficulty.radioTowerRange;
		if (range <= 0)
			return false;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return false;

		array<ref OVT_TownData> allTowns = towns.GetTowns();
		if (!allTowns)
			return false;

		foreach (OVT_TownData town : allTowns)
		{
			if (!town)
				continue;

			// Villages are excluded by size, which is the design's own line: they are too small for
			// their politics to be worth a specops team, and they are not objectives either.
			if (town.size < m_iMinTownSize)
				continue;

			// Still ours. A town the resistance has already taken is not a town we are losing - it is
			// one we have lost, and taking a radio tower back does not undo that.
			if (town.faction != factionIndex)
				continue;

			if (town.SupportPercentage() <= m_iMinSupportPercent)
				continue;

			if (!OVT_InfluenceRules.IsProximitySource(town.location, towerPosition, range))
				continue;

			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here. A module is cloned out of its config template for each
	//! deployment and CloneModule copies by hand, so a forgotten attribute silently ships the class
	//! default instead of the authored value - that is how m_fMaxCruiseSpeed was lost on the vehicle
	//! module for a whole release. What a dropped line would cost here: drop m_iMinSupportPercent and
	//! the clone reads 0, so every town in range qualifies and the operation fires on a contented
	//! town; drop m_iMinTownSize and villages come back.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_TownUnrestConditionDeploymentModule clone = new OVT_TownUnrestConditionDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxDistance = m_fMaxDistance;
		clone.m_iMinSupportPercent = m_iMinSupportPercent;
		clone.m_iMinTownSize = m_iMinTownSize;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Town Unrest Condition Module: %1", m_sModuleName));
		Print(string.Format("  Max Distance: %1m", m_fMaxDistance));
		Print(string.Format("  Support must exceed: %1%%", m_iMinSupportPercent));
		Print(string.Format("  Minimum town size: %1", m_iMinTownSize));
	}
}
