# Reforger 1.7.0 Vanilla Persistence — Verified API Reference

**Verified against:** `/mnt/n/Projects/Arma 4/ArmaReforger/scripts` (retail 1.7.0.54 extraction), 2026-08-02.
**Purpose:** the single source of truth for `core/persistence` implementation. Every claim carries a file:line. When in doubt, re-read the cited file — never trust the old plan's code samples.

Path abbreviations:
- `$GS` = `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game`
- `$CFG` = `/mnt/n/Projects/Arma 4/ArmaReforger/Configs/Systems/Persistence`

**Two layers — do not conflate:**
1. **PersistenceSystem** — a `WorldSystem` (server-only) that tracks instances and serializes them. Config-driven.
2. **SaveGameManager** — a global engine singleton owning save *points*, playthroughs, and load/restart transitions. This is what "press save" talks to.

---

## 1. PersistenceSystem / SCR_PersistenceSystem

`generated/Plugins/Persistence/System/PersistenceSystem.c` — `class PersistenceSystem: WorldSystem` (:12)

| Line | Signature |
|---|---|
| 14 | `static proto static PersistenceSystem GetInstance();` |
| 19 | `proto external bool Save(notnull Managed entityOrState, ESaveGameType saveType = ESaveGameType.MANUAL);` |
| 21 | `proto external void RequestSpawn(notnull PersistenceSpawnRequest spawnRequest, PersistenceResultCallback callback = null);` |
| 23 | `proto external void RequestLoad(notnull PersistenceLoadRequest loadRequest, PersistenceResultCallback callback = null);` |
| 25 | `proto external IEntity GetTrackedParent(IEntity entity);` |
| 27 | `proto external EPersistenceSystemState GetState();` |
| 31 | `proto external bool IsTracked(notnull Managed entityOrState);` |
| 38 | `proto external bool StartTracking(notnull Managed entityOrState, bool lazy = true);` |
| 40 | `proto external bool StopTracking(notnull Managed entityOrState, bool removeData = true);` |
| 42 | `proto external UUID GetId(Managed entityOrState);` |
| 49 | `proto external bool SetId(notnull Managed entityOrState, UUID id, bool makeAvailable = false);` |
| 51 | `proto external Managed FindById(UUID id);` |
| 53 | `proto external PersistenceCollection FindCollection(string resourceOrDisplayName);` |
| 59 | `proto external ref PersistenceConfig GetConfig(notnull Managed entityOrState);` |
| 64 | `proto external bool SetConfig(notnull Managed entityOrState, notnull PersistenceConfig config);` |
| 69 | `proto external bool ReloadConfig(notnull Managed entityOrState);` — re-evaluates rules, resets script customization |
| 85 | `proto external ref PersistenceDeferredDeserializeTask AddDeferredDeserializeTask();` ⚠️ zero vanilla call sites |
| 97 | `proto external void WhenAvailable(UUID uuid, notnull PersistenceWhenAvailableTask task, float maxWaitSeconds = 0.0, PersistenceDeferredDeserializeTask deferredDeserializeTask = null);` |
| 99 | `proto external bool WasDataLoaded();` |
| 100 | `proto external Managed GetPersistentState(typename stateType);` |
| 102 | `proto external bool ShouldKeepSessionData();` |
| 108 | `proto external void CommitStorage(typename storageType, PersistenceStatusCallback callback = null);` ⚠️ "inconsistent save points if misused" |
| 113 | `proto external void ClearStorage(typename storageType, PersistenceStatusCallback callback = null);` |

Native events (override in subclass): `OnStateChanged(old,new)` (:117), `OnBeforeSave(saveType)` (:119), `OnAfterSave(saveType, success)` (:125), `OnAfterLoad(success)` (:127), `HandleDelete(IEntity)` (:134).

**There is exactly ONE `Save()` overload** — it saves a *single* instance. Global saving is `SaveGameManager.RequestSavePoint()` (§5). There is no `TriggerSave()`.

