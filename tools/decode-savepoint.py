#!/usr/bin/env python3
"""Read a Reforger persistence save point: what is in it, what changed, and where an id lives.

WHY THIS EXISTS
---------------
Three persistence bugs in a row could only be settled by reading the save file itself, because
every other source lies by omission:

  * BUG-086  "released records are not durable"  — the logs said NOT_FOUND; only the blob showed
             whether the record had ever been written.
  * BUG-104  a Continue showed $0                — the save held `money 0x000186A0` = 100,000 all
             along, which is what moved the search off the save path and onto the ID mapping.
  * BUG-116  items vanish from containers        — the logs showed 744 rejected ids; only the blob
             showed that the newest save point contains each of them exactly ONCE, which killed
             the leading theory outright.
  * BUG-118  unbounded save growth               — `diff` on two consecutive save points gave the
             exact figure (+492 records per restart, 0 removed) in one command.

Reading the blob is the difference between a hypothesis and a measurement. Everything below was
derived by inspecting real save points; none of it comes from documentation, because there is none.

THE FORMAT (reverse-engineered 2026-08-09, Reforger 1.7.0.54)
-------------------------------------------------------------
A save point is a directory:

    playthrough<NNN>/savepoint<NNN>/
        meta-info.json                     type, save time, playthrough, game version
        WorldState/<uuid>.blob             the entire world, one BlobBundler bundle

The blob filename is stable across every save point of every campaign
(`00e63bef-8b1d-8d00-8800-0f0a07a12329`) because it is a DETERMINISTIC id derived from the bundle
name "WorldState" — `PersistenceIdUtils.FromString`, recognisable by its leading zero byte and BI
subvariant 2. An overwrite therefore replaces that one file; save points never accumulate blobs.

Blob layout:

    u8   format version
    then, per collection, in WorldState-bundle order:
        u32 nameLen | name | u32 | u32 | u32 | u32(0) | <records>

Collections come from the bundle in `Common.conf` plus Overthrow's appended one, so the order is
System, Player, Character, Vehicle, Item, Turret, AIGroup, AIWaypoint, Structure, Storage, Misc,
Overthrow. A collection with no ROOT records has no section at all — `Item` is normally absent and
that is correct, because vanilla's `Item.conf` leaves `StorageRoot` false and `ParentHandling` at
ACCEPT, so an item in a container is only ever written as a nested child of its parent's record.

Inside the records:

  * Persistence ids are RAW 16 BYTES, BIG-ENDIAN, BYTE-ALIGNED. They are UUID v8 per
    `PersistenceIdUtils.Generate`: a 48-bit millisecond timestamp, version nibble 8, variant 10,
    BI subvariant 1. The timestamp is what lets `ids` print when each record was minted — which is
    how BUG-116 established that the colliding ids were a day old and arriving in bursts.
  * Prefab references are 8-byte resource GUIDs stored LITTLE-ENDIAN (i.e. reversed relative to
    how they are written in `.et` files and logs).
  * Overthrow's own scripted payloads — the game-mode record's manager serializers — are written
    into a BIT-PACKED sub-stream, so their strings (player persistent ids, prefab resource names,
    `OVT_PersistedJob`, `OVT_PersistedLoadoutItem`) appear at arbitrary BIT offsets. `strings` and
    `find` therefore scan all eight bit alignments. Engine records themselves are byte-aligned.

FALSE POSITIVES
---------------
Id detection is a structural scan, not a parse: any 16 bytes matching the v8 shape with a plausible
timestamp is counted. At byte alignment this is reliable (a random hit needs ~2^-40); at non-zero
bit alignments it is not, so `ids`/`summary` only count byte-aligned ids and `find` reports the
alignment it matched at so a shifted hit can be judged on its merits.

USAGE
-----
    tools/decode-savepoint.py summary  <path>...            # size, records, collections
    tools/decode-savepoint.py diff     <old> <new>          # per-collection deltas, added/removed
    tools/decode-savepoint.py ids      <path> [--since T]   # every id with its mint time
    tools/decode-savepoint.py find     <path> <uuid>        # locate an id, all bit alignments
    tools/decode-savepoint.py prefab   <path> <GUID>...     # count records per prefab GUID
    tools/decode-savepoint.py strings  <path> [--min N]     # readable text, all bit alignments

<path> is either a `.blob`, a `savepoint<NNN>` directory, or a `playthrough<NNN>` directory (in
which case every save point inside it is processed, oldest first).

EXIT CODES
----------
    0  ok
    1  bad usage, unreadable path, or (for `find`) id not present
"""

