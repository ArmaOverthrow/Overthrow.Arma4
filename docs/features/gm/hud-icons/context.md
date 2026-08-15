# HUD Icons — Context & Decisions

**Feature:** gm/hud-icons (epic `gm`, feature 3 of 5)
**Last Updated:** 2026-08-15 (verified + closed)
**Current Phase:** Complete
**Status:** ✅ Built — user-verified 2026-08-15 (tooltips, delete/drag refusal, altitude visibility,
MP/JIP, and both big tower variants all confirmed by the user after the tooltips-only descope).
Auth path used for the MP pass was not recorded — still owed to the epic if it matters later.

> **⚠️ DESCOPE (user decision, 2026-08-15, after live testing):** the click-driven detail section
> stretched the panel far too much and was **removed entirely** — `OVT_GMDetailUIComponent.c`,
> `GMIconDetail.layout(+.meta)` deleted, the injection reverted, 24 orphaned `.st` items removed.
> Per-entity info now lives **only in the hover tooltip**. Base tooltip carries
> **"%1 Resources - %2 Garrison Groups"** (both seam-gated; falls back to "No campaign data yet";
> no faction — the vanilla tooltip already shows it). Town/tower tooltips unchanged ("all good").
> **Consequence:** group origin/reason and player money/level have NO surface any more (they only
> existed in the panel section, and group/player tooltips are impossible per plan D6 — vanilla owns
> those info objects). If they're wanted later, epic Phase 2's popup actions are the natural home.
> Plan §6 items F-4…F-8 and F-10 are obsolete; F-9 (tooltips) is now the whole click/hover story.
> `OVT_GMIconFormat`'s population/upgrade/origin helpers are currently unused by runtime code but
> stay (pure, Logic-tested, documented for gm-map reuse).

---

## Quick Status

**What's Done:**
- ✅ Phases 0–4: baseline, editable components + 5 prefab blocks + Init gate (advanced agent), detail
  surface + selection plumbing, group/player detail, hover tooltips
- ✅ Gates: compile clean; **Fast 145/145**; all 6 new test cases can-fail-proven with named failures;
  zero new networking / zero base-game forks (grep-proven)
- ✅ Plan corrected: towers are same-GUID deltas with inherited RplComponent — flags 2052 everywhere, no LOCAL

**What's Next:**
- 🖐️ Phase 5: full verification method (`implementation.md` §6) — Workbench host checks, tower `_base`/
  `_medium_base` variants in Eden, MP/JIP pass, auth-path record. **Regenerate localization exports first.**
- 📋 Phase 6 remainder: epic-overview refresh (done via /update-epic), gm-state context if Phase 5
  discharges owed MP items

**Blockers:**
- Phase 5 needs the user (visual + MP evidence; no suite covers it)

---

## Baseline (Phase 0)

Recorded 2026-08-15 16:45.

| Gate | Result |
|---|---|
| `tools/compile-check.sh` | ✅ OK (6095 files, Game module, 10s), exit 0 |
| git head / status | ✅ `b01782c3` (same head the plan cited); tree clean except this feature's docs |
| Highest bug id | ✅ BUG-174 (matches planning) |
| `{6B09…}` GUID series free | ✅ 0 hits for `{6B09` across Prefabs/Configs/Scripts/UI/Language; `{6B0A` also 0 (spare) |
| Seam + panel citations resolve | ✅ `OVT_GMPanelUIComponent.c` GetInstance :232 / GetDetailSlot :247 / ShowDetail :261 / ClearDetail :278; `OVT_GMCampaignState.c` HasData :90; `OVT_ControllerComponent.c` Get :36 — all exact |
| Tower prefab chain has no RplComponent | ❌ **This row was WRONG** — see Phase 1 finding below. The grep checked Overthrow's delta text + the grandparent `Tower_Base.et`, but Overthrow's tower prefabs are **same-GUID deltas** over vanilla `TransmitterTower_01{,_medium,_small}_base.et`, which carry `RplComponent` (e.g. vanilla `TransmitterTower_01_base.et:129`). Towers inherit RplComponent → LOCAL is fatal → **all five prefabs ship `m_Flags 2052`, no LOCAL anywhere** |

