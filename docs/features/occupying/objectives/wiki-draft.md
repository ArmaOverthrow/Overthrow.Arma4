# Wiki draft: Objective Plans (the authoring surface)

**Status:** UNPUBLISHED. The `wikijs` MCP server was not attached to the Phase 8 session, so nothing was
searched for, created or updated on https://wiki.armaoverthrow.com. This file is the draft to publish.

**Proposed path:** `development-documentation/objective-plans`
**Proposed title:** Objective Plans

**Before publishing, do these three things:**

1. `wikijs_search_pages` for `objective`, `counter attack`, `occupying`, `doctrine`. If a page already
   describes the objective director, **update it in place** rather than creating this one beside it.
2. `wikijs_get_page_children` on `development-documentation` to place it in the existing hierarchy.
3. Re-check every number below against the shipped `.conf` at publication time. Every figure here was
   verified on 2026-08-21 and each is cited to a file so it can be re-checked in one grep.

⚠ **This page is developer/modder material and belongs under `development-documentation/`.** The
player-facing description of the same machine is the in-game Field Manual page "Counter Attacks" and
whatever wiki page mirrors it; that one must not gain class names or attribute names.

---

## Objective Plans

The occupying faction does not attack at random. It picks **one objective**, a town, city or military
base the resistance holds, and works toward it through a sequence of phases until it either mounts a
counter attack or gives the place up. Since v1.5 every rule in that sequence is **authored data**, not
code: what the faction attacks, what it thinks a target is worth, what it does in each phase, and what
makes it move on are all read from a config registry that a mod can replace.

This page describes that registry. If you only want to know what the occupying faction does to you as a
player, the in-game Field Manual's "Counter Attacks" page is the shorter answer.

### Where it lives

| Thing | Where |
|---|---|
| The registry | `Configs/Objective/overthrowObjectives.conf` |
| The two shipped plans | `Configs/Objective/Objective_TownOffensive.conf`, `Objective_BaseOffensive.conf` |
| Wired onto | `Prefabs/GameMode/OVT_OverthrowGameMode.et`, on `OVT_ObjectiveDirectorComponent.m_Registry` |
| The runner | `OVT_ObjectiveDirectorComponent` (`Scripts/Game/GameMode/Objectives/`) |

A registry with no plans, or whose plans all fail validation, means the occupying faction selects
**nothing**. That is deliberate: falling back to a built-in doctrine would quietly do something the author
did not ship. There is no longer a hard-coded implementation behind the data.

### The shape of a registry

```
OVT_ObjectiveRegistry            the whole authored doctrine set
 └─ OVT_ObjectiveConfig          one PLAN ("Town Offensive")
     ├─ OVT_ObjectiveTargetSelector   what it attacks, and what a candidate is worth
     └─ OVT_ObjectivePhase[]          ordered phases ("Harassment", "ForwardBase", "CounterAttack")
         └─ OVT_BaseObjectiveModule[] operations, conditions and aborts, IN EVALUATION ORDER
```

### Registry fields

| Field | Meaning |
|---|---|
| `m_sRegistryName` | Label for logs and the Workbench title |
| `m_aObjectiveConfigs` | Every plan the faction may commit to. **Registry order is the tie-break order** between plans of equal rank |
| `m_iSelectionCooldownTicks` | In-game minutes between selection rounds while nothing is running. Shipped value `1`, which is every idle minute. Raising it also slows how fast a blacklisted place works off its cooldown, because a blacklist round is served per selection round |

### Plan fields (`OVT_ObjectiveConfig`)

| Field | Shipped | Meaning |
|---|---|---|
| `m_sObjectiveName` | `Town Offensive` / `Base Offensive` | **A persistence key.** It travels in the save payload; renaming it abandons every saved objective running it. The `.conf` file name is only a hint and is safe to change |
| `m_iAllowedFactionTypes` | `OCCUPYING_FACTION` | Which factions may run it |
| `m_fPriority` | `1` | A **multiplier** on the selector's score when plans are compared, and higher wins. This is the opposite convention to a deployment config's priority |
| `m_fChance` | `100` | Percent chance the plan is considered at all on a round. At 100 the roll is short-circuited, so a shipped campaign never touches the random generator during selection |
| `m_iMaxInstances` | `1` | How many objectives may run this plan at once |
| `m_Selector` | see below | Required. A plan with no selector is skipped |
| `m_aPhases` | 3 | Ordered. Index 0 is entered the moment the plan is committed to |

### Selectors: what a plan attacks

A selector declares **candidate sources** and scores every candidate. Two ship:

| Class | Source | Weights it authors |
|---|---|---|
| `OVT_ResistanceTownObjectiveSelector` | `RESISTANCE_TOWNS` | population 40 (reference 400), support collapse 30, proximity 25, tower coverage 10 |
| `OVT_ResistanceBaseObjectiveSelector` | `RESISTANCE_BASES` | base prize 45, threat 25 (reference 40), proximity 25, tower coverage 10 |

