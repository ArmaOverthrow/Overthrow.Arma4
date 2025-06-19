#!./.scripts/reforger-mcp/.venv/bin/python3
"""
Script to extract class names from Arma Reforger Script API documentation.
Scrapes the annotated.html page and saves class names to a JSON file.
Then incrementally fetches each class's interface page to extract public methods.
"""

import requests
from bs4 import BeautifulSoup
import json
import os
import sys
from urllib.parse import urljoin
import time
import re

def fetch_class_names(url):
    """
    Fetch class names from the Arma Reforger Script API documentation.
    
    Args:
        url (str): URL to the annotated.html page
        
    Returns:
        dict: Dictionary with class names as keys and empty dict as values
    """
    #print(f"Fetching data from: {url}")
    
    try:
        # Add headers to mimic a real browser request
        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
        }
        
        response = requests.get(url, headers=headers, timeout=30)
        response.raise_for_status()
        
        #print("Successfully fetched the page. Parsing HTML...")
        
        soup = BeautifulSoup(response.content, 'html.parser')
        
        class_names = {}
        
        # Look for the directory table that contains the class list
        # The structure is: <table class="directory"> with rows containing class links
        directory_table = soup.find('table', class_='directory')
        
        if directory_table and directory_table.name == 'table':
            #print("Found directory table, extracting class names...")
            
            # Find all anchor tags with class "el" (element links)
            class_links = directory_table.find_all('a', class_='el')
            
            for link in class_links:
                href = link.get('href', '')
                class_name = link.get_text().strip()
                
                # Verify this is a class interface link and get clean class name
                if (href.startswith('interface') and href.endswith('.html') and 
                    class_name and len(class_name) > 0):
                    
                    # Filter out classes ending with "Class" and other unwanted patterns
                    if not class_name.endswith('Class'):
                        class_names[class_name] = {}
            
            #print(f"Extracted {len(class_names)} class names from directory table")
        
        else:
            #print("Directory table not found, trying fallback methods...")
            
            # Fallback: Look for any links to interface pages
            all_links = soup.find_all('a', href=True)
            for link in all_links:
                href = link.get('href', '')
                if href.startswith('interface') and href.endswith('.html'):
                    class_name = link.get_text().strip()
                    if (class_name and len(class_name) > 0 and 
                        not class_name.endswith('Class') and
                        not any(char in class_name for char in ['(', ')', '<', '>', '[', ']', '{', '}'])):
                        class_names[class_name] = {}
        
        #print(f"Found {len(class_names)} total class names")
        
        if not class_names:
            print("Warning: No class names found. The page structure might have changed.")
            print("First 1000 characters of the page content:")
            print(soup.get_text()[:1000])
        else:
            # Show first few class names as examples
            sample_classes = list(class_names.keys())[:10]
            print(f"Sample class names: {sample_classes}")
        
        return class_names
        
    except requests.RequestException as e:
        print(f"Error fetching the page: {e}")
        return {}
    except Exception as e:
        print(f"Error parsing the page: {e}")
        return {}

def load_existing_data(filename):
    """
    Load existing class data from JSON file if it exists.
    
    Args:
        filename (str): JSON filename to load
        
    Returns:
        dict: Existing class data or empty dict if file doesn't exist
    """
    if os.path.exists(filename):
        try:
            with open(filename, 'r', encoding='utf-8') as f:
                data = json.load(f)
            print(f"Loaded existing data with {len(data)} classes from {filename}")
            return data
        except Exception as e:
            print(f"Error loading existing file: {e}")
            return {}
    else:
        print(f"No existing file found at {filename}")
        return {}

