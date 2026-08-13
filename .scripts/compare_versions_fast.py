#!/usr/bin/env python3
"""Compare two extracted Arma Reforger reference trees.

Modes:
  previous      (default) newest archive in ArmaReforger.versions  ->  current ArmaReforger
  experimental  current ArmaReforger  ->  ArmaReforgerExperimental
  explicit      --old PATH --new PATH

Extraction (update-arma-scripts.ps1) stamps each tree with a .version.json
marker ({buildId, label, extracted}); labels are read from there when present.

Results JSON keeps the keys analyze_changes.py consumes:
new_files / deleted_files / modified_files / key_changes.
"""

import argparse
import os
import hashlib
import json
from pathlib import Path
from typing import Dict, Optional
import time
import sys

ARMA_ROOT = Path("/mnt/n/Projects/Arma 4")
CURRENT_PATH = ARMA_ROOT / "ArmaReforger"
EXPERIMENTAL_PATH = ARMA_ROOT / "ArmaReforgerExperimental"
VERSIONS_PATH = ARMA_ROOT / "ArmaReforger.versions"
MARKER_NAME = ".version.json"
REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / ".tmp" / "reforger-compare" / "version_comparison_results.json"


def read_marker(tree: Path) -> Optional[dict]:
    """Read a tree's .version.json marker, if present"""
    marker = tree / MARKER_NAME
    if marker.is_file():
        try:
            # utf-8-sig: PowerShell Set-Content -Encoding UTF8 writes a BOM
            return json.loads(marker.read_text(encoding="utf-8-sig"))
        except (json.JSONDecodeError, OSError):
            return None
    return None


def tree_label(tree: Path) -> str:
    """Human-readable label for a tree: marker label > marker buildId > dir name"""
    marker = read_marker(tree)
    if marker:
        if marker.get("label"):
            return marker["label"]
        if marker.get("buildId"):
            return f"build-{marker['buildId']}"
    return tree.name


def find_latest_archive() -> Optional[Path]:
    """Newest archived version tree, by marker 'extracted' date, else dir mtime"""
    if not VERSIONS_PATH.is_dir():
        return None
    candidates = [d for d in VERSIONS_PATH.iterdir() if d.is_dir()]
    if not candidates:
        return None

    def sort_key(d: Path):
        marker = read_marker(d)
        if marker and marker.get("extracted"):
            return (1, marker["extracted"])
        return (0, str(d.stat().st_mtime))

    return max(candidates, key=sort_key)


