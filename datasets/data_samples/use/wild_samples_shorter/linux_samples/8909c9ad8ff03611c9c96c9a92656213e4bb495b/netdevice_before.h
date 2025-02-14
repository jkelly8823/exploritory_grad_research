extern int netdev_info(const struct net_device *dev, const char *format, ...)
	__attribute__ ((format (printf, 2, 3)));

#if defined(DEBUG)
#define netdev_dbg(__dev, format, args...)			\
	netdev_printk(KERN_DEBUG, __dev, format, ##args)
#elif defined(CONFIG_DYNAMIC_DEBUG)