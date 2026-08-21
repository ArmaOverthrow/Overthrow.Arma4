//------------------------------------------------------------------------------------------------
//! THE AUTHORED SURFACE OF THE OBJECTIVE MACHINE: every plan the occupying faction may commit to.
//!
//! Shaped on OVT_DeploymentRegistry (Scripts/Game/GameMode/Deployments/OVT_DeploymentRegistry.c:1-28)
//! down to the attribute names, because two registries in one tree that answer the same question
//! should not be two different shapes. It is authored as a configRoot .conf and wired onto the
//! game-mode prefab beside the deployment registry, so a mod adds a doctrine by shipping one .conf and
//! changing one line of the prefab - with no EnforceScript at all, which is goal G1.
//!
//! ⚠ VALIDATION HAS A REAL CALL SITE, WHICH IS THE ONE THING THE DEPLOYMENT REGISTRY'S EQUIVALENT
//! NEVER GOT (C6). OVT_DeploymentRegistry.ValidateAllConfigs() has been dead code since it was
//! written - nothing calls it - so "a broken config is named and skipped" was decorative there. Here
//! the director calls ValidateObjectiveRegistry() once from PostGameStart(), server-only, and a plan
//! that fails ANY rule is named in one ERROR line, added to the skipped list and never selected. The
//! rest of the registry runs, because one bad plan in a mod must not stop the campaign.
//!
//! ⚠ THE SKIPPED LIST IS BUILT ONCE AND READ ON EVERY SELECTION ROUND. Re-validating per round would
//! turn one ERROR line into one per in-game minute, which is how a log stops being read.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sRegistryName")]
class OVT_ObjectiveRegistry : ScriptAndConfig
{
	//! Log prefix. Matches the director's, so a validation failure reads as part of the same system.
	static const string LOG = "[Overthrow.ObjectiveRegistry] ";

	[Attribute(desc: "Name of this objective registry, for logs and for the Workbench title")]
	string m_sRegistryName;

	[Attribute(desc: "Every plan the occupying faction may commit to")]
	ref array<ref OVT_ObjectiveConfig> m_aObjectiveConfigs;

	//! HOW OFTEN A SELECTION ROUND MAY RUN WHILE THE MACHINE IS IDLE, in in-game minutes.
	//!
	//! ⚠ THE DEFAULT OF 1 IS TODAY'S BEHAVIOUR, EXACTLY. Before plans existed the director selected on
	//! every idle tick, so 1 means "every idle tick" and reproduces the single-list machine's cadence
	//! byte for byte. It exists because a registry with ten plans can back off without a code change
	//! (D6), and it is deliberately shipped as a no-op.
	//!
	//! ⚠ IT DOES NOT GATE THE RESELECT FLAG. A base or a town changing hands re-evaluates the target
	//! immediately, because that request means "the map changed" and making it wait would leave the
	//! occupying faction working toward a place it has just lost.
	//!
	//! ⚠ A BLACKLIST ROUND IS SERVED PER SELECTION ROUND, NOT PER TICK, so raising this also slows the
	//! rate at which a failed objective works off its cooldown. That coupling is the shipped meaning of
	//! "round" and is left alone rather than given a second clock.
	[Attribute(defvalue: "1", desc: "In-game minutes between selection rounds while no objective is running. 1 = every idle minute, which is what the campaign has always done. Raising it backs off the per-round cost on a registry with many plans - and also slows how fast a blacklisted place works off its cooldown, because a round is what it serves")]
	int m_iSelectionCooldownTicks;

	// ⚠ m_iBlacklistRounds IS STILL NOT AUTHORED HERE, AND THAT IS DELIBERATE. It arrives with the
	// build phase that gives it a READER. A .conf field nothing consults is worse than a missing one: a
	// server owner tunes it, nothing changes, and the whole authored surface loses its credibility.

	//! Plans that failed validation, by name. Built once by ValidateAllConfigs() and consulted by every
	//! selection round thereafter.
	protected ref array<string> m_aSkippedConfigs;

	//! Whether ValidateAllConfigs() has run. A registry nobody validated skips nothing, which is the
	//! correct behaviour for the initialisation tier (its worlds never run PostGameStart) and for the
	//! World Editor.
	protected bool m_bValidated;

	//------------------------------------------------------------------------------------------------
	void OVT_ObjectiveRegistry()
	{
		if (!m_aObjectiveConfigs)
			m_aObjectiveConfigs = new array<ref OVT_ObjectiveConfig>();

		m_aSkippedConfigs = new array<string>();
	}

