import os
import pandas as pd
import re
import dotenv
from pathlib import Path

dotenv.load_dotenv()

def main():
    fail_log = []

    cols = ['TRIAL', 'PROMPT', 'MODEL', 'SOURCE', 'DATA_ID', 'FILENAME', 'VULN_MODEL', 'VULN_TRUTH']
    df = pd.DataFrame(columns=cols)
    print(os.getenv('parser_dir'))
    for root, dirs, files in os.walk(os.getenv('parser_dir')):
        for filename in files:
            if filename == 'fail_log.txt.txt':
                continue
            file_path = os.path.join(root, filename)
            path_parts = Path(file_path).parts

            with open(file_path, 'r+', encoding='utf-8', errors='replace') as file:
                output = file.read()

            # Define the regex pattern to match both cases
            pattern = r"VULN.*:.*(YES)|(NO)"

            # Search for matches using regex
            matches = re.findall(pattern, output, re.IGNORECASE)

            llmVuln = -1
            for match in matches:
                if match[0]:  # Group 1 (YES)
                    llmVuln = True
                    break
                elif match[1]:  # Group 2 (NO)
                    llmVuln = False
                    break
            if (llmVuln == -1):
                fail_log.append(f'Could not match vulnerability status in: {file_path}\n')
                continue
            
            # True Vuln Status
            trueVuln = True
            # print(path_parts)
            if ('cwe' in path_parts[4].lower()) and ('good' in path_parts[6].lower()):
                trueVuln = False
            if ('cwe' not in path_parts[4].lower()) and ('_after.' in path_parts[6].lower()):
                trueVuln = False

            # New row data as a dictionary
            new_row = {'TRIAL':path_parts[1], 'PROMPT':path_parts[2], 'MODEL':path_parts[3], 'SOURCE':path_parts[4], 'DATA_ID':path_parts[5], 'FILENAME':path_parts[6], 'VULN_MODEL':llmVuln, 'VULN_TRUTH':trueVuln}

            # Add the new row
            df.loc[len(df)] = new_row

    out_filename = os.getenv('parser_dir').replace('\\','_').replace('/','_')
    df.to_csv(f"analyzers/parsed/{out_filename}_parsed_outputs.csv", index=True, index_label='INDEX')

    with open(f"analyzers/fail_logs/{out_filename}_fail_log.txt", 'w+') as file:
        file.writelines(fail_log)

if __name__ == "__main__":
    main()