class ArmaVersionComparer:
    def __init__(self, old_path: Path, new_path: Path):
        self.old_path = Path(old_path)
        self.new_path = Path(new_path)
        self.results = {
            'comparison': {
                'old_path': str(self.old_path),
                'new_path': str(self.new_path),
                'old_label': tree_label(self.old_path),
                'new_label': tree_label(self.new_path),
            },
            'new_files': [],
            'deleted_files': [],
            'modified_files': [],
            'statistics': {},
            'key_changes': {}
        }

    def get_file_info(self, filepath: Path) -> tuple:
        """Get file size and modification time instead of hash for faster comparison"""
        try:
            stat = filepath.stat()
            return (stat.st_size, stat.st_mtime)
        except:
            return None

    def get_file_hash(self, filepath: Path) -> str:
        """Get MD5 hash of a file - only for files that differ in size/mtime"""
        try:
            with open(filepath, 'rb') as f:
                # Read in chunks for large files
                hash_md5 = hashlib.md5()
                for chunk in iter(lambda: f.read(4096), b""):
                    hash_md5.update(chunk)
                return hash_md5.hexdigest()
        except:
            return None

    def get_all_files(self, base_path: Path) -> Dict[str, Path]:
        """Get all files in a directory recursively with progress"""
        files = {}
        count = 0
        for root, dirs, filenames in os.walk(base_path):
            for filename in filenames:
                filepath = Path(root) / filename
                relative_path = filepath.relative_to(base_path)
                if str(relative_path) == MARKER_NAME:
                    continue  # extraction metadata, not game content
                files[str(relative_path)] = filepath
                count += 1
                if count % 1000 == 0:
                    print(f"    Scanned {count} files...", end='\r')
        print(f"    Total: {count} files found        ")
        return files

    def compare_directories(self):
        """Compare directory structures using size/mtime first, then hash for changes"""
        print(f"\nScanning old version ({self.results['comparison']['old_label']})...")
        old_files = self.get_all_files(self.old_path)

        print(f"\nScanning new version ({self.results['comparison']['new_label']})...")
        new_files = self.get_all_files(self.new_path)

        old_keys = set(old_files.keys())
        new_keys = set(new_files.keys())

        # Find new and deleted files
        self.results['new_files'] = sorted(list(new_keys - old_keys))
        self.results['deleted_files'] = sorted(list(old_keys - new_keys))

        # Find modified files - use size/mtime for quick check first
        common_files = old_keys & new_keys
        modified = []
        potentially_modified = []

        print(f"\nQuick scan of {len(common_files)} common files...")
        for i, rel_path in enumerate(common_files):
            if i % 1000 == 0:
                print(f"  Progress: {i}/{len(common_files)}", end='\r')

            old_info = self.get_file_info(old_files[rel_path])
            new_info = self.get_file_info(new_files[rel_path])

            if old_info and new_info:
                # If size differs, file is definitely modified
                if old_info[0] != new_info[0]:
                    modified.append(rel_path)
                # If only mtime differs, need to check content
                elif abs(old_info[1] - new_info[1]) > 1:
                    potentially_modified.append(rel_path)

        print(f"\n  Found {len(modified)} files with size changes")
        print(f"  Found {len(potentially_modified)} files needing content check")

        # Hash check for potentially modified files
        additional_modified = 0
        if potentially_modified:
            print(f"\nChecking content of {len(potentially_modified)} files...")
            for i, rel_path in enumerate(potentially_modified):
                if i % 100 == 0:
                    print(f"  Progress: {i}/{len(potentially_modified)}", end='\r')

                old_hash = self.get_file_hash(old_files[rel_path])
                new_hash = self.get_file_hash(new_files[rel_path])

                if old_hash and new_hash and old_hash != new_hash:
                    modified.append(rel_path)
                    additional_modified += 1
            print(f"  Found {additional_modified} additional modified files through content check")

        self.results['modified_files'] = sorted(modified)

        # Calculate statistics
        self.results['statistics'] = {
            'total_old_files': len(old_files),
            'total_new_files': len(new_files),
            'new_files': len(self.results['new_files']),
            'deleted_files': len(self.results['deleted_files']),
            'modified_files': len(self.results['modified_files']),
            'unchanged_files': len(common_files) - len(modified)
        }

    def analyze_key_changes(self):
        """Analyze changes in key system files"""
        categories = {
            'scripts': ['.c'],
            'configs': ['.conf', '.json'],
            'prefabs': ['.et'],
            'layouts': ['.layout'],
            'particles': ['.ptc'],
            'materials': ['.emat'],
            'models': ['.xob'],
            'animations': ['.anm'],
            'sounds': ['.acp'],
            'textures': ['.edds']
        }

        for category, extensions in categories.items():
            new = [f for f in self.results['new_files']
                   if any(f.endswith(ext) for ext in extensions)]
            modified = [f for f in self.results['modified_files']
                       if any(f.endswith(ext) for ext in extensions)]
            deleted = [f for f in self.results['deleted_files']
                     if any(f.endswith(ext) for ext in extensions)]

            self.results['key_changes'][category] = {
                'new': len(new),
                'modified': len(modified),
                'deleted': len(deleted),
                'new_files': new[:30],  # Top 30 for analysis
                'modified_files': modified[:30],
                'deleted_files': deleted[:30]
            }

    def find_important_paths(self):
        """Identify important new systems and features based on paths"""
        important_paths = {}

        # Check for new major systems/folders
        new_dirs = set()
        for f in self.results['new_files']:
            parts = f.split('/')
            if len(parts) > 1:
                # Track top-level and second-level directories
                new_dirs.add(parts[0])
                if len(parts) > 2:
                    new_dirs.add(f"{parts[0]}/{parts[1]}")

        self.results['new_directories'] = sorted(list(new_dirs))

        # Find specific important patterns
        patterns = {
            'vehicle_systems': 'Scripts/Game/Vehicle',
            'weapon_systems': 'Scripts/Game/Weapon',
            'ai_systems': 'Scripts/Game/AI',
            'ui_systems': 'Scripts/Game/UI',
            'network_systems': 'Scripts/Game/Network',
            'audio_systems': 'Scripts/Game/Audio',
            'inventory_systems': 'Scripts/Game/Inventory',
            'medical_systems': 'Scripts/Game/Medical',
            'building_systems': 'Scripts/Game/Building',
            'faction_systems': 'Scripts/Game/Faction'
        }

        for name, pattern in patterns.items():
            new = [f for f in self.results['new_files'] if pattern in f]
            modified = [f for f in self.results['modified_files'] if pattern in f]
            if new or modified:
                important_paths[name] = {
                    'new_count': len(new),
                    'modified_count': len(modified),
                    'sample_files': (new[:5] + modified[:5])[:5]
                }

        self.results['important_system_changes'] = important_paths

    def save_results(self, output_file: str):
        """Save results to JSON file"""
        output_path = Path(output_file)
        if not output_path.is_absolute():
            output_path = Path.cwd() / output_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w') as f:
            json.dump(self.results, f, indent=2)

    def generate_summary(self):
        """Generate a summary of changes"""
        print("\n" + "="*60)
        print("COMPARISON SUMMARY")
        print("="*60)

        comp = self.results['comparison']
        print(f"\n  Old: {comp['old_label']}  ({comp['old_path']})")
        print(f"  New: {comp['new_label']}  ({comp['new_path']})")

        stats = self.results['statistics']
        print(f"\nFile Statistics:")
        print(f"  Old version: {stats['total_old_files']:,} files")
        print(f"  New version: {stats['total_new_files']:,} files")
        print(f"  New files: {stats['new_files']:,}")
        print(f"  Deleted files: {stats['deleted_files']:,}")
        print(f"  Modified files: {stats['modified_files']:,}")
        print(f"  Unchanged files: {stats['unchanged_files']:,}")

        print(f"\nKey Changes by Category:")
        for category, changes in self.results['key_changes'].items():
            total = changes['new'] + changes['modified'] + changes['deleted']
            if total > 0:
                print(f"\n  {category.upper()}:")
                if changes['new'] > 0:
                    print(f"    New: {changes['new']}")
                if changes['modified'] > 0:
                    print(f"    Modified: {changes['modified']}")
                if changes['deleted'] > 0:
                    print(f"    Deleted: {changes['deleted']}")

        if self.results.get('new_directories'):
            print(f"\nNew Major Directories:")
            for d in self.results['new_directories'][:10]:
                if '/' not in d:  # Top-level only
                    print(f"  - {d}")

        if self.results.get('important_system_changes'):
            print(f"\nImportant System Changes Detected:")
            for system, info in self.results['important_system_changes'].items():
                if info['new_count'] > 0 or info['modified_count'] > 5:
                    print(f"  - {system.replace('_', ' ').title()}: {info['new_count']} new, {info['modified_count']} modified")


