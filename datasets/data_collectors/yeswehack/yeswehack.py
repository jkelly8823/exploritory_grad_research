import os
import requests
import dotenv

dotenv.load_dotenv()

# Repo details
REPO_OWNER = "yeswehack"
REPO_NAME = "vulnerable-code-snippets"
BRANCH = "main"
BASE_URL = f"https://raw.githubusercontent.com/{REPO_OWNER}/{REPO_NAME}/{BRANCH}"

# GitHub API URL to fetch the file tree
GITHUB_API_URL = f"https://api.github.com/repos/{REPO_OWNER}/{REPO_NAME}/git/trees/{BRANCH}?recursive=1"

# Optional: Personal Access Token to avoid rate limits (replace None with your token if needed)
GITHUB_TOKEN = os.getenv('github_key')

# Set up headers (optional, for authentication if needed)
headers = {}
if GITHUB_TOKEN:
    headers['Authorization'] = f'token {GITHUB_TOKEN}'

def download_file(file_url, output_dir):
    """Download a file from the given URL to the specified directory."""
    local_filename = os.path.join(output_dir, os.path.basename(file_url))
    print(f"Downloading {file_url} to {local_filename}")
    
    # Send GET request to download the file
    response = requests.get(file_url, stream=True)
    if response.status_code == 200:
        with open(local_filename, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
        print(f"Successfully downloaded: {local_filename}")
    else:
        print(f"Failed to download {file_url}: Status Code {response.status_code}")

def get_vsnippet_files():
    """Get all files in 'vsnippet' directories but no deeper."""
    print(f"Fetching repository data from: {GITHUB_API_URL}")
    response = requests.get(GITHUB_API_URL, headers=headers)

    if response.status_code != 200:
        print(f"Failed to fetch repository data: Status Code {response.status_code}")
        return []

    tree_data = response.json().get('tree', [])
    vsnippet_files = []

    print(tree_data)

    # Look for files in 'vsnippet' directories that aren't nested deeper
    for item in tree_data:
        if item["type"] == "blob" and "/vsnippet/" in item["path"]:
            # Check to ensure it's directly inside a 'vsnippet' folder
            if "vsnippet" in item["path"].split("/") and item["path"].split("/")[-2] == "vsnippet":
                file_url = f"{BASE_URL}/{item['path']}"
                vsnippet_files.append(file_url)

    return vsnippet_files

def main():
    vsnippet_files = get_vsnippet_files()

    if not vsnippet_files:
        print("No files found in 'vsnippet' directories.")
        return

    output_dir = "datasets/data_samples/use2/yeswehack"
    os.makedirs(output_dir, exist_ok=True)

    for file_url in vsnippet_files:
        download_file(file_url, output_dir)

    print(f"Downloaded {len(vsnippet_files)} files to '{output_dir}'.")

if __name__ == "__main__":
    main()