Those numbers are the shipped values in `Objective_TownOffensive.conf` and `Objective_BaseOffensive.conf`;
they are authored, so a doctrine can weigh a target differently without touching code.

**Villages are never candidates**, and neither are radio towers or the occupying faction's own forward
bases. That exclusion lives in the candidate collection, not in a selector, so it is a statement about the
campaign rather than about one doctrine.

Selection runs like this, once per round:

1. Eligible plans are gathered: validation, faction, instance cap, then `m_fChance`.
2. The candidate set is collected **once**, over the union of every eligible plan's sources. Ten town
   doctrines still cost one pass over the town registry.
3. Each plan's selector scores every candidate; a per-plan mask hides candidates from sources the plan did
   not claim, and hides blacklisted places.
4. Each plan's best score is multiplied by its `m_fPriority`; the highest wins, ties going to the earlier
   plan in registry order.

Because both shipped plans author `m_fPriority 1` and claim **disjoint** sources, this reproduces the old
single-list pick exactly. A registry whose plans overlap is a supported thing to author, and there the
priority multiplier is the point: a doctrine can outrank a slightly better target it has no plan for.

### Phase fields (`OVT_ObjectivePhase`)

| Field | Meaning |
|---|---|
| `m_sPhaseName` | **A persistence key**, and the name shown on the Game Master panel. It is also what deployment configs name in their phase spans. Must be unique within the plan, and renaming it abandons every saved objective sitting in it |
| `m_iOperationCadence` | In-game minutes between operations in this phase. `-1` uses `objectiveHarassmentIntervalMinutes` (Easy 90, Normal 60, Hard 45, Extreme 30, Insane 20). **`0` is legal and means every in-game minute**, which is what a phase that waits rather than spends should author |
| `m_fAnchorRadius` | How far from the objective routine deployment spending is nudged. Both shipped plans author 600 m while still choosing and 1200 m once committed. `-1` is the flat default of 600 |
| `m_iIdleTimeoutTicks` | In-game minutes the phase may go **without progress** before the objective is abandoned as wedged. An idle clock, not a phase budget. `-1` uses the director's own value, 240 |
| `m_aModules` | The bag. **Authored order is evaluation order**, and a `.conf` cannot carry a comment saying so, so name your modules "1. …", "2. …" as the shipped plans do |

### The module catalogue

Every module carries `m_sModuleName`, which is only a label for logs and the Workbench title.

**Operations** do something and cost the faction resources.

| Class | What it does | Key fields |
|---|---|---|
| `OVT_SendDeploymentObjectiveOperation` | Buys one deployment and sends it somewhere | `m_sConfigName` for a fixed deployment **or** `m_aLadder` + `m_sLadderProgressKey` for a ramp that escalates; `m_Resolver`; `m_iMaxConcurrent`; `m_fConcurrencyRadius`; `m_fDedupRadius`; `m_iRequiredTargetKind` |
| `OVT_RaiseForwardBaseObjectiveOperation` | Sites and raises the forward operating base, and holds it as a named asset | `m_sAssetKey` (`fob`), `m_sDeploymentConfigName`, `m_sGarrisonConfigName`, `m_iBudgetCost`, and the siting band `m_fBandMinFraction` 0.35 / `m_fBandMaxFraction` 0.75 / `m_fMinStandoff` 350 / `m_fMaxStandoff` 2500 / `m_iSitingSteps` 8 / `m_iSitingLanes` 5 / `m_fLateralSpread` 400 |
| `OVT_StartBattleObjectiveOperation` | Starts the battle and then waits for it. **Terminal**: the phase ends when the battle resolves | `m_eMode` (`COUNTER_ATTACK`), `m_fBaseResolveRadius` |

**Conditions** all have to pass for the phase to advance to the next one.

| Class | Passes when | Key fields |
|---|---|---|
| `OVT_TargetKindIsObjectiveCondition` | The objective is a town (1) or a base (2) | `m_iRequiredKind` |
| `OVT_SupportBelowObjectiveCondition` | The town's support has fallen below a threshold | `m_iSupportThreshold`, `m_sRequiredTownModifier` |
| `OVT_ProgressAtLeastObjectiveCondition` | A counter in the objective's bag has reached a number | `m_sBagKey`, `m_iRequired` |
| `OVT_AssetUpObjectiveCondition` | A named asset is standing | `m_sAssetKey`, `m_bInverted` |
| `OVT_ReserveAtLeastObjectiveCondition` | The faction pool holds at least the gate | `m_iGate` |
| `OVT_DaylightWindowObjectiveCondition` | The world clock is inside the window | `m_iStartHour` 5, `m_iEndHour` 15 on both shipped plans. It **holds the idle clock only**: everything else in the phase keeps running while it waits for dawn |

A condition that is guarding something for a different target kind should be paired with
`OVT_TargetKindIsObjectiveCondition`, which is how both shipped plans keep one plan's gate from firing on
the other's kind.