def resolve_paths(args) -> tuple:
    """Work out (old_path, new_path) from the CLI args"""
    if args.old or args.new:
        if not (args.old and args.new):
            sys.exit("ERROR: --old and --new must be given together")
        return Path(args.old), Path(args.new)

    if args.mode == 'experimental':
        if not EXPERIMENTAL_PATH.is_dir():
            sys.exit(f"ERROR: Experimental tree not found at {EXPERIMENTAL_PATH}")
        return CURRENT_PATH, EXPERIMENTAL_PATH

    # previous (default): newest archive -> current tree
    latest = find_latest_archive()
    if not latest:
        sys.exit(f"ERROR: No archived versions found in {VERSIONS_PATH}\n"
                 f"Archives are created by update-arma-scripts.ps1 when a new game build replaces the tree.\n"
                 f"Use 'experimental' mode or --old/--new for other comparisons.")
    return latest, CURRENT_PATH


def main():
    parser = argparse.ArgumentParser(description="Compare two extracted Arma Reforger reference trees")
    parser.add_argument('mode', nargs='?', default='previous', choices=['previous', 'experimental'],
                        help="previous: latest archive vs current (default); experimental: current vs experimental")
    parser.add_argument('--old', help="Explicit old tree path (overrides mode; requires --new)")
    parser.add_argument('--new', help="Explicit new tree path (overrides mode; requires --old)")
    parser.add_argument('--output', default=str(DEFAULT_OUTPUT),
                        help=f"Results JSON path (default: {DEFAULT_OUTPUT})")
    args = parser.parse_args()

    old_path, new_path = resolve_paths(args)
    for p in (old_path, new_path):
        if not p.is_dir():
            sys.exit(f"ERROR: Not a directory: {p}")

    start_time = time.time()

    comparer = ArmaVersionComparer(old_path, new_path)

    print("Starting fast version comparison...")
    print("="*60)

    comparer.compare_directories()

    print("\nAnalyzing key changes...")
    comparer.analyze_key_changes()

    print("\nFinding important system changes...")
    comparer.find_important_paths()

    comparer.save_results(args.output)
    print(f"\nDetailed results saved to {args.output}")

    comparer.generate_summary()

    elapsed = time.time() - start_time
    print(f"\nComparison completed in {elapsed:.1f} seconds")


if __name__ == "__main__":
    main()