import argparse
import bisect
import collections
import datetime
import glob
import json
import os
import struct
import sys

UTC = datetime.timezone.utc

# Collections in WorldState-bundle order (Common.conf + Overthrow.conf's appended one).
COLLECTIONS = [
    "System", "Player", "Character", "Vehicle", "Item", "Turret",
    "AIGroup", "AIWaypoint", "Structure", "Storage", "Misc", "Overthrow",
]

# Timestamp sanity window for id detection. Widen if you are reading an old or a future save.
TS_LO = int(datetime.datetime(2024, 1, 1, tzinfo=UTC).timestamp() * 1000)
TS_HI = int(datetime.datetime(2030, 1, 1, tzinfo=UTC).timestamp() * 1000)


# ---------------------------------------------------------------------------- path resolution


def resolve(path):
    """Expand a blob / savepoint dir / playthrough dir into an ordered list of (label, blobpath)."""
    if os.path.isfile(path):
        return [(os.path.basename(os.path.dirname(os.path.dirname(path))) or path, path)]

    if not os.path.isdir(path):
        sys.exit("not a file or directory: %s" % path)

    blobs = sorted(glob.glob(os.path.join(path, "WorldState", "*.blob")))
    if blobs:
        return [(os.path.basename(path.rstrip("/")), blobs[0])]

    out = []
    for sp in sorted(glob.glob(os.path.join(path, "savepoint*"))):
        found = sorted(glob.glob(os.path.join(sp, "WorldState", "*.blob")))
        if found:
            out.append((os.path.basename(sp), found[0]))
    if not out:
        sys.exit("no WorldState blob under: %s" % path)
    return out


def meta_of(blobpath):
    """The save point's meta-info.json as a dict, or {} when it is missing."""
    meta = os.path.join(os.path.dirname(os.path.dirname(blobpath)), "meta-info.json")
    try:
        with open(meta) as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return {}


# ---------------------------------------------------------------------------- blob scanning


def bit_views(data):
    """The blob at all eight bit alignments, so bit-packed scripted payloads are searchable."""
    whole = int.from_bytes(data, "big")
    width = len(data) * 8
    return [((whole << shift) & ((1 << width) - 1)).to_bytes(len(data), "big")
            for shift in range(8)]


def is_id(chunk):
    """True when 16 bytes have the shape of a generated persistence id (UUID v8, BI subvariant)."""
    return (len(chunk) == 16
            and (chunk[6] & 0xF0) == 0x80              # version 8
            and (chunk[8] & 0xC0) == 0x80              # variant 10
            and ((chunk[8] >> 2) & 0x0F) in (1, 2)     # BI subvariant 1 (generated) or 2 (derived)
            and TS_LO <= int.from_bytes(chunk[:6], "big") <= TS_HI)


def sections(data):
    """[(offset, name)] for every collection section present, in file order."""
    found = []
    for name in COLLECTIONS:
        offset = data.find(struct.pack("<I", len(name)) + name.encode())
        if offset != -1:
            found.append((offset, name))
    return sorted(found)


def scan(blobpath):
    """Parse a blob into (data, sections, {id_hex: [offsets]}) using byte-aligned ids only."""
    with open(blobpath, "rb") as handle:
        data = handle.read()

    positions = collections.defaultdict(list)
    index = 0
    limit = len(data) - 16
    while index < limit:
        chunk = data[index:index + 16]
        if is_id(chunk):
            positions[chunk.hex()].append(index)
            index += 16
            continue
        index += 1

    return data, sections(data), positions


