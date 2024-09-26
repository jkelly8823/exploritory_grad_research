prev_fails = []
with open("huggingface/gemma1_fail.txt","r+") as file:
    prev_fails = [line.strip() for line in file.readlines()]
print(prev_fails)
tmp = True if "cpython_samples\52b940803860e37bcc3f6096b2d24e7c20a0e807/nametab_before.h" in prev_fails else False
print(tmp)

