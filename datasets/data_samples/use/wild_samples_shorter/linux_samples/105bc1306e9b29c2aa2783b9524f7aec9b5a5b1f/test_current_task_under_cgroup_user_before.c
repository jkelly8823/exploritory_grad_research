#include <unistd.h>
#include <bpf/bpf.h>
#include "bpf_load.h"
#include <linux/bpf.h>
#include "cgroup_helpers.h"

#define CGROUP_PATH		"/my-cgroup"
