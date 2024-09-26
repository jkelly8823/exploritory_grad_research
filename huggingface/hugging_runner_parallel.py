import os
from pathlib import Path
import dotenv
import requests
import re
from transformers import AutoTokenizer
import concurrent.futures
import pandas as pd
import ast

dotenv.load_dotenv()

fail_log = []

api_url = os.getenv('huggingface_url')
api_key = os.getenv('huggingface_key')
headers = {
    "Authorization": f"Bearer {api_key}",
    "Content-Type": "application/json",
    "x-wait-for-model": "true"
}

prev_fails = []
try:
    with open("huggingface/skip_fail.txt","r+") as file:
        prev_fails = [line.strip() for line in file.readlines()]
except Exception as e:
    print("Error opening skip_fail, continuing:", e)

def write_resp(pth,resp):
    try:
        with open(pth,"w+", encoding='utf-8', errors="replace") as file:
            file.write(resp)
    except Exception as e:
        print(f'Failed to write file {pth} with exception: {e}')

def tokenCounter(model_id, prompt, response_json):

    # Load the tokenizer corresponding to your model
    tokenizer = AutoTokenizer.from_pretrained(model_id)
    # Tokenize the prompt and count the tokens
    input_tokens = tokenizer.encode(prompt, return_tensors='pt')
    num_input_tokens = input_tokens.shape[1]  # Number of tokens in the input

    # Assuming the response contains the generated text
    generated_text = response_json[0]["generated_text"]

    # Tokenize the response
    output_tokens = tokenizer.encode(generated_text, return_tensors='pt')
    num_output_tokens = output_tokens.shape[1]  # Number of tokens in the response

    print("-------------------")
    # print("Model:", model_id)
    # print("Prompt:", prompt)
    # print("Generated response:", response_json)

    # Print the token counts
    print(f"Number of tokens in input: {num_input_tokens}")
    print(f"Number of tokens in response: {num_output_tokens}")
    print(f"Total tokens used: {num_input_tokens + num_output_tokens}")
    print("-------------------")

def call_huggingface_single(msg_content, model_name, api_url, headers):
    """Function to process a single input."""
    pth_model_name = model_name.replace("\\", "_").replace("/", "_")
    output_file_path = f'huggingface_outputs/{os.getenv("test_dir")}/prompt_{msg_content[3]}/{pth_model_name}/{msg_content[0]}/{msg_content[1]}.txt'
    
    # if os.path.exists(output_file_path):
    #     print(f"File already exists: {output_file_path}")
    #     return f"File already exists: {output_file_path}"
    # if f"{msg_content[0]}/{msg_content[1]}" in prev_fails:
    #     print(f"File failed by another model, skipping: {msg_content[0]}/{msg_content[1]}")
    #     return f"File failed by another model, skipping: {msg_content[0]}/{msg_content[1]}"

    try:
        # Model specific parameters
        params = {}
        if 'gemma' in model_name.lower():
            params['return_full_text'] = False
        elif 'llama' in model_name.lower():
            params['max_new_tokens'] = 256
        elif 'mixtral' in model_name.lower():
            params['echo'] = False

        data = {
            "inputs": msg_content[2],
            "parameters": params
        }
        
        response = requests.post(api_url, headers=headers, json=data)
        if response.status_code not in range(200, 300):
            raise Exception(f"Bad Status Code: {response.status_code}")
        response = response.json()

        resp_txt = response[0]['generated_text']
        resp_txt = resp_txt.replace(msg_content[2],"") # LLama
        
        os.makedirs(os.path.dirname(output_file_path), exist_ok=True)
        write_resp(output_file_path, resp_txt)
        return f"Processed and saved: {output_file_path}"

    except Exception as e:
        print("An error occurred:", e)
        print(f'Exception on file: {msg_content[0]}/{msg_content[1]}')
        fail_log.append(f"{msg_content[0]}/{msg_content[1]}\n")
        return False

def call_huggingface(msg_content_list, model_name):
    """Main function to call Hugging Face API in parallel."""
    results = []
    
    with concurrent.futures.ThreadPoolExecutor() as executor:
        futures = {
            executor.submit(call_huggingface_single, msg_content, model_name, api_url, headers): msg_content
            for msg_content in msg_content_list
        }

        file_cnt = 0
        for future in concurrent.futures.as_completed(futures):
            msg_content = futures[future]
            try:
                result = future.result()
                results.append(f"{str(result)}\n\n")
                file_cnt += 1
                print("Files evaluated %6d" % file_cnt, end='\r')
            except Exception as e:
                print(f"Error processing {msg_content[0]}/{msg_content[1]}: {e}")
    # print(f"Results: {results}")
    print('\n')
    return results

