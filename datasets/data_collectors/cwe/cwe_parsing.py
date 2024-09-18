import xml.etree.ElementTree as ET
import pandas as pd
import os

# Output directory
output_dir = 'cwe_samples'

# Pandas
cols = ['CWE_ID', 'CWE_NAME', 'CWE_DESCRIPTION', 'CWE_PLATFORMS']
df = pd.DataFrame(columns=cols)

# Parse the XML data
tree = ET.parse('cwec_v4.15.xml')
root = tree.getroot()

# Define the namespace
ns = {'cwe': 'http://cwe.mitre.org/cwe-7', 'xhtml': 'http://www.w3.org/1999/xhtml'}

# Scraped list of language types mapped to file extensions
language_extensions = {
    "Python": ".py",
    "Objective-C": ".m",
    "Perl": ".pl",
    "x86 Assembly": ".asm",
    "Go": ".go",
    "Rust": ".rs",
    "Shell": ".sh",
    "HTML": ".html",
    "JSON": ".json",
    "JSP": ".jsp",
    "XML": ".xml",
    "C": ".c",
    "Verilog": ".v",
    "ASP.NET": ".asp",
    "PHP": ".php",
    "JavaScript": ".js",
    "Other": "None",
    "C#": ".cs",
    "C++": ".cpp",
    "Java": ".java",
    "Pseudocode": "None",
    "SQL": ".sql"
}

# Iterate over each Weakness element
print('------------------------------')
cnt = 0
for weakness in root.findall('cwe:Weaknesses/cwe:Weakness', ns):
    weakness_id = weakness.get('ID')
    
    # Find the Demonstrative_Examples element
    demonstrative_examples = weakness.find('cwe:Demonstrative_Examples', ns)
    if demonstrative_examples is not None:
        # Find all Language elements
        languages = weakness.findall('cwe:Applicable_Platforms/cwe:Language', ns)

        # Extract the Name attribute and concatenate them into a comma-separated string
        language_names = ', '.join([lang.get('Name') if lang.get('Name') 
                            else lang.get('Class') 
                            for lang in languages])
        
        # Get examples
        examples = demonstrative_examples.findall('cwe:Demonstrative_Example/cwe:Example_Code', ns)
        
        cnt2 = 0
        if examples:
            # Set output path for examples
            path = f'{output_dir}/{weakness_id}/'
            
            print(f'{weakness_id} EXAMPLES:')
            for example in examples:
                nature = example.get('Nature')
                language = example.get('Language')
                if nature is None or language not in language_extensions or language_extensions[language] == 'None':
                    continue

                cnt2 += 1
                
                # Ensure output folder exists
                os.makedirs(path, exist_ok=True)

                # Extract the code content
                code_text = ET.tostring(example, encoding='unicode', method='text') 
            
                # Create the filename, checking for dupes
                dupe_count = 0
                dupe_str = ""
                while True:
                    if dupe_count > 0:
                        dupe_str = f'_{dupe_count}'
                    filename = f"{weakness_id}_{nature}{dupe_str}{language_extensions[language]}"
                    if os.path.exists(path+filename):
                        dupe_count += 1
                    else:
                        break
                
                # Save the content to a file
                with open(path + filename, 'w+') as f:
                    f.write(code_text)
                print(f"Saved: {filename}")
            print('------------------------------')
        if cnt2 != 0:
            # Add weakness row entry to dataframe
            new_row = [weakness_id, weakness.get('Name'), weakness.find('cwe:Description', ns).text, language_names]
            df.loc[len(df)] = new_row
            
            cnt += 1

# Save dataframe to csv
filename = "cwe_dataset.csv"
df.to_csv(f'{output_dir}/{filename}',index_label='INDEX')
print(f"Saved: {filename}")
print('------------------------------')