**Aborts** end the objective.

| Class | Ends it when | Key fields |
|---|---|---|
| `OVT_IdleForObjectiveAbort` | The phase has gone its whole idle budget without progress | `m_sPhaseWork` and `m_sGoalNotReached` are the words used in the log line; `m_bBlacklist` |
| `OVT_AssetStarvedObjectiveAbort` | A standing asset has been cut off for its whole budget | `m_sAssetKey`, `m_iStarvationMinutes` (`-1` = `objectiveStarvationMinutes`, Easy 45 down to Insane 15), `m_fAreaRadius` 250 |

`m_bBlacklist` makes the objective's **place** sit out a selection round, so the faction does not pick the
same target again on the very next minute and fail the same way.

### Target resolvers

An operation asks a resolver **where** to send what it buys. Four ship:

| Class | Resolves to |
|---|---|
| `OVT_ObjectiveSelfTargetResolver` | The objective itself. `m_fRequireEnemyHeldBaseWithin` narrows it to an objective with a base that close |
| `OVT_EnemyTowersAffectingTargetResolver` | Radio towers the resistance holds that reach the objective, skipping any the occupying faction already holds |
| `OVT_ForwardBaseTargetResolver` | A named asset's own position, so a garrison is sourced from the forward base it garrisons |
| `OVT_NearestControlledBaseTargetResolver` | The nearest base the occupying faction still holds |

A resolver returns candidates in order, and the send operation walks past ones it has already covered
within `m_fDedupRadius`. That is how a second resistance-held tower gets a team on a later interval instead
of the first one being retried forever.

### The `-1` convention

Most numeric module fields accept `-1`, which means "use the campaign's difficulty setting for this".
Author a real number and you override the difficulty setting for that module only. Both shipped plans use
`-1` wherever a difficulty field exists, which is why difficulty still works exactly as it always did.

The relevant difficulty fields, with their shipped values in Easy / Normal / Hard / Extreme / Insane order:

| Field | Easy | Normal | Hard | Extreme | Insane |
|---|---:|---:|---:|---:|---:|
| `objectiveHarassmentIntervalMinutes` | 90 | 60 | 45 | 30 | 20 |
| `objectiveHarassmentMaxConcurrent` | 1 | 2 | 2 | 3 | 4 |
| `objectiveSabotageMissionsRequired` | 6 | 5 | 4 | 3 | 2 |
| `objectiveTowerRecaptureHoldSeconds` | 900 | 600 | 480 | 360 | 300 |
| `objectiveFOBGarrisonMax` | 1 | 2 | 3 | 5 | 6 |
| `objectiveFOBCost` | 400 | 400 | 400 | 400 | 400 |
| `objectiveStarvationMinutes` | 45 | 30 | 25 | 20 | 15 |
| `objectiveQRFResourceGate` | 750 | 750 | 1200 | 2000 | 3000 |

(from `Configs/Difficulty/Difficulty_*.conf`)

### What one tick of a running objective does

In this order, every in-game minute:

1. The phase's modules are snapshotted, and every module is ticked.
2. **Aborts** are folded. Any one of them ends the objective.
3. **Conditions** are folded. If all pass, the phase advances and the tick stops there: a tick that
   advances runs no operation.
4. If the operation cadence has elapsed, **operations** are asked in authored order. The first one that
   acts consumes the cadence; every refusal leaves the countdown at zero so the next minute asks again.
5. The idle clock is served, and the aborts are folded a second time.

### The validator will tell you what is wrong

The registry validates once at world start and **names every plan it skips at ERROR**, with the plan, the
phase, the attribute and what to do about it. A skipped plan is never selected; every other plan in the
registry still runs. It catches, among others: an empty or duplicate plan name, a negative priority, no
phases, an empty or duplicate phase name, no selector, a selector that declares no candidate sources, an
empty module slot, a send operation with no resolver, a send operation that names neither a deployment nor
a ladder, a ladder rung the deployment registry does not carry, and **a phase with no advance condition
and no terminal operation**, which is a phase that can neither advance nor end.

Read the server log after authoring. No compiler reads a `.conf`, and the validator is the only thing that
does.

### Renaming hazards, in one place

- `m_sObjectiveName` and `m_sPhaseName` are **save keys**. Renaming either abandons the objectives that
  were running under the old name. A save naming a plan or phase this build does not have is logged, the
  objective is discarded, and a new one is chosen; the blacklist survives.
- A deployment's `m_sDeploymentName` is the key a `m_sConfigName` or a ladder rung matches on, not the
  `.conf` file name.
- Deployment configs name phases by **name** in `m_sFromPhase` / `m_sThroughPhase`, so a phase rename has
  to be made in the deployment configs too. A `spans phases … which the running plan … does not carry`
  line in the log is the symptom.

### What is not authorable

The battle itself, the siege ring, the muster window and the wanted system are not part of this registry.
A plan starts a battle; how the battle is fought is the QRF system.
