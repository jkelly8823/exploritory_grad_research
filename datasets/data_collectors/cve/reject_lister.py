import os
import json
import pandas as pd

# Directory where the CVE JSON files are stored
cve_directory = 'cves'

# Create an empty DataFrame with appropriate columns
df = pd.DataFrame(columns=['CVE_ID'])

def process_file(file_path):
    global df
    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            cve_data = json.load(f)
        
        # Check if rejected:
        state = cve_data.get('cveMetadata', {}).get('state', '')
        if 'rejected' not in state.lower(): 
            return

        # Append to the main DataFrame
        tmp_df = pd.DataFrame([{'CVE_ID': os.path.splitext(os.path.basename(file_path))[0]}])
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

# Save the DataFrame to a CSV file, including the index
df.to_csv('cve_rejects.csv', index=True, index_label='INDEX')

print("Data saved to 'cve_rejects.csv'")
