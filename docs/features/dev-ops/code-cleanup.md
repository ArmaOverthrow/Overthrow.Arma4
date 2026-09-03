# Comment cleanup: handoff

This task trims the essay-length comment blocks that were written before CLAUDE.md gained the
"Keep comments sparse" rule. The feature docs already carry this material. The code does not need it.

## Measured baseline (2026-08-22, before any edits)

`Scripts/**/*.c` held 78,180 comment lines and 131,860 code lines (37%). **55,093 of the comment
lines sat in runs of 6 or more consecutive comment lines.** That is the target. Almost all of it is
`//!` doxygen. Thus a split into "narrative `//`" and "doc `//!`" hides the problem.

## Second baseline (2026-09-03, comment prose scored with the STE linter)

The `asd-ste100` skill supplies a linter, `~/.claude/skills/asd-ste100/scripts/ste-lint.py`. It
scores prose for ASD-STE100 violations per 100 words. `comment-prose.py` (see Scripts) strips the
comment markers and the doxygen tags from a `.c` file, so the linter can score the comment text.

| Scope | Comment words | Violations per 100 words |
|---|---|---|
| Whole tree, 2026-09-03 | 805,753 | 3.52 |
| `OVT_InsertionGeometry.c` (pass 1 done) | 1,226 | 4.73 |
| `OVT_DeploymentManager.c` (pass 1 done, has grown since) | 12,504 | 3.85 |
| `OVT_VirtualizationManagerComponent.c` (to do) | 12,415 | 3.91 |
| `OVT_RecruitManagerComponent.c` (to do) | 11,483 | 3.83 |
| `OVT_StorageRequestComponent.c` (to do) | 6,966 | 4.65 |
| BUG-197 doc, rewritten in STE, for comparison | 455 | 1.59 |

Pass 1 cut the volume but not the style. A file that pass 1 finished scores the same as one it did
not touch. The dominant categories in `OVT_DeploymentManager.c` are passive voice (158),
contractions (144), semicolons (46), and `-ing` main verbs (30).

Top of the tree by comment words: `OVT_TEST_InitSuite.c` (27,391), `OVT_TEST_PersistenceRoundTripSuite.c`
(24,137), `OVT_ObjectiveDirectorComponent.c` (20,043), then the two 12,000-word managers above.

## Rules agreed with the user

| | |
|---|---|
| **Aggression** | **Revised 2026-08-23, after the user said the first pass was still too long.** Keep the summary paragraph (1 to 3 lines) and each `\param` and `\return`. A const or a member variable gets one line, two at most. Keep at most one `⚠` or `🔴` line, for a trap that the code cannot say. Cut everything else: history, dated forensics, play-test narratives, rationale essays, restatements of a `\param`, and "why this is correct" arguments. The feature docs already carry it. The rule that made most of the second pass: **keep the first paragraph plus the trailing tag block.** Delete the rest. Then trim by hand what is still over two lines. |
| **Words** | Write each replacement comment in STE, flavored mode (skill `asd-ste100`, Layer 1). Active voice, simple tenses, no contractions, no semicolons, one instruction per sentence. Lint the spec before you apply it. Target: under 2.5 violations per 100 words. |
| **Deleted rationale** | Delete it. `docs/features/` covers it, and git history has the rest. Do **not** append it to the feature docs. |
| **Scope** | The top 13 offenders by essay-line count. Then stop and report. |
| **Hard invariant** | The non-comment lines must be **byte-identical** before and after. Examine each file with the command below. |

## Done so far, all examined code-identical

Committed as 714fab4c, "(chore) Clean up excessive comment bloat pt 1".

| File | Before → after |
|---|---|
| `Scripts/Game/GameMode/Deployments/OVT_InsertionGeometry.c` | 453 → 260 |
| `Scripts/Game/UI/Map/Territory/OVT_TerritorySolver.c` | 1697 → 1565 |
| `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c` | 13301 → 12345 |
| `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` | 13530 → 12822 |
| `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` | 4509 → 4212 |
| `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` | 3749 → 3368, then 3455 → **2587** (the file had grown, and an orphaned `DescribeCrewLiveness` doc moved back onto its method) |
| `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` | 3018 → 2824. Now 3278: features added since then wrote new essays. |

Nobody ran the compile check or the suites after pass 1. The edits touch comments only, so
`tools/compile-check.sh` must pass, but nobody ran it.

## Still to do (the other 7 of the top 13)

1. `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c` (about 900 essay lines)
2. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c` (about 801)
3. `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` (about 767)
4. `Scripts/Game/GameMode/Objectives/Modules/OVT_RaiseForwardBaseObjectiveOperation.c` (about 550)
5. `Scripts/Game/Components/Controller/OVT_RecruitCommandComponent.c` (about 529)
6. `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (about 515)
7. `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` (about 510)

Then **stop and report**. A wider sweep needs the go-ahead of the user. A candidate for the wider
sweep: a second pass over the pass 1 files, for style, with the linter as the gate.

## Workflow

