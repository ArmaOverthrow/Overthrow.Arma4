#!/usr/bin/env python3
"""Verify every placeable AND buildable prefab is wired up: Overthrow component AND replication.

WHY THIS EXISTS
---------------
An object the player puts in the world is only persisted, only owned, and only visible to
type-aware features if its entity carries the matching component - OVT_PlaceableComponent for
placeables, OVT_BuildableComponent for buildables:

  * persistence   Configs/Systems/Persistence/Overthrow.conf matches these objects with
                  `ComponentClassPersistenceConfigRule { ComponentClass "OVT_PlaceableComponent" }`
                  (and the same rule again for OVT_BuildableComponent).
                  OVT_ResistanceFactionManager.PlaceItem()/BuildItem() do call
                  OVT_PersistenceTracking.Track() on every spawn, but tracking does not choose a
                  serializer - "tracking an entity no rule matches is harmless and simply stores
                  nothing". No component, no save.
  * ownership     PlaceItem()/BuildItem() log a WARNING and skip SetOwnerPersistentId(), so the
                  client-side removal mode (which checks ownership) can never remove the object.
  * type matching OVT_PlaceableItemJobStage and OVT_TownPlaceableCountJobCondition compare
                  GetPlaceableType(), which is ONLY ever set as a prefab attribute - nothing
                  assigns it at runtime.

And it is only VISIBLE to anyone but the server if the entity carries an RplComponent:

  * replication   PlaceItem()/BuildItem() spawn the real object on the SERVER
                  (OVT_Global.SpawnEntityPrefabMatrix). A runtime-spawned entity with no
                  RplComponent is server-local: it is never sent to clients, so on a dedicated
                  server nobody sees the thing they just paid for. The placement GHOST is spawned
                  client-side by OVT_PlaceContext and looks perfectly normal, which is what makes
                  this so easy to miss - and a listen-server host, where server and client are one
                  process, cannot reproduce it at all.
                  The trap is inheritance: several placeables are built on static base-game props
                  (Destructible_Props_Base and friends) that never needed replication because
                  vanilla only ever places them at design time.

None of that is visible to compile-check.sh (it compiles scripts, not prefabs) or to the autotests.
The failure is silent: the object places, looks right, and quietly evaporates on continue - or, for
the replication case, never appears for anyone but the server in the first place.

THE PART THAT IS EASY TO GET WRONG - and the reason this is a script and not a grep. The component
is frequently NOT on the prefab named in the config. Overthrow overrides shared BASE prefabs
(Prefabs/Props/Military/Furniture/FurnitureMilitary_base.et, Prefabs/Structures/Signs/Signs_Base.et,
Prefabs/Props/Civilian/LampKerosene_01/LampKerosene_01_base.et) by shipping a file at the SAME PATH
with the SAME GUID as the base game's, adding nothing but the component - so dozens of untouched
base-game leaf prefabs inherit it. Deciding whether an entry is wired up therefore means walking its
whole inheritance chain across both the mod and the game, which is what this does.

USAGE
    tools/check-placeables.py [--reforger DIR] [--conf FILE] [--quiet] [--verbose] [--strict]

EXIT CODES  (same contract as the other tools/ scripts)
    0   every prefab of every entry resolves to a component and to replication - verified clean
    1   at least one problem found (details on stdout)
    2   indeterminate: a config or the reference tree could not be read

Requires python3 (WSL default) and a checkout of the base-game reference tree, by default
../ArmaReforger relative to the repo root, overridable with --reforger or $OVERTHROW_REFORGER_DIR.
"""

import argparse
import os
import re
import sys


class Kind:
    """One config family: which block class holds entries, and which component they must carry."""

    def __init__(self, label, conf, block_class, component, type_attr):
        self.label = label
        self.conf = conf
        self.component = component
        self.type_attr = type_attr
        self.re_open = re.compile(r'^(\s*)' + block_class + r'\s+"\{[0-9A-Fa-f]+\}"\s*\{')
        self.re_component = re.compile(r'^(\s*)' + component + r'\s+"\{([0-9A-Fa-f]+)\}"\s*\{')
        self.re_type = re.compile(r'^\s*' + type_attr + r'\s+"([^"]*)"')


