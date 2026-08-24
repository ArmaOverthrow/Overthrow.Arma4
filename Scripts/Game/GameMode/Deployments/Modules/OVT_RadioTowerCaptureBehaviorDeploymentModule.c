//------------------------------------------------------------------------------------------------
//! Tower capture behavior: when this deployment's whole force has been WIPED, the radio tower it was
//! garrisoning changes hands - once.
//!
//! WHAT THIS REPLACED, AND WHY IT IS SAFER. Capture used to be a side effect of despawn bookkeeping
//! inside the occupying faction manager's 9 s tower loop: the loop dropped garrison entity ids whose
//! entity had vanished or whose group reported zero agents, and if the list happened to be empty
//! afterwards it flipped the tower. Two ways that was wrong:
//!   - under 1.8 a perfectly alive DORMANT or spawn-queued group reports ZERO agents, so a garrison
//!     nobody was standing next to counted as dead;
//!   - the same loop deleted the whole garrison whenever no player was within 1750 m (and, for good
//!     measure, whenever any QRF was running anywhere on the map), which emptied the list by design.
//!
//! What drives it now is the deployment's eliminated flag, and that flag is set from EXACTLY one
//! place: the virtualization core's group-wiped invoker, which fires only when a group's survivor
//! mask reports every slot dead. A proximity despawn cannot set it, so a proximity despawn cannot
//! capture a tower. That is this module's whole safety argument and it is asserted by an Init-tier
//! case, not assumed.
//!
//! EDGE-LATCHED. The flag stays set, and this module is asked again every ~10 s update, so without a
//! latch a captured tower would be "captured" forever - re-notifying every tick. The latch fires
//! once per module instance, and a module instance belongs to one deployment.
//!
//! THE NOTIFICATION AND THE BROADCAST ARE NOT HERE. ChangeRadioTowerControl already sends the
//! "tower controlled by resistance" text notification, the external (Discord) notification and the
//! RPC that moves the marker on every client's map. Duplicating any of that here would double it.
//!
//! ⚠ AUTHOR THIS MODULE BEFORE THE REINFORCEMENT MODULE IN A CONFIG'S m_aModules. Behavior modules
//! are cloned and updated in authored order, and the reinforcement module's condition check is what
//! deletes the deployment once the tower is lost. Ordered after it, this module would be asked in
//! the same pass that had already decided the deployment was over - and on a config that could
//! afford a rebuy, the garrison would be bought back before the capture was ever considered, so a
//! player who cleared a tower would watch it refill and never take it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_RadioTowerCaptureBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "300", desc: "Maximum distance from the deployment to the tower it may capture. A deployment further than this from any tower captures nothing")]
	float m_fMaxDistance;

	//! Fired-once latch. NOT an attribute, NOT persisted and NOT copied by CloneModule - a clone is a
	//! fresh deployment's module and has captured nothing.
	//!
	//! Not persisting it is deliberate rather than an omission: the only way a restored deployment
	//! could reach the capture path again is by being restored with its force already eliminated,
	//! and in that case the tower it captured is already resistance-held, which the faction guard in
	//! TryCapture() refuses. A latch in a save file would be a second source of truth for something
	//! the world already knows.
	protected bool m_bCaptureFired;

	//------------------------------------------------------------------------------------------------
	//! Asked every deployment update (~10 s). Cheaper than the 9 s poll it replaced, and it asks a
	//! question with a real answer instead of counting agents.
	//! \param[in] deltaTime Milliseconds nominally elapsed. Unused - the eliminated flag is an edge,
	//!            not a duration.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (!m_ParentDeployment)
			return;

		EvaluateCapture(m_ParentDeployment.GetSpawnedUnitsEliminated(), m_ParentDeployment.GetPosition(),
			m_ParentDeployment.GetControllingFaction());
	}

	//------------------------------------------------------------------------------------------------
	//! The capture decision, with every input passed in.
	//!
	//! SPLIT OUT FROM OnUpdate ON PURPOSE. The claim this module exists to make - "fires only on a
	//! real wipe, and then exactly once" - is worth asserting, and asserting it through OnUpdate
	//! would mean standing up a live deployment marker entity, which leaks a repeating 10 s update
	//! into whatever world the assertion runs in. With the three inputs as parameters the decision is
	//! answerable directly, against the real occupying faction manager and the real towers.
	//! \param[in] eliminated Whether the parent deployment's whole force has been wiped. Set only
	//!            from the virtualization core's group-wiped bookkeeping - never from a despawn.
	//! \param[in] deploymentPos Where the deployment is, used to find the tower it garrisons.
	//! \param[in] factionIndex The deployment's controlling faction - the faction that must still
	//!            hold the tower for this deployment to have anything to lose.
	//! \return True when THIS call flipped the tower. False on every other path, including a second
	//!         call after a successful one.
	bool EvaluateCapture(bool eliminated, vector deploymentPos, int factionIndex)
	{
		// The gate that makes the whole design safe: no wipe, no capture. A garrison that merely
		// went dormant because the last player drove away never reaches the line below.
		if (!eliminated)
			return false;

		if (m_bCaptureFired)
			return false;

		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (!occupyingFaction)
			return false;

		OVT_RadioTowerData tower = occupyingFaction.GetNearestRadioTower(deploymentPos);
		if (!tower)
			return false;

		// GetNearestRadioTower answers with the nearest tower at ANY distance, so the range test is
		// what stops a wiped deployment on the far side of the map capturing something.
		if (vector.Distance(deploymentPos, tower.location) > m_fMaxDistance)
			return false;

		// THE TOWER MUST STILL BE OURS TO LOSE. Somebody else may have taken it while this garrison
		// was being killed (a specops base upgrade does exactly that), and without this guard a
		// late-arriving capture would hand a resistance tower to the resistance again - harmless
		// today only because ChangeRadioTowerControl early-returns on an unchanged faction. Guarding
		// here means the intent is stated where it is read, not inferred from the callee.
		if (tower.faction != factionIndex)
			return false;

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		// Latch BEFORE the call. ChangeRadioTowerControl notifies and broadcasts, and a re-entrant
		// path through either would otherwise find the latch still down.
		m_bCaptureFired = true;

		occupyingFaction.ChangeRadioTowerControl(tower, config.GetPlayerFactionIndex());

		OVT_DeploymentLog.Debug(string.Format("[Overthrow] Tower garrison at %1 was wiped out - the tower changes hands",
			tower.location.ToString()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here, and the latch must NOT: a clone belongs to a different
	//! deployment and has captured nothing. (A forgotten attribute silently ships the class default
	//! instead of the authored value - D1's standing trap.)
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_RadioTowerCaptureBehaviorDeploymentModule clone = new OVT_RadioTowerCaptureBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fMaxDistance = m_fMaxDistance;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this module has already captured its tower.
	bool HasCaptureFired()
	{
		return m_bCaptureFired;
	}

	//------------------------------------------------------------------------------------------------
	void PrintDebugInfo()
	{
		Print(string.Format("Radio Tower Capture Behavior Module: %1", m_sModuleName));
		Print(string.Format("  Max Distance: %1m", m_fMaxDistance));
		string fired = "No";
		if (m_bCaptureFired)
			fired = "Yes";
		Print(string.Format("  Already Captured: %1", fired));

		if (m_ParentDeployment)
		{
			string eliminated = "No";
			if (m_ParentDeployment.GetSpawnedUnitsEliminated())
				eliminated = "Yes";
			Print(string.Format("  Force Eliminated: %1", eliminated));
		}
	}
}