Per file: extract the essay blocks, read them, write a replacement spec, lint the spec, apply it,
then examine the result. Never edit the file by hand. The apply script is what guarantees that the
code lines stay untouched.

```bash
S=/tmp/cleanup            # any scratch dir; recreate the four scripts below in it
LINT=~/.claude/skills/asd-ste100/scripts/ste-lint.py
F=Scripts/Game/.../Target.c

# 0. score the comment prose before you start
python3 $S/comment-prose.py "$F" > $S/t.before.md && python3 $LINT $S/t.before.md

# 1. extract blocks whose PROSE line count (separators and \param/\return tags excluded) is >= 6
awk -v MINPROSE=6 -f $S/blocks.awk "$F" > $S/t.blocks
grep -c '^@@END' $S/t.blocks                      # how many blocks to rewrite

# 2. skim the index first, then read the dump in chunks of about 300 lines
awk '/^@@[0-9]/{h=$0; getline; getline; print h" | "$0}' $S/t.blocks
sed -n '1,300p' $S/t.blocks

# 3. write $S/t.spec.raw: for each block, its "@@<start>-<end>" header, the replacement
#    lines, then "@@END". Use @T@ for a literal TAB (the file is tab-indented).
#    EVERY block in t.blocks must get an entry. Step 4 validates the ranges.

# 4. validate the ranges before you apply
grep -oE '^@@[0-9]+-[0-9]+' $S/t.spec.raw | sort -u > $S/s.r
grep -oE '^@@[0-9]+-[0-9]+' $S/t.blocks   | sort -u > $S/b.r
comm -13 $S/s.r $S/b.r   # must be empty (nothing missing)
comm -23 $S/s.r $S/b.r   # must be empty (no invented ranges)

# 5. lint the replacement prose. Fix the reported categories, then lint one more time.
grep -vE '^@@' $S/t.spec.raw | python3 $S/comment-prose.py /dev/stdin > $S/t.spec.md
python3 $LINT --fail-over 2.5 $S/t.spec.md

# 6. apply, then make sure the code is untouched
sed 's/@T@/\t/g' $S/t.spec.raw > $S/t.spec
python3 $S/apply.py "$F" $S/t.spec
git show HEAD:"$F" | grep -vE '^\s*(//|$)' > $S/b.txt
grep -vE '^\s*(//|$)' "$F" > $S/a.txt
diff $S/b.txt $S/a.txt && echo "CODE IDENTICAL"    # must print CODE IDENTICAL

# 7. score the comment prose again, and record both scores in the table above
python3 $S/comment-prose.py "$F" > $S/t.after.md && python3 $LINT $S/t.after.md
```

### Gotchas

- **A block can span two doc comments.** Only a line of code ends a run. Thus a `//---` banner, a
  blank line, and then the doc header of a method are ONE block. Examine the raw file at the range
  before you assume. If you invent a range that does not exist, the apply drops your text with no
  error. Step 4 catches this.
- `apply.py` applies the blocks **bottom-up**. Thus the line numbers in `t.blocks` stay valid for
  each entry in one spec. They become invalid the moment you apply a spec. Extract again if you
  need a second pass over the same file.
- Watch the indentation level. File-header blocks are at column 0. Class members are one tab.
  Inline comments in a method body are two or three tabs.
- The linter is a form checker. It cannot tell a true sentence from a hollow one. A block that
  passes the lint can still be an essay. Apply the Aggression rule first, the linter second.
- The linter reads `-` as a word, not as an em dash. Comments use ` - ` as a dash. Count those by
  hand, or with `grep -c ' - '`, when you want the slop-marker number.

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

`$S/comment-prose.py`: emits the prose of each `//` comment line, with the markers and the doxygen
tags removed. Code lines become paragraph breaks. Pipe the output into `ste-lint.py`.
```python
import sys, re
for path in sys.argv[1:]:
    out=[]
    for ln in open(path, encoding='utf-8', errors='replace'):
        t=ln.strip()
        if not t.startswith('//'):
            if out and out[-1]!='': out.append('')
            continue
        t=re.sub(r'^//[!/]*\s?','',t)
        if re.fullmatch(r'-*',t): continue
        t=re.sub(r'^[\\@](param|return|brief|note|sa|see)\S*\s*(\[[^\]]*\]\s*)?(\w+\s+)?','',t)
        if t: out.append(t)
    sys.stdout.write('\n'.join(out)+'\n')
```

### Re-measuring

`$S/c2.awk`: per-file `narrative doc code filename`, to set a new baseline after a pass:
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

To rank the whole tree by essay-block lines (this is how the top 13 were picked), run `blocks.awk`
per file and sum the lines it emits. To rank by comment words and STE score instead:

```bash
mkdir -p $S/tree
find Scripts -name '*.c' | while read f; do
  python3 $S/comment-prose.py "$f" > "$S/tree/$(echo $f | tr / _).md"
done
python3 $LINT $S/tree/*.md | sort -t= -k2 -n -r | head -20      # by words
cat $S/tree/*.md > $S/tree-all.md && python3 $LINT $S/tree-all.md  # whole-tree score
```
