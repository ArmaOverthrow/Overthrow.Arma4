# Comment cleanup — handoff

Trimming the essay-length comment blocks written before CLAUDE.md gained the "Keep comments sparse"
rule. The feature docs already carry this material; the code does not need it.

## Measured baseline (2026-08-22, before any edits)

78,180 comment lines vs 131,860 code lines across `Scripts/**/*.c` (37%). **55,093 of those sat
inside runs of 6+ consecutive comment lines** — that is the target. Almost all of it is written as
`//!` doxygen, so a naive "narrative `//` vs doc `//!`" split hides the problem entirely.

## Rules agreed with the user

| | |
|---|---|
| **Aggression** | Keep the one-line `//!` summary, keep every `\param` / `\return`, keep 1–3 lines for a genuinely load-bearing trap (prefix `⚠` / `🔴` as the original did). Cut history, dated forensics, play-test narratives, "PROVEN ABLE TO FAIL" blocks, rationale essays, and paragraphs that only restate a `\param`. |
| **Deleted rationale** | Just delete it. `docs/features/` covers it and git history has the rest. Do **not** append it to feature docs. |
| **Scope** | Top 13 offenders by essay-line count, then stop and report. |
| **Hard invariant** | Non-comment lines must be **byte-identical** before and after. Verify every file (command below). |

## Done so far — all verified code-identical

| File | Before → after |
|---|---|
| `Scripts/Game/GameMode/Deployments/OVT_InsertionGeometry.c` | 453 → 260 |
| `Scripts/Game/UI/Map/Territory/OVT_TerritorySolver.c` | 1697 → 1565 |
| `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c` | 13301 → 12345 |
| `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` | 13530 → 12822 |
| `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` | 4509 → 4212 |
| `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` | 3749 → 3368 |
| `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` | 3018 → 2824 |

All uncommitted, nothing committed yet. Nothing has been re-compiled or suite-run — comments only,
so `tools/compile-check.sh` should be clean, but it has not been run.

## Still to do (remaining 7 of the top 13)

1. `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c` (~900 essay lines)
2. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c` (~801)
3. `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` (~767)
4. `Scripts/Game/GameMode/Objectives/Modules/OVT_RaiseForwardBaseObjectiveOperation.c` (~550)
5. `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c` (~529)
6. `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (~515)
7. `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` (~510)

Then **stop and report**; a wider sweep needs the user's go-ahead.

## Workflow

Per file: extract the essay blocks → read them → write a replacement spec → apply → verify.
Never hand-edit the file directly; the apply script is what guarantees the code lines are untouched.

```bash
S=/tmp/cleanup            # any scratch dir; recreate the three scripts below in it
F=Scripts/Game/.../Target.c

# 1. extract blocks whose PROSE line count (excluding separators and \param/\return tags) is >= 6
awk -v MINPROSE=6 -f $S/blocks.awk "$F" > $S/t.blocks
grep -c '^@@END' $S/t.blocks                      # how many blocks to rewrite

# 2. skim the index first, then read the dump in ~300-line chunks
awk '/^@@[0-9]/{h=$0; getline; getline; print h" | "$0}' $S/t.blocks
sed -n '1,300p' $S/t.blocks

# 3. write $S/t.spec.raw: for each block, its "@@<start>-<end>" header, the replacement
#    lines, then "@@END". Use @T@ for a literal TAB (the file is tab-indented).
#    EVERY block in t.blocks must get an entry — the ranges are validated below.

# 4. validate the ranges before applying
grep -oE '^@@[0-9]+-[0-9]+' $S/t.spec.raw | sort -u > $S/s.r
grep -oE '^@@[0-9]+-[0-9]+' $S/t.blocks   | sort -u > $S/b.r
comm -13 $S/s.r $S/b.r   # must be empty (nothing missing)
comm -23 $S/s.r $S/b.r   # must be empty (no invented ranges)

# 5. apply and verify code is untouched
sed 's/@T@/\t/g' $S/t.spec.raw > $S/t.spec
python3 $S/apply.py "$F" $S/t.spec
git show HEAD:"$F" | grep -vE '^\s*(//|$)' > $S/b.txt
grep -vE '^\s*(//|$)' "$F" > $S/a.txt
diff $S/b.txt $S/a.txt && echo "CODE IDENTICAL"    # must print CODE IDENTICAL
```

