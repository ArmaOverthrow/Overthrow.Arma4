#!/usr/bin/env python3

import os
import hashlib
import json
from pathlib import Path
from typing import Dict
import time
import sys

class ArmaVersionComparer:
    def __init__(self, stable_path: str, experimental_path: str):
        self.stable_path = Path(stable_path)
        self.experimental_path = Path(experimental_path)
        self.results = {
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
                files[str(relative_path)] = filepath
                count += 1
                if count % 1000 == 0:
                    print(f"    Scanned {count} files...", end='\r')
        print(f"    Total: {count} files found        ")
        return files
        
    def compare_directories(self):
        """Compare directory structures using size/mtime first, then hash for changes"""
        print("\nScanning stable version...")
        stable_files = self.get_all_files(self.stable_path)
        
        print("\nScanning experimental version...")
        experimental_files = self.get_all_files(self.experimental_path)
        
        stable_keys = set(stable_files.keys())
        experimental_keys = set(experimental_files.keys())
        
        # Find new and deleted files
        self.results['new_files'] = sorted(list(experimental_keys - stable_keys))
        self.results['deleted_files'] = sorted(list(stable_keys - experimental_keys))
        
        # Find modified files - use size/mtime for quick check first
        common_files = stable_keys & experimental_keys
        modified = []
        potentially_modified = []
        
        print(f"\nQuick scan of {len(common_files)} common files...")
        for i, rel_path in enumerate(common_files):
            if i % 1000 == 0:
                print(f"  Progress: {i}/{len(common_files)}", end='\r')
                
            stable_info = self.get_file_info(stable_files[rel_path])
            exp_info = self.get_file_info(experimental_files[rel_path])
            
            if stable_info and exp_info:
                # If size differs, file is definitely modified
                if stable_info[0] != exp_info[0]:
                    modified.append(rel_path)
                # If only mtime differs, need to check content
                elif abs(stable_info[1] - exp_info[1]) > 1:
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
                    
                stable_hash = self.get_file_hash(stable_files[rel_path])
                exp_hash = self.get_file_hash(experimental_files[rel_path])
                
                if stable_hash and exp_hash and stable_hash != exp_hash:
                    modified.append(rel_path)
                    additional_modified += 1
            print(f"  Found {additional_modified} additional modified files through content check")
                
        self.results['modified_files'] = sorted(modified)
        
        # Calculate statistics
        self.results['statistics'] = {
            'total_stable_files': len(stable_files),
            'total_experimental_files': len(experimental_files),
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
        with open(output_path, 'w') as f:
            json.dump(self.results, f, indent=2)
            
    def generate_summary(self):
        """Generate a summary of changes"""
        print("\n" + "="*60)
        print("COMPARISON SUMMARY")
        print("="*60)
        
        stats = self.results['statistics']
        print(f"\nFile Statistics:")
        print(f"  Stable version: {stats['total_stable_files']:,} files")
        print(f"  Experimental version: {stats['total_experimental_files']:,} files")
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

def main():
    stable_path = "/mnt/n/Projects/Arma 4/ArmaReforger"
    experimental_path = "/mnt/n/Projects/Arma 4/ArmaReforgerExperimental"
    
    start_time = time.time()
    
    comparer = ArmaVersionComparer(stable_path, experimental_path)
    
    print("Starting fast version comparison...")
    print("="*60)
    
    comparer.compare_directories()
    
    print("\nAnalyzing key changes...")
    comparer.analyze_key_changes()
    
    print("\nFinding important system changes...")
    comparer.find_important_paths()
    
    output_file = "version_comparison_results.json"
    comparer.save_results(output_file)
    print(f"\nDetailed results saved to {output_file}")
    
    comparer.generate_summary()
    
    elapsed = time.time() - start_time
    print(f"\nComparison completed in {elapsed:.1f} seconds")

if __name__ == "__main__":
    main()