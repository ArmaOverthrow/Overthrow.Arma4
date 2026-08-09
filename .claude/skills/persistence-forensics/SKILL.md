---
name: persistence-forensics
description: Investigate Arma Reforger / Overthrow save-file problems — decode a save point, read the load log, and choose an experiment that can actually settle the question
version: 1.0.0
---

# Persistence Forensics

For any report of the shape *"my stuff disappeared after a restart"*, *"the campaign reset"*, *"the
save is huge"*, or *"it didn't come back"*.

**The governing rule: read the save file.** Four persistence bugs in a row (BUG-086, BUG-104,
BUG-116, BUG-118) were each argued about for hours from logs and source, and each was settled in
minutes by decoding the blob. Logs tell you what the engine *said*; the blob tells you what is
*there*. When the two disagree, the blob wins.

---

## 1. Get the artifacts

| artifact | where | what it settles |
|---|---|---|
| `console.log` from the session that **wrote** the save | server profile `logs/<date>/` | what was saved, when, into which save point |
| `console.log` from the session that **loaded** it | the next boot | what failed to come back |
| the save point directory | `<profile>/.save/[app<id>_user<id>/]game/<mission>/playthrough<NNN>/savepoint<NNN>/` | ground truth |

Ask for the **whole save point directory** — `meta-info.json` *and* `WorldState/*.blob`. The blob
alone loses the save type and timestamp, which is often the answer.

If a server owner is slow or unavailable, **do not wait**. `tools/launch-server.sh` reproduces a
full save→restart→load cycle locally in ~90 seconds; see §5.

---

## 2. Decode it

`tools/decode-savepoint.py` — the format, its pitfalls and the false-positive rules are documented
in that file's header. Accepts a `.blob`, a `savepoint<NNN>` dir, or a whole `playthrough<NNN>`.

```bash
tools/decode-savepoint.py summary <savepoint>          # size, records, per-collection counts
tools/decode-savepoint.py diff <old> <new>             # what a restart / a session changed
tools/decode-savepoint.py find <savepoint> <uuid>      # where an id lives, and how many times
tools/decode-savepoint.py prefab <savepoint> <GUID>    # how many of a given item are in there
tools/decode-savepoint.py ids <savepoint> --since ...  # every record with its mint time
```

Facts worth knowing before you interpret the output:

- **A save point is a FULL snapshot, not a delta.** Verified across three campaigns; two
  *consecutive* save points held 946 and 944 records. `SaveGameManager.Load()` takes exactly one
  save point, and there is no other on-disk store. So if a record is not in the loaded save point,
  it was not loaded — full stop.
- **`Item` having no section is normal.** Vanilla's `Item.conf` leaves `StorageRoot` false and
  `ParentHandling` at ACCEPT, so an item in a container is written as a nested child of its
  parent's record, never as a root record. An absent `Item` section is not missing data.
- **Ids carry their creation time** (UUID v8, 48-bit ms prefix). `ids` prints it. This is how you
  tell "created during the session that broke" from "inherited from a week ago" — decisive in
  BUG-116, where the colliding ids turned out to predate the session entirely.
- **An id appearing twice is usually fine.** Cross-collection pairs are references —
  AIGroup→Character, System→Vehicle. Only **two occurrences in the same collection** is a genuine
  duplicate record.

---

## 3. Read the load log

Grep the loading session for these. They are native `DEFAULT (E)` messages with no mod context, so
nothing points at them and they scroll past unnoticed — on the reporting server they had been
firing every restart for at least three sessions.

| line | meaning |
|---|---|
| `Attempted to deserialize meta data for an already existing id` | two records claim one id; the later one loses and its entity is destroyed. **Measured cause: a `RequestSpawn` for a record whose instance is already live.** |
| `Unable to locate configuruation ''` (sic) | a record was written with a SCRIPTED config — see `OVT_PersistenceTracking.MarkForSelfSpawn`'s do-not-use header. Poisons the save. |
| `Attempted to deserialize meta data without configuration` | same cause as above |
| `[PERSISTENCE] Save (AUTO\|SHUTDOWN) started` / `Save completed successfully` | which saves ran, and whether Overthrow's or the engine's |
| `Save data transaction to '…/savepoint<NNN>'` | **which save point was written** — Overthrow's autosave OVERWRITES the newest AUTO save, the engine's creates a new one, so the two interleave |