def fmt_uuid(hexstr):
    return "%s-%s-%s-%s-%s" % (hexstr[0:8], hexstr[8:12], hexstr[12:16], hexstr[16:20], hexstr[20:32])


def minted_at(hexstr):
    return datetime.datetime.fromtimestamp(int(hexstr[:12], 16) / 1000.0, UTC)


def section_of(secs, offset):
    starts = [start for start, _ in secs]
    slot = bisect.bisect_right(starts, offset) - 1
    if slot < 0:
        return "(header)"
    return secs[slot][1]


def per_section(secs, positions):
    counts = collections.Counter()
    for offsets in positions.values():
        for offset in offsets:
            counts[section_of(secs, offset)] += 1
    return counts


# ---------------------------------------------------------------------------- commands


def cmd_summary(args):
    for path in args.paths:
        for label, blobpath in resolve(path):
            data, secs, positions = scan(blobpath)
            meta = meta_of(blobpath)
            occurrences = sum(len(v) for v in positions.values())
            repeats = sum(1 for v in positions.values() if len(v) > 1)

            stamp = ""
            if meta.get("m_iSavedAtUnix"):
                stamp = datetime.datetime.fromtimestamp(meta["m_iSavedAtUnix"], UTC).strftime(" %Y-%m-%d %H:%M:%S UTC")
            kind = ""
            if meta.get("m_eType") is not None:
                kind = " type=%s" % meta["m_eType"]

            print("%s%s%s" % (label, stamp, kind))
            print("   bytes=%-9d records=%-6d occurrences=%-6d ids_seen_2plus=%d"
                  % (len(data), len(positions), occurrences, repeats))
            counts = per_section(secs, positions)
            print("   " + "  ".join("%s=%d" % (name, counts.get(name, 0)) for _, name in secs))


def cmd_diff(args):
    (_, old_blob), = resolve(args.old)[:1] or [(None, None)]
    (_, new_blob), = resolve(args.new)[:1] or [(None, None)]

    old_data, old_secs, old_pos = scan(old_blob)
    new_data, new_secs, new_pos = scan(new_blob)
    old_counts = per_section(old_secs, old_pos)
    new_counts = per_section(new_secs, new_pos)

    print("%-12s %>8s %8s %8s" .replace(">", "") % ("collection", "old", "new", "delta"))
    for name in COLLECTIONS:
        if old_counts.get(name) or new_counts.get(name):
            before, after = old_counts.get(name, 0), new_counts.get(name, 0)
            print("%-12s %8d %8d %+8d" % (name, before, after, after - before))

    added = set(new_pos) - set(old_pos)
    removed = set(old_pos) - set(new_pos)
    print("\nbytes   %d -> %d (%+d)" % (len(old_data), len(new_data), len(new_data) - len(old_data)))
    print("records %d -> %d (%+d)" % (len(old_pos), len(new_pos), len(new_pos) - len(old_pos)))
    print("added: %d   removed: %d" % (len(added), len(removed)))

    if added:
        buckets = collections.Counter(minted_at(u).strftime("%Y-%m-%d %H:%M") for u in added)
        print("\nmint times of ADDED records (newest 8 buckets):")
        for key in sorted(buckets)[-8:]:
            print("   %s  %d" % (key, buckets[key]))


def cmd_ids(args):
    for label, blobpath in resolve(args.path):
        _, secs, positions = scan(blobpath)
        rows = []
        for hexstr, offsets in positions.items():
            when = minted_at(hexstr)
            if args.since and when < datetime.datetime.fromisoformat(args.since).replace(tzinfo=UTC):
                continue
            rows.append((when, fmt_uuid(hexstr), section_of(secs, offsets[0]), len(offsets)))
        rows.sort()
        print("%s: %d ids" % (label, len(rows)))
        for when, uuid, sect, seen in rows:
            print("   %s  %s  %-10s x%d" % (when.strftime("%Y-%m-%d %H:%M:%S"), uuid, sect, seen))