Tower prefab real paths: `Prefabs/Structures/Infrastructure/Towers/TransmitterTower_01/TransmitterTower_01{,_medium,_small}_base.et`.

---

## Phase 1 — As Built (2026-08-15)

**⚠️ Plan correction (verified by orchestrator, not just the agent): towers are NOT LOCAL.**
Plan §1 finding 4, §3.4 and §5 D3 assumed the towers have no `RplComponent` because the *grandparent*
`Tower_Base.et` has none. But Overthrow's tower `.et.meta` GUIDs equal the vanilla prefab GUIDs
(`{68CA807F237FFB7A}` / `{01E409EF2B75692E}` / `{4DE187A3508D5A46}`) — they are same-GUID **deltas** over
vanilla `TransmitterTower_01{,_medium,_small}_base.et`, each of which carries an `RplComponent`
(vanilla `_base.et:129` = `{59F4D7DB7CCA3C99}`). LOCAL + inherited RplComponent would null the component
with one ERROR per tower (`SCR_EditableEntityComponent.c:2243`) → zero tower icons.
**As built: all five prefabs `m_Flags 2052`** (NON_DELETABLE + HAS_FACTION). Consequences:
- `IsReplicated()→false` is the **only** movement lock for towers (no free LOCAL lock).
- Towers count toward the SYSTEMS budget too (plan R8 cost is wider than stated).
- Phase 6 must correct `implementation.md` §1/§3.4/§5 D3. (Project memory "same-GUID prefabs are deltas" strikes again.)

**Shipped values (all five prefabs):** `m_EntityType SYSTEM`, `m_bAutoRegister ALWAYS`,
`m_fMaxDrawDistance 20000`, `m_Flags 2052`; `m_vIconPos` town `0 10 0`, base `0 8 0`,
towers `0 40 0` / `0 25 0` / `0 6 0`; kinds TOWN/BASE/RADIO_TOWER.

**GUIDs minted (`{6B09…}`):** `{6B09A1C0E4D50001}`/`0002` town, `0003`/`0004` base, `0005`/`0006` tower,
`0007`/`0008` medium, `0009`/`000A` small (component + empty authored `m_UIInfo` per prefab);
`{6B09A1C0E4D50101}`…`0104` for the `.st` items (`OVT-GMIcon_RadioTower`, `_Base`, `_BaseGeneric`, `_Town`).