KINDS = [
    Kind("placeable", "Configs/Resistance/placeables.conf",
         "OVT_Placeable", "OVT_PlaceableComponent", "m_sPlaceableType"),
    Kind("buildable", "Configs/Resistance/buildables.conf",
         "OVT_Buildable", "OVT_BuildableComponent", "m_sBuildableType"),
]

RE_PARENT = re.compile(r'^\s*[\w_]+\s*:\s*"\{([0-9A-Fa-f]+)\}([^"]*)"')
RE_META_GUID = re.compile(r'Name\s+"\{([0-9A-Fa-f]+)\}')

# Replication is kind-independent, so these live outside Kind. A component may inherit a component
# template (`RplComponent "{guid}" : "{guid}path.ct" {`), which is still a declaration.
RE_RPL_OPEN = re.compile(r'^(\s*)RplComponent\s+"\{([0-9A-Fa-f]+)\}"\s*(?::\s*"[^"]*"\s*)?\{')
RE_RPL_ENABLED = re.compile(r'^\s*Enabled\s+(\d+)\s*$')
RE_NAME = re.compile(r'^\s*m_sName\s+"([^"]*)"')
RE_PREFABS_OPEN = re.compile(r'^(\s*)m_aPrefabs\s*\{')
RE_PREFAB_REF = re.compile(r'^\s*"\{([0-9A-Fa-f]+)\}([^"]*)"')
RE_RANDOMIZE = re.compile(r'^\s*m_bRandomizePrefab\s+1\s*$')

# ANSI only when stdout is a terminal, so redirected output stays diffable
if sys.stdout.isatty():
    RED, YELLOW, GREEN, DIM, RESET = "\033[31m", "\033[33m", "\033[32m", "\033[2m", "\033[0m"
else:
    RED = YELLOW = GREEN = DIM = RESET = ""


class Entry:
    """One OVT_Placeable / OVT_Buildable entry from a config."""

    def __init__(self, name):
        self.name = name
        self.prefabs = []       # list of (guid, path)
        self.randomize = False  # m_bRandomizePrefab: the game picks a variant, the player does not


class ChainResult:
    """What walking one prefab's inheritance chain found."""

    def __init__(self):
        self.chain = []             # [(path, shadows_a_game_path)] most derived first
        self.component_levels = []  # [(path, component_guid)] every level declaring the component
        self.object_type = None     # effective type: most derived non-empty type attribute
        self.type_level = None      # which file that type came from
        self.unresolved = None      # path we could not open, if the walk stopped early
        self.rpl_levels = []        # [(path, rpl_guid)] every level declaring an RplComponent
        self.rpl_enabled = None     # effective Enabled: most derived level that states one
        self.rpl_enabled_level = None  # which file stated it


def parse_conf(path, kind):
    """Read a placeables/buildables config into a list of Entry, preserving file order."""
    entries = []
    current = None
    in_prefabs = False
    prefabs_indent = None

    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if kind.re_open.match(line):
                current = Entry(None)
                entries.append(current)
                in_prefabs = False
                continue

            if current is None:
                continue

            if in_prefabs:
                ref = RE_PREFAB_REF.match(line)
                if ref:
                    current.prefabs.append((ref.group(1).upper(), ref.group(2)))
                    continue
                if line.strip() == "}" and (prefabs_indent is None or line.index("}") <= len(prefabs_indent)):
                    in_prefabs = False
                continue

            named = RE_NAME.match(line)
            if named and current.name is None:
                current.name = named.group(1)
                continue

            if RE_RANDOMIZE.match(line):
                current.randomize = True
                continue

            prefabs_open = RE_PREFABS_OPEN.match(line)
            if prefabs_open:
                in_prefabs = True
                prefabs_indent = prefabs_open.group(1)

    return entries


