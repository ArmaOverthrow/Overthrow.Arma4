# Core / Options - Task Checklist

**Last Updated:** 2026-09-08 23:20
**Progress:** 54/54 tasks complete (100%)
**Advanced phases:** P3 (`network-specialist-advanced`), P7 (`ui-developer-advanced`)

---

## Phase 1: The pure registry and its Logic cases (8/8 complete)

- [x] **P1.1 Enums and the definition record**
  - Description: Write `OVT_EOptionScope`, `OVT_EOptionType` and `OVT_OptionDefinition`.
  - File(s): `Scripts/Game/Data/OVT_OptionDefinition.c`
  - Estimate: 0.5 h

- [x] **P1.2 Registry skeleton**
  - Description: Register, idempotency, definition lookup, ids for a scope.
  - File(s): `Scripts/Game/Data/OVT_OptionsRegistry.c`
  - Estimate: 1 h

- [x] **P1.3 Codec and Normalize**
  - Description: Bool codec, clamped and snapped slider, choice key fallback.
  - File(s): `Scripts/Game/Data/OVT_OptionsRegistry.c`
  - Estimate: 1 h

- [x] **P1.4 SetValue, GetValue and typed getters**
  - Description: Stored value beats default, default beats empty.
  - File(s): `Scripts/Game/Data/OVT_OptionsRegistry.c`
  - Estimate: 0.5 h

- [x] **P1.5 Pending overrides**
  - Description: Store an unknown id as pending. Re-normalize on Register.
  - File(s): `Scripts/Game/Data/OVT_OptionsRegistry.c`
  - Estimate: 0.5 h

- [x] **P1.6 GetOverrides and GetPendingIds**
  - Description: Non-default, non-pending values in one list. Pending ids in another.
  - File(s): `Scripts/Game/Data/OVT_OptionsRegistry.c`
  - Estimate: 0.5 h

- [x] **P1.7 Scope predicates**
  - Description: Three static predicates that map a scope to a permission.
  - File(s): `Scripts/Game/Data/OVT_OptionsRegistry.c`
  - Estimate: 0.25 h

- [x] **P1.8 Logic cases**
  - Description: Eight cases from section 8. Each case header records how it was made to fail.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Options.c`
  - Estimate: 1.75 h

---

## Phase 2: The manager (6/6 complete)

- [x] **P2.1 Manager singleton**
  - Description: `s_Instance`, `GetInstance()`, `OnPostInit`.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 1 h

- [x] **P2.2 OVT_Global.GetOptions()**
  - Description: Add beside `GetHighCommand()`.
  - File(s): `Scripts/Game/Global/OVT_Global.c`
  - Estimate: 0.25 h

- [x] **P2.3 Game-mode prefab block**
  - Description: Add the manager block with GUID `6B8B000000000001`.
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 0.25 h

- [x] **P2.4 RegisterDefaults**
  - Description: Register the two autosave options with the shipped constants.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 1 h

- [x] **P2.5 Public API**
  - Description: `RegisterOption`, `OverrideDefault`, typed getters, `m_OnOptionChanged`.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 1.5 h

- [x] **P2.6 Init case**
  - Description: The manager resolves off the game mode and both built-in definitions exist.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_OptionsManager.c`
  - Estimate: 1 h

---

## Phase 3: The request seam, the permissions and the wire (8/8 complete), ADVANCED

- [x] **P3.1 Request component**
  - Description: `OVT_OptionsRequestComponent` with the `Replication.IsServer()` branch.
  - File(s): `Scripts/Game/Components/Controller/OVT_OptionsRequestComponent.c`
  - Estimate: 1 h

- [x] **P3.2 RpcAsk_SetOption validation**
  - Description: The seven-step validation of section 3.4. One WARNING line on rejection.
  - File(s): `Scripts/Game/Components/Controller/OVT_OptionsRequestComponent.c`
  - Estimate: 1 h

