# Persistence Patterns (Vanilla / First-Party)

Complete guide for save/load in Overthrow using **Reforger's first-party persistence system**.

> ## ⚠️ EPF is retired — read this first
>
> Overthrow used the Enfusion Persistence Framework (EPF) until **2026-08-02**, when the `core/persistence` migration shipped. **EPF and EDF are gone**: zero `EPF_` references in `Scripts/`, and neither is a mod dependency in `addon.gproj`.
>
> If you have seen an EPF-shaped example anywhere — an old doc, a code comment, a memory, another skill — **it is stale**. None of these exist any more:
>
> | ❌ Retired (EPF) | ✅ Current (vanilla) |
> |---|---|
> | `EPF_ComponentSaveDataClass` / `EPF_ComponentSaveData` | `ScriptedComponentSerializer` |
> | `EPF_EntitySaveDataClass` / `EPF_EntitySaveData` | `ScriptedEntitySerializer` |
> | `EPF_PersistentScriptedState` | `ScriptedStateSerializer` + a `PersistentState` proxy |
> | `[EPF_ComponentSaveDataType(...)]` attribute binding | a rule in `Configs/Systems/Persistence/Overthrow.conf` |
> | `ReadFrom()` / `ApplyTo()` | `Serialize()` / `Deserialize()` |
> | `EPF_Component<T>.Find()` | ordinary `FindComponent` / `OVT_ComponentFinder<T>.Find()` |
> | `#ifndef PLATFORM_CONSOLE` guards | **nothing** — consoles are handled internally |
>
> ⚠️ EPF-era saves are dead. There is no converter.
>
> **API truth:** `docs/features/core/persistence/vanilla-api-reference.md` — verified against retail 1.7.0.54 with file:line citations. When in doubt, read that, then read a shipped serializer in `Scripts/Game/Persistence/Serializers/`.

---

## Overview

Two layers, and conflating them is the classic mistake:

1. **`PersistenceSystem` / `SCR_PersistenceSystem`** — a server-only `WorldSystem` that *tracks instances and serializes them*. Config-driven.
2. **`SaveGameManager`** — a global engine singleton owning save *points*, playthroughs and load/restart transitions. This is what "press Save" talks to.

Overthrow wraps the second in `OVT_PersistenceManagerComponent` (`SaveGame()` → `RequestSavePoint(ESaveGameType.MANUAL)`), and implements the first as serializer classes under `Scripts/Game/Persistence/Serializers/`.

**Saving is server-only.** Clients cannot write saves; on a client `HasSaveGame()` stays false.

---

## Basic Component Serializer Pattern

### When to Use

- Component state must survive a save/load
- Manager or Controller data must persist
- Structured state needs an explicit save format

### Pattern Structure

```cpp
class OVT_SomeManagerSerializer : ScriptedComponentSerializer
{
    //! \return The component class this serializer is responsible for.
    override static typename GetTargetType()
    {
        return OVT_SomeManagerComponent;
    }

    override protected ESerializeResult Serialize(notnull IEntity owner, notnull GenericComponent component, notnull SaveContext context)
    {
        OVT_SomeManagerComponent manager = OVT_SomeManagerComponent.Cast(component);
        if (!manager)
            return ESerializeResult.ERROR;

        context.WriteValue("version", 1);   // ALWAYS first

        int someValue = manager.GetSomeValue();
        context.Write(someValue);

        string someName = manager.GetSomeName();
        context.Write(someName);

        return ESerializeResult.OK;
    }

    override protected bool Deserialize(notnull IEntity owner, notnull GenericComponent component, notnull LoadContext context)
    {
        OVT_SomeManagerComponent manager = OVT_SomeManagerComponent.Cast(component);
        if (!manager)
            return false;

        // No version means no payload for this component - tolerate it, don't fail.
        int version;
        context.ReadValue("version", version);
        if (version < 1)
            return true;

        int someValue;
        context.Read(someValue);            // SAME ORDER as Serialize

        string someName;
        context.Read(someName);

        manager.SetSomeValue(someValue);
        manager.SetSomeName(someName);

        return true;
    }
}
```

Live examples: `OVT_TownManagerSerializer`, `OVT_EconomyManagerSerializer`, `OVT_BuildableComponentSerializer` (a short one — read this first).

### Key Points

- **`GetTargetType()` is `static`** and names the component class.
- **Return values differ:** `Serialize` returns `ESerializeResult` (`OK` / `ERROR` / `DEFAULT`); `Deserialize` returns `bool` (true = payload consumed).
- **Binary contexts are POSITIONAL.** Write order must equal read order. `WriteValue()`'s name string is only meaningful in JSON — it does **not** make the format order-independent.
- **Version first, hand-rolled**, as in every vanilla serializer. `version < 1` means "no payload for me", which must be tolerated rather than treated as corruption.
- **Deserialize must be idempotent.** It runs both when starting from a savepoint *and* when re-applying save data to a live session (`OVT_PersistenceManagerComponent.ReapplyLatestSaveData`). Plain assignments are safe; re-entering a non-re-entrant start sequence is not.

---

## Binding a Serializer (config, not script)

**A serializer that compiles but is not listed in config is never called.** There is no per-serializer registration API in script.

Add it to the matching block in `Configs/Systems/Persistence/Overthrow.conf`:

```
ComponentSerializers {
    OVT_SomeManagerSerializer "{<fresh-GUID>}" {
    }
}
```

