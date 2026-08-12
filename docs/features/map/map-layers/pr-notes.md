# PR notes — pre-submit review of the `new-map` branch, 2026-08-12

Raised during an independent pre-submit review of the whole `map` epic. Recorded here rather than
fixed on the branch, by the user's decision.

## 1. Phase 7 ships at 4/37 — gamepad verification deferred to GitHub PR review

**User's decision (2026-08-12): "Gamepad verification will happen during the pr review process once
it's on GitHub."** This note exists so the deferral is a record rather than an omission.

The verification gate in `tasks.md` Phase 7 is user-driven and essentially unrun. What is outstanding,
in the order it will bite:

1. 🔴 **`pad_left` is shadowed.** `tasks.md:206` predicts that with the layers panel open, **D-pad Left
   unticks the focused row instead of returning to the tool strip** — `MapToolMenuFocus` is the only pad
   route to the strip and the new context takes the binding. `MenuLeft` was included deliberately
   (`SCR_ToolboxComponent` only listens to `MenuLeft`/`MenuRight`, so without it a pad could turn layers
   on but never off). **This is a predicted regression that has never been confirmed or refuted.**
2. **P4** — the left stick will walk rows rather than pan the map while the panel is open. That is the
   accepted price of the Phase 4b fix, not a redesign trigger, but it has not been observed.
3. **F-7** — hide Towns (5 s refresh) and Vehicles (2 s refresh), leave the map open 30 s, neither may
   reappear. This is the R8/BUG-136 interaction and the one most likely to be silently broken.
4. **I-2 / I-3** — two profiles proving preferences are per-profile, and JIP.
5. **Workbench clean load** — the only gate in the project that can see a dangling GUID in the 14 new
   `.layout`/`.meta` files this branch adds. Nothing automated covers it.

`compile-check.sh`, the full autotest group and `check-input-conflicts.py` are all green, and none of
them can see any of the above: the input checker is blind to cross-context collisions and to the base
game's ~197 inline `ActionContext` actions, and no test in this project opens a UI.

## 2. Four near-identical config filenames in `Configs/Map/`

`MapOverthrow.conf` (an `SCR_MapConfig`) sits beside `OverthrowMap.conf` (an `OVT_OverthrowMapConfig`),
and `MapRespawn.conf` beside `OverthrowMapRespawn.conf` — same directory, near-anagram names, different
types. All four are new on this branch and referenced from one or two places each, so renaming is cheap
now and will not be again once saves, prefabs and docs accumulate references.

## 3. Component reorder in `OVT_OverthrowGameMode.et`

The prefab moves `OVT_ReconnectComponent` and `OVT_TownManagerComponent` to different positions in the
`components {}` block. It looks like Workbench re-serialization rather than an intentional change, and
the Init suite is green, but component order can affect init order and nothing in the diff explains it.
Worth a glance before merge.
