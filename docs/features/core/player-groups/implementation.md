# Player Groups — Implementation Plan

**Status:** 🟢 Ready for Review — all 6 phases built 2026-08-06; automated gates green (compile-check 0 · Fast 38 · All 66). **MP play-test outstanding** (§6, the runtime gate).
**Started:** 2026-08-06
**Target Completion:** built 2026-08-06
**Last Updated:** 2026-08-06 23:05

**Epic:** `core` (feature #6, the last one — see `docs/features/core/epic-overview.md`)
**Requirements:** `docs/features/core/player-groups/requirements.md` (authoritative for scope)
**Addresses:** GitHub issue **#147** (second half — "Cant manage Group menu") and the MP play-test of 2026-08-06

---

## 1. Executive Summary

Overthrow hands every player a one-man group at spawn and nothing supports squading up. This feature makes that group **private-by-default and yours**, and makes joining someone else's group a **deliberate, warned, reversible** act.

The governing rule is **recruits are owned by a player, not by a group**. Recruits follow their owner into whatever group the owner is in; while they are there the group's leader commands them (pure vanilla — the leader commands the group's slave group); ownership (dismiss, rename, inventory, loadout, XP) never transfers, and the recruits come back with the owner when they leave, switch groups or reconnect.

**Approach: a UX layer over vanilla.** Vanilla already ships private groups, request-to-join with leader approval, leader→player invites, and per-group slave groups for AI. Overthrow adds exactly three things:

1. the **private flag at group creation** (one call in the existing spawn path),
2. a **`modded class` UX layer** on `SCR_GroupSubMenuBase` — explanatory text plus a confirmation dialog on both join routes,
3. a **server-side reactor** (`OVT_PlayerGroupManagerComponent`) that moves recruits on membership change and guarantees every connected player always has a group.

This is the least code, the least divergence from vanilla, and the best survival odds across Reforger updates. **No new client→server RPC is required** — see decision D1.

Two vanilla defects sit directly in the path and this feature must fix both (D3, D4). They are the same failure family as BUG-088 and are invisible in solo play.

---

## 2. Goals

### Primary

- **G1** Every connected player is always in a group. There is no window in which `GetGroupID() == -1` for a live player — that is the BUG-088 failure mode (no commanding, no recruits).
- **G2** A player's own group is created **private**; another player must request and be approved, or be invited and accept. Nobody is ever silently added to someone else's group.
- **G3** Joining another player's group is **warned before it happens**, on both sides, naming the other leader and the recruit count.
- **G4** A player's online recruits **follow their owner** into and out of any group, server-authoritatively, and never remain in an ex-group's slave group.
- **G5** Owner-only actions (dismiss, rename, open-inventory, loadout apply, show-on-map) and kill XP stay with the owner in every group.
- **G6** Leaving a shared group returns the player to their own group, recreating it if vanilla deleted it.

### Secondary

- **G7** Leader→player invites work alongside player→leader requests (locked decision).
- **G8** The group leader is told, non-blockingly, when someone joins bringing recruits.
- **G9** A join the server rejects (full, faction mismatch, group gone) surfaces a message rather than failing silently.
- **G10** The "which recruits move" mapping is a **pure helper** pinned by Logic-tier tests.
- **G11** The ~96-AI-in-one-slave-group scenario is **measured** and the result recorded, not assumed.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
CLIENT                                      SERVER (authority)
------                                      ------------------

