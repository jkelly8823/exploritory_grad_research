import pandas as pd
import re

# Load the CSV file into a DataFrame
df = pd.read_csv('wild_samples.csv')

# Function to extract the year from a CVE string (format "CVE-YYYY-XXXXX")
def extract_cve_year(cve):
    match = re.search(r'CVE-(\d{4})', cve)
    return int(match.group(1)) if match else None

# Expand the 'cves' column into individual CVEs if it's an array or list of CVEs
df['cves_list'] = df['cves'].apply(lambda x: str(x).split(',') if pd.notna(x) else [])

# Flatten the DataFrame so each row contains a single CVE
df_exploded = df.explode('cves_list')

# Remove any leading/trailing spaces and clean up the CVEs
df_exploded['cves_list'] = df_exploded['cves_list'].str.strip()

# Filter out rows where CVE is missing or invalid
df_exploded = df_exploded[df_exploded['cves_list'].str.contains(r'CVE-\d{4}-\d+', na=False)]

# Extract the year from each CVE
df_exploded['cve_year'] = df_exploded['cves_list'].apply(extract_cve_year)

# Sort the DataFrame by the CVE year
df_sorted = df_exploded.sort_values(by='cve_year')

# Get the oldest CVE
oldest_cve = df_sorted.iloc[0]['cves_list']

# Print the oldest CVE
print(f"The oldest CVE is: {oldest_cve}")
