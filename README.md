# Grad Research

This repository contains tools and scripts for conducting research with the GROQ API. Follow the instructions below to set up your environment and run the analysis.

## Setup

1. **Environment Configuration**
   - Ensure that python3 and pip are installed
   - Install dependencies from `requirements.txt`
   - Create a `.env` file in the top-level directory with the following entries:

    ```plaintext
    groq_key=               # Your GROQ API key
    groq_key2=              # GROQ API key for a second account to avoid rate limits
    groq_key3=              # GROQ API key for a third account to avoid rate limits
    groq_limit=             # Number of files to run, -1 to run all
    groq_models=            # Array of model names (e.g., '["llama-3.1-70b-versatile", "gemma2-9b-it"]')

    huggingface_key=         # Your huggingface key
    huggingface_model=       # Name of first model, I used google/gemma-2-9b-it
    huggingface_url=         # Inference endpoint url for model
    huggingface_model2=      # Name of second model, I used meta-llama/Meta-Llama-3.1-8B-Instruct
    huggingface_url2=        # Inference endpoint url for model
    huggingface_model3=      # Name of second model, I used LeroyDyer/Mixtral_AI_CyberCoder_7b
    huggingface_url3=        # Inference endpoint url for model
    huggingface_limit=       # Number of files to run, -1 to run all

    sample_dir=datasets/data_samples    # Directory containing your samples
    prompt_dir_groq=prompts/run_groq    # Directory containing your prompts for groq
    prompt_dir_hug=prompts/run_hug      # Directory containing your prompts for huggingface
    test_dir=run_name                   # Subdirectory name to save results to from this test
    parser_dir=                         # Directory holding LLM outputs to parse

    github_src=datasets/data_descriptors/sampled_cve_commits.csv    # CSV containing flagged commits to pull from
    github_key=                                                     # Your github key
    github_limit=-1                                                 # Number of files to run, -1 to run all
    github_dir=datasets/data_samples/wild_samples_shorter           # Directory to save the collected files in
     ```

2. **Running the Scripts**
   - All commands are run from the top-level directory
   - Run the following command to execute the GROQ runner:

     ```bash
     python groq/groq_runner.py
     ```

   - Run the following command to execute the iterative Huggingface endpoint runner:

     ```bash
     python huggingface/hugging_runner.py
     ```

   - Run the following command to execute the parallelized Huggingface endpoint runners:

     ```bash
     python huggingface/hugging_runner.py
     python huggingface/hugging_runner2.py
     python huggingface/hugging_runner3.py
     ```

   - After that, run the output parser:

     ```bash
     python analyzers/output_parser.py
     ```

   - The notebook `analyzers/data_playbook.ipynb` has a few existing queries and filters to investigate the parsed outputs

## Directories Overview

- **analyzers**: Contains analyzers, and subdirectories for parsed outputs and their logs 
- **datasets/data_collectors**: Contains scripts to collect samples.
- **datasets/data_descriptors**: Contains csv files that describe and annotate the samples.
- **datasets/data_samples**: Contains all input samples.
- **datasets/data_samples/use**: Contains the input samples to be evaluated.
- **groq**: Contains the groq runner py file
- **groq_outputs**: Contains the outputs from the LLM to be parsed.
- **huggingface**: Contains the huggingface runner py files, and the `skip_fail.txt` file
- **huggingface_outputs**: Contains the outputs from the LLM to be parsed.
- **llm_rag**: Contains files from other student to potentially be used as a template in future work.
- **prompts**: Holds the prompt txt files.
- **prompts/run_groq**: Holds prompts to be ran against groq
- **prompts/run_hug**: Holds prompts to be ran against huggingface

## Notes

- Ensure you have all necessary dependencies installed before running the scripts.
- Adjust the parameters in the `.env` file according to your specific requirements.
- The iterative `hugging_runner.py` file is missing some of the features in the parallel files, as they were the ones used 
- `skip_fail.txt` is a manually edited file. It can be used to indicate specific files for the huggingface models to skip, such as `cpython_samples\52b940803860e37bcc3f6096b2d24e7c20a0e807/expat_after.h`