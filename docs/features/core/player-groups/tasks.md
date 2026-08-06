# Player Groups - Task Checklist

**Last Updated:** 2026-08-06 23:05
**Progress:** 30/31 tasks complete (97%) — all six phases built ✅ · the one open task is T1.3, a user play-test

**Epic:** `core` (feature #6) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md`

> Task ids match the `T<phase>.<n>` ids in `implementation.md` — do not renumber them.
> **Agent tiers are set by the plan** (see its Agent Routing Summary): Phases 3, 4 and 5 are **ADVANCED**.

---

## Phase 1: Diagnose the vanilla Group Menu (2/4 complete, 2 superseded) ✅ — `ui-developer`

*Measurement, not code. Delivered as a **static code-reading verdict** instead of instrumentation + play-test — the live half needs two client processes on a dedicated server and a human. Verdict is in `context.md`.*

- [x] ❌ **T1.1 — Instrument the client-side group menu path** — **CANCELLED / superseded.** The verdict was reached by reading the real vanilla sources; shipping throwaway `[OVT-GRPDIAG]` prints that only a human play-test could read would have left instrumentation in the tree against the phase's own acceptance criteria. Zero code written.
- [x] ❌ **T1.2 — Server-side playable-group table dump** — **CANCELLED / superseded**, same reason. The existing server-log evidence in `docs/bugs/BUG-088.md` supplied the group/faction table the dump would have produced (`Group faction: FIA`).
- [ ] ⏸️ **T1.3 — Two-client confirmation (dedicated server)** — **DEFERRED TO THE USER.** Not instrumentation any more: just open the Group tab on two clients and confirm tiles + Join appear. Listed under "Needs human verification" in `context.md`.
- [x] ✅ **T1.4 — Verdict written into `context.md`**
  - **No residual fault in the listing/selection path** — every reported symptom is downstream of the single null BUG-088 removed (`GetLocalPlayerFaction()` → `m_MappedPlayerFactionInfo`). No tile ⇒ no selection ⇒ Join never becomes visible. D11 recorded as out of scope.
  - **Phase 5's seam did NOT move** — `JoinSelectedGroup():259` / `AcceptInvite():285` re-verified exact and confirmed to be the only routes; no task-list amendment needed.

**Corrections the verdict made to the plan (all folded into Phases 2/3/5 below):**
1. 🐛 **`SetName()` is the wrong call and F1 is false even in solo today** — `OVT_SpawnLogic.c:878` uses the engine entity-name setter; `SCR_AIGroup` has no such method and no UI reads it. Tiles draw callsign + `GetCustomName()`. Must be `SetCustomName(playerName, playerId)`.
2. The **faction-mismatch hypothesis is false** — `SetCivilianFaction` is a misnomer that assigns `m_sPlayerFaction` (default **FIA**); both sides hold the same `Faction` instance.
3. The **`PlayableGroup.et` suspect is eliminated** — same-GUID addon files are deltas, not replacements (`persistence/tasks.md` is wrong about this; see `context.md`).
4. **D3's line is `:560`, not `:562`**, and `playerComponent` is **not null-checked** there — a disconnect between request and approval can null-deref the server.
5. **T2.1 and T2.2 must ship together** — groups are public today so the join takes the working public branch; `SetPrivate(true)` switches it to the private branch that dies at `:560`.
6. `IsPrivacyChangeable() == false` degrades cleanly but **also removes the rename button**.
7. Expect a **stray empty FIA group** — `m_bEnableAutoGroupCreationWhenFull` defaults 1, so vanilla makes one before Overthrow does.

---

## Phase 2: Private-by-default groups + the two server-authority fixes (3/3 complete) ✅ — `network-specialist`

- [x] ✅ **T2.1 — Private flag at group creation + the group-name fix**
  - Description: Replace `newGroup.SetName(playerName)` with **`newGroup.SetCustomName(playerName, playerId)`** (Phase 1 correction #1 — `SetName` is the engine entity-name setter, no UI reads it, so F1 is false even in solo today). Then `SetPrivate(true)` and `SetPrivacyChangeable(false)` (the latter also blocks vanilla's auto-unlock at `SCR_GroupsManagerComponent.c:748`/`:759`). Known cost of `SetPrivacyChangeable(false)`: it **also hides the rename button** — accepted, the group is named after its owner by design. Fallback if it breaks the attributes UI = drop the call and re-assert `SetPrivate(true)` from the reactor, recorded in `context.md`.
  - File(s): `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c` (~:878)
  - Estimate: 🟡 1 h
- [x] ✅ **T2.2 — Fix the BUG-088-family hole in the leader-approve path (D3)**
  - Description: `modded class SCR_PlayerControllerGroupComponent` overriding the **non-RPC wrapper** `RequestJoinGroup(int)`: call `RPC_AskJoinGroup` directly when `Replication.IsServer()`, else `Rpc(...)`. Fixes every server-side caller at once (approve path + `RejoinPlayer`); no-op on clients. **Also add the missing null guard** on `playerComponent` at `RPC_ConfirmJoinPrivateGroup` **`:560`** (Phase 1 correction #4 — the plan's `:562` is `RemoveRequester`; a disconnect between request and approval null-derefs the server there).
  - File(s): NEW modded-class file — put it at `Scripts/Game/Player/Modded/SCR_PlayerControllerGroupComponent.c`, alongside the repo's existing `Scripts/Game/Player/Modded/SCR_PlayerController.c`
  - Estimate: 🟡 2 h
  - ⚠️ **Ships together with T2.1** (Phase 1 correction #5): groups are public today, so the join currently takes the working *public* branch. `SetPrivate(true)` is what switches it onto the private branch that dies at `:560` — landing T2.1 without T2.2 would *introduce* the broken join.
- [x] ✅ **T2.3 — Fix the full-group hole (D4)**
  - Description: `modded class SCR_GroupsManagerComponent` overriding `MovePlayerToGroup` to return `previousGroupID` when the target group is null or `IsFull()` — **before** vanilla empties the player's current group. Prevents a live player at `GetGroupID() == -1` (the G1 violation).
  - File(s): NEW modded-class file
  - Estimate: 🟡 1-2 h

**Gate result 2026-08-06:** `tools/compile-check.sh` **exit 0** (5918 files) · All group **exit 0** (60 tests, 20s) · ⏸️ play-test steps 1, 5, 14 are the user's.

**Phase 2 notes:**
- Files: `OVT_SpawnLogic.c` (edit) · `Scripts/Game/Player/Modded/SCR_PlayerControllerGroupComponent.c` (new) · `Scripts/Game/Modded/SCR_GroupsManagerComponent.c` (new).
- **Phase 1 correction #4 turned out half wrong** — `playerComponent` really has no null test at `:558-560`, but `:554-556` already returns on a null `PlayerController`, so the disconnect-between-request-and-approval case is already closed by vanilla. Deliberately not patched (would require overriding an `[RplRpc]` method with no precedent in this repo); rationale + a revisit trigger are in `context.md`.
- `RPC_DoSetCustomName` routes the name through the async platform profanity filter, so a custom name settles a frame or two after the call. Harmless.

---

## Phase 3: `OVT_PlayerGroupManagerComponent` — the own-group guarantee (5/5 complete) ✅ — `network-specialist-advanced` 🔴 ADVANCED

*Deliberately excludes recruits so the group lifecycle is play-testable on its own.*

- [x] ✅ **T3.1 — Create the manager**
  - Description: Manager pattern (`s_Instance` in `OnPostInit`, `GetInstance()`, `SCR_Global.IsEditMode()` early-out) modelled on `OVT_RecruitManagerComponent.c:82-122`. Register on the game-mode prefab. **No `OVT_Global` accessor** (locator half is frozen — D10).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PlayerGroupManagerComponent.c` (NEW), `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟡 2 h
- [x] ✅ **T3.2 — Move group creation into `EnsureOwnGroup(int playerId)`**
  - Description: Idempotent (no-op returning the current id when `GetGroupID() != -1`); keeps `SetCivilianFaction`, the faction null-check + BUG-088 error print, the **`SetCustomName`** call (not `SetName` — Phase 1 correction #1), and the T2.1 privacy calls. `OVT_SpawnLogic.CreateAndJoinGroup` becomes a thin caller that keeps its retry ladder **and keeps firing `m_OnPlayerGroupCreated` from the spawn path only** (D6).
  - File(s): `OVT_PlayerGroupManagerComponent.c`, `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`
  - Estimate: 🔴 3-4 h
- [x] ✅ **T3.3 — Subscribe to the static membership invokers (server-only)**
  - Description: `SCR_AIGroup.GetOnPlayerAdded()/GetOnPlayerRemoved()` in `OnPostInit`. Both fire from `RplRcver.Broadcast` RPCs called locally-then-broadcast by the authority, so they fire on the server for every membership change — **and on clients**, hence an explicit `Replication.IsServer()` guard in each handler body.
  - File(s): `OVT_PlayerGroupManagerComponent.c`
  - Estimate: 🟡 1-2 h
- [x] ✅ **T3.4 — The one-frame deferred return-to-own-group (D7, §3.3)**
  - Description: `OnGroupPlayerRemoved` → `CallLater(RestoreOwnGroupDeferred, 0, false, playerID)`; the deferred handler no-ops unless the player still has a live `PlayerController` **and** `GetGroupID() == -1`, then calls `EnsureOwnGroup`. One mechanism covers normal switch / genuine leave / rejected join / disconnect.
  - File(s): `OVT_PlayerGroupManagerComponent.c`
  - Estimate: 🔴 3 h
- [x] ✅ **T3.5 — Verify the deletion-timing interaction + Init-tier assertion**
  - Description: Confirm by log that Overthrow's removal handler sees a non-null `group.GetSlave()` (vanilla's `DeleteGroupDelayed` drains next frame). Add the Init-tier assertion that `OVT_PlayerGroupManagerComponent.GetInstance()` resolves after manager init, **proven able to fail** (remove the component from the prefab → exit 1 → restore) with the method recorded in the case comment.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟡 2 h

**Gate result 2026-08-06:** `compile-check.sh` **exit 0** (5919 files) · All group **exit 0**, **61 tests** (was 60) · fail-proof verified both directions (prefab entry deleted → `OVT_TEST_Init_PlayerGroups_ManagerResolves` **exit 1** on the intended assertion, verbatim in `junit.xml`; restored → **exit 0**), method recorded in the case comment, no `maxAttempts` · ⏸️ play-test steps 1, 9, 10, 14 are the user's.

**Phase 3 discoveries — three things vanilla contradicted the plan on:**
1. **§3.3's "rejected join → -1 → restore" case no longer exists.** Phase 2's T2.3 rejects *before* `super`, so neither invoker fires. The deferral has **three** live cases, not four.
2. 🐛 **Phase 2 silently activated a D2 violation.** `m_bAllowRejoinPlayerAfterReconnecting` defaults to `1`, and T2.2 made `RejoinPlayer` work *for the first time* — so reconnect would have put you back in your friend's group. Fixed by setting the attribute to `0` on the game-mode prefab.
3. 🐛 **Vanilla has no Leave action at all** — "Create new group" *is* the exit, and it was permanently disabled, because the stray empty FIA group makes `TryFindEmptyGroup()` non-null forever (killing both `CanCreateNewGroup:1347` and `RPC_AskCreateGroup:667`). **Nobody could ever leave a group.** Addressed by empty-group adoption in `EnsureOwnGroup` + a deletion-queue guard (`IsGroupQueuedForDeletionOVT()`).

**Three calls a reviewer should scrutinise** (all recorded as decisions in `context.md`, all reversible): empty-group adoption + the deletion-queue guard; `OnGroupPlayerAdded` claiming a group you are alone in (vanilla's leave-group is public and unnamed, so without it Phase 3's own acceptance criterion fails); subscriptions left unconditional with guarded bodies (vanilla's own trade at `:1723-1724`) rather than `IsServer()`-gated subscriptions.

**Files:** `OVT_PlayerGroupManagerComponent.c` (new) · `OVT_SpawnLogic.c` (thin caller, retry ladder byte-identical) · `OVT_OverthrowGameMode.et` (registers the manager `{6A7D9B2E00000080}` + the rejoin attribute) · `SCR_GroupsManagerComponent.c` (+`IsGroupQueuedForDeletionOVT()`) · `OVT_TEST_InitSuite.c`.

---

## Phase 4: Recruits follow their owner + the leader-guard fix + Logic tier (6/6 complete) ✅ — `component-developer-advanced` 🔴 ADVANCED

- [x] ✅ **T4.1 — The pure helper `OVT_GroupRecruitTransfer`**
  - Description: Static-only, world-free, manager-free: `SelectTransferable(ownedRecruits, ownerOnline, out outSkippedOffline)`, `ProjectSlaveAiCount`, `ExceedsAiBudget` (`budget <= 0` = disabled). Owner liveness is passed in from `PlayerManager.GetPlayerController()` — **never** `OVT_PlayerData.IsOffline()` (D9); leave the `//!` comment saying why.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c` (NEW)
  - Estimate: 🟡 2 h
- [x] ✅ **T4.2 — `MoveRecruitsToGroup(ownerPersistentId, targetGroup, ownerOnline)`**
  - Description: For each transferable id, `FindRecruitEntity` → activate the AI (`AIControlComponent.ActivateAI()`) → `AddAIToSlaveGroup(entity, targetGroup)` on the **owner's** `SCR_PlayerControllerGroupComponent` (the call already used at `:1764`). Keep the `GetSlave()` null guard.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🔴 3 h
- [x] ✅ **T4.3 — `RemoveRecruitsFromGroup(ownerPersistentId, exGroup)`**
  - Description: Vanilla has no public mirror of `AddAIToSlaveGroup` — write it: `exGroup.GetSlave().RemoveAgentFromControlledEntity(entity)` then `AskRemoveAiMemberFromGroup(slaveRplId, characterRplId)`, modelled on vanilla `OnAIMemberRemoved` (`SCR_GroupsManagerComponent.c:1593`) for the RplId resolution.
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🔴 3 h
- [x] ✅ **T4.4 — Wire the reactor**
  - Description: `OnGroupPlayerAdded` → `MoveRecruitsToGroup(...)`; `OnGroupPlayerRemoved` → `RemoveRecruitsFromGroup(...)` **synchronously** (before next-frame group deletion). Confirm by log that a normal switch nets exactly one slave-group membership.
  - File(s): `OVT_PlayerGroupManagerComponent.c`
  - Estimate: 🟡 2 h
- [x] ✅ **T4.5 — The leader-guard fix (D8) — the concrete breakage**
  - Description: `OVT_RecruitManagerComponent.c:1737` and `:1963` both demand `GetLeaderID() == playerId`; an owner inside a friend's group is not the leader, so recruiting-while-shared and reconnect-recruit-respawn silently do nothing. Change both to `IsPlayerInGroup(playerId)`. `:1963` **also gets a retry cap of 10** (it re-arms a 500 ms `CallLater` with no counter today) + `LogLevel.WARNING` on exhaustion. Keep the `groupId == -1` and `GetSlave()` guards.
  - File(s): `OVT_RecruitManagerComponent.c`
  - Estimate: 🟡 2 h
- [x] ✅ **T4.6 — Logic-tier coverage**
  - Description: New suite in the **Fast** group: empty owner; all-online; mixed online/offline (exact `outSkippedOffline`); owner offline → empty; `ProjectSlaveAiCount`/`ExceedsAiBudget` arithmetic incl. `budget <= 0`. **Every case proven able to fail once**, method recorded in a preamble comment (match `OVT_TEST_Logic_Skills.c`). No `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GroupRecruits.c` (NEW)
  - Estimate: 🔴 3 h

**Gate result 2026-08-06:** `compile-check.sh` **exit 0** (5921 files) · Fast **exit 0, 38 tests** · All **exit 0, 66 tests** (was 61) · fail-proof: three separate injected faults each turned the intended cases red (**exit 1**, 3/38 then 2/38), revert → **exit 0, 38**; method recorded in the suite preamble, no `maxAttempts` · ⏸️ play-test steps 2, 6, 7, 8, 9 are the user's.

**Phase 4 findings:**
- **Vanilla DOES have a public mirror of `AddAIToSlaveGroup`** — `SCR_PlayerControllerGroupComponent.RemoveAiFromSlaveGroup` (`:1526`), which also `Deactivate()`s the last agent, a step the planned hand-roll would have missed. Its body was inlined rather than called, because it is reached through a player controller and the pull-out must work for an owner who no longer has one.
- **D9's rationale is stale** — `OVT_PlayerData.IsOffline()` already reads `id <= 0`. The decision stands, for a better reason now stated in the code comments.
- The removal broadcast is **already double-fired in vanilla** (`RPC_DoSetGroupSlave` subscribes `OnAIMemberRemoved` to the slave's `GetOnAgentRemoved`); idempotent, and the explicit synchronous call is what orders remove-before-add.
- ⚠️ **The retry cap counts both re-arm branches**, not just the membership one — read literally, "keep the `-1` guard as-is" would have left an unbounded loop. Exhaustion logs at WARNING (a returning player's recruit bodies are not asked back that session).
- ⚠️ **A disconnect does not reach the pull-out yet** — `ClearPlayerIdMappings` (`OVT_OverthrowGameMode.c:836`) runs *before* `super` (`:838`), which is what drives `SCR_AIGroup.OnPlayerDisconnected → RemovePlayer`. It bails loudly at WARNING. **Fix is T6.3**; the seam (`m_OnPlayerDisconnected`, `:832`) is documented.
- "Exactly one slave-group membership" was verified **by code trace plus a paired log line**, not at runtime — §6 step 6 and the `Pulled N … / Moved N …` grep are the user's half.

---

## Phase 5: Group Menu UX layer + localization (6/6 complete) ✅ — `ui-developer-advanced` 🔴 ADVANCED

*Amend this list first if Phase 1's verdict moved the seam.*

- [x] ✅ **T5.1 — `modded class SCR_GroupSubMenuBase`: confirm-then-`super` on both join routes**
  - Description: Override `JoinSelectedGroup()` and `AcceptInvite()`; resolve target group + leader name, count the local player's recruits, open the dialog, call `super` **only** from the confirm handler. Skip the dialog when the target is your own group. Client-side `IsFull()` pre-check with a "group is full" message (G9). Never change membership locally.
  - File(s): NEW modded-class file for `SCR_GroupSubMenuBase`
  - Estimate: 🔴 4 h
- [x] ✅ **T5.2 — Two dialog presets**
  - Description: `JOIN_GROUP_CONFIRM` and `JOIN_GROUP_CONFIRM_RECRUITS`, copying `DELETE_CAMP`/`DISMISS_RECRUIT` exactly — `confirm` = `DialogConfirm`, `cancel` = `MenuBack` (that wiring is what makes them gamepad-usable). Opened via `SCR_ConfigurableDialogUi.CreateFromPreset`; leader name and recruit count substituted after creation.
  - File(s): `Configs/UI/Dialogs/DialogPresets_Campaign.conf`
  - Estimate: 🟡 2 h
- [x] ✅ **T5.3 — Explanatory text in the Group tab**
  - Description: State the Overthrow model (your own group by default; joining hands command of your recruits to that leader while you are there). Prefer adding a text widget from the modded `OnTabCreate` over forking a vanilla `.layout`; if a fork is unavoidable, follow the GUID rules in `overthrow-ui-patterns`.
  - File(s): modded `SCR_GroupSubMenuBase` (+ `.layout` only if unavoidable)
  - Estimate: 🟡 2 h
- [x] ✅ **T5.4 — Leader notification (G8, non-blocking)**
  - Description: Server-side in `OnGroupPlayerAdded`: when the joiner brings ≥1 recruit, `OVT_Global.GetNotify().SendTextNotification(tag, leaderId, playerName, count)`. Do **not** add an `ENotification` enum value.
  - File(s): `OVT_PlayerGroupManagerComponent.c`
  - Estimate: 🟢 1 h
- [x] ✅ **T5.5 — Localization**
  - Description: All new strings as `#OVT-` keys in `Language/localization_Overthrow.st` **only**. **Never** touch `localization_Overthrow.<lang>.conf` (Workbench-generated; hand-editing corrupts them silently). List every key awaiting a Workbench export pass in `context.md`; literal text until exported.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h
- [x] ✅ **T5.6 — Gamepad/console pass**
  - Description: Group tab reachable; Join and Accept focusable; dialog confirm/cancel respond to `DialogConfirm`/`MenuBack`; no mouse-only affordance introduced.
  - File(s): —
  - Estimate: 🟡 1-2 h

**Gate result 2026-08-06:** `compile-check.sh` **exit 0** (5922 files) · All **exit 0, 66 tests** (unchanged — UI adds nothing assertable) · `git diff --stat Language/` → **only `localization_Overthrow.st`** (1 file, +153) ✅ Q5 · conflict checker exit 0, no duplicate widget GUIDs · ⏸️ play-test steps 3, 4, 12, 13, 16 are the user's.

**§3.4's single-seam claim re-verified and still holds** — `JoinSelectedGroup` `:259` (2 refs in the whole vanilla tree), `AcceptInvite` `:285` (1 call site); both subclasses read in full, neither overrides either. `GrouplistFooter` exists in **both** `GroupMenuPlayerlist.layout:143` and `SelectGroupMenu.layout:132`, so the deploy route gains the text and is otherwise unchanged. **No vanilla layout forked.**

**Phase 5 notes:**
- A **third** preset `JOIN_GROUP_BLOCKED` was added for the refusal messages (justified as a decision in `context.md`).
- `MoveRecruitsToGroup` now returns `int` — what it actually *placed*, not the paper roster — so F5's "N recruits" notification is truthful for free. Signature + `return` only.
- 9 `#OVT-` keys added, GUIDs `{6A7D9B2E00000090}`–`{…98}`; the export table is in `context.md`.
- **Verified statically:** `ShowGroupMenu` = `KC_P`/`view`+`X`; `MenuJoinGroup` = `y` click and `GroupAcceptInvite` = `y` hold, mutually exclusive by visibility; `DialogConfirm` = `a`, `MenuBack` = `b`; a dialog force-disables the buttons beneath it; Overthrow's input `.conf` touches none of these. **Not verified: anything visual or interactive** — layouts, `.meta`, `.conf` and the string table are compiled and tested by nothing.

**Reviewer should scrutinise:** the explanatory paragraph may clip (`GrouplistFooter` is `FillWeight 0.08` in a clipping parent, ~250 chars of text — only fixable by looking at it); `%2` appears twice in the recruits message and repeat-substitution was not verified at runtime; English fallbacks duplicate the `.st` text in a `const` block, so both must be edited together; `OvtOnJoinConfirmed` consuming its pending id on line 1 is a **gamepad double-fire guard** (`a` fires both `DialogConfirm` and `MenuSelect`), not tidiness; the blocked dialog can open from inside a closing dialog in a rare race, left synchronous on purpose.

---

## Phase 6: Edge cases, AI-density measurement, hardening (7/7 complete) ✅ — `network-specialist`

- [x] ✅ **T6.1 — Leader disconnect / promotion** — verify by play-test that recruits stay with their owner and the promoted leader commands them (no Overthrow code keys off leader identity after T4.5). Estimate: 🟢 <1 h
- [x] ✅ **T6.2 — AI-density measurement** — `MAX_SLAVE_AI_PER_GROUP` on the manager, **default 0 = disabled**; warn (joiner + leader) when `ExceedsAiBudget` trips; never refuse the join, never silently drop recruits. Measure 2 players × 16 recruits, record server frame time + slave-group agent count in `context.md`, set the budget only if the number justifies it. Estimate: 🟡 2 h + measurement
- [x] ✅ **T6.3 — Offline owner's recruits are not commandable** — `SelectTransferable` returns nothing when `ownerOnline` is false; removal pulls them out immediately. Liveness from `PlayerManager`, not `IsOffline()`. Estimate: 🟢 1 h
- [x] ✅ **T6.4 — Group deleted mid-join** — vanilla re-resolves and returns on null; T2.3 keeps the requester in place. Add the "that group no longer exists" user-facing message. Estimate: 🟢 1 h
- [x] ✅ **T6.5 — Join/recruit/leave in quick succession** — guard both transfer methods against a null `FindRecruitEntity`, `LogLevel.WARNING` instead of dereferencing. Estimate: 🟢 1 h
- [x] ✅ **T6.6 — Server-rejected approve surfaces a message** — `RPC_ConfirmJoinPrivateGroup` does not check `IsFull()`, so post-T2.3 a leader approving into a full group is a silent no-op; send the requester a "group is full" text notification from the reactor when a confirm produces no membership change. Estimate: 🟡 1-2 h
- [x] ✅ **T6.7 — Contain the `FindRecruitEntity` mid-iteration hazard** — snapshot the id list before iterating so this feature does not amplify the known `OVT_RecruitManagerComponent.c:1587` hazard. Do **not** fix it here (belongs to `resistance/recruits`). Estimate: 🟢 <1 h

**Gate result 2026-08-06:** `compile-check.sh` **exit 0** (5922 files) · Fast **exit 0, 38 tests** · All **exit 0, 66 tests** (independently re-run by the orchestrator: exit 0, 66 tests, 28s) · `OVT_PlayerCommsComponent.c` diff **empty** ✅ Q6 · `Configs/Systems/Persistence/` + `Scripts/Game/Persistence/` diffs **empty** ✅ I4 · zero `[OVT-GRPDIAG]` hits repo-wide ✅ Q10 · no new `OVT_Global` getter ✅.

**Phase 6 outcomes:**
- **T6.3 (the real gap) — BUILT.** Ordering confirmed: `m_OnPlayerDisconnected.Invoke` (`:832`) → `ClearPlayerIdMappings` (`:836`) → `super` (`:838`), and `super` is what reaches `SCR_AIGroup.OnPlayerDisconnected → RemovePlayer`. Fix: an `OnPlayerDisconnecting` hook caches `playerId → persistentId` for one frame (`m_mDepartingPersistentIds`, dropped via `CallLater(...,0,...)`), and `MoveOwnedRecruitsOut` falls back to it — so the pull-out still runs at the single existing site, synchronously, inside the removal frame. Pulling recruits early at `:832` was rejected (the removal has not happened yet there; it would create a second pull-out site with different preconditions). Hook installed from both `OnPostInit` and a new `EOnInit` behind a flag, since sibling-component init order is not safe to bet on.
- **T6.1 — verified, nothing built.** Six leader-identity hits found by grep: a notification target, a log string, the dialog's leader *name*, and the two T4.5 membership tests. **No survivor gates a recruit on leadership.**
- **T6.2 — built, disabled.** `MAX_SLAVE_AI_PER_GROUP = 0` + `WarnIfAiBudgetExceeded` over the already-Logic-pinned arithmetic; two notifications (joiner + leader) and one WARNING. Join never refused, no recruit dropped. ⏸️ **The measurement itself is the user's** — procedure written into `context.md`.
- **T6.4 / T6.5 / T6.7 — verified already satisfied** by Phases 2/4/5; nothing duplicated (row-by-row table in `context.md`).
- **T6.6 — built at the Phase 2 seam, not the reactor** (the reactor cannot observe a non-event): the server branch of `modded RequestJoinGroup` reads `GetGroupID()` back after `RPC_AskJoinGroup` and sends `GroupJoinRefusedFull` when the target was full. Client path byte-identical.
- **Hardening:** every WARNING/ERROR sits on a failure branch; the two routine membership lines are NORMAL. Nothing needed downgrading.
- ⚠️ **One flaky All-group run**: the first returned exit 1 on `OVT_TEST_Init_Loadout_NestedItemsSurviveApply` (`TestResultTimeout` after 60s, in a 152s session with a second Workbench process running). Unrelated path, green on re-run and green again on the orchestrator's independent run. Recorded rather than swept.

---

## Bugs & Issues

**Active Bugs:**
- (none yet — Phase 1's verdict may file one against the vanilla Group Menu)

**Fixed Bugs:**
- [x] ✅ 🐛 **BUG-088 — faction manager prefab had no `RplComponent`** — Fixed & play-test-verified 2026-08-06 (hard precondition for this feature)

---

## Technical Debt

- [ ] 💳 **`OVT_PlayerData.IsOffline()` tests `id == 0` while disconnect sets `id = -1`** - Priority: Medium
  - Belongs to `core/player-manager`. This feature deliberately does **not** consume it (D9) and says so in a code comment.
- [ ] 💳 **`OVT_RecruitManagerComponent.FindRecruitEntity` mid-iteration removal hazard (`:1587`)** - Priority: Medium
  - Belongs to `resistance/recruits`. Contained here by snapshotting ids (T6.7), not fixed.
- [ ] 💳 **Vanilla-update check-list** — the three `modded class` overrides (`JoinSelectedGroup`/`AcceptInvite`, `RequestJoinGroup`, `MovePlayerToGroup`) each mirror a vanilla file:line and will break silently on a vanilla rewrite (R10). List them in `context.md`.

---

## Testing Tasks

- [ ] **Logic tier** — `OVT_TEST_Logic_GroupRecruits` (T4.6), Fast group, every case proven able to fail
- [ ] **Init tier** — manager-resolves assertion (T3.5), proven able to fail
- [ ] **Manual, two clients on a dedicated server** — the 16 numbered steps in `implementation.md` §6 Verification Method are the procedure. Per-phase gates: P2 → 1, 5, 14 · P3 → 1, 9, 10, 14 · P4 → 2, 6, 7, 8, 9 · P5 → 3, 4, 12, 13, 16 · P6 → 11, 15
- [ ] **Solo regression** — re-run steps 1-2 and 13 in single-player

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

*Update this file as tasks are completed. Task ids are the plan's — never renumber.*