**Implementation decisions (agent, reviewed):**
- `CopyFrom()` runs **before** `Configure()` — CopyFrom assigns Icon/IconSetName from the source and would erase the icon otherwise.
- Each prefab authors an empty `m_UIInfo OVT_GMCampaignUIInfo {}` so CopyFrom inherits container-initialised budget arrays (`GetEntityBudgetCost` iterates them one frame after registration).
- **Lazy icon resolution** via `override SetIconTo()` — town controllers get their `OVT_TownData` after spawn, so a fixed OnPostInit icon would be a village forever. Town size read from `OVT_TownData.size` (locally discovered), not `OVT_TownControllerComponent.m_Size` (prefab default on clients).
- **Faction retint = refresh-on-next-lookup** for towns/towers (bases live via vanilla affiliation invoker). `m_OnTownControlChange` NOT wired: fires server-side only (client path `RpcDo_SetTownFaction` doesn't invoke it), declared `ScriptInvoker<IEntity>` but invoked with `OVT_TownData` (type hazard), and would add per-entity subscriptions with init-ordering dependencies.
- `GetTownName()` called only when the cached name is non-empty (its empty-cache path dereferences a map-marker query unguarded at `:858-867`).

**Gates (orchestrator-run):**
- `compile-check.sh` exit 0 (6098 files, +3).
- **Can-fail proof:** mutated all three families at once (town block removed; base `m_Flags 4`; small tower `m_Flags 2060`) → `run-tests.sh OVT_TEST_InitSuite` → exactly the 4 new cases failed by name (4 of 42), rest green → prefabs restored byte-identical from backup. Note: bare *case* class is not a valid `-autotest` target — use the **suite** class (`OVT_TEST_InitSuite`).
- **Fast group: OK (143 tests, 41 s)** — includes the 4 new Init cases, clean.
- Load log: zero `flagged as LOCAL` / `missing RplComponent` lines (Q-9 for the test world).

---

## Phase 2 — As Built (2026-08-15)

**Files:** `UI/Layouts/GM/GMIconDetail.layout` + `.meta` (27 widgets, all pre-authored, no `AlignableSlot`,
chrome copied from GMPanel rows); `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c` (SELECTED-filter +
seam subscriber, four-call panel contract in class Doxygen); `Scripts/Game/UI/GM/OVT_GMIconFormat.c`
(pure statics, world-free incl. comments); `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMIconFormat.c`;
modded `SCR_EditModeEditorUIComponent.c` (+CallLater(CreateDetail,0), +HandlerDeattached removing the queued
call — needed so closing EDIT inside the deferral frame can't invoke a destroyed component); 16 `.st` items.

**GUIDs:** layout resource `{6B09A1C0E4D50200}`; widgets `0202`–`0229` (rows at `0210+3N`); `.st`
`0105`–`0114`. `0201` unused (root widget carries no GUID, matching GMPanel). Duplicate-GUID script: clean.

**Notable decisions:** `FormatOriginType`/`FormatUpgradeType` return readable English (internal identifiers,
not prose — only way a Logic case pins an exact string); `FormatOrigin(-1)` returns the reason alone (type
name only when reason empty); base Location row renders "east, north" whole metres; `SetFailure` caps at 3
format args (engine limit, matches GMPanelFormat precedent).

**Gates (orchestrator-run):**
- `compile-check.sh` exit 0 (6101 files).
- **Logic can-fail proof:** all 9 documented mutations applied at once → `OVT_TEST_Logic_GMIconFormat` failed
  named ("support 248 of 400 formatted as '54%', expected '62%'"); second run with only the tail mutation →
  failed named ("origin with no index formatted as 'QRF -1 counter-attack'…") proving head AND tail assertions
  live; file restored byte-identical. (A Logic case aborts at first SetFailure, so per-assertion proof runs
  are not economical — head+tail brackets the case.)
- **Fast group: 144/144 SUCCESS per autotest logs — but the tool verdict was TIMEOUT, twice.** Both runs: every
  case (incl. the 5 new) in `autotest_succeded.log`, 0 failures in `autotest.log`, then the client hangs at
  shutdown ("Audio -> Stop", never reaches resource teardown) and is killed at 300 s. The identical world +
  prefab set exited cleanly at 17:13 (41 s), and Phase 2 ships zero world-side changes (layout/widget/statics
  only — none load in autotest), so this is judged an environmental client-shutdown wedge, **not** a
  regression. ⚠️ Watch item: if the Phase 3 gate also times out, escalate to the user instead of absorbing it.
- Grep gates (all feature files): zero `Rpc(` / `[RplProp]` / `RplRcver` / `ClearDetail(`; Logic-tier purity
  grep clean.

---

## Phase 3 — As Built (2026-08-15)

**Files:** `OVT_GMDetailUIComponent.c` (Classify() → SEL_CAMPAIGN/SEL_GROUP/SEL_PLAYER/SEL_NONE; RenderGroup,
RenderPlayer, ResolveOriginLabel, ResolveTownName, ResolveMoney helpers); 8 new `.st` items
(`{6B09A1C0E4D50115}`–`011C`).

**Notable decisions:**
- Player name is the **title**, not a row (would duplicate the vanilla info name).
- `TOWER_GUARD` origin index is a **base** index (tagged from `OVT_BaseUpgradeTowerGuard`, a base upgrade);
  only `RADIO_TOWER_GARRISON` carries a tower id.
- `CAMP_GARRISON`/`FOB_GARRISON` fall through to `FormatOrigin` ("Camp Garrison 2") — no display name exists.
- Reason row suppressed when origin index is `-1` (FormatOrigin already shows the reason — no double print).
- "No Overthrow record" path is print-free by design (acceptance criterion: zero console output).
- Pre-existing duplicate GUIDs in the string table (not `{6B09…}`, not ours): `{5D5C558A6E391A20}`,
  `{5D86A310C893DBDE}`, `{5D86A310C893DBDF}`, `{A1B2C3D4E5F60003}` — flagged for a future cleanup, untouched.

**Gates:** `compile-check.sh` exit 0 (6101 files); networking/ClearDetail grep zero across all feature files.
**Fast group: skipped — pure UI-script + localization phase, the suites cover none of it** (per
`.claude/test-policy.md`; it runs again after Phase 4, which touches world-side code).

---

## Phase 4 — As Built (2026-08-15)

**Files:** `OVT_GMCampaignUIInfo.c` — `GetDescription()` override dispatching on kind (town: support+stability
local; base: faction local + garrison count from the seam, degrading to faction alone on ANY miss; tower:
Online / "Sabotaged - m:ss" via `GetDisabledRemaining()` accessor + `OVT_GMPanelFormat.FormatCountdown`);
every path non-empty (`#OVT-GMIcon_Tooltip_NoData` fallback — the HasDescription trap is documented in the
class header). `OVT_TEST_Init_GMIcons.c` — fifth case `InfoHasNameAndDescription` guarding that trap.
Five `.st` items `{6B09A1C0E4D5011D}`–`0121`. **No `Configs/` change, no new file, no timer** (tooltip is
written once per hover by design).

**Notable:** one line per tooltip (two allowed, none needed; `<br/>` in RichText remains available); plain `-`
separator matching the `.st` house style; base garrison is the only seam-gated tooltip value — unauthorized
players see correct local data, no new path around gm-state's authorization; faction-label helper deliberately
duplicated between the info class and the widget component (promote to a static on a third copy).

**Gates (orchestrator-run):**
- `compile-check.sh` exit 0.
- **Can-fail proof (5th Init case):** `GetDescription() → string.Empty` mutation → `OVT_TEST_InitSuite`
  failed exactly `InfoHasNameAndDescription` with the named HasDescription message (1 of 43); restored
  byte-identical.
- **Fast group: OK (145/145, 38 s, clean client exit).** The Phase 2 shutdown-wedge watch item did **not**
  recur across three subsequent runs (Init 37 s, Init 38 s, Fast 38 s) — judged transient/environmental,
  closed.
- Grep gates still zero.

---

## Key Files

### New (this feature)
- `Scripts/Game/Components/GM/OVT_GMCampaignUIInfo.c` — per-instance UI info: icon, name, tooltip description; `OVT_EGMIconKind` enum (gm-map reuses)
- `Scripts/Game/Components/GM/OVT_GMEditableCampaignComponent.c` — `SCR_EditableSystemComponent` subclass: faction, movement lock
- `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c` — SELECTED-filter subscriber; renders detail into the panel's slot
- `Scripts/Game/UI/GM/OVT_GMIconFormat.c` — pure statics (world-free, Logic-testable)
- `UI/Layouts/GM/GMIconDetail.layout` + `.meta` — fixed authored rows, GUID series `{6B09…}`
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMIcons.c`, `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMIconFormat.c`

### Modified
- `Prefabs/Controllers/OVT_TownController.et`, `OVT_BaseController.et` — component block, flags 2052
- `TransmitterTower_01{,_medium,_small}_base.et` — component block, flags 2060 (+LOCAL, no RplComponent)
- `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` — +~6 lines (detail layout injection)
- `Language/localization_Overthrow.st`

### Contracts consumed
- overthrow-panel detail seam: `GetInstance()` / `GetDetailSlot()` / `ShowDetail(bool)` — never cache, never `ClearDetail()` (`overthrow-panel/context.md`)
- gm-state store: `HasData()`, `FindBase`, `FindGroup`, `m_aBases`, `m_aBaseUpgrades`, `m_aGroups` via `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()` — never `OVT_Global`; never ask the seam for already-replicated data

---

## Important Decisions

Plan §5 D1–D10 are the settled decisions (vanilla editable-entity ride-along; SYSTEM type; flags 2052/2060;
per-instance `SCR_UIInfo` tooltips with zero config fork; `IsReplicated()→false` movement lock; zero new
networking; fixed authored rows). Record here only decisions made *during* implementation.

---

## Gotchas & Learnings

_(from plan; append implementation findings)_

1. **Four silent failure modes in prefab blocks** — wrong flag arithmetic, `WHEN_SPAWNED` auto-register, LOCAL/RplComponent mismatch, unset draw distance. The Init test is the only gate; Q-9 reads the world-load log.
2. **`SetInfoInstance()` stores a weak ref** — component must hold `ref OVT_GMCampaignUIInfo`.
3. **SYSTEM draw-distance default is 1000 m** → 316 m radius at ground altitude coefficient. All prefabs author `m_fMaxDrawDistance 20000`.
4. **Logic-tier grep guard reads comments** — no `GetGame()`/`OVT_Global` anywhere in `OVT_GMIconFormat.c` or its test, including prose.
5. **`{6B09` grep needs the brace** — bare `6B09` false-hits inside unrelated GUIDs.

---

## Conventions for gm-map (reuse, do not re-invent)

- **Kind enum:** `OVT_EGMIconKind { TOWN, BASE, RADIO_TOWER }` in
  `Scripts/Game/Components/GM/OVT_GMCampaignUIInfo.c` — add values there, never a second enum.
- **Icon assets:** imageset `{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset`, images
  `village`/`town`/`city` (by `OVT_TownData.size`), `tower`; bases use the vanilla texture
  `{DD5F23CBB1731598}…/EditableEntity_System_Base.edds` (same as Conflict bases).
- **Format helpers:** `OVT_GMIconFormat` (support %, population, upgrade/origin names — pure statics,
  Logic-tested) and `OVT_GMPanelFormat.FormatCountdown` (durations). Do not duplicate either.
- **Detail row model:** title + subtitle + 8 label/value rows + note, all pre-authored, `SetText` only —
  `UI/Layouts/GM/GMIconDetail.layout` is the reference implementation.
- **Join keys:** base = base index (`GetBaseIndex`), group = `RplComponent.Id()`, town = townID
  (bounds-check before `GetTownName`), tower = nearest-by-position.
- **GUID series:** hud-icons owns `{6B09…}` (high-water: `{6B09A1C0E4D50229}` widgets,
  `{6B09A1C0E4D50121}` strings). gm-map must mint a different series.

---

## Triage: "the GM sees no icons"

1. Editor is an **unlimited** editor? (Icons exist only in Edit/Admin/Strategy/CampaignBuilding modes —
   reaching one is the authorization gate, plan D4.)
2. Prefab block present? → run `OVT_TEST_InitSuite` — the five `OVT_TEST_Init_GMIcons_*` cases are the gate
  for component presence, SYSTEM type, NON_DELETABLE, the LOCAL/RplComponent invariant and HasDescription.
3. World-load log: any `flagged as LOCAL, but contains RplComponent` / `missing RplComponent` line
   (`SCR_EditableEntityComponent.c:2243/:2252`)? Either **nulls the component silently** — zero icons for
   that family, nothing else says why.
4. Icons visible near ground but not at altitude? → `m_fMaxDrawDistance` didn't take (must be 20000 in the
   prefab; the type default 1000 m shrinks to a 316 m radius at low altitude coefficient).
5. All present but no detail on click? → gm-state triage in `docs/features/gm/gm-state/context.md`
   (seam null / no snapshot / unauthorized client).

---

## Needs Human Verification

- Phase 1 smoke check (icons visible/protected) — 🖐️
- Phase 5 full verification method (§6) — 🖐️

---

## Session Notes

### 2026-08-15 16:40
- Feature started via /autorun-feature (Discord). Docs scaffolded from implementation.md (7 phases, 26 tasks).

### 2026-08-15 18:50
- Phases 0–4 built and gated in one autonomous run (advanced agent for Phase 1, ui-developer for 2–3,
  component-developer for 4). All can-fail proofs done by the orchestrator with prefab/file backups —
  restored byte-identical, verified by diff.
- **Tower LOCAL-flag correction** (the run's headline finding) verified independently against the vanilla
  tree before acceptance; plan §1/§3.4/§5 D3 annotated in place.
- Transient Fast-group client shutdown hang (2×, all tests green in logs) did not recur; closed as
  environmental.
- Phase 1 smoke check and Phase 5 batched to one user session (user opted to keep building).
- `run-tests.sh` learning: a bare *case* class is not a valid `-autotest` target — use the suite class.
