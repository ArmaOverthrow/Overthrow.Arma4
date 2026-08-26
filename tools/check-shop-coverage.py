#!/usr/bin/env python3
"""Report base-game catalogue items that no Overthrow shop rule can ever stock.

WHY THIS EXISTS
---------------
Shop inventories are declarative catalogue QUERIES (OVT_ShopInventoryItem: type + mode + m_sFind),
so new vanilla items normally flow onto the shelves for free. They do not when Bohemia adds a new
SCR_EArsenalItemType (HANDWEAR, RADIO_BACKPACK) or files items under a mode the rules never ask for
(WEAPON_VARIANTS) - then the item is priced, registered, lootable and sellable-in-theory but no shop
ever lists it, and nobody notices until a player asks where the gloves are.

This mirrors the runtime exactly, without the Workbench:
  * universe   = the ITEM entity catalogue of every faction the Overthrow faction manager loads
                 (OVT_Faction.GetAllInventoryItems -> SCR_Faction.GetFactionEntityCatalogOfType(ITEM))
  * dropped    = disabled entries, SCR_NonArsenalItemCostCatalogData entries, and anything a
                 `hidden 1` OVT_PriceConfig matches (BuildResourceDatabase)
  * reachable  = matched by at least one OVT_ShopInventoryItem in ShopConfig.conf or
                 GunDealerConfig.conf under FindInventoryItems semantics (type equal; m_sFind substring;
                 mode DEFAULT = any; m_bIncludeSupportStationItems 0 drops SUPPORT_STATION)
Faction include flags and m_bSingleRandomItem are stocking concerns, not reachability, and are
ignored here - a shop that CAN list an item but chooses not to for the occupying faction is fine.

Usage:
    tools/check-shop-coverage.py [--reforger DIR] [--mode missing|summary|all]
                                 [--ignore PATTERN]... [--no-default-ignores] [--quiet]

Exit 0 = every non-ignored item is reachable; 1 = unreachable items found; 2 = could not read inputs.

The default ignore list names item families Overthrow deliberately keeps off civilian shelves
(crew-served/mortar parts, sandbags, barbed tape, mortar/heli/vehicle ammunition, ballistic tables,
the FIA tent vanilla mis-types as RADIO_BACKPACK). Revisit it when that design changes.
"""
import argparse
import collections
import os
import re
import sys

DEFAULT_IGNORES = [
    "/Tripods/", "/Mortars/", "/Sandbags/", "/BarbedTape/",
    "Ammo_Rocket_", "Ammo_Shell_", "BallisticTable", "Magazine_M242", "/Tents/",
]

FACTION_MANAGER = "Prefabs/GameMode/OVT_OverthrowFactionManager.et"
SHOP_CONF = "Configs/System/ShopConfig.conf"
DEALER_CONF = "Configs/System/GunDealerConfig.conf"
PRICE_CONF = "Configs/Pricing/itemPrices.conf"

# SCR_ArsenalItem attribute defaults ("2" for both): RIFLE and DEFAULT.
DEFAULT_TYPE = "RIFLE"
DEFAULT_MODE = "DEFAULT"


def parse_conf(path):
    """Minimal Enfusion text-conf reader -> tree of {cls, name, base, props, children}."""
    root = {"cls": "ROOT", "name": None, "base": None, "props": {}, "children": []}
    stack = [root]
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            s = line.strip()
            if not s:
                continue
            if s == "}":
                stack.pop()
                continue
            if s.endswith("{"):
                head = s[:-1].strip()
                m = re.match(r'^(\w+)\s*(?:"([^"]*)")?\s*(?::\s*"([^"]*)")?$', head)
                node = {"cls": m.group(1) if m else head, "name": m.group(2) if m else None,
                        "base": m.group(3) if m else None, "props": {}, "children": []}
                stack[-1]["children"].append(node)
                stack.append(node)
                continue
            m = re.match(r'^(\w+)\s+(.*)$', s)
            if m:
                stack[-1]["props"][m.group(1)] = m.group(2).strip('"')
    return root


def walk(node):
    yield node
    for child in node["children"]:
        yield from walk(child)