def cmd_find(args):
    wanted = args.uuid.replace("-", "").lower()
    try:
        raw = bytes.fromhex(wanted)
    except ValueError:
        sys.exit("not a uuid: %s" % args.uuid)

    hits = 0
    for label, blobpath in resolve(args.path):
        with open(blobpath, "rb") as handle:
            data = handle.read()
        secs = sections(data)
        for shift, view in enumerate(bit_views(data)):
            start = view.find(raw)
            while start != -1:
                hits += 1
                where = section_of(secs, start)
                note = ""
                if shift:
                    note = "  (bit-shifted %d — verify before trusting)" % shift
                print("%s: offset 0x%x  section=%s%s" % (label, start, where, note))
                start = view.find(raw, start + 1)

    if not hits:
        print("%s not present" % args.uuid)
        return 1
    print("\n%d occurrence(s). More than one in the SAME section = a genuine duplicate record;\n"
          "cross-section pairs are normally legitimate references (AIGroup->Character, System->Vehicle)."
          % hits)
    return 0


def cmd_prefab(args):
    for label, blobpath in resolve(args.path):
        with open(blobpath, "rb") as handle:
            data = handle.read()
        secs = sections(data)
        print(label)
        for guid in args.guids:
            clean = guid.strip("{}")
            try:
                needle = bytes.fromhex(clean)[::-1]      # stored little-endian
            except ValueError:
                print("   %s  (not a 16-hex-digit GUID)" % guid)
                continue
            spots = []
            start = data.find(needle)
            while start != -1:
                spots.append(start)
                start = data.find(needle, start + 1)
            by_section = collections.Counter(section_of(secs, s) for s in spots)
            detail = ""
            if by_section:
                detail = "  " + " ".join("%s=%d" % kv for kv in sorted(by_section.items()))
            print("   {%s}  %d%s" % (clean, len(spots), detail))


def cmd_strings(args):
    import re
    pattern = re.compile(rb"[ -~]{%d,}" % args.min)
    for label, blobpath in resolve(args.path):
        with open(blobpath, "rb") as handle:
            data = handle.read()
        seen = set()
        print(label)
        for shift, view in enumerate(bit_views(data)):
            for match in pattern.finditer(view):
                text = match.group()
                if text in seen:
                    continue
                seen.add(text)
                print("   shift=%d 0x%-6x %s" % (shift, match.start(), text.decode("ascii", "replace")))


# ---------------------------------------------------------------------------- entry point


def main():
    parser = argparse.ArgumentParser(
        description=__doc__.split("USAGE")[0].strip(),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("summary", help="size, record count and collections")
    p.add_argument("paths", nargs="+")
    p.set_defaults(func=cmd_summary)

    p = sub.add_parser("diff", help="per-collection deltas between two save points")
    p.add_argument("old")
    p.add_argument("new")
    p.set_defaults(func=cmd_diff)

    p = sub.add_parser("ids", help="every record id with its mint time")
    p.add_argument("path")
    p.add_argument("--since", help="ISO datetime; only ids minted at or after it")
    p.set_defaults(func=cmd_ids)

    p = sub.add_parser("find", help="locate an id across all bit alignments")
    p.add_argument("path")
    p.add_argument("uuid")
    p.set_defaults(func=cmd_find)

    p = sub.add_parser("prefab", help="count records referencing prefab GUIDs")
    p.add_argument("path")
    p.add_argument("guids", nargs="+")
    p.set_defaults(func=cmd_prefab)

    p = sub.add_parser("strings", help="readable text, including bit-packed scripted payloads")
    p.add_argument("path")
    p.add_argument("--min", type=int, default=12)
    p.set_defaults(func=cmd_strings)

    args = parser.parse_args()
    sys.exit(args.func(args) or 0)


if __name__ == "__main__":
    main()
