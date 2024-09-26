import transformers
import torch

model_id = "meta-llama/Meta-Llama-3.1-8B-Instruct"
auth_token = "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"  # Replace with your actual Hugging Face token

pipeline = transformers.pipeline(
    "text-generation",
    model=model_id,
    model_kwargs={"torch_dtype": torch.bfloat16},
    device_map="auto",
    use_auth_token=auth_token  # Add the auth token here
)

messages = [
    {"role": "system", "content": "You are a pirate chatbot who always responds in pirate speak!"},
    {"role": "user", "content": "Who are you?"},
]

outputs = pipeline(
    messages,
    max_new_tokens=256,
)

print(outputs[0]["generated_text"][-1])