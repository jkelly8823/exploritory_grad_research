import pandas as pd
import os

def scoreAll(df, source):
    if source == 'all':
        pass
    elif source == ('wild_samples'):
        df = df[df['SOURCE'] != 'cwe_samples']
    else:
        df = df[df['SOURCE'] == source]

    countCorrect = ((df['VULN_MODEL']) == (df['VULN_TRUTH'])).sum()
    countFalse = ((df['VULN_MODEL']) != (df['VULN_TRUTH'])).sum()
    total = countCorrect+countFalse
    score = countCorrect/(total)*100

    new_row = {'SOURCE': source, 'CORRECT': countCorrect, 'INCORRECT': countFalse, 'TOTAL': total, 'SCORE': score}
    return new_row

def main():
    cols = ['TRIAL', 'PROMPT', 'MODEL', 'SOURCE', 'CORRECT', 'INCORRECT', 'TOTAL', 'SCORE']
    combined_df = pd.DataFrame(columns=cols)

    for root, dirs, files in os.walk('analyzers/parsed'):
        for filename in files:
            if "huggingface" in filename:
                file_path = os.path.join(root, filename)
                with open(file_path, 'r+', encoding='utf-8', errors="replace") as file:
                    src_df = pd.read_csv(file_path, index_col='INDEX')
                    for trial in src_df['TRIAL'].unique().tolist():
                        trial_df = src_df[src_df['TRIAL'] == trial]

                        for prompt in trial_df['PROMPT'].unique().tolist():
                            prompt_df = trial_df[trial_df['PROMPT'] == prompt]

                            srcs = trial_df['SOURCE'].unique().tolist()

                            row = {'TRIAL':trial, 'PROMPT':prompt, 'MODEL': 'all'}
                            row.update(scoreAll(prompt_df, 'all'))
                            combined_df.loc[len(combined_df)] = row

                            row = {'TRIAL':trial, 'PROMPT':prompt, 'MODEL': 'all'}
                            row.update(scoreAll(prompt_df, 'wild_samples'))
                            combined_df.loc[len(combined_df)] = row
                           
                            for src in srcs:
                                row = {'TRIAL':trial, 'PROMPT':prompt, 'MODEL': 'all'}
                                row.update(scoreAll(prompt_df, src))
                                combined_df.loc[len(combined_df)] = row

                            for model in prompt_df['MODEL'].unique().tolist():
                                model_df = prompt_df[prompt_df['MODEL'] == model]

                                row = {'TRIAL':trial, 'PROMPT':prompt, 'MODEL': model}
                                row.update(scoreAll(model_df, 'all'))
                                combined_df.loc[len(combined_df)] = row

                                row = {'TRIAL':trial, 'PROMPT':prompt, 'MODEL': model}
                                row.update(scoreAll(model_df, 'wild_samples'))
                                combined_df.loc[len(combined_df)] = row

                                for src in srcs:
                                    row = {'TRIAL':trial, 'PROMPT':prompt, 'MODEL': model}
                                    row.update(scoreAll(model_df, src))
                                    combined_df.loc[len(combined_df)] = row

    combined_df.to_csv('analyzers/scored_outputs.csv',index=True,index_label='INDEX')
    print('Wrote output scores to csv')







if __name__ == "__main__":
    main()