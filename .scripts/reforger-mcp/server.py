#!./.venv/bin/python3
"""
Arma Reforger Class Information MCP Server
Provides tools to search and retrieve class information from the scraped data.
"""

import json
import os
from typing import List, Dict, Any
from mcp.server.fastmcp import FastMCP

# Create an MCP server
mcp = FastMCP("Arma Reforger Classes")

# Global variable to store the class data
class_data: Dict[str, Any] = {}

def load_class_data():
    """Load the class data from the JSON file"""
    global class_data
    json_file = "reforger_classes.json"
    
    if os.path.exists(json_file):
        try:
            with open(json_file, 'r', encoding='utf-8') as f:
                class_data = json.load(f)
            #print(f"Loaded {len(class_data)} classes from {json_file}")
        except Exception as e:
            print(f"Error loading {json_file}: {e}")
            quit()
            class_data = {}
    else:
        print(f"Class data file {json_file} not found")
        quit()
        class_data = {}

@mcp.tool()
def search_reforger_classes(query: str) -> List[str]:
    """
    Search for Arma Reforger class names that match the query.
    
    Args:
        query: Search term to match against class names (case-insensitive)
    
    Returns:
        List of class names that contain the query string
    """
    if not class_data:
        load_class_data()
    
    if not query:
        return []
    
    query_lower = query.lower()
    matching_classes = []
    
    for class_name in class_data.keys():
        if query_lower in class_name.lower():
            matching_classes.append(class_name)
    
    # Sort the results for consistent output
    matching_classes.sort()
    
    # Limit results to avoid overwhelming output
    if len(matching_classes) > 50:
        matching_classes = matching_classes[:50]
        matching_classes.append(f"... and {len(matching_classes) - 50} more matches")
    
    return matching_classes

@mcp.tool()
def get_reforger_class(classname: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific Arma Reforger class.
    
    Args:
        classname: The exact name of the class to retrieve
    
    Returns:
        Dictionary containing class information including public methods
    """
    if not class_data:
        load_class_data()
    
    if classname not in class_data:
        return {
            "error": f"Class '{classname}' not found",
            "available_classes": len(class_data),
            "suggestion": "Use search_reforger_classes() to find available classes"
        }
    
    class_info = class_data[classname]
    
    result = {
        "class_name": classname,
        "processed": class_info.get("done", False),
        "public_methods": class_info.get("publicMethods", []),
        "method_count": len(class_info.get("publicMethods", []))
    }
    
    if not result["processed"]:
        result["note"] = "This class has not been fully processed yet. Method information may be incomplete."
    
    return result

@mcp.tool()
def get_reforger_stats() -> Dict[str, Any]:
    """
    Get statistics about the Arma Reforger class database.
    
    Returns:
        Dictionary containing database statistics
    """
    if not class_data:
        load_class_data()
    
    total_classes = len(class_data)
    processed_classes = sum(1 for info in class_data.values() if info.get("done", False))
    classes_with_methods = sum(1 for info in class_data.values() if info.get("publicMethods"))
    total_methods = sum(len(info.get("publicMethods", [])) for info in class_data.values())
    
    # Get some sample classes with methods
    sample_classes = []
    for name, info in list(class_data.items())[:10]:
        if info.get("publicMethods"):
            sample_classes.append({
                "name": name,
                "method_count": len(info["publicMethods"])
            })
    
    return {
        "total_classes": total_classes,
        "processed_classes": processed_classes,
        "classes_with_methods": classes_with_methods,
        "total_methods": total_methods,
        "processing_progress": f"{processed_classes}/{total_classes} ({processed_classes/total_classes*100:.1f}%)" if total_classes > 0 else "0%",
        "sample_classes": sample_classes[:5]  # Show first 5 samples
    }

@mcp.resource("class://classes")
def get_class_names() -> list[str]:
    """
    Get Arma Reforger class names as a resource.
    """
    if not class_data:
        load_class_data()
    return list(class_data.keys())

@mcp.resource("class://{class_name}")
def get_class_resource(class_name: str) -> str:
    """
    Get Arma Reforger class information as a resource.
    
    Args:
        class_name: The name of the class to retrieve
        
    Returns:
        Formatted string containing class information and methods
    """
    if not class_data:
        load_class_data()
    
    if class_name not in class_data:
        return f"""Class '{class_name}' not found.

Available classes: {len(class_data)}
Use search_reforger_classes() tool to find available classes."""

    class_info = class_data[class_name]
    methods = class_info.get("publicMethods", [])
    processed = class_info.get("done", False)
    
    result = f"""# {class_name}

**Status**: {'✅ Processed' if processed else '⏳ Not fully processed'}
**Method Count**: {len(methods)}

## Public Methods

"""
    
    if methods:
        for i, method in enumerate(methods, 1):
            result += f"{i}. `{method}`\n"
    else:
        if not processed:
            result += "_This class has not been fully processed yet. Method information may be incomplete._"
        else:
            result += "_No public methods found for this class._"
    
    return result


# Load data when the server starts
load_class_data()

if __name__ == "__main__":
    # Run the server
    mcp.run() 