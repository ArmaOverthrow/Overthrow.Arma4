# Recruit UX - Context & Decisions

**Last Updated:** 2026-08-15 00:35
**Current Phase:** Complete (all 9 phases, 73/73) — **all automated gates green** (final All 165/165); play-test checklists owed
**Status:** 🟢 Ready for Review

---

## Quick Status

**What's Done:**
- ✅ Planning complete (`implementation.md`, 8 phases, D1-D16 decided)
- ✅ Dev docs scaffolded
- ✅ **Phase 1** (T1.1-T1.10) — flag on record, JIP append (trailing bool after skills block), `RpcDo_RecruitActiveStateChanged` broadcast, serializer v3 + `ClearInactiveFlags`, BUG-107 fix, `FindRecruitEntity` collect-then-remove, `IsRecruitInactive`/`GetPlayerRecruitsByState`, persistence+logic test extensions. Gate: All group 143/143 green (exit 0).

- ✅ **Phase 2** (T2.1-T2.11, ADVANCED) — `OVT_Group_InactiveRecruits.et` `{6B4C000000000001}` + marker `{6B4C000000000002}`, `OVT_RecruitInactiveGrouping` (pure), `SetRecruitInactive` entry point, `PlaceRecruitInWorld` fork, transfer + fast-travel exclusions (incl. fare-preview parity the plan missed). Gate: All 148/148 green.

- ✅ **Phase 3** (T3.1-T3.7) — `OVT_RecruitCommandComponent` `{6B4C000000000003}` on controller prefab, `GetRecruitCommands()`, base/set-inactive/set-active actions on 3 prefabs, in-vehicle park guard (client + server), Init fail-proof case, 6 `.st` keys. Gate: Fast 108/108 green.

- ✅ **Phase 4** (T4.1-T4.5) — `OVT_RecruitStatus` pure class, `ReadRecruitStatus`, 10 s server sweep, client status cache + `GetStatusFlags`, Logic flag matrix. Compile clean. **Gate: Fast run DEFERRED — user was play-testing; run before/with Phase 7's gate.**