modded SCR_GroupSubMenuBase                 OVT_SpawnLogic.CreateAndJoinGroup
  override JoinSelectedGroup()                -> OVT_PlayerGroupManagerComponent.EnsureOwnGroup()
  override AcceptInvite()                          creates group, SetPrivate(true),
    | confirmation dialog                          SetPrivacyChangeable(false), joins owner
    | (recruit count read locally from
    |  the JIP'd recruit table)             modded SCR_PlayerControllerGroupComponent
    v                                         override RequestJoinGroup()  <-- BUG-088-family fix
  vanilla client call, unchanged:                if server: call RPC_AskJoinGroup directly
    PlayerRequestToJoinPrivateGroup()
    RequestJoinGroup()                      modded SCR_GroupsManagerComponent
                                              override MovePlayerToGroup() <-- full-group hole fix
                                                reject BEFORE emptying the old group
   ^  notification (text)                   
   |                                        OVT_PlayerGroupManagerComponent  (NEW manager)
   +----------------------------------------  SCR_AIGroup.GetOnPlayerAdded()   -> recruits follow in
                                              SCR_AIGroup.GetOnPlayerRemoved() -> recruits follow out
                                                                               -> EnsureOwnGroup (deferred 1 frame)
                                              OVT_GroupRecruitTransfer  (NEW pure helper, Logic-tier)

                                            OVT_RecruitManagerComponent (EDITED)
                                              :1737 leader guard -> membership guard
                                              :1963 leader guard -> membership guard + retry cap
                                              NEW: MoveRecruitsToGroup() / RemoveRecruitsFromGroup()
```

**No replicated Overthrow state is added.** All group state already lives on vanilla's replicated `SCR_AIGroup` / `SCR_PlayerControllerGroupComponent`; all recruit state already lives on the JIP'd `OVT_RecruitManagerComponent` table. The new manager is a stateless server-side reactor.

**Nothing is persisted.** Group membership is deliberately **not** saved across sessions (locked decision — see D2). Nothing in `Configs/Systems/Persistence/Overthrow.conf` changes.

### 3.2 The new manager

`OVT_PlayerGroupManagerComponent` — a Manager (singleton on `Prefabs/GameMode/OVT_OverthrowGameMode.et`, `s_Instance` + `GetInstance()`, following `OVT_RecruitManagerComponent`'s shape).

It qualifies as a Manager, not a Controller: system-wide, server-only, no per-entity instances, coordinates two other managers. Responsibilities:

| Responsibility | Trigger |
|---|---|
| `EnsureOwnGroup(int playerId)` — idempotent "this player has their own private group, and is in it" | called by `OVT_SpawnLogic` on spawn, and by the reactor on removal |
| Recruits follow owner **in** | `SCR_AIGroup.GetOnPlayerAdded()` |
| Recruits follow owner **out** | `SCR_AIGroup.GetOnPlayerRemoved()` |
| Return-to-own-group | `GetOnPlayerRemoved()`, deferred one frame |
| Leader notification ("X joined with N recruits") | `GetOnPlayerAdded()` |
| AI-density warning | `GetOnPlayerAdded()` |

`OVT_Global` gets **no new accessor** — the locator half is frozen and nothing outside the manager needs it. Reach it with `OVT_PlayerGroupManagerComponent.GetInstance()`.

### 3.3 The one-frame deferral — the central design trick

Vanilla `MovePlayerToGroup` (`SCR_GroupsManagerComponent.c:109-134`) calls `previousGroup.RemovePlayer()` **then** `newGroup.AddPlayer()` in the same frame, so a normal group switch fires `Removed(old)` before `Added(new)`. Reacting to `Removed` immediately would wrongly recreate a group for a player who is mid-move.

**Design:** on `GetOnPlayerRemoved(group, playerID)`, do the recruit-pull-out immediately (the ex-group's slave group still exists this frame — vanilla's `DeleteGroupDelayed` queues to next frame via `GetGame().GetCallqueue().Call(DeleteGroups)`, `SCR_GroupsManagerComponent.c:721-730`), then `CallLater(RestoreOwnGroupDeferred, 0, false, playerID)` and **re-check `GetGroupID()` there**. By that point the add has happened, so:

- normal switch → `GetGroupID()` is the new group → no-op,
- genuine leave → `-1` → `EnsureOwnGroup`,
- **rejected join (full group)** → `-1` → `EnsureOwnGroup` — the safety net for G1,
- disconnect → no player controller → no-op.

One mechanism covers four edge cases. This is the load-bearing part of the design and the first thing to sanity-check in review.

### 3.4 Client-side seam — verified single point

- `SCR_GroupSubMenuBase.JoinSelectedGroup()` (`SCR_GroupSubMenuBase.c:259`) is the **only** join route. It branches on `group.IsPrivate()`: private → `PlayerRequestToJoinPrivateGroup(...)` + a `GROUPS_REQUEST_SENT` notification; public → `RequestJoinGroup(...)`. All Overthrow player groups are private, so the private branch is the live one. *(The requirements doc describes this seam slightly wrong — it says everything funnels through `RequestJoinGroup`. The private branch is what Overthrow actually takes.)*
- `SCR_GroupSubMenuBase.AcceptInvite()` (`:285`) is the **only** invite-accept route.
- **Neither subclass overrides either method.** `SCR_GroupSubMenuPlayerlist` (the tab Overthrow players actually reach, via the player list / pause menu) overrides only `OnMenuUpdate`, `OnTabCreate`, `OnTabShow`, `OnTabHide`, `UpdateGroups`; `SCR_GroupSubMenu` (deploy) overrides only `OnTabCreate`, `OnTabShow`, `OnTabHide`. → **one `modded class SCR_GroupSubMenuBase` covers every route.**
- Both are wired through `SCR_InputButtonComponent.m_OnActivated` (`:359`, `:379`), i.e. `WLib_NavigationButton`s — already gamepad-navigable.
- `SCR_GroupTileButton.OnClick` only **selects** a group and drives `SetJoinGroupButton` text; it does not join. No second seam.
- `SCR_GroupInviteStickyNotificationUIComponent` is display-only (it listens to `GetOnInviteAccepted()` purely to remove itself). Not a seam.

---

## 3.5 Phase 1 corrections to this plan (2026-08-06)

Phase 1's verdict (full text in `context.md`) re-read every vanilla line cited below and corrected seven things. **These override the corresponding text later in this document**, which is left as written so the reasoning trail survives:

1. **`SetName()` is the wrong call — and F1 is false even in solo today.** `OVT_SpawnLogic.c:878` calls the engine *entity-name* setter; `SCR_AIGroup` has no such method and no UI reads it. Group tiles draw the callsign plus `GetCustomName()` (`SCR_GroupHelperUI.c:9-24`). Use **`SetCustomName(playerName, playerId)`** (`SCR_AIGroup.c:744-747`, applies locally *and* broadcasts). Affects T2.1 and T3.2.
2. **The faction-mismatch hypothesis (§ Phase 1's prime suspect) is false.** `SetCivilianFaction` is a misnomer: it assigns `m_sPlayerFaction`, whose default is **FIA** (`OVT_OverthrowConfigComponent.c:75`), and `CreateAndJoinGroup` builds the group from that same `Faction` instance (`OVT_SpawnLogic.c:863,870`). BUG-088's server log agrees (`Group faction: FIA`).
3. **The `PlayableGroup.et` suspect is eliminated.** Same-GUID addon files are **deltas merged onto the base resource**, not replacements, so `RplComponent` and `m_bPlayable 1` survive. (`docs/features/core/persistence/tasks.md:59,176` states the opposite and is wrong — noted there for that feature's owner, not fixed here.)
4. **D3's line is `:560`, not `:562`** (`:562` is `RemoveRequester`), and `playerComponent` is **not null-checked** at `:558-560`. A disconnect between request and approval null-derefs the server. Add the guard in T2.2.
5. **T2.1 and T2.2 must ship together.** Groups are public today, so `JoinSelectedGroup` takes the working *public* branch. `SetPrivate(true)` is what routes it onto the private branch that dies at `:560` — landing T2.1 alone would introduce the broken join rather than fix it.
6. **T2.1's open question is answered:** `IsPrivacyChangeable() == false` degrades cleanly (`SCR_GroupTileButton.c:1220-1228` hides and disables the toggle) but **also removes the rename button**. Accepted — groups are named after their owner by design.
7. **Expect a stray empty FIA group.** `SCR_Faction.m_bEnableAutoGroupCreationWhenFull` defaults to 1 (`:75`), so `OnPlayerFactionChanged` (`SCR_GroupsManagerComponent.c:1112-1114`) creates one before Overthrow does. Relevant to D11 (it is the empty group `TryFindEmptyGroup` keeps finding) and to R5's "no extra group created" check.

**Phase 5's seam did not move** — `JoinSelectedGroup()` (`:259`) and `AcceptInvite()` (`:285`) were re-verified as the only join routes, neither subclass overrides them, and Overthrow has no modded class or layout touching the group stack. No Phase 5 amendment beyond reusing vanilla's `CanPlayerJoinGroup` (`SCR_PlayerControllerGroupComponent.c:136-245`) for T5.1's client-side pre-check.

---

## 4. Implementation Phases

Estimates assume one agent per phase plus a user play-test round. **Sequence is deliberate: server-side mechanics land and are play-testable before the UX layer**, because the UX layer is untestable by the automated spine and the mechanics are what actually break.

---

### Phase 1 — Diagnose the vanilla Group Menu (measurement, not code)

**Agent:** `ui-developer` — throwaway instrumentation only; the user runs the session and reports.
**Estimate:** 2-4 h + one play-test round.
**Why first:** the whole UX layer is built on this menu. If the fault turns out to be in `UpdateGroups` or tile creation, that may change which seam the dialog hooks into.

**Already answered — do not chase it.** "Create new group is greyed out" is **vanilla by design, not a bug.** `SCR_GroupsManagerComponent.CanCreateNewGroup()` (`:1319`) returns false when `playerGroup.GetPlayerCount() == 1` ("disable creation of new group if player is the last in his group", `:1344`) **and** when `TryFindEmptyGroup()` finds any empty faction group (`:1347`). Overthrow's default state — every player alone in their own group — permanently satisfies the first condition. That is **correct** behaviour for Overthrow's model: you already have a group of your own, there is nothing to create. Record this in the verdict and leave `CanCreateNewGroup` alone.

**Tasks**

1. **T1.1** Add temporary `Print()` instrumentation (clearly tagged, e.g. `[OVT-GRPDIAG]`, all removed before the phase closes) at these exact points, and log on **both** client and server so the two can be compared:
   - `SCR_GroupSubMenuBase.UpdateGroups()` — the local player faction it resolves, and `groupManager.GetSortedPlayableGroupsByFaction(faction).Count()`.
   - Tile creation inside `UpdateGroups` — how many `SCR_GroupTileButton`s are actually created and inserted into `GroupList`.
   - `SCR_GroupsManagerComponent.GetSortedPlayableGroupsByFaction()` (`:503`) — input faction key, output count.
   - `SetSelectedGroupButtonStatus()` (`:331`) — `selectedGroupId` and the resulting `canShow`, since `m_JoinGroupButton.SetVisible(canShow)` is what makes Join appear at all.
   - `SCR_GroupSubMenuPlayerlist.UpdateGroups()` override (`:46`) — confirm it runs and what it does differently.
2. **T1.2** On the server, log `SCR_GroupsManagerComponent`'s full playable-group table at the moment the menu opens: group id, faction key, player count, leader id, `IsPrivate()`. Compare against what the client logged.
3. **T1.3** Two-client session (dedicated server preferred, since that is where BUG-088 lived). Both players open the Group tab from the player list. Capture both clients' logs and the server log.
4. **T1.4** Write the verdict into `docs/features/core/player-groups/context.md` (create it): **which specific step fails, on which side.** Name the file and line. If the menu turns out to already work now that the faction map replicates (BUG-088 fix), say so — that is a valid verdict and Phase 5 proceeds unchanged.

**Acceptance criteria**

- A written verdict exists in `context.md` naming the faulting step and side (client/server), or stating "no fault — the menu works post-BUG-088" with the log evidence that proves it.
- The verdict explicitly records that greyed-out **Create new group** is vanilla-by-design and out of scope.
- All `[OVT-GRPDIAG]` instrumentation is removed; `tools/compile-check.sh` exits 0.
- If the verdict changes the seam, Phase 5's task list is amended in this document before Phase 5 starts.

---

### Phase 2 — Private-by-default groups + the two server-authority fixes

**Agent:** `network-specialist` — server-authority and RPC-direction work, low file count but high blast radius.
**Estimate:** 4-6 h.

**Tasks**

1. **T2.1 Private flag at creation.** In `OVT_SpawnLogic.CreateAndJoinGroup` (`Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c:816`), right after `newGroup.SetName(playerName)` (`:878`), call:
   - `newGroup.SetPrivate(true)` — **verified as the replicating server entry point**: `SCR_AIGroup.SetPrivate` (`SCR_AIGroup.c:535`) calls `RPC_SetPrivate(isPrivate)` locally *and* `Rpc(RPC_SetPrivate, ...)` as a `RplRcver.Broadcast`. `SCR_GroupsManagerComponent.SetPrivateGroup(groupID, ...)` (`:345`) is only a find-then-call wrapper around the same thing, so `SetPrivate` alone is sufficient.
   - `newGroup.SetPrivacyChangeable(false)` — also broadcasts (`SCR_AIGroup.c:548`). This does two jobs: it stops a player flipping their own group public from the vanilla attributes UI, and it blocks vanilla's **auto-unlock** at `SCR_GroupsManagerComponent.c:748` and `:759`, which calls `group.SetPrivate(false)` when a group empties or is the faction's last group. Without this, an emptied Overthrow group can silently become public.
   - **Verify** in the play-test that the vanilla group-attributes UI degrades gracefully with `IsPrivacyChangeable()` false (expected: the privacy toggle is hidden/disabled). If it errors or looks broken, drop `SetPrivacyChangeable(false)` and instead re-assert `SetPrivate(true)` from the reactor's `GetOnPlayerRemoved` handler; record the choice in `context.md`.
2. **T2.2 Fix the BUG-088-family hole in the leader-approve path.** New file `Scripts/Game/Modded/SCR_PlayerControllerGroupComponent.c` (match the repo's existing modded-class location convention — check where `modded class SCR_PlayerController` lives and put it alongside):
   ```
   modded class SCR_PlayerControllerGroupComponent
   {
       override void RequestJoinGroup(int groupID)
       {
           if (Replication.IsServer())
               RPC_AskJoinGroup(groupID);
           else
               Rpc(RPC_AskJoinGroup, groupID);
       }
   }
   ```
   **Why:** `RPC_ConfirmJoinPrivateGroup` (`SCR_PlayerControllerGroupComponent.c:544`) is `RplRcver.Server` — it *runs on the server* — and at `:562` it calls `playerComponent.RequestJoinGroup(...)`, which is the client wrapper (`:830`: `Rpc(RPC_AskJoinGroup, groupID)`, `RplRcver.Server`). That is precisely the BUG-088 / BUG-045 / BUG-052 family: a `RplRcver.Server` RPC marshalled by the authority goes nowhere. **Predicted symptom: the leader approves a join request and nothing happens on a dedicated server.** Overriding the non-RPC wrapper fixes every server-side caller at once — the approve path and `RejoinPlayer` (`SCR_GroupsManagerComponent.c:1143`) — and is a no-op change on clients. This is the same shape already inlined at `OVT_SpawnLogic.c:899-902`; leave that inline branch in place (it documents the hazard and is now redundant-but-correct).
3. **T2.3 Fix the full-group hole.** New file `Scripts/Game/Modded/SCR_GroupsManagerComponent.c`:
   ```
   modded class SCR_GroupsManagerComponent
   {
       override int MovePlayerToGroup(int playerID, int previousGroupID, int newGroupID)
       {
           SCR_AIGroup newGroup = FindGroup(newGroupID);
           if (!newGroup || newGroup.IsFull())
               return previousGroupID;   // reject WITHOUT emptying the player's current group
           return super.MovePlayerToGroup(playerID, previousGroupID, newGroupID);
       }
   }
   ```
   **Why:** vanilla `MovePlayerToGroup` (`:109-134`) calls `previousGroup.RemovePlayer(playerID)` at `:114` **before** testing `newGroup.IsFull()` at `:119`, then returns `-1`. `RPC_AskJoinGroup` (`:889-895`) then sets `m_iGroupID = -1` and broadcasts it. **A failed join to a full group strands the player with no group at all** — the exact state G1 forbids. Returning `previousGroupID` makes `groupIDAfter == m_iGroupID`, so nothing changes and the player stays put. `IsFull()` = `IsMaxMembersLimited() && m_iMaxMembers <= m_aPlayerIDs.Count()` (`SCR_AIGroup.c:614-616`); `m_iMaxMembers` is **6** on `Prefabs/Groups/PlayableGroup.et` and counts **players only** — slave-group AI does not consume slots.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) exits 0 — no regression.
- Two-client dedicated-server play-test: Player B sees Player A's group in the Group tab marked **private** (padlock icon), presses Join, A gets the request, A approves, **B is in A's group on both clients and on the server**. (This is the T2.2 fix proving itself; before T2.2 this is predicted to do nothing.)
- Solo play-test: player still spawns into their own group, is its leader, AI commanding still opens, recruits still join and follow orders.
- Filling a group to 6 and having a 7th request → the 7th player **remains in their own group**, is never groupless.

---

### Phase 3 — `OVT_PlayerGroupManagerComponent`: the own-group guarantee

**Agent:** `network-specialist-advanced` — **advanced.** This phase creates a new manager, subscribes to global static invokers whose ordering interacts with vanilla's group-deletion queue, and refactors the load-bearing spawn path. Several integration points, and a mistake here reproduces BUG-088.
**Estimate:** 8-12 h.
**Deliberately excludes recruits** so the group lifecycle can be play-tested on its own.

**Tasks**

1. **T3.1** Create `Scripts/Game/GameMode/Managers/OVT_PlayerGroupManagerComponent.c` following the Manager pattern (`s_Instance` set in `OnPostInit`, `GetInstance()`, `SCR_Global.IsEditMode()` early-out — copy the shape from `OVT_RecruitManagerComponent.c:82-122`). Register it on `Prefabs/GameMode/OVT_OverthrowGameMode.et`. **No `OVT_Global` accessor** (locator half is frozen; nothing external needs it).
2. **T3.2** Move the "create this player's own private group and put them in it" body out of `OVT_SpawnLogic.CreateAndJoinGroup` into `OVT_PlayerGroupManagerComponent.EnsureOwnGroup(int playerId)`, returning the group id or `-1`. Requirements:
   - **Idempotent** — no-op returning the current id when `groupController.GetGroupID() != -1`.
   - Keeps `SetCivilianFaction`, the faction null-check with its BUG-088 error print, `SetName(playerName)`, and the T2.1 privacy calls.
   - `OVT_SpawnLogic.CreateAndJoinGroup` becomes a thin caller that keeps its existing retry ladder (`CreateAndJoinGroupDelayed`, 500 ms, max 10) **and keeps firing `m_OnPlayerGroupCreated.Invoke(playerId, groupID, playerName)` from the same place it does today** — `OVT_RecruitManagerComponent.OnPlayerGroupCreated` subscribes to it (via `OVT_RespawnSystemComponent.GetOnPlayerGroupCreated()`) and drives recruit respawn. **Do not fire that invoker from the reactor path** (a group switch must not trigger a full recruit respawn; the reactor moves live recruits directly in Phase 4).
3. **T3.3** Subscribe in `OnPostInit`, server-only:
   - `SCR_AIGroup.GetOnPlayerAdded().Insert(OnGroupPlayerAdded)` — `SCR_AIGroup.c:1072`, invoked from `RPC_DoAddPlayer` (`:1280`).
   - `SCR_AIGroup.GetOnPlayerRemoved().Insert(OnGroupPlayerRemoved)` — `:1078`, invoked from `RPC_DoRemovePlayer` (`:1503`).
   Both RPCs are `RplRcver.Broadcast` and are called **locally first, then broadcast** by the server-authority entry points `AddPlayer()` (`:1401`, `:1408-1409`) and `RemovePlayer()` (`:1526`, `:1538-1539`), so the invokers fire on the server for every membership change regardless of cause. Signature: `Invoke(SCR_AIGroup group, int playerID)`. Guard the handler bodies with an authority check (`Replication.IsServer()`) — these are *static broadcast* invokers and will also fire on clients.
4. **T3.4** Implement the deferred return-to-own-group (see §3.3): `OnGroupPlayerRemoved` schedules `GetGame().GetCallqueue().CallLater(RestoreOwnGroupDeferred, 0, false, playerID)`; `RestoreOwnGroupDeferred` no-ops unless the player still has a live `PlayerController` **and** `groupController.GetGroupID() == -1`, then calls `EnsureOwnGroup(playerID)`.
5. **T3.5** Verify the interaction with vanilla's own `GetOnPlayerRemoved` subscriber, `SCR_GroupsManagerComponent.OnGroupPlayerRemoved` (`:735`, subscribed at `EOnInit` `:1724`), which calls `DeleteGroupDelayed(group)` (`:765`) for emptied groups. Deletion drains next frame (`:721-730`), so the ex-group is still readable during the removal frame. Confirm by log that Overthrow's handler sees a non-null `group.GetSlave()` on removal.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0; `tools/run-tests.sh "{6A6E2A002F53A581}"` exits 0.
- New Init-tier assertion: `OVT_PlayerGroupManagerComponent.GetInstance()` resolves after manager init (add to `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`), and it must be proven able to fail (temporarily remove the component from the prefab, observe exit 1, restore) — record the method in the case comment.
- Two-client play-test: B joins A's group; **B leaves** (vanilla Leave action) → B is immediately back in a group of their own, named after B, private, with B as leader. B can command AI again.
- Two-client play-test: B joins A's group, then **B disconnects and reconnects** → B is in B's own group (not A's), leader, commanding works.
- Full-group rejection (7th player into a 6-player group): the rejected player is never groupless; log shows the deferred restore either no-ops (T2.3 held the player in place) or restores.
- Solo unchanged: spawn → own group → commanding → recruit → orders followed.

---

### Phase 4 — Recruits follow their owner + the leader-guard fix + Logic tier

**Agent:** `component-developer-advanced` — **advanced.** Edits the 2,219-line `OVT_RecruitManagerComponent`, changes a guard that gates two separate lifecycle paths, and adds the first Logic-tier coverage for this feature.
**Estimate:** 10-14 h.

**Tasks**

1. **T4.1 The pure helper.** New file `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c` — a static-only class with **no world, no manager, no engine access**, so it is Logic-tier testable:
   ```
   class OVT_GroupRecruitTransfer
   {
       //! Which of an owner's recruits can physically move to another group's slave group.
       //! Offline recruits have no entity to move; an offline owner's recruits must not be
       //! commandable by anyone, so they transfer nothing.
       //! \param[in] ownedRecruits every record the owner holds (from GetPlayerRecruits)
       //! \param[in] ownerOnline false when the owner has no live PlayerController
       //! \param[out] outSkippedOffline how many records were skipped for being offline
       //! \return recruit ids to move, in table order
       static array<string> SelectTransferable(notnull array<ref OVT_RecruitData> ownedRecruits, bool ownerOnline, out int outSkippedOffline);

       //! Projected slave-group AI count after a transfer.
       static int ProjectSlaveAiCount(int currentSlaveAiCount, int incomingCount);

       //! budget <= 0 means "no budget configured" and always returns false.
       static bool ExceedsAiBudget(int projectedCount, int budget);
   }
   ```
   **Owner liveness must NOT come from `OVT_PlayerData.IsOffline()`** — that known player-manager bug tests `id == 0` while disconnect sets `id = -1`, so departed players report online. Resolve it at the call site from `GetGame().GetPlayerManager().GetPlayerController(playerId) != null` and pass the boolean in. Leave a `//!` comment saying why, and do not fix `IsOffline()` here (it belongs to `core/player-manager`).
2. **T4.2 Recruit move-in.** New public method on `OVT_RecruitManagerComponent`:
   `void MoveRecruitsToGroup(string ownerPersistentId, notnull SCR_AIGroup targetGroup, bool ownerOnline)`.
   For each id from `SelectTransferable`, resolve the entity with `FindRecruitEntity(recruitId)` (`:1596`) and call `groupController.AddAIToSlaveGroup(recruitEntity, targetGroup)` where `groupController` is the **owner's** `SCR_PlayerControllerGroupComponent` — the exact call already used at `:1764`. Activate the AI first (`AIControlComponent.ActivateAI()`, `:1750-1752`) — the AI must be running before it can take orders. `AddAIToSlaveGroup` (`SCR_PlayerControllerGroupComponent.c:1470`) does `slaveGroup.AddAgentFromControlledEntity()` + `groupManager.AskAddAiMemberToGroup(...)`, which broadcasts membership itself. Keep the `group.GetSlave()` null guard (`:1743-1747`).
3. **T4.3 Recruit move-out.** New public method:
   `void RemoveRecruitsFromGroup(string ownerPersistentId, notnull SCR_AIGroup exGroup)`.
   Vanilla has no public mirror of `AddAIToSlaveGroup`, so write the four-line mirror: `exGroup.GetSlave().RemoveAgentFromControlledEntity(entity)` (`SCR_AIGroup.c:1143`) then `groupsManager.AskRemoveAiMemberFromGroup(slaveRplId, characterRplId)` (`SCR_GroupsManagerComponent.c:1557`, which broadcasts `RPC_DoRemoveAIMemberFromGroup`). Model it on vanilla's own `OnAIMemberRemoved` (`:1593-1615`), which does exactly this RplId resolution.
4. **T4.4 Wire the reactor.** In `OVT_PlayerGroupManagerComponent`:
   - `OnGroupPlayerAdded(group, playerID)` → `MoveRecruitsToGroup(persistentId, group, true)`.
   - `OnGroupPlayerRemoved(group, playerID)` → `RemoveRecruitsFromGroup(persistentId, group)` **immediately** (before the group can be deleted next frame). This is what guarantees "none of their recruits may remain in the ex-group's slave group".
   - On a normal switch the sequence is remove-from-old → add-to-new in one frame; the recruits are pulled then re-placed. Confirm by log that the net result is exactly one slave-group membership.
5. **T4.5 The leader-guard fix — the concrete breakage.** Under the new model an owner inside a friend's group is **not** the leader, so both of these silently bail:
   - `OVT_RecruitManagerComponent.c:1737` — `if (group.GetLeaderID() != playerId)` in `AddRecruitToPlayerGroup`, the path used when recruiting. Change to a **membership** test (`group.IsPlayerInGroup(playerId)`). Without this, recruiting while in a shared group does nothing.
   - `OVT_RecruitManagerComponent.c:1963` — the same guard in `RespawnRecruitsDelayed`. Change to membership **and add a retry cap**: this branch currently re-arms `CallLater(RespawnRecruitsDelayed, 500, ...)` with **no retry counter** (unlike `CreateAndJoinGroupDelayed`, which caps at 10), so a player who is never the leader produces an unbounded 500 ms timer loop. Cap it at 10 like its sibling and log at `LogLevel.WARNING` on exhaustion.
   Keep the `groupId == -1` guard and the `group.GetSlave()` guard as-is.
6. **T4.6 Logic-tier coverage.** New `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GroupRecruits.c`, registered in the Fast group. Cases:
   - empty owner → empty set, `outSkippedOffline == 0`
   - all recruits online, owner online → every id, in table order
   - mixed online/offline recruits → only the online ids, `outSkippedOffline` exact
   - owner offline → empty set even when every recruit is online
   - `ProjectSlaveAiCount` / `ExceedsAiBudget` arithmetic, including `budget <= 0` meaning disabled
   **Every case must be proven able to fail once before shipping** (no `maxAttempts`) — e.g. temporarily invert the `m_bIsOnline` filter, or assert `Count() + 1`, observe exit 1, revert. **Record the method used in a preamble comment in the suite file**, matching the convention in `OVT_TEST_Logic_Skills.c`.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0; `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) and `"{6A6E2A002F53A581}"` (All) both exit 0, with the new Logic cases included and their fail-proof method recorded.
- Two-client play-test: A has 3 recruits, B joins A's group → **A's 3 recruits appear in the shared group's slave group and take orders from B (the leader)**; A's roster still shows them as A's; kill XP still credits A.
- A leaves → A's 3 recruits leave with A; B (ex-leader) can no longer command them; none remain in B's slave group.
- **A recruits a new civilian *while inside B's group*** → the new recruit joins B's group's slave group and takes B's orders (this is the T4.5 `:1737` fix; before it, this silently does nothing).
- Owner-only actions refused to the leader: B cannot dismiss, rename, open-inventory, apply a loadout to, or show-on-map any of A's recruits.

---

### Phase 5 — Group Menu UX layer + localization

**Agent:** `ui-developer-advanced` — **advanced.** Two `modded class` overrides on a vanilla menu, two new dialog presets, gamepad/console reachability, and it must not break the deploy-menu route.
**Estimate:** 8-12 h. **Amend this task list first if Phase 1's verdict moved the seam.**

**Tasks**

1. **T5.1** New file `Scripts/Game/Modded/SCR_GroupSubMenuBase.c`:
   ```
   modded class SCR_GroupSubMenuBase
   {
       override protected void JoinSelectedGroup() { /* confirm, then super */ }
       override void AcceptInvite()                { /* confirm, then super */ }
   }
   ```
   Each override: resolve the target group and its leader name, count the **local** player's recruits, open the dialog, and call `super.<method>()` **only** from the confirm handler. Cancel does nothing. Never perform the membership change locally — the vanilla call inside `super` is the only path.
   - **The confirmation must appear on the invitee side too** (locked decision): accepting an invite is the moment *they* hand command of *their* recruits to someone else.
   - **Skip the dialog when joining your own group** (`group.GetGroupID() == m_PlayerGroupController.GetGroupID()`) — vanilla already rejects that at `RPC_AskJoinGroup:876`, but do not show a scary dialog for a no-op.
   - **Client-side full-group pre-check:** if `group.IsFull()`, show a "that group is full" message and do not send the request. The Phase 2 T2.3 server fix is the guarantee; this is the message (G9).
2. **T5.2** Two new presets in `Configs/UI/Dialogs/DialogPresets_Campaign.conf`, copying the exact shape of `DELETE_CAMP` / `DISMISS_RECRUIT` — each with a `confirm` button (`m_sActionName "DialogConfirm"`) and a `cancel` button (`m_sActionName "MenuBack"`); **that action wiring is what makes them gamepad-usable**:
   - `JOIN_GROUP_CONFIRM`
   - `JOIN_GROUP_CONFIRM_RECRUITS` (used only when the local player has ≥1 recruit)
   Opened with the established pattern `SCR_ConfigurableDialogUi.CreateFromPreset("{GUID}Configs/UI/Dialogs/DialogPresets_Campaign.conf", "TAG")` — see `OVT_CampMenuContext.c:76` and `OVT_RecruitsContext.c:371,403`. Substitute the leader name and recruit count into the message after creation, the same way those call sites do.
   Message content (per requirements):
   - always: *"\<Leader\> will be able to command your recruits while you are in their group."*
   - with recruits: *"Your N recruits will move to \<Leader\>'s group and take orders from \<Leader\>. You still own them — they return to you when you leave the group, change groups, or reconnect."*
3. **T5.3** Explanatory text in the Group tab stating the Overthrow model (you start in your own group; joining another player's group hands command of your recruits to that group's leader for as long as you are in it). Prefer adding a text widget from the modded `OnTabCreate` over forking a vanilla `.layout`; if a layout fork is unavoidable, follow the GUID rules in the `overthrow-ui-patterns` skill.
4. **T5.4 Leader notification** (G8, non-blocking, not a dialog). Server-side in `OVT_PlayerGroupManagerComponent.OnGroupPlayerAdded`: when the joining player brings ≥1 recruit, `OVT_Global.GetNotify().SendTextNotification(tag, group.GetLeaderID(), playerName, recruitCount.ToString())`. Use `OVT_NotificationManagerComponent.SendTextNotification` (`:106`) — do **not** try to add an `ENotification` enum value.
5. **T5.5 Localization.** All new user-facing strings are `#OVT-` keys added to **`Language/localization_Overthrow.st`** (the editable master). **Never touch `Language/localization_Overthrow.<lang>.conf`** — those are Workbench-generated runtime exports and hand-editing corrupts them silently. The user regenerates exports in Workbench; until then any layout referencing a not-yet-exported key must use literal text. Keys needed: dialog titles/messages ×2, the group-tab explanatory paragraph, the group-full message, and the leader notification.
6. **T5.6 Gamepad/console pass.** Verify on a controller: the Group tab is reachable, the Join and Accept buttons are focusable, and the dialog's confirm/cancel respond to `DialogConfirm` / `MenuBack`. No mouse-only affordance is introduced. (Mouse-wired `m_OnClicked` buttons are fine — arrow/enter context bindings cover gamepads.)

**Acceptance criteria**

- `tools/compile-check.sh` exits 0; All group exits 0.
- Two-client: B selects A's group, presses Join → **a dialog appears naming A and stating A will command B's recruits**, with B's exact recruit count when B has any. Cancel sends nothing (verify: A receives no request). Confirm sends the request and A sees it.
- Same dialog on the invite path: A invites B, B accepts → the dialog appears **before** the join is requested.
- A (the leader) receives a text notification "\<B\> joined with N recruits" when B joins with recruits, and no notification when B has none.
- The Group tab shows the explanatory text.
- Joining a full group shows the group-full message and sends nothing.
- Every new string is an `#OVT-` key in `localization_Overthrow.st`; no hardcoded English survives except where an export does not yet exist (and that is listed in `context.md` for the user to regenerate).
- Full gamepad walkthrough of join → dialog → confirm, and join → dialog → cancel.

---

### Phase 6 — Edge cases, AI-density measurement, hardening

**Agent:** `network-specialist` (with a user play-test round for the measurements).
**Estimate:** 6-8 h.

Every edge case from the requirements, with its answer:

| Edge case | Design answer | Task |
|---|---|---|
| **Leader disconnects / is promoted away.** `CheckForLeader` (`SCR_AIGroup.c:1286`) promotes `m_aPlayerIDs[0]`. | Recruits stay with their owner and remain in the group; the new leader commands them. Nothing to build — no Overthrow code keys off leader identity for the slave group after T4.5. | **T6.1** verify by play-test only |
| **~96 AI in one slave group.** 6 players × 16 recruits. | **Measure, don't guess.** Add `MAX_SLAVE_AI_PER_GROUP` on the manager, **default 0 = disabled**, and warn (notification to joiner + leader) when `ExceedsAiBudget` trips. Do not refuse the join and do not silently drop recruits — that orphans AI. | **T6.2** measure with 2 players × 16, log server frame time and slave-group agent count, extrapolate, record the number in `context.md`, then set the budget only if the measurement justifies it |
| **Offline owner's recruits must not be commandable.** | `SelectTransferable` returns nothing when `ownerOnline` is false, and `OnGroupPlayerRemoved` pulls them out of the ex-group's slave group immediately. Liveness comes from `PlayerManager.GetPlayerController(playerId)`, **not** the buggy `OVT_PlayerData.IsOffline()`. Bodies then follow the existing BUG-086 offline flow (reserved/despawned after ~10 min). | **T6.3** |
| **Group emptied / deleted while a member is mid-join.** | `JoinSelectedGroup` re-resolves the group via `groupManager.FindGroup(...)` and returns on null (vanilla, `:262-265`); server-side `RPC_AskJoinGroup` → `MovePlayerToGroup` returns `previousGroupID` on a null group after T2.3, so the requester is never stranded. Add the user-facing "that group no longer exists" message. | **T6.4** |
| **Join, recruit, leave in quick succession (agents mid-move).** | The reactor is synchronous within the membership frame; the only async step is the one-frame restore deferral, which re-checks `GetGroupID()`. Add a guard in `MoveRecruitsToGroup`/`RemoveRecruitsFromGroup` skipping recruits whose `FindRecruitEntity` returns null, and log at `LogLevel.WARNING` rather than dereferencing. | **T6.5** |
| **Server-rejected join surfaces a message.** | Client pre-check (T5.1) covers full/gone. Add the missing server-side case: `RPC_ConfirmJoinPrivateGroup` does **not** check `IsFull()`, so a leader can approve into a full group; after T2.3 that is a silent no-op. Send the requester a "group is full" text notification from the reactor when a confirm produces no membership change. | **T6.6** |
| **`FindRecruitEntity` mid-iteration removal** (`OVT_RecruitManagerComponent.c:1587`, a known pre-existing hazard). | The new transfer methods call it in a loop, which raises the exposure. Do **not** fix it here (it belongs to `resistance/recruits`), but snapshot the id list before iterating so this feature does not amplify it. | **T6.7** |

**Acceptance criteria**

- Every row above has a recorded play-test observation in `context.md`.
- The AI-density number is written down with the conditions it was measured under, and the budget decision (set / left at 0) is justified by it.
- `tools/compile-check.sh` exits 0; All group exits 0.
- No `Print(...)` diagnostic left at a level above `LogLevel.NORMAL` in a hot path.

---

## 5. Key Technical Decisions

**D1 — No new client→server RPC, and therefore no new controller component.**
The join itself already has a correct client→server path in vanilla (`PlayerRequestToJoinPrivateGroup`, `RequestJoinGroup`); the UX layer only defers the *existing* client call behind a dialog. The dialog's recruit count is readable client-side from the JIP'd recruit table (`OVT_RecruitManagerComponent` ships the whole roster via `RplSave`/`RplLoad` and keeps it live with broadcast RPCs). The leader notification is server→client text. So nothing new crosses the wire from a client. YAGNI applies. *Contingency:* if the approving leader's client cannot resolve the requester's persistent id (and therefore their recruit count), do **not** add a query RPC — have the server include the count in the request notification instead.
*(And under no circumstances add anything to `OVT_PlayerCommsComponent` — it is frozen/deprecated. If a controller component ever does become necessary, it goes on `OVT_OverthrowController` and must be reachable via `controller-migration`'s planned `OVT_ControllerComponent<Class T>.Get()`, with **no** new `OVT_Global` getter.)*

**D2 — Reconnect returns you to your OWN group; group membership is not persisted.**
Locked with the user. Predictable, matches "your group is yours", and avoids vanilla's `SCR_GroupsManagerComponent.RejoinPlayer()` (`:1143`), which calls the client-request wrapper from server code — the BUG-088 failure family — and is gated behind `m_bAllowRejoinPlayerAfterReconnecting` anyway. Players re-request to join their friend. Recruits follow the owner either way. Consequence: nothing in this feature touches persistence config or serializers.

**D3 — Fix the leader-approve path by overriding the non-RPC wrapper, not the RPC.**
`RPC_ConfirmJoinPrivateGroup` (`SCR_PlayerControllerGroupComponent.c:544`, `RplRcver.Server`) calls `RequestJoinGroup` (`:830`) from server code — a `RplRcver.Server` RPC marshalled by the authority, which goes nowhere. Overriding the **wrapper** rather than the RPC handler fixes every server-side caller at once, keeps client behaviour byte-identical, and avoids the question of whether `modded class` overrides of `[RplRpc]`-attributed methods re-register cleanly. Same shape as the fix already inlined at `OVT_SpawnLogic.c:899-902`.

**D4 — Close the full-group hole in `MovePlayerToGroup` by returning `previousGroupID`, not `-1`.**
Vanilla removes the player from their old group (`:114`) before checking `IsFull()` (`:119`) and returns `-1`, which `RPC_AskJoinGroup` (`:889-895`) turns into `m_iGroupID = -1` — a live player with no group, the exact G1 violation. Returning `previousGroupID` makes the caller's `groupIDAfter != m_iGroupID` test false, so nothing changes and the player stays put. Simpler and safer than trying to undo the removal.

**D5 — `SetPrivate(true)` is enough to replicate; `SetPrivacyChangeable(false)` is added for a second reason.**
`SCR_AIGroup.SetPrivate` (`:535`) calls the handler locally *and* broadcasts, so it is the replicating server entry point; `SCR_GroupsManagerComponent.SetPrivateGroup` (`:345`) is only a lookup wrapper. `SetPrivacyChangeable(false)` is added because vanilla **auto-unlocks** groups at `SCR_GroupsManagerComponent.c:748` and `:759` (emptied group, and the faction's last group), which would silently make an Overthrow group public. Both calls are cheap and both are verifiable in one play-test.

**D6 — One shared `EnsureOwnGroup`, one shared reactor.**
Rather than duplicating group-creation logic for the "restore" case, `OVT_SpawnLogic.CreateAndJoinGroup` and the reactor both call the same idempotent `EnsureOwnGroup(playerId)`. The `m_OnPlayerGroupCreated` invoker keeps firing **only** from the spawn path, so a group *switch* does not trigger a full recruit respawn — the reactor moves live recruits directly instead.

**D7 — Defer the restore by one frame and re-check.**
See §3.3. One mechanism disambiguates a genuine leave from a mid-move removal, and simultaneously catches rejected joins and disconnects. Vanilla's own in-flight marker `m_iMovingPlayerToGroupID` (`SCR_GroupsManagerComponent.c:64`) is `protected` and not usable from a subscriber.

**D8 — Membership, not leadership, is the recruit guard.**
`OVT_RecruitManagerComponent.c:1737` and `:1963` both demand `group.GetLeaderID() == playerId`. Under the new model an owner inside a friend's group is not the leader, so recruiting-while-shared and reconnect-recruit-respawn both break. Membership is the correct predicate: the recruit belongs in whatever group its owner is in. `:1963` additionally gets a retry cap — it currently re-arms a 500 ms timer with no counter.

**D9 — Owner liveness is resolved from `PlayerManager`, not `OVT_PlayerData.IsOffline()`.**
`IsOffline()` tests `id == 0` while disconnect sets `id = -1`, so departed players report online. That bug belongs to `core/player-manager`; this feature simply does not consume it, and says so in a code comment.

**D10 — Manager, not Controller.**
System-wide, server-only, no per-entity instances, coordinates two other managers, subscribes to global static invokers. Textbook Manager. No `OVT_Global` accessor (the locator half is frozen and nothing external needs it) — `s_Instance` + `GetInstance()` like `OVT_RecruitManagerComponent`.

**D11 — Greyed-out "Create new group" is not a bug and will not be changed.**
`CanCreateNewGroup()` (`SCR_GroupsManagerComponent.c:1319`) disables creation when you are the last player in your group (`:1344`) or an empty faction group exists (`:1347`). Overthrow's steady state satisfies the first permanently. That is *correct*: you already have your own group. Documented so Phase 1 does not chase it.

**Out of scope, honoured explicitly:** no recruit ownership transfer/gifting; no squad roles or group loadout presets; no group flags/callsign customisation; no group chat, radio-frequency or map-marker rework; no garrison/officer command structures; no AI-only or occupying-faction group handling.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1** A player who spawns is in a group named after themselves, is its leader, and the group shows as **private** (padlock) in the Group tab to other players.
- **F2** Player B selects Player A's group and presses Join → **a dialog appears naming Player A** and stating that Player A will be able to command B's recruits while B is in A's group. If B owns recruits, the dialog also names the exact count N and states the recruits return to B on leave/switch/reconnect. **Nothing is sent until B confirms**; on cancel, Player A receives no request.
- **F3** Player A (leader) approves B's request → **B is in A's group on both clients and on the server.** (Fails before the D3 fix on a dedicated server.)
- **F4** Player A invites Player B; B accepts → **the same confirmation dialog appears on B's side first**, and only on confirm does B join.
- **F5** When B joins A's group carrying N ≥ 1 recruits, A receives a **non-blocking text notification** naming B and N. No notification when N = 0.
- **F6** All of B's online recruits are in the shared group's slave group and **take orders from A** (the leader).
- **F7** B's roster still lists those recruits as B's. **A cannot** dismiss, rename, open the inventory of, apply a loadout to, or show-on-map any of B's recruits. Kill XP from B's recruits credits **B**.
- **F8** B recruits a new civilian **while in A's group** → the new recruit joins A's group's slave group and follows A's orders. (Fails before the D8 fix.)
- **F9** B leaves the group → all of B's recruits leave with B, **none remain in A's slave group**, A can no longer command them, and B is immediately back in a private group of their own with B as leader.
- **F10** B disconnects and reconnects → **B is in B's own group**, not A's; B is leader; B's recruits are with B; commanding works.
- **F11** A (the leader) disconnects → the group promotes the next player, that new leader commands the remaining recruits, and every recruit is still owned by its original owner.
- **F12** A 7th player attempts to join a 6-player group → they see a "group is full" message and **remain in their own group**. At no point does any live player have no group.
- **F13** Solo play is unchanged end to end: spawn → own group → AI commanding opens → recruit a civilian → the recruit follows orders.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0**.
- **Q2** `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) exits **0** and `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) exits **0**, including the new Logic-tier `OVT_TEST_Logic_GroupRecruits` cases and the new Init-tier manager assertion.
- **Q3** Every new test case has a recorded proof that it can fail (the exact edit used, in a preamble comment). No `maxAttempts` anywhere.
- **Q4** No silent failure: every rejected or impossible join produces a user-visible message, and every server-side bail logs at `LogLevel.WARNING` or above with the reason.
- **Q5** No user-facing string is hardcoded English where an `#OVT-` key belongs. All new keys are in `Language/localization_Overthrow.st`. **No `localization_Overthrow.<lang>.conf` file is modified** (`git diff --stat` on `Language/` shows only the `.st`).
- **Q6** No new client→server RPC on `OVT_PlayerCommsComponent`. No new getter on `OVT_Global`.
- **Q7** No client-side membership change anywhere: every path into `AddPlayer` / `RemovePlayer` / `MovePlayerToGroup` originates on the server.
- **Q8** No orphaned AI: after any join/leave/disconnect sequence, every live recruit is in exactly one slave group and that group's master contains its owner.
- **Q9** The dialog is fully operable on a gamepad — reachable, confirmable, cancellable — with no mouse-only affordance.
- **Q10** All `[OVT-GRPDIAG]` Phase-1 instrumentation is gone.

### Integration Criteria

- **I1 `resistance/recruits`:** `OVT_RecruitData.m_sOwnerPersistentId` and `OVT_PlayerOwnerComponent` are **byte-identical before and after** any group operation — verify by grep that no new code assigns either. Recruit placement goes exclusively through `SCR_PlayerControllerGroupComponent.AddAIToSlaveGroup` (the owner's controller, the target master group), as at `OVT_RecruitManagerComponent.c:1764`.
- **I2 `core/game-mode`:** `OVT_SpawnLogic.CreateAndJoinGroup` still fires `m_OnPlayerGroupCreated` from the spawn path (the recruit manager subscribes to it), still retries via `CreateAndJoinGroupDelayed`, and still calls vanilla's **server-side** entry points only.
- **I3 Controller seam:** no controller component was added. If one was, it is on `OVT_OverthrowController`, reachable without an `OVT_Global` getter, and the deviation is justified in `context.md`.
- **I4 Persistence:** `Configs/Systems/Persistence/Overthrow.conf` and every serializer under `Scripts/Game/Persistence/` are unchanged — group membership is deliberately not persisted.

### Verification Method

**Automated (run these first, from the repo root):**

1. `tools/compile-check.sh` → expect exit 0, no `file:line: message` output.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → expect exit 0.
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → expect exit 0.
4. `git diff --stat Language/` → expect only `localization_Overthrow.st`.
5. `git diff Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` → expect empty.

**Manual, two clients on a dedicated server** (the only way — JIP/MP is outside the automated spine). Player A hosts the recruits; Player B is the joiner. Run in order:

1. Both connect and spawn. **Expect:** each is in a group named after themselves, leader, AI commanding opens. → F1, F13
2. A recruits 3 civilians. **Expect:** all 3 join A's group and follow A's orders.
3. B opens the Group tab (player list → Groups). **Expect:** A's group is listed and marked private; the Overthrow explanatory text is visible; "Create new group" is greyed out (expected, D11).
4. B selects A's group, presses Join. **Expect:** dialog naming A. B presses Cancel. **Expect:** A receives nothing. → F2
5. B presses Join again, confirms. **Expect:** A receives a join request. A approves. **Expect:** B is in A's group on both screens. → F2, F3
6. **Expect:** A receives no "joined with recruits" notification (B has none). B now recruits 2 civilians while in A's group. **Expect:** both join and follow **A's** orders. → F8
7. A orders B's 2 recruits to move. **Expect:** they obey. → F6
8. A opens the recruit roster. **Expect:** B's recruits are not dismissable/renameable/inventory-openable by A. → F7
9. B leaves the group. **Expect:** B's 2 recruits leave with B; A cannot command them; B is in B's own private group as leader. → F9
10. B rejoins A's group (repeat 5), then disconnects and reconnects. **Expect:** B is in **B's own** group, leader, with B's recruits. → F10
11. B rejoins A's group; **A** disconnects. **Expect:** B is promoted to leader, still commands the remaining recruits, ownership unchanged. → F11
12. A invites B via the player list. **Expect:** B sees the invite, accepts, **the confirmation dialog appears before the join**. → F4
13. With B carrying recruits, B joins A's group. **Expect:** A receives "B joined with N recruits". → F5
14. Fill a group to 6 players and have a 7th request. **Expect:** a "group is full" message; the 7th player is still in their own group; the server log shows no `groupId == -1` for a live player. → F12
15. **AI-density measurement:** two players each at the 16-recruit cap in one group. Record server frame time and the slave group's agent count; write both into `context.md`. → G11
16. Repeat the whole gamepad-only: navigate to the Group tab, select, Join, confirm, and cancel. → Q9

---

## 7. Testing Strategy

**Automated coverage is a spine, not the surface.** The existing 42 assertions span Logic (pure maths, world-free), Init (managers resolve), Campaign (started-campaign state) and Persistence (same-session + save/re-apply round trips). **JIP/multiplayer, UI, performance and AI movement are not covered** — and this feature is mostly those. Plan accordingly.

### Logic tier — the only meaningfully automatable part

`Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GroupRecruits.c`, added to the Fast group, pinning `OVT_GroupRecruitTransfer` (see T4.1 for the signature and T4.6 for the cases). The helper is deliberately world-free and manager-free so the tier can reach it. It covers the offline-recruit exclusion, the offline-owner exclusion, ordering, and the cap arithmetic.

Every case must be **proven able to fail once** before shipping, with the method recorded in a preamble comment (matching `OVT_TEST_Logic_Skills.c`). No `maxAttempts` — a test that needs retries is a bug in the test.

### Init tier

One assertion that `OVT_PlayerGroupManagerComponent.GetInstance()` resolves after manager init, added to `OVT_TEST_InitSuite.c`. Fail-proof method: temporarily remove the component from `Prefabs/GameMode/OVT_OverthrowGameMode.et`, observe exit 1, restore.

### Not automatable, and why

| Area | Why manual |
|---|---|
| Join / approve / invite / accept | Needs two client processes; the whole point is cross-client state |
| The confirmation dialog | UI is uncovered by the spine |
| Recruit follow / leader commanding | Needs a live AI world and a second player issuing orders |
| Disconnect / reconnect | Needs a real session teardown |
| AI density | A performance measurement, not an assertion |
| The restart/continue path | `SaveGameManager.Load`'s world transition restarts the autotest harness |

### Manual procedure

The numbered two-client steps in §6 Verification Method **are** the manual test procedure. Run them on a dedicated server (that is where BUG-088 lived; solo hides this entire class of bug), and re-run steps 1-2 and 13 in solo to confirm no single-player regression.

**Play-test gates by phase:** Phase 2 → steps 1, 5, 14. Phase 3 → steps 1, 9, 10, 14. Phase 4 → steps 2, 6, 7, 8, 9. Phase 5 → steps 3, 4, 12, 13, 16. Phase 6 → steps 11, 15 plus the edge-case table.

---

## 8. Dependencies

**Hard preconditions (all satisfied):**

- **BUG-088 fix** — faction, group and leadership now replicate on a dedicated server (verified 2026-08-06). Without it none of this is testable.
- `resistance/recruits` — `OVT_RecruitManagerComponent` ownership records, `GetPlayerRecruits` (`:178`), `FindRecruitEntity` (`:1596`), `AddRecruitToPlayerGroup` (`:1682`).
- `core/game-mode` — `OVT_SpawnLogic.CreateAndJoinGroup` (`:816`) and `OVT_RespawnSystemComponent.GetOnPlayerGroupCreated()`.

**Vanilla APIs this plan builds on** (re-check cheaply at these lines; tree at `/mnt/n/Projects/Arma 4/ArmaReforger`):

| API | File:line | Used for |
|---|---|---|
| `SCR_GroupSubMenuBase.JoinSelectedGroup()` | `Groups/SCR_GroupSubMenuBase.c:259` | the only join seam |
| `SCR_GroupSubMenuBase.AcceptInvite()` | `:285` | the only invite-accept seam |
| button wiring (`m_OnActivated`) | `:359`, `:379` | gamepad reachability |
| `SCR_AIGroup.SetPrivate` / `SetPrivacyChangeable` | `Entities/SCR_AIGroup.c:535`, `:548` | private-by-default |
| `SCR_AIGroup.GetOnPlayerAdded/Removed` | `:1072`, `:1078` | the reactor hooks |
| `RPC_DoAddPlayer` / `RPC_DoRemovePlayer` (invoke sites) | `:1280`, `:1503` | prove the hooks fire server-side |
| `AddPlayer` / `RemovePlayer` (authority entries) | `:1401`, `:1526` | local-then-broadcast ordering |
| `CheckForLeader` | `:1286` | leader promotion edge case |
| `IsFull` / `IsMaxMembersLimited` | `:614`, `:953` | the 6-player cap |
| `RemoveAgentFromControlledEntity` | `:1143` | recruit move-out |
| `SCR_GroupsManagerComponent.MovePlayerToGroup` | `Groups/SCR_GroupsManagerComponent.c:109` | the full-group hole |
| `OnGroupPlayerRemoved` / `DeleteGroupDelayed` | `:735`, `:721` | deletion timing, auto-unlock |
| `CanCreateNewGroup` / `TryFindEmptyGroup` | `:1319`, `:1205` | D11 |
| `CreateNewPlayableGroup` | `:1228` | slave-group creation |
| `AskAddAiMemberToGroup` / `AskRemoveAiMemberFromGroup` | `:1567`, `:1557` | broadcast AI membership |
| `OnAIMemberRemoved` (RplId resolution model) | `:1593` | copy for T4.3 |
| `SCR_PlayerControllerGroupComponent.RequestJoinGroup` | `Groups/SCR_PlayerControllerGroupComponent.c:830` | D3 override target |
| `RPC_AskJoinGroup` | `:873` | the server-side entry point |
| `RPC_ConfirmJoinPrivateGroup` | `:544` | the broken approve path |
| `PlayerRequestToJoinPrivateGroup` | `:106` | the request path |
| `InvitePlayer` / `CanInvitePlayer` | `:396`, `:327` | invites |
| `AddAIToSlaveGroup` | `:1470` | recruit placement |

**Overthrow files edited or created:**

```
Scripts/Game/
├── GameMode/Managers/
│   ├── OVT_PlayerGroupManagerComponent.c        (NEW - the reactor)
│   ├── OVT_GroupRecruitTransfer.c               (NEW - pure helper)
│   └── OVT_RecruitManagerComponent.c            (EDIT - :1737, :1963 guards; 2 new methods)
├── Respawn/Logic/
│   └── OVT_SpawnLogic.c                         (EDIT - delegate to EnsureOwnGroup)
├── Modded/
│   ├── SCR_GroupSubMenuBase.c                   (NEW - UX layer)
│   ├── SCR_PlayerControllerGroupComponent.c     (NEW - D3 fix)
│   └── SCR_GroupsManagerComponent.c             (NEW - D4 fix)
└── Tests/TestSuites/
    ├── Logic/OVT_TEST_Logic_GroupRecruits.c     (NEW)
    └── Init/OVT_TEST_InitSuite.c                (EDIT - one assertion)

Configs/UI/Dialogs/DialogPresets_Campaign.conf   (EDIT - 2 presets)
Prefabs/GameMode/OVT_OverthrowGameMode.et        (EDIT - register the manager)
Language/localization_Overthrow.st               (EDIT - #OVT- keys; exports regenerated by the user)
docs/features/core/player-groups/context.md      (NEW - Phase 1 verdict onward)
```

*(Place the three `modded class` files wherever the repo already keeps them — check `modded class SCR_PlayerController`'s location and match it rather than inventing `Scripts/Game/Modded/`.)*

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **The leader-approve path is dead on dedicated servers** (D3). `RPC_ConfirmJoinPrivateGroup` calls the client wrapper from server code — the BUG-088 family. | **High** (predicted from code reading) | Feature does not work at all in MP | Fixed in Phase 2 T2.2 before anything is built on it, and the Phase 2 acceptance criteria fail loudly if it is still broken. Solo cannot detect this — test on a dedicated server. |
| R2 | **A rejected join strands a player with no group** (D4) — the exact BUG-088 symptom. | **High** (present in vanilla today) | No commanding, no recruits, feels like a hard bug | Two independent defences: T2.3 rejects before emptying the old group, and the Phase 3 deferred restore catches any live player who ends a frame at `GetGroupID() == -1`. |
| R3 | Phase 1 finds a Group Menu fault that moves the seam the dialog hooks into. | Medium | Phase 5 rework | Phase 1 is deliberately first and its acceptance criterion is a written verdict; Phase 5's task list is amended before Phase 5 starts. Phases 2-4 are seam-independent and can proceed regardless. |
| R4 | Invoker ordering: Overthrow's `GetOnPlayerRemoved` handler runs after vanilla's, which may already have queued the ex-group for deletion. | Medium | Recruits orphaned in a deleted group's slave | Deletion drains next frame (`DeleteGroupDelayed` → `CallqueueCall(DeleteGroups)`, `:721-730`), so the ex-group is readable during the removal frame. T3.5 verifies `GetSlave()` is non-null by log. Do the pull-out **synchronously** in the handler; only the restore is deferred. |
| R5 | The one-frame restore deferral misfires on a normal group switch, spawning a spurious group. | Medium | Group churn, `TryFindEmptyGroup` noise, possible frequency exhaustion (`CanCreateNewGroup:1328`) | The deferred handler re-checks `GetGroupID()` and no-ops when the add already happened. Play-test step 5 (join) must show **no** extra group created; check the server's playable-group table before and after. |
| R6 | `SetPrivacyChangeable(false)` breaks or blanks the vanilla group-attributes UI. | Low-Medium | Cosmetic to broken sub-menu | T2.1 includes an explicit verification step and a named fallback (drop the call, re-assert `SetPrivate(true)` from the reactor). |
| R7 | ~96 AI in one slave group tanks server frame time. | Medium | Unplayable large groups | T6.2 measures rather than guesses; the budget knob and the pure `ExceedsAiBudget` helper are in place so enforcement is a one-line change once the number is known. Warn, never silently drop — dropping orphans AI. |
| R8 | Editing the 2,219-line `OVT_RecruitManagerComponent` regresses recruiting or respawn. | Medium | Recruits break — the feature's whole point | Phase 4 runs on `component-developer-advanced`. The guard change is two lines with an explicit before/after play-test (step 8 covers the new path, steps 2 and 13 cover the old). `FindRecruitEntity`'s known mid-iteration hazard is contained by snapshotting ids (T6.7), not fixed here. |
| R9 | `OVT_PlayerData.IsOffline()`'s `id == 0` vs `-1` bug leaks in and makes a departed owner's recruits commandable. | Medium | Grief vector: command a disconnected player's AI | D9 — do not consume `IsOffline()`; resolve liveness from `PlayerManager.GetPlayerController(playerId)`. Enforced by grep in review and by a `//!` comment at the call site. |
| R10 | A vanilla update changes `JoinSelectedGroup` / `AcceptInvite` / `MovePlayerToGroup` and silently breaks the overrides. | Low per-update, certain eventually | Feature degrades quietly | The UX-layer approach was chosen partly for this: three small overrides with `super` calls, each citing the vanilla file:line it mirrors. List them in `context.md` as an update-check checklist. |
| R11 | `#OVT-` keys are added to `.st` but exports are not yet regenerated, so the dialog shows raw keys. | High (expected, transient) | Ugly but harmless | Layouts use literal text until the export exists; `context.md` lists every key awaiting regeneration so the user can do one Workbench pass. **Never** hand-edit `localization_Overthrow.<lang>.conf`. |
| R12 | Group radio frequencies exhaust with many short-lived groups (`CanCreateNewGroup:1328` returns false when `GetFreeFrequency == -1`). | Low | New groups silently fail to create | R5's mitigation limits churn. `EnsureOwnGroup` already logs on `CreateNewPlayableGroup` returning null; make sure that log names the frequency case as a candidate cause. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Diagnose the Group Menu | `ui-developer` | no |
| 2 — Private groups + server-authority fixes | `network-specialist` | no |
| 3 — `OVT_PlayerGroupManagerComponent` / own-group guarantee | `network-specialist-advanced` | **yes** — new manager, global invoker subscriptions, refactor of the load-bearing spawn path |
| 4 — Recruits follow owner + guard fix + Logic tier | `component-developer-advanced` | **yes** — 2,219-line manager, guard gating two lifecycle paths, first Logic coverage |
| 5 — Group Menu UX layer + localization | `ui-developer-advanced` | **yes** — two vanilla menu overrides, new dialog presets, gamepad/console gate |
| 6 — Edge cases, AI density, hardening | `network-specialist` | no |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (2-4, 6), `overthrow-ui-patterns` (1, 5), `workbench-workflow` (all).