- Manager components hang off the game-mode entity configuration (`{65ACD95F40F6C669}`).
- Per-instance components (buildables, placeables) match via a `ComponentClassPersistenceConfigRule { ComponentClass "OVT_BuildableComponent" }`.
- An **entity** serializer is swapped by overriding the `EntitySerializer` entry, reusing vanilla's serializer GUID — see `OVT_OverthrowGameModeSerializer`, which nests vanilla's payload under a `"base"` sub-object and delegates to `super` so the base payload is not silently dropped.

⚠️ **Scripted persistence config *rules* are dead code.** The engine never calls a scripted `IsMatch()`. Use `GetConfig()` / `SetConfig()` instead (BUG-018).

⚠️ **Same-GUID `.conf` overrides are DELTAS, not replacements** — they merge over the inherited file rather than replacing it.

---

## Spawned Entities Must Be Tracked

Component state alone restores **nothing** for an entity that has no authored instance in the world file. Overthrow spawns buildables and placeables from config at a player-chosen transform, so a save must be able to **create** the entity again:

1. Its persistence configuration carries **`SelfSpawn`**.
2. The spawn site calls **`OVT_PersistenceTracking.Track(entity)`** (wraps `SCR_PersistenceSystem.StartTracking`, server-side, lazy, safe to call twice).

Three tracking mechanisms exist overall:
1. The native **`Persistence` component on the prefab** (the class name in `.et` is literally `Persistence`; no script binding exists) — the primary route for authored prefabs.
2. **`StartTracking()` from script** — for spawned/untagged entities.
3. **`PersistentState` subclasses** — for non-entity state.

⚠️ Ask `IsTracked()` before `Track()` when an entity may already be registered by another route (a prefab carrying the native component, or an instance the system itself spawned via `RequestSpawn`).

⚠️ **`RequestSpawn` hazards:** a collection-wide `RequestSpawn` wedges saves, and untracking needs the `IsTracked` retry queue (BUG-118).

---

## Collections

Write the whole collection through the context rather than element-by-element where the type supports it — `OVT_TownManagerSerializer` builds an array of plain records and does a single `context.Write(records)` / `context.Read(records)`. Keep the record type a simple `Managed` class with `ref` members; build it in `Serialize` from live state, and rebuild live state from it in `Deserialize`.

Prefer **sparse** payloads keyed by a **stable id** (never an array index) so that adding a new config entry later does not shift or corrupt existing saved data.

---

## Console Platforms

**Nothing to do.** The vanilla system handles console storage internally. The `#ifdef PLATFORM_CONSOLE` carve-outs that used to wrap persistence code were an EPF requirement (EPF wrote to disk, which consoles do not permit) and were removed by the migration. Do not reintroduce them.

---

## Save / Load Triggers

Go through `OVT_PersistenceManagerComponent`, not the engine singleton directly:

```cpp
OVT_Global.GetPersistence().SaveGame();       // → RequestSavePoint(ESaveGameType.MANUAL)
```

- `HasSaveGame()` is a **cache** — `SaveGameManager.GetSaves()` is async-only, so a synchronous accessor cannot be anything else. Wait for the "answer is real" flag before reading it.
- `IsSaveInProgress()` **cannot** distinguish "finished" from "never started". For "did the save I asked for finish?", use the save-finished invoker.
- `GetOnStateChanged()` watching for `EPersistenceSystemState.ACTIVE` is the after-load hook — **there is no `GetOnAfterLoad()` invoker**.
- ⚠️ **A "Continue" replaces the world without anyone connecting**, so `SetupPlayer` never re-runs for already-connected players (BUG-104). Anything keyed on player connect needs a second path for this.

---

## Gotchas and Best Practices

### ✅ DO
- Read a shipped serializer before writing a new one — they carry long header comments explaining *why*, not just what
- Write `version` first, and tolerate `version < 1`
- Keep `Deserialize` idempotent and safe on a live session
- Key saved data on stable ids
- Cast and null-check the component before touching it
- Track spawned entities explicitly

### ❌ DON'T
- Use any `EPF_*` type, or write `ReadFrom`/`ApplyTo` methods
- Assume a `WriteValue("name", …)` makes binary order-independent — it does not
- Add `#ifdef PLATFORM_CONSOLE` guards around persistence
- Rely on a scripted config rule's `IsMatch()` — it is never called
- Write a serializer and forget the `.conf` entry (it will silently never run)
- Save from a client

---

## Testing Persistence

The persistence tiers are real and run headlessly:

```bash
tools/run-tests.sh "{6A6E2A002F53A581}"          # All group (includes persistence tiers)
tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite   # the save→dirty→re-apply gate
```

- The **Persistence** tier round-trips state through the public manager API; the **PersistenceRoundTrip** suite goes through real vanilla persistence storage, plus one per-instance vehicle despawn→storage→respawn.
- Assertions deliberately reference **no** persistence API type, so the suite survives a backend change without reporting it as a regression.
- ⚠️ The true quit-and-continue restart path is **not** covered — `SaveGameManager.Load`'s world transition restarts the autotest harness, so the suite covers in-session re-apply instead. That path is still manual play-testing.
- Prove any new case can fail before shipping it.

---

## Related Resources

- `docs/features/core/persistence/vanilla-api-reference.md` — verified API, file:line citations
- `docs/features/core/persistence/implementation.md` — how the migration was built
- `Scripts/Game/Persistence/Serializers/` — 16 shipped serializers
- `Scripts/Game/Persistence/OVT_PersistenceTracking.c`, `OVT_PersistenceReservation.c`
- `Configs/Systems/Persistence/Overthrow.conf` — the binding
- `persistence-forensics` skill — decoding a save point and diagnosing save-file problems
