import os
from pathlib import Path
import dotenv
import requests
import re
from transformers import AutoTokenizer

dotenv.load_dotenv()

api_url = os.getenv('huggingface_url')
api_key = os.getenv('huggingface_key')
headers = {
    "Authorization": f"Bearer {api_key}",
    "Content-Type": "application/json",
    "x-wait-for-model": "true"
}

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

def call_huggingface(msg_content, model_name):
    for i in msg_content:
        pth_model_name = model_name.replace("\\","_").replace("/","_")
        output_file_path = f'huggingface_outputs/{os.getenv("test_dir")}/prompt_{i[3]}/{pth_model_name}/{i[0]}/{i[1]}.txt'
        if os.path.exists(output_file_path):
            continue

        try:
            data = {
                "inputs": i[2],
                "parameters": {
                    "max_new_tokens":256
                }
            }
            print(data["inputs"])
            response = requests.post(api_url, headers=headers, json=data)
            if response.status_code not in range(200,300):
                raise Exception(f"Bad Status Code: {response.status_code}")
            response = response.json()
            resp_txt = response[0]['generated_text'].replace(i[2],"") 
            # tokenCounter(model_name, i[2], response)

            os.makedirs(os.path.dirname(output_file_path), exist_ok=True)
            write_resp(output_file_path,resp_txt)

        except Exception as e:
            print("An error occurred:", e)
            print(f'Exception on file: {i[0]}/{i[1]}')
            return False

    return True

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
                content.append([match.group(1), filename, value + '\n\n' + code_sample + '\n\nThis concludes your task. Provide your response here:', key])
                
            cnt += 1
            if cnt == limit:
                break
        if cnt == limit:
            break

    print(f'Number of samples: {len(content)}')

    print(f'Running model: {model}')
    succeeded = call_huggingface(content, model)
    if not succeeded:
        print('Failed in model call')

if __name__ == "__main__":
    main()