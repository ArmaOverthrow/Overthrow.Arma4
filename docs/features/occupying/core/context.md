# Occupying Core (Faction Manager) - Context & Decisions

**Last Updated:** 2026-08-22
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- ⏸️ **Play-test the defense-share drip (2026-08-22)** — nothing has watched the faction spend on the new rhythm; the point of the change is *feel*, so a suite cannot close it
- 📋 **These docs are stale** and were written against a 1508-line manager that is now ~2,300 lines. The spend-loop cluster (`SpendResources`, dead `perBase`, divide-by-zero) is **deleted**, `GetBase()` **is** null-guarded, and **BUG-025/026 are both closed** — the "highest-value fixes" this file used to name are all done. The Gotchas below are marked where they no longer hold

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (1508 L) — manager + data records
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c` — per-base controller (flagpole prefab `Prefabs/Controllers/OVT_BaseController.et`, 9 placed in `bases.layer`)
- `Scripts/Game/Controllers/OccupyingFaction/OVT_TowerControllerComponent.c` — empty marker class (towers are pure manager logic)
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c` — save/load
- `Scripts/Game/UserActions/OVT_CaptureBaseAction.c`, `OVT_ManageBaseAction.c`
- `Scripts/Game/GameMode/Systems/Modifiers/*/OVT_OccupyingFactionDeath*Modifier.c` — town modifiers off `m_OnAIKilled`
- Config: `OVT_DifficultySettings.c` (OF tuning block), `OVT_OverthrowConfigComponent.c`

---

## Important Decisions

- **The defense share is a DEBT, dripped hourly, not a transfer (2026-08-22).** `GainAndSpendResources` arms `m_iPendingDefenseTransfer` instead of moving 80 % across in one statement; `DripDefenseShare()` pays a slice an hour at a jittered minute. **The income half deliberately did not move** — `resistance/sleep` replays income through these exact methods, so changing the income cadence changes the replay's granularity with it, which is BUG-183's family (an unpersisted latch paying a sweep twice on load is a repeatable money exploit). The pool is the contended resource, so smoothing its *arrival* lands exactly where the burst behaviour was, and the half carrying the save-format and time-skip hazards is untouched.
  - **Author decision:** the money **stays in the reserve** while owed, and the jitter is **timing only**.
  - ⚠️ **Known, accepted consequence for `occupying/objectives`:** the reserve now sits ~80 % fatter for most of each window, and `OVT_ObjectiveDirectorComponent` reads the reserve for `objectiveQRFResourceGate` — **counter-attacks clear their funding gate earlier than before.** If the objective ramp starts feeling too fast, this is the first thing to look at, not the difficulty fields.
  - **Nothing can be stranded:** `ArmDefenseShareDrip` flushes the previous window before arming the new one, so a window that loses drips to a QRF freeze (`CheckUpdate` returns early for the whole of an engaged QRF), a mid-window load, or a sleep replay costs **timing and never money**.

- **Records + controllers linked by position:** `OVT_BaseData.entId` at discovery; nearest-position matching for persistence restore. Survives ID churn; O(n) lookups.
- **Two unrelated "threat" concepts:** global scalar `m_iThreat` (escalation) vs `GetThreatByLocation()` (spatial score for base ranking + map overlay). They never interact.
- **Garrisons respawn, never restore:** AI self-spawn disabled in `Overthrow.conf`; serializer stores upgrade state / prefab lists; `InitBaseControllers` is the single replay point for new and continued campaigns alike.
- **Hand-rolled positional JIP** (no RplProps); delta updates via reliable broadcast RPCs. `RplLoad` writes faction keys back into the shared config component.
- **Continue-path guard:** deserialize clears `m_bDistributeInitial` so the opening build-out isn't doubled — restored state must be applied before `PostGameStart` runs.

---

## Gotchas & Learnings

