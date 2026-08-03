# Recruits - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code; body persistence reworked in the `vanilla-persistence` epic)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements (see implementation.md Future Enhancements — server-validating the recruit RPCs, the multiplayer rename, and the fast-travel search-origin bug are the highest-value fixes)

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

- **Rename never reaches the server:** the UI calls `RenameRecruit()` on the client's replica (`OVT_RecruitsContext.c:443`); there is no rename RPC, so on dedicated servers the name is not persisted and reverts on the next update/rejoin.
- **Fast travel searches the destination:** `RpcAsk_RequestFastTravelWithRecruits` teleports the player, *then* looks for recruits within 50 m of the player's (now-moved) origin (`OVT_PlayerCommsComponent.c:985-992`) — recruits stay behind, fees already charged client-side.
- **The recruit RPCs trust the client:** `playerId` is a parameter, money is only ever deducted client-side, `RpcAsk_RecruitCivilian` will recruit any character an RplId resolves to, and the tent path can orphan a spawned civilian at the cap and spawns even when the supporter deduction no-ops (`OVT_TownManagerComponent.c:1194`).
- **`FindRecruitEntity` removes from `m_mEntityToRecruit` mid-`foreach`** (`OVT_RecruitManagerComponent.c:1587`) — the same hazard `SyncRecruitPositions` explicitly avoids at `:1496`.
- **Kill XP is hardcoded to `US`/`USSR`** (`:679`) — custom occupying factions earn recruits nothing.
- **Skills/training are format-only:** persisted, JIP'd and broadcast, but no system writes recruit skills or training; `m_OnRecruitXPGained` has no listeners; the recruit copy of `GetLevelProgress()` is uncalled. (Measured on 1.7.0.54: EnforceScript does *not* truncate the int/int division here — see `OVT_TEST_Logic_Skills.c` preamble — so the duplicated curve is dead-but-correct, not broken.)
- **Timing ladders everywhere:** group insertion needs the leader to exist, so the server retries at 3 s/500 ms and the client RPC is pre-delayed 6 s with a 10×2 s entity-resolution retry. Fragile but currently load-bearing.
- `PreShutdownPersist` → `SyncRecruitPositions()` is what makes standing-right-there recruits save with current position *and* a body id; without it they'd reload where they were hired, in civilian clothes.

---

*This context file was created retrospectively by analyzing existing code.*
