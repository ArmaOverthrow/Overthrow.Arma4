# Recruits - Context & Decisions

**Last Updated:** 2026-08-06
**Current Phase:** Maintenance / bug fixing
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code; body persistence reworked in the `vanilla-persistence` epic)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)
- ✅ **BUG-051/052/053 fixed** (2026-08-02): recruit RPCs validate sender-derived identity, `RpcAsk_RenameRecruit` exists, fast travel captures the departure point before teleporting
- ✅ **BUG-080 fixed** (2026-08-04): recruits follow orders after the owner dies — group insertion goes through the slave-group path, which is commanded by player id
- ✅ **BUG-088 fixed** (2026-08-06, awaiting MP play-test): recruits (and commanding, and the group menu) were dead in every replicated session

**What's Next:**
- 🔬 Dedicated-server play-test of BUG-088 (see below) — MP is uncovered by the test suite
- 📋 Remaining enhancements: the `FindRecruitEntity` mid-iteration removal, the hardcoded `US`/`USSR` XP faction keys, and Logic-tier coverage for the recruit record maths

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` — the heart (2,219 lines): table, lifecycle, despawn/respawn, JIP + broadcast RPCs
- `Scripts/Game/Data/OVT_RecruitData.c` — the record (XP curve, skills map, body persistence id)
- `Scripts/Game/UserActions/OVT_RecruitCivilianAction.c` / `OVT_RecruitFromTentAction.c` — the two acquisition paths (client-side cost checks)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:818-864,966-1012,1304-1353` — client→server seams (recruit, tent, fast-travel-with-recruits, dismiss)
- `Scripts/Game/UI/Context/OVT_RecruitsContext.c` + `Scripts/Game/UI/Components/OVT_RecruitListEntryHandler.c` — roster UI (rename/dismiss/show-on-map)
- `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerSerializer.c` — versioned record serializer (v2 adds the body UUID)
- AI/wanted integration: `Scripts/Game/AI/Modded/SCR_ChimeraAIAgent.c`, `SCR_AIRetreatFromTargetBehavior.c`, `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c`, `Scripts/Game/Commanding/Commands/OVT_OpenInventoryCommand.c`

---

## Important Decisions

- **Record ≠ body:** the generated recruit id is the durable identity; the body is an ordinary vanilla-tracked character whose persistence UUID the record carries (`m_sBodyPersistenceId`, written only by `CaptureRecruitBodyId()`). Permadeath = drop the record, keep the corpse.
- **Bodies follow the owner:** 10 minutes after the owner disconnects, bodies are saved-and-released (write to storage, untrack keeping data, delete). When the owner returns and leads a group, the *same* character is asked back from the persistence system (async, pending-list deduped, 15 s timeout) with a fresh-prefab fallback so a recruit is never lost.
- **Vanilla group system:** recruits are plain AI agents in the player's slave group; commanding/formations are vanilla. Overthrow adds ownership (`OVT_PlayerOwnerComponent`) and an owner-only open-inventory command (via temporary possession).
- **Whole-table JIP + three broadcast RPCs:** `RplSave/RplLoad` ships the roster (incl. skills + RplIds); create/remove/update broadcasts keep it live; the body UUID never crosses the wire (server-only handle).
- **Two costs, one cap:** civilian = `baseRecruitCost` (client-charged); tent = 50% + 1 town supporter + 1 population; hard cap `MAX_RECRUITS_PER_PLAYER = 16`.

---

## Gotchas & Learnings

- **A recruit can only join a player who is a group leader, and in MP nobody was** (BUG-088, fixed 2026-08-06). `AddRecruitToPlayerGroup` guards on `GetGroupID() != -1` and `GetLeaderID() == playerId`; both failed on every dedicated/hosted server because the *player* never got a faction or a group. The cause was two server-side calls to vanilla **client-request** APIs — `SCR_PlayerFactionAffiliationComponent.RequestFaction()` and `SCR_PlayerControllerGroupComponent.RequestJoinGroup()` — each of which only marshals an `RplRcver.Server` RPC that goes nowhere when the caller *is* the server. Solo hid it because with no replication session those calls execute locally. **The rule: from server code call vanilla's server-side entry point (`SetFaction_S`, `RPC_AskJoinGroup`, `AddAIToSlaveGroup`), never the `Request*` wrapper.** Same family as BUG-045 (officer promotion) and BUG-052 (rename).
- **Group insertion is a direct server call now**, not a broadcast round trip through the owning client. The old path (`RpcDo_AddRecruitToGroup` + 6 s pre-delay + 10 × 2 s entity-resolution ladder) is gone; `AddAIToSlaveGroup` broadcasts membership itself. The remaining timing ladder in the *respawn* flow is unrelated.
- ~~Rename never reaches the server~~ — fixed (BUG-052): `RpcAsk_RenameRecruit` in `OVT_PlayerCommsComponent` validates ownership server-side and broadcasts. Note it lives in the deprecated comms component; new client→server work belongs on `OVT_OverthrowController`.
- ~~Fast travel searches the destination~~ — fixed (BUG-053): the departure point is captured into a local before `TeleportPlayer` (`OVT_PlayerCommsComponent.c:1482`). The fee is still charged client-side.
- ~~The recruit RPCs trust the client~~ — fixed (BUG-051): `ResolveSenderPlayerId()` derives the actor from the RPC sender. Money is still deducted client-side.
- **`FindRecruitEntity` removes from `m_mEntityToRecruit` mid-`foreach`** (`OVT_RecruitManagerComponent.c:1587`) — the same hazard `SyncRecruitPositions` explicitly avoids at `:1496`.
- **Kill XP is hardcoded to `US`/`USSR`** (`:679`) — custom occupying factions earn recruits nothing.
- **Skills/training are format-only:** persisted, JIP'd and broadcast, but no system writes recruit skills or training; `m_OnRecruitXPGained` has no listeners; the recruit copy of `GetLevelProgress()` is uncalled. (Measured on 1.7.0.54: EnforceScript does *not* truncate the int/int division here — see `OVT_TEST_Logic_Skills.c` preamble — so the duplicated curve is dead-but-correct, not broken.)
- **Timing ladders everywhere:** group insertion needs the leader to exist, so the server retries at 3 s/500 ms and the client RPC is pre-delayed 6 s with a 10×2 s entity-resolution retry. Fragile but currently load-bearing.
- `PreShutdownPersist` → `SyncRecruitPositions()` is what makes standing-right-there recruits save with current position *and* a body id; without it they'd reload where they were hired, in civilian clothes.

---

*This context file was created retrospectively by analyzing existing code.*
