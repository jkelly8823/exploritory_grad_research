# import transformers
# import torch

# model_id = "meta-llama/Meta-Llama-3.1-8B-Instruct"

# pipeline = transformers.pipeline(
#     "text-generation",
#     model=model_id,
#     model_kwargs={"torch_dtype": torch.bfloat16},
#     device_map="auto",
# )

# messages = [
#     {"role": "system", "content": "You are a pirate chatbot who always responds in pirate speak!"},
#     {"role": "user", "content": "Who are you?"},
# ]

# outputs = pipeline(
#     messages,
#     max_new_tokens=256,
# )
# print(outputs[0]["generated_text"][-1])

import torch
from transformers import pipeline

pipe = pipeline(
    "text-generation",
    model="google/gemma-2-2b",
    device="cpu",  # replace with "mps" to run on a Mac device
)

text = "Once upon a time,"
outputs = pipe(text, max_new_tokens=256)
response = outputs[0]["generated_text"]
print(response)