def read_meta_guid(prefab_file):
    """The GUID a prefab file publishes in its .meta, or None when there is no meta."""
    meta = prefab_file + ".meta"
    if not os.path.isfile(meta):
        return None
    with open(meta, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            found = RE_META_GUID.search(line)
            if found:
                return found.group(1).upper()
    return None


def resolve(rel_path, mod_root, game_root):
    """Resolve an addon-relative prefab path, mod first.

    The mod shadows the game at the same path, which is exactly how Overthrow overrides shared base
    prefabs. Returns (absolute_path, shadows_game) where shadows_game means the game has a file at
    that path too, or (None, False) when neither tree has it.
    """
    rel_path = rel_path.lstrip("/")
    in_mod = os.path.join(mod_root, rel_path)
    in_game = os.path.join(game_root, rel_path)

    mod_has = os.path.isfile(in_mod)
    game_has = os.path.isfile(in_game)

    if mod_has:
        return in_mod, game_has
    if game_has:
        return in_game, False
    return None, False


def scan_prefab(prefab_file, kind):
    """Read one prefab file: its parent reference, its matching components, and its RplComponent.

    Returns (parent_guid, parent_path, [(component_guid, type)], [(rpl_guid, enabled)]).
    The type is only collected while inside that component's own braces, so a type attribute
    belonging to some other component can never be misattributed - and the same applies to the
    RplComponent's Enabled flag, which is a name plenty of other components use too.
    `enabled` is None when the declaration does not state one (i.e. it inherits).
    """
    parent_guid = None
    parent_path = None
    components = []
    rpl = []

    inside = False
    depth = 0
    guid = None
    otype = None

    in_rpl = False
    rpl_depth = 0
    rpl_guid = None
    rpl_enabled = None

    with open(prefab_file, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if parent_guid is None and not inside and not in_rpl:
                parent = RE_PARENT.match(line)
                if parent:
                    parent_guid = parent.group(1).upper()
                    parent_path = parent.group(2)

            if in_rpl:
                rpl_depth += line.count("{") - line.count("}")
                flag = RE_RPL_ENABLED.match(line)
                if flag:
                    rpl_enabled = flag.group(1) != "0"
                if rpl_depth <= 0:
                    rpl.append((rpl_guid, rpl_enabled))
                    in_rpl = False
                    rpl_guid = None
                    rpl_enabled = None
                continue

            if inside:
                depth += line.count("{") - line.count("}")
                typed = kind.re_type.match(line)
                if typed:
                    otype = typed.group(1)
                if depth <= 0:
                    components.append((guid, otype))
                    inside = False
                    guid = None
                    otype = None
                continue

            opened_rpl = RE_RPL_OPEN.match(line)
            if opened_rpl:
                in_rpl = True
                rpl_guid = opened_rpl.group(2).upper()
                rpl_enabled = None
                rpl_depth = line.count("{") - line.count("}")
                if rpl_depth <= 0:  # single-line declaration
                    rpl.append((rpl_guid, rpl_enabled))
                    in_rpl = False
                    rpl_guid = None
                continue

            opened = kind.re_component.match(line)
            if opened:
                inside = True
                guid = opened.group(2).upper()
                otype = None
                depth = line.count("{") - line.count("}")
                if depth <= 0:  # single-line declaration
                    components.append((guid, otype))
                    inside = False
                    guid = None

    return parent_guid, parent_path, components, rpl


def walk_chain(rel_path, mod_root, game_root, kind, max_depth=32):
    """Follow a prefab's inheritance chain, collecting every component declaration on the way up.

    A level where the mod shadows a game path is read TWICE, mod first. A same-path override is a
    DELTA, not a replacement: Prefabs/Structures/Signs/Signs_Base.et in the mod contains nothing but
    OVT_PlaceableComponent, while the game's file at that path supplies the mesh, the rigid body and
    the RplComponent - and both are live at runtime. Reading only the mod side would report every
    sign as unreplicated, which is exactly backwards.
    """
    result = ChainResult()
    seen = set()

    while rel_path and len(result.chain) < max_depth:
        prefab_file, shadows_game = resolve(rel_path, mod_root, game_root)
        if not prefab_file:
            result.unresolved = rel_path
            break
        if prefab_file in seen:  # cyclic inheritance would otherwise spin
            break
        seen.add(prefab_file)

        result.chain.append((rel_path, shadows_game))

        files = [prefab_file]
        if shadows_game:
            files.append(os.path.join(game_root, rel_path.lstrip("/")))

        parent_path = None

        for level_file in files:
            _, level_parent, components, rpl = scan_prefab(level_file, kind)

            # The delta may or may not restate the base; whichever side names one, they agree
            if level_parent and parent_path is None:
                parent_path = level_parent

            for guid, otype in components:
                result.component_levels.append((rel_path, guid))
                # most derived wins, and the chain is walked most-derived first
                if otype and result.object_type is None:
                    result.object_type = otype
                    result.type_level = rel_path

            for guid, enabled in rpl:
                result.rpl_levels.append((rel_path, guid))
                # Same most-derived-wins rule: a level that states nothing inherits the flag
                if enabled is not None and result.rpl_enabled is None:
                    result.rpl_enabled = enabled
                    result.rpl_enabled_level = rel_path

        rel_path = parent_path

    return result


def replication_verdict(result, leaf, strict=False):
    """Decide what one prefab's chain says about replication. Returns (problem, caution), either None.

    An UNRESOLVED chain is the interesting case and the reason this is not a hard fail everywhere.
    For the Overthrow component a truncated chain is harmless - resolve() looks in the mod first, so
    anything unresolvable is by definition a base-game file and only a mod file can declare
    OVT_PlaceableComponent. That reasoning does NOT transfer: base-game prefabs declare RplComponent
    all the time, and the reference tree is a partial extract with some stale paths (nothing in it
    publishes a .meta, so a GUID cannot be re-resolved to the file's real path either). So a chain
    that stopped early and showed no RplComponent is reported as UNPROVEN, not as broken - and
    --strict promotes that to a failure for callers that would rather over-report than ship a
    server-local placeable.
    """
    if result.rpl_levels:
        if result.rpl_enabled is False:
            return ("%s: RplComponent is declared but disabled (Enabled 0 in %s) - the entity the "
                    "SERVER spawns will not be sent to clients, so nobody but the server sees it"
                    % (leaf, os.path.basename(result.rpl_enabled_level))), None
        return None, None

    if result.unresolved:
        message = ("%s: no RplComponent found, but its chain stops at %s (in neither tree), so "
                   "replication could not be proven either way - open the prefab in Workbench and "
                   "confirm the base carries one"
                   % (leaf, result.unresolved))
        if strict:
            return message, None
        return None, message

    return ("%s: no RplComponent anywhere in its chain (%s) - PlaceItem()/BuildItem() spawn it on the "
            "server, and a server-spawned entity without replication never reaches clients (the "
            "placement ghost still looks right, and a listen host cannot reproduce it)"
            % (leaf, " <- ".join(os.path.basename(p) for p, _ in result.chain))), None


def check_kind(kind, repo_root, game_root, args):
    """Check one config. Returns (errors, warnings, entry_count, prefab_count)."""
    conf_path = os.path.join(repo_root, kind.conf)
    if not os.path.isfile(conf_path):
        print("check-placeables: cannot read %s" % conf_path, file=sys.stderr)
        return None

    entries = parse_conf(conf_path, kind)
    if not entries:
        print("check-placeables: parsed no %s entries out of %s" % (kind.label, conf_path),
              file=sys.stderr)
        return None

    errors = 0
    warnings = 0

    if not args.quiet:
        print("%s%s%s" % (DIM, kind.conf, RESET))

    for entry in entries:
        name = entry.name or "<unnamed>"
        problems = []   # break persistence / ownership / matching - these set the exit code
        cautions = []   # worth knowing, not broken
        notes = []
        types_seen = set()

        for guid, rel_path in entry.prefabs:
            result = walk_chain(rel_path, repo_root, game_root, kind)
            leaf = os.path.basename(rel_path)

            if not result.chain:
                problems.append("%s: the prefab named in the config is in neither the mod nor the "
                                "reference tree" % leaf)
                continue

            # A truncated chain is NOT a fault. Base-game prefabs are resolved by GUID at runtime and
            # the path recorded in a .et can be stale (Destructible_Props_Base.et no longer exists
            # under that name), and the reference tree is a partial extract. It is also harmless:
            # resolve() looks in the MOD first, so anything unresolvable is by definition not a mod
            # file, and only a mod file can declare an Overthrow component
            if result.unresolved and args.verbose:
                notes.append("%s: chain truncated at %s (not in either tree - cannot carry %s)"
                             % (leaf, result.unresolved, kind.component))

            # An override only works when the mod's file publishes the SAME guid the referrer asks
            # for. A mod file at the right path with a fresh guid is a DIFFERENT resource, and the
            # inheritance silently keeps using the game's original
            head_file, _ = resolve(rel_path, repo_root, game_root)
            head_guid = read_meta_guid(head_file)
            if head_guid and head_guid != guid:
                problems.append("%s: the config asks for {%s} but that file publishes {%s}"
                                % (leaf, guid, head_guid))

            if not result.component_levels:
                problems.append("%s: no %s anywhere in its chain (%s) - it will not be saved, cannot "
                                "be owned, and cannot be removed by hand"
                                % (leaf, kind.component,
                                   " <- ".join(os.path.basename(p) for p, _ in result.chain)))
            else:
                # Two levels declaring the component under different GUIDs means the child ADDED a
                # second one instead of overriding the inherited declaration
                guids = {g for _, g in result.component_levels}
                if len(guids) > 1:
                    problems.append("%s: %s declared %d times under different GUIDs (%s) - the child "
                                    "adds a duplicate component instead of overriding the inherited one"
                                    % (leaf, kind.component, len(result.component_levels),
                                       ", ".join("%s in %s" % (g, os.path.basename(p))
                                                 for p, g in result.component_levels)))

                if not result.object_type:
                    cautions.append("%s: has %s but no %s - it is saved and owned, but no type-aware "
                                    "feature (job stages, quota conditions) can see it"
                                    % (leaf, kind.component, kind.type_attr))
                else:
                    types_seen.add(result.object_type)

            # Replication is checked whether or not the Overthrow component resolved: they are
            # independent faults and a prefab can easily have both
            problem, caution = replication_verdict(result, leaf, args.strict)
            if problem:
                problems.append(problem)
            if caution:
                cautions.append(caution)

            if args.verbose:
                type_source = result.type_level
                if not type_source and result.component_levels:
                    type_source = result.component_levels[0][0]
                notes.append("%s: type %s from %s"
                             % (leaf,
                                "'%s'" % result.object_type if result.object_type else "<unset>",
                                os.path.basename(type_source) if type_source else "<nowhere>"))
                if result.rpl_levels:
                    notes.append("    RplComponent from %s%s"
                                 % (os.path.basename(result.rpl_levels[0][0]),
                                    "" if result.rpl_enabled is None
                                    else " (Enabled %d in %s)"
                                         % (1 if result.rpl_enabled else 0,
                                            os.path.basename(result.rpl_enabled_level))))
                notes.append("    chain: %s" % " <- ".join(
                    "%s%s" % (os.path.basename(p), "*" if o else "") for p, o in result.chain))

        # Variants disagreeing on type is only a fault when the GAME picks the variant; when the
        # player picks it from the place menu, a mixed bag is a content decision
        if len(types_seen) > 1:
            spread = "variants resolve to different types: %s" % ", ".join(sorted(types_seen))
            if entry.randomize:
                problems.append(spread + " - m_bRandomizePrefab makes the resulting type a dice roll")
            else:
                cautions.append(spread + " (player picks the variant, so this may be intended)")

        # Not a fault: the config name and the prefab type are separate strings. Worth surfacing,
        # because job configs and quota conditions must use the PREFAB's string, never m_sName
        if len(types_seen) == 1:
            only = next(iter(types_seen))
            if entry.name and only != entry.name:
                cautions.append("type is '%s', not '%s' - features matching on type must use '%s'"
                                % (only, entry.name, only))

        if problems:
            errors += len(problems)
            print("%sFAIL%s %-26s %d prefab(s)" % (RED, RESET, name, len(entry.prefabs)))
        elif cautions:
            print("%sWARN%s %-26s %d prefab(s)" % (YELLOW, RESET, name, len(entry.prefabs)))
        elif not args.quiet:
            only = next(iter(types_seen)) if types_seen else "?"
            print("%s OK %s %-26s type '%s' across %d prefab(s)"
                  % (GREEN, RESET, name, only, len(entry.prefabs)))

        for problem in problems:
            print("     %s" % problem)

        warnings += len(cautions)
        if not args.quiet or problems:
            for caution in cautions:
                print("     %s" % caution)

        if args.verbose:
            for note in notes:
                print("     %s%s%s" % (DIM, note, RESET))

    if not args.quiet:
        print()

    return errors, warnings, len(entries), sum(len(e.prefabs) for e in entries)


def main():
    parser = argparse.ArgumentParser(
        description="Verify every placeable and buildable prefab resolves to its Overthrow component.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--reforger", default=os.environ.get("OVERTHROW_REFORGER_DIR"),
                        help="base-game reference tree (default: ../ArmaReforger, or $OVERTHROW_REFORGER_DIR)")
    parser.add_argument("--conf", default=None,
                        help="check only this config, repo-relative (default: both placeables.conf and buildables.conf)")
    parser.add_argument("--quiet", action="store_true", help="only print problems and the summary")
    parser.add_argument("--verbose", action="store_true", help="print each prefab's resolved type and full chain")
    parser.add_argument("--strict", action="store_true",
                        help="fail on prefabs whose replication could not be PROVEN (chain truncated "
                             "outside both trees), not only on prefabs proven to lack it")
    args = parser.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    game_root = args.reforger or os.path.join(os.path.dirname(repo_root), "ArmaReforger")

    if not os.path.isdir(game_root):
        print("check-placeables: reference tree not found: %s" % game_root, file=sys.stderr)
        print("check-placeables: pass --reforger DIR or set $OVERTHROW_REFORGER_DIR", file=sys.stderr)
        return 2

    kinds = KINDS
    if args.conf:
        kinds = [k for k in KINDS if os.path.normpath(k.conf) == os.path.normpath(args.conf)]
        if not kinds:
            print("check-placeables: --conf must be one of: %s"
                  % ", ".join(k.conf for k in KINDS), file=sys.stderr)
            return 2

    errors = warnings = entry_count = prefab_count = 0
    for kind in kinds:
        result = check_kind(kind, repo_root, game_root, args)
        if result is None:
            return 2
        errors += result[0]
        warnings += result[1]
        entry_count += result[2]
        prefab_count += result[3]

    if errors:
        print("check-placeables: FAILED (%d problem(s) across %d entries / %d prefabs)"
              % (errors, entry_count, prefab_count))
        return 1

    print("check-placeables: OK (%d entries, %d prefabs, %d warning(s))"
          % (entry_count, prefab_count, warnings))
    return 0


if __name__ == "__main__":
    sys.exit(main())
