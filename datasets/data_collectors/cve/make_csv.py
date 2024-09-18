import os
import json
import pandas as pd

# Directory where the CVE JSON files are stored
cve_directory = 'cves'

# Create an empty DataFrame with appropriate columns
df = pd.DataFrame(columns=['CVE_ID', 'CVE_DESCRIPTION', 'LINKED_CWES'])

def process_file(file_path):
    global df
    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            cve_data = json.load(f)
        
        # Check if rejected:
        state = cve_data.get('cveMetadata', {}).get('state', '')
        if 'published' not in state.lower(): 
            # print(os.path.splitext(os.path.basename(file_path))[0], state)
            return

        # Extract descriptions
        descriptions = cve_data.get('containers', {}).get('cna', {}).get('descriptions', [])
        english_descriptions = [desc.get('value', '') for desc in descriptions if 'en' in desc.get('lang').lower()]

        # Extract unique cweId values
        problemTypes = cve_data.get('containers', {}).get('cna', {}).get('problemTypes', [])
        # Adapted extraction of unique cweId values
        cwe_ids = list({desc.get('cweId') for pt in problemTypes for desc in pt.get('descriptions', []) if 'cweId' in desc})

        
        # Append to the main DataFrame
        tmp_df = pd.DataFrame([{'CVE_ID': os.path.splitext(os.path.basename(file_path))[0], 'CVE_DESCRIPTION': english_descriptions, 'LINKED_CWES': cwe_ids}])
        df = pd.concat([df, tmp_df], ignore_index=True)
    
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON in file {file_path}: {e}")
    except Exception as e:
        print(f"Unexpected error with file {file_path}: {e}")

def scan_directory(directory):
    try:
        with os.scandir(directory) as it:
            for entry in it:
                if entry.is_dir():
                    # Recursively scan subdirectories
                    print(entry.name)
                    scan_directory(entry.path)
                elif entry.is_file() and entry.name.endswith('.json') and not entry.name.startswith('delta'):
                    # Process the file
                    process_file(entry.path)
    except PermissionError as e:
        print(f"Permission error accessing {directory}: {e}")
    except Exception as e:
        print(f"Unexpected error accessing {directory}: {e}")

# Start scanning from the top-level directory
scan_directory(cve_directory)

# Check if any row contains more than one item in the 'CVE_DESCRIPTION' array
if all(df['CVE_DESCRIPTION'].apply(lambda x: len(x) == 1)):
    # Replace the list with the single item if it only contains one
    df['CVE_DESCRIPTION'] = df['CVE_DESCRIPTION'].apply(lambda x: x[0] if isinstance(x, list) and len(x) == 1 else x)

# Save the DataFrame to a CSV file, including the index
df.to_csv('cve_descriptions.csv', index=True, index_label='INDEX')

print("Data saved to 'cve_descriptions.csv'")