- ~~**The spend loop's budget math is dead**~~ — ✅ **GONE (BUG-026 closed).** `SpendResources` and the whole per-base loop were deleted by `virtualization/base-defense-migration`; the defense share now goes to the one deployment pool.
- ~~**`GetBase()` has no null guard**~~ — ✅ **FIXED.** It null-checks the marker and returns null.
- **Tower capture = garrison wipe.** No user action, no timer; and towers never flip back to the OF except via specops' 600 s capture.
- **`SetClientBaseFactions` is a one-shot 1 s bet** on JIP + streaming; a lost race leaves wrong client flags forever.
- **Counter-attacks are much rarer than intended:** pick-then-filter wastes rolls on OF-held bases, and the last base index is unreachable (`RandInt` half-open).
- ~~**`RpcAsk_InstantCaptureBase` ships unauthenticated**~~ — ✅ **BUG-025 closed.** The capture RPCs moved to `OVT_CampaignRequestComponent` and carry no client-supplied position; `InstantCaptureBase` is server-resolved.
- **`m_iThreat` is a float** with an int prefix; `GetThreatLevel()` truncates it, and deployments consume the truncated value.
- The player-count resource ladder (×2/×3/×4/×5/×6 at 4/8/16/24/32) is duplicated verbatim in the QRF controller — change both or neither.
- **Discovery stamps tower/base factions with the config-DEFAULT occupying index** (`GetOccupyingFactionIndex()` computes and caches from `m_sOccupyingFaction = "USSR"` at `Init` time); the save's real faction key is only applied later in `ApplyPersistedOccupyingFaction`. Fixed for both towers AND bases 2026-08-13: any tower/base the save has no record for (map updated after the save) is stamped to the occupying faction in the apply sweep; restored records are never trampled.

---

*This context file was created retrospectively by analyzing existing code.*

---

## 2026-08-24 — A tower belongs to whoever holds the ground under it

**Author, on the test server:** *"they successfully recaptured it, and then after that I killed the specops team, but the tower is still theirs, probably because there's no tower guards yet but that blocks recapture."* Exactly right, and worse than it looks.

**What the code said.** Grepping every caller of `ChangeRadioTowerControl` found **two**, and both require a *deployment* to exist:

| Direction | Only mechanism | Requires |
|---|---|---|
| → resistance | `OVT_RadioTowerCaptureBehaviorDeploymentModule` | a live `Deployment_TowerGarrison` **whose own garrison is wiped** |
| → occupying | `OVT_TowerRecaptureBehaviorDeploymentModule` | a live recapture deployment |

So a tower the occupying faction had just recaptured and **not yet garrisoned** could not be taken back by any means at all: there was no garrison deployment in existence to be wiped, and killing every man standing on it did nothing. The recapture deployment itself is collected the moment it captures (its `OVT_RadioTowerControlConditionDeploymentModule` authors `m_bRequireControl 0`, so the condition fails as soon as the tower is theirs), which is why the specops team the author killed was already irrelevant to ownership.

**Fix — `CheckTowerGroundControl()` on the existing 9 s `CheckRadioTowers` tick.** An occupying-held tower with **no living occupying force inside 80 m** and **a resistance presence inside 80 m** flips to the resistance, whatever deployment is or is not there. `TOWER_CONTROL_RADIUS_M` 80 matches the recapture module's own `m_fHoldRadius`, so "who holds this tower" has one answer whichever side is asking. Chosen by the author over two narrower options (hold-until-relieved, and force-buying the garrison on recapture).

🔴 **Registered alive members, never spawned agents.** This is precisely the rule that historically let a player capture a tower by *walking away* — the old per-tower garrison list held whatever was materialised that tick, so a despawned garrison read as an empty tower. It counts through `OVT_VirtualizationManagerComponent.GetAliveMemberCount()`, which answers from the survivor mask and from dormant counts, so an unspawned garrison still holds its tower. The header on `OVT_RadioTowerData` that says "THERE IS NO GARRISON LIST ON A TOWER ANY MORE" is the record of that bug and still holds.

⚠ **A resistance presence is required**, and that is the second half of the same safety: an empty tower in occupied territory is not captured by nobody. Ground held is players *and* their recruits (`OVT_ResistancePresence.IsGroundHeld`), the same test the deployment behaviours use.

⚠ **One direction only.** Taking a tower *for* the occupying faction stays the recapture module's job, where it is paid for and announced.

