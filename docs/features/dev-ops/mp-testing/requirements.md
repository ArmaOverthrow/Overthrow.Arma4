# MP Testing — Requirements

**Epic:** dev-ops (feature #6)
**Created:** 2026-08-06
**Status:** 🟡 Phase 1 shipped ahead of planning — `tools/launch-server.sh` exists and works; the rest is planned

## Overview

Shortens the iteration loop for the project's **most common regression class**: multiplayer and join-in-progress. Today a JIP bug costs a full manual round-trip — publish or stage a build, host a session, join with a second client, play to the repro — and the dev-ops epic's automated gate cannot help, because `tools/run-tests.sh` runs a single client and structurally cannot observe a second one.

The epic's original scope statement put MP out of scope on the grounds that it "needs two coordinated processes". That is still true of *automated* MP testing, but it conflated two things: coordinating two processes is hard, whereas **launching one local server against the working tree is not**. Feature #1's process boundary already does the hard part. This feature separates them: get a server running for manual testing first (done), then decide whether automated MP testing is worth building on top.

Sequenced after #5 in priority, not in dependency — it needs only #1.

## Requirements

### Phase 1 — local server for manual testing (shipped 2026-08-06)

- **A local dedicated server runs the working tree**, launched with one command from WSL, with no publish, no staging and no Workshop round-trip. ✅
- **The server is headless** — no window, no local player, so it is a faithful server rather than a listen-host. ✅
- **The tool reports when the server is joinable**, rather than leaving the operator to guess from log spam. ✅
- **It reuses feature #1's process boundary.** No second `.exe`-launching implementation, and Ctrl-C leaves no orphan. ✅
- **The composition difference between the two engine routes is documented, not smoothed over** — dedicated mode mounts EPF+EDF that the working tree does not use. ✅
- **The server profile is separate from the CI profile**, so a play session cannot perturb the state `run-tests.sh` asserts against. ✅

### Phase 2 — prove the join path (not started; needs a human)

- **A source-built client connects to the source-built server**, and the checksum validation passes. This is the single assumption the whole feature rests on and it is currently **unverified**.
- **The client-side launch is one command**, ideally auto-joining (`-client <address>`), so testing two-player behaviour does not require menu navigation.
- **A second machine on the LAN can join**, with the firewall/port requirements written down.
- **JIP is exercised deliberately**: a client joins a server that has already been running, and the documented result says what worked and what did not.

### Phase 3 — automated MP testing (not started; explicitly gated on Phase 2)

- **Whether to build this at all is a decision, not a foregone conclusion.** It is only worth it if Phase 2 shows the loop is reliable; a flaky two-process harness is worse than no harness, because it teaches everyone to ignore red.
- **If built:** a scripted run starts a server, connects one or more clients, drives a scenario, and returns an honest 0/1/2/124 verdict in the same taxonomy as `compile-check.sh` and `run-tests.sh`.
- **No new in-game test framework.** `SCR_Autotest` already exists and `SCR_AutotestHelper.WORLD_MPTEST` (`worlds/MP/MpTest/MpTest.ent`) is the framework's own MP starting point.
- **The control seam is chosen empirically, not by preference.** Three candidates, none yet examined: **RCON** (initialises on the dedicated route; protocol and command surface unknown), the **`-autotest` harness running client-side against a remote server**, and feature #1's Phase 7 **`OVT_AgentBridge`** (`CompileScript` eval over a `$profile:` drop-box), which would work on both processes and is the only one that can assert on *server-side* state.
- **Verdicts come from artifacts, never from a client exit code** — the client always exits 0 (finding 1.14b).
- **It must never be the gate that blocks a push** until it has proven itself non-flaky over a documented number of consecutive runs.

## Dependencies

- **`dev-ops/workbench-automation`** — `ovt_run_win`, path translation, log resolution, pidfile registry. Already consumed.
- **Arma Reforger Server**, Steam app **1874900** — a separate download from the game client, and a genuinely new external dependency for this repo. `ovt_server_exe` fails with exit 2 and install instructions when absent.
- **A human with two Reforger instances** for Phase 2. There is no way around this one.
- Phase 3 would additionally depend on whichever control seam wins, and — if it is the agent bridge — on `workbench-automation` Phase 7.

## Out of Scope

- **Console (Xbox/PlayStation) multiplayer.** PC only.
- **Public/Workshop server hosting.** The generated config is `"visible": false` and stays that way; this feature never lists a server publicly.
- **Performance and load testing.** Player-count scaling, server FPS and network saturation are a different problem with different tooling.
- **Replacing manual MP play-testing.** Even a working Phase 3 covers scripted, deterministic MP behaviour. Feel, desync-under-load and emergent multiplayer weirdness still need humans.
- **Anything that could publish.** This feature launches servers; it never packs, uploads or releases (that is feature #5).

---

*Empirical record: `findings.md` in this folder. The shipped tool is documented in `tools/README.md` → `tools/launch-server.sh`.*
