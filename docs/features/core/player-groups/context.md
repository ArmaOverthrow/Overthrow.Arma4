# Player Groups - Context & Decisions

**Last Updated:** 2026-08-06 (MP play-test GREEN — feature COMPLETE)
**Current Phase:** — none. Built, gate-verified and play-test-verified.
**Status:** ✅ **COMPLETE.** Automated gates green (compile-check exit 0 · Fast 38 · All 66) **and the dedicated-server MP play-test passed with zero defects found** (user, 2026-08-06). The runtime gate the automated spine cannot reach is discharged.

**Epic:** `core` (feature #6) · **Addresses:** GitHub issue **#147** (second half) + the MP play-test of 2026-08-06

---

## Quick Status

**What's Done:**
- ✅ Requirements written (2026-08-06) and plan written (`implementation.md`, 6 phases, 31 tasks)
- ✅ **BUG-088 fixed and play-test-verified on a dedicated server** — the hard precondition. Faction replicates (`OVT_OverthrowFactionManager.et` was missing an `RplComponent`), each player has their own group, is its leader, AI commanding opens, recruits follow orders

- ✅ Phase 1 — verdict written below (static code-reading; no fault found in the listing path)
- ✅ Phase 2 — T2.1/T2.2/T2.3 implemented; compile-check exit 0, All group exit 0 (60 tests)
- ✅ Phase 3 — T3.1-T3.5 implemented; compile-check exit 0, All group exit 0 (**61** tests), new Init case proven able to fail both ways
- ✅ Phase 4 — T4.1-T4.6 implemented; compile-check exit 0, Fast **38**, All **66** (was 61), five new Logic cases proven able to fail with three separate deliberate faults
- ✅ Phase 5 — T5.1-T5.6 implemented; compile-check exit 0, All **66** (unchanged — UI is outside the automated spine), `git diff --stat Language/` shows only the `.st`
- ✅ Phase 6 — T6.1-T6.7 closed (three built, four verified-already-done); compile-check exit 0, Fast **38**, All **66**. **The AI-density number itself is still unmeasured and the budget is shipped at 0 (disabled)** — the procedure is written up under "Needs human verification"

### ✅ MP play-test GREEN — 2026-08-06 (user, dedicated server)

**Every dedicated-server test passed. No defects found.** This discharges the runtime gate that the automated spine structurally cannot reach (JIP/MP is uncovered — it is where BUG-088 lived, and where every one of this feature's four unpredicted defects would have surfaced).

Explicitly confirmed by the user, including the hardest case in the whole plan:

- **F11 / T6.1 — a group leader with joined players AND recruits disconnects** → a new leader was promoted and **had command of the group's recruits**. This is the one edge case with *no* automated coverage and the most moving parts (leader promotion × recruit ownership × the T6.3 disconnect pull-out all firing in the same teardown). It behaved exactly as designed: the departing leader's own recruits left with them, everyone else's stayed and answered to the promoted leader, and ownership never moved.

The verification items previously listed below were superseded by this pass and are retained as the record of what was checked.

---

**Remaining housekeeping (not blocking — the feature is done):**
- ⏸️ **One more Workbench localization export pass** — 2 Phase 6 keys (`OVT-Msg-GroupJoinRefusedFull`, `OVT-Msg-GroupAiBudgetExceeded`) are `.st`-only and, being notification presets, have **no code fallback**; they render as raw `#OVT-Msg-…` keys in the notification strip until exported. Both sit on rare paths (a join refused because the group filled up first; the AI-budget warning, which is dormant while the budget is 0), which is why the play-test would not have surfaced them. The 9 Phase 5 UI keys were already caught by the user's 21:52 export.
- ⏸️ **The AI-density number is still unmeasured** — `MAX_SLAVE_AI_PER_GROUP` ships at **0 = disabled**, so nothing warns and nothing is capped. The knob, the warning notifications and the Logic-pinned arithmetic are all in place; only the number is missing. Procedure below. Worth doing if large shared groups ever become common.

**Blockers:**
- None. The feature is complete.

---

## Key Files

### Core Implementation
- ✅ `Scripts/Game/GameMode/Managers/OVT_PlayerGroupManagerComponent.c` — NEW (Phase 3), the server-side reactor: `EnsureOwnGroup`, the two membership handlers, the one-frame restore
- ✅ `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c` — EDIT (Phase 3), `CreateAndJoinGroup` is now a thin caller of `EnsureOwnGroup`
- ✅ `Prefabs/GameMode/OVT_OverthrowGameMode.et` — EDIT (Phase 3), registers the manager **and** sets `m_bAllowRejoinPlayerAfterReconnecting 0`
- ✅ `Scripts/Game/Modded/SCR_GroupsManagerComponent.c` — NEW (Phase 2, D4) + `IsGroupQueuedForDeletionOVT()` (Phase 3)
- ✅ `Scripts/Game/Player/Modded/SCR_PlayerControllerGroupComponent.c` — NEW (Phase 2, D3)
- ✅ `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c` — EDIT (Phase 3), `OVT_TEST_Init_PlayerGroups_ManagerResolves`
- ✅ `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c` — NEW (Phase 4), the pure helper; the only Logic-tier-testable part of this feature
- ✅ `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` — EDIT (Phase 4), both leader→membership guards, the retry cap, `MoveRecruitsToGroup` / `RemoveRecruitsFromGroup` / `FindOwnerGroupController`
- ✅ `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GroupRecruits.c` — NEW (Phase 4), 5 cases, already inside the Fast **and** All groups (both list `OVT_TEST_LogicSuite`, so no group config changed)
- ✅ `Scripts/Game/UI/Modded/SCR_GroupSubMenuBase.c` — NEW (Phase 5), the whole UX layer: confirm-then-`super` on both join routes, the client-side pre-check, the explanatory paragraph
- ✅ `Configs/UI/Dialogs/DialogPresets_Campaign.conf` — EDIT (Phase 5), **3** presets (`JOIN_GROUP_CONFIRM`, `JOIN_GROUP_CONFIRM_RECRUITS`, `JOIN_GROUP_BLOCKED` — see the decision on the third)
- ✅ `UI/Layouts/Menu/GroupModelExplainer.layout` + `.layout.meta` — NEW (Phase 5), one wrapped `RichTextWidget`, instantiated into vanilla's existing empty `GrouplistFooter`; **no vanilla layout is forked**
- ✅ `Configs/overthrowBroadcastMessages.conf` — EDIT (Phase 5), the `PlayerJoinedWithRecruits` notification preset
- ✅ `Scripts/Game/GameMode/Managers/OVT_PlayerGroupManagerComponent.c` — EDIT (Phase 5), `NotifyLeaderOfArrivingRecruits` off `OnGroupPlayerAdded`
- ✅ `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` — EDIT (Phase 5), `MoveRecruitsToGroup` now **returns the number it placed** (was `void`)
- ✅ `Language/localization_Overthrow.st` — EDIT (Phase 5), 9 `#OVT-` keys (**never** the `.<lang>.conf` exports)

### Related
- `docs/features/core/player-groups/implementation.md` — the plan (architecture, decisions D1-D11, risks R1-R12, DoD F1-F13/Q1-Q10/I1-I4)
- `docs/features/core/player-groups/requirements.md` — scope truth
- `docs/features/core/epic-overview.md` — epic rollup and build order
- Vanilla reference tree: `/mnt/n/Projects/Arma 4/ArmaReforger`

---

## Important Decisions

The plan's **D1-D11** are the decision record (`implementation.md` §5) — they are not duplicated here. This file records decisions **made during the build**, plus the Phase 1 verdict.

### Decision: the `RPC_ConfirmJoinPrivateGroup` null hole was NOT patched — Phase 1 correction #4 is half wrong (2026-08-06, Phase 2)

Phase 1 said "`playerComponent` is not null-checked at `:558-560`, so a requester who disconnects between request and approval null-derefs the server". Re-read empirically against `Groups/SCR_PlayerControllerGroupComponent.c:544-565`:

- The literal claim is true — `playerComponent` (`:558`) is dereferenced at `:560` with no null test.
- **But the disconnect scenario it describes is already closed**: `:554-556` resolves `GetGame().GetPlayerManager().GetPlayerController(playerID)` and returns on null. A requester who disconnected has no PlayerController, so the method returns before `:558` is reached.
- The residual reachable case is therefore only "a live PlayerController that has no `SCR_PlayerControllerGroupComponent`", which cannot happen with the vanilla PlayerController prefab (the component is where `RequestJoinGroup` itself lives).

**Decided: leave vanilla alone.** Closing it would require overriding `RPC_ConfirmJoinPrivateGroup`, an `[RplRpc(RplChannel.Reliable, RplRcver.Server)]`-attributed method. Overthrow has **no precedent** for overriding an existing vanilla RPC (the one `[RplRpc]` in a modded class, `SCR_PlayerController.RpcAsk_RestorePossessionOVT`, is a *new* RPC), and D3 deliberately avoids that question by overriding the plain wrapper instead. Adding divergence in a replicated dispatch path to guard an unreachable branch is a bad trade. There is also no reachable seam from Overthrow code in which to guard defensively — the deref is inside vanilla, before any Overthrow-controlled call. **If a future Reforger version removes the `:554-556` PlayerController guard, this becomes live and must be revisited** — recorded in the vanilla-update check-list below.

### Decision: `SetPrivate` / `SetPrivacyChangeable` are called on the group before the owner is joined to it (2026-08-06, Phase 2)

`CreateAndJoinGroup` now does `SetCustomName` → `SetPrivate(true)` → `SetPrivacyChangeable(false)` → join. Verified this cannot self-reject: neither `RPC_AskJoinGroup` (`SCR_PlayerControllerGroupComponent.c:872-896`) nor `MovePlayerToGroup` / `AddPlayerToGroup` tests `IsPrivate()` — privacy is enforced purely by the client-side menu branch in `SCR_GroupSubMenuBase.JoinSelectedGroup` (`:267`). Setting the flags first means the group is never observable as public, not even for one frame.

### Decision: the invoker subscriptions are NOT gated on `Replication.IsServer()` — only the handler bodies are (2026-08-06, Phase 3)

T3.3 asked for "subscribe in `OnPostInit`, server-only". Implemented as **subscribe unconditionally, guard every handler body** instead. The two failure modes are wildly asymmetric: if `Replication.IsServer()` is not yet meaningful during a game-mode component's `OnPostInit` on a dedicated server, a gated subscription is a **silently dead own-group guarantee** (BUG-088 all over again, invisible in solo); a wrongly-kept subscription on a client costs one comparison per membership change. Vanilla makes exactly the same trade — `SCR_GroupsManagerComponent` subscribes to both invokers unconditionally (`Groups/SCR_GroupsManagerComponent.c:1723-1724`) and guards its handler bodies with `IsProxy()`. Server authority is unaffected: `OnGroupPlayerAdded`, `OnGroupPlayerRemoved`, `RestoreOwnGroupDeferred` and `EnsureOwnGroup` all open with `Replication.IsServer()`.

Ordering fallout worth knowing: Overthrow subscribes in `OnPostInit` and vanilla in `EOnInit`, so **Overthrow's handlers run first** for any one membership event. That is the ordering Phase 4 wants (pull recruits out before vanilla queues the group for deletion).

### Decision: `SetCivilianFaction` stays on the spawn path, it did NOT move into `EnsureOwnGroup` (2026-08-06, Phase 3)

T3.2 listed `SetCivilianFaction` among the things `EnsureOwnGroup` "keeps". It is kept — but in `OVT_SpawnLogic.CreateAndJoinGroup`, immediately before the delegation, not inside the manager. Assigning a faction is **destructive to group membership**: it fires `SCR_GroupsManagerComponent.OnPlayerFactionChanged` (`Groups/SCR_GroupsManagerComponent.c:1101-1140`), which calls `oldGroup.RemovePlayer()` at `:1130` and `groupComp.ResetGroupIDs_S()` at `:1139`. That is correct exactly once, at spawn; running it from the reactor's restore would tear down the membership the restore is trying to establish. The manager keeps the *faction null-check with its BUG-088 error print* (`GetPlayerFaction(playerId)` → `ERROR` + return -1), which is the part that belongs with group creation.

### Decision: the retry ladder and the reactor cooperate through ONE idempotent primitive, not through a flag (2026-08-06, Phase 3)

`EnsureOwnGroup` is the only thing that creates a group, it is idempotent (returns the current id untouched when `GetGroupID() != -1`), and both callers run on the main thread through the call queue. So "the spawn ladder and the reactor both fire" is safe **by construction** — whichever gets there first creates the group, the other returns the same id and does nothing. They cannot both create.

The only observable difference is which one fires `m_OnPlayerGroupCreated`, and per D6 that is **only ever the spawn path** (`CreateAndJoinGroup`, on the branch where it found the player at `GetGroupID() == -1`). Traced: the race needs `OnPlayerFactionChanged` to produce a `RemovePlayer` for a player who is *also* mid-`CreateAndJoinGroup`, which needs a faction change while already in a group — and `SetCivilianFaction` early-returns when the faction is already right (`OVT_SpawnLogic.c:992-996`), so after the first assignment it never fires again. On a fresh or reconnecting player the old group id is `-1`, `FindGroup(-1)` is null, and no removal happens at all. **The race is unreachable in practice**; if it ever did happen the cost is one skipped recruit-respawn pass, and Phase 4 (which moves live recruits directly on membership change) removes the last reason to care.

### Decision: §3.3's "rejected join" case is closed UPSTREAM by T2.3, not caught by the deferral (2026-08-06, Phase 3)

The plan expected a full-group rejection to arrive in `RestoreOwnGroupDeferred` at `GetGroupID() == -1` and be repaired there (the G1 safety net). Re-traced against the real code with Phase 2 in the tree, that path no longer exists:

- T2.3's `MovePlayerToGroup` override rejects **before** `super`, so no `RemovePlayer` and no `AddPlayer` run, **neither invoker fires**, and `RPC_AskJoinGroup`'s `groupIDAfter != m_iGroupID` test at `:889` is false — nothing changes and the player stays exactly where they were. Strictly better than being restored after the fact, and the deferral is never even scheduled.
- The residual shape — a player who is **already** at `-1` and whose join is refused (`AddPlayerToGroup` returns `-1`, `Groups/SCR_GroupsManagerComponent.c:169-180`) — also fires no invoker, so the deferral cannot catch it either. Its recovery is `OVT_SpawnLogic`'s own 3 s `CreateAndJoinGroup` timer, which runs after every player-entity change.

So the deferral's live cases are three, not four: **normal switch → no-op**, **genuine leave → restore**, **disconnect → no-op** (no `PlayerController`). This is a correction to `implementation.md` §3.3, which is left as written.

### Decision: `m_bAllowRejoinPlayerAfterReconnecting 0` on the game-mode prefab — D2 was not actually enforced (2026-08-06, Phase 3)

D2 says reconnect returns you to your **own** group and reasons that the design "avoids vanilla's `RejoinPlayer()` … and is gated behind `m_bAllowRejoinPlayerAfterReconnecting` anyway". That gate **defaults to 1** (`Groups/SCR_GroupsManagerComponent.c:34-35`) and Overthrow's prefab never overrode it. Worse, Phase 2's T2.2 fix made `RejoinPlayer` *work for the first time*: it ends in `playerGroupCompoment.RequestJoinGroup(...)` (`:1159`, `:1168`) from server code, which before T2.2 marshalled a `RplRcver.Server` RPC on the authority and went nowhere. So T2.2 silently activated a D2 violation — a player who disconnected from a friend's group would be put straight back into it on reconnect (`m_mDisconnectedPlayerIDs` is written by `SCR_AIGroup.OnPlayerDisconnected:1184`).

Turning the attribute off restores the pre-T2.2 behaviour of that path exactly (it was a no-op), implements D2 with no code, and hands the reconnecting player to `OVT_SpawnLogic.CreateAndJoinGroup` → `EnsureOwnGroup` like any other spawn. **This is what makes §6 step 10 pass.**

### Decision: `EnsureOwnGroup` ADOPTS an empty playable group before creating a new one (2026-08-06, Phase 3)

Not in the task list; added inside T3.2 and worth a reviewer's attention. Three reasons, in order of importance:

1. **It dissolves the permanent stray group that traps every player in whatever group they are in.** Phase 1 correction #7 noted vanilla creates an extra empty FIA group at the first faction assignment (`OnPlayerFactionChanged:1112-1114`) and called it cosmetic. It is not. Nothing ever deletes a group that never had a player (`DeleteGroupDelayed` is only ever called from the *removal* handler), so it sits empty forever, and an empty faction group makes `TryFindEmptyGroup()` non-null — which permanently returns **false** from `CanCreateNewGroup()` (`:1347`) **and** makes `RPC_AskCreateGroup()` return at `:667`. Vanilla has no "leave group" action at all (verified: zero hits for `LeaveGroup`/`RequestLeaveGroup` in the whole script tree; `RequestRemovePlayer` is editor-only) — **"Create new group" *is* the leave route**. With the stray present, nobody can ever leave a group they joined. Adopting the empty group at the first spawn means there is no stray, and a player who is in a shared group gets the Add-group button back.
2. **R12 / R5.** Reusing beats churning: fewer groups created and destroyed, less pressure on `GetFreeFrequency` (`CanCreateNewGroup:1328` returns false when frequencies run out).
3. It is what vanilla itself insists on — `RPC_AskCreateGroup` refuses to create while an empty group exists.

**The deletion-queue guard is load-bearing.** `DeleteGroups` (`:1647-1654`) destroys everything in `m_aDeletionQueue` next frame **without re-testing emptiness**, so adopting a group emptied earlier in the same frame would hand the player a group that vanishes a frame later — a live player at `GetGroupID() == -1`, the exact state this manager exists to prevent. `m_aDeletionQueue` is `protected`, so the check is exposed as `IsGroupQueuedForDeletionOVT()` on the already-modded `SCR_GroupsManagerComponent` (OVT-suffixed so a future vanilla method of the same name cannot silently collide).

*To back this out:* delete `FindAdoptableGroup` and call `CreateNewPlayableGroup` directly. Everything else keeps working — the stray comes back and with it the "nobody can leave a group" trap.

### Decision: `OnGroupPlayerAdded` claims a group the player is now ALONE in (2026-08-06, Phase 3)

Also not in the task list. Consequence of the above: the only exit from a shared group is vanilla's `RPC_AskCreateGroup` (`:675-683`), which does nothing but create and join — **the group it makes is public and unnamed**. Without this rule, "B leaves A's group" lands B in an anonymous public group and Phase 3's own acceptance criterion ("B is immediately back in a group of their own, named after B, private, with B as leader") fails.

The rule is: *a group whose only member is you IS your group* — brand it (custom name, private, privacy not changeable), however you got there. `IsPrivate()` is the decisive skip, because `EnsureOwnGroup` brands **before** it joins the owner and `SetPrivate` applies `m_bPrivate` synchronously (`RPC_SetPrivate`, `Entities/SCR_AIGroup.c:1342-1344`), so a group this manager made is already private and is left alone — no double `SetCustomName`, no second trip through the async profanity filter. `GetPlayerCount()` already includes the joining player (`RPC_DoAddPlayer` inserts before invoking, `:1279-1280`).

### Decision: vanilla DOES have a public mirror of `AddAIToSlaveGroup` — and the pull-out still inlines it (2026-08-06, Phase 4)

T4.3 said "vanilla has no public mirror of `AddAIToSlaveGroup`, so write the four-line mirror, modelled on `SCR_GroupsManagerComponent.OnAIMemberRemoved`". **That is wrong.** `SCR_PlayerControllerGroupComponent.RemoveAiFromSlaveGroup(notnull IEntity, SCR_AIGroup)` (`Groups/SCR_PlayerControllerGroupComponent.c:1526-1552`) is public, is on the same component as `AddAIToSlaveGroup`, takes the same **master** group, is documented "Should be only called on the server", and is vanilla's own dismiss-an-AI path (`RPC_AskRemoveAIAgent:1501-1521`). It does one thing the plan's four-line sketch would have missed: `if (slaveGroup.GetAgentsCount() == 1) slaveGroup.Deactivate()` before the removal.

**Decided: use vanilla's body, but inline it rather than calling it.** The reason is not stylistic — `RemoveAiFromSlaveGroup` is reached through an owner's `SCR_PlayerControllerGroupComponent`, and the move-OUT half must keep working for an owner who no longer has a player controller (a disconnect is a removal, and it is the removal where leaving recruits behind is worst). The method reads nothing off the component it is called on, so `RemoveRecruitsFromGroup` performs the same three steps directly — `Deactivate()` on the last agent, `RemoveAgentFromControlledEntity`, `AskRemoveAiMemberFromGroup` — with no controller dependency. Added to the vanilla-update checklist below.

The move-IN half keeps calling `groupController.AddAIToSlaveGroup(...)` on the owner's controller, exactly as `AddRecruitToPlayerGroup` already did: there the owner is definitionally online (they just joined a group), and matching the existing call site is worth more than symmetry.

### Decision: the deletion-queue guard is consulted on the ADD path and deliberately NOT on the REMOVE path (2026-08-06, Phase 4)

Phase 3 exposed `IsGroupQueuedForDeletionOVT()`. Phase 4 uses it in exactly one place, `MoveOwnedRecruitsIn`:

- **ADD — consulted.** A group emptied earlier in the same frame is already queued and will be destroyed next frame *without a re-test of emptiness* (`DeleteGroups`, `Groups/SCR_GroupsManagerComponent.c:1647-1654`). Placing recruits in a doomed group's slave strands them. Skipping costs nothing: the player ends that frame at `GetGroupID() == -1`, `RestoreOwnGroupDeferred` gives them their own group, and that add comes straight back through the same method. **The reactor is self-healing this way** — every route into any group re-runs the transfer.
- **REMOVE — not consulted, on purpose.** A doomed ex-group is precisely the case where the pull-out matters most. Guarding there would skip the removal exactly when it is load-bearing.

### Decision: `RespawnRecruitsDelayed`'s retry cap counts BOTH re-arm branches, not just the one T4.5 named (2026-08-06, Phase 4)

T4.5 asked for a cap on the leadership/membership branch and said to keep the `groupId == -1` guard "as-is". Read literally that leaves the **other** branch re-arming a 500 ms timer forever, which is the same defect the cap exists to remove — so the single `retryCount` parameter is incremented by both branches and both stop at 10 (matching `OVT_SpawnLogic.CreateAndJoinGroupDelayed`). The guard *predicates* are untouched; only the re-arm is counted.

Worth a reviewer's eye, because exhaustion is not free: it means a returning player's stored recruit bodies are not asked back this session, and the only other trigger is `m_OnPlayerGroupCreated`, which `CreateAndJoinGroup` fires **only** when it found the player at `GetGroupID() == -1` — a player handed a group by the reactor's restore does not re-fire it. Exhaustion therefore logs at `LogLevel.WARNING` naming that consequence. In practice the branch is now almost unreachable: the membership test is satisfied by every group the player is in, and the `-1` branch would need a live player to stay groupless for 5 s after their group was created, which is the state the Phase 3 reactor exists to prevent.

### Decision: D9's stated rationale is STALE — `OVT_PlayerData.IsOffline()` is already fixed — and the decision stands anyway (2026-08-06, Phase 4)

D9 and risk R9 both say "`IsOffline()` tests `id == 0` while disconnect sets `id = -1`, so departed players report online". Measured against the current tree, `Scripts/Game/Data/OVT_PlayerData.c:84-87` reads `return id <= 0;` and its comment names both sentinels. Somebody in `core/player-manager` fixed it; the docs never caught up.

**The decision is unchanged, for a better reason than the one recorded.** Liveness is still resolved from `GetGame().GetPlayerManager().GetPlayerController(playerId) != null` at the reactor call site, because a recruit's commandability is a question about the *engine's* session state, while the record's accessor answers from Overthrow's own connect/disconnect bookkeeping — a different fact, maintained by different code, that has been out of step before. Keeping exactly one source for the offline rule inside this feature is also what lets `SelectTransferable` stay a pure function and therefore Tier-A testable. The `//!` comments at both ends say this rather than repeating the stale bug claim.

### Decision: a DISCONNECT does not reach the recruit pull-out yet — that gap is T6.3, and it is now logged rather than silent (2026-08-06, Phase 4)

Found while wiring T4.4, and it changes what §6 step 10 will show. A disconnect **is** a removal (`SCR_AIGroup.OnPlayerDisconnected:1175-1187` → `RemovePlayer`), so `OnGroupPlayerRemoved` does fire for one — but by then the departing player's runtime→persistent id mapping is gone:

```
OVT_OverthrowGameMode.OnPlayerDisconnected
  :832  m_PlayerManager.m_OnPlayerDisconnected.Invoke(persId, playerId)   <-- persistent id still available HERE
  :836  m_PlayerManager.ClearPlayerIdMappings(playerId)                   <-- mapping dropped
  :838  super.OnPlayerDisconnected(...)  -> SCR_BaseGameMode fires its disconnect invoker
                                         -> SCR_AIGroup.OnPlayerDisconnected -> RemovePlayer
                                         -> our OnGroupPlayerRemoved, with no persistent id to look up
```

So `MoveOwnedRecruitsOut` bails, and a player who disconnects from a **shared** group leaves their recruits under the remaining leader's command until the offline despawn collects them (~10 min). It bails **loudly** — `LogLevel.WARNING`, naming the cause and pointing at T6.3 — rather than silently, and this was left as a Phase 6 item because the plan's own edge-case table assigns the offline-owner rule to **T6.3**, not to Phase 4.

**The seam for the fix is already prepared:** `m_OnPlayerDisconnected` fires at `:832` carrying the persistent id, one line before the mapping is cleared, and `RemoveRecruitsFromGroup()` was written to need no player controller precisely so it can be called from there unchanged.

### Decision: Phase 3's "claim a group you are alone in" rule moved into its own method, untouched (2026-08-06, Phase 4)

`OnGroupPlayerAdded` was a chain of early returns (`GetPlayerCount() != 1` → return, `IsPrivate()` → return), so appending the recruit move would have made it run only for players alone in a public group. The Phase 3 body is now `ClaimGroupIfAlone()` verbatim, and the handler is two calls: claim, then move recruits in. Nothing about the Phase 3 rule changed.

### Decision: the transfer target is passed in, never read back from the owner's controller (2026-08-06, Phase 4)

`MoveRecruitsToGroup` takes the target group as an argument and must keep doing so. During a group switch **both** membership invokers fire while `SCR_PlayerControllerGroupComponent.m_iGroupID` still holds the OLD group id (it is written afterwards, in `RPC_AskJoinGroup:892` — the same gotcha Phase 3 recorded for the deferred restore), so a `GetGroupID()` lookup inside `OnGroupPlayerAdded` would put the recruits straight back where they came from. Recorded because it looks like a harmless simplification.

### Decision: §3.4's single-seam claim was RE-VERIFIED against the files and still holds (2026-08-06, Phase 5)

Not taken on trust. Re-read for Phase 5 against `/mnt/n/Projects/Arma 4/ArmaReforger`:

- `SCR_GroupSubMenuBase.JoinSelectedGroup()` is still at **`Groups/SCR_GroupSubMenuBase.c:259`** and `AcceptInvite()` at **`:285`** — both line numbers exact.
- `grep -rn JoinSelectedGroup scripts/` returns **two** hits in the whole vanilla tree: the definition and `:359`, the Join button's `m_OnActivated.Insert`. `grep -rn 'AcceptInvite\b'` returns the definition, its one call site `:287`, the button wiring `:379`, the label const, and `SCR_PlayerControllerGroupComponent.AcceptInvite:434` — i.e. **nothing else in the game calls either**.
- `class SCR_GroupSubMenuBase` has exactly **two** subclasses (`SCR_GroupSubMenu`, `SCR_GroupSubMenuPlayerlist`) and **neither overrides either method** — re-read in full, not grepped: `SCR_GroupSubMenuPlayerlist` overrides `OnMenuUpdate`, `OnTabCreate`, `OnTabShow`, `OnTabHide`, `UpdateGroups`; `SCR_GroupSubMenu` overrides `OnTabCreate`, `OnTabShow`, `OnTabHide`.
- The **deploy route is covered and not broken**: `SCR_GroupSubMenu` lives in `UI/layouts/Menus/GroupSlection/SelectGroupMenu.layout:4` and inherits both overrides unchanged; the pause-menu route is `SCR_GroupSubMenuPlayerlist` in `GroupMenuPlayerlist.layout:4`, reached from `GroupMenu.layout:127` (tab 1) via `chimeraMenus.conf:286`. The explanatory paragraph attaches to `GrouplistFooter`, which **both** layouts have (`GroupMenuPlayerlist.layout:143`, `SelectGroupMenu.layout:132`), so both routes gain it and neither is forked.
- `SCR_GroupInviteStickyNotificationUIComponent` is confirmed display-only: its `OnButton()` opens `ChimeraMenuPreset.PlayerListMenu` and nothing else; it subscribes to `GetOnInviteAccepted()` purely to remove itself.
- `UI/layouts/Menus/GroupSlection/GroupRequestEntry.layout:207` also names the `GroupAcceptInvite` **action**, but it belongs to `SCR_RequestToJoinSubMenu` — the *leader's* approve/deny list, a different operation (`AcceptJoinPrivateGroup`), not a third join seam.

**One modded class still covers every route. No amendment to the plan was needed.**

### Decision: a THIRD dialog preset, `JOIN_GROUP_BLOCKED`, carries the refusal messages (2026-08-06, Phase 5)

T5.2 asked for two presets. A third was added because the refusal messages (G9: "that group is full", "that group no longer exists") needed a delivery surface and every alternative was worse:

- **A HUD text notification** is the Overthrow house style for "you can't do that", and `OVT_NotificationManagerComponent.SendTextNotification` even resolves **entirely locally** when the target is the local player (`:108-119` short-circuits to `RpcDo_RcvTextNotification` with no RPC). But the Group menu is a full-screen `MenuBase` drawn over the HUD, and whether the strip renders on top of it is exactly the kind of thing an agent cannot check. A message the player may not see is not a message.
- **`SCR_NotificationsComponent.SendToPlayer`** — vanilla's own choice at this seam (`GROUPS_REQUEST_SENT`) — needs an `ENotification` value, and there is no "group is full" one. Adding one means a `modded enum` **and** a vanilla notification-preset entry. Out of proportion, and T5.4 already forbids the enum for the leader notification.
- **Reusing `JOIN_GROUP_CONFIRM` with the confirm button hidden** leaves an informational dialog whose only button is labelled "Cancel".

`JOIN_GROUP_BLOCKED` is modelled on vanilla's own `timeout_ok` (`Configs/Dialogs/CommonDialogs.conf:29-39`): `m_eVisualStyle WARNING`, warning icon, **one** button, tag `confirm`, action `DialogConfirm`, label `#AR-Menu_OK`, right-aligned — byte-for-byte the shape of `Configs/ConfigurableDialogs/Buttons/Ok.conf`. Deliberately Overthrow-owned rather than opening vanilla's `timeout_ok` by tag, so a Reforger rename cannot silently kill Overthrow's error messages.

*To back this out:* delete the preset and the three `OvtShowBlockedDialog` call sites; the pre-check then reverts to a silent refusal, which is what Phase 2 shipped.

### Decision: the dialog is opened, but membership still goes exclusively through `super` (2026-08-06, Phase 5)

Worth stating explicitly because it is the whole safety argument. `OVT_GROUP_DIALOGS` opens a dialog; the confirm handler ends in `OvtCallSuperJoin()`, whose entire body is `super.AcceptInvite()` or `super.JoinSelectedGroup()`. There is **no** Overthrow code path that calls `PlayerRequestToJoinPrivateGroup`, `RequestJoinGroup`, `AddPlayer`, `RemovePlayer` or `MovePlayerToGroup`. Cancel does nothing at all — not even a notification to the other player. Verified by grep over `Scripts/`: the only hits for those five names are the Phase 2/3 server-side files, not the UI layer. D1 holds: **nothing new crosses the wire in either direction.**

`super.X()` **is** legal from a helper method in a `modded class` (compile-verified), so the two `super` calls live in one place instead of behind a re-entrancy sentinel.

### Decision: the confirm handler consumes its pending id on the first line — this is a double-fire guard, not tidiness (2026-08-06, Phase 5)

`OvtOnJoinConfirmed` reads `m_iOvtPendingJoinGroupId` into a local, sets the member to `-1`, and returns if the local is `< 0`. That makes a second `m_OnConfirm` invocation a no-op.

It matters because on a gamepad **`a` is both `DialogConfirm` and `MenuSelect`**, and `SCR_InputButtonComponent.OnInput()` (`UI/Components/WidgetLibrary/SCR_InputButton/SCR_InputButtonComponent.c:726-748`) has no double-fire guard of its own — `OnClick` and the action listener both funnel into it. Every vanilla dialog in the game has this shape, so it is presumably handled upstream, but "presumably" is not something to bet a duplicate join request on. Do not collapse those two lines.

### Decision: the join target is re-resolved AND the tile selection is restored before delegating (2026-08-06, Phase 5)

Both vanilla methods act on `m_PlayerGroupController.GetSelectedGroupID()`, not on an argument. Between opening the dialog and confirming it, two things can change:

1. **The group can be deleted or filled** — so `OvtOnJoinConfirmed` re-runs `FindGroup(groupId)` and `IsFull()` and shows the blocked dialog instead of sending a doomed request.
2. **The selection can move.** `SCR_GroupTileButton.OnFocus` calls `SetSelectedGroupID` (`SCR_GroupTileButton.c:1055-1061`), and closing a dialog restores focus to the menu underneath. If the restored focus lands on a different tile, `super` would join the wrong group. So the handler puts the selection back on the group the player actually agreed to. `SetSelectedGroupID` is pure client-side UI state (`SCR_PlayerControllerGroupComponent.c:1083-1090`: assign `m_iUISelectedGroupID`, fire a UI invoker) — it is not a membership change.

### Decision: the leader notification counts recruits that ARRIVED, so `MoveRecruitsToGroup` now returns an int (2026-08-06, Phase 5)

T5.4's sketch passed `recruitCount` — the owner's paper roster. That is the wrong number: `SelectTransferable` refuses offline recruits and offline owners, `FindRecruitEntity` can come back null mid-respawn, and `MoveOwnedRecruitsIn` bails outright for a group queued for deletion. A leader told "B joined with 3 recruits" who can command none of them is worse than no notification.

`OVT_RecruitManagerComponent.MoveRecruitsToGroup` therefore changed from `void` to `int`, returning its existing `moved` counter (every early bail returns 0), and `OVT_PlayerGroupManagerComponent.MoveOwnedRecruitsIn` forwards it. **F5 falls out for free**: 0 recruits placed ⇒ no notification. The change is signature-only; the body, the log line and the one existing call site are otherwise untouched, and the All group still passes 66.

The notification is also skipped when `group.GetLeaderID() == playerID` — spawning into your own group and being promoted into an empty one both arrive through `OnGroupPlayerAdded`, and nobody needs telling about their own recruits.

### Decision: every new string has an English fallback in code, so the export lag is invisible (2026-08-06, Phase 5)

Risk R11 says a brand-new `#OVT-` key shows as a raw key until the user regenerates the runtime exports in the Workbench. The repo's actual convention is to reference the key anyway — `Configs/Jobs/assassinateOfficer.conf` (commit `d25700cc`, yesterday) references `#OVT-Job_AssassinateOfficer_Description`, which is in the `.st` and in **no** export.

Rather than choose between "ship a raw key" and "ship hard-coded English that has to be edited twice", `SCR_GroupSubMenuBase` routes every user-facing string through `OvtTranslateOrEnglish` / `...1` / `...2`: it calls `WidgetManager.Translate(key, ...)`, and falls back to an English literal only when the result is empty or identical to the key it was handed. Once the export exists the fallbacks become unreachable and can stay as insurance. The `.conf` presets carry the `#OVT-` keys (correct long-term) but the script overwrites title and message on every open, so the preset text is only ever a design-time placeholder.

**Trade-off a reviewer should weigh:** the English text now exists in two places (the `.st` and the `const` block at the top of the modded class). They must be kept in step by hand. The alternative was a screen full of `#OVT-...` between now and the next Workbench pass.

### Decision: the explanatory paragraph is a new Overthrow layout dropped into a vanilla container, not a fork (2026-08-06, Phase 5)

`GrouplistFooter` is an **empty** `VerticalLayoutWidget` that both vanilla group layouts already declare below the group list. `OnTabCreate` instantiates `UI/Layouts/Menu/GroupModelExplainer.layout` into it. Consequences worth knowing:

- Forking `GroupMenuPlayerlist.layout` (708 lines) or `SelectGroupMenu.layout` (515) would have meant maintaining a copy of a vanilla menu through every Reforger update. This degrades instead: if `GrouplistFooter` is ever renamed, `FindAnyWidget` returns null and the **text is missing** — the tab still works.
- The widget is a `RichTextWidget` inheriting `{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout` (16 px, `Wrap 1`), dimmed to 70 % alpha. It is **not focusable**, so the gamepad focus chain between tiles and the footer buttons is untouched.
- `GrouplistFooter`'s slot has `FillWeight 0.08` inside a `Clipping True` parent, and the paragraph is ~250 characters in a narrow column. **This is the one thing in Phase 5 an agent genuinely cannot check** — see the human-verification item. If it clips, the fix is to shorten `OVT-PlayerGroups_ModelExplainer` (and the matching `OVT_EN_MODEL_EXPLAINER` literal), not to restructure anything.
- Guarded against being added twice (`footer.FindAnyWidget(...)` before `CreateWidgets`) in case a tab is ever created twice on one component.

### Decision: the client-side pre-check exists because vanilla's own gate is STALE, not because vanilla lacks one (2026-08-06, Phase 5)

`SCR_GroupTileButton.SetupJoinGroupButton` (`:703-765`) already hides and disables the Join button when `CanPlayerJoinGroup` fails — which covers full, wrong faction, own group, commander role and rank. So the pre-check looks redundant. It is not: `SetupJoinGroupButton` only runs from `RefreshPlayers`, so a group that filled up **since the tile was last drawn** still shows an enabled Join button. Before Phase 5, pressing it did nothing at all and the player got no explanation; the server-side guarantee (T2.3) is silent by design.

The pre-check therefore reuses vanilla's own `CanPlayerJoinGroup` (`SCR_PlayerControllerGroupComponent.c:136-245`) verbatim — it is public, it is the same predicate the tile uses, and reusing it means Overthrow cannot drift from vanilla's answer. `IsFull()` is tested *separately and first* only to choose the more specific of the two messages.

### Decision: joining your own group skips BOTH the dialog and the pre-check (2026-08-06, Phase 5)

The own-group test has to come before `OvtCheckCanJoin`, because `CanPlayerJoinGroup` returns false for "already in this group" (`:222-228`) and would otherwise pop "you cannot join that group right now" at a player pressing Join on their own tile. The order is: no controller → `super`; nothing selected → `super`; group gone → message; **own group → `super`**; can't join → message; otherwise → dialog.

### Phase 6 — the edge-case table, row by row: what was actually done vs what the plan predicted (2026-08-06)

| Row (plan §4 Phase 6) | Predicted | What was actually done |
|---|---|---|
| **T6.1 Leader disconnects / promoted away** | "Nothing to build — no Overthrow code keys off leader identity for the slave group after T4.5" | **Claim verified by grep and it holds. Nothing built.** `grep -rn "GetLeaderID\|IsPlayerInGroup\|GetPlayerGroup(" Scripts/` returns six hits: the notification target in `OVT_PlayerGroupManagerComponent:300`, a **log string** in `OVT_RecruitManagerComponent:1745`, the dialog's leader *name* in `SCR_GroupSubMenuBase:420`, and the two T4.5 membership tests (`:1743`, `:2226`). A broader `grep -rni leader Scripts/` adds only the resistance-menu *leaderboard* and comment text. **No predicate anywhere gates a recruit on who the leader is** — recruits sit in the slave group, and vanilla's `CheckForLeader` promoting `m_aPlayerIDs[0]` simply changes who commands them. Interaction with T6.3 (the leader is the one who disconnects) now behaves correctly for the first time: the ex-leader's OWN recruits are pulled out by the disconnect path, the remaining members' recruits stay put, and the promoted leader inherits command of exactly those |
| **T6.2 AI-density budget** | Add `MAX_SLAVE_AI_PER_GROUP`, default 0, warn joiner + leader; measure separately | **Built as specified.** `MAX_SLAVE_AI_PER_GROUP = 0` on the manager; `WarnIfAiBudgetExceeded` reads the slave group's agent count **before** the move, projects with `OVT_GroupRecruitTransfer.ProjectSlaveAiCount(before, moved)` and tests `ExceedsAiBudget` — the Logic-tier-pinned arithmetic, not a fresh comparison. Two `GroupAiBudgetExceeded` text notifications (joiner + leader) plus one `LogLevel.WARNING`. **The join is never refused and no recruit is ever dropped.** The measurement is the user's and is written up below |
| **T6.3 Offline owner's recruits** | Not commandable; pull-out on removal; liveness from `PlayerManager` | **The real gap Phase 4 found is closed.** See the decision below. A disconnect now reaches the pull-out via a one-frame persistent-id cache filled from `m_OnPlayerDisconnected` (fires at `OVT_OverthrowGameMode.c:832`, one line before `ClearPlayerIdMappings` at `:836` and two before `super` at `:838`). Resulting behaviour matches the design exactly: the departed owner is offline ⇒ `SelectTransferable` transfers nothing anywhere, and `RemoveRecruitsFromGroup` takes them out of the group they were in ⇒ **they are in no slave group and commandable by nobody**. Bodies then follow the existing BUG-086 offline flow untouched |
| **T6.4 Group emptied/deleted mid-join** | Vanilla re-resolves + T2.3 returns `previousGroupID`; add the "no longer exists" message | **Already closed by Phases 2 and 5 — nothing added, deliberately.** T2.3's override returns `previousGroupID` on a **null** group as well as a full one (`Scripts/Game/Modded/SCR_GroupsManagerComponent.c`), and Phase 5 shipped `OVT-PlayerGroups_JoinBlockedGone` at **three** call sites in `SCR_GroupSubMenuBase` (`:104` join, `:150` accept, `:324` the confirm handler's re-resolve). Adding a second message would have been a duplicate |
| **T6.5 Join/recruit/leave in quick succession** | Guard both transfer methods against a null `FindRecruitEntity`, log at WARNING | **Already done by Phase 4 — verified, not duplicated.** `MoveRecruitsToGroup` (`:1836-1842`) logs `LogLevel.WARNING` and `continue`s; `RemoveRecruitsFromGroup` (`:1932-1949`) `continue`s on a null entity, a null `AIControlComponent`, a null agent **and** on an agent whose parent group is not this slave group. Nothing is dereferenced on any of those paths. See the decision below for why the removal side stays silent |
| **T6.6 Server-rejected approve surfaces a message** | Notification to the requester when a confirm produces no membership change | **Built, at the seam Phase 2 already owns.** The check went into the existing `modded SCR_PlayerControllerGroupComponent.RequestJoinGroup` override rather than the reactor — see the decision below |
| **T6.7 `FindRecruitEntity` mid-iteration hazard** | Snapshot ids before iterating; do NOT fix the underlying bug here | **Already done by Phase 4 — verified.** `MoveRecruitsToGroup` iterates `SelectTransferable`'s fresh array; `RemoveRecruitsFromGroup` copies `m_mRecruitsByOwner[owner]` into a local array first (`:1920-1927`). Neither holds a manager-owned collection open across a `FindRecruitEntity` call. The underlying `:1587` hazard is untouched and still belongs to `resistance/recruits` |

### Decision: the disconnect fix CACHES the persistent id rather than pulling recruits out early (2026-08-06, Phase 6, T6.3)

Two ways to make a disconnect reach the pull-out, both hanging off `m_OnPlayerDisconnected` (`OVT_OverthrowGameMode.c:832`), which is the last moment the persistent id exists:

1. **Do the pull-out right there.** Rejected. At `:832` the removal has not happened yet — the player is still a group member, vanilla has not yet decided the group is empty, and `SCR_AIGroup.OnPlayerDisconnected` (`Entities/SCR_AIGroup.c:1175`, subscribed to the game mode's disconnect invoker at `:1416`) has not run. It would have created a **second** pull-out site with different preconditions from the one every other removal uses, and left the two to drift.
2. **Cache the id for one frame and let the existing site consume it.** Chosen. `OnPlayerDisconnecting` stores `playerId -> persistentId` in `m_mDepartingPersistentIds`; `MoveOwnedRecruitsOut` falls back to that map when `GetPersistentIDFromPlayerID` comes back empty. The pull-out still happens exactly where it always did — synchronously inside the removal frame, while the ex-group's slave group is still readable (deletion is only *queued*, drained next frame).

**The cache is bounded and cannot leak:** `CallLater(ForgetDepartingPlayer, 0, ...)` drops the entry next frame whether or not a removal ever arrived (a player who disconnects while in no group produces none). It holds at most the players disconnecting in a single frame.

**Where the hook is installed matters.** `InstallDisconnectHook()` is attempted in `OnPostInit` **and** again in `EOnInit` behind an installed flag, because `OVT_Global.GetPlayers()` resolves a sibling component on the same game-mode entity and the init order between them is not something this feature can bet on — the same belt-and-braces `OVT_RecruitManagerComponent` uses (`:112` and `:152`). Getting this wrong is a silently dead fix. `EOnInit` needs `SetEventMask(owner, EntityEvent.INIT)`, now set in `OnPostInit`.

**What a reviewer should scrutinise:** the ordering claim. It rests on `OVT_OverthrowGameMode.OnPlayerDisconnected` invoking `m_OnPlayerDisconnected` **before** `ClearPlayerIdMappings` and before `super`. If those three lines are ever reordered, the fallback silently goes back to being empty and the WARNING log line returns. It is on the vanilla-update checklist below (as an Overthrow-update item).

### Decision: T6.6's check went into the Phase 2 `RequestJoinGroup` override, NOT into the reactor (2026-08-06, Phase 6)

The plan says "send the requester a 'group is full' notification **from the reactor** when a confirm produces no membership change". The reactor cannot see it: the reactor is subscribed to `SCR_AIGroup`'s membership invokers, and a confirm that changes nothing fires **neither** of them. There is no event to react to.

The seam that does see it is the one Phase 2 already owns. `RPC_ConfirmJoinPrivateGroup` (`Groups/SCR_PlayerControllerGroupComponent.c:544`, `RplRcver.Server`) ends in `playerComponent.RequestJoinGroup(groupID)` at `:560` — **on the requester's own component, from server code** — which is precisely the method Overthrow already overrides for D3. So the server branch now reads the outcome back: `RPC_AskJoinGroup` assigns `m_iGroupID` synchronously (`:889-895`), so `GetGroupID() != groupID` immediately afterwards means the join was refused, and `OvtNotifyJoinRefused` sends `GroupJoinRefusedFull` to that one player when the target group is `IsFull()`.

Deliberate limits on it:
- **Only "full" gets a notification.** A group that has *vanished* logs at WARNING and says nothing, because the client-side pre-check already covers that case with a dialog (`JoinBlockedGone`) and this same method is also reached by internal server-side callers (the kick path, `RejoinPlayer`) whose target may legitimately have been replaced. Any other refusal (faction, role, rank) also logs at WARNING — vanilla's `RPC_AskJoinGroup` gives no reason back, so inventing a message would be guessing.
- **No new RPC, no new component, nothing on `OVT_PlayerCommsComponent`.** `SendTextNotification(tag, playerId)` targets one player over the existing broadcast-plus-filter path.
- **Client behaviour is byte-identical** — the whole check is inside the `Replication.IsServer()` branch.

### Decision: the recruit **pull-out** stays silent on a bodiless recruit while the move-in warns (2026-08-06, Phase 6, T6.5)

Asymmetric on purpose, and it looks like an omission. On the way **in**, a recruit whose record claims a body the world does not have is a genuine anomaly (record and world disagree) and is worth a `LogLevel.WARNING`. On the way **out**, the same shape is *routine*: the loop walks every recruit the owner has ever had, including ones that are legitimately offline, dead or mid-respawn, and none of them are in the slave group to be removed. Logging there would put a per-recruit line in a per-membership-change path for the normal case — exactly the noise the Phase 6 hardening sweep exists to remove. Both paths are equally null-safe; only the reporting differs.

### Phase 1 verdict — the vanilla Group Menu

**Method note.** The planned two-client instrumented play-test (T1.1-T1.3) **was not run** — it needs two client processes on a dedicated server and a human. No instrumentation was written and no code was changed. What follows is a **static code-reading verdict** against the real vanilla sources in `/mnt/n/Projects/Arma 4/ArmaReforger`, cross-checked against the server-log evidence already recorded in `docs/bugs/BUG-088.md`. Every line number below was re-read; corrections to the plan are called out.

#### Verdict: no residual fault found in the listing/selection path. The Group tab is expected to work post-BUG-088.

Traced end to end for a dedicated-server client whose local player is alone in their own FIA group:

1. **Faction resolution.** `SCR_GroupSubMenuBase.UpdateGroups()` (`Groups/SCR_GroupSubMenuBase.c:40`) resolves `SCR_Faction.Cast(factionManager.GetLocalPlayerFaction())` at **`:46`** (not `GetPlayerFaction`, as the plan says) → `GetPlayerFaction(SCR_PlayerController.GetLocalPlayerId())` (`GameMode/FactionManager/SCR_FactionManager.c:288`, `:243`), which reads the `m_MappedPlayerFactionInfo` cache — **the map that BUG-088 was never replicating.** With the `RplComponent` now on `OVT_OverthrowFactionManager.et` this returns FIA on the client, and the two early returns at `:44`/`:48` (the whole reason the tab was inert) no longer fire.
2. **No faction-key mismatch — the plan's prime suspect is wrong.** `OVT_SpawnLogic.SetCivilianFaction` is a misnomer: it assigns `OVT_Global.GetConfig().GetPlayerFaction()`, i.e. `m_sPlayerFaction`, whose default is **`FIA`** (`OVT_OverthrowConfigComponent.c:75-76`), not CIV. `CreateAndJoinGroup` keys the group off `factionManager.GetPlayerFaction(playerId)` (`OVT_SpawnLogic.c:863`) and passes it to `CreateNewPlayableGroup(faction)` (`:870`) → `group.SetFaction(faction)` (`SCR_GroupsManagerComponent.c:1249`). Both sides therefore hold the **same `Faction` instance**, which is what `m_mPlayableGroups` is keyed by (`RegisterGroup`, `:1049-1067`). The server log in BUG-088 confirms it: `Group faction: FIA`.
3. **Client-side registration.** Groups only enter a client's `m_mPlayableGroups` through `SCR_AIGroup.RplLoad` → `groupsManager.RegisterGroup(this)` (`Entities/SCR_AIGroup.c:2763-2772`), gated on `m_bPlayable`, after `BroadCastSetFaction(factionIndex)` has set the key (`:2749-2750`). This requires the group prefab to keep both its `RplComponent` and `m_bPlayable 1` — see the prefab finding below. Vanilla Conflict drives group creation through the identical `CreateNewPlayableGroup` path, so the mechanism itself is proven in MP.
4. **Tiles.** `UpdateGroups:84-106` creates one `SCR_GroupTileButton` per entry of `GetSortedPlayableGroupsByFaction` (`SCR_GroupsManagerComponent.c:503`, which filters on **nothing** — it is a plain `m_mPlayableGroups.Get(faction)` re-sorted by group id, `:506-525`). Zero tiles has exactly two causes: a null local faction (fixed) or an empty per-faction array (i.e. registration never happened).
5. **Join button.** `SetSelectedGroupButtonStatus(int)` (**`:331`**, plan correct) sets `m_JoinGroupButton.SetVisible(selectedGroupId > -1 && !HasInviteFromGroup(...))` and is driven only by `GetOnSetSelectedGroupID` (subscribed `:169`), fired from `SetSelectedGroupID` (`SCR_PlayerControllerGroupComponent.c:1084-1090`), called from `SCR_GroupTileButton.OnClick:205` and `OnFocus:1055-1061`. **No tile ⇒ no selection ⇒ the Join button can never appear** — which is the whole of the reported "can't manage the Group menu", and it is downstream of the faction, not a separate defect. Final say on visibility is `SCR_GroupTileButton.SetupJoinGroupButton` (`:705-765`) → `CanPlayerJoinGroup` (`SCR_PlayerControllerGroupComponent.c:136-245`): same faction, not full, not your own group. All satisfied for another player's group.
6. **`SCR_GroupSubMenuPlayerlist.UpdateGroups` (`:46-52`)** — the override the player-list route runs — is `super.UpdateGroups()` plus `UpdateGroupSettingsButtons()`. It adds no filtering and cannot suppress tiles.

#### The `PlayableGroup.et` suspect is **eliminated** — and `persistence/tasks.md` is wrong about it

`Prefabs/Groups/PlayableGroup.et` in this repo is 3 lines (`SCR_AIGroup { ID "56FC873F6D4EEACC" }`) overriding vanilla's GUID `{8B4D49A9F324E7D5}`. `docs/features/core/persistence/tasks.md:59` and `:176` read that as a full replacement that strips `RplComponent`, `Persistence`, `AIFormationComponent`, `SCR_CallsignGroupComponent`, `m_bPlayable 1`, `m_iMaxMembers 6`, `m_bDeleteWhenEmpty 0` and `m_iGroupRadioFrequency 52000`. **That is not how same-GUID addon files behave — they are deltas merged onto the base resource.** Three independent proofs:

- `Prefabs/Characters/Core/Character_Base.et` in this repo is **4 lines with an empty `components { }`** overriding vanilla's 1,258-line prefab. Under replacement semantics every character in Overthrow would have no components at all.
- `Configs/System/chimeraInputCommon.conf` here is 917 lines of Overthrow-only actions overriding vanilla's 9,583. Under replacement semantics the game would have no keybindings — including `GroupMenuContext` / `MenuJoinGroup` / `GroupAcceptInvite` (vanilla `:8775-8780`, `:4358`, `:4374`, `:445`), which the Group tab activates every frame (`SCR_GroupSubMenuPlayerlist.OnMenuUpdate:11`).
- Runtime proof specific to this prefab: `CreateNewPlayableGroup` returns **null** at `SCR_GroupsManagerComponent.c:1275-1277` if the group has no `RplComponent` whenever a `SCR_CommandingManagerComponent` exists — and Overthrow's game mode has one (`Prefabs/GameMode/OVT_OverthrowGameMode.et:232`). BUG-088's dedicated-server log shows `Created group 1001 … Group faction: FIA`, i.e. a non-null return, and recruits demonstrably reach `group.GetSlave()`. The `RplComponent` is present at runtime, therefore so is everything else the file does not mention, including `m_bPlayable 1`.

So the group prefab is effectively vanilla, the empty override is a Workbench artefact, and step 3 above holds. **Action:** correct `docs/features/core/persistence/tasks.md:59`/`:176` (out of scope for this feature; note only).

#### Confidence

**High** that the *listing* path is sound (every step read; the only step not verifiable by reading is the timing of the initial `RplSave` snapshot relative to `SetFaction`/`AssignGroupID` inside the same frame — and vanilla's own group creation depends on exactly that ordering in shipped MP modes).
**High** that the *join-completion* path still has the two defects already recorded as D3 and D4 — both re-verified below.

#### Two faults that remain, both already planned, plus one new one

- **D3 confirmed, with a line correction.** `RPC_ConfirmJoinPrivateGroup` (`SCR_PlayerControllerGroupComponent.c:544`, `RplRcver.Server`, so server-side) calls `playerComponent.RequestJoinGroup(...)` at **`:560`** (the plan says `:562`; that line is `RemoveRequester`). `RequestJoinGroup` (`:830-833`) is `Rpc(RPC_AskJoinGroup, …)`, an `RplRcver.Server` RPC issued *by the authority*, on **another player's** component — it goes nowhere. Additional hazard the plan does not mention: `playerComponent` is **not null-checked** at `:558-560`, so a requester who disconnects between request and approval can null-deref the server. T2.2's wrapper override fixes the first; add the null guard note.
- **D4 confirmed exactly as written.** `MovePlayerToGroup` (`SCR_GroupsManagerComponent.c:109-134`) removes at `:114` before testing `IsFull()` at `:119` and returns `-1`; `RPC_AskJoinGroup:889-895` turns that into `m_iGroupID = -1`.
- **NEW — group names are never replicated and never displayed.** `OVT_SpawnLogic.c:878` calls `newGroup.SetName(playerName)`. `SCR_AIGroup` has **no** `SetName`; that is the engine's entity-name setter — server-local, and not read by any UI. The Group tab draws `SCR_GroupHelperUI.GetTranslatedGroupName` (`UI/Menu/SCR_GroupHelperUI.c:9-24`), which uses the **callsign** plus `group.GetCustomName()`. So today every tile reads "Alpha-1"-style callsigns on every machine, and requirement **F1** ("a group named after themselves") is currently false even in solo. The replicating setter is `SCR_AIGroup.SetCustomName(name, authorID)` (`Entities/SCR_AIGroup.c:744-747`: local call + `Rpc` broadcast). **Amend T2.1/T3.2 to call `SetCustomName(playerName, playerId)`** (keep or drop `SetName`; it is harmless debug naming).

#### Two smaller findings for the phases that follow

- **The private branch is not live yet.** `JoinSelectedGroup` branches on `group.IsPrivate()` (`SCR_GroupSubMenuBase.c:267`); groups are public until T2.1 sets the flag, so *today* joining takes the public branch `RequestJoinGroup` — a normal client→server RPC that works. The moment **T2.1** lands, the private branch (request → leader approves → the broken `:560` call) becomes the only route. **T2.1 and T2.2 must ship in the same change**, or joining will regress from working to silently doing nothing.
- **T2.1's "does the attributes UI degrade gracefully with `IsPrivacyChangeable()` false?" is answerable statically: yes.** `SCR_GroupTileButton.CheckLeaderOptions` (`:1220-1228`) hides *and* disables both the privacy checker and the rename button when `!group.IsPrivacyChangeable()`. Note the side effect: **the rename button goes with it**, so leaders lose group renaming — acceptable under the Overthrow model (the group is named after you) but worth stating. `SetPrivacyChangeable(false)` is still needed for its second job: `OnGroupPlayerRemoved` auto-unlocks at `SCR_GroupsManagerComponent.c:748-749` (group emptied, `m_bDeleteWhenEmpty 0` on the prefab means this branch is the live one) and `:759-760` (faction's last group).
- **Expect a stray empty FIA group in the tab.** `SCR_Faction.m_bEnableAutoGroupCreationWhenFull` defaults to `1` (`Faction/SCR_Faction.c:75-76`) and vanilla `Configs/Factions/FIA.conf` does not override it, so `SCR_GroupsManagerComponent.OnPlayerFactionChanged:1112-1114` creates a playable FIA group when the first player is assigned the faction — *before* Overthrow creates its own at `OVT_SpawnLogic.c:870`, and it joins nobody to it. That stray is a second, independent reason `CanCreateNewGroup` stays false (`TryFindEmptyGroup`, `:1347`). Cosmetic; do not fix in this feature.

#### Ruled out

| Candidate | Why it is not the fault |
|---|---|
| Faction-key mismatch between the group and the menu (plan's prime suspect) | Both are the same `Faction` instance — `m_sPlayerFaction` = `FIA`, group created with `GetPlayerFaction(playerId)`; server log says `Group faction: FIA` |
| `GetSortedPlayableGroupsByFaction` filtering Overthrow's groups out | It filters on nothing (`SCR_GroupsManagerComponent.c:503-525`); a map lookup plus a sort |
| Overthrow's group-creation route not registering the group as playable | It uses vanilla `CreateNewPlayableGroup` (`OVT_SpawnLogic.c:870`), which calls `RegisterGroup` at `:1250` |
| `Prefabs/Groups/PlayableGroup.et` stripping `RplComponent` / `m_bPlayable` | Same-GUID addon files are deltas, not replacements — three proofs above; the prefab is effectively vanilla |
| A subclass overriding the join seams | Only two subclasses exist (`SCR_GroupSubMenu`, `SCR_GroupSubMenuPlayerlist`); neither overrides `JoinSelectedGroup` or `AcceptInvite` |
| Overthrow modding the group stack anywhere | Zero hits: no `modded class` on any `SCR_Group*` / `SCR_AIGroup` / group menu, no layout override, no input-context override for the tab |
| Missing `GroupMenuContext` keybindings | Present in vanilla (`:8775-8780`) and Overthrow's input `.conf` is a delta that does not touch them |
| `AcceptInvite` broken in MP | `SCR_PlayerControllerGroupComponent.AcceptInvite:434-457` runs client-side and ends in a normal client→server `RequestJoinGroup` — correct direction |
| **D11 — "Create new group" greyed out** | **Vanilla by design; explicitly OUT OF SCOPE.** `CanCreateNewGroup` (`SCR_GroupsManagerComponent.c:1319`) returns false when you are the last player in your group (`:1344`) and when an empty faction group exists (`:1347`). Overthrow's steady state satisfies both permanently. Not investigated further, not changed |

#### Impact on Phase 5 — the seam did **not** move

Re-verified against the real files: `SCR_GroupSubMenuBase.JoinSelectedGroup()` is at **`:259`** and `AcceptInvite()` at **`:285`** (both exact). They are the only callers of the join/accept APIs anywhere in the vanilla script tree (`Groups/SCR_GroupSubMenuBase.c:287` is the sole `.AcceptInvite()` call site; `:359` is the sole `JoinSelectedGroup` wiring). `SCR_GroupSubMenuPlayerlist` overrides `OnMenuUpdate`, `OnTabCreate`, `OnTabShow`, `OnTabHide`, `UpdateGroups` only; `SCR_GroupSubMenu` overrides `OnTabCreate`, `OnTabShow`, `OnTabHide` only. **One `modded class SCR_GroupSubMenuBase` still covers every route; Phase 5's task list needs no amendment** beyond the T5.1 note that the client-side full-group pre-check should reuse `CanPlayerJoinGroup` (`SCR_PlayerControllerGroupComponent.c:136`), which already covers full / wrong-faction / own-group in one call.

Amendments owed elsewhere: **T2.1** (use `SetCustomName`, and ship with T2.2), **T3.2** (same), and a null guard note on **T2.2**.

---

## Gotchas & Learnings

### Vanilla has no "leave group" action (2026-08-06, Phase 3)

Searched the whole vanilla script tree: no `LeaveGroup`, no `RequestLeaveGroup`, no leave button anywhere in `Game/Groups/`. The four ways out of a group are: join another one, **create a new one** (`SCR_GroupSubMenuBase.CreateNewGroup:238` → `RequestCreateGroup:254`), be kicked by the leader (`RPC_AskKickPlayer:760`), or disconnect. Every mention of "the vanilla Leave action" in this feature's docs means *Create new group*. Phase 5 should consider whether Overthrow wants a first-class "Leave group" affordance, since the one it inherits is called something else and is gated by `CanCreateNewGroup`.

### `SCR_PlayerControllerGroupComponent.m_iGroupID` is NOT maintained by `RemovePlayer` (2026-08-06, Phase 3)

`SCR_AIGroup.RemovePlayer` (`:1526`) updates the *group's* member list and nothing else. The player-controller's `m_iGroupID` is only ever written in four places: `RPC_AskJoinGroup:892`, `RpcAsk_RemovePlayer:923` (editor-only), `ResetGroupIDs_S`/`RPC_DoResetGroupIDs:840` (faction change), and the owner-side `RPC_DoChangeGroupID:849`. Two consequences the deferred restore depends on:
- during `MovePlayerToGroup`, **both** invokers fire while `m_iGroupID` still holds the OLD group — which is exactly why the restore has to be deferred and re-read rather than reading the id inside the handler;
- on disconnect nothing resets it, so `RestoreOwnGroupDeferred` is double-guarded (no `PlayerController` *and* a non-`-1` id).

### `m_bDeleteWhenEmpty` and `m_bDeleteIfNoPlayer` are different fields (2026-08-06, Phase 3)

`Prefabs/Groups/PlayableGroup.et` sets `m_bDeleteWhenEmpty 0` — that is the **agent**-count field (`SCR_AIGroup.c:330`, `:2385`). The one `SCR_GroupsManagerComponent.OnGroupPlayerRemoved:746` reads is `GetDeleteIfNoPlayer()` → `m_bDeleteIfNoPlayer`, whose attribute default is **1** (`:140-141`) and which the prefab does not touch. So an emptied Overthrow player group **is** queued for deletion (unless it is the faction's last one), which is why leaving a group needs `EnsureOwnGroup` to build a new one and why the deletion-queue guard on group adoption matters.

### A kicked player does not get their own group back (2026-08-06, Phase 3) — unhandled, Phase 6

`RPC_AskKickPlayer` (`:760-789`) moves the kicked player into a RESERVES group, else `GetFirstNotFullForFaction(faction, group, respectPrivate: true)`, else a fresh group — then joins them with `RequestJoinGroup`, which T2.2 just made work server-side. `respectPrivate: true` skips every Overthrow player group, so the target is a fresh vanilla group: public and unnamed, and the `OnGroupPlayerAdded` claim rule brands it. Net effect is acceptable (they are never groupless, and they end up in a private group named after them), but it has **not been play-tested** and is not in any phase's acceptance criteria.

### The AI-member removal broadcast fires TWICE, in vanilla too, and is idempotent (2026-08-06, Phase 4)

`RequestSetGroupSlave` → `RPC_DoSetGroupSlave` (`Groups/SCR_GroupsManagerComponent.c:1506`) subscribes `OnAIMemberRemoved` to the **slave** group's `GetOnAgentRemoved()`, and `SCR_AIGroup.OnAgentRemoved` (`Entities/SCR_AIGroup.c:2403-2407`) invokes it from the engine. So `slaveGroup.RemoveAgentFromControlledEntity(entity)` already ends in `AskRemoveAiMemberFromGroup` by itself — and vanilla's own `RemoveAiFromSlaveGroup` calls it a second time explicitly anyway. Overthrow's pull-out copies that shape, so the behaviour is byte-identical to vanilla's dismiss path. Harmless: `RPC_DoRemoveAIMemberFromGroup` is `SetRecruited(false)` plus a `RemoveItem` that no-ops on a member that is already gone. Worth knowing before anyone "optimises" the explicit call away — the explicit, **synchronous** call is what guarantees the removal is ordered before the re-add on a same-frame group switch; the engine callback's timing is not ours to rely on.

Note the asymmetry it comes from: `RPC_DoSetGroupSlave` subscribes a removal handler and no addition handler, which is why the add path has always had to broadcast explicitly.

### Pulling recruits out is also a slave-group LEAK fix (2026-08-06, Phase 4)

`UnregisterGroup` deletes a deleted group's slave **only** when the slave has no agents left (`Groups/SCR_GroupsManagerComponent.c:1035-1040`), with vanilla's own `//mourTodo: handle what the AIs should do in case their master group is deleted` sitting beside it. Before Phase 4, a player leaving or disconnecting from a group they were alone in left their recruits inside a slave group whose master was destroyed the next frame: a leaked group entity holding live AI with nothing to command them. The synchronous pull-out empties the slave first, so vanilla deletes it cleanly.

### `AIAgent.GetParentGroup()` is the membership test, not `GetAIMembers()` (2026-08-06, Phase 4)

`RemoveRecruitsFromGroup` only removes a recruit whose agent's parent group **is** this slave group. Two candidates existed: `SCR_AIGroup.IsAIControlledCharacterMember()` (`Entities/SCR_AIGroup.c:2659-2667`), which reads the replicated `m_aAIMembers` list maintained by the add/remove RPCs, and the engine's agent hierarchy. The hierarchy wins: it is what `RemoveAgent` actually operates on, while `m_aAIMembers` is bookkeeping for the UI and can only ever be a shadow of it.

### The Group tab is reached from the PAUSE menu, not the player list (2026-08-06, Phase 5)

Several places in this feature's docs say "player list → Groups". That is not where it is. `ChimeraMenuPreset.GroupMenu` (`chimeraMenus.conf:286`) opens `UI/layouts/Menus/GroupSlection/GroupMenu.layout`, whose first tab is `GroupMenuPlayerlist.layout` (`SCR_GroupSubMenuPlayerlist`) and whose second is the leader's Requests tab. `UI/layouts/Menus/PlayerList/PlayerListMenu.layout` has exactly one tab ("All") and no group sub-menu at all. The action that opens it is **`ShowGroupMenu` = `KC_P` / gamepad `view` + `x`**. `UI/layouts/Menus/PlayerList/Playerlist_GroupMenu.layout` exists but is referenced by nothing — dead vanilla file, do not chase it.

### A dialog force-disables the input buttons of the menu underneath it (2026-08-06, Phase 5)

`SCR_InputButtonComponent.OnDialogOpen` (`:1128-1133`) calls `SetForceDisabled(!IsInTopMenu())` for every input button in the game, and `OnInput()` additionally bails on `!IsParentMenuFocused()` (`:735`). So while the confirmation dialog is up, the Group tab's own Join / Accept / Add-group buttons are inert — pressing pad `y` again cannot open a second dialog or fire a second request. **Two independent guards, both vanilla.** Worth knowing before anyone adds a manual "is a dialog open?" flag.

### `MenuJoinGroup` and `GroupAcceptInvite` share gamepad `y`, and that is fine (2026-08-06, Phase 5)

`MenuJoinGroup` is `KC_J` / `gamepad0:y` **click**; `GroupAcceptInvite` is `KC_U` / `gamepad0:y` **hold** (`Configs/System/chimeraInputCommon.conf:4374`, `:445`). Both live in `GroupMenuContext` (`:8775-8783`). They are additionally mutually exclusive by visibility: `SetSelectedGroupButtonStatus` (`SCR_GroupSubMenuBase.c:331-339`) hides Join whenever `HasInviteFromGroup(selectedGroupId)`, and `SetAcceptButtonStatus` hides Accept whenever it is false — and a hidden `SCR_InputButtonComponent` does not respond to its action. Overthrow's input `.conf` is a delta that touches **none** of these actions and does not define `GroupMenuContext`, so nothing here is Overthrow's to break. The conflict checker exits 0 and reports no `GroupMenuContext` row.

### `CreateAndJoinGroup`'s retry ladder can still loop unbounded (pre-existing, untouched)

`CreateAndJoinGroup` schedules `CreateAndJoinGroupDelayed(playerId, 0)` — always retry count **0** — so the "max 10" cap in `CreateAndJoinGroupDelayed` only ever counts consecutive *missing player controller* attempts and is reset every time the ladder re-enters `CreateAndJoinGroup`. Phase 3 deliberately kept the ladder byte-identical (it is what T3.2 asked for) and made sure `EnsureOwnGroup` returning `-1` **does not** re-arm it, so no new loop was introduced. Same family as the uncapped `RespawnRecruitsDelayed` loop that T4.5 fixes; worth folding into Phase 6.

---

## Vanilla-update check-list (R10)

Three `modded class` overrides mirror vanilla code and will break silently if a Reforger update rewrites it. Re-check these after every game update:

| Override | Mirrors | Why |
|---|---|---|
| `SCR_GroupSubMenuBase.JoinSelectedGroup()` / `AcceptInvite()` | `Groups/SCR_GroupSubMenuBase.c:259`, `:285` | the only join / invite-accept seams |
| `SCR_PlayerControllerGroupComponent.RequestJoinGroup()` | `Groups/SCR_PlayerControllerGroupComponent.c:830` | D3 — server-side callers marshalling a `RplRcver.Server` RPC |
| `SCR_GroupsManagerComponent.MovePlayerToGroup()` | `Groups/SCR_GroupsManagerComponent.c:109` | D4 — full-group hole strands a player at `GetGroupID() == -1` |
| `SCR_GroupsManagerComponent.IsGroupQueuedForDeletionOVT()` | `Groups/SCR_GroupsManagerComponent.c:63` (`m_aDeletionQueue`) | Phase 3 — reads a protected vanilla field; a rename breaks the empty-group adoption guard |
| `OVT_PlayerGroupManagerComponent.OnGroupPlayerRemoved/Added` | `Entities/SCR_AIGroup.c:1277-1281`, `:1496-1504`, `:1401-1409`, `:1526-1543` | the whole reactor: local-call-then-broadcast ordering, and `m_aPlayerIDs` being updated before the invoker fires |
| `OVT_PlayerGroupManagerComponent.RestoreOwnGroupDeferred` | `Groups/SCR_GroupsManagerComponent.c:109-134` | the one-frame deferral assumes remove-then-add **in the same frame** |
| deletion timing (`DeleteGroupDelayed` → `DeleteGroups`) | `Groups/SCR_GroupsManagerComponent.c:721-730`, `:1647-1654` | if deletion ever becomes synchronous, Phase 4's recruit pull-out loses its window |
| `OVT_RecruitManagerComponent.RemoveRecruitsFromGroup` | `Groups/SCR_PlayerControllerGroupComponent.c:1526-1552` (`RemoveAiFromSlaveGroup`) | Phase 4 — the pull-out is that method inlined minus its player-controller dependency: `Deactivate()` on the last agent, `RemoveAgentFromControlledEntity`, `AskRemoveAiMemberFromGroup`. If vanilla changes those three steps, change these |
| `OVT_RecruitManagerComponent.MoveRecruitsToGroup` | `Groups/SCR_PlayerControllerGroupComponent.c:1470-1495` (`AddAIToSlaveGroup`) | Phase 4 — assumes `AddAIToSlaveGroup` still re-activates the slave group and broadcasts membership itself |
| membership test in `AddRecruitToPlayerGroup` / `RespawnRecruitsDelayed` | `Entities/SCR_AIGroup.c:487-490` (`IsPlayerInGroup`) | Phase 4 / D8 — if this ever stops being a plain `m_aPlayerIDs.Contains()`, recruiting inside a shared group is what breaks |
| `m_bAllowRejoinPlayerAfterReconnecting 0` on the game-mode prefab | `Groups/SCR_GroupsManagerComponent.c:34-35`, `:1135-1136` | D2 — if vanilla renames or drops the attribute, reconnect silently starts putting players back in a friend's group |
| `SCR_GroupSubMenuBase` overrides reading `GetSelectedGroupID()` | `Groups/SCR_GroupSubMenuBase.c:262`, `:288`; `SCR_PlayerControllerGroupComponent.c:436` | Phase 5 — both vanilla joins act on the **selected** group, not an argument. If either ever takes a group parameter, the confirm handler's selection-restore becomes wrong rather than merely unnecessary |
| `SCR_GroupSubMenuBase.OnTabCreate` + the `GrouplistFooter` widget | `GroupMenuPlayerlist.layout:143`, `SelectGroupMenu.layout:132` | Phase 5 — the explanatory paragraph hangs off an empty vanilla container. A rename silently drops the text (the tab still works) |
| `SCR_PlayerControllerGroupComponent.CanPlayerJoinGroup` | `Groups/SCR_PlayerControllerGroupComponent.c:136-245` | Phase 5 — the client-side pre-check is vanilla's own predicate. If it stops covering "full", the specific message survives (`IsFull()` is tested separately) but the generic one loses its meaning |
| `SCR_InputButtonComponent`'s dialog/focus guards | `UI/Components/WidgetLibrary/SCR_InputButton/SCR_InputButtonComponent.c:735`, `:1128-1133` | Phase 5 — what stops the Join shortcut re-firing while the confirmation dialog is open. If these go, add an explicit "dialog pending" guard |
| vanilla dialog-button label keys | `#AR-Button_Confirm-UC`, `#AR-Workshop_ButtonCancel`, `#AR-Menu_OK` | Phase 5 — the three new presets borrow vanilla labels; a removed key shows as a raw `#AR-...` on a button |
| `SCR_AIGroup.OnPlayerDisconnected` → `RemovePlayer` | `Entities/SCR_AIGroup.c:1175-1187`, subscribed `:1416` | Phase 6 / T6.3 — if a disconnect ever stops going through `RemovePlayer`, the disconnect pull-out stops firing and a departed player's recruits stay under the ex-leader's command again |
| **Overthrow-internal, not vanilla:** the three lines of `OVT_OverthrowGameMode.OnPlayerDisconnected` | `Scripts/Game/GameMode/OVT_OverthrowGameMode.c:832` invoke → `:836` `ClearPlayerIdMappings` → `:838` `super` | Phase 6 / T6.3 — the disconnect id cache exists **only** because the invoke comes first. Reorder those lines and the pull-out silently reverts to the Phase 4 gap (the WARNING log line comes back) |

Plus one *deliberate non-override* to re-check: `RPC_ConfirmJoinPrivateGroup` (`Groups/SCR_PlayerControllerGroupComponent.c:544`) dereferences `playerComponent` at `:560` without a null test. It is currently unreachable only because `:554-556` returns on a null `PlayerController`. **If that guard ever disappears, the null hole becomes live** — see the decision above.

---

## Testing Approach

- **Logic tier** ✅ delivered — `OVT_TEST_Logic_GroupRecruits`, **5 cases**, in the Fast and All groups: empty owner, all-online-in-table-order, mixed online/offline with an exact skipped count, offline owner (with an online-owner control in the same case so it cannot pass for the wrong reason), and the budget arithmetic. Three separate deliberate faults were needed to make all five fail, which is the evidence they are not restating one claim. Method recorded in the suite preamble. No `maxAttempts`.
- **What the Logic tier canNOT reach, and why it matters here:** everything that makes a recruit actually move. `AddAIToSlaveGroup`, `RemoveAgentFromControlledEntity`, the membership broadcast and the same-frame remove-then-add ordering all need a live world and a second client. The transfer methods are covered by **code trace plus a log line each** — `Pulled N ... / Moved N ...` — and the play-test is what closes them.
- **Init tier** — `OVT_PlayerGroupManagerComponent.GetInstance()` resolves after manager init.
- **Everything else is manual** — join/approve/invite/accept, the dialog, recruit follow and leader commanding, disconnect/reconnect and AI density all need two client processes and a live world. The 16 numbered steps in `implementation.md` §6 are the procedure.
- **Phase 5 added NO automated coverage, and that is not an oversight.** `.layout`, `.layout.meta`, `.conf` and the string table are parsed by neither `compile-check.sh` nor any test suite — UI is explicitly outside this project's automated spine, and a menu needs a workspace, a focused menu stack and an input device that the autotest world does not have. The All group stayed at **66** on purpose. What Phase 5 *did* verify mechanically: the scripts compile, no test regressed, the input-conflict checker exits 0 (and reports nothing in `GroupMenuContext`), the new layout has no duplicate widget GUIDs, and `git diff --stat Language/` shows only the `.st`. Everything about how the screen looks and behaves is in "Needs human verification".

---

## Needs human verification

> ✅ **ALL DISCHARGED 2026-08-06** — the user ran the dedicated-server MP play-test and reported every test passing with no problem found, explicitly including the leader-disconnect promotion case (F11). The list below is retained as the record of what was to be checked, not as outstanding work. The only items never exercised are the two unexported notification keys and the AI-density measurement (see Quick Status).

*(running list — items an agent cannot verify; the user runs these)*

- ⏸️ **Phase 1 confirm/refute the code-reading verdict** — two clients (A and B) on a **dedicated server**, both spawned, each open the player list → **Groups** tab. No instrumentation was shipped, so these are all naked-eye observations:
  1. **Tiles appear on both clients.** Expect **at least one tile per connected player** (plus possibly one stray empty group — see the verdict). Zero tiles on a client ⇒ the verdict is wrong and the fault is client-side registration (`SCR_AIGroup.RplLoad:2763-2772`); report the tile count on each client.
  2. **Tile contents.** Each tile shows a **callsign** ("Alpha-1"-style), a frequency, and a player count — **not** the owner's player name. That confirms the `SetName` finding (`OVT_SpawnLogic.c:878`). If names *do* appear, say so; the finding is wrong.
  3. **Selection.** B clicks A's tile → the tile highlights and the **Join / Request join** button becomes visible in the footer. If the tile is clickable but no Join button appears, capture whether the label read "Join group" or "Request to join group".
  4. **Padlock.** Expect **no** padlock on any tile today (groups are not private until Phase 2 T2.1).
  5. **Join actually works today.** B presses Join on A's tile → **B should end up in A's group on both screens** (public branch, `RequestJoinGroup`). This is the baseline Phase 2 must not regress. If it already fails *now*, the verdict is wrong and the fault is server-side in `RPC_AskJoinGroup`/`MovePlayerToGroup`.
  6. **Gamepad reachability** (cheap while you are there): d-pad/stick moves focus between tiles and the Join button fires on `A`. `GroupMenuContext` is activated every frame by `SCR_GroupSubMenuPlayerlist.OnMenuUpdate`.
  7. **Stray group check.** Count tiles versus connected players. One extra, permanently empty tile is the expected vanilla artefact (`SCR_GroupsManagerComponent.OnPlayerFactionChanged:1112-1114`); more than one extra is worth reporting.
  8. **"Create new group" is greyed out — expected (D11), do not report as a bug.**

- ⏸️ **Phase 2 gate — steps 1, 5 and 14 of `implementation.md` §6**, two clients on a **dedicated server**. Phase 2 is server-authority code with no automatable surface; these three are the only proof it works. There is no confirmation dialog yet (Phase 5), so Join goes straight through.
  1. **§6 step 1 — spawn.** Both connect and spawn. **Expect:** each player is in their own group, is its leader, AI commanding opens, and the group tile now shows the **owner's player name** (this is the `SetCustomName` fix — before Phase 2 the tile showed only a callsign) and a **padlock / private marker** (the `SetPrivate(true)` fix). Also expect the privacy toggle **and the rename button** to be gone from the group-attributes UI — that is the accepted `SetPrivacyChangeable(false)` cost, not a bug.
  5. **§6 step 5 — the D3 fix proving itself.** B selects A's group, presses Join. **Expect:** because the group is private this now takes the *request* branch — A receives a join request. A approves. **Expect: B is in A's group on both screens and on the server.** This is the whole point of T2.2: before it, on a dedicated server the approve did nothing at all. If nothing happens on approval, the `RequestJoinGroup` override is not taking effect — report the server log around the approval.
  14. **§6 step 14 — the D4 fix.** Fill a group to 6 players and have a 7th press Join and be approved. **Expect:** the 7th player **remains in their own group** and is never groupless. There is no "group is full" message yet (that is T5.1/T6.6) — for now it is a silent no-op, which is correct for this phase. **Check the server log for any live player at `groupId == -1`** — there must be none.
  - Also worth a look while there: solo/host play is unchanged (spawn → own group → commanding → recruit a civilian → recruit follows orders).

- ⏸️ **Phase 3 gate — steps 1, 9, 10 and 14 of `implementation.md` §6**, two clients on a **dedicated server**. Still no confirmation dialog (Phase 5) and still no recruit-following (Phase 4), so this round is purely about the **group lifecycle**.
  1. **§6 step 1 — spawn, and the stray-group check.** Both connect and spawn. **Expect:** each player is in their own private group named after them, is its leader, commanding opens. **New in Phase 3: count the tiles in the Group tab — there should be exactly one per connected player and NO permanently-empty extra tile.** Phase 2 expected one stray; Phase 3 adopts it. If a stray is still there, `EnsureOwnGroup`'s empty-group adoption is not running — say so, because everything in step 9 depends on it.
  9. **§6 step 9 — leaving.** B joins A's group (step 5), then leaves. **The leave route is the "Create new group" button in the Group tab** — vanilla has no separate Leave action, and that button is *expected to be enabled only while B is in a shared group* (greyed out while alone is correct, D11). **Expect:** B is immediately back in a private group of their own, **named after B**, with B as leader, and commanding works again. If the button is greyed out while B is in A's group, report it — that means an empty group is still hanging around.
  10. **§6 step 10 — disconnect / reconnect.** B rejoins A's group, then disconnects and reconnects. **Expect: B is in B's OWN group, not A's**, leader, commanding works. This is the `m_bAllowRejoinPlayerAfterReconnecting 0` change proving itself — before it (and after Phase 2's T2.2), vanilla's `RejoinPlayer` would have put B straight back into A's group.
  14. **§6 step 14 — full group.** Unchanged from Phase 2: fill a group to 6, have a 7th ask. **Expect:** the 7th stays in their own group and the server log shows no live player at `groupId == -1`.
  - **Server-log lines worth grepping** (all `[Overthrow]`): `Player <id> removed from group <n> (… slave group readable: 1)` on every leave — the `1` is Phase 4's precondition, and a `0` there is worth reporting; `Player <id> ended a frame with no group - restoring their own` on a genuine leave and **not** on a normal group switch; `adopted empty group` on the very first spawn and `created group` thereafter; `Player <id> is alone in public group <n> - claiming it as their own` when someone leaves via Create-new-group.
  - **Regression watch:** solo/host — spawn → own group → commanding → recruit a civilian → recruit follows orders. The whole group-creation body moved into a new manager, so this is the "did the refactor break single player" check.

---

- ⏸️ **Phase 4 gate — steps 2, 6, 7, 8 and 9 of `implementation.md` §6**, two clients on a **dedicated server**. This is the round where recruits start moving. Still no confirmation dialog (Phase 5), so Join goes straight through.
  2. **§6 step 2 — recruiting still works at all.** A recruits 3 civilians while alone in A's own group. **Expect:** all 3 join A's group and follow A's orders, exactly as before Phase 4. This is the regression check on the `AddRecruitToPlayerGroup` guard change — if recruiting broke, it broke here.
  6. **§6 step 6 — THE T4.5 FIX PROVING ITSELF, and the single most important observation of this phase.** B joins A's group (step 5), then **B recruits 2 civilians while inside A's group**. **Expect:** both new recruits join and follow **A's** orders. Before this change the guard demanded that B be the group's *leader*, so this silently did nothing at all — no error, no recruit, no clue. If the recruits do not appear, grep the server log for `is not a member of group`.
  7. **§6 step 7 — the leader commands somebody else's recruits.** A orders B's 2 recruits to move. **Expect:** they obey. (Vanilla slave-group behaviour; Overthrow only put them there.)
  8. **§6 step 8 — ownership did NOT move with them.** A opens the recruit roster. **Expect:** B's recruits are **not** dismissable, renameable, inventory-openable, loadout-appliable or show-on-map-able by A, and they still appear as B's on B's roster. Kill XP from B's recruits must still credit **B**.
  9. **§6 step 9 — they leave with their owner.** B leaves (the "Create new group" button, see the gotcha above). **Expect:** B's recruits leave with B, **A can no longer command any of them**, and B is back in a private group of their own with the recruits following B again.
  - **Server-log lines worth grepping** (all `[Overthrow]`), in this order for a normal switch: `Pulled N of <persId>'s recruits out of group <old>'s slave group` then `Moved N of <persId>'s recruits into group <new>`. **Both lines, same frame, same N** is the proof that a switch nets exactly one slave-group membership — that pairing is what an agent cannot verify and you can.
  - **Known and expected, do NOT report as a bug:** on a **disconnect** you will see `Player <id> left group <n> but has no persistent id any more (a disconnect clears the mapping first) ...`. That is the documented T6.3 gap (decision above), not a regression — a player who disconnects from a *shared* group leaves their recruits under the remaining leader's command until the ~10 min offline despawn. Worth confirming the log line appears, because the Phase 6 fix will be judged against it.
  - **Also worth a look:** solo/host regression — spawn → own group → recruit a civilian → recruit follows orders → quit and continue → recruits come back. The `RespawnRecruitsDelayed` retry cap touches the reconnect/continue path; if recruits ever fail to come back, grep for `Gave up respawning recruits for player`.

- ⏸️ **Phase 5 gate — steps 3, 4, 12, 13 and 16 of `implementation.md` §6**, two clients on a **dedicated server**. This is the round where the confirmation dialog exists. **Reach the Group tab with `P` (keyboard) or `view` + `X` (gamepad) — it is in the PAUSE menu, not the player list.**
  3. **§6 step 3 — the Group tab, with new furniture.** B opens the Group tab. **Expect:** A's group listed and marked private; **a new dimmed paragraph under the group list** ("Your group is private and named after you … To leave a group, use Create new group."); "Create new group" greyed out while B is alone (expected, D11).
     - ⚠️ **The one thing that could look wrong: does the paragraph fit?** It sits in vanilla's `GrouplistFooter`, whose slot is 8 % of the column height inside a clipping parent. If it is cut off, or if it squashes the group list, **say so and quote the last words you can read** — the fix is to shorten one string, not to restructure anything.
     - Also check it appears in the **deploy menu's** group selector (same component, same container) and that the deploy menu is otherwise unchanged.
  4. **§6 step 4 — the dialog, and CANCEL SENDING NOTHING.** B selects A's group and presses Join. **Expect: a dialog titled "Join Group" naming A.** With no recruits: "*A* will be able to command your recruits while you are in their group." With N recruits: "Your *N* recruits will move to *A*'s group and take orders from *A*…" — **check both copies of A's name rendered** (see the `%2`-twice warning below). B presses **Cancel**. **Expect: A receives NOTHING** — no request, no notification, nothing in A's Requests tab. That is the whole point of the phase; if A sees a request after a cancel, stop and report it.
  12. **§6 step 12 — the same dialog on the INVITE side.** A invites B from the player list. B opens the Group tab, selects A's group, presses Accept. **Expect: the dialog appears BEFORE anything is sent**, worded from B's point of view. Cancel ⇒ the invite is still pending and B can accept later. Confirm ⇒ B joins.
  13. **§6 step 13 — the leader notification.** With B owning ≥1 recruit, B joins A's group. **Expect: A gets a non-blocking text notification** "*B* joined your group with *N* recruits". **N must be the number that actually arrived**, so cross-check it against A's slave group. **Expect NO notification** when B has no recruits, and none for a player spawning into their own group.
  16. **§6 step 16 — THE GAMEPAD-ONLY WALKTHROUGH. Unplug the mouse and keyboard.** `view`+`X` to open the Group menu → d-pad/stick between tiles → **`Y`** to Join → the dialog appears → **`A`** confirms (**exactly one** request must reach A — watch A's screen) → repeat and **`B`** cancels → nothing is sent. Then, holding an invite, **hold `Y`** to Accept → same dialog → `A`/`B`. Finally trigger a refusal (7th player into a 6-player group, or ask a full group) and confirm the **"Cannot Join Group"** dialog appears and that **both `A` and `B` dismiss it** — it only has an OK button, so if `B` does nothing, say so.
  - **Not reachable without staging it, but worth trying if you can:** confirm the join *after* the target group has become full or been deleted (e.g. B opens the dialog on A's 5-player group, a 6th joins, then B confirms). **Expect the "Cannot Join Group" dialog and no request sent.** This is the one path that opens a dialog from inside a closing dialog — if the message flashes and vanishes, or focus ends up stranded, that is the cause and it is noted in the code. A silent refusal here is the pre-Phase-5 baseline, not a regression.
  - **Regression watch:** solo/host — open the Group tab, press Join on your own group tile. **Expect no dialog and no error** (it is a deliberate no-op).

- ⏸️ **Phase 6 gate — steps 11 and 15 of `implementation.md` §6, plus the two edge cases that only exist in MP**, two clients on a **dedicated server**.
  11. **§6 step 11 — the leader disconnects (T6.1 + T6.3 together).** B joins A's group, both carrying recruits. **A (the leader) disconnects.** **Expect:** B is promoted to leader; **A's recruits are no longer in the group's slave group and B cannot command any of them**; B's own recruits are still there and still obey B; every recruit is still owned by whoever recruited it. Server log should show `Player <id> disconnected out of group <n> - pulling their recruits out using the cached persistent id` immediately followed by `Pulled N of <persId>'s recruits out of group <n>'s slave group`. **The old Phase 4 line `... has no persistent id any more ...` must NOT appear** — if it does, the T6.3 hook did not install and the fix is dead.
  - **The mirror case, worth doing right after:** B (not the leader) disconnects out of A's group. **Expect:** the same two log lines, and **A immediately loses command of B's recruits**. This is the grief vector R9 describes; before Phase 6 A kept them for ~10 minutes.
  - **Regression watch on the same hook:** a player who disconnects while **alone in their own group** must still come back normally on reconnect with their recruits (their recruits are now pulled out of their own slave group on the way out — the bodies are untouched and the reconnect respawn path is what puts them back).
  15. **§6 step 15 — THE AI-DENSITY MEASUREMENT (goal G11, task T6.2). This is the number the budget knob is waiting for.**
      **The knob ships at `MAX_SLAVE_AI_PER_GROUP = 0` (disabled) and nothing warns until you set it.** Procedure:
      1. Dedicated server, two clients. Each player recruits to the **16-recruit cap** (`MAX_RECRUITS_PER_PLAYER`) while alone in their own group. Let everything settle.
      2. **Baseline first:** with the two players in *separate* groups (16 AI in each slave group), record the server's frame time — `#frame` / the server diag overlay, or the `FPS` column of the server console — over ~30 seconds of ordinary play, plus the value with the AI idle and the value while both squads are moving.
      3. B joins A's group. **Expect 32 agents in one slave group.** Record the same three numbers again, under the same conditions.
      4. Record **both** numbers in this file: the agent count and the server frame time, with the conditions (idle / moving / in combat), the map area, and anything else running (towns, patrols, jobs).
      5. **Extrapolate to the 6 × 16 = 96 case** from the 16 → 32 delta and say what the extrapolation is. Do **not** set the budget from a guess.
      6. **Then decide the budget.** If 32 agents cost nothing measurable, leave `MAX_SLAVE_AI_PER_GROUP` at 0 and record that as the justified decision. If the delta is real, set it to the highest count that still played acceptably — it is a one-line change in `OVT_PlayerGroupManagerComponent`, the arithmetic behind it is already test-pinned, and the effect is two text notifications, never a refused join.
      - While you are there: does a 32-AI slave group still take orders sanely (formation, move, engage), or does the *command UI* fall over before the frame time does? That answer matters as much as the number.
  - **T6.6, if you can stage it (§6 step 14's sibling):** get a join **request** pending on a group that then fills to 6 players, and have the leader approve it afterwards. **Expect:** the requester stays in their own group **and now gets a "That group is full - your join request could not be completed" notification** (before Phase 6 this was a completely silent no-op). Note: this string is **not yet exported** — see the table below — so until the next Workbench pass it may render as `#OVT-Msg-GroupJoinRefusedFull`.

---

## Localization keys awaiting a Workbench export pass

> **UPDATE, 2026-08-06 (Phase 6): the user ran an export pass at 21:52 and all nine Phase 5 keys below are now in the runtime `.conf` exports.** `git diff --stat Language/` therefore shows the six `.<lang>.conf` files **as well as** the `.st` — those six are the **user's Workbench output**, not an agent edit (their mtimes predate the Phase 6 session, and Phase 6 opened only the `.st`). **Two new keys were added in Phase 6 and are still `.st`-only** — they are listed at the bottom of the table and need one more export pass. Unlike the Phase 5 UI strings, these two are **notification presets** referenced from `Configs/overthrowBroadcastMessages.conf`, so they have **no English fallback in code**: until the export runs they will render as raw `#OVT-Msg-...` keys in the notification strip. Nothing else is affected.

**The 9 keys below were in `Language/localization_Overthrow.st` only when Phase 5 shipped.** Please run one Workbench export pass so they reach the runtime tables (and so translators can see them).

**Nothing is broken until you do.** Every one of them is fetched through `SCR_GroupSubMenuBase.OvtTranslateOrEnglish*`, which falls back to an English literal when the key does not resolve, so the screens read correctly today. The `.conf` presets do carry the keys, but the script overwrites title and message on every open.

| Key | Where it shows | English |
|---|---|---|
| `OVT-PlayerGroups_JoinConfirmTitle` | title of both confirmation dialogs | Join Group |
| `OVT-PlayerGroups_JoinConfirmMessage` | confirmation, player owns **no** recruits (`%1` = leader) | %1 will be able to command your recruits while you are in their group. |
| `OVT-PlayerGroups_JoinConfirmRecruitsMessage` | confirmation, player owns ≥1 recruit (`%1` = count, `%2` = leader, **used twice**) | Your %1 recruits will move to %2's group and take orders from %2. … |
| `OVT-PlayerGroups_JoinBlockedTitle` | title of the refusal dialog | Cannot Join Group |
| `OVT-PlayerGroups_JoinBlockedFull` | pressing Join on a full group | That group is full. Nobody else can join it until somebody leaves. |
| `OVT-PlayerGroups_JoinBlockedUnavailable` | `CanPlayerJoinGroup` said no for any other reason | You cannot join that group right now. |
| `OVT-PlayerGroups_JoinBlockedGone` | the selected group vanished (T6.4's message, delivered early) | That group no longer exists. |
| `OVT-PlayerGroups_ModelExplainer` | the paragraph under the group list | Your group is private and named after you. … To leave a group, use Create new group. |
| `OVT-Msg-PlayerJoinedWithRecruits` | leader notification (`%1` = joiner, `%2` = count) | %1 joined your group with %2 recruits - you can command them while they are here |
| **`OVT-Msg-GroupJoinRefusedFull`** ⚠️ **NEW, Phase 6, NOT exported** | notification to a requester whose approved join was refused because the group filled up first (T6.6) | That group is full - your join request could not be completed |
| **`OVT-Msg-GroupAiBudgetExceeded`** ⚠️ **NEW, Phase 6, NOT exported** | notification to the joiner and the leader when a join takes the group over `MAX_SLAVE_AI_PER_GROUP` (T6.2). **Dormant while the budget is 0** | This group now commands %1 AI, over the recommended limit of %2 - performance may suffer |

⚠️ **`OVT-PlayerGroups_JoinConfirmRecruitsMessage` uses `%2` twice.** `WidgetManager.Translate` is expected to substitute every occurrence, and the English fallback goes through `string.Format`, which is expected to do the same — **neither was verified at runtime**. If the second copy of the leader's name renders as a literal `%2`, that is the cause; the fix is to rephrase the string (e.g. "… and take orders from them"), not to change code.

⚠️ **When editing the English text, edit it in TWO places** — the `.st` item and the matching `OVT_EN_*` const at the top of `Scripts/Game/UI/Modded/SCR_GroupSubMenuBase.c`. See the fallback decision above.

---

## Open Questions

- [x] **Q:** Reconnect — back to your own group or your friend's?
      **A:** Your own. Locked (D2); group membership is not persisted.
- [x] **Q:** Invites as well as requests?
      **A:** Both. Locked (G7) — the confirmation dialog appears on the invitee's side too.
- [x] **Q:** Warn the leader when someone joins with recruits?
      **A:** Yes, a non-blocking text notification (G8, T5.4).
- [x] **Q:** Does anything besides recruits key off "same group" in Overthrow (job assignment, notifications, map icons) that changes meaning once groups can be shared?
      **A:** **No.** Answered 2026-08-06 (Phase 3) by grepping `GetGroupID()` / `GetPlayerGroup(` / `IsPlayerInGroup(` / `GetLeaderID()` across `Scripts/`. Every hit outside the new manager and the two modded classes is either `OVT_RecruitManagerComponent` (`:1719`, `:1737`, `:1946`, `:1963` — precisely the two leader guards T4.5 changes to membership guards) or `OVT_SpawnLogic.CreateAndJoinGroup:850`. Jobs, notifications, map markers, the economy and real estate all key off persistent ids or positions, never group membership. So sharing a group has **no** second-order consumers to audit beyond Phase 4's work.

---

## Session Notes

### 2026-08-06 20:02 — feature started
- `/autorun-feature core/player-groups`. Requirements + plan already existed, so planning was skipped; `tasks.md` (31 tasks over 6 phases) and this file scaffolded, `implementation.md` status → In Progress.
- Phases 3, 4 and 5 are marked ADVANCED by the plan's routing table and will run on the `*-advanced` agents.

### 2026-08-06 — Phase 2 implemented (T2.1, T2.2, T2.3)

- **T2.1** `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c` — `newGroup.SetName(playerName)` replaced with `SetCustomName(playerName, playerId)`, followed by `SetPrivate(true)` and `SetPrivacyChangeable(false)`, each with a `//!` comment citing the vanilla file:line. All three signatures re-read in `Entities/SCR_AIGroup.c` (`SetPrivate` `:535`, `SetPrivacyChangeable` `:547`, `SetCustomName(string name, int authorID)` `:744`) — all three call the handler locally then `Rpc(...)` broadcast, so all three are correct server entry points. Note `RPC_DoSetCustomName` runs the name through the async platform profanity filter (`:751-785`), so the custom name lands a frame or two later on each machine; harmless.
- **T2.2** NEW `Scripts/Game/Player/Modded/SCR_PlayerControllerGroupComponent.c` — `modded class` overriding the non-RPC wrapper `RequestJoinGroup(int)` with the server/client branch. Placed alongside the existing `SCR_PlayerController.c` per the repo convention (the plan text said `Scripts/Game/Modded/`; the prompt's `Player/Modded/` matches the repo and was used). The vanilla defect is confirmed exactly as described: `:830-833` is a bare `Rpc(RPC_AskJoinGroup, groupID)` and `RPC_ConfirmJoinPrivateGroup` (`:544`, `RplRcver.Server`) calls it at `:560` from server code. The inline branch at `OVT_SpawnLogic.c` (now `:917-920`) was **left in place** as planned — redundant but correct, and it documents the hazard.
- **T2.3** NEW `Scripts/Game/Modded/SCR_GroupsManagerComponent.c` — `modded class` overriding `int MovePlayerToGroup(int playerID, int previousGroupID, int newGroupID)` to reject before the removal. Vanilla re-verified: signature exact, the ordering defect is real (`RemovePlayer` `:114` before `IsFull()` `:119`, returns `-1` at `:122`), and `FindGroup` (`:358`) is public and reachable from the subclass.
- **Gate:** `tools/compile-check.sh` → **exit 0** (5918 files). `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → **exit 0**, 60 tests, no regression.
- **Deliberately not done:** the `RPC_ConfirmJoinPrivateGroup` null guard (see the decision above — the disconnect case is already covered by vanilla's `:554-556` PlayerController guard, and overriding an `[RplRpc]` method is not worth it for an unreachable branch).
- Nothing was committed; all changes are uncommitted in the working tree.

### 2026-08-06 — Phase 3 implemented (T3.1-T3.5)

- **T3.1** NEW `Scripts/Game/GameMode/Managers/OVT_PlayerGroupManagerComponent.c` — Manager pattern copied from `OVT_RecruitManagerComponent.c:82-122` (`s_Instance` assigned in `OnPostInit` before the `SCR_Global.IsEditMode()` early-out, static `GetInstance()`). Registered on `Prefabs/GameMode/OVT_OverthrowGameMode.et` as `{6A7D9B2E00000080}`. No `OVT_Global` accessor (D10).
- **T3.2** `EnsureOwnGroup(int playerId)` holds the whole group-creation body; `OVT_SpawnLogic.CreateAndJoinGroup` is now a thin caller that keeps its retry ladder byte-identical and still fires `m_OnPlayerGroupCreated` from the same branch it always did. `SetCivilianFaction` stayed on the spawn path (decision above). `SetCustomName`/`SetPrivate`/`SetPrivacyChangeable` and the BUG-088 faction error print all moved intact.
- **T3.3** Both static invokers subscribed in `OnPostInit`, unconditionally, with `Replication.IsServer()` in every handler body (decision above). Signature re-verified against the real source: `s_OnPlayerAdded.Invoke(this, playerID)` (`Entities/SCR_AIGroup.c:1280`) / `s_OnPlayerRemoved.Invoke(this, playerID)` (`:1503`), both from `RplRcver.Broadcast` RPCs that `AddPlayer` (`:1408-1409`) and `RemovePlayer` (`:1538-1539`) call locally then broadcast — exactly as the plan claimed.
- **T3.4** `OnGroupPlayerRemoved` → `CallLater(RestoreOwnGroupDeferred, 0, false, playerID)`; the deferred handler no-ops without a live `PlayerController` or when `GetGroupID() != -1`. Traced against T2.3 — the "rejected join" case it was meant to catch no longer reaches it (decision above), so three live cases, not four.
- **T3.5** Deletion timing **verified by reading**: `OnGroupPlayerRemoved` (`Groups/SCR_GroupsManagerComponent.c:735`, subscribed `:1724`) → `DeleteGroupDelayed` (`:765`) → queue + `GetGame().GetCallqueue().Call(DeleteGroups)` (`:721-730`) → `DeleteGroups` (`:1647-1654`) **next frame**. Cited in the handler's comment, and the handler logs `slave group readable: <bool>` on every removal so the play-test can confirm it at runtime. Init case `OVT_TEST_Init_PlayerGroups_ManagerResolves` added.
- **Beyond the task list** (both recorded as decisions above, both trivially reversible): empty-group **adoption** in `EnsureOwnGroup` + `IsGroupQueuedForDeletionOVT()` on the modded `SCR_GroupsManagerComponent`; the "alone in a public group ⇒ it's yours" claim in `OnGroupPlayerAdded`; and `m_bAllowRejoinPlayerAfterReconnecting 0` on the game-mode prefab. Without the first two, **no player can ever leave a group** (vanilla's only exit is "Create new group", which the permanent stray empty group disables) and §6 step 9 is unrunnable; without the third, §6 step 10 fails D2.
- **Gate:** `tools/compile-check.sh` → **exit 0** (5919 files, 5 s). `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → **exit 0**, **61 tests** (was 60), 18 s.
- **Fail-proof, both directions, recorded in the case comment:** with the `OVT_PlayerGroupManagerComponent` entry deleted from the game-mode prefab, `tools/run-tests.sh OVT_TEST_Init_PlayerGroups_ManagerResolves` → **exit 1**, failing on the first assertion verbatim ("`OVT_PlayerGroupManagerComponent.GetInstance()` is null — …"); with the entry restored → **exit 0**. No `maxAttempts`.
- **Deliberately not done:** anything recruit-related (Phase 4 — the two handlers are the hooks it will use), anything UI or localized (Phase 5), and the kicked-player case (noted under Gotchas for Phase 6).
- Nothing was committed; all changes are uncommitted in the working tree.

### 2026-08-06 — Phase 4 implemented (T4.1-T4.6)

- **T4.1** NEW `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c` — static-only, world-free, manager-free: `SelectTransferable(ownedRecruits, ownerOnline, out outSkippedOffline)`, `ProjectSlaveAiCount` (a clamped sum — a negative term contributes 0 rather than dragging the projection below what is already there) and `ExceedsAiBudget` (`budget <= 0` = disabled; sitting exactly on the budget is not exceeding it). `outSkippedOffline` is defined as **recruits skipped for having no body**, so an offline owner reports **0** — nothing was examined per-recruit, the whole transfer was refused. That definition is pinned by a case.
- **T4.2** `MoveRecruitsToGroup(ownerPersistentId, targetGroup, ownerOnline)` — `SelectTransferable` → `FindRecruitEntity` → `AIControlComponent.ActivateAI()` → `groupController.AddAIToSlaveGroup(entity, targetGroup)` on the **owner's** controller, the exact call `AddRecruitToPlayerGroup` already used. Keeps the `GetSlave()` null guard, warns (never dereferences) on a recruit whose record claims a body the world does not have.
- **T4.3** `RemoveRecruitsFromGroup(ownerPersistentId, exGroup)` — vanilla's `RemoveAiFromSlaveGroup` **inlined** rather than called, so it needs no player controller (decision above): `Deactivate()` on the last agent, `RemoveAgentFromControlledEntity`, `AskRemoveAiMemberFromGroup(slaveRplId, characterRplId)`. Only touches recruits whose agent's parent group **is** that slave group. Ids are snapshotted before iterating (T6.7).
- **T4.4** `OnGroupPlayerAdded` → `ClaimGroupIfAlone` (Phase 3, verbatim, extracted) + `MoveOwnedRecruitsIn`; `OnGroupPlayerRemoved` → `MoveOwnedRecruitsOut` **synchronously**, before the deferred restore is even scheduled. Net-one-membership on a switch was verified **by code trace, not at runtime** — the pull runs from `RemovePlayer` (`MovePlayerToGroup:114`) and the place from `AddPlayer` (`:125`), same frame, in that order, and both methods log their count so the play-test can confirm the pairing (§6 step 6's grep note). A live two-client check is the user's.
- **T4.5** Both `GetLeaderID() != playerId` guards became `!IsPlayerInGroup(playerId)` (`Entities/SCR_AIGroup.c:487`), at `OVT_RecruitManagerComponent.c:1737` and `:1963` — **both line numbers were exactly as the plan said**. `RespawnRecruitsDelayed` gained a `retryCount` parameter capped at `RESPAWN_RECRUITS_MAX_RETRIES = 10`, incremented by **both** re-arm branches (decision above), with a `LogLevel.WARNING` on exhaustion naming the consequence. The `groupId == -1` and `GetSlave()` guard predicates are untouched.
- **T4.6** NEW `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GroupRecruits.c`, 5 cases. **No test-group config was edited** — `OVT_TestGroup_Fast.conf` and `OVT_TestGroup_All.conf` both already list `OVT_TEST_LogicSuite`, so a `[Test(suite: OVT_TEST_LogicSuite)]` case joins both automatically.
- **Gate:** `tools/compile-check.sh` → **exit 0** (5921 files, 5 s). Fast `{6A6E29FF47ECB840}` → **exit 0, 38 tests**. All `{6A6E2A002F53A581}` → **exit 0, 66 tests** (was 61).
- **Fail-proof, both directions, three separate faults** (recorded in the suite preamble): (1) inverting `SelectTransferable`'s recruit filter *and* disabling its offline-owner early return → **exit 1, 3 of 38** (Mixed / OfflineOwner / OnlineOrder); the other two survived it, so (2) `ExceedsAiBudget`'s `budget <= 0` → `budget < 0` failed AiBudgetArithmetic and (3) `Count() + 1 != 0` failed EmptyOwner → **exit 1, 2 of 38**. Everything reverted → **exit 0, 38**. No `maxAttempts`.
- **I1 held:** `git diff | grep '^+'` over `Scripts/` finds `m_sOwnerPersistentId` / `OVT_PlayerOwnerComponent` **only inside comments** saying they must not be written. No new code assigns ownership.
- **Deliberately not done:** the disconnect pull-out (T6.3 — gap documented and now logged at WARNING), the AI-density measurement and the budget knob's call site (T6.2 — the pure predicate exists and is tested, nothing consumes it yet), anything UI or localized (Phase 5), and `OVT_PlayerData.IsOffline()` (already fixed by `core/player-manager`; this feature simply does not consume it).
- Nothing was committed; all changes are uncommitted in the working tree.

### 2026-08-06 — Phase 5 implemented (T5.1-T5.6)

- **Seam re-verified first.** Before writing anything, `implementation.md` §3.4's claim was re-checked line by line against the vanilla tree: `JoinSelectedGroup` at `:259` with exactly two references in the whole game, `AcceptInvite` at `:285` with one call site, both subclasses read in full and neither overriding either, and the sticky invite notification confirmed display-only. **The claim held; no plan amendment was needed.** Details in the decision above.
- **T5.1** NEW `Scripts/Game/UI/Modded/SCR_GroupSubMenuBase.c` (placed under a `Modded/` folder beside the area it mods, matching `AI/Modded/`, `UserActions/Modded/`, `Player/Modded/`). Overrides `JoinSelectedGroup()`, `AcceptInvite()`, `OnTabCreate()` and `OnTabRemove()`. Order of tests in both join overrides: no controller → `super`; nothing selected → `super`; group gone → message; **own group → `super`** (no scary dialog for a no-op); `IsFull()` / `CanPlayerJoinGroup` → message; otherwise → dialog. The confirm handler re-resolves the group, re-tests `IsFull()`, restores the tile selection, then calls `super`. Cancel does nothing at all.
- **T5.2** `Configs/UI/Dialogs/DialogPresets_Campaign.conf` — `JOIN_GROUP_CONFIRM` and `JOIN_GROUP_CONFIRM_RECRUITS` copied byte-for-byte from `DISMISS_RECRUIT`'s shape (`confirm`/`DialogConfirm`, `cancel`/`MenuBack`, cancel sound), plus a third, `JOIN_GROUP_BLOCKED`, for the refusal messages (decision above). Opened with `SCR_ConfigurableDialogUi.CreateFromPreset`; title and message substituted after creation exactly as `OVT_CampMenuContext.c:76` and `OVT_RecruitsContext.c:371` do.
- **T5.3** NEW `UI/Layouts/Menu/GroupModelExplainer.layout` + `.layout.meta` (all five console configurations), instantiated from `OnTabCreate` into vanilla's already-empty `GrouplistFooter`. **No vanilla layout was forked.** One widget GUID, checked for duplicates. Text covers all three facts, including Phase 3's finding that **"Create new group" is the only way to leave** — the least discoverable thing in this feature.
- **T5.4** `OVT_PlayerGroupManagerComponent.NotifyLeaderOfArrivingRecruits`, hung off `OnGroupPlayerAdded` after the recruit move. `OVT_NotificationManagerComponent.SendTextNotification("PlayerJoinedWithRecruits", leaderId, name, count)`; new preset in `Configs/overthrowBroadcastMessages.conf`. **No `ENotification` value added.** `MoveRecruitsToGroup` changed `void` → `int` so the count is what arrived, not what the joiner owns (decision above).
- **T5.5** 9 `#OVT-` keys appended to `Language/localization_Overthrow.st` (GUIDs `{6A7D9B2E00000090}`-`{...98}`, a range verified unused across the whole repo), each with its `Comment` filled in. **No `.<lang>.conf` was opened, let alone edited.** Every string is also reachable through an English fallback in code so the export lag is invisible to the player.
- **T5.6** Gamepad pass done **by reading**, since there is no way to run it here. Confirmed: `ShowGroupMenu` = `KC_P` / `view`+`X`; `MenuJoinGroup` = `KC_J` / `y` click and `GroupAcceptInvite` = `KC_U` / `y` **hold**, both in vanilla's `GroupMenuContext`, and mutually exclusive by visibility so they cannot double-fire; `DialogConfirm` = `KC_RETURN` / `a` and `MenuBack` = `KC_ESCAPE` / `b`, both in `MenuContext`, which is what an empty `m_sActionContext` selects; a dialog force-disables the buttons of the menu beneath it (`SCR_InputButtonComponent.OnDialogOpen`) and `OnInput` bails on an unfocused parent menu. Nothing mouse-only was introduced — the only new widget is non-interactive text. **Whether a controller actually walks it end to end is step 16 and is the user's.**
- **Gate:** `tools/compile-check.sh` → **exit 0** (5922 files, 5 s). `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → **exit 0, 66 tests** (unchanged — UI adds no assertable surface). `git diff --stat Language/` → **`localization_Overthrow.st` only, 153 insertions**. Input-conflict checker → **exit 0**, 12 pre-existing `BASE` rows, none of them in `GroupMenuContext` or any screen this phase touched.
- **Deliberately not done:** T6.4's full "group deleted mid-join" handling (only its *message* shipped early, because the confirm handler had to re-resolve the group anyway); T6.6's server-side "leader approved into a full group" notification; any new test case (there is nothing here the spine can reach — see Testing Approach).
- Nothing was committed; all changes are uncommitted in the working tree.

### 2026-08-06 — Phase 6 implemented (T6.1-T6.7 + hardening sweep)

- **Three tasks built, four found already satisfied.** Built: **T6.3** (the disconnect pull-out), **T6.2** (the budget knob and its two warnings), **T6.6** (the refused-approve notification). Verified-already-done and deliberately **not** duplicated: **T6.1** (nothing keys off leader identity — proven by grep), **T6.4** (T2.3's null-group branch plus Phase 5's three `JoinBlockedGone` sites), **T6.5** (Phase 4's null guards in both transfer methods), **T6.7** (Phase 4's id snapshots in both loops). Row-by-row record in the table above.
- **T6.3 — the real gap.** `OVT_PlayerGroupManagerComponent` gained `OnPlayerDisconnecting` (subscribed to `OVT_PlayerManagerComponent.m_OnPlayerDisconnected`), a one-frame `m_mDepartingPersistentIds` cache, `ForgetDepartingPlayer`, `InstallDisconnectHook` (called from both `OnPostInit` and a new `EOnInit`, behind a flag) and a fallback in `MoveOwnedRecruitsOut`. The Phase 4 WARNING that pointed at T6.3 was replaced by a `LogLevel.NORMAL` line naming the cached-id path; the WARNING now only fires when **neither** lookup resolves, which is a genuine anomaly.
- **T6.2 —** `MAX_SLAVE_AI_PER_GROUP = 0` (disabled) plus `WarnIfAiBudgetExceeded`, driven by the existing pure helpers so the arithmetic keeps its Logic-tier coverage. New notification preset `GroupAiBudgetExceeded`. **No new test case was added:** `ProjectSlaveAiCount` / `ExceedsAiBudget` (including `budget <= 0` = disabled) were already pinned by Phase 4's `AiBudgetArithmetic` case, and the only new logic — "read the count before the move, warn after" — needs a live slave group and is not Logic-tier reachable. Adding a case that restated an existing claim would have been coverage theatre.
- **T6.6 —** the check lives in the Phase 2 `modded SCR_PlayerControllerGroupComponent.RequestJoinGroup` server branch, not in the reactor (the reactor cannot see a non-event — decision above). New notification preset `GroupJoinRefusedFull`.
- **Hardening sweep.** `grep -rn GRPDIAG` over the whole repo (excluding `docs/`): **zero hits** — Phase 1 shipped no instrumentation and none has appeared since (**Q10 satisfied**). Every `LogLevel.WARNING`/`ERROR` in this feature's files was re-read against its branch: all sit on genuine failure or anomaly paths, none on a per-frame or routine per-membership-change path. The two routinely-hit lines in the membership handlers (`Player <id> removed from group ...`, `Moved N ... / Pulled N ...`) are `LogLevel.NORMAL` and were left — they are the running proof the play-test greps for. **Nothing was downgraded or removed, because nothing needed it.**
- **Gate:** `tools/compile-check.sh` → **exit 0** (5922 files, 5 s). Fast `{6A6E29FF47ECB840}` → **exit 0, 38 tests, 15 s**. All `{6A6E2A002F53A581}` → **exit 0, 66 tests, 19 s** — *on the second run*. The **first** All run came back **exit 1** with a single failure, `OVT_TEST_Init_Loadout_NestedItemsSurviveApply`, `TestResultTimeout` after 60 s in a 152 s session (the same suite completes in 19 s when the machine is quiet, and a second Workbench process was running concurrently). Unrelated to this feature — it is a loadout/persistence case, and nothing in Phase 6 touches that path — but **recorded rather than swept**: if that case times out again on an idle machine it is a real flake in the harness and belongs to whoever owns that suite.
- **`git diff --stat` gates:** `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` → **empty** (Q6 holds; no new client→server RPC, no new `OVT_Global` getter). `Configs/Systems/Persistence/` and `Scripts/Game/Persistence/` → **empty** (I4 holds; group membership is still deliberately unpersisted). `Language/` → the `.st` **plus six `.<lang>.conf` exports the user regenerated at 21:52, before this session** — Phase 6 opened only the `.st`; see the note above the key table.
- **Deliberately not done:** the AI-density measurement itself and the resulting budget value (needs two clients and a live server — procedure written up under "Needs human verification"); the `FindRecruitEntity` mid-iteration bug at `OVT_RecruitManagerComponent.c:1587` (contained, not fixed — it belongs to `resistance/recruits`); the kicked-player case noted under Gotchas (unchanged behaviour, still un-play-tested); `CreateAndJoinGroup`'s retry-count-always-0 ladder (pre-existing, untouched, still worth folding into a later pass).
- Nothing was committed; all changes are uncommitted in the working tree.

---

*Update this file at the end of each work session.*

### 2026-08-06 — MP play-test GREEN, feature closed
- User ran the dedicated-server multiplayer tests: **all passing, not a single problem found.**
- Explicitly included the highest-risk uncovered case: **a group leader with joined players and recruits disconnecting** → new leader promoted, and that new leader had command of the group's recruits. That exercises leader promotion, recruit ownership and the T6.3 disconnect pull-out simultaneously — the scenario R9 called a grief vector and the one the automated spine cannot touch at all.
- Feature marked **COMPLETE**. Remaining items are housekeeping only: one Workbench export for 2 notification keys with no code fallback, and the optional AI-density measurement (budget still shipped at 0 = disabled).
- Nothing was changed in code by this session — the play-test found nothing to fix.
