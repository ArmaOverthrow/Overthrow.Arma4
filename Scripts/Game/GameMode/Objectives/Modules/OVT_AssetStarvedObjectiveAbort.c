//------------------------------------------------------------------------------------------------
//! THE RESISTANCE'S COUNTERPLAY, AS AN AUTHORED ABORT: a standing asset that has been cut off for long
//! enough is abandoned, and the objective with it.
//!
//! The port of the hard-coded forward-base starvation rule. The arithmetic is unchanged and still lives
//! in OVT_ObjectivePhaseRules.IsFOBStarved(): THREE INPUTS, ANY ONE OF WHICH CUTS THE ASSET OFF, and
//! each is separately sufficient because each separately means the same thing - nothing more is
//! arriving.
//!
//!   1. THE BASE SUPPLYING IT HAS BEEN TAKEN.
//!   2. ITS OWN GARRISON IS DEAD.
//!   3. THE RESISTANCE IS STANDING ON IT.
//!
//! 🔴 THE PRESENCE INPUT IS MEASURED AT THE FORWARD BASE ITSELF, NOT AT THE BASE THAT SUPPLIES IT, AND
//! THE REQUIREMENTS-ERA PROSE THAT SAYS OTHERWISE IS WRONG (C4). The shipped rule reads
//! OVT_WorldUtils.PlayerInRange(<the asset's own position>, difficulty.baseCloseRange) and the code is
//! the parity reference. "The resistance is standing on it" is the third way to cut a forward base off:
//! a player at the flag is interdicting the thing itself. Measured at the source base instead, a player
//! could sit on the forward base indefinitely with no effect, and clearing a base a kilometre away
//! would starve one that was perfectly supplied. DO NOT "FIX" THIS.
//!
//! 🔴 THE GARRISON IS COUNTED THROUGH REGISTERED HANDLES AND THE SURVIVOR MASK, NEVER THROUGH SPAWNED
//! ENTITIES. A dormant group reports zero agents while being perfectly alive, so a count of bodies
//! would starve every forward base on the map the moment the last player drove away - which is exactly
//! when one is supposed to be quietly doing its job. That is the OPPOSITE of what the dismantle rule
//! counts (see OVT_RaiseForwardBaseObjectiveOperation.CountOccupyingDefendersNear), on purpose: that one
//! asks "is anybody shooting at me", which is a question about materialised bodies.
//!
//! ⚠ THE COUNTER IS SERVED IN OnTick() AND ONLY READ IN ShouldAbort(). The runner folds the abort
//! modules TWICE on a tick that reaches the end - once before anything is spent and once after the idle
//! clock is served - so a ShouldAbort() that incremented would count every such tick twice. OnTick()
//! runs exactly once.
//!
//! ⚠ RECOVERY ZEROES THE COUNT rather than pausing it. A forward base that was cut off for ten minutes
//! and then relieved is not half-dead, it is supplied.
//!
//! ⚠ BOTH TRANSITIONS ARE LOGGED, ONCE EACH. "The occupying faction's forward base disappeared" has to
//! have an explanation in the log, and so does "it survived": a latch rather than a line per in-game
//! minute, because this runs for as long as the phase does.
//!
//! ⚠ NO PENALTY. Starvation is the occupying faction giving up on a supply line it could not hold, not
//! the resistance taking the base off it - only the player-initiated dismantle is paid for.
//!
//! ⚠ IT RUNS THROUGH A DAYLIGHT WAIT. See OVT_DaylightWindowObjectiveCondition: the wait holds the idle
//! clock and nothing else, so a base cut off at 22:00 comes down at 22:00.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_AssetStarvedObjectiveAbort : OVT_BaseObjectiveAbortModule
{
	[Attribute(defvalue: "fob", desc: "Which standing asset can be starved. 'fob' is the forward operating base. A later asset adds a key rather than an abort class")]
	string m_sAssetKey;

	[Attribute(defvalue: "-1", desc: "Consecutive in-game minutes cut off before the objective is abandoned. -1 = the campaign's objectiveStarvationMinutes difficulty setting (Easy 45 down to Insane 15). 0 or below disables the abort entirely, which is a supported authoring gesture for a doctrine whose asset cannot be interdicted")]
	int m_iStarvationMinutes;

	[Attribute(defvalue: "250", desc: "How far from the asset a deployment's registered groups count as ITS garrison, in metres. Matches OVT_RaiseForwardBaseObjectiveOperation.AREA_RADIUS and the garrison sender's own concurrency radius, so all three agree about what 'at the forward base' means. ⚠ A .conf cannot reference a constant, so this number is authored in three places")]
	float m_fAreaRadius;

	//! How close to the recorded supplying base a real base has to be for the record to be believed, in
	//! metres. A recorded position with no base within this is a base that has gone, which is the first
	//! starvation input.
	static const float SOURCE_MATCH_RADIUS = 100;

	//! True while the asset is currently reported as cut off, so the two transitions are each logged
	//! once rather than once per in-game minute. A LOG LATCH and nothing else - the machine reads the
	//! counter on the record, never this.
	protected bool m_bStarvationLogged;

	//------------------------------------------------------------------------------------------------
	//! ⚠ THE LATCH IS DROPPED ON EVERY ENTRY. A second objective running the same plan must be able to
	//! say the line again.
	override protected void OnEnter()
	{
		m_bStarvationLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! One tick of the starvation rule: serve the counter, and log the two transitions.
	//!
	//! ⚠ IT DECIDES NOTHING. The verdict is ShouldAbort()'s, which the runner asks immediately after
	//! this and again after the idle clock. Splitting them is what makes the double fold free.
	override protected void OnTick()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return;

		OVT_ObjectiveAssetRecord asset = objective.GetAsset(m_sAssetKey);
		if (!asset || !asset.up)
			return;

		bool starved = OVT_ObjectivePhaseRules.IsFOBStarved(IsSourceBaseHeld(asset), CountAliveGroups(asset), IsPlayerAtAsset(asset));

		if (!starved)
		{
			if (m_bStarvationLogged)
			{
				m_bStarvationLogged = false;
				Print(OVT_ObjectiveDirectorComponent.LOG + "The forward base for objective '" + objective.GetTargetName() + "' is supplied again after " + asset.starvationTicks.ToString() + " in-game minute(s) cut off", LogLevel.NORMAL);
			}

			asset.starvationTicks = 0;
			return;
		}

		asset.starvationTicks = asset.starvationTicks + 1;

		if (!m_bStarvationLogged)
		{
			m_bStarvationLogged = true;
			Print(OVT_ObjectiveDirectorComponent.LOG + "The forward base for objective '" + objective.GetTargetName() + "' is cut off - it has " + ResolveStarvationMinutes().ToString() + " in-game minute(s) before it is abandoned", LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] reason The sentence for the campaign log, written only when this returns true.
	//! \param[out] blacklist Whether the objective's PLACE should sit out a selection round.
	//! \return True when the asset has been cut off for its whole budget.
	override bool ShouldAbort(out string reason, out bool blacklist)
	{
		reason = "";
		blacklist = false;

		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		OVT_ObjectiveAssetRecord asset = objective.GetAsset(m_sAssetKey);
		if (!asset || !asset.up)
			return false;

		int budget = ResolveStarvationMinutes();
		if (budget <= 0)
			return false;

		if (asset.starvationTicks < budget)
			return false;

		reason = "its forward base was cut off for " + asset.starvationTicks.ToString() + " in-game minutes and has been abandoned";

		// ⚠ THE PLACE SITS OUT A ROUND. The occupying faction could not hold a supply line to it, and
		// picking it again on the very next round would fail the same way.
		blacklist = true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The budget, authored or from difficulty.
	//! \return Consecutive in-game minutes cut off before the objective is abandoned.
	int ResolveStarvationMinutes()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyValue = difficulty.objectiveStarvationMinutes;

		return OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iStarvationMinutes, difficultyValue);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the base supplying the asset is still the occupying faction's.
	//! \param[in] asset The asset's record.
	//! \return True when it is.
	bool IsSourceBaseHeld(notnull OVT_ObjectiveAssetRecord asset)
	{
		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if (!occupying || !config)
			return false;

		// An asset with no recorded source is one raised by an insertion that walked in without
		// resolving an origin. It is not starving for want of a base it never had.
		if (asset.sourceBasePosition == vector.Zero)
			return true;

		OVT_BaseData base = occupying.GetNearestBase(asset.sourceBasePosition);
		if (!base)
			return false;

		if (vector.Distance(base.location, asset.sourceBasePosition) > SOURCE_MATCH_RADIUS)
			return false;

		return base.faction == config.GetOccupyingFactionIndex();
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the occupying faction's registered groups are alive at the asset.
	//!
	//! ⚠ COUNTED THROUGH HANDLES AND THE SURVIVOR MASK, NEVER THROUGH SPAWNED ENTITIES. See the class
	//! header - a dormant group is alive and reports no agents.
	//! \param[in] asset The asset's record.
	//! \return The number of alive registered groups.
	int CountAliveGroups(notnull OVT_ObjectiveAssetRecord asset)
	{
		// ⚠ MOVED, NOT CHANGED (2026-08-21). The body of this method is now
		// OVT_ObjectiveDirectorComponent.CountAliveOccupyingGroupsAt(), because the anchor source
		// provider has to ask the identical question - "is this forward base still manned" - and two
		// copies of a rule this specific drift. m_fAreaRadius stays authored here; the counting does not.
		return OVT_ObjectiveDirectorComponent.CountAliveOccupyingGroupsAt(asset.position, m_fAreaRadius);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the resistance is standing on the asset.
	//!
	//! 🔴 AT THE ASSET, NOT AT THE BASE THAT SUPPLIES IT (C4). See the class header.
	//!
	//! ⚠ ANY LIVING RESISTANCE-FACTION CHARACTER, not players only (OVT_ResistancePresence - a player,
	//! a recruit or a High Command group's rifleman all interdict it equally), and the difficulty's own
	//! baseCloseRange, which is the distance the rest of the campaign already means by "at a base". It is
	//! deliberately NOT an objective-specific radius: R5 forbids a new difficulty knob and this is what
	//! the campaign already means.
	//! \param[in] asset The asset's record.
	//! \return True when somebody is there.
	bool IsPlayerAtAsset(notnull OVT_ObjectiveAssetRecord asset)
	{
		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (!difficulty)
			return false;

		return OVT_ResistancePresence.IsGroundHeld(asset.position, difficulty.baseCloseRange);
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the counter lives on the ASSET RECORD under its key, not in the bag.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_sAssetKey and every clone asks about the empty key, so no
	//! asset is ever found and the forward base can never be starved - the resistance's whole counterplay
	//! is gone with no symptom; drop m_iStarvationMinutes and the clone reads 0, which disables the abort
	//! for the same result; drop m_fAreaRadius and the clone reads 0, so no deployment is ever inside it,
	//! the garrison always counts as dead and EVERY forward base starves out on schedule.
	//!
	//! ⚠ THE LOG LATCH IS NOT COPIED. A clone is a fresh module for a fresh phase entry and has said
	//! nothing yet.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_AssetStarvedObjectiveAbort clone = new OVT_AssetStarvedObjectiveAbort();

		clone.m_sModuleName = m_sModuleName;
		clone.m_sAssetKey = m_sAssetKey;
		clone.m_iStarvationMinutes = m_iStarvationMinutes;
		clone.m_fAreaRadius = m_fAreaRadius;

		return clone;
	}
}