`StartTracking` doc (:33-34): "Mostly for scripted states. IEntity should usually be registered by putting the PersistenceComponent on it!"

### SCR_PersistenceSystem (`$GS/Plugins/Persistence/System/SCR_PersistenceSystem.c`)

Invoker typedefs at :1-8. API:
- :69 `GetOnStateChanged()` → `ScriptInvokerBase<SCR_PersistenceSystem_OnStateChanged>` — `(EPersistenceSystemState old, EPersistenceSystemState new)`
- :75 `GetOnBeforeSave()` — `(ESaveGameType)`
- :81 `GetOnAfterSave()` — `(ESaveGameType, bool success)`
- :91 `sealed static bool IsLoadInProgress(float msSinceLoad = 1000.0)`
- :109 `sealed static SCR_PersistenceSystem GetScriptedInstance()`
- :115 `GetByCurrentWorld()`, :125 `GetByEntityWorld(IEntity)`

**No `GetOnAfterLoad()` invoker exists.** For an after-load hook, watch `GetOnStateChanged()` for `EPersistenceSystemState.ACTIVE` (vanilla does this: `SCR_ReconnectSerializer.c:45-48,145`).

Enums:
- `EPersistenceSystemState` (:12-19 of its file): `INIT, SETUP, ACTIVE, SHUTDOWN, FAILURE` — ordinal comparison legal (`GetState() < ACTIVE`).
- `EPersistenceStatusCode`: `OK, BUSY, UNAVAILABLE, DISK_FULL, WRITE_ERROR, NOT_FOUND, READ_ERROR, NOT_SUPPORTED, BAD_REQUEST, UNKNOWN_ERROR`.

### How tracking works — three mechanisms

1. **Native `Persistence` component on the prefab** (primary; class name in .et files is literally `Persistence`, no script binding exists). E.g. `ArmaReforger/Prefabs/MP/Modes/GameMode_Base.et:4`, `Prefabs/Characters/Core/Character_Base.et:41`; ~50 vanilla prefabs carry it (vehicles, items, weapons, groups, waypoints, buildings, doors…). Observed prefab attributes: `Enabled 0`, `Flags 0 0x1`.
2. **`StartTracking()` from script** for spawned/untagged entities — vanilla: `SCR_EditableEntityComponent.c:2266-2271` (placeables), `SCR_DestructibleBuildingComponent.c:442-444, 1337-1339`.
3. **`PersistentState` subclasses** for non-entity state (§4).

A tracked instance is matched to its `PersistenceConfig` at runtime via **rules in .conf** — there is no per-serializer registration API in script.

---

## 2. Collections & config (.conf-driven — NOT script-creatable)

`PersistenceCollection` is `sealed` with a private ctor ("Only constructed through the internal system"). Obtain via `PersistenceSystem.FindCollection(string)` only. Same seal applies to `PersistenceSystemConfig`, `PersistenceBundle`, databases, storages, bundlers.

Script-visible config fields:
- `PersistenceConfig` (sealed): `m_Collection`, `ESaveGameType m_eSaveMask`, `bool m_bSelfDelete`, `IsScripted()`.
- `EntityPersistenceConfig : PersistenceConfig` (sealed): `m_bSelfSpawn` ("Automatically spawn back the entity on load"), `m_bStorageRoot`, `EPersistenceParentHandling m_eParentHandling`.
- `ScriptedStatePersistenceConfig : PersistenceConfig` (sealed, no extra fields).
- `PersistenceConfigRule` — **script-extensible**: override `IsMatch(IEntity)`, `GetTypePriority()`, `Compare()`. Native subclasses: `PrefabPersistenceConfigRule`, `EntityClassPersistenceConfigRule`, `ComponentClassPersistenceConfigRule`, `PlayerPersistenceConfigRule`.

### Vanilla config chain (all under `$CFG`)