- [x] **P3.3 PlayerMayEditWorld**
  - Description: Admin role with the host fallback (D7). Verify the single-player answer.
  - File(s): `Scripts/Game/Components/Controller/OVT_OptionsRequestComponent.c`
  - Estimate: 1 h

- [x] **P3.4 SetOption and RpcDo_SetOption**
  - Description: Normalize, store, direct call, broadcast, invoker.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 1 h

- [x] **P3.5 RplSave and RplLoad**
  - Description: Stream version, definition signature, override pairs.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 1 h

- [x] **P3.6 Controller prefab block**
  - Description: Add the request block with GUID `6B8B000000000002`.
  - File(s): `Prefabs/GameMode/OVT_OverthrowController.et`
  - Estimate: 0.25 h

- [x] **P3.7 Controller seam case line**
  - Description: One line for `OVT_OptionsRequestComponent`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ControllerSeam.c`
  - Estimate: 0.25 h

- [x] **P3.8 Rpc arity audit**
  - Description: Hand-audit every `Rpc()` call site and record the audit in a comment.
  - File(s): both new files above
  - Estimate: 0.5 h

---

## Phase 4: Persistence (5/5 complete)

- [x] **P4.1 Serializer**
  - Description: `OVT_PersistedOption` and `OVT_OptionsManagerSerializer`, version 1.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_OptionsManagerSerializer.c`
  - Estimate: 1.5 h

- [x] **P4.2 ApplyPersisted and GetPersistableOverrides**
  - Description: Load applies and fires the invoker. Save reads the overrides.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 0.75 h

- [x] **P4.3 Serializer conf entry**
  - Description: GUID `6B8B000000000004`.
  - File(s): `Configs/Systems/Persistence/Overthrow.conf`
  - Estimate: 0.25 h

- [x] **P4.4 Pending id warning**
  - Description: Log dropped pending ids one time at save (D6).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 0.25 h

- [x] **P4.5 Round-trip case**
  - Description: One case in `OVT_TEST_PersistenceRoundTripSuite`.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/` (existing suite file)
  - Estimate: 1.25 h

---

## Phase 5: The LOCAL profile store (5/5 complete)

- [x] **P5.1 Pure local store**
  - Description: Key and value set, version rule, cap.
  - File(s): `Scripts/Game/Data/OVT_OptionsLocalStore.c`
  - Estimate: 1 h

- [x] **P5.2 Profile module**
  - Description: `OVT_LocalOptionEntry` and `OVT_OptionsLocalSettings` as a nested record.
  - File(s): `Scripts/Game/Global/OVT_OptionsLocalSettings.c`
  - Estimate: 0.5 h

- [x] **P5.3 Accessor**
  - Description: `Load`, `Save`, `Reset`, `GetModuleContainer`, `IsConsoleApp` early-out.
  - File(s): `Scripts/Game/Global/OVT_OptionsLocalSettingsAccessor.c`
  - Estimate: 1 h

- [x] **P5.4 Wire into the manager**
  - Description: `LOCAL` reads and writes go through the store.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c`
  - Estimate: 0.5 h

- [x] **P5.5 Logic cases**
  - Description: Three cases from section 8.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_OptionsLocalStore.c`
  - Estimate: 1 h

---

## Phase 6: The autosave consumer (6/6 complete)

- [x] **P6.1 StopAutosaves and RestartAutosaves**
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 0.5 h

- [x] **P6.2 Registry-driven StartAutosaves**
  - Description: Read the registry. Fall back to the attributes when the manager is absent.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 0.75 h

- [x] **P6.3 OverrideDefault from OnPostInit**
  - Description: Two calls, server only.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 0.25 h

- [x] **P6.4 Subscribe to m_OnOptionChanged**
  - Description: One time, behind a flag, inside `StartAutosaves()`.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 0.5 h

- [x] **P6.5 Unsubscribe in OnDelete**
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 0.25 h

- [x] **P6.6 Init case**
  - Description: Interval change flips the scheduled flag. Disable clears it. No wall clock.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_OptionsAutosaveRearm.c`
  - Estimate: 1.5 h

