	if (!dev->power.early_init) {
		spin_lock_init(&dev->power.lock);
		dev->power.qos = NULL;
		dev->power.early_init = true;
	}
{
	return container_of(entry, struct device, power.entry);
}

extern void device_pm_sleep_init(struct device *dev);
extern void device_pm_add(struct device *);
extern void device_pm_remove(struct device *);
extern void device_pm_move_before(struct device *, struct device *);
extern void device_pm_move_after(struct device *, struct device *);
extern void device_pm_move_last(struct device *);

#else /* !CONFIG_PM_SLEEP */

static inline void device_pm_sleep_init(struct device *dev) {}

static inline void device_pm_add(struct device *dev) {}

static inline void device_pm_remove(struct device *dev)
{
	pm_runtime_remove(dev);
}

static inline void device_pm_move_before(struct device *deva,
					 struct device *devb) {}
static inline void device_pm_move_after(struct device *deva,
					struct device *devb) {}
static inline void device_pm_move_last(struct device *dev) {}

#endif /* !CONFIG_PM_SLEEP */

static inline void device_pm_init(struct device *dev)
{
	device_pm_init_common(dev);
	device_pm_sleep_init(dev);
	pm_runtime_init(dev);
}

#ifdef CONFIG_PM

/*
 * sysfs.c
 */

extern int dpm_sysfs_add(struct device *dev);
extern void dpm_sysfs_remove(struct device *dev);
extern void rpm_sysfs_remove(struct device *dev);
extern int wakeup_sysfs_add(struct device *dev);
extern void wakeup_sysfs_remove(struct device *dev);
extern int pm_qos_sysfs_add_latency(struct device *dev);
extern void pm_qos_sysfs_remove_latency(struct device *dev);
extern int pm_qos_sysfs_add_flags(struct device *dev);
extern void pm_qos_sysfs_remove_flags(struct device *dev);

#else /* CONFIG_PM */

static inline int dpm_sysfs_add(struct device *dev) { return 0; }
static inline void dpm_sysfs_remove(struct device *dev) {}
static inline void rpm_sysfs_remove(struct device *dev) {}
static inline int wakeup_sysfs_add(struct device *dev) { return 0; }
static inline void wakeup_sysfs_remove(struct device *dev) {}
static inline int pm_qos_sysfs_add(struct device *dev) { return 0; }
static inline void pm_qos_sysfs_remove(struct device *dev) {}

#endif