```
BaseSetup.conf                    ← databases, storages, "System" collection
  └─ Common.conf                  ← 10 gameplay collections + WorldState bundle + PersistentStates (Garbage/Door/Reconnect)
       └─ Mission.conf            ← "Logic" collection, task/SF states
            └─ EditableMission.conf  ← editor serializers
                 ├─ GameMode/Conflict.conf
                 └─ GameMode/GameMaster.conf
```

Collections in `Common.conf:3-42`: `Player`, `Character`, `Vehicle`, `Item`, `Turret`, `AIGroup`, `AIWaypoint`, `Structure`, `Storage`, `Misc` — all on session storage `{62488048AF9D8A55}` (defined in `BaseSetup.conf`, database `{6624ADA9A88DA0F6}` → `Database/BinarySaveGame.conf`). Derived confs append with the `+{ }` merge operator.

`Database/BinarySaveGame.conf` names script context classes as strings: `SerializationSaveContext "SCR_PersistenceBinarySaveContext"`, `SerializationLoadContext "SCR_PersistenceBinaryLoadContext"`, `Bundler "BlobBundler"`. **The retail save format is binary** (see §3.3 gotcha).

### Binding an entity/component to serializers — canonical .conf shapes

By entity class + component serializers (`$CFG/Configuration/GameMode/GameMode.conf:1-26`):
```
EntityPersistenceConfig {
 Rule EntityClassPersistenceConfigRule "{65ACD95F696D276B}" {
  EntityClass "SCR_BaseGameMode"
 }
 ParentHandling "Ignore always"
 EntitySerializer SCR_GameModeSerializer "{691481C816B7FF22}" { }
 ComponentSerializers {
  SCR_NightModeGameModeComponentSerializer "{65DA86351E4300EF}" { }
  ...
 }
}
```

By component class (`$CFG/Configuration/Storage/ResourceHolder.conf:1-13`): `Rule ComponentClassPersistenceConfigRule { ComponentClass "SCR_ResourceComponent" }`, `Priority 30000`.

By prefab (`$CFG/Configuration/Building/Door.conf:1-12`): `Rule PrefabPersistenceConfigRule { Prefabs { "{...}Door_Base.et" } }`, `StorageRoot 0`.

Extend an existing config file by inheritance (the pattern Overthrow needs — `$CFG/GameMode/Tutorial.conf:3-9` bolts `SCR_TutorialGamemodeComponentSerializer` onto the shared `GameMode.conf`).

`ParentHandling` conf strings: `"Ignore always"` / `"Ignore loaded"` / `"Ignore untracked"` → `EPersistenceParentHandling.IGNORE / IGNORE_LOADED / IGNORE_UNTRACKED` (plus `ACCEPT` default). `GenericEntitySerializer` accepts native conf keys `TransformMask a b` and `SavePrefabChildren 1` (not exposed in script). `SaveMask` never appears in shipped confs.

### Binding the system to a world

Via `SystemSettings` conf — `ArmaReforger/Configs/Systems/MissionSystems.conf`:
```
SystemSettings : "{1C60D2EDA2B468B8}Configs/Systems/BaseGameModeSystems.conf" {
 Systems {
  SCR_PersistenceSystem "{65DC893A5A752E89}" {
   SystemLocation Server
   SystemPoints 0x2000 0x10
   Config "{5BA7C4643477E2D7}Configs/Systems/Persistence/EditableMission.conf"
  }
 }
}
```
`SystemLocation Server` — the persistence system exists **only on the authority**. `SCR_PersistenceSystem` does not override `InitInfo`; registration is purely by this conf entry.

**Overthrow-specific fact:** `Overthrow.Arma4/Configs/Systems/ChimeraSystemsConfig.conf` **overrides the vanilla resource by GUID `{86E953538A28A98D}`** (vanilla `BaseGameModeSystems.conf:1` inherits exactly that GUID). Adding `SCR_PersistenceSystem { ... Config "<our conf>" }` to Overthrow's override injects the system into every world whose SystemSettings chain passes through ChimeraSystemsConfig — including the test world (`OVT_Campaign_Test.ent` is a SubScene of vanilla `MpTest/MpTest_Basic.ent`).

