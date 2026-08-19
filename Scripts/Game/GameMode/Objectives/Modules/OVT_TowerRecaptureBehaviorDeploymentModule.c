//------------------------------------------------------------------------------------------------
//! TOWER RECAPTURE: the INVERSE of OVT_RadioTowerCaptureBehaviorDeploymentModule, and the closing of
//! a regression the base-defense migration opened.
//!
//! Until this module the occupying faction had NO way to take a radio tower back. The old specops
//! base upgrade did it and was retired with the whole BaseUpgrades/ directory, so towers only ever
//! flowed one way: to the resistance, permanently. That is what this restores, and it restores it as
//! a deployment the resistance can see coming and fight, rather than as a timer on a base menu.
//!
//! ================== HOW IT DIFFERS FROM THE MODULE IT MIRRORS ==============================
//!   OVT_RadioTowerCaptureBehaviorDeploymentModule   the garrison is WIPED    -> the tower flips,
//!                                                   on a single edge, instantly.
//!   THIS MODULE                                     the tower is HELD, by living men, with no
//!                                                   player standing on it, for the whole of
//!                                                   objectiveTowerRecaptureHoldSeconds (600 s on
//!                                                   Normal) -> then it flips.
//! Losing something should be instant and taking something should be slow: the resistance gets ten
//! minutes of warning during which killing the team, or simply standing at the tower, stops it.
//! ⚠ THE HOLD TIMER IS NEW CODE. The module this mirrors has no timer of any kind - see the plan's
//! correction C5, which exists because the requirements said this mechanic was inherited and it is not.
//! ==========================================================================================
//!
//! ⚠ THE CLOCK PAUSES, IT NEVER RESETS, for the same reason the town harassment module's does: a
//! defender who walks past every few minutes would otherwise make the tower permanently unrecapturable
//! while never actually contesting it.
//!
//! ⚠ AUTHOR THIS MODULE BEFORE THE REINFORCEMENT MODULE IN A CONFIG'S m_aModules - the ordering
//! constraint OVT_RadioTowerCaptureBehaviorDeploymentModule's own header records, and here it closes
//! the loop rather than merely avoiding a rebuy: the flip is what makes
//! OVT_RadioTowerControlConditionDeploymentModule (authored with m_bRequireControl 0, "deploy only
//! while we do NOT hold this tower") start answering false, and the reinforcement module's
//! m_bDeleteOnConditionFail is what then collects this deployment. Ordered after it, the flip would be
//! considered in a pass that had already decided the deployment was fine, and the team would sit at a
//! tower it already owned until the objective ended.
//!
//! THE NOTIFICATION AND THE BROADCAST ARE NOT HERE. ChangeRadioTowerControl already sends the text
//! notification, the external notification and the RPC that moves the marker on every client's map.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_TowerRecaptureBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "300", desc: "Maximum distance from the deployment to the tower it may recapture. Matches the 300 m ring GetLocationTypeAtPosition uses to classify a position as RADIO_TOWER")]
	float m_fMaxDistance;

	[Attribute(defvalue: "80", desc: "How close to the tower a group has to be to count as holding it. Also the circle a player has to be inside to interrupt the hold")]
	float m_fHoldRadius;

	[Attribute(defvalue: "600", desc: "Seconds the tower must be held before it changes hands. FALLBACK ONLY - the campaign's objectiveTowerRecaptureHoldSeconds is used whenever difficulty settings are loaded")]
	int m_iHoldSeconds;

	//! One deployment update, in seconds. See OVT_TownHarassmentBehaviorDeploymentModule.UPDATE_SECONDS.
	static const int UPDATE_SECONDS = 10;

	//! Fired-once latch. NOT an attribute, NOT persisted and NOT copied by CloneModule - a clone is a
	//! fresh deployment's module and has captured nothing. Not persisting it is safe for the same
	//! reason the module this mirrors gives: on the only path a restored deployment could reach the
	//! flip again the tower is already ours, and the faction guard in EvaluateRecapture refuses it.
	protected bool m_bCaptureFired;

	protected int m_iTicksLeft;

	protected bool m_bArmed;

	//------------------------------------------------------------------------------------------------
	//! One observation of the hold.
	//! \param[in] deltaTime Milliseconds nominally elapsed. Unused - the hold is counted in updates.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (m_bCaptureFired || !m_ParentDeployment)
			return;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return;

		vector deploymentPos = m_ParentDeployment.GetPosition();

		OVT_RadioTowerData tower = occupying.GetNearestRadioTower(deploymentPos);
		if (!tower)
			return;

		// GetNearestRadioTower answers with the nearest tower at ANY distance, so the range test is what
		// stops a team that ended up somewhere unexpected flipping something on the far side of the map.
		if (vector.Distance(deploymentPos, tower.location) > m_fMaxDistance)
			return;

		if (!m_bArmed)
		{
			m_iTicksLeft = ResolveHoldTicks();
			m_bArmed = true;
		}

		int myFaction = m_ParentDeployment.GetControllingFaction();

		bool holding = CountAliveRegisteredMembersWithin(tower.location, m_fHoldRadius) >= 1;
		bool enemyPresent = NearestPlayerDistance(tower.location) <= m_fHoldRadius;

		if (!EvaluateRecapture(holding, enemyPresent, tower.faction, myFaction, m_iTicksLeft))
			return;

		occupying.ChangeRadioTowerControl(tower, myFaction);

		Print(string.Format("[Overthrow] A recapture team held the radio tower at %1 and it changes hands",
			tower.location.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! THE RECAPTURE DECISION, with every input passed in.
	//!
	//! SPLIT OUT FROM OnUpdate ON PURPOSE, the OVT_RadioTowerCaptureBehaviorDeploymentModule pattern:
	//! the claim worth asserting - "it takes the whole hold, an interruption pauses it, and it fires
	//! exactly once" - is answerable directly with five arguments and needs no live deployment marker,
	//! no tower and no AI.
	//!
	//! \param[in] holding Whether at least one living member of this force is at the tower.
	//! \param[in] enemyPresent Whether a player is standing at the tower.
	//! \param[in] towerFaction Which faction currently holds the tower.
	//! \param[in] myFaction This deployment's controlling faction.
	//! \param[inout] ticksLeft Updates still owed. PAUSED, never reset, on an interrupted tick.
	//! \return True when THIS call completed the recapture. False on every other path, including a
	//!         second call after a successful one.
	bool EvaluateRecapture(bool holding, bool enemyPresent, int towerFaction, int myFaction, inout int ticksLeft)
	{
		if (m_bCaptureFired)
			return false;

		// ⚠ THE TOWER MUST NOT ALREADY BE OURS. This is the INVERSE of the guard the module this mirrors
		// carries, and it does the same job: somebody else may have flipped it while the team was
		// walking there (another recapture deployment, a Game Master), and without this the hold would
		// run to completion and "capture" a tower the faction already owned. ChangeRadioTowerControl
		// early-returns on an unchanged faction, so the cost of getting it wrong is only a latch spent
		// and a misleading log line - which is exactly the kind of thing nobody ever traces.
		if (towerFaction == myFaction)
			return false;

		if (!holding)
			return false;

		if (enemyPresent)
			return false;

		// ⚠ A NON-POSITIVE HOLD MUST STILL TAKE ONE TICK, so a misauthored zero cannot flip a tower on
		// the update the team is registered.
		if (ticksLeft > 0)
			ticksLeft = ticksLeft - 1;

		if (ticksLeft > 0)
			return false;

		// Latch BEFORE the caller acts. ChangeRadioTowerControl notifies and broadcasts, and a
		// re-entrant path through either would otherwise find the latch still down.
		m_bCaptureFired = true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many updates the hold is worth, difficulty first and the authored attribute as the fallback.
	//! \return At least one update.
	protected int ResolveHoldTicks()
	{
		int seconds = m_iHoldSeconds;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty && difficulty.objectiveTowerRecaptureHoldSeconds > 0)
			seconds = difficulty.objectiveTowerRecaptureHoldSeconds;

		if (seconds < UPDATE_SECONDS)
			return 1;

		return seconds / UPDATE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here, and the latch and the armed timer must NOT: a clone belongs
	//! to a different deployment and has captured nothing.
	//!
	//! What a dropped line would cost here: drop m_fHoldRadius and it clones as 0, so nothing ever
	//! counts as holding the tower and no tower is ever retaken - the regression this module exists to
	//! close, silently reopened; drop m_iHoldSeconds and a world with no difficulty settings loaded
	//! flips towers on the first update.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_TowerRecaptureBehaviorDeploymentModule clone = new OVT_TowerRecaptureBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxDistance = m_fMaxDistance;
		clone.m_fHoldRadius = m_fHoldRadius;
		clone.m_iHoldSeconds = m_iHoldSeconds;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this module has already flipped its tower.
	bool HasCaptureFired()
	{
		return m_bCaptureFired;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Updates still owed on the hold, or 0 before the first update arms it.
	int GetHoldTicksLeft()
	{
		return m_iTicksLeft;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Tower Recapture Behavior Module: %1", m_sModuleName));
		Print(string.Format("  Max Distance: %1m  Hold Radius: %2m", m_fMaxDistance, m_fHoldRadius));
		Print(string.Format("  Hold: %1 update(s) left", m_iTicksLeft));
		string fired = "No";
		if (m_bCaptureFired)
			fired = "Yes";
		Print(string.Format("  Already Recaptured: %1", fired));
	}
}