def makePrompt(value, code_sample, file_path, prompt):
    split_pth = file_path.split('\\')
    # print(file_path, prompt, split_pth)
    if prompt == 'context':
        cwe_df = pd.read_csv('datasets/data_descriptors/sampled_cwe_descriptions.csv',index_col='INDEX')
        cve_df_cmt = pd.read_csv('datasets/data_descriptors/sampled_cve_commits.csv',index_col='index')
        cve_df_dsc = pd.read_csv('datasets/data_descriptors/sampled_cve_descriptions.csv',index_col='INDEX')
        desc = ''
        
        if 'cwe_samples' == split_pth[0]:
            desc = cwe_df[cwe_df['CWE_ID'] == int(split_pth[1])]['CWE_DESCRIPTION'].values[0]
            return f'{value}\n\nVULNERABILITY LIST:\nCWE_ID: {split_pth[1]}\nCWE Description:\n{desc}\n\nCode Sample:\n{code_sample}\n\nThis concludes your task. Provide your response here:'
        else:
            suspects = "\nVULNERABILITY LIST:\n"
            row = cve_df_cmt[(cve_df_cmt['repo_name'] == split_pth[0].split('_')[0]) & (cve_df_cmt['commit_id'] == split_pth[1])]
            cves = ast.literal_eval(row['cves'].values[0])
            cwes = ast.literal_eval(row['cwes'].values[0])
            if len(cves) > 0:
                for id in cves:
                    try:
                        suspects = suspects + f"CVE_ID:{id}\nCVE Description:\n{cve_df_dsc[cve_df_dsc['CVE_ID'] == id]['CVE_DESCRIPTION'].values[0]}\n"
                    except Exception as e:
                        if type(e) != IndexError:
                            print(f"Exception {e} for {file_path}")
            if len(cwes) > 0:
                for id in cwes:
                    try:
                        cwe_id = int(re.findall(r'\d+', id)[0])
                        suspects = suspects + f"CWE_ID:{cwe_id}\nCWE Description:\n{cwe_df[cwe_df['CWE_ID'] == cwe_id]['CWE_DESCRIPTION'].values[0]}\n"
                    except Exception as e:
                        if type(e) != IndexError:
                            print(f"Exception {e} for {file_path}")

            return f'{value}\n{suspects}\nCode Sample:\n{code_sample}\n\nThis concludes your task. Provide your response here:'

    return value + '\n\n' + code_sample + '\n\nThis concludes your task. Provide your response here:'



def get_prompts(dir):
    messages = {}
    for root, dirs, files in os.walk(dir):
        for filename in files:
            file_path = os.path.join(root, filename)
            with open(file_path, 'r+', encoding='utf-8', errors="replace") as file:
                msg = file.read()
                messages[os.path.splitext(filename)[0]] = msg
                
    return messages

def main():
    # Prompt
    msgs = get_prompts(os.getenv('prompt_dir_hug'))

    # Limit the number of before/after file pairs to evaluate
    limit = int(os.getenv('huggingface_limit'))

    # Define the directory of samples you want to iterate through
    directory = os.getenv('sample_dir')

    # Define the models to run on groq
    model = os.getenv('huggingface_model')

    # print('-------------------')
    # print(msgs)
    # print('-------------------')
    # print(next(iter(msgs)) + ':\n' + msgs[next(iter(msgs))])
    # print('-------------------')
    # print(limit)
    # print('-------------------')
    # print(directory)
    # print('-------------------')
    # print(models)
    # print('-------------------')
    # return

    # Recursively iterate through the directory
    content = []
    cnt = 0
    for root, dirs, files in os.walk(directory):
        for filename in files:
            file_path = os.path.join(root, filename)
            with open(file_path, 'r+', encoding='utf-8', errors="replace") as file:
                code_sample = file.read()
            # print(f'File: {file_path}')
            match = re.search(r'([^\\/]+[\\/][^\\/]+)$', root)
            # print(f'root: {root}')
            # print(f'dir: {match.group(1)}')
            if ('attack' in filename.lower()) and ('cwe' in file_path.lower()):
                continue
            for key, value in msgs.items():
                content.append([match.group(1), filename, makePrompt(value, code_sample, match.group(1), key), key])
                
            cnt += 1
            if cnt == limit:
                break
        if cnt == limit:
            break

    print(f'Number of samples: {len(content)}')

    succeeded = ''
    
    print(f'Running model: {model}')
    succeeded = call_huggingface(content, model)
    # print(succeeded)

    # with open('huggingface/prompts.txt', 'w+') as file:
    #     for arr in content:
    #         file.write(arr[0]+arr[1] + '\n' + arr[2] + '\n\n' + '~'*10 + '\n')
    # with open('huggingface/results.txt','w+') as file:
    #     file.writelines(succeeded)
    model_pth = model.replace("\\", "_").replace("/", "_")
    with open(f'huggingface_outputs/fail_logs/{model_pth}_fail.txt','w+') as file:
        file.writelines(fail_log)

if __name__ == "__main__":
    main()