# Wiki draft — `core/damage` (task 8.3), OWED

**Written:** 2026-08-20 by the Phase 8 `help-docs-sync` session.
**Status:** 🔴 NOT PUBLISHED. **No `mcp__wikijs__*` tool was exposed to this session at all** — not even
`wikijs_connection_status`. That is the MCP server not being connected, not an auth failure and not a
`RetryError`. Nothing was written to the wiki and nothing was invented; the text below is ready to paste.

The same thing happened to `occupying/counter-attacks` T10.3 (`docs/features/occupying/counter-attacks/context.md:952`),
whose draft is still owed as well. If a session ever gets the tools back, do both in one pass — they touch
the same page.

**Before publishing:**
- `wikijs_search_pages` / `wikijs_get_page_children` first. Page paths are flat-ish and the search is known
  to return wrong `pageId`s, so `wikijs_get_page` and re-read before updating.
- An update call needs `tags`, and a failed update still leaves the render stale.
- Player-facing voice: no class names, no GUIDs, no file paths. Everything below is already written that way.
- No em-dashes.

---

## Page 1 — the page that describes sabotage (search `counter-attacks`, then `sabotage`, `occupying-forces`)

⚠ The `counter-attacks` page may not exist yet: the counter-attacks feature's own wiki sync never ran.
If it does not exist, the correction below belongs on whichever page currently tells players that
sabotaged structures are lost for good.

**What must change:** the wiki must stop saying that what saboteurs demolish at a base is destroyed
permanently, with no rubble, no repair and no salvage. That was true before this feature and is not true now.

**Replacement paragraph** (drop-in for the "what sabotage costs" section):

> What saboteurs pull down at a base stays where it stood. The structure is wrecked rather than taken away,
> with an explosion and a column of smoke to mark it, and what is left is a ruin standing on the same
> ground. The ruin can be repaired for a share of what the structure cost to build, and a campaign saved
> and loaded again brings it back as a ruin rather than as an empty patch. Gear stored inside survives the
> demolition untouched, though nothing about the place works until it is put back. Saboteurs still take the
> cheapest thing standing first and work upward, so bunkers and tents go before the garage does, only what
> belongs to that base is ever at risk, and a single player inside the base stops the demolitions where
> they are.

Add a short pointer after it: `See [Ruins and Repair](/ruins-and-repair) for what a ruin can and cannot do.`

---

## Page 2 — `ruins-and-repair` (NEW page, player-facing)

Suggested path: `ruins-and-repair`. Title: **Ruins and Repair**.
Tags: `structures`, `building`, `sabotage`, `repair`.

> ## Ruins and Repair
>
> A structure the occupying faction wrecks at one of your bases does not vanish. It goes up with an
> explosion and a column of smoke, and what is left stands where it stood, on the same ground, as a ruin.
> A campaign saved and loaded again brings a ruin back as a ruin, and a structure that has been put back
> comes back repaired.
>
> All eight buildable structures behave this way: the guard tower, the recruitment tent, the medical tent,
> the vehicle maintenance ramp, the fuel depot, the garage, the bunkers and the helipad.
>
> ### A ruin does nothing
>
> A ruin is inert. There is no refuelling at a wrecked fuel depot, no recruiting from a collapsed tent, no
> treatment at a flattened medical tent, no shop at a ruined garage, no parking, and no reaching into the
> rubble for what was stored there. Pumps and repair bays stop answering from a vehicle's side as well, so
> a truck parked at a wrecked depot finds nothing to draw fuel from. Nothing about the place works while it
> is wreckage.
>
> ### The gear inside survives
>
> Nothing stored in a structure is lost when it is wrecked. Containers and their contents come through the
> destruction untouched, and only the way in is closed. Putting the structure back opens them again with
> everything where it was left.
>
> ### Putting it back
>
> A ruin carries a repair action, held for twenty seconds, and the action itself shows the price.
>
> Repair costs a share of what the structure cost to build, scaled by the same difficulty setting that
> priced the build in the first place:
>
> | Difficulty | Share of the build price |
> |---|---|
> | Easy | half |
> | Normal | half |
> | Hard | three quarters |
> | Extreme | the full build price |
> | Insane | the full build price |
>
> Because the build price itself already moves with difficulty, the two stack. A guard tower, listed at
> 1200, is built on Normal for 1200 and repaired for 600; on Insane it is built for 4800 and repaired for
> 4800. The money is checked before the work counts and taken when it finishes, so a
> repair beyond your means is turned down rather than left half done. Repair costs money and nothing else:
> there is no separate stock of materials.
>
> A ruin is repaired, never dismantled and rebuilt. It keeps its place, its owner and whatever is inside it.
>
> ### The other side repairs too
>
> The occupying faction does the same on ground it holds. A repair detail is sent to a base of theirs where
> something stands wrecked, and it puts structures back one at a time, cheapest first, without announcing
> anything. The work only goes on while nobody is close: a player within about a hundred and fifty metres of
> the base centre pauses it where it is, and it picks up again once they leave.
>
> In practice this is a narrow thing to run into. It needs the resistance to have built at a base, the
> occupying faction to have wrecked it, and the base to have changed hands afterwards.

---

## Page 3 — `difficulty` (existing, update in place)

Add one row to the difficulty table, in the same style as the other cost multipliers:

- **`repairCostMultiplier`** — Easy 0.5, Normal 0.5, Hard 0.75, Extreme 1, Insane 1. The share of a
  structure's build price charged to repair its ruin. It multiplies the build price *after*
  `buildableCostMultiplier` has already been applied, so the two stack.

If the page carries the counter-attacks fields that `occupying/counter-attacks` T10.3 owed, add them in the
same pass (that list is in `docs/features/occupying/counter-attacks/context.md`).

---

## Page 4 — the FOB / buildings page (search `fobs`, `building`, `base-building`)

One sentence added wherever the page lists what can happen to a structure you have built:

> A structure that is wrecked is not lost. It leaves a ruin in the same place that can be repaired for a
> share of its build price, and anything stored inside it survives. See
> [Ruins and Repair](/ruins-and-repair).

If the page says anywhere that dismantling is the only way a structure leaves the world, that is still true
and should stay: destruction leaves a ruin, it does not remove anything.

---

## Deliberately NOT on the wiki

- The admin chat commands `/ruin-structure` and `/repair-structure` are server-admin material. If they go
  anywhere it is a server-admin page, not a player page.
- Ruin mesh choices, hit-zone health numbers and the per-structure repair hold ring are implementation
  detail and belong in `docs/features/core/damage/`.
- Exact dollar figures for a repair: they are a product of two difficulty multipliers and the structure's
  authored cost, and quoting one number would be wrong on four presets out of five.
