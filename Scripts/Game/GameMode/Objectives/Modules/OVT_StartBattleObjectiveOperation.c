//------------------------------------------------------------------------------------------------
//! THE BATTLE, AS AN AUTHORED OPERATION: start the campaign's one battle at this objective, then wait
//! for it, then end the objective whatever happened.
//!
//! The port of the hard-coded counter-attack starters and the battle-phase tick they answered to. It
//! is the only thing in this feature that starts a battle: the occupying faction manager's two
//! starters keep exactly two callers each after this build phase - the player-initiated one and this.
//!
//! =============================================================================================
//! 🔴 THE BATTLE'S END IS POLLED, NEVER SUBSCRIBED. DO NOT "IMPROVE" THIS INTO A CALLBACK.
//! =============================================================================================
//! The occupying faction manager's own finish handlers DELETE THE BATTLE CONTROLLER'S ENTITY from
//! inside the invoker's own dispatch, so a second subscriber ordered after them runs against a deleted
//! entity - and the crash is in the engine's dispatch, not in anything a stack trace would point at
//! here. One null check per in-game minute cannot be got wrong.
//!
//! THE POLL IS THE RUNNER'S THIRD EARLY RETURN AND NEEDS NO CODE OF ITS OWN. While the campaign's
//! battle slot is occupied, OVT_ObjectiveDirectorComponent.DirectorTick() returns before it reaches
//! any module at all - so the phase is frozen, not spinning - and the FIRST tick on which the slot is
//! empty again is the first tick that reaches TryAct(). Finding the latch set and the slot empty is
//! therefore exactly "the battle this objective started has resolved".
//!
//! ⚠ AND IT IS WHY THIS PHASE AUTHORS A CADENCE OF ZERO. Every other phase spends on an interval; this
//! one spends nothing and is polling. A phase that inherited the difficulty interval would be asked
//! again up to a whole cadence after its battle ended - sixty in-game minutes on Normal - and the
//! objective would stand there finished, holding the deployment bias and the machine's one objective
//! slot, for all of it. An authored zero is a supported gesture ("act every in-game minute") and this
//! is what it is for. Both shipped plans author it and an initialisation case pins it.
//!
//! ⚠ A WIN AND A LOSS TAKE THE SAME PATH, DELIBERATELY. The campaign's battle slot reports no outcome
//! to this component and is not asked for one: the occupying faction has spent its ramp, and whether
//! it took the place or not, THIS OBJECTIVE is finished. ResetObjective() is that one path (R1) - the
//! forward base is torn down, the objective's deployments are collected, the bias and the reserve floor
//! are dropped and the machine goes idle - and the IDLE branch of the NEXT tick chooses again, one
//! in-game minute later, which is the same one-tick gap every other ending in this machine has.
//!
//! ⚠ AND THE RESOLUTION DOES NOT BLACKLIST. A resolved battle is not a failure of the objective: the
//! place is re-evaluated on its merits, and if the resistance held it, it is very likely worth
//! attacking again. The FAILURE arm of the same phase is authored separately and does blacklist - see
//! the OVT_IdleForObjectiveAbort beside this module in both shipped plans, and the refusal note below.
//!
//! 🔴 A REFUSED START SITS OUT THE PHASE AND IS THEN ABANDONED WITH THE BLACKLIST, which is exactly
//! what the hard-coded machine did with one. Every reason the starters below refuse - the base retaken
//! by some other route, a marker that no longer resolves, an objective position with nothing under it -
//! is a state that lasts until the objective ends, and each says so ONCE. Resetting on the first
//! refusal would throw a whole ramp away for one bad tick, so the phase's idle clock is what ends it.
//!
//! ⚠ THE LATCHES ARE PER PHASE ENTRY BY CONSTRUCTION AND ARE NEVER PERSISTED. A phase entry clones this
//! module fresh, so a restored objective sitting in the battle phase comes back having started nothing -
//! which is the honest state, because the battle it had started did not survive the save either. It
//! starts a new one on its first tick, exactly as the hard-coded machine did.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_StartBattleObjectiveOperation : OVT_BaseObjectiveOperationModule
{
	[Attribute(defvalue: "1", uiwidget: UIWidgets.ComboBox, desc: "How the battle is fought. COUNTER_ATTACK is the silent siege the director exists to mount - no announcement, a 15-real-minute muster, and only then a fight. STANDARD is the player-facing battle a captured base raises and is almost certainly not what a doctrine wants", enums: ParamEnumArray.FromEnum(OVT_EQRFMode))]
	OVT_EQRFMode m_eMode;

	[Attribute(defvalue: "100", desc: "How near (m) the objective's recorded position a BASE must still be for the battle to be mounted on it. The objective stores the base's own location, so this resolves by metres in a live campaign; it exists so a RESTORED payload naming a base that has since gone does not drop a battle on whichever base happens to be nearest. Ignored for a town")]
	float m_fBaseResolveRadius;

	//! True once a battle has actually been started for this objective, so the phase starts one and then
	//! waits for it rather than starting one every in-game minute.
	//!
	//! ⚠ SET WHEN A BATTLE ACTUALLY STARTS, NEVER ON THE ATTEMPT. Every starter refuses silently when a
	//! battle is already running, and the base starter additionally needs a live base marker to hang a
	//! controller on. Latching on the attempt would leave the machine waiting for a battle that was never
	//! started and end the objective on the very next tick - a whole ramp thrown away with nothing in the
	//! log.
	protected bool m_bStarted;

	//! Whether the "ready, but the battle could not be started" line has already been said for this phase
	//! entry. Once, not once per in-game minute: the gate is re-asked every minute and every refusal
	//! below lasts until the objective ends, so an unlatched line would be several hundred identical
	//! warnings before the idle clock gives up. The timeout IS the exit here; this line is the
	//! explanation for it.
	protected bool m_bRefusalLogged;

	//------------------------------------------------------------------------------------------------
	//! Starts the battle, or ends the objective if the one it started has resolved.
	//! \return True when a battle was started or the objective was ended by this call.
	override bool TryAct()
	{
		OVT_ObjectiveInstance objective = GetObjective();
		if (!objective || !objective.IsLive())
			return false;

		OVT_ObjectiveDirectorComponent director = GetDirector();
		if (!director)
			return false;

		OVT_OccupyingFactionManager occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying)
			return false;

		// The tick's freeze would have returned before this module was reached, so a battle visible here
		// means one started outside the tick - a player capturing a base mid-siege, a scripted scenario.
		// Refusing is the same answer every starter below would give.
		if (occupying.m_CurrentQRF)
			return false;

		if (m_bStarted)
		{
			// 🔴 THE POLL. See the class header before replacing this with a subscription.
			director.ResetObjective("the counter-attack has resolved", false);

			return true;
		}

		if (!StartBattle(objective, occupying))
			return false;

		m_bStarted = true;

		Print(OVT_ObjectiveDirectorComponent.LOG + "Objective '" + objective.GetTargetName() + "': the counter-attack has begun", LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 THE PHASE ENDS BY ACTING, NOT BY ADVANCING, AND THE VALIDATOR NEEDS TO KNOW. A phase carrying
	//! this module needs no advance condition: it finishes the plan. Answering false would make the last
	//! phase of both shipped plans look like a wedge and the whole plan would be skipped at world start.
	//! \return True.
	override bool IsTerminal()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the battle appropriate to what kind of place this objective is.
	//! \param[in] objective The objective.
	//! \param[in] occupying The occupying faction manager, which owns the campaign's one battle slot.
	//! \return True when a battle was started.
	protected bool StartBattle(notnull OVT_ObjectiveInstance objective, notnull OVT_OccupyingFactionManager occupying)
	{
		if (objective.GetTargetKind() == OVT_EObjectiveKind.BASE)
			return StartOnBase(objective, occupying);

		if (objective.GetTargetKind() == OVT_EObjectiveKind.TOWN)
			return StartOnTown(objective, occupying);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the battle against a BASE objective.
	//!
	//! ⚠ THE OBJECTIVE POSITION MUST STILL BE A RESISTANCE-HELD BASE, the same precondition sabotage
	//! makes and for the same reasons: selection copies the base's own location verbatim so this resolves
	//! by metres in a live campaign, but a restored payload naming a base that has since gone must not
	//! drop a battle on whichever base happened to be nearest, and a base the occupying faction has
	//! retaken by some other route must not be attacked by its own side. The objective is LOCKED against
	//! re-selection from the forward-base phase onward, so a base recaptured mid-ramp sits until the idle
	//! clock gives up - loudly, with the line below in the log rather than a battle.
	//! \param[in] objective The objective.
	//! \param[in] occupying The occupying faction manager.
	//! \return True when a battle was started.
	protected bool StartOnBase(notnull OVT_ObjectiveInstance objective, notnull OVT_OccupyingFactionManager occupying)
	{
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return false;

		OVT_BaseData data = occupying.GetNearestBase(objective.GetTargetPosition());
		if (!data)
			return false;

		if (vector.Distance(data.location, objective.GetTargetPosition()) > m_fBaseResolveRadius)
		{
			LogRefusal(objective, "there is no base within " + m_fBaseResolveRadius.ToString() + " m of its recorded position");
			return false;
		}

		if (data.faction == config.GetOccupyingFactionIndex())
		{
			LogRefusal(objective, "the occupying faction already holds it");
			return false;
		}

		OVT_BaseControllerComponent controller = occupying.GetBase(data.entId);
		if (!controller)
		{
			LogRefusal(objective, "its base marker could not be resolved");
			return false;
		}

		occupying.StartBaseQRF(controller, m_eMode);

		return occupying.m_CurrentQRF != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the battle against a TOWN or CITY objective.
	//! \param[in] objective The objective.
	//! \param[in] occupying The occupying faction manager.
	//! \return True when a battle was started.
	protected bool StartOnTown(notnull OVT_ObjectiveInstance objective, notnull OVT_OccupyingFactionManager occupying)
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns)
			return false;

		OVT_TownData town = towns.GetNearestTown(objective.GetTargetPosition());
		if (!town)
		{
			LogRefusal(objective, "no town could be resolved at its recorded position");
			return false;
		}

		occupying.StartTownQRF(town, m_eMode);

		return occupying.m_CurrentQRF != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Says ONCE, per phase entry, that everything was ready and the battle still could not be started.
	//! \param[in] objective The objective, for its name.
	//! \param[in] reason Why no battle could be started. Named in the log.
	protected void LogRefusal(notnull OVT_ObjectiveInstance objective, string reason)
	{
		if (m_bRefusalLogged)
			return;

		m_bRefusalLogged = true;

		Print(OVT_ObjectiveDirectorComponent.LOG + "Objective '" + objective.GetTargetName() + "' is ready to be counter-attacked but " + reason + ". It will sit out the rest of the battle phase and be abandoned when that times out", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! \return An empty prefix: the battle is the campaign's, not the objective's, and nothing about it
	//!         is worth carrying across a save - see the class header's note on the latches.
	override string GetBagPrefix()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! 🔴 EVERY ATTRIBUTE, BY HAND, INCLUDING THE PARENT'S. Not chained, silently drops what it forgets.
	//!
	//! What a dropped line costs here: drop m_eMode and every clone reads 0 = STANDARD, so the director's
	//! silent siege becomes a player-facing battle with an announcement and a 120-second countdown - the
	//! one thing the counter-attack mode exists to avoid, and nothing anywhere logs the difference. Drop
	//! m_fBaseResolveRadius and it reads 0, so NO base is ever within it and every base doctrine refuses
	//! its own battle and is abandoned when the phase times out.
	//!
	//! ⚠ THE TWO LATCHES ARE DELIBERATELY NOT COPIED. They are per-entry runtime state, not authored
	//! attributes, and a clone starts its phase having started nothing and said nothing.
	//! \return An independent copy.
	override OVT_BaseObjectiveModule CloneModule()
	{
		OVT_StartBattleObjectiveOperation clone = new OVT_StartBattleObjectiveOperation();

		clone.m_sModuleName = m_sModuleName;
		clone.m_eMode = m_eMode;
		clone.m_fBaseResolveRadius = m_fBaseResolveRadius;

		return clone;
	}
}