def strip_guid(res):
    return res.split("}", 1)[-1] if "}" in res else res


def resolve(rel, repo_root, game_root):
    """A resource path lives in the mod if the mod has it (same-GUID deltas inherit the rest)."""
    for root in (repo_root, game_root):
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            return p
    return None


def faction_item_catalogs(repo_root, game_root):
    """faction key -> ITEM catalogue path, following the faction manager's SCR_Faction confs."""
    out = {}
    fm = parse_conf(os.path.join(repo_root, FACTION_MANAGER))
    for n in walk(fm):
        if n["cls"] != "SCR_Faction" or not n["base"]:
            continue
        conf = resolve(strip_guid(n["base"]), repo_root, game_root)
        if not conf:
            continue
        key = None
        catalog = None
        # the mod's delta may omit FactionKey / the catalogue list; fall back to the vanilla file
        for path in (conf, resolve(strip_guid(n["base"]), game_root, game_root)):
            if not path:
                continue
            tree = parse_conf(path)
            if tree["children"]:
                key = key or tree["children"][0]["props"].get("FactionKey")
            for c in walk(tree):
                if c["cls"] == "SCR_EntityCatalogMultiList" and c["base"] and "InventoryItems" in c["base"]:
                    catalog = catalog or resolve(strip_guid(c["base"]), repo_root, game_root)
        if catalog:
            out[key or os.path.basename(conf)] = catalog
    return out


def load_items(catalogs):
    items, order = {}, []
    for fac, path in catalogs.items():
        for n in walk(parse_conf(path)):
            if n["cls"] not in ("SCR_EntityCatalogInventoryItem", "SCR_EntityCatalogEntry"):
                continue
            prefab = n["props"].get("m_sEntityPrefab", "")
            if not prefab:
                continue
            arsenal, non_arsenal = None, False
            for c in walk(n):
                if c is n or c["props"].get("m_bEnabled", "1") == "0":
                    continue
                if c["cls"] == "SCR_ArsenalItem" and arsenal is None:
                    arsenal = c["props"]
                if c["cls"] == "SCR_NonArsenalItemCostCatalogData":
                    non_arsenal = True
            if prefab in items:
                items[prefab]["factions"].append(fac)
                continue
            items[prefab] = {
                "prefab": prefab, "factions": [fac],
                "enabled": n["props"].get("m_bEnabled", "1") != "0",
                "non_arsenal": non_arsenal, "has_arsenal": arsenal is not None,
                "type": (arsenal or {}).get("m_eItemType", DEFAULT_TYPE),
                "mode": (arsenal or {}).get("m_eItemMode", DEFAULT_MODE),
            }
            order.append(prefab)
    return [items[p] for p in order]


def load_rules(path, cls, label_of):
    rules = []
    for cfg in walk(parse_conf(path)):
        if cfg["cls"] != cls:
            continue
        label = label_of(cfg)
        for n in walk(cfg):
            if n["cls"] != "OVT_ShopInventoryItem":
                continue
            p = n["props"]
            rules.append({
                "shop": label,
                "type": p.get("m_eItemType", DEFAULT_TYPE),
                "mode": p.get("m_eItemMode", DEFAULT_MODE),
                "find": p.get("m_sFind", ""),
                "support": p.get("m_bIncludeSupportStationItems", "1") != "0",
                "single": p.get("m_bSingleRandomItem", "0") == "1",
            })
    return rules


def load_prices(path):
    out = []
    for n in walk(parse_conf(path)):
        if n["cls"] == "OVT_PriceConfig":
            p = n["props"]
            out.append({"type": p.get("m_eItemType", DEFAULT_TYPE), "mode": p.get("m_eItemMode", DEFAULT_MODE),
                        "find": p.get("m_sFind", ""), "hidden": p.get("hidden", "0") == "1"})
    return out


def price_hidden(item, prices):
    for c in prices:
        if c["find"]:
            hit = c["find"] in item["prefab"]
        else:
            hit = item["has_arsenal"] and item["type"] == c["type"] and (c["mode"] == DEFAULT_MODE or item["mode"] == c["mode"])
        if hit and c["hidden"]:
            return True
    return False


