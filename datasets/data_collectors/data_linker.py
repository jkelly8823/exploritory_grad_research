import pandas as pd

# Read the CSV files into DataFrames
cve_descriptions = pd.read_csv('cve_descriptions.csv')
wild_samples = pd.read_csv('wild_samples.csv')
wild_samples['cves'] = wild_samples['cves'].apply(lambda x: eval(x) if pd.notna(x) else [])

# Ensure the 'cves' column is a list or series
cves_list = set(wild_samples['cves'].dropna().explode())

# Filter rows in cve_descriptions based on CVE_ID
filtered_df = cve_descriptions[cve_descriptions['CVE_ID'].isin(cves_list)]

# Reindex the INDEX column
filtered_df = filtered_df.reset_index(drop=True)
filtered_df['INDEX'] = filtered_df.index

# Write the filtered DataFrame to a new CSV file
filtered_df.to_csv('sampled_cve_descriptions.csv', index=False)