---

## 3. Serializers

### 3.1 Base classes (`generated/Plugins/Persistence/System/Serializers/`)

`PersistenceSerializerBase` (sealed as a direct base; extend the three subclasses):
- :15 `static event typename GetTargetType();` — "MUST be implemented!"
- :17 `static event EDeserializeFailHandling GetDeserializeFailHandling()` — default `IGNORE`
- :26 `proto external PersistenceSystem GetSystem();`
- :31 `event protected void Setup();`

`ScriptedComponentSerializer`:
- :18 `static event EComponentDeserializeEvent GetDeserializeEvent()` — default `AFTER_ENTITY_FINALIZE`
- :34 `event protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)`
- :39 `event protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)`

`ScriptedEntitySerializer`:
- :18 `static event EEntityDeserializeEvent GetDeserializeEvent()` — default `AFTER_FINALIZE`
- :42 `event protected ESerializeResult SerializeSpawnData(notnull IEntity entity, notnull SaveContext context, SerializerDefaultSpawnData defaultData)`
- :23 `event protected bool DeserializeSpawnData(out ResourceName prefab, notnull out EntitySpawnParams params, notnull LoadContext context)`
- :47 `event protected ESerializeResult Serialize(notnull IEntity entity, notnull SaveContext context)`
- :52 `event protected bool Deserialize(notnull IEntity entity, notnull LoadContext context)`

`ScriptedStateSerializer`:
- :20 `static event EScriptedStateDeserializeEvent GetDeserializeEvent()` — default `AFTER_CONSTRUCTOR`
- :36 `event protected ESerializeResult Serialize(notnull Managed instance, notnull SaveContext context)`
- :41 `event protected bool Deserialize(notnull Managed instance, notnull LoadContext context)`

`GenericEntitySerializer : ScriptedEntitySerializer {}` — the "just do the native thing" default; extend it for entity serializers (vanilla does).

`SerializerDefaultSpawnData: Managed` — `vector Transform[4]`, `ResourceName Prefab` (implicit spawn info, e.g. world-layer entities).

Enums: `ESerializeResult { ERROR, OK, DEFAULT }` (DEFAULT = "no custom data"); `ESerializeMode { NATIVE, SCRIPT, BOTH }`; `EDeserializeFailHandling { IGNORE, DELETE, ERROR }`; `EEntityDeserializeEvent { AFTER_CONSTRUCTOR, AFTER_FINALIZE }`; `EComponentDeserializeEvent { BEFORE_POSTINIT, BEFORE_EONINIT, AFTER_ENTITY_FINALIZE }`; `EScriptedStateDeserializeEvent { BEFORE_CONSTRUCTOR, REPLACE_CONSTRUCTOR, AFTER_CONSTRUCTOR }`.

### 3.2 Registration = script class + .conf entry (both required)

1. Subclass a base, `override static typename GetTargetType()`.
2. Name the class in the persistence conf: entity → `EntitySerializer <Class> "{GUID}" { }`; component → inside `ComponentSerializers { <Class> "{GUID}" { } }`; state → `Serializer <Class> "{GUID}" { }`.

No attribute/naming-convention auto-registration exists. Serializers may carry `[Attribute]` members configurable per conf entry (e.g. `SCR_AIGroupSerializer.m_fMaxPlayerReconnectTime`).

### 3.3 Serialization context (`generated/Plugins/Serialization/`)

`SerializationContext` (base): `CanSeekMembers()`, `IsValid()`, `StartMap(name, out count)`/`EndMap()`, `StartArray(name, out count)`/`EndArray()`, `StartObject(name)`/`EndObject()`, type-discriminator controls.