	//------------------------------------------------------------------------------------------------
	//! A plan by its persistence key.
	//!
	//! ⚠ IT DOES NOT FILTER SKIPPED PLANS, and the restore path depends on that. A save naming a plan
	//! that has since become invalid must resolve far enough to be REPORTED by name; the selection path
	//! is what refuses to commit to it. Mirrors OVT_DeploymentRegistry.FindConfigByName exactly.
	//! \param[in] name The plan's m_sObjectiveName.
	//! \return The plan, or null when no plan carries that name.
	OVT_ObjectiveConfig FindConfigByName(string name)
	{
		if (name == "" || !m_aObjectiveConfigs)
			return null;

		foreach (OVT_ObjectiveConfig config : m_aObjectiveConfigs)
		{
			if (config && config.m_sObjectiveName == name)
				return config;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! How many plans are authored, valid or not.
	//! \return The plan count.
	int GetConfigCount()
	{
		if (!m_aObjectiveConfigs)
			return 0;

		return m_aObjectiveConfigs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! One plan by index.
	//! \param[in] index Index into m_aObjectiveConfigs.
	//! \return The plan, or null when the index is out of range.
	OVT_ObjectiveConfig GetConfig(int index)
	{
		if (!m_aObjectiveConfigs || index < 0 || index >= m_aObjectiveConfigs.Count())
			return null;

		return m_aObjectiveConfigs[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a plan failed validation and must never be selected.
	//! \param[in] name The plan's name.
	//! \return True when the plan was skipped.
	bool IsSkipped(string name)
	{
		if (!m_aSkippedConfigs || name == "")
			return false;

		return m_aSkippedConfigs.Find(name) != -1;
	}

	//! \return How many plans failed validation.
	int GetSkippedCount()
	{
		if (!m_aSkippedConfigs)
			return 0;

		return m_aSkippedConfigs.Count();
	}

	//! \return True once ValidateAllConfigs() has run over this registry.
	bool WasValidated() { return m_bValidated; }

	//------------------------------------------------------------------------------------------------
	//! In-game minutes between idle selection rounds, floored at one.
	//!
	//! ⚠ THE FLOOR IS NOT DEFENSIVE PROGRAMMING. A zero or negative authored value would mean "never
	//! select", which stops the occupying faction ever choosing an objective again with nothing in the
	//! log and no symptom a player could report - the same failure mode the concurrent-objective floor
	//! exists to prevent. "Turn the director off" is not an authoring gesture this feature supports.
	//! \return At least 1.
	int GetSelectionCooldownTicks()
	{
		if (m_iSelectionCooldownTicks < 1)
			return 1;

		return m_iSelectionCooldownTicks;
	}

	//------------------------------------------------------------------------------------------------
	//! Validates every plan, names each fault once at ERROR, and skips the plans that carry one.
	//!
	//! 🔴 THE MESSAGE CONTRACT, AND IT IS THE ONLY MECHANICAL SUPPORT THE AUTHORABILITY BAR HAS. Every
	//! rule's line names, IN THIS ORDER: the PLAN (added here, by the caller), the PHASE and the module
	//! slot inside it (added by ValidateConfig/ValidatePhase as they descend), the ATTRIBUTE at fault by
	//! its authored field name, and WHAT TO DO ABOUT IT. The field name matters more than the prose: it
	//! is what a mod author can search their own .conf for, and it is the one part of the sentence that
	//! is the same in the log and in the file. A rule added later that names only the symptom is a rule
	//! whose fault will be reported to somebody who cannot find it.
	//!
	//! ⚠ FAIL LOUD AND SKIP, NEVER FAIL SILENT AND NEVER WEDGE (G7). A plan that fails any rule is added
	//! to the skipped list and is never selected; every other plan in the registry still runs, and the
	//! line says so, because "one line about one plan" and "the campaign is broken" are very different
	//! things to read at 3am.
	//!
	//! ⚠ IT IS IDEMPOTENT. A campaign restarted in the same session runs PostGameStart() again, and a
	//! second pass must not double the skipped list or double the log.
	//!
	//! THE RULES GROW WITH THE FEATURE. Phase 2 owns the structural ones below; plan-driven selection
	//! adds the selector rules, and the operation modules add the "does this name resolve" rules once
	//! there is something to resolve against. A rule is added in the phase that makes it checkable.
	//! \return True when every plan passed.
	bool ValidateAllConfigs()
	{
		if (!m_aSkippedConfigs)
			m_aSkippedConfigs = new array<string>();

		m_aSkippedConfigs.Clear();
		m_bValidated = true;

		if (!m_aObjectiveConfigs || m_aObjectiveConfigs.IsEmpty())
		{
			Print(LOG + "Registry '" + m_sRegistryName + "': m_aObjectiveConfigs authors NO plans at all, so the occupying faction will never choose an objective. Add at least one OVT_ObjectiveConfig to the registry .conf wired on the game-mode prefab", LogLevel.ERROR);
			return false;
		}

		bool allValid = true;
		array<string> seenNames = new array<string>();

		// ⚠ RESOLVED ONCE, AND A MISSING ONE SKIPS THE NAME RULES RATHER THAN FAILING EVERY PLAN. The
		// operation modules name deployment configs by string and those names are the whole reason the
		// "does it resolve" rules exist - but a validator that turned "I could not check" into "this
		// plan is broken" would skip both shipped doctrines in any world without a deployment manager
		// (the World Editor, an initialisation-tier world that builds a registry by hand) and the
		// occupying faction would never attack anything. Not checking is the safe answer; not being
		// able to check and saying nothing is not, so it says so once.
		OVT_DeploymentManagerComponent deployments = OVT_Global.GetDeploymentManager();
		if (!deployments)
			Print(LOG + "Registry '" + m_sRegistryName + "': no deployment manager resolved, so the deployment-config names the plans' operations reference were NOT checked. Every other rule still ran", LogLevel.WARNING);

		int count = m_aObjectiveConfigs.Count();
		for (int i = 0; i < count; i++)
		{
			OVT_ObjectiveConfig config = m_aObjectiveConfigs[i];

			if (!config)
			{
				Print(LOG + "Registry '" + m_sRegistryName + "', m_aObjectiveConfigs entry " + i.ToString() + ": the plan slot is EMPTY. Either author an OVT_ObjectiveConfig in it or delete the slot", LogLevel.ERROR);
				allValid = false;
				continue;
			}

			string fault = ValidateConfig(config, seenNames, deployments);
			if (fault == "")
			{
				seenNames.Insert(config.m_sObjectiveName);
				continue;
			}

			allValid = false;

			// An unnamed plan is still reported, by index, or the one fault nobody can act on would be
			// the one with no label.
			string label = config.m_sObjectiveName;
			if (label == "")
				label = "<unnamed plan at index " + i.ToString() + ">";

			Print(LOG + "SKIPPING plan '" + label + "': " + fault + ". Every other plan in registry '" + m_sRegistryName + "' still runs; this one will never be selected until it is fixed", LogLevel.ERROR);

			if (config.m_sObjectiveName != "" && m_aSkippedConfigs.Find(config.m_sObjectiveName) == -1)
				m_aSkippedConfigs.Insert(config.m_sObjectiveName);
		}

		return allValid;
	}

	//------------------------------------------------------------------------------------------------
	//! Every Phase-2 structural rule for one plan.
	//!
	//! ⚠ IT ANSWERS THE FIRST FAULT AND STOPS. Reporting five faults for one plan buries the first,
	//! and the first is the one an author fixes; the next pass reports the next one.
	//! \param[in] config The plan to check.
	//! \param[in] seenNames Plan names already accepted in this pass, for the duplicate rule.
	//! \param[in] deployments The deployment framework, for the name-resolution rules. NULL SKIPS THEM.
	//! \return An empty string when the plan is valid, otherwise the fault, phrased for a log line.
	protected string ValidateConfig(notnull OVT_ObjectiveConfig config, notnull array<string> seenNames, OVT_DeploymentManagerComponent deployments)
	{
		if (config.m_sObjectiveName == "")
			return "m_sObjectiveName is empty. It is the persistence key and the selection log's label, so a plan with no name can be neither saved nor reported - give it a name that is unique in this registry and never rename it afterwards";

		if (seenNames.Find(config.m_sObjectiveName) != -1)
			return "m_sObjectiveName duplicates a plan earlier in the registry, and BOTH copies are skipped. The name is the persistence key, so two plans sharing one would restore each other's objectives - rename one of them";

		if (config.m_fPriority < 0)
			return "m_fPriority is negative (" + config.m_fPriority.ToString() + "). It is a MULTIPLIER on the selector's score and higher wins, so a negative one lets a poor candidate out-rank a good one - author 1 for 'rank exactly on the score', above 1 to favour this doctrine, below 1 to hold it back, and never below 0";

		if (config.GetPhaseCount() == 0)
			return "m_aPhases is empty. A plan with no phases can be committed to and can then never act, advance or end, which is the one failure mode with no symptom a player could report - author at least one phase, whose last one carries a terminal operation";

		array<string> seenPhases = new array<string>();
		int phases = config.GetPhaseCount();

		for (int i = 0; i < phases; i++)
		{
			OVT_ObjectivePhase phase = config.GetPhase(i);
			if (!phase)
				return "m_aPhases entry " + i.ToString() + ": the phase slot is EMPTY. Either author an OVT_ObjectivePhase in it or delete the slot";

			if (phase.m_sPhaseName == "")
				return "m_aPhases entry " + i.ToString() + ": m_sPhaseName is empty. The name is what the save payload and the Game Master panel carry, so an unnamed phase can be neither restored nor displayed - name it, and never rename it once a campaign has saved in it";

			if (seenPhases.Find(phase.m_sPhaseName) != -1)
				return "phase '" + phase.m_sPhaseName + "', m_sPhaseName: two phases in this plan share it. A restored objective would resolve to the first one whichever it was actually in - rename one of them";

			seenPhases.Insert(phase.m_sPhaseName);

			string phaseFault = ValidatePhase(phase, deployments);
			if (phaseFault != "")
				return "phase '" + phase.m_sPhaseName + "' " + phaseFault;
		}

		// --- PLAN-DRIVEN SELECTION'S TWO RULES. Both describe a plan that would sit in the registry
		//     forever competing for objectives it can never describe, which is the exact shape of fault
		//     that has no symptom: the campaign plays, the plan never runs, and nothing says why.
		if (!config.m_Selector)
			return "m_Selector is empty. A plan has to say WHAT it attacks before it can be ranked against another, so without one it scores nothing on every round and can never be committed to - author a selector, e.g. OVT_ResistanceTownObjectiveSelector or OVT_ResistanceBaseObjectiveSelector";

		if (config.GetCandidateSources() == 0)
			return "m_Selector ('" + config.m_Selector.GetSelectorName() + "') declares NO candidate sources. Selection collects the union of every plan's sources once per round, so a selector that declares none is handed an empty set and scores nothing - forever, and silently. Use a selector subclass that declares a source, or declare one on this selector";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every rule about ONE PHASE's module bag.
	//!
	//! ⚠ THE FAULTS HERE ARE THE ONES NO COMPILER CAN SEE. A phase names deployment configs by string,
	//! names a resolver polymorphically and expresses its whole exit condition as which modules it
	//! carries; every one of those is authored data, and every one of them fails SILENTLY - the ramp
	//! simply stops sending, or the phase simply never ends, with nothing in the log to say which.
	//!
	//! ⚠ IT ANSWERS THE FIRST FAULT AND STOPS, like its caller. Reporting five faults for one phase
	//! buries the first, and the first is the one an author fixes.
	//!
	//! ⚠ THERE IS NO RULE ABOUT THE TWO SENTINELS, AND THERE NO LONGER NEEDS TO BE. Both resolve for
	//! every phase at every index: m_iOperationCadence -1 takes objectiveHarassmentIntervalMinutes,
	//! which every difficulty preset authors, and m_fAnchorRadius -1 takes the director's single
	//! DEFAULT_ANCHOR_RADIUS. Build phase 5 needed one, because the anchor sentinel then meant "the
	//! ported value for THIS phase index" and a fourth phase would have silently inherited the third's;
	//! build phase 6 moved the per-phase numbers into the plans and the sentinel stopped being
	//! positional.
	//! \param[in] phase The phase to check.
	//! \param[in] deployments The deployment framework, or null to skip the name-resolution rules.
	//! \return An empty string when the phase is valid, otherwise the fault, phrased for a log line.
	protected string ValidatePhase(notnull OVT_ObjectivePhase phase, OVT_DeploymentManagerComponent deployments)
	{
		bool hasCondition = false;
		bool hasTerminalOperation = false;

		if (phase.m_aModules)
		{
			int moduleCount = phase.m_aModules.Count();
			for (int i = 0; i < moduleCount; i++)
			{
				OVT_BaseObjectiveModule module = phase.m_aModules[i];
				if (!module)
					return "m_aModules entry " + i.ToString() + ": the module slot is EMPTY. Either author a module in it or delete the slot - .conf module ORDER is evaluation order, so an empty slot is also a gap in the sequence a reader is counting";

				if (OVT_BaseObjectiveConditionModule.Cast(module))
					hasCondition = true;

				OVT_BaseObjectiveOperationModule operation = OVT_BaseObjectiveOperationModule.Cast(module);
				if (operation && operation.IsTerminal())
					hasTerminalOperation = true;

				OVT_SendDeploymentObjectiveOperation send = OVT_SendDeploymentObjectiveOperation.Cast(module);
				if (send)
				{
					string sendFault = ValidateSendOperation(send, deployments);
					if (sendFault != "")
						return "module '" + module.m_sModuleName + "' " + sendFault;
				}
			}
		}

		// 🔴 THE WEDGE RULE. A phase that can neither ADVANCE (no condition to satisfy) nor END (no
		// terminal operation) runs until its idle clock abandons the objective, every single time,
		// and the only symptom is an occupying faction that never gets past this phase. An idle abort
		// is not a way out either - it is the backstop that catches this, not a design for it.
		if (!hasCondition && !hasTerminalOperation)
			return "m_aModules carries NO advance condition and NO terminal operation, so this phase can neither advance nor end - every objective that reaches it runs its idle clock down and is abandoned. Add a condition module for a phase that hands over to the next one, or a terminal operation for a phase that finishes the objective";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Every rule about ONE send-deployment operation: it has somewhere to send, and everything it names
	//! resolves.
	//!
	//! ⚠ THE NAMES ARE THE KEYS AND NOTHING ELSE CHECKS THEM. A rung renamed in overthrowDeployments.conf
	//! and not in the plan does not fail to parse and does not warn at runtime beyond one refusal line
	//! per in-game minute; this is the pass that catches it once, at world start, by name.
	//! \param[in] send The operation to check.
	//! \param[in] deployments The deployment framework, or null to skip the name-resolution rules.
	//! \return An empty string when it is valid, otherwise the fault.
	protected string ValidateSendOperation(notnull OVT_SendDeploymentObjectiveOperation send, OVT_DeploymentManagerComponent deployments)
	{
		if (!send.m_Resolver)
			return "m_Resolver is empty, so this operation has nowhere to send anything and would refuse on every cadence interval, silently, forever. Author a target resolver - OVT_ObjectiveSelfTargetResolver sends at the objective itself";

		bool hasLadder = send.m_aLadder && !send.m_aLadder.IsEmpty();

		if (!hasLadder && send.m_sConfigName == "")
			return "authors neither m_sConfigName nor m_aLadder, so there is no deployment for this operation to buy. Author m_sConfigName for one fixed deployment, or m_aLadder plus m_sLadderProgressKey for a ramp that escalates";

		if (!deployments)
			return "";

		if (hasLadder)
		{
			int rungs = send.m_aLadder.Count();
			for (int i = 0; i < rungs; i++)
			{
				string rung = send.m_aLadder[i];

				if (rung == "")
					return "m_aLadder rung " + i.ToString() + " is EMPTY. The ladder is indexed by progress, so an empty rung is a hole the ramp falls into at exactly one success count - name a deployment config in it, or shorten the ladder";

				if (!deployments.FindConfigByName(rung))
					return "m_aLadder rung " + i.ToString() + " names '" + rung + "', which the deployment registry does not carry. Check the m_sDeploymentName spellings in Configs/Deployment/overthrowDeployments.conf - the name is the key, and no compiler reads a .conf";
			}

			return "";
		}

		if (!deployments.FindConfigByName(send.m_sConfigName))
			return "m_sConfigName names '" + send.m_sConfigName + "', which the deployment registry does not carry. Check the m_sDeploymentName spellings in Configs/Deployment/overthrowDeployments.conf - the name is the key, and no compiler reads a .conf";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! One line per plan, for a server owner checking what actually loaded. Mirrors
	//! OVT_DeploymentRegistry.PrintRegistryInfo().
	void PrintRegistryInfo()
	{
		Print(LOG + "Objective Registry: " + m_sRegistryName);
		Print(LOG + "  Plans: " + GetConfigCount().ToString() + ", skipped: " + GetSkippedCount().ToString() + ", selection every " + GetSelectionCooldownTicks().ToString() + " in-game minute(s)");

		if (!m_aObjectiveConfigs)
			return;

		foreach (OVT_ObjectiveConfig config : m_aObjectiveConfigs)
		{
			if (!config)
				continue;

			string selector = "NO SELECTOR";
			if (config.m_Selector)
				selector = config.m_Selector.GetSelectorName();

			Print(LOG + "  '" + config.m_sObjectiveName + "' - " + config.GetPhaseCount().ToString() + " phase(s), priority " + config.m_fPriority.ToString() + ", targets " + selector);
		}
	}
}
