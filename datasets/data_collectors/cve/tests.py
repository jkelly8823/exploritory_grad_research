# import pandas as pd

# # Load the DataFrame from CSV
# df = pd.read_csv('cve_descriptions.csv')

# # Filter rows where the 'LINKED_CWES' list has more than one item
# filtered_df = df[df['CVE_DESCRIPTION'].apply(lambda x: len(eval(x)) > 1)]

# # Print the filtered rows
# print(filtered_df)

import pandas as pd

# Load the DataFrame from CSV
df = pd.read_csv('cve_descriptions.csv')
df2 = pd.read_csv('../cwe_xml/cwe_dataset.csv')

df['LINKED_CWES'] = df['LINKED_CWES'].apply(lambda x: eval(x) if pd.notna(x) else [])

# Flatten the list of lists in df1['col1']
flat_list = [item for sublist in df['LINKED_CWES'] for item in sublist]

# Convert the flattened list and df2['col2'] to sets
set1 = set(flat_list)
set2 = set(df2['CWE_ID'])

# Find items in set1 that are not in set2
difference = set1 - set2

# Convert the result to a list if needed
difference_list = list(difference)

print(difference_list)



# # Filter rows where the 'LINKED_CWES' list has more than one item
# filtered_df = df[df['CVE_DESCRIPTION'].apply(lambda x: len(eval(x)) > 1)]

# # Print the filtered rows
# print(filtered_df)
# # Check if all rows have only one item in their 'CVE_DESCRIPTION' list
# # Assuming that 'CVE_DESCRIPTION' is read as a string and each entry should be treated as a list-like structure
# df['CVE_DESCRIPTION'] = df['CVE_DESCRIPTION'].apply(lambda x: eval(x) if pd.notna(x) else [])

# print('TRUTHY:', all(df['CVE_DESCRIPTION'].apply(lambda x: len(x) == 1)))

# # Filter rows where the 'LINKED_CWES' list has more than one item
# filtered_df2 = df[df['CVE_DESCRIPTION'].apply(lambda x: len(x) > 1)]

# # Print the filtered rows
# print(filtered_df2)

# # Filter rows where the 'LINKED_CWES' list has more than one item
# filtered_df3 = df[df['CVE_DESCRIPTION'].apply(lambda x: len(x) < 1)]

# # Print the filtered rows
# print(filtered_df3)

# df2 = pd.read_csv('cve_rejects.csv')
# filtered_df4 = filtered_df3[~filtered_df3['CVE_ID'].isin(df2['CVE_ID'])]
# print(filtered_df4)

# # Check if any row contains more than one item in the 'CVE_DESCRIPTION' array
# if all(df['CVE_DESCRIPTION'].apply(lambda x: len(x) == 1)):
#     # Replace the list with the single item if it only contains one
#     df['CVE_DESCRIPTION'] = df['CVE_DESCRIPTION'].apply(lambda x: x[0] if isinstance(x, list) and len(x) == 1 else x)
#     print('In here')
# # Save the updated DataFrame back to the CSV file
# df.to_csv('cve_descriptions2.csv', index=False)

# # Print the modified DataFrame
# print(df)
