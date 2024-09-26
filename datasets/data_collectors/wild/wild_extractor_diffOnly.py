import requests
from requests.adapters import HTTPAdapter, Retry
import os
from pathlib import Path
import re
import pandas as pd
import base64
import dotenv

dotenv.load_dotenv()

# Create a session
session = requests.Session()

# Define retry settings
retries = Retry(
    total=2,  # Total number of retries
    backoff_factor=30,  # Delay between retries (backoff_factor * (2 ** (retry_number - 1)))
    status_forcelist=[i for i in range(400, 600) if i != 404],  # Retry on all 4xx and 5xx status codes except not found
    raise_on_status=False
)

# Create and mount the adapter
adapter = HTTPAdapter(max_retries=retries)
# session.mount("http://", adapter)
session.mount("https://", adapter)

# Failure logs
fail_log = []
fail_403 = 0
fail_403_limit = 3

# Filepath reject regex
reject_pattern = re.compile(r'news|log|readme|.rst', re.IGNORECASE)  # Pattern to detect suppression commits

def get_commit_diff(repo_owner, repo_name, commit_id, token=None):
    url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/commits/{commit_id}"
    headers = {}
    if token:
        headers['Authorization'] = f'token {token}'

    response = session.get(url, headers=headers)
    
    if response.status_code == 200:
        return response.json()
    else:
        if response.status_code == 403:
            global fail_403 
            fail_403 += 1
        msg = f"Failed to fetch commit data: {response.status_code} for {repo_owner}/{repo_name}/commits/{commit_id}"
        fail_log.append(msg)
        print(msg)
        return None

def fetch_file_content(repo_owner, repo_name, file_path, ref, token=None, isBefore=True):
    url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/contents/{file_path}?ref={ref}"
    headers = {}
    if token:
        headers['Authorization'] = f'token {token}'
    
    response = session.get(url, headers=headers)
    
    if response.status_code == 200:
        file_data = response.json()
        # Decode the base64 content and convert it to a string
        content_base64 = file_data.get('content', '')
        file_content = base64.b64decode(content_base64).decode('utf-8')
        return file_content
    else:
        if response.status_code == 403:
            global fail_403 
            fail_403 += 1
        if isBefore:
            msg = f"Failed to fetch file content: {response.status_code} for {repo_owner}/{repo_name}/commits/{ref} - {file_path} (BEFORE FILE)"
        else:
            msg = f"Failed to fetch file content: {response.status_code} for {repo_owner}/{repo_name}/commits/{ref} - {file_path} (AFTER FILE)"
        fail_log.append(msg)
        print(msg)
        return None

def find_function_boundaries(file_content, modified_line):
    lines = file_content.splitlines()
    
    # Find the start of the function
    start_line = modified_line
    while start_line > 0:
        line = lines[start_line].strip()
        if re.match(r'^\s*(def|function|.*\))\s*:', line) or line.endswith('{'):
            break
        start_line -= 1
    
    # Find the end of the function
    end_line = modified_line
    brace_count = 0
    while end_line < len(lines):
        line = lines[end_line].strip()
        if line.endswith('}'):
            brace_count -= 1
            if brace_count == 0:
                break
        elif line.endswith('{'):
            brace_count += 1
        end_line += 1
    
    return start_line, end_line

def parse_patch(patch, before_file_content, after_file_content):
    before_lines = []
    after_lines = []
    
    tag_pattern = re.compile(r'^@@ -(\d+),(\d+) \+(\d+),(\d+) @@')
    hunk_start_lines = []

    for line in patch.splitlines():
        match = re.search(tag_pattern, line)
        if match:
            before_line = int(match.group(1))
            before_amt = int(match.group(2))
            after_line = int(match.group(3))
            after_amt = int(match.group(4))
            hunk_start_lines.append((before_line, before_amt, after_line, after_amt))
    
    for before_line, before_amt, after_line, after_amt in hunk_start_lines:     
        before_lines.extend(before_file_content.splitlines()[before_line-1:before_line+before_amt])
        after_lines.extend(after_file_content.splitlines()[after_line-1:after_line+after_amt])
    
    return before_lines, after_lines

def extract_and_save_changes(repo_owner, repo_name, commit_data, output_dir='samples', token=None):
    
    commit_id = commit_data['sha']
    files = commit_data.get('files', [])
    
    for file in files:
        if fail_403 >= fail_403_limit:
            break
        
        filename = file['filename']
        file_status = file['status']

        if not '.' in filename:
            continue
        if not commit_data['parents'][0]['sha']:
            continue
        if reject_pattern.search(filename):
            continue
        if (file_status == 'added') or (file_status == 'removed'):
            print(f"Skipping added/removed file: {filename}")
            continue

        patch = file.get('patch', '')
        
        
        if patch:
            print(f'Repo: {repo_name} Commit ID: {commit_id}')
            # Fetch full file content before and after the commit
            before_file_content = fetch_file_content(repo_owner, repo_name, filename, commit_data['parents'][0]['sha'], token, True)
            if not before_file_content:
                continue
            
            after_file_content = fetch_file_content(repo_owner, repo_name, filename, commit_id, token, False)
            if not after_file_content:
                continue

            # Parse the patch into before and after sections
            before_lines, after_lines = parse_patch(patch, before_file_content, after_file_content)
            
            # Save the before and after content to separate files
            save_content_to_file(filename, commit_id, "before", before_lines, output_dir)
            save_content_to_file(filename, commit_id, "after", after_lines, output_dir)

def save_content_to_file(filename, commit_id, version, content_lines, output_dir):
    # Create output directory if it doesn't exist
    commit_output_dir = os.path.join(output_dir, commit_id)
    Path(commit_output_dir).mkdir(parents=True, exist_ok=True)
    
    # Define the output file path
    output_file_path = os.path.join(commit_output_dir, f"{Path(filename).stem}_{version}{Path(filename).suffix}")

    # Write content to the file
    with open(output_file_path, "w+", encoding='utf-8') as f:
        f.write('\n'.join(content_lines))
    
    print(f"Saved {version} version of {filename} to {output_file_path}")

def main():
    output_dir = os.getenv('github_dir')

    # Set limit for how many commits to iterate through out of the csv
    limit = int(os.getenv('github_limit'))
    
    # Read the CSV file
    file_pth = os.getenv('github_src')
    df = pd.read_csv(file_pth)
    df = df[df['index'] >= 983]

    token = os.getenv('github_key')  # Optional: Replace with your GitHub personal access token if needed

    # for commit_id in commit_ids:
    count = 0
    for row in df.itertuples(index=True,):
        if fail_403 >= fail_403_limit:
            fail_log.append(f'Hit 403 limit at index: {row.index - fail_403_limit}')
            break

        # Fetch commit data
        commit_data = get_commit_diff(row.repo_owner, row.repo_name, row.commit_id, token)
        
        if commit_data:
            # Extract and save the changes
            extract_and_save_changes(row.repo_owner, row.repo_name, commit_data, f'{output_dir}/{row.repo_name}_samples', token)

            count += 1
            if count >= limit:
                break

    with open(f"{output_dir}/fail_log.txt", 'w+') as file:
        for line in fail_log:
            file.write(f"{line}\n")

if __name__ == "__main__":
    main()