def fetch_class_methods(class_name, debug_mode=False):
    """
    Fetch public member functions from a class interface page.
    
    Args:
        class_name (str): Name of the class to fetch methods for
        debug_mode (bool): Enable verbose debug logging
        
    Returns:
        list: List of public method signatures as strings
    """
    # Replace underscores with double underscores for the URL
    url_class_name = class_name.replace('_', '__')
    url = f"https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interface{url_class_name}.html"
    if debug_mode:
        print(f"Fetching {url}")
    try:
        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
        }
        
        response = requests.get(url, headers=headers, timeout=30)
        
        # Check if we got redirected (common sign of 403 or blocked access)
        if response.url != url:
            print(f"  Request was redirected from {url} to {response.url}")
            return []
        
        if response.status_code == 404:
            print(f"  Class interface not found for {class_name} (404)")
            return [], None
        elif response.status_code == 403:
            print(f"  Access forbidden for {class_name} (403)")
            return [], None
        
        response.raise_for_status()
        
        soup = BeautifulSoup(response.content, 'html.parser')
        
        public_methods = []
        extends_class = None
        
        # Look for Doxygen-style member declarations table
        # Find tables with class "memberdecls"
        member_tables = soup.find_all('table', class_='memberdecls')
        
        if debug_mode:
            print(f"  Found {len(member_tables)} memberdecls tables")
        
        if not member_tables:
            if debug_mode:
                print(f"  No member declaration tables found for {class_name}")
                # Let's see what we got instead
                all_tables = soup.find_all('table')
                print(f"  Found {len(all_tables)} total tables")
                if all_tables:
                    for i, table in enumerate(all_tables[:3]):  # Show first 3 tables
                        table_classes = table.get('class', [])
                        print(f"    Table {i}: classes={table_classes}")
            return [], None
        
        for table in member_tables:
            # Look for the "Public Member Functions" heading row
            heading_rows = table.find_all('tr', class_='heading')
            
            if debug_mode:
                print(f"  Found {len(heading_rows)} heading rows in table")
            
            for heading_row in heading_rows:
                heading_text = heading_row.get_text()
                if debug_mode:
                    print(f"  Heading text: '{heading_text.strip()}'")
                
                if 'Public Member Functions' in heading_text:
                    if debug_mode:
                        print(f"  Found Public Member Functions section!")
                    
                    # Found the public methods section, now extract the methods
                    # Get all rows after this heading until we hit another heading or end of table
                    current_row = heading_row.find_next_sibling('tr')
                    row_count = 0
                    
                    while current_row:
                        row_count += 1
                        if debug_mode:
                            row_classes = current_row.get('class', [])
                            print(f"    Row {row_count}: classes={row_classes}")
                        
                        # Check if we hit another heading or inherit section
                        row_classes = current_row.get('class', [])
                        if (current_row.has_attr('class') and 
                            (any('heading' in cls for cls in row_classes) or 
                             any('inherit_header' in cls for cls in row_classes))):
                            
                            # Check if this is an inherit_header to extract parent class
                            if any('inherit_header' in cls for cls in row_classes):
                                # Look for the parent class link in the inherit header
                                inherit_link = current_row.find('a', class_='el')
                                if inherit_link:
                                    parent_class = inherit_link.get_text().strip()
                                    if parent_class:
                                        extends_class = parent_class
                                        if debug_mode:
                                            print(f"    Found inheritance: extends {parent_class}")
                            
                            if debug_mode:
                                print(f"    Breaking at row {row_count} - found heading/inherit")
                            break
                        
                        # Skip separator rows only
                        if (current_row.has_attr('class') and 
                            any('separator' in cls for cls in row_classes)):
                            if debug_mode:
                                print(f"    Skipping row {row_count} - separator")
                            current_row = current_row.find_next_sibling('tr')
                            continue
                        
                        # Extract method information from memitem rows
                        if current_row.has_attr('class') and any('memitem' in cls for cls in row_classes):
                            if debug_mode:
                                print(f"    Processing memitem row {row_count}")
                            
                            left_cell = current_row.find('td', class_='memItemLeft')
                            right_cell = current_row.find('td', class_='memItemRight')
                            
                            if left_cell and right_cell:
                                # Get return type and clean up HTML entities
                                return_type = left_cell.get_text().strip()
                                # Replace HTML entities properly
                                return_type = return_type.replace('\u00a0', ' ').replace('&#160;', ' ').strip()
                                
                                # Get the full method signature from right cell
                                method_text = right_cell.get_text().strip()
                                # Clean up HTML entities in method text too
                                method_text = method_text.replace('\u00a0', ' ').replace('&#160;', ' ')
                                
                                if debug_mode:
                                    print(f"      Raw return_type: '{return_type}'")
                                    print(f"      Raw method_text: '{method_text}'")
                                    print(f"      Right cell HTML: '{right_cell}'")
                                
                                # Get method name from the link
                                method_link = right_cell.find('a', class_='el')
                                if method_link:
                                    method_name = method_link.get_text().strip()
                                    
                                    if debug_mode:
                                        print(f"      Method name: '{method_name}'")
                                    
                                    # Build the complete signature
                                    if return_type and method_name:
                                        # Clean up the method text and ensure proper spacing
                                        method_text_clean = re.sub(r'\s+', ' ', method_text).strip()
                                        full_signature = f"{return_type} {method_text_clean}"
                                        # Clean up any double spaces
                                        full_signature = re.sub(r'\s+', ' ', full_signature).strip()
                                        
                                        # Check if the next row has a description
                                        next_row = current_row.find_next_sibling('tr')
                                        if (next_row and next_row.has_attr('class') and 
                                            any('memdesc' in cls for cls in next_row.get('class', []))):
                                            desc_cell = next_row.find('td', class_='mdescRight')
                                            if desc_cell:
                                                description = desc_cell.get_text().strip()
                                                # Clean up HTML entities and remove HTML tags
                                                description = description.replace('\u00a0', ' ').replace('&#160;', ' ')
                                                # Remove <br /> tags and other HTML
                                                description = re.sub(r'<[^>]+>', '', description).strip()
                                                # Clean up extra whitespace
                                                description = re.sub(r'\s+', ' ', description).strip()
                                                
                                                if description and len(description) > 0:
                                                    full_signature += f": {description}"
                                                    
                                                if debug_mode:
                                                    print(f"      Found description: '{description}'")
                                        
                                        if debug_mode:
                                            print(f"      Final signature: '{full_signature}'")
                                        
                                        public_methods.append(full_signature)
                                else:
                                    if debug_mode:
                                        print(f"      No method link found in right cell")
                            else:
                                if debug_mode:
                                    print(f"      Missing left or right cell")
                        
                        # Skip description rows (but don't process them separately since we handle them above)
                        elif (current_row.has_attr('class') and 
                              any('memdesc' in cls for cls in row_classes)):
                            if debug_mode:
                                print(f"    Skipping row {row_count} - memdesc (already processed)")
                            current_row = current_row.find_next_sibling('tr')
                            continue
                        
                        current_row = current_row.find_next_sibling('tr')
                    
                    if debug_mode:
                        print(f"  Finished processing {row_count} rows")
                    
                    break  # Found and processed the public methods section
        
        # Fallback method: Look for detailed member function documentation
        if not public_methods:
            # Look for detailed method documentation sections
            method_docs = soup.find_all('table', class_='memname')
            for method_table in method_docs:
                rows = method_table.find_all('tr')
                for row in rows:
                    cells = row.find_all('td')
                    if cells:
                        # Try to extract method signature from the detailed documentation
                        full_text = ''.join(cell.get_text() for cell in cells)
                        full_text = re.sub(r'\s+', ' ', full_text).strip()
                        
                        # Look for patterns like "ReturnType ClassName.MethodName ( params )"
                        if '(' in full_text and ')' in full_text and '.' in full_text:
                            # Clean up and format the signature
                            signature = full_text.replace('(', ' (').replace(')', ') ')
                            signature = re.sub(r'\s+', ' ', signature).strip()
                            public_methods.append(signature)
        
        # Remove duplicates and clean up
        public_methods = list(set(public_methods))
        public_methods = [method for method in public_methods if method.strip() and len(method.strip()) > 5]
        
        if debug_mode:
            print(f"  Found {len(public_methods)} public methods for {class_name}")
            if extends_class:
                print(f"  Found inheritance: {class_name} extends {extends_class}")
        
        return public_methods, extends_class
        
    except requests.RequestException as e:
        print(f"  Error fetching {class_name}: {e}")
        return [], None
    except Exception as e:
        print(f"  Error parsing {class_name}: {e}")
        return [], None