def rule_matches(rule, item):
    if not item["has_arsenal"] or item["type"] != rule["type"]:
        return False
    if rule["find"] and rule["find"] not in item["prefab"]:
        return False
    if rule["mode"] != DEFAULT_MODE and item["mode"] != rule["mode"]:
        return False
    if not rule["support"] and item["mode"] == "SUPPORT_STATION":
        return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("--reforger", default=os.environ.get("OVERTHROW_REFORGER_DIR"), help="base-game reference tree (default: ../ArmaReforger, or $OVERTHROW_REFORGER_DIR)")
    ap.add_argument("--mode", choices=["missing", "summary", "all"], default="missing")
    ap.add_argument("--ignore", action="append", default=[], metavar="PATTERN", help="substring of a prefab path to treat as deliberately unsold (repeatable)")
    ap.add_argument("--no-default-ignores", action="store_true", help="do not apply the built-in design-exclusion list")
    ap.add_argument("--quiet", action="store_true", help="summary line only")
    args = ap.parse_args()

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    game_root = args.reforger or os.path.join(os.path.dirname(repo_root), "ArmaReforger")
    if not os.path.isdir(os.path.join(game_root, "Configs")):
        print("check-shop-coverage: pass --reforger DIR or set $OVERTHROW_REFORGER_DIR", file=sys.stderr)
        return 2
    try:
        catalogs = faction_item_catalogs(repo_root, game_root)
        if not catalogs:
            raise RuntimeError("no faction ITEM catalogues resolved from " + FACTION_MANAGER)
        items = load_items(catalogs)
        prices = load_prices(os.path.join(repo_root, PRICE_CONF))
        rules = load_rules(os.path.join(repo_root, SHOP_CONF), "OVT_ShopInventoryConfig", lambda c: c["props"].get("type", "SHOP_GENERAL"))
        rules += load_rules(os.path.join(repo_root, DEALER_CONF), "OVT_GunDealerConfig", lambda c: "GUN_DEALER")
    except (OSError, RuntimeError) as e:
        print(f"check-shop-coverage: {e}", file=sys.stderr)
        return 2

    ignores = ([] if args.no_default_ignores else list(DEFAULT_IGNORES)) + args.ignore
    rows = []
    for it in items:
        if not it["enabled"] or it["non_arsenal"] or price_hidden(it, prices):
            continue
        shops = sorted({r["shop"] + ("(rnd)" if r["single"] else "") for r in rules if rule_matches(r, it)})
        rows.append((it, shops))

    missing = [(it, any(p in it["prefab"] for p in ignores)) for it, shops in rows if not shops]
    unexpected = [it for it, ign in missing if not ign]
    ignored = [it for it, ign in missing if ign]

    if not args.quiet:
        if args.mode == "missing":
            grp = collections.defaultdict(list)
            for it in unexpected:
                grp[(it["type"], it["mode"])].append(it)
            for k in sorted(grp):
                print(f"\n## {k[0]} / {k[1]}  ({len(grp[k])} unreachable by any shop rule)")
                for it in grp[k]:
                    flag = "" if it["has_arsenal"] else "  (no SCR_ArsenalItem)"
                    print(f"  [{'/'.join(it['factions'])}] {strip_guid(it['prefab'])}{flag}")
        elif args.mode == "summary":
            c = collections.Counter((it["type"], it["mode"], "reachable" if shops else "MISSING") for it, shops in rows)
            for k in sorted(c):
                print(f"{k[0]:22} {k[1]:16} {k[2]:9} {c[k]}")
        else:
            for it, shops in rows:
                print(f"{it['type']:22} {it['mode']:16} {','.join(shops) or '-':28} {'/'.join(it['factions']):16} {strip_guid(it['prefab'])}")
        print()
    print(f"check-shop-coverage: {len(rows)} sellable catalogue items, {len(rows) - len(missing)} reachable, "
          f"{len(unexpected)} unreachable, {len(ignored)} unreachable-but-ignored ({len(catalogs)} factions: {', '.join(sorted(catalogs))})",
          file=sys.stderr)
    return 1 if unexpected else 0


if __name__ == "__main__":
    sys.exit(main())
