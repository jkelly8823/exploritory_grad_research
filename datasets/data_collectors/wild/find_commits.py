import requests
import re
import pandas as pd
import dotenv
import os
import time

dotenv.load_dotenv()

def search_commits(sources, token):
    commits_data = []
    
    cve_pattern = re.compile(r'cve-\d{4}-\d{4,7}', re.IGNORECASE)
    cwe_pattern = re.compile(r'cwe-\d{1,5}', re.IGNORECASE)
    reject_pattern = re.compile(r'suppress|logback', re.IGNORECASE)  # Pattern to detect suppression commits

    query = "CVE OR CWE"
    headers = {
        'Accept': 'application/vnd.github.cloak-preview',
        'Authorization': f'token {token}'
    }

    fail_out = False
    for row in sources.itertuples(index=False):
        if fail_out:
            break

        repo_name = row.repo_name
        repo_owner = row.repo_owner

        url = f"https://api.github.com/search/commits?q={query}+repo:{repo_owner}/{repo_name}"
        
        page = 1
        
        while True:
            rate_wait_attempts = 2
            while rate_wait_attempts > 0:
                paged_url = f"{url}&per_page=100&page={page}"
                response = requests.get(paged_url, headers=headers)

                # Print rate limit information
                print("---------------------")
                print(f"Rate Limit Limit: {response.headers.get('X-RateLimit-Limit')}")
                print(f"Rate Limit Remaining: {response.headers.get('X-RateLimit-Remaining')}")
                print(f"Rate Limit Used: {response.headers.get('X-RateLimit-Used')}")
                print(f"Rate Limit Reset Time (UTC timestamp): {response.headers.get('X-RateLimit-Reset')}")
                print(f"Rate Limit Resource: {response.headers.get('X-RateLimit-Resource')}")
                print("---------------------")
                
                if response.status_code != 200:
                    print(f"Error: {response.status_code} - {response.text}")
                    if 'rate limit' in response.text:
                        print("Trying a minute delay to reset rate limits")
                        time.sleep(65)
                    else:
                        fail_out = True
                    rate_wait_attempts -= 1
                else:
                    break

            if fail_out:
                break    

            commit_items = response.json().get('items', [])

            if not commit_items:
                break  # Stop if there are no more results
            
            for commit in commit_items:
                commit_message = commit['commit']['message']
                commit_sha = commit['sha']
                commit_url = commit['html_url']
                commit_date = commit['commit']['committer']['date']
                
                if reject_pattern.search(commit_message):
                    continue
                
                # Use case-insensitive regex to find CVEs and CWEs
                cves = [cve.upper() for cve in cve_pattern.findall(commit_message)]
                cwes = [cwe.upper() for cwe in cwe_pattern.findall(commit_message)]
                
                if cves or cwes:
                    commits_data.append({
                        'repo_owner': repo_owner,
                        'repo_name': repo_name,
                        'commit_id': commit_sha,
                        'commit_url': commit_url,
                        'commit_date': commit_date,
                        'cves': cves,
                        'cwes': cwes
                    })
                
            page += 1  # Go to the next page

    # Create a DataFrame and sort by commit date
    df = pd.DataFrame(commits_data)
    df['commit_date'] = pd.to_datetime(df['commit_date'])  # Convert to datetime
    df_sorted_grouped = df.groupby('repo_name').apply(lambda x: x.sort_values(by='commit_date', ascending=False)).reset_index(drop=True)  # Sort by date within repos, most recent first

    return df_sorted_grouped

# Example usage
token = os.getenv('github_key')

# Load csv of repos
df_src = pd.read_csv('shortlist_repositories.csv')

# Fetch commits and store in DataFrame
df_commits = search_commits(df_src, token)

# Print DataFrame
print(df_commits)

# Save to CSV if needed
df_commits.to_csv("wild_samples.csv",index_label='index')
