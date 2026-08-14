# Controller Migration - Task Checklist

**Last Updated:** 2026-08-14
**Progress:** 56/56 tasks complete (100%) — all 10 phases built and gate-verified 2026-08-14.

**⚠️ Advanced-agent phases:** P1 (`network-specialist-advanced`), P7 (`component-developer-advanced`), P10 (`network-specialist-advanced`).
**Every phase gate:** compile-check exit 0 · Fast ≥ 101 · All ≥ 142 · `GetServer()` count at/below phase ceiling · `grep -n "Rpc(Rpc"` arity hand-check on touched files.
Tasks marked **[USER]** are Workbench work the agent cannot do; they block play-testability only, never the compile/test gates.

---

## Phase 1: Seam hardening (9/9 complete) ✅ 2026-08-14 — `network-specialist-advanced`

- [x] ✅ **T1.1 Generic accessor** — `OVT_ControllerComponent.c` created, §3.2 verbatim.
- [x] ✅ **T1.2 Delete the six per-domain getters** — done; 26 calls + 6 comment mentions re-pointed; getter count 23 → 17 (forwarders only).
- [x] ✅ **T1.3 Inline `LootBattlefield()`** — moved into `OVT_OccupyingFactionManager`, wrapper deleted.
- [x] ✅ **T1.4 Null-guard `GetUI()` and `GetDifficulty()`** — plan's counts were mentions: GetUI had 0 live sites, GetDifficulty 3 (one now-reachable null fixed to fail closed).
- [x] ✅ **T1.5 Continue ≠ connect** — `OVT_TEST_Campaign_ContinueControllerRespawn` added (proven to fail); trap (a) fixed via `GetController()` revalidation; trap (b) fixed with `RplComponent.DeleteRplEntity` (first use in tree). USER listen-host play-test still owed.
- [x] ✅ **T1.6 Shared caller-identity base** — `OVT_ControllerRequestComponent.c` created; ShopTransaction + AdminCommands re-pointed. NOTE: 3 more duplicates found (TravelRequest, RespawnRequest, TowerSabotage) — deliberately deferred, see Discovered Tasks.
- [x] ✅ **T1.7 Controller cache** — `s_LocalController` with identity guard + world-transition clear in `PlayerManager.Init()`; fallback untouched (D9).
- [x] ✅ **T1.8 Idempotent consumers** — guard flag + Remove-before-Insert; contract doc comments rewritten.
- [x] ✅ **T1.9 Skill doc fixed** — `overthrow-controller.md` + `ui-contexts.md`.
- [x] ✅ **[USER] P1 play-test** — GREEN 2026-08-14. — SP/listen host: save → quit → Continue → balance, shop purchase, camp menu. _(tracked in context.md; not counted)_

**Gates:** compile 0 (6038 files) · Fast **102** (+1) · All **144** (+2) · getters 17 · GetServer 60 (unchanged, correct) · Prefabs/Language/Persistence untouched. Verified by orchestrator.

## Phase 2: Vehicles (5/5 complete) ✅ 2026-08-14 — `network-specialist`

