	[LOCKDOWN_DEBUGFS] = "debugfs access",
	[LOCKDOWN_XMON_WR] = "xmon write access",
	[LOCKDOWN_BPF_WRITE_USER] = "use of bpf to write user RAM",
	[LOCKDOWN_INTEGRITY_MAX] = "integrity",
	[LOCKDOWN_KCORE] = "/proc/kcore access",
	[LOCKDOWN_KPROBES] = "use of kprobes",
	[LOCKDOWN_BPF_READ_KERNEL] = "use of bpf to read kernel RAM",
	[LOCKDOWN_PERF] = "unsafe use of perf",
	[LOCKDOWN_TRACEFS] = "use of tracefs",
	[LOCKDOWN_XMON_RW] = "xmon read and write access",
	[LOCKDOWN_XFRM_SECRET] = "xfrm SA secret",