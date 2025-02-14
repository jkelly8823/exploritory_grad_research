union bpf_attr;
struct io_uring_params;
struct clone_args;
struct open_how;

#include <linux/types.h>
#include <linux/aio_abi.h>
#include <linux/capability.h>
asmlinkage long sys_fchown(unsigned int fd, uid_t user, gid_t group);
asmlinkage long sys_openat(int dfd, const char __user *filename, int flags,
			   umode_t mode);
asmlinkage long sys_openat2(int dfd, const char __user *filename,
			    struct open_how *how, size_t size);
asmlinkage long sys_close(unsigned int fd);
asmlinkage long sys_vhangup(void);

/* fs/pipe.c */