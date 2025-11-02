#!/usr/bin/env python3

import json
import os
from pathlib import Path

def analyze_detailed_changes():
    """Analyze the comparison results in detail"""
    
    with open('version_comparison_results.json', 'r') as f:
        data = json.load(f)
    
    print("="*60)
    print("DETAILED CHANGE ANALYSIS")
    print("="*60)
    
    # Find new AI system files
    print("\n### NEW AI SYSTEM FILES ###")
    ai_files = [f for f in data['new_files'] if 'AI/' in f or '/AI/' in f]
    for f in ai_files[:20]:
        print(f"  - {f}")
    print(f"  ... and {len(ai_files) - 20} more AI files") if len(ai_files) > 20 else None
    
    # Find new configs
    print("\n### NEW CONFIG SYSTEMS ###")
    config_files = [f for f in data['new_files'] if 'Configs/' in f]
    config_categories = {}
    for f in config_files:
        parts = f.split('/')
        if len(parts) > 1:
            category = parts[1] if parts[0] == 'Configs' else parts[0]
            if category not in config_categories:
                config_categories[category] = []
            config_categories[category].append(f)
    
    for cat, files in sorted(config_categories.items())[:10]:
        print(f"\n  {cat}: {len(files)} files")
        for f in files[:3]:
            print(f"    - {f}")
    
    # Find new script classes/components
    print("\n### NEW SCRIPT COMPONENTS ###")
    script_files = data['key_changes']['scripts']['new_files']
    component_files = [f for f in script_files if 'Component' in f]
    for f in component_files[:15]:
        print(f"  - {f}")
    
    # Find modified important scripts
    print("\n### MODIFIED CORE SCRIPTS ###")
    modified_scripts = data['key_changes']['scripts']['modified_files']
    important_patterns = ['GameMode', 'Manager', 'Controller', 'System', 'Network', 'Player']
    important_modified = []
    for f in modified_scripts:
        if any(p in f for p in important_patterns):
            important_modified.append(f)
    
    for f in important_modified[:20]:
        print(f"  - {f}")
    
    # Texture analysis
    print("\n### NEW TEXTURE/ASSET FILES ###")
    print(f"  Total new textures: {data['key_changes']['textures']['new']}")
    texture_dirs = {}
    for f in data['new_files']:
        if '.edds' in f:
            dir_path = '/'.join(f.split('/')[:-1])
            texture_dirs[dir_path] = texture_dirs.get(dir_path, 0) + 1
    
    sorted_dirs = sorted(texture_dirs.items(), key=lambda x: x[1], reverse=True)
    for dir_path, count in sorted_dirs[:10]:
        print(f"  {dir_path}: {count} textures")
    
    # New prefabs
    print("\n### NEW PREFABS ###")
    prefab_files = [f for f in data['new_files'] if f.endswith('.et')]
    prefab_categories = {}
    for f in prefab_files:
        parts = f.split('/')
        if len(parts) > 1:
            category = parts[1] if parts[0] == 'Prefabs' else parts[0]
            if category not in prefab_categories:
                prefab_categories[category] = []
            prefab_categories[category].append(f)
    
    for cat, files in sorted(prefab_categories.items())[:10]:
        if len(files) > 0:
            print(f"\n  {cat}: {len(files)} prefabs")
            for f in files[:3]:
                print(f"    - {f.split('/')[-1]}")

if __name__ == "__main__":
    analyze_detailed_changes()