`SaveContext`: `Write(void value)` (name derived from variable name), `WriteValue(string name, void value)`, `WriteDefault(value, defaultValue)`, `WriteValueDefault(name, value, defaultValue)`, `WriteMapKey(string)`.

`LoadContext`: `Read(out value)`, `ReadValue(name, out value)`, `ReadDefault(out value, defaultValue)`, `ReadValueDefault(name, out value, defaultValue)`, `ReadMapKey(idx, out key)`, `DoesKeyExist(name)`, `DoesObjectExist(name)`.

⚠️ **Binary-format gotcha** (`SaveContext.c:22`, `LoadContext.c:30-35`): with the binary DB, fields are positional — `WriteDefault` *writes* defaults in binary (only JSON omits), and `DoesKeyExist` always returns true. The vanilla idiom for optional data is `if (!x.IsEmpty() || !context.CanSeekMembers())`. **Write order must equal read order.** Note the persistence binary contexts enable `ConfigureObjectSeeking(true)` + `ConfigureSkippableObjects(true)` (`SCR_PersistenceSerializationContext.c:40-41,52-53`).

**Versioning is manual.** Every vanilla serializer hand-writes `context.WriteValue("version", 1)` first and reads it back first. No built-in version API.

Arrays/maps/sets of primitives, vectors, UUIDs and `ref` objects serialize via plain `Write`/`Read` (e.g. `array<UUID>`/`set<UUID>` in `SCR_AIGroupSerializer.c:34,48,78,81`).

### 3.4 Canonical vanilla serializer (copy this shape)

`$GS/Plugins/Persistence/System/Serializers/Entities/SCR_SpawnPointSerializer.c` (entire file):
```c
class SCR_SpawnPointSerializer : GenericEntitySerializer
{
	override static typename GetTargetType() { return SCR_SpawnPoint; }

	override protected ESerializeResult Serialize(notnull IEntity entity, notnull SaveContext context)
	{
		const SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.Cast(entity);
		const BaseContainer source = entity.GetPrefabData().GetPrefab();
		const bool enabled = spawnPoint.IsSpawnPointEnabled();
		bool enabledDefault;
		source.Get("m_bSpawnPointEnabled", enabledDefault);
		if (enabled == enabledDefault)
			return ESerializeResult.DEFAULT;
		context.WriteValue("version", 1);
		context.WriteDefault(enabled, enabledDefault);
		return ESerializeResult.OK;
	}

	override protected bool Deserialize(notnull IEntity entity, notnull LoadContext context)
	{
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.Cast(entity);
		int version;
		context.ReadValue("version", version);
		bool enabled;
		if (context.Read(enabled))
			spawnPoint.SetSpawnPointEnabled_S(enabled);
		return true;
	}
}
```
Diff-against-prefab-default idiom: `entity.GetPrefabData().GetPrefab()` + `source.Get(...)`, return `DEFAULT` when unchanged. Component equivalent: `component.GetComponentSource(owner)` (`SCR_ResourceComponentSerializer.c:25`).

Other reference implementations: `SCR_GameMasterMetaSerializer.c` (state serializer + dummy proxy), `SCR_EditableEntityComponentSerializer.c` (UUID cross-refs + WhenAvailable), `SCR_AIGroupSerializer.c` (custom SerializeSpawnData + UUID sets), `SCR_ReconnectSerializer.c` (data-bearing state + state-change subscription), `BuildableEntity.c` (only vanilla `DeserializeSpawnData` override).

---

## 4. PersistentState — non-entity script state

`PersistentState: ScriptAndConfig` — "Created and maintained only through internal system configuration." **You do NOT instantiate it** — declare an empty subclass in script, then declare it in the conf's `PersistentStates { }` block; the engine constructs and tracks the singleton. A `ScriptedStateSerializer` whose `GetTargetType()` returns the class handles the data.

