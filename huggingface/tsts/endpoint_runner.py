import requests

# Define the endpoint and the token
# endpoint = "https://v3mpasi5ydiiklpc.us-east-1.aws.endpoints.huggingface.cloud"
# endpoint = "https://c5ia7r4hdvs0ec4f.us-east-1.aws.endpoints.huggingface.cloud"
endpoint = "https://ibn9tyxe9i39kcoq.us-east-1.aws.endpoints.huggingface.cloud"
token = "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"  # Replace with your actual Hugging Face token


with open(r'datasets\data_samples\use\cwe_samples\1298\1298_Bad_1.v', "r") as file:
    sample = file.read()
with open(r'prompts\run_hug\prompt1_v3.txt', "r") as file:
    prompt = file.read()
# Define the input data
input_data = {
    "inputs": prompt + '\n\n' + sample + '\n\nThis concludes your task. Provide your response here:',
    "parameters": {
        "return_full_text": False,
        # "max_length":8192,
        "echo":False,
        # "max_new_tokens":256
    }
}

# Set up the headers, including the authorization token
headers = {
    "Authorization": f"Bearer {token}",
    "Content-Type": "application/json"
}

# Send the POST request
response = requests.post(endpoint, headers=headers, json=input_data)

# Check if the request was successful
if response.status_code == 200:
    # Print the response from the API
    print("Response:", response.json()[0]['generated_text'])
else:
    print("Error:", response.status_code, response.text)