---

## Phase 7: The generated form (10/10 complete), ADVANCED

- [x] **P7.1 Four layouts and .meta files**
  - File(s): `UI/Layouts/Menu/OptionsMenu.layout`, `UI/Layouts/Menu/OptionsMenu/*.layout`
  - Estimate: 2.5 h

- [x] **P7.2 Context skeleton**
  - Description: `OnShow`, `OnClose`, the section walk, empty sections skipped.
  - File(s): `Scripts/Game/UI/Context/OVT_OptionsContext.c`
  - Estimate: 1.5 h

- [x] **P7.3 Row builders**
  - Description: Toggle, slider and choice rows with the binding list.
  - File(s): `Scripts/Game/UI/Context/OVT_OptionsContext.c`
  - Estimate: 1.5 h

- [x] **P7.4 Change handlers**
  - Description: Three handlers behind `m_bPopulating`.
  - File(s): `Scripts/Game/UI/Context/OVT_OptionsContext.c`
  - Estimate: 1 h

- [x] **P7.5 Broadcast re-populate**
  - Description: Subscribe to `m_OnOptionChanged` and re-populate the row.
  - File(s): `Scripts/Game/UI/Context/OVT_OptionsContext.c`
  - Estimate: 0.5 h

- [x] **P7.6 Initial gamepad focus**
  - File(s): `Scripts/Game/UI/Context/OVT_OptionsContext.c`
  - Estimate: 0.5 h

- [x] **P7.7 Close button through m_OnClicked**
  - File(s): `Scripts/Game/UI/Context/OVT_OptionsContext.c`
  - Estimate: 0.25 h

- [x] **P7.8 Enable the main menu button**
  - Description: Remove `"Is Enabled" 0`. Add the handler in `OVT_MainMenuContext.OnShow`.
  - File(s): `UI/Layouts/Menu/MainMenu.layout`, `Scripts/Game/UI/Context/OVT_MainMenuContext.c`
  - Estimate: 0.5 h

- [x] **P7.9 Character prefab context entry**
  - Description: GUID `6B8B000000000003`.
  - File(s): `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et`
  - Estimate: 0.25 h

- [x] **P7.10 Localization**
  - Description: Ten items, GUIDs `6B8B000000000020` to `29`.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 1 h

---

## Phase 8: Review, docs and the play-test checklist (5/5 complete)

- [x] **P8.1 Definition of Done review**
  - Estimate: 1 h

- [x] **P8.2 GUID audit**
  - Description: Every reserved GUID marked as taken, none used two times.
  - Estimate: 0.25 h

- [x] **P8.3 Language diff check**
  - Description: `git diff --stat Language/` shows only the `.st` master.
  - Estimate: 0.1 h

- [x] **P8.4 Comment score**
  - Description: Score against `docs/features/dev-ops/code-cleanup.md`. Cut any essay.
  - Estimate: 0.75 h

- [x] **P8.5 Play-test checklist**
  - File(s): `docs/features/core/options/context.md`
  - Estimate: 0.5 h

---

## Phase 9: Help and documentation sync (1/1 complete)

- [x] **P9.1 Help, Field Manual and wiki**
  - Description: Tutorial popups, Field Manual entry, wiki page for the Options menu.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`, wiki
  - Estimate: 3 h
  - 2026-09-10: wiki published at path `options`, page id 75. Linked from `getting-started` (id 2) and `home` (id 1).

---

## Bugs & Issues

**Active Bugs:**
- None filed. Two partial DoD items are recorded in `implementation.md` section 7: no `CHOICE` consumer ships, and the toggle round trip is proven at its default only.

**Fixed Bugs:**
- None