Template: `$CFG/GameMode/GameMaster.conf:14-32`:
```
ScriptedStatePersistenceConfig + Serializer SCR_GameMasterMetaDataSerializer  (:18-23)
PersistentStates { SCR_GameMasterMetaData "{65D4DA3E0736554A}" { } }          (:29-32)
```
Vanilla states: `GarbageSystemState`, `DoorSystemState`, `SCR_PlayerReconnectData` (Common.conf:252-256), `SCR_TaskSystemData`, `SCR_ScenarioFrameworkSystemData`, `SCR_VoiceoverSystemData` (Mission.conf:76-80), `SCR_EditableEntityCoreData`, `SCR_GameMasterMetaData`.

Two idioms: **dummy proxy** (state empty; serializer reaches the real manager singleton — `SCR_TaskSystemSerializer`) and **data-bearing** (`SCR_PlayerReconnectData` holds `ref set<UUID>` and subscribes to state changes — `SCR_ReconnectSerializer.c:40-80,145`).

Runtime access: `PersistenceSystem.GetPersistentState(typename)` — a state is itself UUID-addressable (`SCR_ScenarioFrameworkSystemSerializer.c:505`).

Alternatively, `StartTracking(anyManagedInstance, lazy)` registers an arbitrary script object ("Mostly for scripted states") — but then YOU own the ref and the conf must still bind a serializer/config to its type.

---

## 5. SaveGame layer (global save points)

