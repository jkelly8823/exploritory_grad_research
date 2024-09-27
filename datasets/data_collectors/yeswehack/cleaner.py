import os
import re

# Define the directory to process
DIR_PATH = 'datasets/data_samples/use2/yeswehack'

# Regular expressions for different types of comments
# Single-line comments
single_line_patterns = {
    '.php': re.compile(r'//.*|#.*'),
    '.py': re.compile(r'#.*'),
    '.go': re.compile(r'//.*'),
    '.js': re.compile(r'//.*'),
    '.html': re.compile(r'<!--.*?-->', re.DOTALL),
    '.c': re.compile(r'//.*'),
    '.java': re.compile(r'//.*'),
    '.rb': re.compile(r'#.*'),
}

# Multi-line comments
multi_line_patterns = {
    '.php': re.compile(r'/\*.*?\*/', re.DOTALL),
    '.py': re.compile(r'"""(.*?)"""|\'\'\'(.*?)\'\'\'', re.DOTALL),
    '.go': re.compile(r'/\*.*?\*/', re.DOTALL),
    '.js': re.compile(r'/\*.*?\*/', re.DOTALL),
    '.html': re.compile(r'<!--.*?-->', re.DOTALL),
    '.c': re.compile(r'/\*.*?\*/', re.DOTALL),
    '.java': re.compile(r'/\*.*?\*/', re.DOTALL),
    '.rb': re.compile(r'=begin(.*?)=end', re.DOTALL),
}

# Specific line pattern to remove
# specific_line_pattern = re.compile(r"<?php\
# include_once('./ignore/design/design.php');\
# $title = 'Vsnippet #31 - Local File Inclusion (LFI)';\
# $design = Design(__FILE__, $title);\
# \
# /**\
#  * YesWeHack - Vulnerable Code Snippet\
#  */\
# ?>")

def process_file(file_path, extension):
    """Process each file by removing comments and specific lines."""
    with open(file_path, 'r', encoding='utf-8') as file:
        file_contents = file.read()

    # Remove single-line comments
    if extension in single_line_patterns:
        file_contents = single_line_patterns[extension].sub('', file_contents)

    # Remove multi-line comments
    if extension in multi_line_patterns:
        file_contents = multi_line_patterns[extension].sub('', file_contents)

    # Remove specific lines (e.g., include_once('./ignore/design/design.php');)
    # file_contents = specific_line_pattern.sub('', file_contents)

    # Write the cleaned content back to the file
    with open(file_path, 'w', encoding='utf-8') as file:
        file.write(file_contents)

    print(f"Processed: {file_path}")

def iterate_directory(directory):
    """Recursively iterate through the directory and process files based on extensions."""
    for root, dirs, files in os.walk(directory):
        for file in files:
            # Detect file extension
            _, extension = os.path.splitext(file)
            
            # Process only supported file types
            if extension in single_line_patterns or extension in multi_line_patterns:
                file_path = os.path.join(root, file)
                process_file(file_path, extension)

if __name__ == "__main__":
    iterate_directory(DIR_PATH)