`Scripts/Game/Modded/SCR_PersistenceSystem.c` adds Overthrow-side instrumentation on top: it hooks
`HandleDelete` (the scripted event raised for every entity the system destroys, *including*
deserialize failures) and prints a per-load summary naming the container each lost item was being
restored into. Silent when nothing is wrong.

---

## 4. Know the traps

These have each cost a full investigation at least once.

- **`SelfSpawn 0` means "don't bring it back", NOT "don't save it".** The record is still written.
  Nothing instantiates it on load, so `SelfDelete` — which needs a tracked instance to be destroyed
  — can never fire, and the record is immortal. This was BUG-118: ~490 permanently orphaned records
  per restart. If Overthrow rebuilds a thing from manager state, it must be **untracked**, not just
  scoped out of self-spawning.
- **Registration is LAZY.** The native `Persistence` component registers frames after spawn, so a
  `StopTracking()` issued at spawn time has nothing to act on. Queue and retry; `IsTracked()` is the
  only oracle — `StartTracking()` returns true even when it tracks nothing.
- **A released record is not durable.** `Save()` + `StopTracking(keepData)` + delete keeps a record
  for a short window only (measured: gone within ten minutes, in session, no restart). Nothing may
  depend on a record outliving its entity — BUG-086. The answer there was the reservation model:
  keep the entity alive and hidden.
- **`RequestSpawn` on a record whose instance is already live** retries `already existing id`
  several times a second and never completes, and a collection-wide sweep of them can wedge every
  subsequent save for the session. Always ask `FindById` first.
- **In-session round trips prove nothing about restarts.** A save point commits storage, but a LOAD
  only instantiates records whose config says `SelfSpawn 1`. The automated persistence suites are
  in-session, which is exactly why they stayed green while a dedicated server lost player bodies.
- **A green dedicated run says nothing about a listen-host Continue, and vice versa.** A dedicated
  server continues at boot before anyone connects; a Continue replaces the world with players
  already connected. BUG-104 lived in the gap.

---

## 5. Reproduce locally instead of waiting

```bash
# make the loop fast (revert before committing!)
#   Prefabs/GameMode/OVT_OverthrowGameMode.et
#     OVT_PersistenceManagerComponent { m_fAutosaveInterval 30 }

tools/launch-server.sh --scenario testworld --profile OverthrowDS --timeout 100
# ...it continues the existing playthrough and autosaves every 30s

# snapshot, restart, compare
cp -r "<profile>/.save/game/<mission>/playthrough000" .tmp/before
tools/decode-savepoint.py diff .tmp/before/savepoint003 "<...>/playthrough000/savepoint003"
```

The server binary is genuinely headless — launching it is safe and opens no window. A **client**
does open a window on the user's desktop, so say so before launching one.

To involve a player, the admin password is whatever `--admin-password` was given (default
`devadmin`), and it works in local mode despite there being no RCON.

**Back up the save tree before any experiment.** `cp -r` the playthrough directory; every cycle
overwrites it.

---

## 6. Choose experiments that can fail

The BUG-116 investigation ran five player-driven reproductions (items at rest, a stack split across
containers, eight restart cycles, looting a player corpse, looting an AI body) and every one came
back clean. Each cost a play session. The lesson is not "try harder" — it is:

1. **Prefer a measurement over a reproduction.** Decoding one save point eliminated three
   hypotheses in ten minutes that reproduction attempts had not touched in two hours.
2. **State the prediction before running.** "If overwrite accumulates, the blob is ~7× a fresh one"
   is a test. "Let's see what happens" is not.
3. **When guessing fails twice, ship instrumentation.** The information needed is *which two
   records claim the id*; no amount of guessing at player actions produces it, and one log line
   from an affected server does.
4. **Record the negatives.** "Five transitions are clean" is most of the value of a failed night,
   and it is worthless if it is not written down.

---

## Related

- `enforcescript-patterns` skill → `persistence.md` for the component/serializer patterns
- `docs/features/core/persistence/vanilla-api-reference.md` — **API truth, read before writing any
  persistence code**
- `docs/features/core/persistence/context.md` — the decision log and every session note
- `tools/README.md` — the automation contract for `launch-server.sh` / `run-tests.sh`
