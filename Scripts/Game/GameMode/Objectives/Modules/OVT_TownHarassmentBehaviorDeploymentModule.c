//------------------------------------------------------------------------------------------------
//! TOWN HARASSMENT: a group the objective director sent has to REACH the town and HOLD it, and only
//! then does the town's support slide.
//!
//! It is the visible half of the ramp. The resistance is supposed to be able to watch this happen -
//! a truck arrives, men get out, they stand in the square, and a while later the town likes the
//! resistance less. A debuff that landed the instant a deployment was created would be a number
//! changing for no reason on screen, which is the unreadable thing this feature exists to replace.
//!
//! WHAT IT DOES, PER UPDATE (~10 s):
//!   count the living members of this deployment's force inside m_fHoldRadius of the town centre;
//!   ask whether a player is standing inside the same circle;
//!   and hand both to EvaluateHold(), which owns the whole decision.
//! At zero: the "ObjectiveHarassment" support modifier is stacked onto the town, the director's
//! success counter goes up (which buys the next rung of the group ladder), and the deployment asks
//! to be collected so the next operation has a concurrency slot.
//!
//! ⚠ THE CLOCK PAUSES, IT NEVER RESETS. A resistance patrol walking through, or a squad temporarily
//! displaced by a firefight, should DELAY the debuff - not immunise the town by rolling the timer
//! back to full every time somebody drives past. A reset would also make the whole mechanic
//! impossible to finish on a busy server, which reads in play as "the occupying faction never does
//! anything" and has no other diagnosis.
//!
//! ⚠ THE HOLD LENGTH COMES FROM DIFFICULTY BECAUSE THE SHIPPED CONFIG ASKS IT TO (-1), NOT BECAUSE
//! DIFFICULTY OUTRANKS THE CONFIG. The convention was flipped on 2026-08-21 and the config re-authored
//! in the same change - see m_iHoldSeconds. objectiveHarassmentHoldSeconds is the campaign's knob (240
//! on Easy down to 90 on Insane); a server owner who wants this one operation to differ authors a
//! number and it is honoured.
//!
//! ⚠ AUTHOR THIS MODULE BEFORE THE REINFORCEMENT MODULE IN A CONFIG'S m_aModules, the same ordering
//! constraint OVT_RadioTowerCaptureBehaviorDeploymentModule's header records. Behavior modules are
//! updated in authored order, and a mission that completes after the reinforcement module has already
//! run in the same pass would have its force rebought on the way out.
//!
//! THE SPLIT: EvaluateHold() takes its three inputs as arguments so the claim this module exists to
//! make - "the clock only runs while the force is there and unopposed, and it fires exactly once" -
//! is assertable without standing up a live deployment marker, a town, or a single AI. The pattern is
//! OVT_RadioTowerCaptureBehaviorDeploymentModule.EvaluateCapture(), including its fired-once latch and
//! its rule that the latch is NOT copied by CloneModule.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_TownHarassmentBehaviorDeploymentModule : OVT_BaseBehaviorDeploymentModule
{
	[Attribute(desc: "Name of this module")]
	string m_sModuleName;

	[Attribute(defvalue: "120", desc: "How close to the town centre a group has to be to count as holding it. Also the circle a player has to be inside to interrupt the hold")]
	float m_fHoldRadius;

	//! =============================================================================================
	//! ⚠ THE DIFFICULTY CONVENTION IS "-1 MEANS ASK THE CAMPAIGN", AND IT WAS FLIPPED HERE ON
	//! 2026-08-21 (occupying/objectives build phase 4).
	//! =============================================================================================
	//! It used to be the opposite - "FALLBACK ONLY: the campaign's value is used WHENEVER difficulty
	//! settings are loaded" - which meant an authored number was silently IGNORED in every real
	//! campaign. A .conf field a server owner can tune and that then does nothing is worse than a
	//! missing one, because the whole authored surface loses its credibility with it.
	//!
	//! ⚠ THE FLIP IS BEHAVIOUR-NEUTRAL ONLY BECAUSE THE SHIPPED CONFIG WAS RE-AUTHORED TO -1 IN THE
	//! SAME CHANGE. A config left holding its old positive number would now be HONOURED instead of
	//! overridden, and the campaign would stop scaling with difficulty. That is exactly why
	//! OVT_TowerRecaptureBehaviorDeploymentModule was NOT flipped with the other two - see its own
	//! header - and why OVT_BaseRepairBehaviorDeploymentModule, which left for the deployments
	//! framework as a pure relocation, is excluded as well.
	//!
	//! ⚠ AN AUTHORED ZERO IS NOW AN AUTHORED ZERO, not "unauthored". Every resolver below still clamps
	//! to its own floor, so a zero reads as the smallest sane value rather than as a divide-by-nothing.
	[Attribute(defvalue: "-1", desc: "Seconds the town must be held before the support debuff lands. -1 = the campaign's objectiveHarassmentHoldSeconds difficulty setting (240 on Easy down to 90 on Insane), which is what the shipped config authors. Any other value OVERRIDES difficulty for this config")]
	int m_iHoldSeconds;

	[Attribute(defvalue: "ObjectiveHarassment", desc: "Name of the support modifier stacked onto the town on success, as authored in Configs/Modifiers/supportModifiers.conf")]
	string m_sSupportModifier;

	//! One deployment update, in seconds. Mirrors OVT_DeploymentComponent.UPDATE_FREQUENCY, which is
	//! what Update() is actually called with; the real interval is staggered by 0.8-1.2x, so a hold is
	//! accurate to about a fifth of its length. That is the right precision for "stand here a while".
	static const int UPDATE_SECONDS = 10;

	//! Fired-once latch. NOT an attribute, NOT persisted and NOT copied by CloneModule - a clone is a
	//! fresh deployment's module and has harassed nobody. Not persisting it costs nothing: a restored
	//! deployment's hold timer restarts, which is a delay rather than a double debuff, and the modifier
	//! it would have applied has its own stack limit.
	protected bool m_bHoldFired;

	//! Updates left on the hold. Armed lazily on the first update, because the difficulty settings this
	//! reads are not guaranteed to be loaded at the moment a module is cloned out of its config.
	protected int m_iTicksLeft;

	protected bool m_bArmed;

	//------------------------------------------------------------------------------------------------
	//! One observation of the hold.
	//! \param[in] deltaTime Milliseconds nominally elapsed. Unused - the hold is counted in updates,
	//!            not in wall time; see UPDATE_SECONDS.
	override void OnUpdate(int deltaTime)
	{
		super.OnUpdate(deltaTime);

		if (m_bHoldFired || !m_ParentDeployment)
			return;

		if (!m_bArmed)
		{
			m_iTicksLeft = ResolveHoldTicks();
			m_bArmed = true;
		}

		vector centre = ResolveTownCentre();

		int aliveInside = CountAliveRegisteredMembersWithin(centre, m_fHoldRadius);
				// ⚠ THE RESISTANCE, NOT JUST PLAYERS (author, 2026-08-21). Recruits and - when they arrive -
		// high command groups contest this place exactly as a player does. See DefenderWithin().
		bool enemyPresent = DefenderWithin(centre, m_fHoldRadius);

		if (!EvaluateHold(aliveInside, enemyPresent, m_iTicksLeft))
			return;

		ApplyHarassment(centre);
	}

	//------------------------------------------------------------------------------------------------
	//! THE HOLD DECISION, with every input passed in.
	//!
	//! SPLIT OUT FROM OnUpdate ON PURPOSE, the OVT_RadioTowerCaptureBehaviorDeploymentModule pattern:
	//! asserting "the clock only runs while the place is held and unopposed, and the debuff lands
	//! exactly once" through OnUpdate would mean standing up a live deployment marker, which leaks a
	//! repeating 10 s update into whatever world the assertion runs in.
	//!
	//! \param[in] aliveInside Living members of this deployment's force inside the hold radius.
	//! \param[in] enemyPresent Whether a player is standing inside the same circle.
	//! \param[inout] ticksLeft Updates still owed. PAUSED, never reset, on an interrupted tick -
	//!               see the class header.
	//! \return True when THIS call completed the hold. False on every other path, including a second
	//!         call after a successful one.
	bool EvaluateHold(int aliveInside, bool enemyPresent, inout int ticksLeft)
	{
		if (m_bHoldFired)
			return false;

		// Nobody there: the men are dead, still on the road, or scattered. The clock waits for them.
		if (aliveInside < 1)
			return false;

		// Contested. The occupying faction is standing in the square being shot at, not intimidating
		// anybody. Pause; do not roll back.
		if (enemyPresent)
			return false;

		// ⚠ A NON-POSITIVE HOLD MUST STILL TAKE ONE TICK. An authored zero would otherwise fire on the
		// first update of the first operation, before the men have got out of the truck - which is the
		// instant debuff this module exists to replace.
		if (ticksLeft > 0)
			ticksLeft = ticksLeft - 1;

		if (ticksLeft > 0)
			return false;

		// Latch BEFORE the caller acts, so a re-entrant path through the modifier system or the
		// director cannot find the latch still down.
		m_bHoldFired = true;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The success path: stack the debuff, tell the director, and stand the operation down.
	//! \param[in] centre The town centre that was held.
	protected void ApplyHarassment(vector centre)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (towns)
		{
			OVT_TownData town = towns.GetNearestTown(centre);
			if (town)
			{
				int townId = towns.GetTownID(town);
				if (townId >= 0 && m_sSupportModifier != "")
					towns.TryAddSupportModifierByName(townId, m_sSupportModifier);
			}
		}

		// ⚠ THE DEBUFF IS APPLIED FIRST AND THE OBJECTIVE IS TOLD SECOND. It is free either way today -
		// reporting only COUNTS, and the objective's own gate re-reads the town's support on the next
		// director tick - but the order is the honest one: the operation's EFFECT lands, and only then
		// is the operation reported as completed. Told first, a gate that ever moved back onto this
		// call would read the support from before the debuff.
		//
		// 🔴 IT REPORTS INTO THE OBJECTIVE'S BAG AND DOES NOTHING ELSE. The signal is PULLED by the
		// director's tick, which compares the counter against a mark; a report that advanced a phase or
		// re-armed a timer would put a transition somewhere other than behind the tick's three early
		// returns, and that cost two red cases in two suites once already. See
		// OVT_ObjectiveDirectorComponent.ReportObjectiveProgress().
		OVT_ObjectiveDirectorComponent director = OVT_ObjectiveDirectorComponent.GetInstance();
		if (director)
			director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES, 1);

		// The operation is over. Collected NEXT FRAME rather than here - see
		// OVT_BaseBehaviorDeploymentModule.RequestDeploymentCollection for why an inline delete from a
		// non-last behavior module walks off the end of the module list it is being iterated in.
		RequestDeploymentCollection(string.Format("a harassment operation completed its hold at %1", centre.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! How many updates the hold is worth: the authored attribute, or the campaign's setting when the
	//! attribute is the -1 sentinel. See the attribute's own note for the convention and for why it was
	//! flipped.
	//!
	//! ⚠ A WORLD WITH NO DIFFICULTY SETTINGS - which is every initialisation-tier subject built with
	//! `new` - resolves the sentinel to zero, and the clamp below turns that into one update. A hold of
	//! one update is not the campaign's pacing, but it is a bounded, non-zero answer for a world that
	//! has no campaign in it.
	//! \return At least one update.
	protected int ResolveHoldTicks()
	{
		int difficultyValue = -1;

		OVT_DifficultySettings difficulty = OVT_Global.GetDifficulty();
		if (difficulty)
			difficultyValue = difficulty.objectiveHarassmentHoldSeconds;

		int seconds = OVT_ObjectivePlanRules.ResolveWithDifficulty(m_iHoldSeconds, difficultyValue);

		if (seconds < UPDATE_SECONDS)
			return 1;

		return seconds / UPDATE_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the hold is measured against.
	//!
	//! THE TOWN'S OWN CENTRE, NOT THE DEPLOYMENT MARKER, when one can be resolved. The director creates
	//! this deployment at the town centre, so the two are normally the same point - but a marker that
	//! was nudged (a spawn snap, a hand-placed test fixture) should not move the square the men have to
	//! stand in.
	//! \return The town centre, or the deployment's own position when no town resolves.
	protected vector ResolveTownCentre()
	{
		vector fallback = m_ParentDeployment.GetPosition();

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return fallback;

		OVT_TownData town = towns.GetNearestTown(fallback);
		if (!town)
			return fallback;

		return town.location;
	}

	//------------------------------------------------------------------------------------------------
	//! EVERY attribute has to appear here, and the latch and the armed timer must NOT: a clone belongs
	//! to a different deployment and has held nothing. (A forgotten attribute silently ships the class
	//! default instead of the authored value - the standing trap of a hand-written clone.)
	//!
	//! What a dropped line would cost here: drop m_fHoldRadius and it clones as 0, so nothing is ever
	//! counted inside the circle and no town is ever harassed; drop m_sSupportModifier and the hold
	//! completes, the counter advances and the town's support never moves - a ramp that runs forever
	//! and never reaches its gate.
	override OVT_BaseDeploymentModule CloneModule()
	{
		OVT_TownHarassmentBehaviorDeploymentModule clone = new OVT_TownHarassmentBehaviorDeploymentModule();

		clone.m_sModuleName = m_sModuleName;
		clone.m_fHoldRadius = m_fHoldRadius;
		clone.m_iHoldSeconds = m_iHoldSeconds;
		clone.m_sSupportModifier = m_sSupportModifier;

		return clone;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether this module has already landed its debuff.
	bool HasHoldFired()
	{
		return m_bHoldFired;
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
		Print(string.Format("Town Harassment Behavior Module: %1", m_sModuleName));
		Print(string.Format("  Hold Radius: %1m  Hold: %2 update(s) left", m_fHoldRadius, m_iTicksLeft));
		Print(string.Format("  Support Modifier: %1", m_sSupportModifier));
		string fired = "No";
		if (m_bHoldFired)
			fired = "Yes";
		Print(string.Format("  Already Harassed: %1", fired));
	}
}