### Gotchas

- **A block can span two doc comments.** A run is only broken by a line of code, so a `//---` banner
  followed by a blank line and then a method's doc header is ONE block. Check the raw file at the
  range before assuming, or you will invent a range that does not exist and the apply will silently
  drop your text. (Range validation in step 4 catches it.)
- Blocks are applied **bottom-up** by `apply.py`, so the line numbers in `t.blocks` stay valid for
  every entry in one spec — but they are invalidated the moment a spec is applied. Re-extract if you
  need a second pass over the same file.
- Watch the indentation level: file-header blocks are at column 0, class members are one tab, inline
  comments inside a method body are two or three.

### Scripts

`$S/blocks.awk`
```awk
BEGIN { start=0; n=0; inblk=0 }
function flush(  i,prose,l) {
  if (n>0) {
    prose=0
    for (i=1;i<=n;i++) {
      l=buf[i]
      gsub(/^[ \t]*(\/\/[!\/]*|\*)[ \t]*/,"",l)
      if (l ~ /^-+$/ || l=="" ) continue
      if (l ~ /^[\\@](param|return|brief|p |sa|see|note)/) continue
      prose++
    }
    if (prose>=MINPROSE) {
      printf "@@%d-%d prose=%d\n", start, start+n-1, prose
      for (i=1;i<=n;i++) print buf[i]
      print "@@END"
    }
  }
  n=0
}
{
  line=$0; t=line; gsub(/^[ \t]+|[ \t]+$/,"",t)
  isc=0
  if (inblk) { isc=1; if (t ~ /\*\//) inblk=0 }
  else if (t ~ /^\/\//) isc=1
  else if (t ~ /^\/\*/) { isc=1; if (t !~ /\*\//) inblk=1 }
  if (isc) { if (n==0) start=NR; buf[++n]=line; next }
  if (t=="" ) { if (n>0) { buf[++n]=line } next }
  flush()
}
END { flush() }
```

`$S/apply.py`
```python
import sys, re
target, spec = sys.argv[1], sys.argv[2]
src = open(target, encoding='utf-8').read().split('\n')
edits, cur = [], None
for ln in open(spec, encoding='utf-8').read().split('\n'):
    m = re.match(r'^@@(\d+)-(\d+)$', ln)
    if m:
        cur = [int(m.group(1)), int(m.group(2)), []]; continue
    if ln == '@@END':
        edits.append(cur); cur = None; continue
    if cur is not None:
        cur[2].append(ln)
edits.sort(key=lambda e: -e[0])
for s, e, rep in edits:
    src[s-1:e] = rep
open(target, 'w', encoding='utf-8').write('\n'.join(src))
print(f"applied {len(edits)} edits to {target}")
```

### Re-measuring

`$S/c2.awk` — per-file `narrative doc code filename`, to re-baseline after a pass:
```awk
BEGIN { doc=0; nar=0; code=0; inblk=0; blkdoc=0 }
{
  line=$0; gsub(/^[ \t]+|[ \t]+$/,"",line)
  if (line=="") next
  if (inblk) { if (blkdoc) doc++; else nar++; if (line ~ /\*\//) inblk=0; next }
  if (line ~ /^\/\/!/) { doc++; next }
  if (line ~ /^\/\//) { nar++; next }
  if (line ~ /^\/\*/) { blkdoc=(line ~ /^\/\*[!*]/); if (blkdoc) doc++; else nar++
                        if (line !~ /\*\//) inblk=1; next }
  code++
}
END { printf "%d %d %d %s\n", nar, doc, code, FILENAME }
```

To rank the whole tree by essay-block lines (this is how the top 13 were picked), run the same
`blocks.awk` per file and sum the lines it emits, or use a run-length variant that reports
`total-lines-in-6+-line-runs` per file.
