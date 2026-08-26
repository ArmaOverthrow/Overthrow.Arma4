# Tutorial System - Task Checklist

**Last Updated:** 2026-08-24 (post-close change set 3 — the seen-state push never ran for a returning player)
**Progress:** 61/61 tasks complete (100%) · **2 tasks cancelled by risk R3** (5.4, 6.8) · **F5 deferred**

**Epic:** `new-player-experience` (feature #1 of 5) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md`

> Task ids match the `<phase>.<n>` ids in `implementation.md` — do not renumber them.
> **Agent tiers are set by the plan:** Phases **5, 6 and 7 are ADVANCED** (`ui-developer-advanced`). Phase 3 routes to `network-specialist`. Phases 0, 1, 2, 4 and 8 route to `component-developer`.
> Every phase ends with `tools/compile-check.sh` clean. Phases 0–4 additionally end with `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) green.

---

## Phase 0: Invoker gaps and wrong doc comments (5/5 complete) ✅ — `component-developer`

*Small, but it edits four managers other systems listen to — done first and alone so any ripple is unambiguous.*

- [x] ✅ **0.1 — Correct the two wrong economy doc comments**
  - Description: `m_OnPlayerBuy` → `Args: int playerId, int actualCost`; `m_OnPlayerSell` → `Args: int playerId, int total`. Leave `m_OnPlayerTransaction`'s comment alone (already accurate, invoker live at two call sites).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c:100-101`
  - Estimate: 🟢 10 min

- [x] ✅ **0.2 — `m_OnPlayerSkill` carries `playerId`**
  - Description: Change to `ScriptInvoker<int>`. Update both invoke sites (`:119` server path, `:340` local-player branch of `RpcDo_SetPlayerSkill`). Update the single listener with an explicit `OnSkillChanged(int playerId)` wrapper calling `Refresh()` — **do not** rely on arity coercion.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c`, `Scripts/Game/UI/Context/OVT_CharacterSheetContext.c:33`
  - Estimate: 🟢 30 min

- [x] ✅ **0.3 — Add the wanted-level-changed invoker**
  - Description: `static ScriptInvoker<int, int, int> GetOnWantedLevelChanged()` — args `(playerId, newLevel, oldLevel)`. **Static** (the `SCR_MapEntity.GetOnMapOpen()` pattern) because the component is per-character and respawns (D13).
  - File(s): `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c`
  - Estimate: 🟢 20 min

- [x] ✅ **0.4 — Fire the wanted invoker from `SetBaseWantedLevel`**
  - Description: Inside the existing `if(m_iWantedLevel < level)` block, after `Replication.BumpMe()` and beside `SendWantedNotification(reason)` (`:62-69`). Resolve `playerId` exactly as `SendWantedNotification` does (`:147`); return early for recruits (`if(!m_PlayerData) return;`). **Do not** fire from `SetWantedLevel` (the raw setter the decay path uses).
  - File(s): `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c`
  - Estimate: 🟢 20 min

- [x] ✅ **0.5 — Grep-verify no other listener of any changed invoker exists**
  - Description: `grep -rn "m_OnPlayerSkill" Scripts/` must show exactly two invoke sites and one listener, all consistent.
  - File(s): —
  - Estimate: 🟢 10 min

**Acceptance:** compile-check exit 0; All group green; gaining a wanted level in-game still produces the existing notification and no duplicate; buying/selling still award skill XP and move the stability/support modifiers.

**✅ Verified 2026-08-07:** `tools/compile-check.sh` exit 0 (5922 files); `tools/run-tests.sh "{6A6E2A002F53A581}"` exit 0 (66 tests) — both match baseline. In-game re-test of the wanted/skill/economy paths is owed (see **Needs Human Verification**).

**Two corrections the phase made to the plan:**
1. 🔎 **A fifth site the task list did not name** — `OVT_CharacterSheetContext.c:184` (`OnClose`) held the matching `Remove` for the `m_OnPlayerSkill` subscription. Leaving it pointed at `Refresh()` while `Insert` moved to the new `OnSkillChanged(int)` wrapper would have leaked the subscription on every sheet close. Both sites now point at the wrapper.
2. 🔎 **A third `m_OnPlayerSell` invoke site** — `OVT_EconomyManagerComponent.c:1004`, inside `AddPlayerMoney(playerId, amount, doEvent = true)`. Signature `(playerId, amount)` matches the corrected doc comment, so 0.1 still holds — **but Phase 2 must know that a `PLAYER_SELL` tutorial trigger will also fire on non-shop money grants** routed through `AddPlayerMoney`. Fold this into the §5 trigger-catalog note for `PLAYER_SELL` (Phase 8.5 doc pass).

---

## Phase 1: Config schema + pure decision logic + Logic tests (8/8 complete) ✅ — `component-developer`

*Nothing here touches a manager, a widget or the world.*

- [x] ✅ **1.1 — Trigger primitives**
  - Description: `OVT_TutorialEvent` enum, `OVT_TutorialEventContext : Managed`, `OVT_TutorialTrigger : ScriptAndConfig` with base `Matches()` implementing event + `m_iMinValue` + `m_sFilter` (virtual, per D3).
  - File(s): `Scripts/Game/Configuration/OVT_TutorialTrigger.c`
  - Estimate: 🟡 1 h

- [x] ✅ **1.2 — Entry config schema**
  - Description: `OVT_TutorialPresentation` enum, `OVT_TutorialPage`, `OVT_TutorialEntryConfig` (`configRoot: true`, modelled on `OVT_JobConfig.c:9-46`).
  - File(s): `Scripts/Game/Configuration/OVT_TutorialEntryConfig.c`
  - Estimate: 🟡 1 h

- [x] ✅ **1.3 — `OVT_TutorialMatcher.FindMatches()`**
  - Description: Enabled entries whose *any* trigger matches, ordered by descending priority then declaration order, no duplicates in the result, empty array (not null) on no match.
  - File(s): `Scripts/Game/Data/OVT_TutorialMatcher.c`
  - Estimate: 🟡 1 h

- [x] ✅ **1.4 — `OVT_TutorialQueue`**
  - Description: Priority-then-FIFO, duplicate-rejecting, capped at `MAX_QUEUE = 8` — over-cap enqueues dropped and counted, never silently overwriting an existing item.
  - File(s): `Scripts/Game/Data/OVT_TutorialQueue.c`
  - Estimate: 🟡 1 h

- [x] ✅ **1.5 — `OVT_TutorialGate.CanShowNow()`**
  - Description: `static bool CanShowNow(bool tipsDisabled, bool alreadyShowing, bool blockingUiOpen, bool playerAlive)`.
  - File(s): `Scripts/Game/Data/OVT_TutorialGate.c`
  - Estimate: 🟢 20 min

- [x] ✅ **1.6 — `OVT_TutorialSeenStore`**
  - Description: In-memory set + `m_iVersion`; `HasSeen`, `MarkSeen` (idempotent), `LoadFrom`/`WriteTo` over plain arrays, cap at 512 ids with a one-time warning. **No engine calls.**
  - File(s): `Scripts/Game/Data/OVT_TutorialSeenStore.c`
  - Estimate: 🟡 1 h

- [x] ✅ **1.7 — Logic suite, five cases**
  - Description: `TriggerMatching`, `MatcherSelectsAndOrders`, `QueueOrdering`, `GatePredicate`, `SeenStore` (§9). Every subject built with `new` and hand-written values; no `OVT_Global`, no `GetGame().GetGameMode()`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Tutorial.c`
  - Estimate: 🔴 3 h

- [x] ✅ **1.8 — Prove each new case red once; record the method**
  - Description: Fallibility rule — a case that has never failed is not evidence. No `maxAttempts`, ever.
  - File(s): `context.md`
  - Estimate: 🟡 1 h

**Acceptance:** compile-check clean; Fast group green with five additional cases; the Logic-tier grep rule holds on the new file.

**✅ Verified 2026-08-07:** compile-check exit 0 (5929 files); **Fast 43** (38 + 5); **All 71** (66 + 5); the `OVT_Global|GetGame()` grep on the new Logic file returns nothing. All five cases proven red once — record in `context.md`.

**One schema correction:** `OVT_TutorialSeenStore.LoadFrom` takes `(array<string> ids, int version)`, not the plan's one-arg sketch — a version mismatch cannot be detected without the incoming version.

**Two findings Phases 2+ must respect:**
1. 🐛 **`array<T>.Remove(index)` does not retain order** — it swaps the last element into the hole (`Types.c:260`). Ordered removal must use `RemoveOrdered`. This was the *real* defect that made `_QueueOrdering` go red, and it would have been invisible in play until two tips queued at different priorities.
2. 🔎 **`set<T>` is sorted, not insertion-ordered.** `WriteTo` therefore emits canonical order. Harmless (nothing reads the seen list positionally) and it makes the stored profile block diff-stable — but do not assume insertion order anywhere downstream.

**Three EnforceScript parser notes** (cost real time; worth knowing): `out` and `event` are **reserved words** — both are rejected as local/parameter names; `SetResultFailure` accepts **at most three** format arguments.

---

## Phase 2: Server manager and invoker subscriptions (8/8 complete) ✅ — `component-developer`

*Escalate to `component-developer-advanced` if Phase 0 turned up listeners beyond the one documented.*

- [x] ✅ **2.1 — `OVT_TutorialManagerComponent` skeleton + registry**
  - Description: `s_Instance`/`GetInstance()`, `Init(owner)`, `PostGameStart()`; entry map build with duplicate/empty-id validation that logs an error **naming the offender**.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c`
  - Estimate: 🟡 2 h

- [x] ✅ **2.2 — Subscribe to all nine server-side invokers**
  - Description: Behind a `Replication.IsServer()` guard. Catalog is §5 of the plan.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c`
  - Estimate: 🔴 3 h

- [x] ✅ **2.3 — Per-event playerId resolution**
  - Description: Including `OVT_RecruitData.m_sOwnerPersistentId` → runtime id via `OVT_Global.GetPlayers()`.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c`
  - Estimate: 🟡 1 h

- [x] ✅ **2.4 — `SendToPlayersNear(pos, radius, entryId)`**
  - Description: For the two global events. Radii as named constants: `NEAR_RADIUS_TOWN = 500`, `NEAR_RADIUS_BASE = 300` (D11).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c`
  - Estimate: 🟡 1 h

- [x] ✅ **2.5 — Per-session sent-set keyed on persistent id**
  - Description: `map<string, ref set<string>>`; never cleared, allocated in the constructor (D5).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c`
  - Estimate: 🟢 30 min

- [x] ✅ **2.6 — Game-mode wiring**
  - Description: Field + `Init(this)` beside the other managers (`:1089-1161`) + `PostGameStart()` in `DoStartGame()` (`:198-245`); add the component to the prefab with the entry array; add `OVT_Global.GetTutorialManager()`.
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`, `Prefabs/GameMode/OVT_OverthrowGameMode.et`, `Scripts/Game/Global/OVT_Global.c`
  - Estimate: 🟡 1 h

- [x] ✅ **2.7 — Init-tier coverage**
  - Description: Extend `OVT_TEST_Init_Globals_ManagersResolve`'s getter list; add `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` and `OVT_TEST_Init_Tutorial_InvokerSeamsExist`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟡 2 h

- [x] ✅ **2.8 — Prove the new Init cases red once**
  - File(s): `context.md`
  - Estimate: 🟢 30 min

**Acceptance:** compile-check clean; All group green; the new getter is in the Init suite's null sweep; starting a campaign logs the entry count once and no duplicate-id error; no invoker subscription happens on a client.

**✅ Verified 2026-08-07:** compile-check exit 0; **All 73** (71 + 2); **Fast 45**; `git diff --stat Language/` empty. Both new Init cases proven red (record in `context.md`).

**GUIDs minted** (block `6B3A`, re-verified unused): `{6B3A000000000001}` conf · `…0002` page · `…0003` trigger · `…0010` component on prefab · `…0011` entry element.

**Registry validation is terminal**, per §5's "the manager refuses to start": a null/empty/duplicate id logs a named error per offender, clears the map, and **subscribes no invoker at all that session**. Loud by design.

**Three §5 catalog corrections:**
1. 🔎 **"Nine server-side invokers" is actually ten** — eight per-player rows plus the two global ones. All ten are subscribed; only the plan's count was wrong.
2. 🐛 **`PLAYER_SKILL` cannot carry a skill key** — §5 promises `m_sFilter` = skill key, but Phase 0.2 made `m_OnPlayerSkill` a `ScriptInvoker<int>` carrying playerId only. Plan-internal contradiction between §5 and task 0.2. **Resolved as new task 3.0** (widen to `<int, string>`) — both invoke sites already have `key` in scope, so it is a 4-line fix, and I6 requires §5 to be accurate against shipped code.
3. 🐛 **`m_OnTownControlChange` is declared `ScriptInvoker<IEntity>` but invoked with an `OVT_TownData`** (`OVT_TownManagerComponent.c:143` vs `:676`). Both pre-existing listeners take `OVT_TownData`; the handler follows them. **Do not trust `ScriptInvoker<T>` declarations in this codebase** — read the invoke site.

**Scope note:** the proof entry's `#OVT-Tutorial_EconomyFirstBuy_Title`/`_Body` keys are deliberately not in `localization_Overthrow.st` yet — that is task 8.4, and nothing renders them until Phases 5/6 exist.

---

## Phase 3: Owner-RPC delivery and the client pipeline (8/8 complete) ✅ — `network-specialist`

*This is the authority boundary and the JIP-correctness surface.*

- [x] ✅ **3.0 — Contract correction: `m_OnPlayerSkill` carries the skill key** *(added 2026-08-07, from Phase 2's finding #2)*
  - Description: Widen to `ScriptInvoker<int, string>` = `(playerId, skillKey)`. Both invoke sites already have `key` in scope (`:120`, `:341`). Update the `OnSkillChanged` wrapper in `OVT_CharacterSheetContext` (**both** the `Insert` and the `Remove`) and the manager's `PLAYER_SKILL` handler to populate `m_sFilter`. Restores §5's published contract, which I6 requires to be accurate against shipped code.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c`, `Scripts/Game/UI/Context/OVT_CharacterSheetContext.c`, `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c`
  - Estimate: 🟢 30 min

- [x] ✅ **3.1 — `OVT_TutorialComponent` + Owner RPC**
  - Description: `OVT_TutorialComponent : OVT_Component` with `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] void RpcDo_ShowTutorial(string entryId)` and the server-side `Notify(entryId)` wrapper.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟡 2 h

- [x] ✅ **3.2 — Register on the controller prefab + `OVT_Global.GetTutorials()`**
  - Description: Fresh GUID from the reserved `6B3A0000…` block.
  - File(s): `Prefabs/GameMode/OVT_OverthrowController.et`, `Scripts/Game/Global/OVT_Global.c`
  - Estimate: 🟢 30 min

- [x] ✅ **3.3 — Client receive path**
  - Description: seen check → tips-disabled check → `OVT_TutorialQueue.Enqueue` → 1000 ms `CallLater` pump → `OVT_TutorialGate` → `m_OnShowTutorial.Invoke(entry)`.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🔴 3 h

- [x] ✅ **3.4 — `blockingUiOpen` computation**
  - Description: New `OVT_UIManagerComponent.IsAnyContextActive()` (loop `m_aContexts`, any `IsActive()`), plus `GetMenuManager().GetTopMenu() != null` and `SCR_MapEntity.GetMapInstance().IsOpen()`.
  - File(s): `Scripts/Game/Components/Player/OVT_UIManagerComponent.c`, `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟡 1 h

- [x] ✅ **3.5 — `FireLocalEvent(OVT_TutorialEventContext)`**
  - Description: Client-local triggers feed the same matcher and the same queue.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟡 1 h

- [x] ✅ **3.6 — Client-local trigger hooks**
  - Description: `SCR_MapEntity.GetOnMapOpen()`, an `OVT_UIContext.ShowLayout` notification for `MENU_OPENED`, and `OVT_OverthrowGameMode.OnPlayerSpawnedLocal` for `PLAYER_SPAWNED`. **Leave the existing `#OVT-IntroHint` call in place** — removing it is `first-spawn`'s task (I7).
  - File(s): `Scripts/Game/UI/OVT_UIContext.c`, `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`
  - Estimate: 🟡 2 h

- [x] ✅ **3.7 — Null-guard the unassigned-controller window**
  - Description: `RpcDo_NotifyOwnerAssignment` is async. A dropped local trigger is acceptable; a script error is not (Q7).
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟢 30 min

**Acceptance:** compile-check clean; All group green; the RPC receive point fires on exactly one client in a two-client session; the queue holds an entry while a menu is open and releases it after close; nothing new on `OVT_PlayerCommsComponent` (Q9).

**✅ Verified 2026-08-07:** compile-check exit 0; **All 73** (unchanged — this phase adds no test cases); `git diff --stat Language/` empty. **Q9 satisfied:** `OVT_PlayerCommsComponent` RPC count 132 before and 132 after, and the file is not in `git status` at all. `OVT_OverthrowController.et` **does** have an `RplComponent` (`{65C4B2D3DE955867}`), so the Owner RPC can route. GUID minted: `{6B3A000000000020}`.

**🔑 The host needs a direct-call branch.** `Notify()` guards `Replication.IsServer()`, then **if the controller belongs to the local player (listen-server host) it calls `RpcDo_ShowTutorial` directly instead of `Rpc(...)`.** The engine never loops an RPC back to the sender — without this branch the host would receive **zero** tips. Instrument `Receive()`, not `RpcDo_ShowTutorial`, when verifying, or the host's path is invisible.

**Parser note:** `map` is a **reserved type name** and cannot be used as a local variable — same family as `out` and `event`.

**Two risks carried forward into later phases:**
1. ⚠️ **`PLAYER_SPAWNED` is flaky on first spawn.** It is pushed from `OnPlayerSpawnedLocal`, which runs from a 0 ms `CallLater`; if `RpcDo_NotifyOwnerAssignment` has not landed yet, `OVT_Global.GetTutorials()` is null and the trigger is silently dropped by design (Q7). DoD step 16 ("each client gets its own `welcome-intro`") will be flaky until this retries. **Added as task 4.6.** `MAP_OPENED`/`MENU_OPENED` fire late enough not to hit this.
2. ⚠️ **JIP (DoD step 20 / I5) is not honestly testable before Phase 4** — the seen store is in-memory only, so a reconnecting client re-shows what it dismissed, and the server's persistent-id sent-set then suppresses the re-send. The two interact to make a reconnect show nothing at all. Resolved by Phase 4.

---

## Phase 4: Per-machine settings store (6/6 complete) ✅ — `component-developer`

*Task 4.4 is a hard gate: do not build UI on an unverified store.*

- [x] ✅ **4.1 — `OVT_TutorialSettings : ModuleGameSettings`**
  - Description: `m_iVersion`, `m_bTipsDisabled`, `ref array<ref OVT_SeenTutorialEntry> m_aSeen` (D7 — the `SCR_FilterSetStorage` shape, which has direct base-game precedent).
  - File(s): `Scripts/Game/Global/OVT_TutorialSettings.c`
  - Estimate: 🟢 30 min

- [x] ✅ **4.2 — `OVT_TutorialSettingsAccessor`**
  - Description: get-module null guard, `WriteToInstance` LOAD, **mandatory** lazy array allocation, `ReadFromInstance` SAVE, `UserSettingsChanged()` + `SaveUserSettings()` (D8), `System.IsConsoleApp()` early-out. **No parallel arrays, ever.**
  - File(s): `Scripts/Game/Global/OVT_TutorialSettingsAccessor.c`
  - Estimate: 🟡 2 h

- [x] ✅ **4.3 — Version handling**
  - Description: On `m_iVersion` mismatch, clear `m_aSeen` and rewrite at the current version (Q2).
  - File(s): `Scripts/Game/Global/OVT_TutorialSettingsAccessor.c`
  - Estimate: 🟢 30 min

- [x] ✅ **4.4 — 🚧 SERIALIZATION GATE (R1) — PASSED**
  - Description: Prove the nested-struct array round-trips through the real `BaseContainer` store. Automatable half: an engine-tier test case that writes two ids + the flag, saves, re-reads, and asserts; then inspect the on-disk `ReforgerGameSettings.conf` for the `OVT_TutorialSettings` block. Cross-restart half is a human play-test. If the shape does not round-trip, fall back per R1 (`array<ResourceName>` → delimited string → enum array) before continuing.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`, `$profile:.save/settings/ReforgerGameSettings.conf`
  - Estimate: 🔴 3 h

- [x] ✅ **4.5 — Wire the accessor into `OVT_TutorialComponent`**
  - Description: Load once on first use, mark-seen on dismiss, write the tips-disabled flag on toggle.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟡 1 h

- [x] ✅ **4.6 — Retry the `PLAYER_SPAWNED` push until the controller resolves** *(added 2026-08-07, from Phase 3's risk #1)*
  - Description: `OnPlayerSpawnedLocal` runs from a 0 ms `CallLater` and can beat the async `RpcDo_NotifyOwnerAssignment`, leaving `OVT_Global.GetTutorials()` null and the trigger silently dropped. Bounded retry (a few `CallLater` attempts, then give up quietly) so DoD step 16 is deterministic. Must stay a *silent* give-up — a dropped tip is acceptable, a script error is not (Q7).
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` or `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟢 45 min

**Acceptance:** compile-check clean; All group green; the store round-trips and the file is human-readable; a headless server run produces no settings access in the log.

**✅ Verified 2026-08-07:** compile-check exit 0 (5933 files); **All 74** (73 + 1); **Fast 46**; `git diff --stat Language/` empty.

### 🟢 R1 IS DISCHARGED — no fallback needed

D7's nested-struct shape round-trips. Verbatim from `…/My Games/OverthrowCI/profile/.save/…/settings/ReforgerGameSettings.conf` after the gate run:

```
 OVT_TutorialSettings OVT_TutorialSettings {
  m_iVersion 1
  m_bTipsDisabled 1
  m_aSeen {
   OVT_SeenTutorialEntry "{6A0D2B3A6F7BED44}" { m_sId "__ovt-selftest-alpha" }
   OVT_SeenTutorialEntry "{6A0D2B3A6F7BED39}" { m_sId "__ovt-selftest-beta"  }
  }
 }
```

Both ids, the flag and `m_iVersion 1`, on disk, human-readable, beside `SCR_AllFilterSetsStorage`'s structurally identical block. Declaring the class **was** the entire registration contract — no config entry needed. **Proven red** by removing `[Attribute()]` from `m_aSeen` (R1's actual failure mode, not a synthetic break): *"The seen store came back with 0 ids, expected 2. The nested ref array did NOT survive the settings container - risk R1 has fired…"*, exit 1.

**One plan correction:** `GetGame().GetGameUserSettings()` returns **`UserSettings`**, not `BaseContainer` — `GetModule` does not exist on the base type. The plan's §4 sketch is wrong on that line.

### ⚠️ Two engine findings that change what D8 buys

1. 🐛 **`SaveUserSettings()` is throttled and *drops* rather than defers.** Measured: two flushes microseconds apart leave only the **first** on disk, and 35 further seconds never flush the second; two flushes six seconds apart both land. **This is survivable only because `Save` writes the whole record rather than a delta** — a dropped flush loses nothing permanently. **Residual exposure:** an unclean exit within a few seconds of a *second* dismissal costs one repeated tip. D8's "flush on every mutation" is still right; it just is not the guarantee the plan assumed.
2. 🔎 **A settings block omits members equal to their attribute default** — the clean state is `{ m_iVersion 1  m_bTipsDisabled 0 }` with no `m_aSeen` key at all. The gate additionally asserts that writing a value back to its default really does clear a stored one, because otherwise "Don't show tips again" would be a **one-way toggle**.

**Profile hygiene:** the gate writes only `__ovt-selftest-*` ids (leading underscores are illegal in the authored id scheme, so no content can collide) and calls `Reset()` on every path including failure — with a 10 s wait before the cleanup write, forced by finding #1. That is why it is the slowest Init case.

**4.6's bound:** `TUTORIAL_SPAWN_PUSH_ATTEMPTS = 10` × `TUTORIAL_SPAWN_PUSH_RETRY_MS = 500` — a hard 5-second ceiling, then a **silent** give-up (a dedicated server and a player who quits mid-countdown hit that path every spawn, so a log line there would be pure noise).

**✅ Verified 2026-08-07:** compile-check exit 0; **All 74** (73 + the new gate case); **Fast 46**; `git diff --stat Language/` empty.

**🟢 R1 IS DISCHARGED.** The nested `ref array<ref OVT_SeenTutorialEntry>` round-trips through the real settings container AND lands on disk. Block observed in `OverthrowCI`'s `ReforgerGameSettings.conf`:

```
 OVT_TutorialSettings OVT_TutorialSettings {
  m_iVersion 1
  m_bTipsDisabled 1
  m_aSeen {
   OVT_SeenTutorialEntry "{6A0D2B3A6F7BED44}" {
    m_sId "__ovt-selftest-alpha"
   }
   OVT_SeenTutorialEntry "{6A0D2B3A6F7BED39}" {
    m_sId "__ovt-selftest-beta"
   }
  }
 }
```

No fallback is needed. `array<ResourceName>` / delimited string / enum array can be struck from the plan's R1.

**Two findings the plan did not anticipate — see context.md gotchas 16 and 17:** `SaveUserSettings()` is throttled and DROPS (does not defer) a second call inside its window; and a `ModuleGameSettings` block omits members that equal their `[Attribute]` default, so an "empty" block is the clean state rather than a missing one.

**Still owed to humans:** the cross-restart half of the smoke test (quit, relaunch, confirm the ids are still there and the tip does not return), the kill-the-process variant that D8 exists for, and a headless-server log check that no settings access occurs.

---

## Phase 5: Non-modal HUD overlay (7/7 complete) ✅ ⚠️ **ADVANCED** — `ui-developer-advanced`

*New layout, a new `SCR_InfoDisplay`, and — the risky part — a **gameplay-context** keybinding. Console usability is decided here.*

- [x] ✅ **5.1 — `UI/Layouts/HUD/TutorialPopup.layout` (+ `.meta`, fresh GUIDs)**
  - Description: Title, body, optional image, one `SCR_InputButtonComponent` prompt. Frame modelled on `UI/Layouts/HUD/ProgressInfo.layout`; content (backdrop colour, heading/body text bases, countdown bar, small nav button) modelled on the base game's own `UI/layouts/HUD/Hint/Hint.layout`, which is the nearest thing in either tree to what this popup is.
  - File(s): `UI/Layouts/HUD/TutorialPopup.layout`, `UI/Layouts/HUD/TutorialPopup.layout.meta`
  - Estimate: 🔴 3 h

- [x] ✅ **5.2 — `OVT_TutorialInfo : SCR_InfoDisplay`**
  - Description: `OnStartDraw` caches widgets + subscribes to `OVT_Global.GetTutorials().m_OnShowTutorial`; `OnStopDraw` unsubscribes and removes both timers and the button handler; hidden by default; **every** `FindAnyWidget` result null-guarded.
  - File(s): `Scripts/Game/UI/HUD/OVT_TutorialInfo.c`
  - Estimate: 🟡 2 h

- [x] ✅ **5.3 — Register under `SCR_BaseHUDComponent.InfoDisplays`**
  - Description: Fourth entry beside `OVT_WantedInfo` / `OVT_EconomyInfo` / `OVT_ProgressInfo`. `m_bAdaptiveOpacity 0` so a bright scene cannot fade a tip out of legibility.
  - File(s): `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et:162-168`
  - Estimate: 🟢 30 min

- [x] ❌ **5.4 — One action + one passive action context — R3 FALLBACK TAKEN, no conf change**
  - Description: **No genuinely free gamepad input exists.** Full survey below. `Configs/System/chimeraInputCommon.conf` is **unmodified**; the popup's prompt is bound to the existing `OverthrowMainMenu` action instead, which already has both a keyboard (`KC_U`) and a gamepad (`gamepad0:pad_down`) source and is already live every frame through `OverthrowGeneralContext`.
  - File(s): — (deliberately none)
  - Estimate: 🟡 2 h

- [x] ✅ **5.5 — Auto-dismiss + hide-on-UI**
  - Description: `AUTO_DISMISS_MS = 20000`, counted down on a 100 ms `CallLater` that also polls the three blocking-UI facts. Both routes end in `NotifyDismissed(entryId)`, so the entry is marked seen either way (F4).
  - File(s): `Scripts/Game/UI/HUD/OVT_TutorialInfo.c`
  - Estimate: 🟡 1 h

- [x] ✅ **5.6 — Escalation to the modal presentation**
  - Description: `OnMoreActivated()` is the seam, marked `// PHASE 6:`. It cancels the auto-dismiss (`StopTick()` before the release) and today retires the popup, because under R3 the escalation route is the Overthrow main menu that the same key is already opening.
  - File(s): `Scripts/Game/UI/HUD/OVT_TutorialInfo.c`
  - Estimate: 🟡 1 h

- [x] ✅ **5.7 — `#OVT-` keys for the prompt label**
  - Description: One key, `OVT-Tutorial_MoreInMenu` = "Overthrow Menu", in `Language/localization_Overthrow.st` only. **Needs a Workbench string-table export before play-testing** or the button renders the raw key.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 15 min

**Acceptance:** overlay legible at 1080p and 4K, never captures movement/aim/fire, dismissed by the timer, hidden the instant the map or a menu opens, and the one prompt is reachable and glyphed on both keyboard and gamepad.

**✅ Verified 2026-08-07:** compile-check exit **0** (5934 files); **All 74**, exit **0**; `check-input-conflicts.py --warnings` exit **0** — **0 errors, 22 warnings, 12 pre-existing, 3 acknowledged**, byte-identical to the recorded baseline; `git diff --stat Language/` shows `localization_Overthrow.st` and nothing else (Q11).

**GUIDs minted** (reserved `6B3A` block, re-verified unused before use): `{6B3A000000000021}` info-display element on the player prefab · `{6B3A000000000030}` layout resource · `{6B3A000000000031}`…`{6B3A00000000003C}` the twelve layout widgets · `{6B3A000000000060}` the string-table item.

### 🔴 5.4 — R3 has fired. There is no free gamepad input, and the fallback was taken.

The survey (both `chimeraInputCommon.conf` files, combo-aware — `InputSourceCombo` groups parsed as single expressions rather than flattened):

- **Keyboard is fine.** `KC_K`, `KC_O` and `KC_T` are bound by **nothing** in either conf. A keyboard-only action is forbidden by the project's UI rules, so this alone does not solve it.
- **Every one of the sixteen gamepad inputs is bound in at least one context that can be live while a non-modal HUD popup is on screen** (on foot, in a vehicle, in a turret, or in Overthrow's own placement/build modes).
- The only two that base gameplay leaves alone **as single presses** are `gamepad0:shoulder_left` and `gamepad0:shoulder_right` — and both are `OverthrowRotateLeft` / `OverthrowRotateRight` in `OverthrowPlaceContext` and `OverthrowBuildContext`.
- Those two Overthrow contexts are **not** covered by the pipeline gate. `OVT_PlaceContext.StartPlace` (`:398-435`) calls `CloseLayout()` — clearing `m_bIsActive`, so `IsAnyContextActive()` is **false** — and *then* sets `m_bPlacing = true`, after which `OnFrame` activates `OverthrowPlaceContext` every frame. `OVT_BuildContext` does the same (`:352-366`, `:53-79`). So a popup **can** be on screen while LB/RB rotate a ghost, and a `PLAYER_PLACE` tip is precisely the entry that fires in that state.
- `gamepad0:shoulder_left` additionally sits under vanilla's `VONMenu` combo (`shoulder_left + x`, live every frame via `SCR_VONController.Update`), and `shoulder_right` under `WeaponOpticsZoomIn/Out` (`pad_up/pad_down + shoulder_right`).
- Chords do not help: `InputSourceCombo` layers *over* its constituent single inputs rather than suppressing them (vanilla ships `pad_left + y` for `HintDismiss` on top of live `pad_left` and `y`), so a chord inherits every collision of its parts.

**Decision:** take R3's documented fallback rather than ship a collision. No `OverthrowTutorialOpen`, no `OverthrowTutorialContext`, no conf change at all — which is also why the conflict script is byte-identical to baseline rather than merely "no new errors".

**What is lost:** **F5** ("the single documented key escalates a non-modal popup into the modal popup"). The modal does not exist until Phase 6, so nothing is lost *today*; the escalation route becomes Overthrow main menu → (Phase 6) Tips.

**What replaces it:** the popup's one `SCR_InputButtonComponent` is bound to the **existing** `OverthrowMainMenu` action. It is already live in `OverthrowGeneralContext` every frame, already has a keyboard **and** a gamepad source, and opens the main menu exactly as it always did — the popup only adds a handler that retires itself. Nothing is stolen, no context is activated, and **F3 holds by construction: this class registers no input listener of its own and calls `ActivateContext` nowhere.**

**Handed to Phase 6:** revisit the binding with the modal in hand. If a gameplay binding is still wanted then, the least-bad candidate is `keyboard:KC_T` + `gamepad0:shoulder_left`, and the collision it must close is Overthrow's own place/build rotate — closable by hiding the prompt while `OVT_PlaceContext.m_bPlacing` / `OVT_BuildContext.m_bBuilding` are set (a hidden `SCR_InputButtonComponent` refuses its keybind), and it must then be recorded in the conflict script's `ACKNOWLEDGED` with that mechanism named.

---

## Phase 6: Modal popup and sequence (8/8 complete) ✅ ⚠️ **ADVANCED** — `ui-developer-advanced`

*A 17th `OVT_UIContext` alongside 16 existing ones, a new modal action context, and the multi-page primitive `first-spawn` depends on.*

- [x] ✅ **6.1 — `UI/Layouts/Menu/TutorialPopup.layout` (+ `.meta`)**
  - Description: Centred 1000×620 panel modelled on `ManageVehicleMenu.layout` — blur backdrop, header (field-manual icon + `TutorialTitle` + `TutorialPageIndicator`), Overthrow-orange `UpperStripe`, a `BodyRow` holding the optional `TutorialImage` and a wrapping `TutorialBody`, and a footer of five `WLib_NavigationButton`s.
  - File(s): `UI/Layouts/Menu/TutorialPopup.layout`, `UI/Layouts/Menu/TutorialPopup.layout.meta`

- [x] ✅ **6.2 — `OVT_TutorialContext : OVT_UIContext`**
  - Description: `SetEntry()` before `ShowLayout()`; page index; Learn-more hidden unless `m_sFieldManualTitleKey != ""`; Next/Back hidden on single-page entries; last-page Next closes; cannot page past either end. Pipeline subscription lives in `RegisterInputs`/`UnregisterInputs` (NOT `PostInit`/`OnShow`) because the context must be listening while closed and the invoker outlives the character.
  - File(s): `Scripts/Game/UI/Context/OVT_TutorialContext.c`

- [x] ✅ **6.3 — Registered in `m_aContexts` with no `m_sOpenAction`**
  - File(s): `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` — `{6B3A000000000022}`

- [x] ✅ **6.4 — `ActionContext OverthrowTutorialMenuContext`**
  - Description: `Priority 50`, `Flags 4`, listing `MenuBack`/`MenuSelect`/`MenuUp`/`MenuDown`/`MenuLeft`/`MenuRight` plus FOUR new actions (two more than the plan named — every on-screen control has to be a prompted `WLib_NavigationButton`, and Learn-more and Don't-show-tips are on-screen controls).
  - File(s): `Configs/System/chimeraInputCommon.conf`

- [x] ✅ **6.5 — Seen/disable wiring**
  - Description: Seen is marked in `OnClose`, not on the Dismiss button — `MenuBack` closes through the base class's own listener and never reaches a button handler. A close caused by death does NOT mark seen.
  - File(s): `Scripts/Game/UI/Context/OVT_TutorialContext.c`

- [x] ✅ **6.6 — Removed the debug `Print()` calls from `OVT_UIContext.ShowLayout()`**
  - Description: Ten, not eight. One of them called `CanShowLayout()` a second time purely to log it. **R7 sweep owed: re-open every existing menu once.**
  - File(s): `Scripts/Game/UI/OVT_UIContext.c`

- [x] ✅ **6.7 — Place/build gate gap closed**
  - Description: New virtual `OVT_UIContext.IsBlockingPopups()` (defaults to `IsActive()`), overridden by `OVT_PlaceContext` (`|| m_bPlacing || m_bRemovalMode`) and `OVT_BuildContext` (`|| m_bBuilding || m_bRemovalMode`). `OVT_UIManagerComponent.IsAnyContextActive()` renamed to `IsAnyContextBlocking()` and now asks that question; both tutorial call sites updated.
  - File(s): `Scripts/Game/UI/OVT_UIContext.c`, `OVT_PlaceContext.c`, `OVT_BuildContext.c`, `OVT_UIManagerComponent.c`, `OVT_TutorialComponent.c`, `OVT_TutorialInfo.c`

- [x] ❌ **6.8 — F5 escalation binding — STILL DEFERRED. R3 confirmed, not lifted.**
  - Description: See the survey below. `Configs/System/chimeraInputCommon.conf` gained no gameplay-context action and no `OverthrowTutorialContext`. The main-menu route was built instead and works.
  - File(s): — (deliberately none)

**✅ Verified 2026-08-07:** compile-check exit **0** (5935 files); **Fast 46**, **All 74**, both exit **0**; `check-input-conflicts.py --warnings` exit **0** — **0 errors, 23 warnings, 12 pre-existing, 3 acknowledged**; `git diff --stat Language/` shows only `localization_Overthrow.st`.

**The one new warning is structural and unavoidable:** `OverthrowTutorialMenuContext / gamepad0:pad_down / MenuDown` vs the always-active `OverthrowGeneralContext`. Every one of the 16 existing menu contexts that lists `MenuDown` produces exactly the same line, because `OverthrowMainMenu` is bound to `gamepad0:pad_down`. Any 17th menu context that obeys 6.4's "must list MenuUp/Down/Left/Right" adds one. See the note on `OverthrowMainMenu` below.

**GUIDs minted** (reserved `6B3A` block): `{6B3A000000000022}` context element on the player prefab · `{6B3A000000000040}` layout resource · `{6B3A000000000041}`…`{6B3A000000000053}` the nineteen layout widgets · `{6B3A000000000054}` the Tips button in `MainMenu.layout` · `{6B3A000000000061}`…`{6B3A00000000006A}` ten string-table items · `{6B3A000000000070}`…`{6B3A000000000083}` twenty input-source GUIDs in the conf.

### 🔴 6.8 — R3 re-surveyed and CONFIRMED. F5 stays deferred.

The brief's premise was that 6.7 would free `gamepad0:shoulder_left`, because its only live-under-a-popup collision was `OverthrowRotateLeft` in place/build. Re-running the survey **combo-aware AND inline-aware** falsifies that, and also falsifies "KC_T is verified unbound".

**The parsing hole.** The base-game conf declares **197 actions INLINE inside `ActionContext` blocks** (`ActionContext X { Actions { Action Y { … } } }`). They are live in that context but never appear in any `ActionRefs` list, so a survey that reads the top-level `Actions` block plus `ActionRefs` cannot see them. Phase 5's survey had that hole, and so does `check-input-conflicts.py`.

**What the inline-aware survey finds:**

| Input | Bound by | Context | Live when |
|---|---|---|---|
| `gamepad0:shoulder_left` | `VONChannel`, `VONDirectToggle` (plain press, not a combo) | `VONContext` **p110** | every frame the player is alive and conscious — `SCR_VONController.Update:963-964` |
| `gamepad0:shoulder_left` | `VONLongRangeToggle` (LB+A), `VONTransceiverCycle` (LB+B), `VONMenu` (LB+X) | `VONContext` / `VONMenuOpeningContext` | same |
| `gamepad0:shoulder_right` | `WeaponManipulation` | `CharacterWeaponContext` p10 | on foot with a weapon |
| `gamepad0:shoulder_right` | `Freelook`, `FocusToggle`, `FreelookReset` | `PlayerCameraContext` p40 | on foot |
| `keyboard:KC_T` | `VONDirect`, `VONDirectToggle` | `VONContext` **p110** | always — this is push-to-talk |
| `keyboard:KC_K` | `GadgetCompass` / `MapToolCompass` | `CharacterGeneralContext` p10 / `MapContext` p50 | always |
| `keyboard:KC_O` | `GadgetWatch` / `MapToolWatch` | `CharacterGeneralContext` p10 / `MapContext` p50 | always |

`VONContext` is `Priority 110` — above every Overthrow menu context (50) — and `Flags 0x2`, so it is not suppressed by a menu either.

**Verdict:** 6.7 removed one of `shoulder_left`'s collisions and left the bigger one standing. All sixteen pad inputs remain bound in a context that can be live under a non-modal HUD popup, and the three "free" keyboard keys are not free. **No action and no `ActionContext` were added.** The conflict script's only delta is the structural `pad_down` warning from the modal's own context.

**What replaces F5:** the Overthrow main menu now has a **Tips** entry. The HUD overlay's prompt (still the existing `OverthrowMainMenu` action) pushes its entry into `OVT_TutorialContext` and releases the pipeline **with `""`**, so the tip is not recorded as seen; the menu that the same key press is opening then offers Tips, which opens the modal on that entry. Tips is `SetEnabled(false)` when there is nothing to re-open, so it is never a dead button.

### ⚠️ Two pre-existing input bugs found while surveying (NOT fixed here)

1. **`OverthrowShopPrevCategory` and `OverthrowWarehouseTakeAll` sit on `gamepad0:shoulder_left`,** which `VONContext` holds at Priority 110 while those menus are open. Paging shop categories on a pad plausibly keys the radio. Invisible to `check-input-conflicts.py` (cross-context, and the VON actions are inline).
2. **`OverthrowMainMenu` on `gamepad0:pad_down` is the root of 17 of the 23 warnings and 2 of the 12 BASE entries.** Inside any Overthrow menu, d-pad-down both moves focus and toggles the Overthrow main menu, because `OverthrowGeneralContext` is activated every frame by `OVT_UIManagerComponent.EOnFrame`. Removing `OverthrowMainMenu` from `OverthrowMainMenuContext`'s `ActionRefs` would silence the report **without fixing the behaviour** — a stale waiver. The only real fix is rebinding the mod's primary open-menu pad input, which is a scheme change for the owner to approve, not a Phase 6 edit.

### ⚠️ One more pre-existing leak found (NOT fixed here)

`OVT_CharacterSheetContext` inserts into `OVT_SkillManagerComponent.m_OnPlayerSkill` in `PostInit` (`:33`) and removes it in `OnClose` (`:185`). After the first close it is unsubscribed for the rest of the character's life, so buying a skill stops refreshing an already-open sheet from the second visit onwards. `OVT_TutorialContext` deliberately uses `RegisterInputs`/`UnregisterInputs` instead, which is the symmetric pair.

---

## Phase 7: Field-manual seam (8/8 complete) ✅ ⚠️ **ADVANCED** — `ui-developer-advanced`

*Edits a **same-GUID override of a base-game config** and modded-classes a base-game menu. Getting it wrong takes the vanilla field manual with it. Fully parallelisable with Phases 1–6.*

- [x] **7.1 — 🚧 MERGE SPIKE (do first)**
  - Description: Confirm the same-GUID delta merges: are the five vanilla categories present alongside the Overthrow one, and do the tile backgrounds render? (They can only render if the delta merged — Overthrow never declares `m_aTileBackgrounds` and `SCR_FieldManualUI.c:253` dereferences it unguarded.) **Expected: yes to both.** Automatable half: load the root by GUID in a test case and count `m_aCategories`. **If vanilla categories are missing, STOP and re-plan** (R2 — that falsifies merge semantics five other configs rely on).
  - File(s): `Configs/FieldManual/FieldManualConfigRoot.conf`
  - Estimate: 🟡 1 h

- [x] **7.2 — Restructure to vanilla's shape**
  - Description: Move the category's content into a **fresh-GUID** `Configs/FieldManual/Categories/FM_Overthrow.conf`; reduce the same-GUID root delta to a single element inheriting from it, **keeping element GUID `{59908331EDFD9788}`** (that is what makes it an append). Re-verify 7.1 afterwards.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`, `Configs/FieldManual/FieldManualConfigRoot.conf`
  - Estimate: 🟡 2 h

- [x] **7.3 — Fix the two nits**
  - Description: Retitle the sub-category from vanilla's `#AR-FieldManual_Category_Introduction_Title` to a new `#OVT-FieldManual_Category_GettingStarted_Title` (it currently renders as a second button named "Introduction"); give the entry a real title key `tutorial-content` can link to.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`, `Language/localization_Overthrow.st`
  - Estimate: 🟢 30 min

- [x] **7.4 — `modded class SCR_FieldManualUI`**
  - Description: `OVT_OpenEntryByTitle(string titleKey)` walking `m_aAllEntries` on `entry.m_sTitle`, setting `m_bOpenedFromOutside` and calling `SetCurrentEntry`, falling back to `SetCurrentEntry(null)` on no match; plus `static OVT_OpenByTitle(string titleKey)` mirroring `Open()` (`:859-866`) **but null-guarding the `OpenMenu` result, which vanilla does not**.
  - File(s): `Scripts/Game/UI/Modded/SCR_FieldManualUI.c`
  - Estimate: 🟡 2 h

- [x] **7.5 — Respect the ordering constraint**
  - Description: Navigate **after** `OpenMenu` returns. `OnMenuShow` (`:153-162`) calls `SetCurrentEntry(null)`, so navigation from inside an `OnMenuOpen` override is silently undone.
  - File(s): `Scripts/Game/UI/Modded/SCR_FieldManualUI.c`
  - Estimate: 🟢 20 min

- [x] **7.6 — `OVT_FieldManualHelper.Open(string titleKey)`**
  - Description: The single call Overthrow code makes; logs a warning and falls back to the front page on an unknown key (I2).
  - File(s): `Scripts/Game/Global/OVT_FieldManualHelper.c`
  - Estimate: 🟢 30 min

- [x] **7.7 — Wire the modal popup's Learn-more button**
  - Description: Prove end-to-end with the proof entry linking to the Overthrow page.
  - File(s): `Scripts/Game/UI/Context/OVT_TutorialContext.c`
  - Estimate: 🟢 30 min

- [x] **7.8 — Record the contract for `field-manual`**
  - Description: The id contract (§5) plus the **two-category-level limit** (`SetAllEntriesAndParents`, `:600-663`).
  - File(s): `context.md`
  - Estimate: 🟢 20 min

**Acceptance:** six categories with tile backgrounds rendering; the Overthrow sub-category no longer named "Introduction"; the normal open path unchanged; Learn-more opens **on the Overthrow page**; a wrong key opens the front page with a warning, no crash. — **Config half asserted automatically; the rendering half is owed to a human.**

### 🟢 7.1 MERGE SPIKE VERDICT — the delta merges. **R2 is discharged.**

Converted from "launch and look" into permanent coverage: `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` loads `{17295EF80DC38D53}` through `SCR_FieldManualConfigLoader.LoadConfigRoot` — **the menu's own entry point, not a reimplementation.** Measured:

| | Measured |
|---|---|
| Top-level categories | **6** — Introduction, Editor, Conflict/MP, Gameplay, Equipment, **`#OVT-FieldManual_Category_Overthrow_Title`** |
| `m_aTileBackgrounds` | **8, populated** — Overthrow's file never declares it, so 8 can only mean the base root's value survived. `SCR_FieldManualUI.c:253` will not null-deref. |
| Entries reachable | **141** (140 vanilla + Overthrow's one) |

Re-measured identically after 7.2 and 7.3 (6 / 8 / 141).

**🔑 The merge mechanism was demonstrated, not inferred.** Red-proof #1 repointed the delta's element GUID at vanilla's Introduction element `{5668E0CC56064794}` — and Overthrow's category **replaced** Introduction (5 categories, Introduction gone). **Element-wise merge keyed on element GUID is now observed behaviour of this engine**, not a belief inherited from three in-repo precedents.

**✅ Verified 2026-08-07:** compile-check exit 0 (5937 files); **All 75** (74 + 1); **Fast 47**; conflict script **0 errors / 23 warnings / 12 pre-existing / 3 acknowledged — byte-identical to the Phase 6 baseline**; `git diff --stat Language/` shows only `localization_Overthrow.st`.

**GUIDs minted:** `{6B3A000000000090}` (`FM_Overthrow.conf` resource) · `{6B3A000000000091}` (string item). **Preserved, not re-minted:** `{59908331EDFD9788}` (append-critical), `{59908331F77F1D0F}`, `{59908331D44CD51F}` and all piece GUIDs — moved, not regenerated.

**Link id for `tutorial-content`:** `#OVT-FieldManual_MainMenu_Title` — deliberately **not** renamed (it is already Overthrow-owned, unique, stable, and named as the example in §5; renaming a title key is the one operation D12 forbids). `m_eId` stays `NONE` on purpose — there is no valid `EFieldManualEntryId` for an Overthrow page, and borrowing one would hijack a vanilla hint's deep link.

**New `#OVT-` key to export:** `OVT-FieldManual_Category_GettingStarted_Title` = `Getting Started`.

**Three red-proofs, each restored:** the R2 guard (above); a D12 link guard (`…MainMenu_Titel` typo → *"no entry in the merged manual has that m_sTitle"*); and the 7.3 nit guard (sub-category title reverted to vanilla's Introduction key). **The link check reads whatever `tutorial-content` authors — no hard-coded keys — so future entries are covered automatically.**

**Risk left open:** the two-category-level limit and the empty-node pruning in `SetAllEntriesAndParents` (`:600-663`) are **silent** — a third level or a content-less entry vanishes with no warning. Both are written into the `field-manual` contract section in `context.md`; the Init case catches only the specific case of Overthrow's category losing its sub-category.

---

## Phase 8: Proof entries, localization and final verification (5/5 complete) ✅ — `component-developer`

- [x] ✅ **8.1 — `Configs/Tutorials/proofFirstBuy.conf`**
  - Description: Non-modal, one page, `PLAYER_BUY` trigger, field-manual link, id `economy-first-buy`. Phase 2's placeholder was complete but *implicit*: it relied on attribute defaults for `m_ePresentation`, `m_iPriority` and `m_bEnabled`. All three are now written out, because this file is the template §5 tells `tutorial-content` to copy and a template should not teach by omission.
  - File(s): `Configs/Tutorials/proofFirstBuy.conf`

- [x] ✅ **8.2 — `Configs/Tutorials/proofWelcome.conf`**
  - Description: `welcome-intro` — `MODAL`, **two** pages, `m_iPriority 100`, one `PLAYER_SPAWNED` trigger, and deliberately **no** `m_sFieldManualTitleKey`. Content is sandbox-preserving: page 1 explains that there is no mission list and no required order, page 2 explains the tip system itself and where to read more. Neither page assigns a goal.
  - File(s): `Configs/Tutorials/proofWelcome.conf` (+ `.meta`)

- [x] ✅ **8.3 — Register both on the game-mode prefab**
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et` — `{6B3A0000000000A4}` alongside the existing `{6B3A000000000011}`

- [x] ✅ **8.4 — All strings into `localization_Overthrow.st` only**
  - Description: Five new items. The complete Phase 5–8 export list is below.
  - File(s): `Language/localization_Overthrow.st`

- [x] ✅ **8.5 — Full verification pass per §8**
  - Description: All six automated gates run and recorded; the whole Definition of Done walked criterion by criterion into the verdict table below; the §5 contract corrected against shipped code (I6).
  - File(s): `tasks.md`, `context.md`, `implementation.md` §5

**GUIDs minted** (reserved `6B3A` block, re-verified unused before use): `{6B3A0000000000A0}` `proofWelcome.conf` resource · `{6B3A0000000000A1}`/`{6B3A0000000000A2}` its two pages · `{6B3A0000000000A3}` its trigger · `{6B3A0000000000A4}` its element in `m_aTutorialEntries` · `{6B3A0000000000B0}`…`{6B3A0000000000B4}` five string-table items.

**Why `welcome-intro` carries no field-manual link.** The two proof entries now cover *both* branches of the Learn-more button between them — `economy-first-buy` has a link and must show it, `welcome-intro` has none and must hide it. Before this, verifying the hidden branch meant temporarily blanking a shipped key.

**One documentation defect fixed in code:** `OVT_TutorialEvent`'s doc comment claimed "Nine of these are raised on the SERVER". It is ten (eight per-player + the two global ones). Corrected in `OVT_TutorialTrigger.c` and in the plan's task 2.2.

### 📤 Complete string-table export list — Phases 5, 6, 7 and 8

**Seventeen new `#OVT-` keys**, all in `Language/localization_Overthrow.st` and **nowhere else**. Verified against the file with `git diff -U0 Language/localization_Overthrow.st`, not against the phase reports — the reports were accurate and complete; nothing was missing and nothing extra was found.

| # | Key | English | Phase | GUID |
|---|---|---|---|---|
| 1 | `OVT-FieldManual_Category_GettingStarted_Title` | Getting Started | 7 | `{6B3A000000000091}` |
| 2 | `OVT-Tutorial_MoreInMenu` | Overthrow Menu | 5 | `{6B3A000000000060}` |
| 3 | `OVT-Tutorial_Title` | Tip | 6 | `{6B3A000000000061}` |
| 4 | `OVT-Tutorial_Dismiss` | Close | 6 | `{6B3A000000000062}` |
| 5 | `OVT-Tutorial_Next` | Next | 6 | `{6B3A000000000063}` |
| 6 | `OVT-Tutorial_Finish` | Finish | 6 | `{6B3A000000000064}` |
| 7 | `OVT-Tutorial_Back` | Back | 6 | `{6B3A000000000065}` |
| 8 | `OVT-Tutorial_LearnMore` | Learn More | 6 | `{6B3A000000000066}` |
| 9 | `OVT-Tutorial_DisableTips` | Don't Show Tips Again | 6 | `{6B3A000000000067}` |
| 10 | `OVT-Tutorial_PageIndicator` | `%1 / %2` | 6 | `{6B3A000000000068}` |
| 11 | `OVT-MainMenu_Tips` | Tips | 6 | `{6B3A000000000069}` |
| 12 | `OVT-Tutorial_NoneAvailable` | No tip to show right now | 6 | `{6B3A00000000006A}` |
| 13 | `OVT-Tutorial_EconomyFirstBuy_Title` | Buying and Selling | 8 | `{6B3A0000000000B0}` |
| 14 | `OVT-Tutorial_EconomyFirstBuy_Body` | Shops buy and sell almost anything… | 8 | `{6B3A0000000000B1}` |
| 15 | `OVT-Tutorial_WelcomeIntro_Title` | Welcome to Overthrow | 8 | `{6B3A0000000000B2}` |
| 16 | `OVT-Tutorial_WelcomeIntro_Body1` | Overthrow is an open sandbox… | 8 | `{6B3A0000000000B3}` |
| 17 | `OVT-Tutorial_WelcomeIntro_Body2` | Occasional tips like this one… | 8 | `{6B3A0000000000B4}` |

`#10` is a format string — `SetTextFormat("#OVT-Tutorial_PageIndicator", page, count)` — so `%1` and `%2` must survive translation. `#16` and `#17` contain `<br/>` rich-text markup, which must also survive.

**Until the user exports these in Workbench, every one of them renders as its raw key on screen.** That is the single prerequisite for all of the play-testing below.

---

## Play-test round 1 — 2026-08-08 · two defects found by the user, both fixed

The first human play-test found **two real defects, neither of which any automated gate could have caught.** Both are worth reading as a pair, because they are the same lesson twice.

### 1. 🐛 The first-buy tip described a game mechanic that does not exist — **FIXED**

Shipped text: *"Shops buy and sell almost anything, and their prices move with the town they are in. **Money you carry can be stolen; money you deposit at your house cannot.**"*

The second sentence was **entirely invented by the Phase 8 agent.** Verified against the code: `OVT_PlayerData.money` (`Scripts/Game/Data/OVT_PlayerData.c:13`) is a persisted `int` on the player record — **not an entity, not carried, not lootable**. There is no theft, drop, pickpocket or loot-money mechanic anywhere (searched `steal|stolen|rob|pickpocket|dropMoney|loseMoney`), and **no deposit, bank or vault exists**. `PURPOSE_DEPOSIT` is an unrelated engine *item*-storage bitmask (`OVT_SellableItemScanner.c:66`). The only real money loss is `OVT_EconomyManagerComponent.ChargeRespawn` (`:1834`) — a flat fee on respawn, only above a 500 balance, and **0 on Easy and Normal**.

**A second instance of the same fault, found by fact-checking the rest rather than waiting for a report:** the `welcome-intro` page 2 claimed *"The Overthrow menu holds your map, your money, your recruits and the Field Manual."* The menu has 12 entries and **neither a money screen nor the Field Manual** is among them (the manual is a base-game menu). Also corrected.

Both replacement strings now carry their evidence in the string-table `Comment` field. **Rule 0 was added to `implementation.md` §5** — the sibling feature `tutorial-content` is about to author ~12 more entries and would have repeated this.

> **Why no gate caught it, and why none could:** compile-check, all 78 assertions, the duplicate-id guard, the field-manual link guard and the localization diff all pass happily on a well-formed lie. The Init cases verify a link *resolves*; nothing verifies a sentence is *true*. A tutorial tip is a factual claim about the game, and a wrong one is worse than no tip — it teaches a mechanic the player then wastes time hunting for.

### 2. 🐛 `welcome-intro` never appeared at all — **FIXED**, and it was worse than reported

`OnPlayerSpawnedLocal` opened with `if (!m_bGameStarted) return;`, but `OVT_SpawnLogic.DoSpawn_S:160` possesses the player **unconditionally, before the start menu is answered** (possession is the only thing that dismisses the engine loading screen — see its comment at `:142-149`). From the user's log:

```
17:00:44.673  Game not started yet, deferring full player preparation   <- possession
17:00:49.727  StartGame button clicked                                  <- m_bGameStarted = true, +5.05 s
```

Nothing possesses again — `TeleportPlayerToHome` only moves the body, and the second `SpawnDeferredPlayer` returns early at `:178-179`. **So `OnPlayerSpawnedLocal` ran exactly once per session, always ~5 s too early, and the trigger was silently dropped.** Task 4.6's retry did not help: it retried the *controller lookup*, not the started-state, and the guard returned before ever reaching it. Not test-world-specific — it reproduced on every new campaign.

**The follow-on half was worse.** `m_bGameStarted` is written in exactly one place (`OVT_OverthrowGameMode.c:254`, inside `DoStartGame`) and every route there is authority-only, so **it is never true on a dedicated-server client** — `welcome-intro` could never have fired for any remote player. DoD step 16 would have failed the MP protocol.

**The fix, both halves:**
- The push became **owed state rather than an event**: `m_bTutorialSpawnPending` / `m_bTutorialSpawnDelivered`, funnelled through one decision point `TryPushSpawnedTutorialTrigger()`, which `DoStartGame()` calls at its end. Idempotent — whichever caller arrives first wins, so the queue and logs see exactly one push per spawn.
- For remote clients, a new **`RplProp` mirror** `m_bCampaignRunningRpl` on the game mode (`onRplName: "OnCampaignRunningReplicated"`), set beside `m_bGameStarted` + `Replication.BumpMe()`. Chosen over an RPC because it **JIP-syncs itself** — which is the only way a player joining an already-running dedicated server can learn the campaign is up, since no RPC fires for them. Same pattern as vanilla `SCR_GameModeCampaign.c:149/600`, and the prefab already carries the `RplComponent`.
- Deliberately a **mirror, not a promotion of `m_bGameStarted` to `RplProp`** — making `HasGameStarted()` true on clients would silently widen the persistence save gates, the `DoSpawn_S` preparation branch, and the legacy `#OVT-IntroHint` (which **I7 forbids touching**). Only the tutorial path reads the new form.

**I7 re-verified:** `#OVT-IntroHint` and `m_aHintedPlayers` are behaviourally identical — the old early return was folded into the hint's own condition so the code below it can run, and the hint still gates on the authority-only `m_bGameStarted`.

**Regression test:** `OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart` (Campaign tier). Campaign and not Init because **the autotest world reproduces the failing order for free** — the harness possesses its local player during world load and `Setup_StartCampaign` runs afterwards, the same sequence as the user's log. **Proven red** by removing the single `TryPushSpawnedTutorialTrigger()` call from `DoStartGame()`: exit 1, failing on the poll backstop with *"the PLAYER_SPAWNED tutorial trigger was still never delivered"* — the pre-fix behaviour exactly.

**Incidental finding, not fixed (belongs to `first-spawn`):** the legacy `#OVT-IntroHint` is dead on the new-game path for this identical reason, and has been since the 1.6 spawn rework (`bb04c331`). The tutorial simply inherited the same dead hook.

**Gates after both fixes:** compile-check **0** (5940 files) · Fast **47** · All **77 → 78** · Q9 `OVT_PlayerCommsComponent` **132**, unmodified.

---

## Merge note — `main` merged in 2026-08-08

`main` was merged back into this branch (merge commit `7efb1c44`, 11 commits). **All five bugs this feature filed were fixed upstream and are closed: BUG-090…094.** Two of those fixes land directly on this feature's own gates:

**1. Q5 is now genuinely clean, and the caveat on it is gone.** Main rewrote `check-input-conflicts.py` (+231 lines) to parse the inline-declared `ActionContext { Actions { … } }` form this feature discovered it was blind to (BUG-092), and rebound `OverthrowMainMenu` off bare `gamepad0:pad_down` onto an `LT + pad_down` chord (BUG-093). Re-measured on the merged tree:

| | Before merge | After merge |
|---|---|---|
| errors | 0 | **0** |
| warnings | 23 | **0** |
| pre-existing (`BASE`) | 12 | **0** |
| combo notes | — | 3 (all base-game-shaped, incl. the new `LT+pad_down`) |
| acknowledged | 3 | 1 |

The structural "+1 warning" Phase 6 had to accept — *"a 17th menu context that lists `MenuDown` cannot land at 22"* — **no longer exists**, because the binding that caused it is gone. Q5 reads clean on its own terms now, and against a checker that can actually see what it is checking.

**Our four tutorial bindings survived the rebinding sweep unchanged** (`KC_Q`/`X`, `KC_E`/`Y`, `KC_F`/`RB`, `KC_N`/`R3`) and produce **zero** findings under the new checker. That was verified positively, not by absence: injecting a deliberate collision (`OverthrowTutorialNext` moved onto `gamepad0:x`, colliding with `OverthrowTutorialBack` in the same context) made the checker emit `ERROR gamepad0:x  OverthrowTutorialBack, OverthrowTutorialNext` and exit 1. It sees our context. Reverted immediately.

**2. BUG-094's fix is in, and it was in a file this feature edited.** `OVT_CharacterSheetContext`'s `m_OnPlayerSkill.Insert` moved from `PostInit` into `OnShow`, now symmetric with `OnClose`'s `Remove`. The Phase 0/3 wrapper (`OnSkillChanged(int playerId, string skillKey)`) is untouched and still correct.

**Merge conflict, and how it was resolved.** One file conflicted: `Configs/System/chimeraInputCommon.conf`. Both sides had *added* new actions at the same offset — ours (`OverthrowTutorialBack/Next/LearnMore/Disable`) and main's (`OverthrowLoadoutsApplyToRecruit/ApplyToAll`). Nothing was in competition, so **all six were kept**; both `ActionRefs` lists survived intact. Everything else auto-merged, including `OVT_SkillManagerComponent.c` — both `m_OnPlayerSkill.Invoke` sites still pass `(playerId, key)`, so main's XP-cheese fix did not reintroduce an arity mismatch.

**Gates re-run on the merged tree:** compile-check **0** (5939 files) · Fast **47** · All **75 → 77** (main added 2 Campaign cases) · conflicts **0/0/0** · `m_OnPlayerSkill` arity consistent at both invoke sites.

### ⚠️ One thing this reopens: F5

F5 was deferred because risk R3 fired twice — there was no free gamepad input during gameplay. **That survey is now out of date.** Main moved several bindings (`OverthrowMainMenu` to a chord, loadout actions onto `shoulder_right`/`right_trigger`, others off `shoulder_left`/`thumb_*`), and the checker that can now see inline actions is a far better instrument than the one both surveys used. Whether a genuinely free gameplay input exists today is **an open question again, and a cheap one to answer**. It was not re-attempted as part of this merge — that is a scope decision, not an oversight.

---

## Definition of Done — Verdict

Walked criterion by criterion on 2026-08-07 at the end of Phase 8. Three verdicts only:

- **✅ MET** — with evidence a third party can re-check. Never claimed for anything only a running screen can confirm.
- **👤 HUMAN** — built and believed correct; a play-test is the only thing that can confirm it. The exact action is named.
- **⚠️ DEFERRED** — not delivered, with the reason.

**Score: 8 ✅ MET · 20 👤 HUMAN · 1 ⚠️ DEFERRED.** That ratio is expected and was predicted by the plan: this is a UI feature in a project where UI is play-test-only by rule, and the harness structurally cannot reach rendering, input or multiplayer.

### Automated gate results (2026-08-07, final)

| Gate | Result | Exit |
|---|---|---|
| `tools/compile-check.sh` | OK, 5937 files, Game module | **0** |
| `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) | **47 tests** | **0** |
| `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) | **75 tests** | **0** |
| `check-input-conflicts.py --warnings` | **0 errors, 23 warnings, 12 pre-existing, 3 acknowledged** | **0** |
| `git diff --stat Language/` | `localization_Overthrow.st` only, 289 insertions, 0 deletions | — |
| `grep -c "RpcAsk_\|RpcDo_" OVT_PlayerCommsComponent.c` | **132** (unchanged; file not in `git status`) | — |

Every number matches the Phase 7 baseline except the two test counts, which are unchanged from Phase 7 (Phase 8 adds no cases — it adds data, and the existing Init cases assert that data).

### Functional criteria

| # | Criterion | Verdict | Evidence / what a human must do |
|---|---|---|---|
| **F1** | First buy shows `economy-first-buy` once, ever, across restarts and campaigns | 👤 **HUMAN** | Three layers are machine-asserted — `OVT_TEST_Logic_Tutorial_SeenStore` (set logic, proven red), `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` (the real `BaseContainer`, proven red), and the server's per-persistent-id sent-set. **Do:** clear the `OVT_TutorialSettings` block, start a campaign, buy once (popup), dismiss, buy ten more times (nothing), quit, relaunch, **new** campaign, buy (still nothing). |
| **F2** | Popup draws title/body/image from `#OVT-` keys; no raw English from new code | 👤 **HUMAN** | Both `.conf`s reference only `#OVT-` keys and all 5 exist in the `.st` (table above). The only literals in either layout are design-time placeholders (`Text "Tutorial"`, `Text "1 / 1"`) that `ApplyTitle`/`ApplyPageIndicator` overwrite or hide before the widget is shown. **Do:** export the string table first, then confirm no `#OVT-` key is visible on either surface. |
| **F3** | Non-modal never captures movement, aim or fire | 👤 **HUMAN** | Structurally true: `OVT_TutorialInfo` contains **no** `ActivateContext`, no `AddActionListener` and no `InputManager` reference at all (grep), and R3's fallback means it added no gameplay binding. **Do:** with a popup up — walk, sprint, aim, fire, crouch, prone, lean, reload, inventory. Then repeat on a gamepad. |
| **F4** | Auto-dismiss ~20 s; instant hide on map/menu; marked seen either way | 👤 **HUMAN** | `AUTO_DISMISS_MS = 20000` on a 100 ms tick that also polls the three blocking-UI facts; both routes end in `NotifyDismissed(entryId)`. **Do:** let one time out, then buy again (must not return); reset the store and open the map mid-popup (must vanish and not return). |
| **F5** | **A documented key escalates non-modal → modal** | ⚠️ **DEFERRED** | **Risk R3 fired in Phase 5.4 and was CONFIRMED on an inline-aware re-survey in Phase 6.8.** There is no free gameplay-context input: all sixteen gamepad inputs are bound in a context that can be live under a HUD popup, `gamepad0:shoulder_left` and `keyboard:KC_T` belong to `VONContext` at **Priority 110** (above every Overthrow menu at 50), `KC_K`/`KC_O` are `GadgetCompass`/`GadgetWatch`, and `InputSourceCombo` layers over its parts rather than suppressing them so a chord inherits every collision. R3's own documented fallback was taken: **`Configs/System/chimeraInputCommon.conf` gained no gameplay action and no `OverthrowTutorialContext`.** The escalation route is instead the HUD prompt (bound to the *existing* `OverthrowMainMenu` action) → Overthrow main menu → **Tips**, which opens the modal on the same entry without marking it seen. The capability survives; the one-keypress convenience does not. Reopening this needs a scheme-level decision by the mod owner, not a code change. |
| **F6** | Two-page `welcome-intro` pages both ways, shows an indicator, cannot overrun, last-page Next dismisses | 👤 **HUMAN** | The entry now ships: `proofWelcome.conf`, `MODAL`, two pages, `PLAYER_SPAWNED`, priority 100 — and `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` asserts it loads with ≥1 page and ≥1 trigger and a unique id. Paging is widget behaviour. **Do:** spawn on a fresh profile; header reads `1 / 2`; Next → page 2 and relabels **Finish**; Back → page 1 and greys out; Back again does nothing and focus jumps to Next; Finish closes. |
| **F7** | "Don't show tips again" suppresses everything, survives a restart, is the only control that does | 👤 **HUMAN** | `m_bTipsDisabled` is written through the accessor and flushed; the Phase 4 gate additionally asserts that writing the flag *back to its default* clears it, so the toggle is provably two-way. **Do:** press it, trigger another entry (nothing), restart, trigger again (nothing), then clear the flag in the profile and confirm tips return. |
| **F8** | No popup over any Overthrow context, the map or a base-game menu; it appears **after**, within ~1 s | 👤 **HUMAN** | `OVT_TEST_Logic_Tutorial_GatePredicate` asserts all four gate terms in isolation (proven red by neutering the `blockingUiOpen` term); the 1000 ms pump is what makes "after, not never". Phase 6.7 additionally closed the place/build hole via `IsBlockingPopups()`. **Do:** buy at a shop — the popup must appear only once the shop menu closes; and arm a tip then enter placement — it must wait for you to cancel. |
| **F9** | Two entries shown one at a time in priority order | 👤 **HUMAN** | `OVT_TEST_Logic_Tutorial_QueueOrdering` asserts priority-then-FIFO and was red for a **genuine** defect (`array.Remove` is swap-with-last). `welcome-intro` at priority 100 vs `economy-first-buy` at 0 is a live ordering pair. **Do:** trigger both close together; the welcome must show first and the buy tip only after it is dismissed. |
| **F10** | Adding an entry needs only a `.conf`, keys and one prefab line — no script change | ✅ **MET** | **Demonstrated this phase, not argued.** `welcome-intro` was added with exactly three edits — `Configs/Tutorials/proofWelcome.conf` (+`.meta`), five items in the `.st`, and one element on the game-mode prefab — and **zero** lines of EnforceScript. compile-check stayed 0 and All stayed 75/exit 0. The procedure is written up in §5 "Adding an entry". |
| **F11** | `m_bEnabled 0` never fires; a duplicate id logs a named error at campaign start | 👤 **HUMAN** | *First half is machine-proven:* `OVT_TutorialMatcher` skips disabled entries and `OVT_TEST_Logic_Tutorial_MatcherSelectsAndOrders` was proven red by neutering exactly that check. *Second half is code-read only:* `OVT_TutorialManagerComponent.c:156` logs `Duplicate tutorial entry id '<id>' at index <n>` at ERROR and validation is **terminal** (no invoker is subscribed that session). **Do:** temporarily point a second `m_aTutorialEntries` element at `proofFirstBuy.conf`, start a campaign, confirm the named ERROR and that tips go dark rather than the mod breaking. Revert. |

### Quality criteria

| # | Criterion | Verdict | Evidence / what a human must do |
|---|---|---|---|
| **Q1** | Dedup cannot double-fire | 👤 **HUMAN** | Three independent layers, each asserted: the queue rejects duplicates (Logic), the seen store is idempotent (Logic), the server sent-set is keyed on persistent id (code). **Do:** trigger the same action 10× in one session and again after a restart. |
| **Q2** | The settings store never corrupts | ✅ **MET** | `OVT_TutorialSettings` holds **one** `ref array<ref OVT_SeenTutorialEntry>` — no parallel arrays anywhere, which is the specific failure the base game's `SCR_HintSettings` had to add self-nuking recovery for. `WriteToInstance` returning null is guarded at every call site (`if (!settings.m_aSeen)` is mandatory, not defensive — gotcha 17 proves the engine really does hand back null). A version mismatch clears `m_aSeen` and **immediately rewrites at the current version** so it is not re-detected every session. A failed load degrades to "show the tip again", never to a crash. *Named limit:* arbitrary hand-corruption beyond a version mismatch is not simulated by any test. |
| **Q3** | The store round-trips across a full application restart | 👤 **HUMAN** | The container round trip and the on-disk block are **already proven** — `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` (proven red by removing `[Attribute()]`, which is R1's real failure mode) plus the verbatim `OVT_TutorialSettings` block quoted in Phase 4 above. What is unproven is *reading it back after a relaunch*. **Do:** dismiss a tip, quit cleanly, relaunch, trigger again (nothing). Then repeat killing the process >10 s after the dismissal. Then repeat dismissing two tips seconds apart and killing — the second id is the known residual exposure of the `SaveUserSettings` throttle (gotcha 16). |
| **Q4** | No dead buttons — every modal button works on mouse **and** gamepad | 👤 **HUMAN** | Five `WLib_NavigationButton`s, each carrying the inherited `SCR_InputButtonComponent`, wired to `m_OnActivated`. **Do:** with the mouse unplugged, drive the whole modal on a pad — stick/d-pad focus, `A` activate, `B` close — and confirm the glyph row reads `B`/`Y`/`X`/`R3`/`RB`, not keyboard letters. |
| **Q5** | No input regressions | 👤 **HUMAN** | The script is at baseline-plus-one: **0 errors, 23 warnings, 12 pre-existing, 3 acknowledged** (Phase 6 baseline was 22 warnings; the one new line is structural — *any* 17th menu context listing `MenuDown` collides with `OverthrowMainMenu` on `gamepad0:pad_down`, which `OverthrowGeneralContext` activates every frame). ⚠️ **The script has a known blind spot** — it cannot see the 197 actions declared inline inside `ActionContext` blocks (filed as tech debt / BUG-092), so "no new conflicts" is weaker than it reads. **Do:** the R7 sweep — open, navigate and close **all 17** Overthrow menus on keyboard and pad. Ten debug `Print()`s were removed from the `ShowLayout()` path every one of them runs through. |
| **Q6** | No manager regressions from Phase 0 | 👤 **HUMAN** | `OVT_TEST_Init_Tutorial_InvokerSeamsExist` asserts all ten invokers still exist and is proven red by un-allocating one. Behaviour is not asserted. **Do:** buy a skill (sheet refreshes), buy and sell (XP + stability/support modifiers move), commit a crime from wanted 0 (**exactly one** notification), escalate again (no extra), let it decay (no side effects). |
| **Q7** | No new nulls | ✅ **MET** | Audited across the whole feature. Every `FindAnyWidget` result is guarded before use (`OVT_TutorialInfo`'s six via the `Apply*` methods' `if (!m_w…) return;`, `OVT_TutorialContext`'s nine at the point of use). `GetGameUserSettings()` **and** `GetModule()` are both guarded in `OVT_TutorialSettingsAccessor.GetModuleContainer()`. `Deliver()` guards the player manager, the controller **and** the component (`:511-521`). `OVT_FieldManualHelper` / `OVT_OpenByTitle` null-guard the `OpenMenu` result, which **vanilla does not**. The component resolves the UI manager itself rather than calling `OVT_Global.GetUI()`, which dereferences the controlled entity unguarded (gotcha 15). The unassigned-controller window drops the trigger silently by design. |
| **Q8** | Style | ✅ **MET** | **Zero ternaries** across all eleven new/edited feature scripts (grep). `ref` on every Managed in a container (`ref array<ref OVT_TutorialPage>`, `ref array<ref OVT_TutorialTrigger>`, `ref array<ref OVT_SeenTutorialEntry>`, `map<string, ref set<string>>`). **No `EntityID` crosses the network at all** — the wire payload is a single string entry id (D4). `OVT_` prefix and `m_i`/`m_f`/`m_s`/`m_b`/`m_a`/`m_m`/`m_e`/`m_w` throughout. Doxygen `//!` on every public member and every public method. |
| **Q9** | Nothing added to `OVT_PlayerCommsComponent` | ✅ **MET** | `grep -c "RpcAsk_\|RpcDo_"` = **132**, identical to the pre-feature count, and the file does not appear in `git status`. Delivery lives on `OVT_TutorialComponent` on `OVT_OverthrowController`, per project rule and D1. |
| **Q10** | Compile and tests clean; every new case proven able to fail | ✅ **MET** | compile-check **0** (5937 files); Fast **47**, exit **0**; All **75**, exit **0**. Baseline was Fast 38 / All 66, so the feature added **nine** cases (5 Logic + 4 Init) — more than the plan's "5 + 2", because the settings-store gate and the field-manual merge guard were both converted from planned play-tests into permanent coverage. **All nine have a recorded red-proof** in `context.md`, and **three of them were not synthetic**: `_QueueOrdering` caught a real `array.Remove` ordering defect, the R1 proof removed `[Attribute()]` (R1's actual failure mode), and the R2 proof repointed a GUID and observed vanilla's Introduction category be **replaced**. No `maxAttempts` anywhere. |
| **Q11** | Localization hygiene | ✅ **MET** | `git diff --stat Language/` → `Language/localization_Overthrow.st \| 289 +++…`, **one file, 289 insertions, 0 deletions**. No `localization_Overthrow.<lang>.conf` is touched by this feature in any phase. |

### Integration criteria

| # | Criterion | Verdict | Evidence / what a human must do |
|---|---|---|---|
| **I1** | Field manual intact and extended; no second "Introduction" | 👤 **HUMAN** | The **config half is machine-asserted and does not need re-checking by eye**: `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` loads `{17295EF80DC38D53}` through the menu's own `SCR_FieldManualConfigLoader.LoadConfigRoot` and measures **6 categories, 8 tile backgrounds, 141 entries**, plus the sub-category title. **Do:** open the manual from the **main menu** and from the **pause menu** (different routes — one strips the blur, one the background) and confirm the six headings *draw*, the tile art renders, and exactly one button is named Introduction. |
| **I2** | Deep link opens the manual **on the Overthrow page** | 👤 **HUMAN** | The link's *resolution* is asserted — the Init case reads whatever `tutorial-content` authored and fails on a key matching no entry (proven red with a one-letter typo). What only a running menu can show is the **ordering** property: `OnMenuShow` calls `SetCurrentEntry(null)`, so the code navigates after `OpenMenu` returns. **Do:** arm `economy-first-buy`, escalate, press Learn more — reading view, not the grid, not the front page, and the popup must be gone. Then temporarily set the key to `#OVT-FieldManual_NoSuchPage`: front page + one `[Overthrow.FieldManual]` WARNING + no error. |
| **I3** | MP delivery is per-player | 👤 **HUMAN** | Correct **by construction**, which is the point of D1/R5: `[RplRpc(RplChannel.Reliable, RplRcver.Owner)]` on a per-player controller entity *cannot* reach another client, unlike the broadcast-and-filter notification path this feature exists to avoid. Plus the host direct-call branch in `Notify()` (an RPC is never looped back to its sender). **Do:** the three-terminal protocol in §8.4 — server + two `--profile` clients; A buys, only A sees it, and B's log must not contain the entry id. |
| **I4** | Per-machine, not per-campaign | 👤 **HUMAN** | The store is a `ModuleGameSettings` in `$profile:.save/settings/`, which is per **profile** — that is exactly what makes the two-`--profile` test meaningful. **Do:** dismiss on profile 1; profile 2 must still see it; profile 1 must never see it again on any server or campaign. |
| **I5** | JIP safe | 👤 **HUMAN** | Phase 4 resolved the interaction that made this untestable earlier (an in-memory-only store plus the server's persistent-id sent-set made a reconnect show *nothing*). **Do:** disconnect B and reconnect into the running campaign — already-seen entries stay silent, an untriggered one still fires. |
| **I6** | The contract is published and accurate against shipped code | ✅ **MET** | §5 was corrected this phase with all four accumulated deltas: `PLAYER_SELL`'s second invoke site (`OVT_EconomyManagerComponent.c:1004`, `AddPlayerMoney` with `doEvent`) and the resulting "prefer `PLAYER_TRANSACTION`" advice; `PLAYER_SKILL` now genuinely carrying the skill key in `m_sFilter` after task 3.0; **ten** invokers not nine (also fixed in `OVT_TutorialTrigger.c`'s enum doc and in task 2.2); and `m_OnTownControlChange`'s decorative `ScriptInvoker<IEntity>` declaration versus its real `OVT_TownData` payload. §5 also gained the presentation/priority rules, the chrome-key list and a rewritten add-an-entry procedure naming both proof entries. The procedure was **executed** this phase to add `welcome-intro`, which is the strongest available check that it is followable. `field-manual`'s fuller contract is the dedicated section in `context.md`. |
| **I7** | Nothing removed yet | ✅ **MET** | `m_aHintedPlayers` is still declared (`OVT_OverthrowGameMode.c:57`), allocated (`:1080`) and used (`:1300-1303`), and `#OVT-IntroHint` still shows through `SCR_HintManagerComponent.ShowCustom` exactly as before — with a comment at `:1307` recording that retiring it belongs to `first-spawn`. `Configs/Jobs` and the job scripts are untouched (`git status` clean). |

---

## Bugs & Issues

**Active Bugs:**
- (none yet)

**Fixed Bugs:**
- (none yet)

---

## Technical Debt

> **All five bugs this feature filed (BUG-090…094) were fixed on `main` and merged into this branch on 2026-08-08.** Every entry below is closed. The two that mattered most to *this* feature's own gates are called out in the merge note at the top of the Definition-of-Done verdict.

- [x] ✅ 💳 **`check-input-conflicts.py` cannot see inline-declared actions** — **RESOLVED upstream, merged 2026-08-08** *(found in Phase 6, filed as BUG-092, closed)*
  - Description: The script reads the top-level `Actions` block plus `ActionRefs` lists, but the base conf declares **197 actions inline inside `ActionContext` blocks**, which appear in no `ActionRefs` list. Its output therefore **understates real collisions**, and any "no new conflicts" gate built on it — including Q5 — is weaker than it reads. This is how `KC_T` and `shoulder_left` were both believed free when both are VON bindings at priority 110.
  - Reason: pre-existing limitation of the tooling, exposed by this feature's two input surveys.
  - Effort: medium. **The highest-leverage fix of the three input findings** — it makes every future input gate trustworthy. Captured inside BUG-092.

- [x] ✅ 💳 **`OVT_ProgressInfo` can be dead for a whole session** — **RESOLVED upstream, merged 2026-08-08** · BUG-091 closed *(found in Phase 5, outside this feature's scope)*
  - Description: `SubscribeToController` attempts its lookup **once** and gives up if the controller is not yet assigned; assignment is async, so transfer-progress UI can be absent for an entire session. This feature hit the same race **three times** (`PLAYER_SPAWNED` push, the HUD subscription, and this) — it is a systemic pattern, and a shared bounded-retry helper is probably the right fix.
  - Reason: pre-existing; not introduced here.
  - Effort: small; wants a play-test to confirm the symptom first.

- [x] ✅ 💳 **Two other controller components may be broken on a listen-server host** — **RESOLVED upstream, merged 2026-08-08** · BUG-090 closed *(found in Phase 3, outside this feature's scope)*
  - Description: `OVT_BaseServerProgressComponent` and `OVT_ShopTransactionComponent.RpcDo_SellResult` send owner-targeted RPCs **without** the host direct-call branch this feature had to add. Since the engine never loops an RPC back to the sender, the listen-server host plausibly never receives its own sell toasts or transfer progress.
  - Reason: pre-existing; not introduced here. Not fixed because it is outside the tutorial-system scope and needs its own play-test to confirm.
  - Effort: small fix, but wants a host-vs-client play-test to confirm the symptom first. **Filed as BUG-090** (medium, linked to this feature) — the fix shape is already proven in `OVT_TutorialComponent.Notify()`. Symptom is a code-reading prediction, not yet observed; BUG-090 carries the verification steps.

---

## Needs Human Verification

Runtime/visual items the harness structurally cannot reach. Populated as phases complete.

### From Phase 0 — the four managers must behave identically (Q6)
- [ ] Commit a crime from wanted 0 in view of an OPFOR patrol — the existing "you are wanted" notification appears **once**, not twice.
- [ ] Escalate again while already wanted (2→3) — no extra notification (the invoker fires; the notification correctly does not).
- [ ] Let wanted decay to 0 — no event-driven side effects during decay (the decay path must not fire the new invoker).
- [ ] Open the character sheet, buy a skill point — the sheet redraws immediately; close and reopen it and confirm no duplicate-refresh or stale listener.
- [ ] Buy and sell at a shop — XP still awarded on both, stability/support modifiers still move.

### From Phase 5 — the HUD overlay (nothing here is reachable by the harness)
- [ ] **Export the string table first.** Workbench → string-table export, or the popup's button reads `#OVT-Tutorial_MoreInMenu` instead of "Overthrow Menu". New key list is in the session note.
- [ ] Trigger `economy-first-buy` (buy anything at a shop on a fresh seen store). The popup appears on the **left edge, vertically centred**, after the shop menu closes.
- [ ] **1080p and 4K**: title and body legible, body wraps inside the 460 px panel, nothing clipped, the 4 px countdown bar drains left to right over 20 s.
- [ ] **No overlap** with the wanted stars / undercover icons (top right), the money panel (top right), the QRF and notification panels (top centre) or the transfer-progress panel (top centre, 180 px down).
- [ ] **F3 — genuinely non-modal.** While it is up: walk, sprint, aim, fire, crouch, prone, lean, reload, cycle fire mode, open and close the inventory, cycle weapons with Back+d-pad. Every one behaves exactly as it does with no popup. Repeat **on a gamepad**.
- [ ] **The prompt renders a glyph on both devices** — `U` on keyboard, the d-pad-down glyph on a pad — and pressing it opens the Overthrow main menu *and* retires the popup.
- [ ] Let one time out untouched (~20 s). It disappears; buying again does not bring it back.
- [ ] Reset the seen store, trigger again, and open the **map** while it is up → it disappears immediately and never returns. Repeat with the **Overthrow main menu**, a **shop**, and the **Escape pause menu**.
- [ ] Trigger it, then **die** while it is up. No script error, and after respawn the same tip is still available (this path deliberately does *not* mark it seen).
- [ ] With the popup up, enter **placement mode** from the Place menu — confirm the popup is already gone by the time the ghost appears, and that LB/RB still rotate normally.
- [ ] Confirm the popup renders at all on a **first spawn** (the display subscribes with a retry because the controller assignment is async — a popup that never appears in the first minute of a session is that retry failing).

### From Phase 5 — the overlay (nothing here is machine-checkable)
- [ ] **Export the string table in Workbench first**, or the prompt button renders the raw key.
- [ ] Buy at a shop on a fresh seen store — popup appears left edge, vertically centred, **after** the shop menu closes.
- [ ] **1080p and 4K**: title/body legible, body wraps inside the 460 px panel, nothing clipped, the 4 px orange bar drains over 20 s.
- [ ] **No overlap** with wanted stars / undercover (top right), money (top right), QRF + notifications (top centre), transfer progress (top centre +180 px).
- [ ] **F3 on a gamepad with no mouse**: walk, sprint, aim, fire, crouch, prone, lean, reload, cycle fire mode, inventory, Back+d-pad weapon switch — all unchanged with the popup up.
- [ ] Prompt shows `U` on keyboard and the **d-pad-down glyph** on a pad; pressing it opens the Overthrow menu *and* retires the popup.
- [ ] Let one time out untouched; buy again — it must not return.
- [ ] Reset the store, trigger, open the **map** → gone instantly, never returns. Repeat with main menu, a shop, and the Escape pause menu.
- [ ] Trigger it then **die** — no script error; after respawn the same tip is still available (that path intentionally does not mark seen).
- [ ] First spawn of a session: confirm a popup appears at all — the display subscribes with a 60 × 1 s retry against async controller assignment, and a permanently blank popup is that retry failing.
- [ ] **R7 sweep:** open, navigate and close all 16 existing Overthrow menus on keyboard **and** pad.

### From Phase 6 — the modal popup (nothing here is machine-checkable)

- [ ] **Export the string table in Workbench first**, or every button on the modal renders a raw `#OVT-` key. Ten new ids, listed in the session note.
- [ ] **Reach it.** Trigger any MODAL or multi-page tip (Phase 8 ships `welcome-intro`; until then, flip `m_ePresentation` to MODAL on `Configs/Tutorials/proofFirstBuy.conf` or add a second page). The popup should open centred, over a blurred backdrop, with the HUD hidden.
- [ ] **Mouse:** click Close, Next, Back, Don't Show Tips Again. Each does what its label says.
- [ ] **Keyboard:** `Esc` closes · `E` next / finish · `Q` back · `N` don't-show · `F` learn-more (only on a linked entry). Each button's chrome shows the right key.
- [ ] **Gamepad, with the mouse unplugged — this is the acceptance test.** Left stick and d-pad move focus between the footer buttons; `A` activates the focused one; `B` closes. The glyph row reads `B` / `Y` / `X` / `R3` / `RB`, not keyboard letters.
- [ ] **Sequence:** on a two-page entry the header shows `1 / 2`; `Next` goes to page 2 and relabels to **Finish**; `Back` returns to page 1 and greys itself out; pressing Back again on page 1 does nothing and does **not** strand gamepad focus (focus should jump to Next). Finish on the last page closes.
- [ ] **Single-page entry:** Next and Back are absent entirely, and their shortcuts (`E`/`Q`, `Y`/`X`) do nothing.
- [ ] **Learn more is absent** on an entry with no `m_sFieldManualTitleKey`. On one that has a key (`economy-first-buy` now has one), pressing it opens the Field Manual on the Overthrow page **and closes the popup** — Phase 7 replaced the placeholder warning.
- [ ] **Seen:** close the modal, re-trigger the same action — it must not come back. Restart the game and re-trigger — still nothing.
- [ ] **Don't Show Tips Again:** press it, then trigger a different entry — nothing appears. Restart the game, trigger again — still nothing. Then clear `m_bTipsDisabled` in the profile and confirm tips return (the toggle is two-way).
- [ ] **Die with the modal open** (have someone shoot you, or `#kill`). No script error, and after respawn the same tip is still available — a death-close deliberately does not mark it seen.

### From Phase 6 — 6.7, the place/build gate (this is the fix, verify it)

- [ ] Reset the seen store. Buy something at a shop to arm the `economy-first-buy` tip, then **immediately enter placement mode** from the Place menu. The popup must **not** appear over the ghost; it should appear only after you cancel placement (`Esc` / `B`).
- [ ] With a popup already on screen, enter placement mode — it must disappear at once, and `LB`/`RB` must still rotate the ghost normally.
- [ ] Repeat both with **Build** mode and with **removal mode** (both Place → Remove and Build → Remove).
- [ ] Regression: placing and building themselves must behave exactly as before — rotate, next/prev item, confirm, cancel.

### From Phase 6 — the main-menu Tips route

- [ ] Open the Overthrow main menu with no tip ever shown: **Tips is greyed out** (like Options).
- [ ] Trigger a HUD tip, press its prompt (`U` / d-pad-down). The main menu opens and the popup goes away. **Tips is now enabled**; pick it and the same tip opens in the modal, unmarked-as-seen until you close the modal.
- [ ] Pick Tips while in placement mode (open the main menu with `U` while a ghost is up): the modal must refuse and show the "No tip to show right now" hint rather than opening on top of the ghost.
- [ ] Navigate to Tips **on a gamepad** — it is a normal entry in the same vertical list, between Character Sheet and Save.

### From Phase 6 — 6.6, the R7 sweep (mandatory, the shared `ShowLayout()` changed)

- [ ] Open, navigate and close **all 17** Overthrow menus on keyboard **and** on a gamepad: main menu, place, map info, shop, resistance, jobs, build, base, manage vehicle, real estate, vehicle menu, warehouse, FOB, camp, port, character sheet, recruits, loadouts, and the new tutorial popup. Ten debug `Print()` calls were removed from the code path every one of them opens through; one of them was calling `CanShowLayout()` a second time.

### From Phase 7 — the Field Manual (the config is asserted; the SCREEN is not)

*What the harness already proves, so do not re-check it by eye: the merged root has 6 categories, 8 tile backgrounds and 141 entries; the Overthrow sub-category is titled `#OVT-FieldManual_Category_GettingStarted_Title`; and `economy-first-buy`'s deep link resolves to a real page. `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` fails the build if any of that stops being true. Everything below is what only a running screen can answer.*

- [ ] **Export the string table in Workbench first**, or the new sub-category button renders the raw key `#OVT-FieldManual_Category_GettingStarted_Title`. One new id.
- [ ] **I1, from the MAIN MENU.** Main menu → Field Manual. The left-hand list shows **six** headings: Introduction, Editor, Multiplayer, Gameplay, Equipment **and Overthrow**. **Tile art renders** behind the cards (a missing background is the `m_aTileBackgrounds` canary, and it would be a script error, not a blank tile).
- [ ] **I1, from the PAUSE MENU.** `Esc` → Field Manual, in a running campaign. Same six headings, same tiles. This route also removes the menu background instead of the blur, so check it separately.
- [ ] **No second "Introduction".** Under the **Overthrow** heading the button reads **Getting Started**. There must be exactly one button named Introduction in the whole list.
- [ ] **The Overthrow page still works.** Click Getting Started → one tile ("Main Menu") → open it. The big Overthrow image, the main-menu paragraph, and the Map Info / Fast Travel / Resistance headings all render, and the `<action name="OverthrowMainMenu"/>` glyph inside the first paragraph draws as a key/pad glyph, not as raw text.
- [ ] **Vanilla is untouched.** Open one page in each of Introduction, Editor, Multiplayer, Gameplay and Equipment. Search for something in the searchbar. Use the breadcrumbs. All exactly as before.
- [ ] **I2 — the deep link, the acceptance test.** Reset the seen store, buy something at a shop to arm `economy-first-buy`, press the HUD prompt to escalate it, then in the modal press **Learn more**. The Field Manual must open **directly on the Overthrow "Main Menu" page** — reading view, not the tile grid, not the front page — and **the tutorial popup must be gone**. (Landing on the front page instead is the specific symptom of the `OnMenuShow` → `SetCurrentEntry(null)` ordering trap; the code navigates after `OpenMenu` returns precisely to avoid it.)
- [ ] **Gamepad, mouse unplugged, for that same route.** Reach Learn more with the stick/d-pad, press `A`. The manual opens on the page; **`B` closes the whole manual** (not back to the grid — `m_bOpenedFromOutside` is set on purpose); focus is somewhere usable in the manual, and the tutorial popup does **not** eat `B` from underneath it.
- [ ] **I2 — a wrong key must be harmless.** Temporarily set `m_sFieldManualTitleKey` in `Configs/Tutorials/proofFirstBuy.conf` to `#OVT-FieldManual_NoSuchPage`, repeat the Learn-more route: the manual opens **on its front page**, a `[Overthrow.FieldManual]` WARNING names the key in the log, and there is no error and no crash. Put the key back afterwards.
- [ ] **Learn more is still absent** on an entry with no link (temporarily blank `m_sFieldManualTitleKey`), and its shortcut (`F` / `RB`) does nothing while it is hidden.
- [ ] *(Expected, not a bug — confirm it looks acceptable rather than filing it.)* Opening the manual this way leaves the **Overthrow HUD visible behind it**: closing the popup restores the HUD, and the Field Manual dialog does not hide it. This is exactly what the base game's own hint deep link does — `SCR_HintManagerComponent.OpenContext` (`:231-240`) calls `SCR_FieldManualUI.Open` straight from gameplay, and only the *pause* menu hides the HUD (`SCR_PauseMenuUI.c:397`). If it reads badly, the fix belongs in a follow-up, not here.

### From Phase 6 — the modal (nothing here is machine-checkable)
- [ ] **Export the string table first** or every button renders a raw key.
- [ ] **Gamepad with the mouse unplugged.** Left stick + d-pad move focus across the footer; `A` activates; `B` closes; the glyph row reads `B` / `Y` / `X` / `R3` / `RB`.
- [ ] **Sequence:** two-page entry shows `1 / 2`; Next → page 2 and relabels to **Finish**; Back → page 1 and greys out; Back again does nothing **and focus jumps to Next** (a deliberate rescue — `SCR_InputButtonComponent.SetEnabled` disables the widget itself and can strand pad focus). Finish closes.
- [ ] **Single-page entry:** Next and Back absent, and `E`/`Q`/`Y`/`X` do nothing.
- [ ] **6.7, the actual fix:** arm a tip, then enter placement immediately — the popup must **not** appear over the ghost, and must appear after you cancel. With a popup up, entering placement must retire it instantly while `LB`/`RB` still rotate. Repeat for Build and **both** removal modes.
- [ ] **Tips route:** greyed with no tip; enabled after pressing the HUD prompt; refuses with a hint if picked mid-placement.
- [ ] **Die with the modal open** — no script error, and the tip is still available after respawn.
- [ ] **R7 sweep — now mandatory.** Open, navigate and close **all 17** menus on keyboard and pad. The shared `ShowLayout()` changed for every one of them (task 6.6).

### From Phase 7 — the field manual (config half is asserted; do NOT re-count by eye)
- [ ] **Export the string table first**, or the new category button renders the raw key.
- [ ] **Main menu → Field Manual: six headings draw** — Introduction, Editor, Multiplayer, Gameplay, Equipment **and Overthrow** — and **tile art renders** behind the cards.
- [ ] **Pause menu → Field Manual: same six.** A separate check — that route strips the blur instead of the background.
- [ ] **Exactly one button named "Introduction".** Under the Overthrow heading it must read **Getting Started**.
- [ ] The Overthrow page still renders: image, main-menu paragraph with a live `<action name="OverthrowMainMenu"/>` glyph, and the Map Info / Fast Travel / Resistance headings.
- [ ] **Vanilla untouched** — open a page in each of the five base categories, use the searchbar and the breadcrumbs.
- [ ] **I2, the acceptance test.** Fresh seen store → buy at a shop → press the HUD prompt → **Learn more**. The manual must open **directly on the Overthrow "Main Menu" page** (reading view, not the grid, not the front page) and the popup must be gone. Front page instead = the `OnMenuShow` → `SetCurrentEntry(null)` ordering trap has bitten.
- [ ] **Gamepad, mouse unplugged, same route.** Stick/d-pad to Learn more, `A` to activate; **`B` closes the whole manual** (intended — `m_bOpenedFromOutside` is set, matching vanilla's hint deep link) and the tutorial context must not eat `B` from underneath.
- [ ] **Wrong key is harmless.** Temporarily set the link to `#OVT-FieldManual_NoSuchPage`: front page, one `[Overthrow.FieldManual]` WARNING naming the key, no error.
- [ ] ℹ️ **Expected, not a bug:** the Overthrow HUD stays visible behind the manual on this route. Vanilla does the same — `SCR_HintManagerComponent.OpenContext` (`:231-240`) opens it straight from gameplay, and only the pause menu hides the HUD.

### From Phase 8 — the two proof entries (do these FIRST; everything else builds on them)

- [ ] **Export the string table in Workbench before anything else.** Seventeen new ids, listed in the Phase 8 table above. Without it every popup renders raw `#OVT-` keys and no visual check below is meaningful.
- [ ] **`welcome-intro`, the sequence (F6).** Clear the `OVT_TutorialSettings` block, start a campaign and spawn. The **two-page modal** opens: header `1 / 2`, Next → page 2 + button relabels to **Finish**, Back → page 1 + Back greys out, Back again does nothing and focus jumps to Next, Finish closes. It must **not** reappear on a second spawn, a second campaign, or after a restart.
- [ ] **`welcome-intro` has no Learn more** — the button must be absent entirely, and its shortcut (`F` / `RB`) must do nothing. This is the branch that previously needed a shipped key temporarily blanked; it now ships covered.
- [ ] **`economy-first-buy` still has Learn more** and it opens the manual on the Overthrow page (I2).
- [ ] **Priority (F9).** Spawn (welcome queues at priority 100) and buy something almost immediately (buy tip at 0). The welcome must show first, and the buy tip only after the welcome is dismissed — never both at once.
- [ ] **Content read (the whole point).** Read both entries as a new player would. Neither may read as an instruction: no "you must", no "your objective is", no implied mission list. If either does, it is a string edit in the `.st` and a re-export — no code change.
- [ ] **F11 duplicate-id check.** Temporarily add a second `m_aTutorialEntries` element pointing at `proofFirstBuy.conf`, start a campaign: the log must carry `[Overthrow.Tutorial] Duplicate tutorial entry id 'economy-first-buy' at index 2` at ERROR, and **no** tip should fire that session (validation is terminal by design). Revert the prefab afterwards.

### From the 2026-08-11 change set — tips over screens, the HUD Learn more, and PLAYER_ENTER_BASE

*Everything here is rendering, input or a server tick — none of it is reachable by the harness. Do these on a profile with the tutorial reset (Overthrow menu → Tips → Reset), since all five entries are once-ever.*

**Tips drawn over the screen they are about (the whole point of the change):**
- [ ] Open the map for the first time → the tip appears **over the map**, within about a second, and does **not** vanish when it does. It should still time out on its own after 20 s.
- [ ] Same for the **place menu**, the **real estate menu** and the **skills/character sheet**. Each tip must be legible against that screen's own art — the popup is a fixed 460 px panel on the left edge, vertically centred, and none of these four screens has been seen behind it before.
- [ ] ⚠️ **The regression risk of this change is every other Overthrow menu.** `OVT_UIContext` no longer calls `hud.SetVisible(false)` — it hides all HUD layers except ALWAYS_TOP. Open **all 17** menus and confirm the HUD still disappears behind each one exactly as it used to: no money readout, no wanted stars, no transfer progress bleeding through.
- [ ] A tip that does **not** set the flag (buy something, then immediately open a menu) must still behave the old way: it disappears and does not come back.
- [ ] The place tip now fires on **opening the place menu**, not after placing. Confirm it no longer appears after a placement, and that it does appear the first time the menu opens.

**The Learn more prompt on the non-modal tip:**
- [ ] With a tip up, press **F5** (keyboard) → the Field Manual opens **on that entry's page**, and the tip is gone. Then press F5 with no tip on screen → nothing at all happens.
- [ ] On a gamepad, the prompt reads **LT + Y**, and the chord opens the manual. Expect LT and Y to *also* do their normal jobs on the way through — that is the accepted trade, not a bug.
- [ ] An entry with no `m_sFieldManualTitleKey` shows **no** Learn more prompt, and F5 / LT+Y do nothing while it is up. (`welcome-intro` is modal, so pick or temporarily blank a non-modal entry's key to check this.)
- [ ] The old **Overthrow Menu** prompt still sits beside it on a normal tip (buy something) and still escalates to the Tips screen.
- [ ] On the four **show-over-UI** tips that prompt must be **absent**, leaving Learn more alone in the footer — and pressing the main-menu key with one up must still open the Overthrow menu as usual, just without handing the tip over.
- [ ] Learn more must be reachable over all four screens above, where a menu is already eating input.

**PLAYER_ENTER_BASE:**
- [ ] Walk toward a base the occupying faction holds → the base tip fires **on the way in**, once, and not once per second while inside. Leave and re-enter: it must not fire again (seen store).
- [ ] Do the same **disguised**, and again on a server with the wanted system **off** — both must still fire, which is the reason the check sits above those guards.
- [ ] Bring a **recruit** into a base's range: no tip for them, and none duplicated for you.
- [ ] Capture a base the old way (win a QRF): the tip must **not** fire on the outcome any more.
- [ ] ⚠️ **Read the tip's art against the new moment.** `bases-first-capture-ui.edds` was shot for a tip about a base *changing hands*; the tip now fires on *arrival*. If the screenshot shows an outcome, it is now a picture that contradicts its own page — the same fault first-spawn caught with the car-in-a-garage header.
- [ ] The body text is **owed a rewrite** (see the scratchpad note) and still describes the old event until the `.st` merge is resolved.

### ~~From Phase 4 — the settings store's cross-restart half (Q3)~~ RETIRED 2026-08-18
*The per-machine profile store was deleted outright (it was re-showing tips on every load in practice — the user's report). Seen state now rides the campaign save via `OVT_PlayerManagerSerializer` v4; the checks below replace this block.*

### From the 2026-08-24 change set — the seen-state push on every arrival path

*Root cause and reasoning: `context.md` -> 2026-08-24. The mechanism half is now covered by
`OVT_TEST_Campaign_Tutorial_StatePushReachesClient`; everything below is the call-site half, which no
tier can reach.*

- [ ] **Run the suites first** — Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}`. The new Campaign case must pass, and it has never been proven red.
- [ ] **The reported failure, on the dedicated server.** Dismiss several tips (at least one menu tip and one shop tip), let the server save, **restart the server**, log back in: not one dismissed tip may return. This is the exact case that was broken.
- [ ] **Reconnect inside one session.** Dismiss a tip, disconnect, reconnect without restarting the server — still gone. (The push now runs on the already-finalized early return too.)
- [ ] **Single player / listen host.** Same check across a save + quit + Continue. SP takes the same call site and had the same defect.
- [ ] **A brand-new player still gets everything.** Fresh persistent id on the same server: the welcome modal fires and the tips behave as they always did — the push must not suppress an empty record.
- [ ] **"Don't show tips again" survives a restart** for a returning player, and Reset still turns them back on.
- [ ] **Two-client:** A dismisses a tip and reconnects (no re-show); B, who never dismissed it, still gets theirs. This is the same per-player isolation the epic still owes an observation of.
- [ ] ⚠️ **Watch the log on a busy join.** The push retries 20 x 500 ms while the controller is unregistered and then gives up **silently**. If a tip repeats for one player and not another, that retry expiring is the first suspect — it has no log line by design.

### From the 2026-08-18 change set — campaign-persisted seen store, real modality, BUG-159

**Before anything: close Workbench and run the suites** — `tools/compile-check.sh` (already 0), `tools/run-tests.sh` Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}`. The new `OVT_TEST_PersistenceRoundTrip_TutorialSeen_SurvivesSaveAndReload` case must pass, and it has never been proven red.

**Seen state in the campaign save:**
- [ ] Fresh campaign, dismiss the welcome and one buy tip, **save and quit, Continue** — neither returns. (This is the exact user-reported failure of the old store.)
- [ ] Start a **new campaign** on the same machine — the welcome shows again (accepted consequence, confirm it reads as intended).
- [ ] Press "Don't show tips again", save/quit/Continue — still off. Reset tips from the menu, save/quit/Continue — still on.
- [ ] **Two-client MP:** A dismisses a tip; B still gets theirs (per-player record). A reconnects — no re-show (the `PushTutorialState` push on rejoin).
- [ ] **Pre-v4 save:** Continue a campaign saved before this build — loads clean, tips simply show again (the `version < 4` clear).

**The modal is modal:**
- [ ] Welcome opens on spawn: **no movement, no aim, no fire** while it is up; every button works on keyboard AND pad (`Esc`/`E`/`Q`/`N`, `B`/`Y`/`X`/`R3`); controls return the instant it closes.
- [ ] Die-with-modal-open still restores controls after respawn (the disable is per-character and the close path re-enables).

**BUG-159:**
- [ ] Gamepad: open the place menu on a fresh profile — the place tip draws over it and the **d-pad keeps driving the menu the whole 20 s**.
- [ ] A modal tip closed while a menu is open puts focus back where it was.

**Still owed from before:** the two stale `BasesFirstCapture` authoring Comments in `localization_Overthrow.st` (they still describe the old BASE_CONTROL_CHANGE trigger; the shipped body text is already correct).

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

*Update this file as tasks are completed. Task ids match `implementation.md` — do not renumber.*
