import os
import dotenv
from groq import Groq
import re
import json

dotenv.load_dotenv()

client = Groq(
    api_key = os.getenv('groq_key')
)

def write_resp(pth,resp):
    try:
        with open(pth,"w+") as file:
            file.write(resp)
    except Exception as e:
        print(f'Failed to write file {pth} with exception: {e}')

def call_groq(msg_content, model_name):

    for i in msg_content:
        msg = []
        msg.append({
            "role":"user",
            "content":i[2]
        })
        # print(msg, model_name)
        chat_completion = client.chat.completions.create(
            messages=msg,
            model=model_name,
        )

        # print(chat_completion.choices[0].message.content)
        os.makedirs(f'groq_outputs/{model_name}/{i[0]}', exist_ok=True)
        write_resp(f'groq_outputs/{model_name}/{i[0]}/{i[1]}.txt',chat_completion.choices[0].message.content)


def main():
    # Prompt
    msg= os.getenv('groq_prompt')

    # Limit the number of before/after file pairs to evaluate
    limit = int(os.getenv('groq_limit'))

    # Define the directory of samples you want to iterate through
    directory = './samples'

    # Define the models to run on groq
    models = json.loads(os.getenv('groq_models'))

    # Recursively iterate through the directory
    content = []
    cnt = 0
    for root, dirs, files in os.walk(directory):
        for filename in files:
            file_path = os.path.join(root, filename)
            with open(file_path, 'r+', encoding='utf-8') as file:
                code_sample = file.read()
            # print(f'File: {file_path}')
            match = re.search(r'([^\\]+\\[^\\]+)$', root)
            # print(f'root: {root}')
            # print(f'dir: {match.group(1)}')
            content.append([match.group(1), filename, msg + code_sample])
            
            cnt += 0.5
            if cnt == limit:
                break
        if cnt == limit:
            break

    for model in models:
        call_groq(content, model)

if __name__ == "__main__":
    main()