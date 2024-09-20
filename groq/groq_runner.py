import os
from pathlib import Path
import dotenv
from groq import Groq
import re
import json

dotenv.load_dotenv()

client1 = Groq(
    api_key = os.getenv('groq_key')
)
client2 = Groq(
    api_key = os.getenv('groq_key2')
)

def write_resp(pth,resp):
    try:
        with open(pth,"w+", encoding='utf-8', errors="replace") as file:
            file.write(resp)
    except Exception as e:
        print(f'Failed to write file {pth} with exception: {e}')

def call_groq(msg_content, model_name):
    use_client1 = True
    client1_failed = False
    use_client2 = False
    client2_failed = False

    for i in msg_content:
        output_file_path = f'groq_outputs/{os.getenv("test_dir")}/prompt_{i[3]}/{model_name}/{i[0]}/{i[1]}.txt'
        if os.path.exists(output_file_path):
            continue

        msg = [{"role": "user", "content": i[2]}]

        while True:
            try:
                # Attempt with client 1 first
                if use_client1:
                    client1_failed = True
                    chat_completion = client1.chat.completions.create(messages=msg, model=model_name)
                    client1_failed = False  # Reset on success
                elif use_client2:
                    client2_failed = True
                    chat_completion = client2.chat.completions.create(messages=msg, model=model_name)
                    client2_failed = False  # Reset on success

                os.makedirs(os.path.dirname(output_file_path), exist_ok=True)
                write_resp(output_file_path, chat_completion.choices[0].message.content)
                break  # Break out of the retry loop if successful

            except Exception as e:
                error_message = str(e).lower()
                if 'rate limit' in error_message or '429' in error_message:
                    print("Rate limit exceeded:", e)
                    if client1_failed and client2_failed:
                        print("Both clients hit rate limit. Returning False.")
                        print(f"Failed file: {output_file_path}")
                        return False
                    if use_client1:
                        print("Client 1 rate limit hit. Switching to Client 2.")
                        use_client1 = False  # Mark client 1 as failed
                        use_client2 = True
                    elif use_client2:
                        print("Client 2 rate limit hit. Switching back to Client 1.")
                        use_client1 = True  # Mark client 1 as failed
                        use_client2 = False
                    else:  # Both clients have hit the rate limit
                        print("Unhandled and unknown rate limit error.")
                        return False
                else:
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
    msgs = get_prompts(os.getenv('prompt_dir'))

    # Limit the number of before/after file pairs to evaluate
    limit = int(os.getenv('groq_limit'))

    # Define the directory of samples you want to iterate through
    directory = os.getenv('sample_dir')

    # Define the models to run on groq
    models = json.loads(os.getenv('groq_models'))

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
                content.append([match.group(1), filename, value + '\n\n' + code_sample, key])
                
            cnt += 1
            if cnt == limit:
                break
        if cnt == limit:
            break

    print(f'Number of samples: {len(content)}')

    for model in models:
        print(f'Running model: {model}')
        succeeded = call_groq(content, model)
        if not succeeded:
            print('Failed in model call')
            break

if __name__ == "__main__":
    main()