def save_to_json(data, filename):
    """
    Save data to a JSON file.
    
    Args:
        data (dict): Data to save
        filename (str): Output filename
    """
    try:
        # Create output directory if it doesn't exist
        os.makedirs(os.path.dirname(filename) if os.path.dirname(filename) else '.', exist_ok=True)
        
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
        
        #print(f"Successfully saved {len(data)} class names to {filename}")
        
    except Exception as e:
        print(f"Error saving to file: {e}")

def main():
    """Main function"""
    url = "https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/annotated.html"
    output_file = "reforger_classes.json"
    debug_mode = False  # Set to True for debug mode
    
    print("Arma Reforger Class Name Extractor & Method Scraper")
    print("=" * 50)
    
    if debug_mode:
        print("DEBUG MODE ENABLED - Only processing AABGridMap class")
    
    # Step 1: Check if reforger_classes.json exists
    class_data = load_existing_data(output_file)
    
    # Step 2: If no existing data, fetch the class list
    if not class_data:
        print("\nFetching initial class list...")
        class_names = fetch_class_names(url)
        
        if not class_names:
            print("No class names were extracted. Please check the URL and try again.")
            sys.exit(1)
        
        # Convert to the new format with empty objects
        class_data = {name: {} for name in class_names}
        
        # Save initial data
        save_to_json(class_data, output_file)
        print(f"Initial class list saved with {len(class_data)} classes")
    
    # Step 3: Process each class that isn't done yet
    total_classes = len(class_data)
    processed_count = 0
    remaining_classes = []
    
    # Find classes that need processing
    for class_name, class_info in class_data.items():
        if debug_mode:
            # In debug mode, only process AABGridMap
            if class_name == "AABGridMap":
                remaining_classes.append(class_name)
                # Force reprocessing in debug mode
                class_data[class_name]['done'] = False
        else:
            if not class_info.get('done', False):
                remaining_classes.append(class_name)
            else:
                processed_count += 1
    
    print(f"\nProcessing status:")
    print(f"Total classes: {total_classes}")
    print(f"Already processed: {processed_count}")
    print(f"Remaining to process: {len(remaining_classes)}")
    
    if not remaining_classes:
        print("\nAll classes have been processed!")
        return
    
    print(f"\nStarting incremental processing of {len(remaining_classes)} classes...")
    
    # Process each remaining class
    for i, class_name in enumerate(remaining_classes, 1):
        print(f"[{i}/{len(remaining_classes)}] Processing: {class_name}")
        
        # Fetch public methods for this class
        public_methods, extends_class = fetch_class_methods(class_name, debug_mode)
        
        # Update the class data
        class_data[class_name]['publicMethods'] = public_methods
        if extends_class:
            class_data[class_name]['extends'] = extends_class
        class_data[class_name]['done'] = True
        
        # Save progress to disk
        save_to_json(class_data, output_file)
        
        # Wait 0.2s before processing the next class
        time.sleep(0.2)
        
        # Print progress every 10 classes
        if i % 10 == 0:
            print(f"  Progress: {i}/{len(remaining_classes)} classes processed")
    
    print(f"\n🎉 Processing complete!")
    print(f"Total classes processed: {len(class_data)}")
    
    # Print some statistics
    classes_with_methods = sum(1 for info in class_data.values() if info.get('publicMethods'))
    total_methods = sum(len(info.get('publicMethods', [])) for info in class_data.values())
    
    print(f"Classes with methods found: {classes_with_methods}")
    print(f"Total methods extracted: {total_methods}")
    
    # Show some sample classes with methods
    sample_classes = [(name, info) for name, info in class_data.items() 
                     if info.get('publicMethods') and len(info['publicMethods']) > 0]
    
    if sample_classes:
        print(f"\nSample classes with methods:")
        for name, info in sample_classes[:3]:
            method_count = len(info['publicMethods'])
            print(f"  {name}: {method_count} methods")
            if method_count > 0:
                print(f"    Example: {info['publicMethods'][0]}")
    
    print(f"\nData saved to: {output_file}")

if __name__ == "__main__":
    main() 