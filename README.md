# Grad Research

This repository contains tools and scripts for conducting research with the GROQ API. Follow the instructions below to set up your environment and run the analysis.

## Setup

1. **Environment Configuration**
   - Create a `.env` file in the top-level directory with the following entries:

     ```plaintext
     groq_key=               # Your GROQ API key
     groq_limit=             # Number of files to run, -1 to run all
     groq_models=            # Array of model names (e.g., '["llama-3.1-70b-versatile", "gemma2-9b-it"]')

     sample_dir=datasets/data_samples  # Directory containing your samples
     prompt_dir=prompts/run            # Directory containing your prompts
     test_dir=run_name                 # Subdirectory to save results from this GROQ test
     parser_dir=groq_outputs           # Directory holding LLM outputs to parse
     ```

2. **Running the Scripts**
   - From the top-level directory, run the following command to execute the GROQ runner:

     ```bash
     python groq/groq_runner.py
     ```

   - After that, run the output parser:

     ```bash
     python analyzers/output_parser.py
     ```

## Directories Overview

- **datasets/data_samples**: Contains your input samples.
- **prompts**: Holds the prompts used for the analysis.
- **initial_run_5**: Subdirectory for storing results from the current GROQ test.
- **groq_outputs**: Contains the outputs from the LLM to be parsed.
- **llm_rag**: Contains files from other student to potentially be used as a template.

## Notes

- Ensure you have all necessary dependencies installed before running the scripts.
- Adjust the parameters in the `.env` file according to your specific requirements.