- ✅ **Phase 5** (T5.1-T5.7) — sectioned roster with flat selection model, toggle button (`KC_G` / `gamepad0:left_trigger` — plan's `shoulder_left` is VON, checker caught it), capacity header, row status icons, 4 imageset placeholder entries, `m_OnRecruitActiveStateChanged` invoker on the manager, 7 `.st` keys, pre-existing `ActionButtons` duplicate GUID fixed. Compile + conflict checker green. **Gate: suites cover no UI — pad/mouse play-test owed (checklist in Phase 5 session note).**

- ✅ **Phase 6** (T6.1-T6.6) — `MapRecruitLocation.layout` (+meta), `OVT_MapRecruitLocation` layer (per-frame inactive/status reads, last-known-position fallback via `GetRecruitEntity`'s built-in client branch), `MapOverthrow.conf` registration, Recruits filter row + prefs. Compile green. **Gate: UI — play-test owed (checklist in session note).**

- ✅ **Phase 7** (T7.1-T7.7, ADVANCED) — `OVT_LoadoutSwap` (server-only entity-transfer swap, Q9 grep empty), `RpcAsk_SwapLoadout` + 7 new `RESULT_` codes + rich 5-arg owner reply, `OVT_SwapLoadoutWithRecruitAction` on the 3 prefabs (`Duration 5`), 7 `.st` keys, 7 Logic cases pinning the outcome classification. Compile clean. **Gate: run before/with the deferred Fast/All run.**

- ✅ **Phase 8** (T8.1-T8.4) — 2 tutorial popups (registered on the game-mode prefab), 3 Field Manual sections + 1 corrected claim, wiki updates (recruits p29, player-groups p55, loadout-manager p30 — verified by content, not search pageIds), full `file:line` citation ledger in the agent report. Docs/config-only — no test gate applicable.
- ✅ **Phase 9** (T9.1-T9.16, post-ship extension) — buy equipped recruits at the tent: quote/buy RPCs, pure pricing rules, shared tent-spawn internals with placement hardening, loadouts-screen purchase mode (`KC_B`/LT), tutorial tip on `PLAYER_BUILD "Recruitment Tent"`, Field Manual + wiki synced with full citation ledger. **Note:** difficulty presets carry scaled fee multipliers (Easy 1.3 / Normal 1.4 / Hard+Extreme+TestWorld 1.5 / Insane 1.7; code default 1.5) — flatten to 1.5 everywhere if per-difficulty scaling is unwanted. Play-test rider fixes: BUG-170 (defend→wait waypoint), hold durations 1.5/1/2.5 s.

**What's Next:**
- ✅ **Deferred regression gate DISCHARGED** (2026-08-14, after the user's game session ended): All group **159/159 green, exit 0** — covers every phase's script changes. All four gates for this feature are now green.
- 👀 **User:** Workbench localization re-export (24 new `.st` items render as raw keys until then), then the play-test checklists (Phases 5/6/7 session notes + implementation.md §6)
- 🐛 Follow-ups filed: BUG-166 (dismiss RPC unvalidated), BUG-167 (join-dialog roster count)

**Blockers:**
- None

---

## Key Files

### Core Implementation
- `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` — the manager every phase touches (JIP :2038/:2096, respawn :1036/:1516, slave-group exit :1959)
- `Scripts/Game/Data/OVT_RecruitData.c` — the record; `m_bInactive` is the single source of truth
- `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerSerializer.c` — v2 → v3
- `Scripts/Game/Components/Controller/` — five existing controller components; `OVT_RecruitCommandComponent` joins them in Phase 3
- `docs/features/resistance/recruit-ux/implementation.md` — the plan; §3 architecture, §4 phases, §5 decisions, §6 DoD

### Related Files
- `Scripts/Game/UI/Map/Visualization/OVT_MapPlayerLocation.c` — template for the Phase 6 marker layer
- `Scripts/Game/UI/Context/OVT_RecruitsContext.c` — Phase 5 roster rework (selection model replaced, D16)
- `Scripts/Game/GameMode/Managers/OVT_GroupRecruitTransfer.c` — pattern for pure logic classes + Phase 2 transferable rule

---

## Important Decisions

Plan decisions D1-D16 live in `implementation.md` §5 — not repeated here. Highlights that constrain implementation:

- **D2:** new broadcast RPC; `RpcDo_RecruitUpdated` is at the 8-param wire limit
- **D6/D7:** inactive group = spawned marked `SCR_AIGroup`, no registry, lifecycle stays `Manual`, untracked from persistence; `SetLifecyclePolicy`/`CleanupGroup`/`CleanupEntity` banned on it
- **D11:** status computed server-side, pushed to owner every 10 s
- **D13:** swap never spawns/deletes — grep proof Q9
- **D14:** localization into `.st` only; literal English in layouts/prefabs until export regen

---

## Gotchas & Learnings

### 1. (from planning) Only three character prefabs are live
**Problem:** Four recruit-ish prefabs exist; two (`INDFOR/FIA/Character_CIV_Recruit.et`, `Character_FIA_Recruit.et`) have zero references.
**Solution:** Actions go on `CIV/Character_CIV_Recruit.et`, `CIV/Character_CIV.et`, `INDFOR/FIA/Character_CIV.et` only.
**Lesson:** Don't touch the dead prefabs.

### 2. (from planning) Controller components resolve sender with `ResolveOwningPlayerId()`
**Problem:** The legacy `ResolveSenderPlayerId` only works on components sitting on the player character.
**Solution:** Copy the helper verbatim from `OVT_TowerSabotageComponent.c:59-80`.

---

## Testing Approach

- **Logic (Fast):** `OVT_TEST_Logic_RecruitClustering.c`, `OVT_TEST_Logic_RecruitStatus.c` (new), `OVT_TEST_Logic_GroupRecruits.c` (extend). ⚠️ No manager-accessor/game-mode-getter identifiers anywhere under `TestSuites/Logic/`, including comments.
- **Persistence (All):** extend recruit round-trip cases with the inactive flag.
- **Init (Fast):** `GetRecruitCommands()` resolves — fails until the component is on the controller prefab (R10's fail-proof).
- Every new case needs a recorded can-fail proof; no `maxAttempts`.
- Test gate: orchestrator runs `run-tests.sh` post-phase; agents stop at `compile-check.sh` exit 0.
- Not automatable (play-test): group behaviour, JIP/MP, map rendering, roster gamepad nav, the swap, quit-and-continue. See `implementation.md` §6/§8.

---

## Next Steps

### Immediate
1. Phase 1 via `component-developer`
2. Orchestrator: compile-check + run-tests (All — persistence touched), then Phase 2 (ADVANCED)

### Future (After This Phase)
1. ✅ R11 filed as **BUG-166** (2026-08-14, high, linked `resistance/recruits`). BUG-107 closed (fixed by T1.6).

---

## Open Questions

- (none blocking)

## Stretch candidates (user-suggested, not in scope)

- **Recruit contact reports on the map** (user, 2026-08-14, mid-play-test): when a recruit's AI perceives an enemy, push an owner-targeted contact ping (position + recruit id) through `OVT_RecruitCommandComponent` and render it as a TTL'd marker type in the Phase 6 recruit map layer. Open design points: ping-once vs persist-while-seen, whether inactive/garrison recruits report (early-warning net — the killer use case), rate limiting during firefights. Do after this feature ships — `resistance/recruit-contacts` or stretch phase 9.

---

## Session Notes

### 2026-08-18 — Parked-recruit wander fix: [move → wait] hold cycle (user report)
- **Root cause found:** `SpawnWaitWaypoint(pos, time)` **ignores its `time` parameter** (`OVT_OverthrowConfigComponent.c:497` never calls `SetHoldingTime`), so the BUG-170 "24 h" hold actually ran on the vanilla prefab default — `AIWaypoint_Wait.et m_holdingTime` is **60 s**. The wait completed after a minute, the group was waypoint-less, and vanilla idle AI wandered.
- **Fix (user-suggested cycle):** parked groups now get `AIWaypointCycle` `[move@pos → wait@pos]` with `SetRerunCounter(-1)` — the exact town perimeter-patrol shape (`GivePatrolWaypoints`), which also proves return-fire still works. Wait's holding time is now applied for real via `SetHoldingTime(INACTIVE_HOLD_WAIT_SECONDS)` at the call site, so the loop is silent between laps; the move leg self-heals position after combat displacement. Degraded fallback (prefab spawn failure) = single wait with real 24 h hold.
- `OVT_InactiveRecruitGroupComponent` now owns an **array** of waypoints (`AddOwnedWaypoint`, replaces `SetWaypoint`/`GetWaypoint`) — cycle children are separate entities `SetWaypoints` does not parent, all three deleted in `OnDelete`.
- **Helper FIXED too (user approved):** `SpawnWaitWaypoint` now calls `SetHoldingTime(time)`. All six call sites audited — every one passes a sane positive value (15–50 s town/base patrols, 45–75 s perimeter/FOB, 60 s deployment module guarded by `> 0`, 86400 s parked recruits), so no instant-complete hazard. Visible behaviour change: town/patrol waits now use their intended randomised times instead of the prefab's flat 60 s. The recruit call site's redundant `SetHoldingTime` workaround was removed again.
- Compile green. Play-test owed: park a recruit, wait >2 min (previously they'd wander at ~60 s), confirm still holding; park, aggro them away, confirm they walk back; no bark loop (BUG-170 regression check).

### 2026-08-18 — Tent spawn point + 3 m scatter (user request)
- User added an `OVT_SpawnPointComponent` (offset 0 0 7) to `OVT_RecruitmentTent.et` in Workbench (uncommitted prefab change in the tree). Both tent spawn paths now use it: `ResolveTentSpawnPosition` prefers the tent's own spawn point over the forward-axis anchor, then scatters uniformly in a `TENT_SPAWN_SCATTER_RADIUS = 3` m disc (ground re-clamp after scatter; `skipSpawnPointSearch` stays true — the tent's point is read directly, never via `FindSafeSpawnPosition`'s 15 m closest-component search, which would still find base/FOB points).
- Legacy path parity: `RpcAsk_RecruitFromTent` only carries a position, so `SpawnTentRecruit` now recovers the tent root server-side (`FindTentAtPosition` — 10 m sphere query filtered on `OVT_BuildableComponent` type `RecruitmentTent`, NOT on `OVT_SpawnPointComponent`, which bases/FOBs also carry; filter mirrors `FindTownMarker`'s store-and-return-false shape). Legacy recruits therefore also gain the tent's facing via `ResolveTentSpawnAngles`.
- Compile check green. Suites not run (user in Workbench — never run suites then). Play-test owed: recruit + buy-equipped at a tent built near a base (must NOT land on the respawn marker), several purchases spread out ~3 m, tent on a slope.

### 2026-08-15 — Play-test PASSED (all green, user-attested); Buy action made instant
- User play-tested the full feature: **all green, works well.** One tweak requested and applied: `OVT_BuyEquippedRecruitAction` on the tent had `Duration 1.5` (held); the sibling Recruit Civilian action is instant, so the Duration line was removed — both tent actions now instant. (The action only opens the picker; the money step is the explicit Buy button, so a hold added nothing.)
- **Localization re-export DONE by the user** — verified: `OVT-Recruit_BuyButton`, `OVT-Tutorial_RecruitsEquipped_Title`, `OVT-FieldManual_Recruits_Head8` all present in `localization_Overthrow.en-us.conf`.
- **Key-swap sweep DONE** — 20 literal sites swapped to exported keys across `OVT_RecruitsContext`, `OVT_RecruitListEntryHandler`, `OVT_LoadoutsContext`, `OVT_MapLayersUI`, `RecruitsMenu.layout`, 3 character prefabs and the tent prefab. Parameterised sinks use `WidgetManager.Translate` (fare precedent `OVT_OverthrowMapUI.c:1238`). Compile clean. The 5 remaining sites (4× plain "Buy Recruit", 1× "Buying...") got their own `.st` items (`OVT-Recruit_BuyButtonPlain` `{6B4C00000000008F}`, `OVT-Recruit_Buying` `{6B4C000000000090}`), the user re-exported a second time, and **all 5 were swapped + layout label — zero TODO-localize literals remain in the feature.** Compile clean. Next free GUID: 91+.

### 2026-08-14 — Phase 9 built (server + UI halves); BUG-170 fixed
- **Server half (ADVANCED):** `OVT_RecruitPurchaseRules` (pure charge/outcome arithmetic, Logic-pinned), `OVT_RecruitLoadoutPricing` (recursive price walk, `IsRegisteredResource` gate — unpriceable refuses naming the item; `GetPrice` returns 500 for unknown ids and `GetInventoryId` resolves unregistered prefabs to id 0, so the gate is the only safe path), quote/buy RPCs + 11 result codes (14–24) on `OVT_RecruitCommandComponent` (Rpc ledger now 9/9 arity-checked), `recruitLoadoutFeeMultiplier` in all six difficulty confs (default 1.5; `<= 0` → default; NOT in the config JIP stream — quotes carry prices, `CONFIG_STREAM_VERSION` still 2), shared `SpawnTentRecruit` internals (legacy `RpcAsk_RecruitFromTent` now delegates; placement hardening: tent forward +4 m, ground clamp — `FindSafeSpawnPosition` ignores its own ground var — `skipSpawnPointSearch=true`). 7 fault-injection can-fail proofs, all compiled clean. Funds check runs LAST so refused quotes still carry a price.
- **UI half:** loadouts screen purchase mode — `PurchaseButton` (`KC_B` / `gamepad0:left_trigger`; checker 0/0; `OverthrowLoadoutsContext` is Flags 4 non-exclusive so LT-vs-ADS needs pad eyes), stale quotes dropped by loadout-name comparison + no-buy-while-pending guard, `MenuSelect` deliberately does NOT buy (money only on the labelled action), success closes via deferred `CallLater`, `OnClose` teardown proven by the reopen-five-times test item. Box mode unchanged.
- **BUG-170 fixed (main thread):** defend waypoint → `SpawnWaitWaypoint(pos, INACTIVE_HOLD_WAIT_SECONDS=86400)`; town patrols prove wait-AI still returns fire (`OVT_TownController.c:175`). Closed in tracker; play-test confirm owed (no barks + returns fire).
- **GUID ledger:** 70–7E (server: action/UIInfo/13 `.st`), 7F–84 (UI: input action + button). **Next free: 85+** (help-sync agent told to use 85+).
- Concurrent tree note: the user re-exported the six runtime localization `.conf`s mid-session (Phase 1–8 keys now render; Phase 9's 13+ keys are NOT in that export yet — another re-export owed) and dropped real atlas art into `overthrow_mapicons.imageset` region row 4.
- Gate: compile clean throughout; **All-group run covering Phase 9 + BUG-170 + duration tweaks still owed** (user at the machine — Workbench open; defer until clear).

### 2026-08-14 — Play-test feedback (Phases 1–8) + live tweaks
- **BUG-170 filed:** parked recruits bark repeatedly / glitch — defend waypoint re-order loop suspected; user wants a plain hold. Fix queued to land right after the Phase 9 server agent releases the manager file. Must verify return-fire behaviour survives (F2) — if a wait waypoint makes them ignore attackers, tune the defend preset instead.
- **Hold durations shortened** per user ("a few secs is enough"): park 3→**1.5 s**, recall 2→**1 s**, swap 5→**2.5 s**, applied to all three character prefabs (per-instance `Duration`).

### 2026-08-14 22:07 — Phase 8 complete; FEATURE BUILD COMPLETE
- Help/docs sync landed: `recruitsHoldPosition.conf` + `recruitsSwapGear.conf` tutorials (GUIDs 61/65, registered in `OVT_TutorialManagerComponent.m_aEntries` — an unregistered conf never loads), 3 Field Manual sections (69–6E), 10 new + 1 revised `.st` items (57–60). **Deviation from D14 (justified):** tutorial/FM configs use `#OVT-…` keys, not literals — every shipped tutorial/FM entry does, and the popup title doubles as the manual deep-link. They render raw until the user re-exports localization in Workbench.
- Wiki: recruits (pageId **29** — search returned 6, wrong; verified by content), player-groups (55), loadout-manager (30). All re-read after write.
- Fact-check cut: "parked recruits equippable from loadout screen" (untraceable — loadouts code never calls the radius getters); status cadence numbers (implementation detail).
- **BUG-167 filed** from the fact-check: join dialog counts whole roster (`SCR_GroupSubMenuBase.c:444-462`) vs transfer excludes parked (`OVT_GroupRecruitTransfer.c:84`).
- Swap-tip trigger is a compromise (`MENU_OPENED` on loadouts screen — no "action performed" event exists in `OVT_TutorialTrigger.c:12-45`; a proper trigger belongs to tutorial-system).
- Session totals: 8/8 phases, 57/57 tasks. Test gates: All 143 (P1) → All 148 (P2) → Fast 108 (P3) green; **one All-group run covering P4–P7 still owed** (user in game; process monitor armed to run it when the client exits). BUG-107 closed, BUG-166/167 filed.

### 2026-08-14 — Phase 7 complete (loadout swap, ADVANCED)
- `compile-check.sh` exit 0. **Q9 grep empty** (`SpawnEntityPrefab|TryDeleteItem|DeleteEntityAndChildren|TrySpawnPrefabToStorage` absent from `OVT_LoadoutSwap.c`, comments included). No `.conf` localization touched; the two dead FIA prefabs untouched.
- **THE FINDING THAT SHAPED THE WHOLE PHASE — the instigator rule.** `SCR_InventoryStorageManagerComponent.ShouldForbidRemoveByInstigator` (`:555`) refuses to let one character's storage manager REMOVE an item from a **living** character's storage (consumables exempt — that is why a medic can push a tourniquet into a patient). Both sides of a loadout swap are alive, so *every* move would have silently returned false if instigated by the wrong manager. The rule the file follows everywhere: **a removal is instigated by the manager that owns the storage being emptied; an insertion may be instigated by either.** `TrySwapItemStorages` necessarily removes from both sides at once, so it is tried (guarded by `CanSwapItemStorages`, on both managers) but never relied on — the three-step fallback behind it needs no permission at all.
- **Pairing model:** slot class = the same *named place* on both characters — the loadout **area typename** for clothing, the **weapon slot index** (`GetWeaponSlotIndex()`, which is also the destination slot id) for weapons. Items are never compared to each other, so a pistol cannot pair with a rifle. Clothing order is **outermost-first** (Back, Vest, ArmoredVest, Jacket, Pants, Boots, HeadCover, Handwear) for the `IsAreaBlocked` hazard (R6).
- **Deliberately NOT swapped** (all documented in the class header): identity/saline/tourniquet storages (no loadout area → excluded for free), the equipment storage (binoculars/watch — sibling component of the medical ones, not worth going near), the hand slot and `CharacterHandWeaponSlotComponent` (`SelectWeapon` is not synchronised), and container contents individually (a rucksack moves as one entity with everything inside it).
- **Deviations:** ① `LoadoutHandwearSlotArea`, not the plan's `LoadoutHandwearArea` — the latter is not a class that exists, and the string comparison at `OVT_InventoryManagerComponent.c:819` has therefore never matched (latent, not fixed here). ② Result class carries **four** counters, not three — `m_iRelocated` ("crossed but not into the matching slot") forces PARTIAL, because "your jacket is in their rucksack" reported as a clean swap is undiagnosable. ③ Swap outcomes use a dedicated `RpcDo_SwapLoadoutResult(code, exchanged, misplaced, failed, dropped)`; validation refusals still go through `SendResult`/`ReasonKeyFor`. ④ Count-bearing hints are literal English with TODO-key comments (keys exist in `.st`); the new *refusal* codes return keys, staying consistent with the sibling `ReasonKeyFor`. ⑤ No in-vehicle refusal for the swap (unlike parking) — an inventory move does not strand a body.
- **Result codes added:** SWAP_OK 7 / SWAP_PARTIAL 8 / SWAP_FAILED 9 / RECRUIT_INACTIVE 10 / TOO_FAR 11 / RECRUIT_DOWN 12 / NO_ACTOR 13. `SWAP_MAX_DISTANCE = 5` m (action radius 2 + slack). Rpc() ledger now **five** call sites, all hand-checked against handlers (BUG-090).
- **Counters are a PARTITION** — every touched item lands in exactly one of exchanged/relocated/failed/dropped, which is what makes the Q3 item-count invariant readable straight off the summary log line. One bug was found and fixed during review: a rewind-dropped item was counted as both dropped *and* failed; `Rewind` now returns its drop count.
- **GUID ledger:** 04–09 prefab actions/UIInfo, 50–56 `.st`. **Next free: 0A–0F, 57+.**
- **Play-test owed (item-count invariant, plan step 9):** open both inventories and count every item on player + recruit; hold Swap Gear; recount — `before(player)+before(recruit) == after(player)+after(recruit)`, and every item is on one of the two characters or on the ground beneath them. Then: swap with a recruit wearing armour that blocks a jacket slot; swap when one side has no rucksack; swap while holding a rifle (both should end with nothing selected, nothing in hands lost); check the console for `[Overthrow] Loadout swap` WARNING lines naming any item that did not cross. MP: a client swapping with its own recruit on a dedicated server (the whole routine runs server-side, so this is the one that proves the instigator rule holds over the wire), and a second client's refusal on somebody else's recruit.

### 2026-08-14 — Phase 6 complete (map marker layer)
- Compile exit 0. Registration only in `MapOverthrow.conf`. `TagImage` is a sibling of `Image` (verified — badge stays upright while the marker rotates). No `SetOpacity` in `SetMarkersVisible`; opacity written only in `Update()`.
- Entity resolve: `GetRecruitEntity()` → `FindRecruitEntity` already branches server (`m_mEntityToRecruit`) vs client (`m_mRplIdToRecruit` → `Replication.FindItem`); null → `m_vLastKnownPosition` fallback with rotation 0.
- Deviations: label is literal "Recruits" (TODO key named); no subscription to `m_OnRecruitActiveStateChanged` (per-frame reads make rebuild wasteful); tag quad change-filtered (`m_mTagQuads`) so `LoadImageFromSet` isn't per-frame; `ClearMarkers` uses `RemoveFromHierarchy` (live rebuilds would orphan widgets otherwise); base marker = `recruit` imageset quad (clothes placeholder until real art at atlas row 4).
- **GUID ledger:** 3E layout meta, 3F `.st`, 47-49 widgets, 4A-4B conf entries. **Next free: 04–0F, 50+.**
- **Play-test checklist (9 steps) owed:** badge upright while turning; live dim on park with map open; unstreamed fallback position (not 0 0 0); filter row toggle + restart persistence; zero-recruits → no row; dismiss → no orphan marker; MP own-recruits-only.

### 2026-08-14 — Phase 5 complete (roster screen)
- Compile exit 0; `check-input-conflicts.py` 0 errors/0 warnings. No `.conf` localization touched.
- **Binding deviation:** `gamepad0:shoulder_left` is VON (`VONContext` p110, always live) — checker caught it; rebound to `gamepad0:left_trigger`. Keyboard `KC_G` hand-checked clean within `OverthrowRecruitsMenuContext` (other `KC_G` users are disjoint contexts).
- New invoker `m_OnRecruitActiveStateChanged(OVT_RecruitData, bool)` on the manager — fired from `SetRecruitInactive` (server/listen-host) and `RpcDo_RecruitActiveStateChanged` (clients); no double-fire. Phase 6's layer may subscribe, though per-frame `m_bInactive` reads remain the plan.
- Imageset entries `recruit`/`recruit_ammo`/`recruit_ammo_empty`/`recruit_wounded` ADDED (aliasing placeholder regions; real-art targets recorded in plan §3.6 — no comments in `.imageset`, format-support unverified). Imageset GUID: `{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset`.
- Fixed pre-existing duplicate GUID: `ActionButtons` no longer shares `{5D5C558A6E391668}/{...69}` with `SelectedHometown`.
- 250 ms toggle debounce (button invoker + OnActiveFrame poll both fire on one press — existing screen pattern).
- **Known limitation (cheap follow-up):** row status icons read at build time; the 10 s push has no invoker on `OVT_RecruitCommandComponent` yet, so icons refresh on open/roster-change/park-unpark, not live.
- **GUID ledger:** 26–3D consumed (widgets 26–2F, button 30, icons 31–36, input 37–3B, ActionButtons re-GUID 3C–3D), `.st` 40–46. **Next free: 3E, 3F, 47+.**
- **Pad/mouse play-test checklist (11 steps) — owed to the user:** sections split + selection preserved across park; G / LT exactly-once; boundary walk on pad; no-body disabled reason; in-vehicle refusal; ~10 s icon refresh; MP listen-host + client roster agreement.

### 2026-08-14 — Phase 4 complete (status derivation + owner push)
- `compile-check.sh` exit 0. Both acceptance greps clean (no `OVT_Global`/`GetGameMode` under `TestSuites/Logic/`; sweep never calls `SyncRecruitPositions`).
- **hasAmmo definition:** any carried weapon whose current muzzle reports `GetAmmoCount() > 0` **or** for which `InventoryStorageManagerComponent.GetMagazineCountByWeapon()` > 0 (engine query, same one vanilla AI uses at `SCR_AICombatComponent.c:530` — magazine-well compatibility is the engine's answer, not hand-rolled).
- **Phase 5/6 read the cache as:** `OVT_Global.GetRecruitCommands().GetStatusFlags(recruitId)` → int mask; decode with `OVT_RecruitStatus.IsArmed/HasAmmo/IsWounded/IsUnconscious(flags)`; map tag quad from `OVT_RecruitStatus.TagIcon(flags)` (`""` = hide the tag widget). Unknown recruit = 0 = "no tag", never an error.
- Sweep runs every `OVT_RecruitManagerComponent.STATUS_SYNC_INTERVAL_MS` (10 s), started unconditionally in `EOnInit` and server-guarded **inside** the tick; it still writes `m_vLastKnownPosition` (clustering depends on it).
- Status cache is client-side on the command component, pruned via a lazily-made `m_OnRecruitRemoved` subscription (manager can be null at controller init) and dropped in `OnDelete`.
- Rpc() ledger now three call sites, all arity-checked by hand against their handlers (BUG-090) — listed in the component's class header.

### 2026-08-14 21:30 — Phase 3 complete
- Fast 108/108 green. No `.conf` localization files touched (verified).
- Result codes on the component: OK 0 / NO_RECRUIT 1 / NOT_OWNER 2 / NO_BODY 3 / IN_VEHICLE 4 / NO_CHANGE 5 / FAILED 6. `ReasonKeyFor` returns "" for OK **and** NO_CHANGE; NO_RECRUIT and NOT_OWNER share `#OVT-Recruit_NotYours` (never reveal another player's recruit exists).
- **Scope addition:** parking (deactivating) a recruit in a vehicle is refused — client reason + server re-check via engine-level `CompartmentAccessComponent.IsInCompartment()`. Reactivation unguarded (intended).
- Init case spawns the controller prefab and FindComponents (Init tier has no campaign, so no live controller); live accessor checked opportunistically.
- **GUID ledger `{6B4C0000000000XX}`:** 01/02 Phase 2, 03 component, 10–1B prefab actions/UIInfo, 20–25 `.st`. **Next free: 04–0F and 26+.**
- Phase 4: extend this component (status cache + `RpcDo_RecruitStatus`); copy `SendResult`'s listen-server shape for new Owner RPCs; keep Rpc() call-site count auditable (BUG-090).
- Phase 7: extend `RESULT_` block with new codes (don't reuse); swap action appends to the same three `additionalActions` blocks; `IsInCompartment` already on the base action.
- MP gate owed (manual): two-client ownership refusal, listen-host both directions, JIP action visibility on an already-parked recruit.

### 2026-08-14 21:00 — Phase 2 complete (ADVANCED)
- All group 148/148 green. Both acceptance greps empty (no lifecycle/cleanup identifiers in the manager; no manager accessors under Logic/).
- **Deviations (all sound):** `RemoveRecruitFromSlaveGroup` returns bool (caller's removed-count); the slave-group exit lives inside `PlaceRecruitInInactiveGroup` (idempotent — handles already-parented bodies since `AddAIEntityToGroup` refuses parented agents, and whether `DeactivateAI` unparents is unverified); `PlaceRecruitInWorld` falls back to the player group on inactive-placement failure (logs WARNING, keeps `m_bInactive` — mismatch is a symptom, not hidden); fare-preview parity: `OVT_FastTravelService.CountRecruitsInRadius` also excludes parked recruits (record-shaped getter got the same param); `m_aUnitPrefabSlots` omitted (matches `Group_Player.et`); marker uses `ScriptComponentClass` not `OVT_ComponentClass` (no config/weather resolve needed).
- **Phase 3 must:** call `SetRecruitInactive(recruitId, inactive)` — it validates existence/liveness/is-a-change but **NOT ownership**; the RpcAsk re-checks `m_sOwnerPersistentId` vs `ResolveOwningPlayerId()`. `false` return includes no-op requests. Do not broadcast again; do not touch replicas optimistically. No-body refusal is the roster's disabled-button condition.
- **Phase 4 must:** keep the sweep writing `m_vLastKnownPosition` (clustering depends on it — `PlaceRecruitInInactiveGroup` also writes it).
- **Phase 5:** `GetPlayerRecruitsByState` = section split, `IsRecruitInactive` = per-row read, both client-safe. Capacity unchanged (inactive counts toward 16).
- Open (minor): parking a recruit inside a vehicle leaves the body in the compartment — consider a Phase 3 guard or accept. `SelectTransferable` now takes 4 args.

### 2026-08-14 20:10 — Phase 1 complete
- All group green (143 tests). Can-fail proofs recorded in test preambles (fault-injection, all compiled clean — compiler can't see them, which is the point).
- **Deviation D-P1a:** `ApplyPersistedRecruits` adopts `inactive` **unconditionally**, not under the `live == ""` guard `bodyPersistenceId` uses — inactive is a campaign fact, peer of `m_bIsTraining`. If Phase 2 decides a live re-apply must not yank a recruit from its current group, that line is the place to revisit.
- **Deviation D-P1b:** T1.9 tests write the record field directly (no server mutator exists yet). Phase 2 should switch the write side to `SetRecruitInactive` once T2.7 lands.
- For Phase 2+: JIP format is now …skills block… then one trailing bool — anything new appends *after* it, both sides. `RpcDo_RecruitActiveStateChanged` does not create missing records (JIP/`RpcDo_RecruitCreated` carry the state); `BroadcastRecruitActiveState` deliberately has no listen-server short-circuit (handler self-guards; BUG-164 rule applies to Phase 3's RpcAsk helpers instead). Comments at ~:1830/:1949 about `FindRecruitEntity` pruning are still true — do not "clean them up".
- Encountered the `owned` reserved-keyword trap once (renamed local to `ownedRecruits`).

### 2026-08-14 19:52
- Feature started via /autorun-feature (Discord). Docs scaffolded from the existing plan; status flipped to In Progress.
- Next: Phase 1 implementation agent.

### Loadout quote "item has no price" (2026-08-23, post-close fix)

Players hit `RESULT_UNPRICEABLE` on loadouts wearing looted civilian clothes (vanilla's `*_Dirty`
variants live only on character prefabs, in no ITEM catalogue) and on weapon presets/modded guns.
`OVT_RecruitLoadoutPricing.AddResource` gated on `IsRegisteredResource`, and the resource database is
built from faction catalogues only - so "has a price" meant "is catalogued", which nothing a player
loots is guaranteed to be. The `itemPrices.conf` type rules are NOT a fallback for that: they only ever
ran over catalogue entries.

Fix: `AddResource` now prices through the new `OVT_EconomyManagerComponent.GetBuyPriceForPrefab(res,
pos, player)` - (1) registered → as before; (2) `ResolvePricingResource`: nearest registered prefab
**ancestor** (dirty trousers → `Pants_M70`), same price, same town curve; (3) `GetFallbackBasePrice`:
`OVT_PrefabItemClassifier` reads the prefab's components (WeaponType / clothing AreaType / magazine /
consumable / gadget / mine) and runs the same price configs, margin applied, no town curve; (4) -1 only
when none of those place it. **Nothing is ever registered** - the int ids are the wire format. Both
lookups are cached per prefab. Case: `OVT_TEST_Init_EconomyPrefabPricing` (compile clean; the suite run
is owed - `tools/run-tests.sh OVT_TEST_Init_EconomyPrefabPricing`). Play-test: save a loadout wearing a
civilian's dirty shirt + a looted AK-74N 1P29, quote an equipped recruit.

Same day, the other gates were moved onto the seam: shop sell (server `OVT_ShopTransactionComponent`
+ client sell browser `OVT_ShopContext`), port export price (`OVT_StorageRequestComponent.
ResolveExportUnitPrice`), port/warehouse browse categories, the high-command manifest and vehicle quote,
and vehicle storage-capacity identity (`OVT_StorageComponent`). Where an int id is needed (sell, export,
category, capacity) only the ancestor route applies - a looted dirty shirt sells/exports/lists AS the
clean catalogue shirt and restocks it; a prefab with no registered ancestor stays unsellable. Price-only
callers (HC manifest/vehicle) use all three routes.

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