- [x] ✅ **T2.1 `OVT_VehicleRequestComponent`** — all 6 handlers; upgrade/repair gained ownership/proximity validation; upgrade charge moved server-side (P2-2 — the menu's separate debit raced the new check); repair confirmed free (P2-3); §3.6(a) fixed in BuyVehicle.
- [x] ✅ **T2.2 Re-point call sites** — all 7 (plan said 8; `OVT_ShopContext:1357` is P4's — see P2-1).
- [x] ✅ **T2.3 Monolith handlers deleted** — 2,001 → 1,733 lines; P4's TakePlayerMoney/TakeFromInventory untouched.
- [x] ✅ **T2.4 Init case** — proven to fail by removing the prefab block (message names the prefab); restored green.
- [x] ✅ **T2.5 Phase gates** — compile 0, Fast 103, All 145, GetServer **53** (= corrected ceiling, P2-1), arity 6/6 hand-checked.
- [x] ✅ **Prefab wired in text** — `OVT_VehicleRequestComponent "{6A8F2C7D4E13A590}"` added to `OVT_OverthrowController.et` (agent, fresh GUID). **[USER] verify it opens clean in Workbench** _(tracked in context.md)_.

**Note:** all later GetServer ceilings inherit +1 (P3 ≤ 44, P4 ≤ 32, P5 ≤ 17, P6 ≤ 13, P7 ≤ 6, P8 = 0 still holds if P4 absorbs the extra `OVT_ShopContext` site).

## Phase 3: Real estate + warehouses (6/6 complete) ✅ 2026-08-14 — `component-developer`

- [x] ✅ **T3.1 `OVT_RealEstateRequestComponent`** — 8 handlers; warehouse RPCs validate via `IsRegisteredResource(ResourceName)` (string form — warehouses key by ResourceName, and an unregistered string was both a junk save row AND an arbitrary client-chosen prefab spawn); `SetBuildingHome` deleted, disposition noted in `economy/real-estate/context.md`.
- [x] ✅ **T3.2 Warehouse helpers moved** onto `OVT_RealEstateManagerComponent` (+null guards on blind RplComponent derefs); gone from `OVT_Global.c`; `OVT_ContainerTransferComponent` re-pointed to the manager.
- [x] ✅ **T3.3 Call sites re-pointed** — RealEstateContext ×6, SetHomeAction, WarehouseContext, manager client branches.
- [x] ✅ **T3.4 Monolith handlers deleted** — 1,733 → 1,515 lines.
- [x] ✅ **T3.5 Init case** — proven to fail via prefab-block removal; restored green.
- [x] ✅ **T3.6 Phase gates** — compile 0, Fast 104, All 146, GetServer **43** (ceiling 44), arity 8/8 hand-checked.
- [x] ✅ **Prefab wired in text** — `"{6A9B3E14C27D08F6}"` (unique). **[USER] verify in Workbench.**

**Note:** buy/sell/rent/set-home were silent no-ops for a listen-server host on the legacy path (same class as P2-5) — now fixed; play-test at P10. New flake variant: exit 124 with an EMPTY autotest.log = launch flake, re-run once (documented in context.md).

## Phase 4: Economy, money, shop purchase (6/6 complete) ✅ 2026-08-14 — `network-specialist`

- [x] ✅ **T4.1 `RpcAsk_BuyItems` on `OVT_ShopTransactionComponent`** — 30 m gate, server-derived unit cost, affordability, spawn/insert/equip loop, partial + failure notifications; **§3.6(a) fixed** (direct `DoTakePlayerMoney` + `shop.TakeFromInventory` + stream); added `IsValidResourceId` and `num` 1..100.
- [x] ✅ **T4.2 `RpcAsk_AddToInventory`/`RpcAsk_TakeFromInventory` → server-side methods on `OVT_ShopComponent`**; RPCs deleted. The two open-coded copies (`RestockShop`, `TakeFromShopStock`) now delegate — three definitions of "mutate stock and stream" collapse to one (P4-6).
- [x] ✅ **T4.3 `OVT_EconomyRequestComponent`** — 7 handlers + `RpcDo_DoneTakingMoney` (listen-server-safe via `ShouldRespondLocally`, always-reply contract kept); **§3.6(b)** direct notifications; BUG-016 same-tick target resolution in `ResolveTransferTarget`; `SellDrugs` keeps its one-item shape (P4-5).
- [x] ✅ **T4.4 Call sites re-pointed** — ShopContext ×1 (P4-1: the second was P2's), ResistanceMenuContext ×3, SellDrugsAction, CharacterSheetContext, ShopComponent ×2, EconomyManager client branches ×2; 11 monolith handlers/wrappers + 3 helpers deleted (1,515 → 1,122 lines).
- [x] ✅ **T4.5 Init case** — proven to fail via prefab-block removal; restored green. No `maxAttempts`.
- [x] ✅ **T4.6 Phase gates** — compile 0, Fast **105**, All **147**, GetServer **33** grep lines / **31** live calls (P4-1: the plan's 32 is unreachable arithmetic); arity 9/9 hand-checked including the `Replication.IsServer()` direct-call twins; **§3.6 verdicts recorded (P4-2/P4-3) and filed as BUG-161 + BUG-162; docs/wiki pass at P10 is now REQUIRED (P4-4)**.
- [x] ✅ **Prefab wired in text** — `"{6AA7D8B502E4193C}"` (unique repo-wide). **[USER] verify in Workbench.**

**Note:** buy, donate, send money/funds, tax and buy-skill were ALL silent no-ops for a listen-server host on the legacy path (same class as P2-5) — now fixed; play-test at P10. §6 steps 2 and 8 are the acceptance evidence for BUG-161/BUG-162.

## Phase 5: Resistance ops + FOBs (5/5 complete) ✅ 2026-08-14 — `component-developer`

**Gates verified by orchestrator:** compile 0 · Fast **107** · All **149** · GetServer **18** lines / 17 live (zero resistance/FOB-shaped; plan's ≤16 unreachable per P2-1/P4-1 arithmetic) · `OVT_UndeployFOBAction_New.c` deleted · prefab GUIDs `{6AB3E9C61FD5720A}`/`{6ABF4A73D8261E95}`. Headline: `DeleteCamp` was an **arbitrary-entity delete** for any client (RplId never validated against the camp record) — now proven-at-camp + ownership/officer + proximity.

- [x] **T5.1 `OVT_ResistanceRequestComponent`** — PlaceItem, RemovePlacedItem, BuildItem, AddOfficer, AddGarrison, ConvertSupporter (+ owner-response result; BUG-063 checks verbatim); position-sanity checks added to the bare forwards. 🔴
- [x] **T5.2 `OVT_FOBRequestComponent`** — AddGarrisonCamp, AddGarrisonFOB, DeployFOB, UndeployFOB, SetPriorityFOB (+officer), SetCampPrivacy, DeleteCamp (ownership/officer/proximity added — both currently zero-validation). 🔴
- [x] **T5.3 Re-point call sites** — contexts/actions per §4/P5 list; **delete `OVT_UndeployFOBAction_New.c`**; delete monolith handlers. 🟡
- [x] **T5.4 Init cases ×2**; proven able to fail. 🟢
- [x] **T5.5 Phase gates** (GetServer ≤ 16) + arity check. 🟢
- [x] ✅ (Workbench green 2026-08-14) **[USER] Wire both components** _(added in text, GUIDs `{6AB3E9C61FD5720A}` / `{6ABF4A73D8261E95}`; user to confirm in Workbench)_ _(not counted)_

## Phase 6: Recruits (4/4 complete) ✅ 2026-08-14 — `component-developer`

**Gates verified:** compile 0 · Fast **108** · All **150** · GetServer **14** lines / 13 live (zero recruit-shaped; the plan's ≤12 is the same P2-1/P4-1 arithmetic, see P6-1) · prefab GUID `{6ACB5B84E9372FA1}` · monolith 826 → **642** lines. Headline: **dismiss had no ownership check at all** — any client could permanently delete any player's recruit by an id that is broadcast to every client.

- [x] **T6.1 `OVT_RecruitRequestComponent`** — RecruitCivilian, RecruitFromTent, RenameRecruit, DismissRecruit (**added the missing ownership check**). 🟡
- [x] **T6.2 Re-point** `OVT_RecruitCivilianAction`, `OVT_RecruitFromTentAction`, `OVT_RecruitsContext` ×2; delete monolith handlers. 🟡
- [x] **T6.3 Init case**; proven able to fail. 🟢
- [x] **T6.4 Phase gates** (GetServer ≤ 12 → **14**, see P6-1) + arity check. 🟢
- [x] ✅ (Workbench green 2026-08-14) **[USER] Wire `OVT_RecruitRequestComponent`** _(added in text, GUID `{6ACB5B84E9372FA1}`; user to confirm in Workbench)_ _(not counted)_

## Phase 7: Loadouts + possession (5/5 complete) ✅ ⚠️ ADVANCED — `component-developer-advanced`

- [x] **T7.1 `OVT_LoadoutRequestComponent`** — SaveLoadout, LoadLoadoutFromBox, DeleteLoadout; client-supplied `playerId` params **dropped from signatures**; BUG-043 ownership; 20 m both-ends box check; no no-box endpoint reappears. 🔴
- [x] **T7.2 `OVT_PossessionRequestComponent`** — SetPossessedEntityAndOpenInventory (+ownership/proximity added), `RpcDo_OpenInventory` **Broadcast → Owner**; client lifecycle carried verbatim (one-shot invoker, 300 ms CallLater — BUG-147). 🔴
- [x] **T7.3 Re-point** `OVT_LoadoutsContext` ×3, `OVT_SaveLoadoutAction`, `OVT_SaveOfficerLoadoutAction`, `OVT_OpenInventoryCommand`; delete monolith handlers + `RpcAsk_RestorePossessedEntity` (§3.7). 🟡
- [x] **T7.4 Init cases ×2**; proven able to fail. 🟢
- [x] **T7.5 Phase gates** (GetServer ≤ 5) + arity check. 🟢
- [x] ✅ (Workbench green 2026-08-14) **[USER] Wire both components** _(not counted)_

## Phase 8: Jobs + campaign actions (5/5 complete) ✅ 2026-08-14 — `component-developer`

- [x] **T8.1 `OVT_JobRequestComponent`** — AcceptJob, DeclineJob (`playerId` dropped, public/private ownership branch carried, jobIndex bounds-checked, decline iterates a snapshot — see P8-1). 🟡
- [x] **T8.2 `OVT_CampaignRequestComponent`** — StartBaseCapture (BUG-025 verbatim, **vector dropped**), InstantCaptureBase → non-RPC `#ifdef WORKBENCH` method **on the game mode** (P8-2), DeliverMedicalSupplies (+proximity + `PlayerMayUseVehicle`), LootWantedCheck (character re-derived from the caller + life check), RequestSave (+officer check, invoker contract preserved) + `RpcDo_SaveResult` owner response via `ShouldRespondLocally`. 🔴
- [x] **T8.3 Re-point** `OVT_JobManagerComponent` ×2, `OVT_CaptureBaseAction`, `OVT_DeliverMedicalSuppliesAction`, `OVT_MainMenuContext` ×2 (incl. the `:14` doc comment), `OVT_PlayerWantedComponent`, DiagMenu; monolith emptied to a bare shell and the `SendNotification` pair deleted (§3.7). 🟡
- [x] **T8.4 Init cases ×2**; proven able to fail (both blocks removed → exactly 2 of 34 red). 🟢
- [x] **T8.5 Phase gates** (**GetServer = 0 lines**; `RpcAsk_SendNotification` gone from `Scripts/`) + arity check. 🟢
- [x] ✅ (Workbench green 2026-08-14) **[USER] Wire both components** _(added in text, GUIDs `{6AEF8EB71C6A50D4}` / `{6AFB9FC82D7B61E5}`; user to confirm in Workbench)_ _(not counted)_

## Phase 9: `OVT_Global` utility split (6/6 complete) ✅ 2026-08-14 — `component-developer`

- [x] **T9.1 `OVT_WorldUtils`** — `Scripts/Game/Utilities/OVT_WorldUtils.c`, 14 helpers + 3 private statics (`s_SpawnPointSearchResults`, `m_Bodies`, `FilterDeadBodiesAndWeapons`), moved byte-identical; `NearestPlayer` deleted (0 callers, verified repo-wide unqualified). 🟡
- [x] **T9.2 `OVT_PrefabUtils`** — `Scripts/Game/Utilities/OVT_PrefabUtils.c`, all four moved byte-identical. 🟡
- [x] **T9.3 `OVT_LoadoutUtils`** — `Scripts/Game/Utilities/OVT_LoadoutUtils.c`; `SpawnDefaultCharacterItem` moved (0 external callers, but a live dependency of `ApplyCivilianLoadout` — P9-5); `RandomizeCivilianGroupClothes` deleted (0 callers). 🟢
- [x] **T9.4 Thin forwarders** — the three are one-line `return`s with signatures (and default parameters) preserved exactly; **63** other call sites across **36** files re-pointed. Real counts in P9-1. 🔴
- [x] **T9.5 `ShowHint` + `GetLocalPersistentId` stayed on `OVT_Global`**; the test spine needed **no** edit — every utility use in it is forwarded (P9-4). 🟢
- [x] **T9.6 Phase gates** — compile 0; Fast **112** exit 0; All **154** exit 0; `OVT_Global.c` **302** lines (< 400); `git status --porcelain Prefabs/` unchanged by this phase. 🟢

## Phase 10: Delete the monolith + sweep (5/5 complete) ✅ 2026-08-14 ⚠️ ADVANCED — `network-specialist-advanced`

**Gates:** compile **0** (6059 files, 5 s) · Fast **112** exit 0 (36 s) · All **154** exit 0 (41 s), **no flakes, both green first try** · `OVT_PlayerCommsComponent` 0 lines · `GetServer()` 0 lines · getters **16** (not 17 — P10-1) · `OVT_Global.c` **292** lines · `git diff --stat Language/` = only `.st` · persistence untouched. Headline: **the monolith is deleted**, and the arity audit caught a listen-host routing defect in `OVT_TowerSabotageComponent` that P2-P8 could not have found because that component was never migrated.

- [x] ✅ **T10.1 Delete `OVT_PlayerCommsComponent.c` and `OVT_Global.GetServer()`** — plus **both prefab blocks stripped in text** (2 lines each; delta-format checked first, P10-2). 🟡
- [x] ✅ **T10.2 Repo sweep** — both greps return **0**. Cost **63** doc-comment rewordings across 33 files, not the plan's 7 (P10-4); standard phrase is "the legacy comms monolith". 4 `.st` `Comment` lines re-pointed via a script that aborts if a non-`Comment` line would change; **no `.<lang>.conf` touched**. Every cited fact still held; nine _co-cited_ line numbers had drifted and were corrected (P10-5). 🟡
- [x] ✅ **T10.3 Arity audit across all 20 files in `Components/Controller/`** — **75 marshalled `Rpc()` calls, 75 direct twins, count AND order hand-verified, zero arity defects**; no dead endpoints, no cross-component marshalling. Found and fixed one **routing** defect: `OVT_TowerSabotageComponent` had no `Replication.IsServer()` branch (P10-3). Full table recorded in context.md as the Q10 audit record. 🟡
- [x] ✅ **T10.4 Update docs** — `epic-overview.md` (status, rows :23/:27, build-order §5, dependencies, rollup ×2); `game-mode/` context.md ×4 + implementation.md ×9 + tasks.md ×1; `overthrow-controller.md` (P1's checklist verified + extended; "Old Pattern"/"Migration Guide"/DON'T/roster sections de-staled); `SKILL.md`; `ui-contexts.md`; both component-developer agent docs. 🟢
- [x] ✅ **T10.5 Final gates** — §6 automated verification items 1-10 all run and reported; item 6 is **16 and 16 is correct** (P10-1: the probe was counting `GetServer()` itself). 🟢
- [x] ✅ (2026-08-14: prefabs load clean, ALL 21 STEPS GREEN) **[USER] Open BOTH stripped prefabs + `OVT_OverthrowController.et` in Workbench; run the §6 21-step MP play-test.** _(not counted — the only gate left on the feature)_

---

## Bugs & Issues

**Filed by this feature:** **BUG-161** (item buying was FREE for clients and a total no-op for a listen host — §3.6(a) verdict, P4-2) and **BUG-162** (three economy notifications never fired — §3.6(b) verdict, P4-3). §6 steps 2 and 8 are their acceptance evidence.
**Watching:** ~~§3.6(a)/(b) verdicts~~ — resolved and filed in P4. **P4-4 makes the docs/wiki pass REQUIRED**, because BUG-161 means an economy that did not charge is starting to charge: that is a player-facing change. Run `help-docs-sync` after the MP play-test confirms it.
**Found in P10, fixed in P10:** `OVT_TowerSabotageComponent.RequestSabotage()` was an unconditional `Rpc()` to an `RplRcver.Server` handler — radio-tower sabotage did nothing at all for a listen-server host (P10-3). Not filed as a bug because it was fixed in the same change; it is in the play-test list.
**Found and fixed in the post-completion validation audit (2026-08-14):** **BUG-166** — `OVT_ContainerTransferComponent`'s six `RplRcver.Server` handlers had no caller resolution, no proximity, no ownership and no radius bound, so a modified client could empty any replicated storage (including another player's locked truck) into any other from anywhere, and could hand the server an unbounded sphere-query radius. Open, high, code-derived; the play checks that confirm it are in the bug file.

## Discovered New Tasks

- [x] ✅ (2026-08-14, post-play-test — user-approved) **Fold the 3 remaining `ResolveOwningPlayerId` duplicates onto `OVT_ControllerRequestComponent`** — `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent`, `OVT_TowerSabotageComponent` still carry private copies (found in P1/T1.6; deferred as play-test surface). **Deliberately NOT folded into P10** — it is a hierarchy change with real play-test surface, and P10's whole point is that nothing moves. `OVT_TowerSabotageComponent.c`'s own comment still reads "candidate for a shared base class when a third controller component needs it"; there are now seventeen. Follow-up. 🟡
- [x] ✅ (2026-08-14, `network-specialist-advanced`) **Audit the pre-existing controller components the migration never touched.** P10-3 found a listen-host routing defect in `OVT_TowerSabotageComponent` — one of the seven components that were already on the controller before this feature started. P2-P8 audited what they _moved_; nobody had audited what was already there. The other six (`OVT_ContainerTransferComponent`, `OVT_ShopTransactionComponent`, `OVT_TutorialComponent`, `OVT_AdminCommandsComponent`, `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent`) now pass the arity + twin check, but only their `Rpc()` seams were examined — their _validation_ was never diffed against anything. 🟡
  - **Done:** all **13** `RplRcver.Server` handlers diffed against the §3.4 ladder and Q4/Q5/Q9. Full table + triage in `context.md` → "Pre-migration component validation audit (2026-08-14)".
  - **Headline: `OVT_ContainerTransferComponent` — all six handlers were bare forwards** with no caller resolution at all, i.e. "move the contents of any replicated storage on the map into any other, from anywhere", plus two unbounded client-supplied sphere-query radii. Filed as **BUG-166** (open, high) and fixed in place: caller from the controller entity, 30 m to both ends, the shared lock rule on both ends, bounded radii, named rejections.
  - `OVT_TowerSabotageComponent` **hardened** (missing `IsServer` guard + five silent rejections now logged). `OVT_ShopTransactionComponent` ×3, `OVT_AdminCommandsComponent`, `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent` **conforming**; `OVT_TutorialComponent` and `OVT_BaseServerProgressComponent` have **no client→server surface at all**.
  - Gates: compile 0 · Fast **112** · All **154**, no flakes. No `Rpc()` signature changed. Six accepted-debt items recorded (A6).

---

_Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered._
