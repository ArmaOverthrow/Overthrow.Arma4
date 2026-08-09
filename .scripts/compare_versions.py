#!/usr/bin/env python3

import os
import hashlib
import json
from pathlib import Path
from typing import Dict, List, Set, Tuple
import difflib
import re

class ArmaVersionComparer:
    def __init__(self, stable_path: str, experimental_path: str):
        self.stable_path = Path(stable_path)
        self.experimental_path = Path(experimental_path)
        self.results = {
            'new_files': [],
            'deleted_files': [],
            'modified_files': [],
            'new_directories': [],
            'deleted_directories': [],
            'statistics': {},
            'key_changes': {}
        }
        
    def get_file_hash(self, filepath: Path) -> str:
        """Get MD5 hash of a file"""
        try:
            with open(filepath, 'rb') as f:
                return hashlib.md5(f.read()).hexdigest()
        except:
            return None
            
    def get_all_files(self, base_path: Path) -> Dict[str, Path]:
        """Get all files in a directory recursively"""
        files = {}
        for root, dirs, filenames in os.walk(base_path):
            for filename in filenames:
                filepath = Path(root) / filename
                relative_path = filepath.relative_to(base_path)
                files[str(relative_path)] = filepath
        return files
        
    def compare_directories(self):
        """Compare directory structures"""
        print("Scanning stable version...")
        stable_files = self.get_all_files(self.stable_path)
        print(f"Found {len(stable_files)} files in stable version")
        
        print("Scanning experimental version...")
        experimental_files = self.get_all_files(self.experimental_path)
        print(f"Found {len(experimental_files)} files in experimental version")
        
        stable_keys = set(stable_files.keys())
        experimental_keys = set(experimental_files.keys())
        
        # Find new and deleted files
        self.results['new_files'] = sorted(list(experimental_keys - stable_keys))
        self.results['deleted_files'] = sorted(list(stable_keys - experimental_keys))
        
        # Find modified files
        common_files = stable_keys & experimental_keys
        modified = []
        
        print("Comparing common files...")
        for i, rel_path in enumerate(common_files):
            if i % 1000 == 0:
                print(f"  Progress: {i}/{len(common_files)}")
                
            stable_hash = self.get_file_hash(stable_files[rel_path])
            exp_hash = self.get_file_hash(experimental_files[rel_path])
            
            if stable_hash and exp_hash and stable_hash != exp_hash:
                modified.append(rel_path)
                
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
            'models': ['.xob']
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
                'new_files': new[:20],  # Top 20 for brevity
                'modified_files': modified[:20],
                'deleted_files': deleted[:20]
            }
            
    def find_important_script_changes(self):
        """Find important changes in script files"""
        important_patterns = [
            r'class\s+(\w+)',  # New classes
            r'proto\s+(\w+)',  # New proto methods
            r'ScriptInvoker\s+(\w+)',  # New events
            r'RplRpc',  # RPC changes
            r'Attribute\(',  # New attributes
        ]
        
        important_changes = []
        
        for file in self.results['modified_files'][:100]:  # Check first 100 modified scripts
            if not file.endswith('.c'):
                continue
                
            stable_file = self.stable_path / file
            exp_file = self.experimental_path / file
            
            try:
                with open(stable_file, 'r', encoding='utf-8', errors='ignore') as f:
                    stable_content = f.read()
                with open(exp_file, 'r', encoding='utf-8', errors='ignore') as f:
                    exp_content = f.read()
                    
                for pattern in important_patterns:
                    stable_matches = set(re.findall(pattern, stable_content))
                    exp_matches = set(re.findall(pattern, exp_content))
                    
                    new_matches = exp_matches - stable_matches
                    if new_matches:
                        important_changes.append({
                            'file': file,
                            'pattern': pattern,
                            'new_items': list(new_matches)[:10]
                        })
            except:
                pass
                
        self.results['important_script_changes'] = important_changes
        
    def save_results(self, output_file: str):
        """Save results to JSON file"""
        with open(output_file, 'w') as f:
            json.dump(self.results, f, indent=2)
            
    def generate_summary(self):
        """Generate a summary of changes"""
        print("\n" + "="*60)
        print("COMPARISON SUMMARY")
        print("="*60)
        
        stats = self.results['statistics']
        print(f"\nFile Statistics:")
        print(f"  Stable version: {stats['total_stable_files']} files")
        print(f"  Experimental version: {stats['total_experimental_files']} files")
        print(f"  New files: {stats['new_files']}")
        print(f"  Deleted files: {stats['deleted_files']}")
        print(f"  Modified files: {stats['modified_files']}")
        print(f"  Unchanged files: {stats['unchanged_files']}")
        
        print(f"\nKey Changes by Category:")
        for category, changes in self.results['key_changes'].items():
            if changes['new'] > 0 or changes['modified'] > 0:
                print(f"\n  {category.upper()}:")
                print(f"    New: {changes['new']}")
                print(f"    Modified: {changes['modified']}")
                print(f"    Deleted: {changes['deleted']}")
                
                if changes['new'] > 0 and changes['new_files']:
                    print(f"    Sample new files:")
                    for f in changes['new_files'][:5]:
                        print(f"      - {f}")

def main():
    stable_path = "/mnt/n/Projects/Arma 4/ArmaReforger"
    experimental_path = "/mnt/n/Projects/Arma 4/ArmaReforgerExperimental"
    
    comparer = ArmaVersionComparer(stable_path, experimental_path)
    
    print("Starting version comparison...")
    comparer.compare_directories()
    
    print("\nAnalyzing key changes...")
    comparer.analyze_key_changes()
    
    print("\nFinding important script changes...")
    comparer.find_important_script_changes()
    
    output_file = ".scripts/version_comparison_results.json"
    comparer.save_results(output_file)
    print(f"\nDetailed results saved to {output_file}")
    
    comparer.generate_summary()

if __name__ == "__main__":
    main()