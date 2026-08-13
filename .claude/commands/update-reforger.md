---
description: "Detect a new Arma Reforger build, refresh the reference script extraction, and write a version-change report to docs/reforger. Usage: /update-reforger [version-label] [experimental]"
---

You have been asked to check for a new Arma Reforger version, update the extracted reference tree if needed, and produce a change report focused on impact to Overthrow.

**Arguments (optional):** `$ARGUMENTS`
- A version label like `1.8.0` — the human-readable game version for this build (find it in patch notes or the game main menu). Used to name the extraction and the report file.
- The word `experimental` — compare current stable vs the experimental tree instead of vs the previous stable version. Experimental mode never runs extraction (the experimental tree at `N:\Projects\Arma 4\ArmaReforgerExperimental` is maintained by hand).

## Key paths

| What | Path |
|---|---|
| Steam app manifest (build id) | `/mnt/n/Program Files (x86)/Steam/steamapps/appmanifest_1874880.acf` |
| Current reference tree | `/mnt/n/Projects/Arma 4/ArmaReforger` (marker: `.version.json`) |
| Archived previous versions | `/mnt/n/Projects/Arma 4/ArmaReforger.versions/<label>/` |
| Experimental tree | `/mnt/n/Projects/Arma 4/ArmaReforgerExperimental` |
| Extraction script | `update-arma-scripts.ps1` (repo root, run via `powershell.exe`) |
| Compare script | `.scripts/compare_versions_fast.py` |
| Reports | `docs/reforger/<label>-changes.md` |

## Process

### 1. Detect whether a new build is installed

```bash
grep -oP '"buildid"\s+"\K[0-9]+' "/mnt/n/Program Files (x86)/Steam/steamapps/appmanifest_1874880.acf"
cat "/mnt/n/Projects/Arma 4/ArmaReforger/.version.json"
```

- **Build ids match** → the reference tree is current. Skip extraction (Step 2). If the user asked for `experimental`, continue at Step 3; otherwise tell the user the tree is up to date and ask whether they still want a re-comparison/report against the previous archive.
- **Build ids differ (or marker missing)** → a new build is installed; do Step 2.
- Steam may still be mid-download: if `StateFlags` in the manifest is not `4`, warn the user the install may be incomplete and confirm before extracting.

### 2. Run extraction (only when a new build is detected)

The extraction archives the old tree to `ArmaReforger.versions/<old-label>/` automatically, then extracts ~7 GB of paks (takes minutes — run in background and poll):

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "N:\Projects\Arma 4\Overthrow.Arma4\update-arma-scripts.ps1" -VersionLabel "<label>"
```

- `<label>` = the version label from `$ARGUMENTS` (e.g. `1.8.0`). If the user gave none, use the game version if you can find it (patch notes headline, e.g. via WebSearch for "Arma Reforger <today> update changelog"); fall back to omitting `-VersionLabel` (the script then labels it `build-<buildid>`).
- Watch the output for `WARNING: Source folder not found` — game updates shuffle content between numbered paks (e.g. UI moved data009→data010 in mid-2026). If a folder went missing, list the extracted `dataNNN` dirs next to the paks, find the new home, fix `$extractionMap` in `update-arma-scripts.ps1` (it must stay `[ordered]@{}` — later paks layer over earlier ones), and re-run.
- Verify afterwards: `.version.json` in the tree has the new buildId, and the old tree landed in `ArmaReforger.versions/`.

### 3. Compare versions

```bash
python3 .scripts/compare_versions_fast.py            # current vs latest archive (default)
python3 .scripts/compare_versions_fast.py experimental   # current vs experimental tree
```

Results land in `.tmp/reforger-compare/version_comparison_results.json` (with `comparison.old_label` / `new_label`). `.scripts/analyze_changes.py [path]` gives a quick categorized breakdown of the same JSON.

### 4. Analyze impact on Overthrow

Both trees are on disk, so you can diff real files. Don't just report counts — investigate:

1. **Breaking-risk first.** Build the list of vanilla classes Overthrow touches:
   - `grep -rhoP 'modded class \K\w+' Scripts/` — modded classes (highest risk: signature/member changes break compile)
   - `grep -rhoP 'class OVT_\w+ : \K[A-Z]\w+' Scripts/ | sort -u | grep -v '^OVT_'` — base classes we inherit
2. For each changed script file in the comparison whose class appears in those lists, diff it between the two trees (`diff old/new`) and read the hunks. Classify: signature change / removed member / behavioral change / cosmetic.
3. **New systems.** Read the `new_directories` and `important_system_changes` sections; skim a few representative new files to describe what each new system actually is — never guess from filenames alone.
4. **Configs and prefabs Overthrow overrides.** Same-GUID overrides are deltas over vanilla — a vanilla prefab/config change flows into Overthrow silently. Check modified `.et`/`.conf` files against `Prefabs/` and `Configs/` overrides in this repo (match by GUID where possible).
5. **Compile gate.** Run `tools/compile-check.sh` — this compiles Overthrow against the *installed* (new) build and is the single best breaking-change detector. Include its verdict and any errors verbatim. If it passes, also run the Fast test group `tools/run-tests.sh "{6A6E29FF47ECB840}"`.

### 5. Write the report

Write to `docs/reforger/<new-label>-changes.md` (e.g. `1.8.0-changes.md`; for experimental comparisons use `<label>-vs-experimental.md`). Structure:

```markdown
# Arma Reforger <old-label> → <new-label> — Change Report

**Date:** <today>  |  **Build:** <old buildid> → <new buildid>
**Compile check:** PASS/FAIL  |  **Fast tests:** X/Y

## Summary
2-4 sentences: how big is this update, and the headline for Overthrow.

## Breaking / High-Risk Changes
Per finding: vanilla file, what changed (with the relevant diff hunk or signature),
which Overthrow files are affected (file:line), and required action.

## New Systems & Content
What's new, what it does (from reading the files), and any opportunity for Overthrow.

## Changed Systems Relevant to Overthrow
Systems Overthrow builds on (AI, economy seams, persistence, inventory, factions, UI)
that changed — even non-breaking — with a note on whether behavior may shift.

## Statistics
File counts table from the comparison JSON.

## Recommended Follow-ups
Checklist of concrete actions (fixes, play-tests to run, opportunities to file).
```

Every claim about what changed must be backed by a file you actually diffed or read — cite paths. Counts alone are not findings.

### 6. Show summary

Tell the user: old → new version, whether extraction ran, compile/test verdict, the top 3 findings, and the report path. Suggest committing the report.

## Error handling

- **PakEntpacker missing** (`N:\Temp\PakEntpacker\PakEntpacker.exe`): stop and tell the user — extraction is impossible without it.
- **No archives yet** in `previous` mode: the archive is only created when extraction replaces an older tree. If extraction just ran for the first time with versioning, the pre-1.8 tree should have been archived; if `ArmaReforger.versions/` is truly empty, fall back to `experimental` mode or ask the user.
- **Comparison shows almost everything modified**: extraction rewrites mtimes, so the script hash-checks same-size files — this is normal and just slower; let it finish.
