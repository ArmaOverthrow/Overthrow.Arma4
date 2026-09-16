# Wiki draft : `core/options` (phase 9), PUBLISHED

**Written:** 2026-09-08 by the Phase 9 `help-docs-sync` session.
**Status:** PUBLISHED 2026-09-10 at wiki path `options`, page id 75. Also linked from
`getting-started` (id 2) and `home` (id 1). The text below is kept as the record of what shipped.

**Before publishing:**
- Run `wikijs_search_pages` for `options`, `main menu`, `settings` and `server settings` first. This
  may already be a page, or a section of one, under a different path.
- Read whatever page you land on with `wikijs_get_page` before you edit it, and keep its structure.
- Player-facing voice: no class names, no GUIDs, no file paths. The draft below is already written
  that way.
- No em dashes.
- Lint the finished page as a whole after paste, since surrounding text this session did not write
  can still carry a violation. `python3 ~/.claude/skills/asd-ste100/scripts/ste-lint.py --fail-over
  2.5 <exported-page>.md`.

---

## New page : path `options`, title "Options"

Tags: `menu`, `settings`, `server`, `autosave`.

Lint score of the body below: 0.00 violations per 100 words (208 words).

> # Options
>
> The pause menu carries an Options button. Open the main menu with U, then select Options.
>
> The screen holds up to three sections: This Player, Resistance and Server. A section appears only
> if it holds a setting you may change. A single player, or the host of their own game, always sees
> every section that holds a setting.
>
> ## This Player
>
> This section is empty in the current version. It holds a place for settings that stay on your own
> machine. It appears once a future update adds one.
>
> ## Resistance
>
> This section needs officer status. It holds settings for your faction. No setting lives here yet.
>
> ## Server
>
> This section needs the admin role. On a dedicated server, only an admin can open it. In single
> player, or as the host of your own game, you always see it.
>
> Two settings live here.
>
> **Automatic saving.** A toggle that turns the campaign save timer on or off.
>
> **Time between saves.** A slider from 2 to 60 minutes, in steps of one minute. It sets the gap
> between two automatic saves.
>
> A change to either setting applies at once. No restart follows. It reaches every player on the
> server, and a value you set stays after a save and a Continue.

Source facts, each checked against the shipped code:
- The button and its key: `UI/Layouts/Menu/MainMenu.layout` (Options no longer carries `"Is Enabled"
  0`), `Scripts/Game/UI/Context/OVT_MainMenuContext.c` wires it to `OVT_OptionsContext`.
- Three sections, empty-section rule: `docs/features/core/options/implementation.md` sections 3.5
  and D9; `Scripts/Game/UI/Context/OVT_OptionsContext.c`.
- Section permission: `Scripts/Game/Components/Controller/OVT_OptionsRequestComponent.c`,
  `PlayerMayEditScope` (officer for Resistance) and `PlayerMayEditWorld` (admin role for Server,
  with the single-player/listen-host fallback).
- The two Server settings and their ranges: `Scripts/Game/GameMode/Managers/
  OVT_OptionsManagerComponent.c`, `RegisterDefaults` (toggle default on, interval 120 to 3600
  seconds in 60-second steps, shown to the player in minutes).
- Apply-at-once, replication, save survival: `docs/features/core/options/implementation.md` section
  3.6 (data flow) and the Definition of Done entries for autosave enable/interval.

---

## If a `main-menu` page already exists

Search for it before you create the `options` page above; if the wiki already documents the pause
menu's button list, add one line to it rather than a second page:

> The Options button opens a screen where you can change server and resistance settings you have the
> rank to edit. See [Options](/options).

If the existing page ever said the Options button did nothing, remove that line. It is stale.

---

## Deliberately NOT on the wiki

- The registry, the request component, the RPC and the persistence serializer are implementation
  detail. They belong in `docs/features/core/options/implementation.md`, not on a player page.
- The forged-write rejection path (an edit attempt from a role that may not make it) is a security
  detail with no player-facing action to describe.
- The `CHOICE` field type ships with no consumer in this version, so it has nothing to document yet.
