# Vanilla Persistence — Manual Play-Test Checklist

**Created:** 2026-08-02 (overnight autorun, Phases 1-6 complete)
**Status of automated coverage:** compile clean · Fast 20/20 · All 42/42 (incl. the 10-case round-trip gate, formerly quarantined — its exit 1→0 flip discharged the migration's acceptance criterion; the 10th case is the GitHub #143 per-instance vehicle despawn→storage→respawn round trip added 2026-08-02)

> ⚠️ **BREAKING CHANGE — player-facing notice needed before release:** all existing EPF save files are dead. There is no converter. Players (and your own campaigns) start fresh. Server operators: the game now saves through Reforger's native save system (`profile/.save/.../game/<mission>/playthrough/savepoint`); the old `.db/Overthrow` tree is ignored and can be deleted.

Everything below is what the harness **cannot** reach: real quit→continue (a world transition restarts the autotest harness), JIP/MP (needs two clients), UI, and live AI behavior. Ordered by risk — **do section A first**.

> **RESULTS 2026-08-03 (SP run, first clean pass):** items **1–15 all ✅** with two notes: item 10 "hard to tell but probably ok if everything else is"; **item 12c ❌ — corpses do not survive a continue → BUG-018** (user-rated minor: EPF never persisted corpses either; new capability, not a regression). Five fixes landed during the run (see context.md 2026-08-03 session notes): pre-start spawn stealing the home, continue path unwired, Restart hijacked → chooser screen (×2 UX iterations), ammo-box VME (vanilla 1.7 landmine, modded guard), chooser not closing on Continue. **Section D (items 16–20, MP/dedicated) pending a dev build on a server.**

## A. Spawn & lifecycle (highest priority — spawn logic was re-parented off EPF in Phase 5)

1. **You spawn at all.** New campaign → Start. You must get a character at your home in civilian clothes. (The EPF spawn base was measurably dead on this branch — character creation is now Overthrow-owned code that has never run in this exact form.)
2. **You respawn after dying — with NOTHING.** Kit yourself out first (rifle, vest, loaded backpack), then die. You must be charged the respawn cost and get a new character one frame later **at home, in the civilian loadout, carrying none of it**. Corpse stays lootable where it fell. *(Death = complete loss is the design; if you respawn with your gear, that is a bug in the new player-body restore, report it immediately.)*
3. **Your squad forms.** ~3s after spawning you're in a group named after you (console: `Created group N for player ...`). Recruits depend on this hook.
4. **SP save→quit→continue.** Save from the menu (it should now HONESTLY report success/failure — no more unconditional "Saved"), quit to menu, Continue. Campaign resumes started (no start menu), your money/XP/skills/home intact, no starting cash re-granted.
5. **Your character survives 60s after continuing.** Vanilla's reconnect sweep runs ~1 min after load; it's expected to find nothing. If your character vanishes at ~60s, report it — `SCR_PlayerReconnectData` then needs scoping out in `Overthrow.conf`.

## B. World state across continue

6. **No double AI.** Note patrol/garrison counts at one occupied base; save/quit/continue; counts must match (doubling = the `SelfSpawn 0` AI overrides aren't taking). Watch the opening minutes for a *second* opening build-out (the `m_bDistributeInitial` guard).
7. **Placeables survive.** Place camp + ammo box (with contents) + poster; build a guard tower. All four back after continue, right places, box contents + ownership/lock intact.
8. **Deleted stays deleted.** Remove a placed object, save, continue — it must NOT return (the statically-unverifiable `SelfDelete` default).
9. **Base compositions.** Bunkers/MG nests/caches back in the SAME slots. A bounded extra composition minutes later is the known `m_Spawned` debt, not a regression.
10. **Deployments.** Enemy deployment count + faction resources persist; no second wave on continue.
11. **Jobs.** A job in progress stays at its stage, no payout on continue. Kill-target jobs are expected to re-offer.
12. **Camp objects belong to their camp.** After continue, deleting the camp removes its objects.

## C. Player systems

12b. **YOU come back with your gear, where you left off.** Kit yourself out with something identifiable (a specific rifle, a vest, a backpack with a named item in it), walk somewhere that is **not** your home, save, quit, Continue. You must spawn **at the spot you saved**, carrying **exactly** what you had — not at home in a civilian shirt.
    - **Console line to look for:** `Requesting stored body <uuid> for player 1` followed by `Player 1 restored from their stored body, gear intact`.
    - Fallback lines (you still get a character, but report the exact one): `Persistence answered NOT_FOUND … - spawning a fresh one`, `No answer to player … - spawning a fresh one`, `Stored body for player … came back dead`.
    - Also check you were **not** handed the difficulty's starting items a second time, and that your money/XP are unchanged.
    - Reconnect half (MP or host): kit up, disconnect, reconnect — same character, same gear, same spot.

12c. **Corpses are still lootable after a continue.** Kill an armed enemy and, if you can, get one of your recruits killed too. Do **not** loot them. Save, quit, Continue. Both bodies must be back where they fell, **still carrying their weapons and inventory**, and still lootable. A dead recruit must NOT come back alive or rejoin your squad, and must not appear in the recruit UI.
    - Then leave a corpse alone for a long session and confirm it eventually vanishes. Corpses are meant to be cleaned up by the engine's garbage system, whose timers are supposed to survive the save (vanilla `GarbageSystemState`). That is engine-native and could not be verified statically — **an immortal corpse, or a save file that visibly grows across many firefights, is the one residual risk in this change.**
    - Sanity check: a restored corpse must be a CORPSE. If any body stands back up, walks, or shoots after a continue, stop and report it.

13. **Loadouts.** Save a loadout at an equipment box (**first time this has ever worked**), verify it lists + applies; survives continue; deleted loadouts stay deleted.
14. **Recruits come back WITH THEIR GEAR.** Recruit two civilians, **arm them differently** (give one a rifle + vest + backpack with something in it, leave the other in civilian clothes), move them somewhere distinctive, save/quit/continue. A few seconds after you spawn (rebuilt on group creation) they must be at the right spot with names/XP/level intact **and carrying exactly what you gave them** — same weapon, same vest, same backpack contents. No duplication on a second reload.
    - Also test the reconnect half (MP or host): with recruits armed, disconnect, wait >10 min for the offline despawn, reconnect — same recruits, same gear.
    - **Console line to look for:** `Recruit <id> restored from its stored body, gear intact`. If instead you see `Persistence answered NOT_FOUND …  spawning a fresh one` or `No answer to recruit … spawning a fresh one`, the fallback fired — the recruit is still there but in civilian clothes; report the exact line.
    - A recruit that **died** must NOT come back at all after a continue (unless the save predates the death).
15. **Vehicle ownership.** Buy + lock a car (and a heli), continue: still yours, still locked, still sellable.

## D. Multiplayer / dedicated (the classic regression class — nothing automated covers this)

16. **Second player joins** (hosted or dedicated): they get a character and their own group.
17. **JIP after load:** client joining a *continued* session sees correct towns/money/real-estate/recruits (JIP RplSave/RplLoad paths — untouched by the migration, but the state they ship is now vanilla-restored).
18. **Two-player isolation:** each keeps their own money/home/loadouts across a save/continue.
19. **Leaving player's locked vehicle COMES BACK ON RECONNECT** (MP or host) — *GitHub #143, changed 2026-08-02: this used to be a documented limitation and is now a fixed behaviour, so test it as a feature, not a caveat.* Park a locked, owned car somewhere distinctive, put something identifiable in its boot, note the fuel gauge. Disconnect that player and wait out the ~60 s offline timer — the car must vanish. Reconnect. **The car must be back at the same spot, still owned by that player, still locked, still holding the same cargo and the same fuel level.**
    - *Fuel nuance (2026-08-02):* the automated case found fuel restoring to the prefab-initial level on the **starting UAZ** — vanilla's fuel serializer only persists `SCR_FuelNode` tanks and the local UAZ chain's node typing is unconfirmed. So run this item with a **shop-bought** vehicle too: if the shop car keeps its fuel and the starting UAZ doesn't, it's a prefab data gap on the UAZ base (report it as such), not a persistence bug.
    - **Console lines to look for:** `Requesting stored vehicle <uuid> for <persId>` followed by `Vehicle <uuid> restored for <persId>, contents intact`.
    - Report the exact line if you instead see `persistence answered NOT_FOUND … - the registration was dropped` (the record was gone: the car will not come back and will not be re-offered), `no answer to the spawn request … within 15000 ms` (the request was never answered; a later reconnect retries), or `Restored vehicle … carried no owner - repairing it from the registry` (the car came back but its ownership record did not — a serializer problem).
    - **Reconnect twice in a row** and confirm you get exactly ONE car, not two.
    - **Reconnect during the grace period** (before the ~60 s timer expires): the car must never have vanished, and must not be duplicated.
    - Then still verify the other half: save→quit→continue must also bring it back, as before.
20. **Dedicated restart cycle:** run a dedicated server, play, stop the server process, start it again — campaign must continue (this exercises the purge guard; without it vanilla wipes the playthrough at game-mode end).

## Known accepted limitations (don't report as bugs)

- A recruit whose stored body genuinely cannot be found (wiped save data, changed prefab) comes back as a fresh civilian-loadout body rather than not at all. This is the deliberate fallback, and it announces itself in the console — see item 14.
- The same fallback exists for YOU (item 12b): a player whose stored body cannot be found spawns fresh at home in civilian clothes rather than not at all. It always announces itself in the console.
- Corpses from a save taken BEFORE this change do not come back — they were written under the old `SelfSpawn 0` configuration. Only deaths from now on persist.
- One bounded extra base composition / parked vehicle may appear after continue (`m_Spawned` not restored — pre-existing under EPF too).
- Shop stock re-randomizes every continue (never persisted; economy-scope debt).
- A vehicle whose stored record genuinely cannot be found comes back **not at all**, and its registration is dropped so it stops being asked for. Unlike a recruit there is deliberately no fresh-prefab fallback — minting a replacement car would hand the player a vehicle that never existed. It announces itself in the console — see item 19.
- A vehicle restored on reconnect comes back **unlocked** if its stored ownership record could not be read (the console says so). Ownership itself is repaired from the manager's registry; lock state is not, because nothing outside the record knows it.
- "Save failed" UI string is an unlocalized literal pending a localization key.

Findings → `/fix-feature core/persistence` or file bugs linked to `core/persistence`.
