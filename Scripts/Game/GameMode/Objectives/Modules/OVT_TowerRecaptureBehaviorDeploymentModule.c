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

	//! =============================================================================================
	//! ⚠ THIS ONE KEEPS THE OLD "FALLBACK ONLY" CONVENTION, AND THE INCONSISTENCY IS DELIBERATE
	//! (occupying/objectives build phase 4, 2026-08-21).
	//! =============================================================================================
	//! The two purely objective-side behaviour modules - town harassment and base sabotage - were
	//! flipped to "-1 = ask the campaign" and their configs re-authored to -1 in the same change, which
	//! is what makes a flip behaviour-neutral. THIS MODULE COULD NOT BE, because it is authored in TWO
	//! configs and only one of them is objective doctrine:
	//!
	//!   Deployment_ObjectiveTowerRecapture.conf   the ramp's recapture operation  (this feature's)
	//!   Deployment_TowerRecaptureUnrest.conf      the standalone unrest response  (NOT this feature's)
	//!
	//! Flipping the convention while leaving the second config holding its authored 600 would HONOUR
	//! that 600 instead of overriding it, so the unrest recapture would take 600 s on Easy (where the
	//! campaign says 900) and 600 s on Insane (where it says 300) - a real behaviour change to a shipped
	//! deployment this feature is not allowed to touch. Re-authoring that config instead is the same
	//! statement made in a file this feature must leave byte-identical.
	//!
	//! ⚠ SO THE RESIDUAL INCONSISTENCY IS ACCEPTED AND RECORDED RATHER THAN PAPERED OVER. Whoever
	//! separates the two configs - or decides the unrest recapture should scale with difficulty like
	//! everything else - can flip this in one commit with both configs. The same exclusion, for the same
	//! reason, applies to OVT_BaseRepairBehaviorDeploymentModule, which left for the deployments
	//! framework as a pure relocation.
	[Attribute(defvalue: "600", desc: "Seconds the tower must be held before it changes hands. FALLBACK ONLY - the campaign's objectiveTowerRecaptureHoldSeconds is used whenever difficulty settings are loaded. ⚠ Deliberately NOT on the -1 convention the other objective behaviour modules use - see the note above this attribute")]
	int m_iHoldSeconds;

	//! HOW CLOSE THE TEAM HAS TO GET BEFORE THE PLAYER IS TOLD THEY ARE COMING.
	//!
	//! 300 m is the insertion module's own m_fLZStandoffDistance on every objective config that carries
	//! a recapture team, so the warning lands as the team is put down rather than while it is still
	//! driving - which is what the author asked for: "it should trigger when they are almost at the
	//! location (about the distance the insertion would drop them)".
	//!
	//! ⚠ IT IS DELIBERATELY WIDER THAN m_fHoldRadius (80). The hold radius answers "are they ON the
	//! tower", which is far too late to be a warning - by then the 600-second clock has already started.
	//! A player told at 300 m has the whole hold to respond in, which is the entire point of telling them.
	[Attribute(defvalue: "300", desc: "How close a team has to get to the tower before the resistance is warned they are coming. Roughly the insertion's drop distance, so the warning lands as they are put down")]
	float m_fApproachWarningRadius;

	//! One deployment update, in seconds. See OVT_TownHarassmentBehaviorDeploymentModule.UPDATE_SECONDS.
	static const int UPDATE_SECONDS = 10;

	//! Fired-once latch. NOT an attribute, NOT persisted and NOT copied by CloneModule - a clone is a
	//! fresh deployment's module and has captured nothing. Not persisting it is safe for the same
	//! reason the module this mirrors gives: on the only path a restored deployment could reach the
	//! flip again the tower is already ours, and the faction guard in EvaluateRecapture refuses it.
	protected bool m_bCaptureFired;

	//! Announced-once latch for the approach warning. Same rules as m_bCaptureFired: not an attribute,
	//! not persisted, NOT copied by CloneModule. Without it the warning would repeat every update for as
	//! long as the team stood near the tower - once every ten seconds, for the whole ten-minute hold.
	protected bool m_bApproachAnnounced;

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

		// 🔴 THE WARNING THE MIGRATION LOST. "The enemy is attempting to capture a radio tower near %1"
		// is authored in overthrowBroadcastMessages.conf as the RadioTowerCapture preset and localised
		// in three languages - and after virtualization/base-defense-migration deleted
		// OVT_BaseUpgradeSpecops NOTHING SENT IT. The preset and its strings sat orphaned; a recapture
		// team simply arrived unannounced, which is what the author noticed (2026-08-20): "a specops
		// team just arrived at it (good) but no notification".
		//
		// ⚠ IT IS SENT ON APPROACH, NOT ON ARRIVAL AT THE TOWER, and that is the whole value of it. The
		// hold radius is 80 m and the hold is ten minutes; a warning fired at 80 m tells the player
		// something is happening at the moment it is already happening. At the drop distance they get
		// the entire hold to do something about it.
		WarnOnApproach(tower);

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
	//! Tells everyone, ONCE, that a team has reached the tower it means to take.
	//!
	//! ⚠ IT COUNTS THE TEAM, NOT THE DEPLOYMENT MARKER. The marker is created at the tower and never
	//! moves, so measuring from it would fire the warning the instant the operation was bought - while
	//! the men were still at the source base, possibly a 2.4 km drive away. CountAliveRegisteredMembersWithin
	//! asks where the MEN are, which is the only thing that makes "they are almost here" true.
	//!
	//! ⚠ ALIVE MEMBERS, so a team wiped out on the way never announces itself. That falls out of using
	//! the same counter the hold does rather than being a separate rule.
	//!
	//! THE TOWN NAME IS THE TOWER'S NEAREST, matching every other radio-tower notification in the
	//! campaign (RadioTowerControlledResistance, RadioTowerSabotaged, RadioTowerRepaired all resolve the
	//! same way), so a player reading two of them about one tower sees one name.
	//! \param[in] tower The tower this team is going for.
	protected void WarnOnApproach(notnull OVT_RadioTowerData tower)
	{
		if (m_bApproachAnnounced)
			return;

		if (CountAliveRegisteredMembersWithin(tower.location, m_fApproachWarningRadius) < 1)
			return;

		m_bApproachAnnounced = true;

		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		if (!notify)
			return;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return;

		notify.SendTextNotification("RadioTowerCapture", -1, towns.GetNearestTownName(tower.location));
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

		// ⚠ THE ATTRIBUTE IS COPIED, THE LATCH IS NOT. Dropped, m_fApproachWarningRadius clones as 0 and
		// the warning can never fire - the operation goes back to arriving unannounced, silently, which
		// is the defect this was written to fix. m_bApproachAnnounced is deliberately absent: a clone is
		// a fresh deployment's module and has announced nothing.
		clone.m_fApproachWarningRadius = m_fApproachWarningRadius;

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
