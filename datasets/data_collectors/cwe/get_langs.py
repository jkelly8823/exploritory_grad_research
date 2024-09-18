import xml.etree.ElementTree as ET

# Your XML string

# Parse the XML data
tree = ET.parse('cwec_v4.15.xml')
root = tree.getroot()

# Define the namespace
ns = {'ns': 'http://cwe.mitre.org/cwe-7', 'xhtml': 'http://www.w3.org/1999/xhtml'}

# Find all Example_Code elements and extract the Language attribute
languages = set()
for example_code in root.findall('.//ns:Example_Code', ns):
    language = example_code.get('Language')
    if language:
        languages.add(language)

# Print unique languages
print("Unique demonstrative example languages:")
for language in languages:
    print(language)