⚠ **Cost order matters and is deliberate:** faction check (free) → 80 m sphere query, only on towers they hold → the handle sweep, only for a tower somebody is actually standing on.

**Additive API:** `OVT_VirtualizationManagerComponent.GetGroupFactionKey(int handle)`. `m_sFactionKey` was already stored at `RegisterGroup` and round-tripped by persistence; nothing outside the class could read it, and the rule needs the occupying count *whatever system registered the group*. Read-only, no behaviour change — but `docs/features/virtualization/core/api.md` should pick it up.

⚠ **Consequence the author should weigh:** an occupying tower that has *never* been garrisoned is now free to take by standing next to it. `Deployment_TowerGarrison` is `m_bFreeAtGameStart 1` with `m_iMaxInstances -1`, so this should be rare outside the post-recapture window — but a faction with an empty pool, or the moments before the seeding pass, would leave towers takeable for nothing. That falls directly out of the rule as chosen; narrowing it would mean reintroducing the garrison dependency that was the bug.

`tools/compile-check.sh` exit 0 (6346 files). Suite not run; play-test owed.

### 2026-08-25 — "Capture Radio Tower" action, because the automatic rule cannot see the truth

**Author, on the test server:** *"still issues with the radio tower, im here, its owned by them, theres noone here (confirmed in GM). maybe there is a team walking here from somewhere but I cant see them. we might need to add a 'Capture radio tower' action on it, long press (same as sabotage) that is disabled if you are wanted and/or there are soldiers within a reasonable distance."*

🔴 **The automatic ground-control rule (entry above) has a blind spot it cannot fix.** It counts REGISTERED members through the virtualization core, and `OVT_VirtualGroupGeometry.ResolveLivePosition` answers a **dormant** group with its RECORD position — which for a recapture team is *the tower it is still walking towards*. So a tower reads as garrisoned by men who are kilometres away and not yet materialised. That file's own header documents this exact trap (it is what made a forward base spring up while its party was still a kilometre out), and it is deliberate: the alternative — counting agents — answers "nobody is here" for every dormant group on a quiet server, which is worse. The registry simply cannot answer this question honestly, so the player is given a way to answer it instead.

**What shipped:**
- `OVT_CaptureRadioTowerAction` — a 20 s hold on all three `TransmitterTower_01_*_base.et` deltas, in the **same `sabotage` context and at the same duration** as `OVT_SabotageTowerAction`, read off that action's own block rather than re-authored. Shown only on occupying-held towers.
- Both of the author's gates, with stated reasons: `#OVT-CaptureTower_Wanted` and `#OVT-CaptureTower_Defended` (`DEFENDER_RADIUS_M` 100 — wider than the action context on purpose; the question is whether the place is contested, not whether somebody is touching the mast).
- `OVT_ResistancePresence.IsGroundHeldByOccupying()`, and the existing body refactored to `IsGroundHeldBy(faction, …)` so both sides ask one query. ⚠ **An entity query, not a virtualization read** — that is the whole point: a test the player is standing in front of has to ask the world, because the player can see the world.
- `OVT_TowerSabotageComponent.RequestCapture` / `RpcAsk_CaptureTower`, re-running **every** gate server-side. BUG-025 is this epic's headline debt and this adds no third unvalidated capture RPC: the client's position is a hint, the server re-resolves the tower from its own registry, measures the caller's own character against it, and re-asks both refusals against its own world. Same `Replication.IsServer()` branch as `RequestSabotage`, or it would silently do nothing on a listen host.

⚠ **Three new `.st` keys** (`OVT-CaptureTower`, `_Wanted`, `_Defended`) — master only. **A Workbench localization re-export is owed** or they render as raw keys.

⚠ Placed on `OVT_TowerSabotageComponent` rather than a new controller component, so it needs no `OVT_OverthrowController.et` edit and no `OVT_TEST_Init_ControllerSeam` entry. Its `ComponentEditorProps` description now says "radio tower actions (sabotage, capture)"; the class name is now narrower than what it does.

`tools/compile-check.sh` exit 0 (6348 files). Workbench prefab load + play-test owed.