`SaveGameManager` (sealed) — get via `GetGame().GetSaveGameManager()` (vanilla's way; `ChimeraGame.c:33`) or `SaveGameManager.Get()`.

| Line | Signature |
|---|---|
| 17 | `proto external bool IsBusy();` |
| 19 | `proto external bool IsSavingEnabled();` |
| 21 | `proto external void SetEnabledSaveTypes(ESaveGameType enabled);` |
| 23 | `proto external ESaveGameType GetEnabledSaveTypes();` — treat as bitmask |
| 28 | `proto external void SetSavingAllowed(bool allowed);` / 30 `IsSavingAllowed()` |
| 32 | `proto external void StartPlaythrough(string mission, string optionalName = "", bool transition = true);` |
| 34 | `proto external int GetCurrentPlaythroughNumber();` |
| 42 | `proto external void RequestSavePoint(ESaveGameType type, string displayName = "", ESaveGameRequestFlags flags = 0, SaveGameOperationCallback callback = null);` |
| 50 | `proto external void RequestSavePointOverwrite(notnull SaveGame save, ESaveGameRequestFlags flags = 0, SaveGameOperationCallback callback = null);` |
| 56 | `proto external void GetSaves(string missionfilter, notnull SaveGameObtainCallback callback);` — **always async** |
| 58 | `proto external SaveGame GetActiveSave();` |
| 60 | `proto external void Load(notnull SaveGame saveGame, bool transition = true);` |
| 62 | `proto external void Delete(notnull SaveGame saveGame, SaveGameOperationCallback callback = null);` |
| 64 | `proto external void Purge(string mission, int playthroughFilter = -1, SaveGameOperationCallback callback = null);` |
| 66 | `static proto string GetCurrentMissionResource();` |

Events (EventProvider style, `[EventAttribute()]`, connect with `ConnectEvent` + `[ReceiverAttribute()]`): `OnSaveCreated(SaveGame)` (:71), `OnSaveDeleted(SaveGame)` (:74), `OnBusyStateChanged(bool)` (:77).

`SaveGame` (sealed): `GetMissionResource()`, `GetId()→UUID`, `GetType()`, `GetSavePointNumber()`, `GetPlaythroughNumber()`, `GetSavePointName()`, `GetPlaythroughName()`, `GetSavePointCreatedUnix()`, `GetPlaytime()`, `GetSavePointGameVersion()`, `IsSavePointGameVersionCompatible()`, `GetSavePointAddons(out array<SaveGameAddonVersion>)`, `AreSavePointAddonsCompatible()`.

`ESaveGameType { MANUAL, AUTO, SCRIPTED, SHUTDOWN }` — ⚠️ used as **bit flags** in headers/masks (`SCR_MissionHeader.c:33` default `"15"`; `SCR_PauseMenuUI.c:849` masks with `&`) but as plain values in `RequestSavePoint`. `ESaveGameRequestFlags { BLOCKING }` — only one flag; the doc's "shutdown after save" flag does not exist in 1.7.0. `BLOCKING` only in SP (vanilla gates on `RplSession.Mode() == RplMode.None`).

Callbacks: `SaveGameOperationCallback(delegate(bool success, Managed ctx), ctx)`; `SaveGameObtainCallback(delegate(bool success, array<SaveGame> saves, Managed ctx), ctx)`.

### Mission opt-in

`SCR_MissionHeader.m_eSaveTypes` (`SCR_MissionHeader.c:33-34`, flags, **default "15" = all enabled**; 0 disables the entire persistence system). Applied in `game.c:258-273` `OnMissionSet` → `SetEnabledSaveTypes`. Overthrow's `Missions/24_OVT_Eden.conf` is `SCR_MissionHeader` and does not set `m_eSaveTypes` → default all-enabled. There is **no `SCR_SaveLoadComponent`**.

### Vanilla wiring examples

- Manual save: `SCR_CreateNewSaveDialog.c:49`, `SCR_RewindComponent.c:56-160` (full create/delete/load/list lifecycle)
- Auto-save on event: `SCR_CampaignMilitaryBaseComponent.c:1604-1605` — `if (!SCR_PersistenceSystem.IsLoadInProgress() && ...) GetGame().GetSaveGameManager().RequestSavePoint(ESaveGameType.AUTO);`
- Overwrite latest: `SCR_SaveSessionToolbarAction.c:75`
- Detect + load a save: `SCR_ScenarioUICommon.c:65-81` — `GetSaves(missionId, new SaveGameObtainCallback(ProcessLoadSave, ...))` then `Load(saves[saves.Count()-1], false)`
- In-mission mission filter: `manager.GetSaves(manager.GetCurrentMissionResource(), ...)` (`SCR_RewindComponent.c:137-154`)
- Save one disconnecting player immediately: `m_Persistence.Save(playerController);` (`SCR_SpawnLogic.c:107`)

### ⚠️ Dedicated-server end-of-game purge

`SCR_BaseGameMode.c:723-747`: on game mode end, `SetSavingAllowed(false)`; then **unless** `-keepSessionSave` CLI or `persistence.ShouldKeepSessionData()`, on dedicated servers it runs `ClearStorage(PersistenceSessionStorage)` **and** `Purge(mission, playthrough)` — wiping the playthrough's saves. Overthrow's persistent-campaign model must override this behavior (method is in the `OnGameModeEnd` path of `SCR_BaseGameMode`) or configure keep-session-data.

---

## 6. Entity references / UUID

`UUID` (`Core/generated/Types/UUID.c:27`, `sealed class UUID: string`): `NULL_UUID` (:35), `IsNull()` (:69), `IsUUID(string)` (:84), `GenV4()` (:99), `GenV8(ns, name)` (:117). ⚠️ `""` and `"00000000-…"` are both null but not `==` — **always `IsNull()`, never string compare**.

`PersistenceIdUtils` (sealed, static): `Generate()` (:37, UUIDv8 time-sortable), `FromEntity(IEntity, bool ignoreParents=false)` (:39, deterministic), `FromString(string)` (:53, deterministic FNV-1a), `ConfigureHive(int)`/`GetHiveId(UUID)` (:21,:62), `GetUnixTime(UUID)` (:60).

Write/read a reference (canonical, `SCR_EditableEntityComponentSerializer.c:29,41,59-64,100`):
```c
// save
const UUID parentId = GetSystem().GetId(parentEntity);
context.WriteDefault(parentId, UUID.NULL_UUID);
// load
UUID parentId;
context.ReadDefault(parentId, UUID.NULL_UUID);
if (!parentId.IsNull())
{
	Tuple1<SCR_EditableEntityComponent> parentContext(editable);
	PersistenceWhenAvailableTask parentTask(OnParentAvailable, parentContext);
	GetSystem().WhenAvailable(parentId, parentTask);   // optional 3rd arg: maxWaitSeconds
}
// callback — static, exact arity:
protected static void OnParentAvailable(Managed instance, PersistenceDeferredDeserializeTask task, bool expired, Managed context)
```
`PersistenceWhenAvailableTask(delegate = null, Managed context = null)` — delegate `(Managed instance, PersistenceDeferredDeserializeTask task, bool expired, Managed context = null)`. Caveats from docs: `instance` may be null on load failure; `expired=true` means timeout; instance may not have finished its own deferred deserialization. Timeout examples: 30.0 (`SCR_VoiceoverSystemSerializer.c:95`), attribute-configurable (`SCR_AIGroupSerializer.c:308`).

Synchronous alternative when existence is known: `FindById(UUID)` (`SCR_ReconnectSerializer.c:68`).

Request objects:
```c
class PersistenceLoadRequest: Managed { ref array<Managed> Instances; }
class PersistenceSpawnRequest: Managed { PersistenceCollection Collection; ref array<UUID> Include; ref array<UUID> Exclude; int Limit; int Offset; }
```
`PersistenceResultCallback(delegate(EPersistenceStatusCode, Managed result, bool isLast, Managed ctx), ctx)`. Real spawn/load usage: `SCR_SpawnLogic.c:368-373` (spawn player character from `Character` collection), `:310-315` (`RequestLoad` re-applies data to an existing instance), `:51-55` (`FindCollection("Player")/("Character")` cached at setup), `:302-307` (fallback when `GetState() != ACTIVE`), `:168-174` (`ReloadConfig` on possession change).

---

## 7. Migration-critical facts summary

1. **Global save** = `SaveGameManager.RequestSavePoint()`. Per-instance `PersistenceSystem.Save()` exists for special cases (disconnecting player). No `TriggerSave`.
2. **Collections, serializer bindings, persistent states, storages are 100% .conf-defined.** Script only reads (`FindCollection`) or nudges (`SetConfig`/`ReloadConfig`).
3. **"Does a save exist?" is async-only** (`GetSaves` + callback). Cache the answer (init query + `OnSaveCreated`/`OnSaveDeleted` events) to serve synchronous UI/tests.
4. **Save location on disk is engine-managed and not script-visible** — must be discovered empirically after the first real save (needed to update `.scripts/reset_save.sh` etc.).
5. **Mission header default enables all save types**; persistence system only runs where the SystemSettings conf registers it (`SystemLocation Server`).
6. **Vanilla Common.conf already persists characters, vehicles, items, doors, AI groups, storages** — inheriting it gives Overthrow world-state persistence for free; EPF's character/vehicle SaveData has vanilla equivalents.
7. **Dedicated servers purge the playthrough on game-mode end** unless keep-session-data is configured (§5) — must be handled for Overthrow's persistent campaign.
8. **Binary save context**: write order == read order; `WriteDefault` still writes in binary; guard optional structure with `CanSeekMembers()`.
9. Clients cannot save; everything persistence runs server-side only.
10. Version fields are hand-rolled: `WriteValue("version", N)` first, read it first.
11. EnforceScript: no generic methods (generic classes OK), no ternaries; `notnull`/`out` qualifiers in overrides must match base exactly.
12. Sealed (can't extend): `SaveGameManager`, `ReforgerSaveGameManager`, `SaveGame`, `PersistenceCollection`, all config classes, `PersistenceIdUtils`, `PersistenceDeferredDeserializeTask`, `UUID`. Extensible: `PersistenceSystem`/`SCR_PersistenceSystem`, the three scripted serializer bases, `GenericEntitySerializer`, `PersistenceConfigRule`, `PersistentState`, callbacks, load/spawn requests.
