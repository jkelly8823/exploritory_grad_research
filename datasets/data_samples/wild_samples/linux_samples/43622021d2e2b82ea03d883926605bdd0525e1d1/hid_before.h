struct hid_report {
	struct list_head list;
	unsigned id;					/* id of this report */
	unsigned type;					/* report type */
	struct hid_field *field[HID_MAX_FIELDS];	/* fields of the report */
	unsigned maxfield;				/* maximum valid field index */
	unsigned size;					/* size of the report (bits) */
	struct hid_device *device;			/* associated device */
};

struct hid_report_enum {
	unsigned numbered;
	struct list_head report_list;
	struct hid_report *report_id_hash[256];
};

#define HID_REPORT_TYPES 3

#define HID_MIN_BUFFER_SIZE	64		/* make sure there is at least a packet size of space */
#define HID_MAX_BUFFER_SIZE	4096		/* 4kb */
#define HID_CONTROL_FIFO_SIZE	256		/* to init devices with >100 reports */
#define HID_OUTPUT_FIFO_SIZE	64

struct hid_control_fifo {
	unsigned char dir;
	struct hid_report *report;
	char *raw_report;
};

struct hid_output_fifo {
	struct hid_report *report;
	char *raw_report;
};

#define HID_CLAIMED_INPUT	1
#define HID_CLAIMED_HIDDEV	2
#define HID_CLAIMED_HIDRAW	4

#define HID_STAT_ADDED		1
#define HID_STAT_PARSED		2

struct hid_input {
	struct list_head list;
	struct hid_report *report;
	struct input_dev *input;
};

enum hid_type {
	HID_TYPE_OTHER = 0,
	HID_TYPE_USBMOUSE,
	HID_TYPE_USBNONE
};

struct hid_driver;
struct hid_ll_driver;

struct hid_device {							/* device report descriptor */
	__u8 *dev_rdesc;
	unsigned dev_rsize;
	__u8 *rdesc;
	unsigned rsize;
	struct hid_collection *collection;				/* List of HID collections */
	unsigned collection_size;					/* Number of allocated hid_collections */
	unsigned maxcollection;						/* Number of parsed collections */
	unsigned maxapplication;					/* Number of applications */
	__u16 bus;							/* BUS ID */
	__u16 group;							/* Report group */
	__u32 vendor;							/* Vendor ID */
	__u32 product;							/* Product ID */
	__u32 version;							/* HID version */
	enum hid_type type;						/* device type (mouse, kbd, ...) */
	unsigned country;						/* HID country */
	struct hid_report_enum report_enum[HID_REPORT_TYPES];

	struct semaphore driver_lock;					/* protects the current driver, except during input */
	struct semaphore driver_input_lock;				/* protects the current driver */
	struct device dev;						/* device */
	struct hid_driver *driver;
	struct hid_ll_driver *ll_driver;

#ifdef CONFIG_HID_BATTERY_STRENGTH
	/*
	 * Power supply information for HID devices which report
	 * battery strength. power_supply is registered iff
	 * battery.name is non-NULL.
	 */
	struct power_supply battery;
	__s32 battery_min;
	__s32 battery_max;
	__s32 battery_report_type;
	__s32 battery_report_id;
#endif

	unsigned int status;						/* see STAT flags above */
	unsigned claimed;						/* Claimed by hidinput, hiddev? */
	unsigned quirks;						/* Various quirks the device can pull on us */
	bool io_started;						/* Protected by driver_lock. If IO has started */

	struct list_head inputs;					/* The list of inputs */
	void *hiddev;							/* The hiddev structure */
	void *hidraw;
	int minor;							/* Hiddev minor number */

	int open;							/* is the device open by anyone? */
	char name[128];							/* Device name */
	char phys[64];							/* Device physical location */
	char uniq[64];							/* Device unique identifier (serial #) */

	void *driver_data;

	/* temporary hid_ff handling (until moved to the drivers) */
	int (*ff_init)(struct hid_device *);

	/* hiddev event handler */
	int (*hiddev_connect)(struct hid_device *, unsigned int);
	void (*hiddev_disconnect)(struct hid_device *);
	void (*hiddev_hid_event) (struct hid_device *, struct hid_field *field,
				  struct hid_usage *, __s32);
	void (*hiddev_report_event) (struct hid_device *, struct hid_report *);

	/* handler for raw input (Get_Report) data, used by hidraw */
	int (*hid_get_raw_report) (struct hid_device *, unsigned char, __u8 *, size_t, unsigned char);

	/* handler for raw output data, used by hidraw */
	int (*hid_output_raw_report) (struct hid_device *, __u8 *, size_t, unsigned char);

	/* debugging support via debugfs */
	unsigned short debug;
	struct dentry *debug_dir;
	struct dentry *debug_rdesc;
	struct dentry *debug_events;
	struct list_head debug_list;
	spinlock_t  debug_list_lock;
	wait_queue_head_t debug_wait;
};

static inline void *hid_get_drvdata(struct hid_device *hdev)
{
	return dev_get_drvdata(&hdev->dev);
}

static inline void hid_set_drvdata(struct hid_device *hdev, void *data)
{
	dev_set_drvdata(&hdev->dev, data);
}

#define HID_GLOBAL_STACK_SIZE 4
#define HID_COLLECTION_STACK_SIZE 4

struct hid_parser {
	struct hid_global     global;
	struct hid_global     global_stack[HID_GLOBAL_STACK_SIZE];
	unsigned              global_stack_ptr;
	struct hid_local      local;
	unsigned              collection_stack[HID_COLLECTION_STACK_SIZE];
	unsigned              collection_stack_ptr;
	struct hid_device    *device;
};

struct hid_class_descriptor {
	__u8  bDescriptorType;
	__le16 wDescriptorLength;
} __attribute__ ((packed));

struct hid_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 bcdHID;
	__u8  bCountryCode;
	__u8  bNumDescriptors;

	struct hid_class_descriptor desc[1];
} __attribute__ ((packed));

#define HID_DEVICE(b, g, ven, prod)					\
	.bus = (b), .group = (g), .vendor = (ven), .product = (prod)
#define HID_USB_DEVICE(ven, prod)				\
	.bus = BUS_USB, .vendor = (ven), .product = (prod)
#define HID_BLUETOOTH_DEVICE(ven, prod)					\
	.bus = BUS_BLUETOOTH, .vendor = (ven), .product = (prod)

#define HID_REPORT_ID(rep) \
	.report_type = (rep)
#define HID_USAGE_ID(uhid, utype, ucode) \
	.usage_hid = (uhid), .usage_type = (utype), .usage_code = (ucode)
/* we don't want to catch types and codes equal to 0 */
#define HID_TERMINATOR		(HID_ANY_ID - 1)

struct hid_report_id {
	__u32 report_type;
};
struct hid_usage_id {
	__u32 usage_hid;
	__u32 usage_type;
	__u32 usage_code;
};

/**
 * struct hid_driver
 * @name: driver name (e.g. "Footech_bar-wheel")
 * @id_table: which devices is this driver for (must be non-NULL for probe
 * 	      to be called)
 * @dyn_list: list of dynamically added device ids
 * @dyn_lock: lock protecting @dyn_list
 * @probe: new device inserted
 * @remove: device removed (NULL if not a hot-plug capable driver)
 * @report_table: on which reports to call raw_event (NULL means all)
 * @raw_event: if report in report_table, this hook is called (NULL means nop)
 * @usage_table: on which events to call event (NULL means all)
 * @event: if usage in usage_table, this hook is called (NULL means nop)
 * @report: this hook is called after parsing a report (NULL means nop)
 * @report_fixup: called before report descriptor parsing (NULL means nop)
 * @input_mapping: invoked on input registering before mapping an usage
 * @input_mapped: invoked on input registering after mapping an usage
 * @input_configured: invoked just before the device is registered
 * @feature_mapping: invoked on feature registering
 * @suspend: invoked on suspend (NULL means nop)
 * @resume: invoked on resume if device was not reset (NULL means nop)
 * @reset_resume: invoked on resume if device was reset (NULL means nop)
 *
 * probe should return -errno on error, or 0 on success. During probe,
 * input will not be passed to raw_event unless hid_device_io_start is
 * called.
 *
 * raw_event and event should return 0 on no action performed, 1 when no
 * further processing should be done and negative on error
 *
 * input_mapping shall return a negative value to completely ignore this usage
 * (e.g. doubled or invalid usage), zero to continue with parsing of this
 * usage by generic code (no special handling needed) or positive to skip
 * generic parsing (needed special handling which was done in the hook already)
 * input_mapped shall return negative to inform the layer that this usage
 * should not be considered for further processing or zero to notify that
 * no processing was performed and should be done in a generic manner
 * Both these functions may be NULL which means the same behavior as returning
 * zero from them.
 */
struct hid_driver {
	char *name;
	const struct hid_device_id *id_table;

	struct list_head dyn_list;
	spinlock_t dyn_lock;

	int (*probe)(struct hid_device *dev, const struct hid_device_id *id);
	void (*remove)(struct hid_device *dev);

	const struct hid_report_id *report_table;
	int (*raw_event)(struct hid_device *hdev, struct hid_report *report,
			u8 *data, int size);
	const struct hid_usage_id *usage_table;
	int (*event)(struct hid_device *hdev, struct hid_field *field,
			struct hid_usage *usage, __s32 value);
	void (*report)(struct hid_device *hdev, struct hid_report *report);

	__u8 *(*report_fixup)(struct hid_device *hdev, __u8 *buf,
			unsigned int *size);

	int (*input_mapping)(struct hid_device *hdev,
			struct hid_input *hidinput, struct hid_field *field,
			struct hid_usage *usage, unsigned long **bit, int *max);
	int (*input_mapped)(struct hid_device *hdev,
			struct hid_input *hidinput, struct hid_field *field,
			struct hid_usage *usage, unsigned long **bit, int *max);
	void (*input_configured)(struct hid_device *hdev,
				 struct hid_input *hidinput);
	void (*feature_mapping)(struct hid_device *hdev,
			struct hid_field *field,
			struct hid_usage *usage);
#ifdef CONFIG_PM
	int (*suspend)(struct hid_device *hdev, pm_message_t message);
	int (*resume)(struct hid_device *hdev);
	int (*reset_resume)(struct hid_device *hdev);
#endif
/* private: */
	struct device_driver driver;
};

/**
 * hid_ll_driver - low level driver callbacks
 * @start: called on probe to start the device
 * @stop: called on remove
 * @open: called by input layer on open
 * @close: called by input layer on close
 * @hidinput_input_event: event input event (e.g. ff or leds)
 * @parse: this method is called only once to parse the device data,
 *	   shouldn't allocate anything to not leak memory
 * @request: send report request to device (e.g. feature report)
 * @wait: wait for buffered io to complete (send/recv reports)
 * @idle: send idle request to device
 */
struct hid_ll_driver {
	int (*start)(struct hid_device *hdev);
	void (*stop)(struct hid_device *hdev);

	int (*open)(struct hid_device *hdev);
	void (*close)(struct hid_device *hdev);

	int (*power)(struct hid_device *hdev, int level);

	int (*hidinput_input_event) (struct input_dev *idev, unsigned int type,
			unsigned int code, int value);

	int (*parse)(struct hid_device *hdev);

	void (*request)(struct hid_device *hdev,
			struct hid_report *report, int reqtype);

	int (*wait)(struct hid_device *hdev);
	int (*idle)(struct hid_device *hdev, int report, int idle, int reqtype);

};

#define	PM_HINT_FULLON	1<<5
#define PM_HINT_NORMAL	1<<1

/* Applications from HID Usage Tables 4/8/99 Version 1.1 */
/* We ignore a few input applications that are not widely used */
#define IS_INPUT_APPLICATION(a) (((a >= 0x00010000) && (a <= 0x00010008)) || (a == 0x00010080) || (a == 0x000c0001) || ((a >= 0x000d0002) && (a <= 0x000d0006)))

/* HID core API */

extern int hid_debug;

extern bool hid_ignore(struct hid_device *);
extern int hid_add_device(struct hid_device *);
extern void hid_destroy_device(struct hid_device *);

extern int __must_check __hid_register_driver(struct hid_driver *,
		struct module *, const char *mod_name);

/* use a define to avoid include chaining to get THIS_MODULE & friends */
#define hid_register_driver(driver) \
	__hid_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)

extern void hid_unregister_driver(struct hid_driver *);

/**
 * module_hid_driver() - Helper macro for registering a HID driver
 * @__hid_driver: hid_driver struct
 *
 * Helper macro for HID drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_hid_driver(__hid_driver) \
	module_driver(__hid_driver, hid_register_driver, \
		      hid_unregister_driver)

extern void hidinput_hid_event(struct hid_device *, struct hid_field *, struct hid_usage *, __s32);
extern void hidinput_report_event(struct hid_device *hid, struct hid_report *report);
extern int hidinput_connect(struct hid_device *hid, unsigned int force);
extern void hidinput_disconnect(struct hid_device *);

int hid_set_field(struct hid_field *, unsigned, __s32);
int hid_input_report(struct hid_device *, int type, u8 *, int, int);
int hidinput_find_field(struct hid_device *hid, unsigned int type, unsigned int code, struct hid_field **field);
struct hid_field *hidinput_get_led_field(struct hid_device *hid);
unsigned int hidinput_count_leds(struct hid_device *hid);
__s32 hidinput_calc_abs_res(const struct hid_field *field, __u16 code);
void hid_output_report(struct hid_report *report, __u8 *data);
struct hid_device *hid_allocate_device(void);
struct hid_report *hid_register_report(struct hid_device *device, unsigned type, unsigned id);
int hid_parse_report(struct hid_device *hid, __u8 *start, unsigned size);
int hid_open_report(struct hid_device *device);
int hid_check_keys_pressed(struct hid_device *hid);
int hid_connect(struct hid_device *hid, unsigned int connect_mask);
void hid_disconnect(struct hid_device *hid);
const struct hid_device_id *hid_match_id(struct hid_device *hdev,
					 const struct hid_device_id *id);
s32 hid_snto32(__u32 value, unsigned n);

/**
 * hid_device_io_start - enable HID input during probe, remove
 *
 * @hid - the device
 *
 * This should only be called during probe or remove and only be
 * called by the thread calling probe or remove. It will allow
 * incoming packets to be delivered to the driver.
 */
static inline void hid_device_io_start(struct hid_device *hid) {
	if (hid->io_started) {
		dev_warn(&hid->dev, "io already started");
		return;
	}
	hid->io_started = true;
	up(&hid->driver_input_lock);
}

/**
 * hid_device_io_stop - disable HID input during probe, remove
 *
 * @hid - the device
 *
 * Should only be called after hid_device_io_start. It will prevent
 * incoming packets from going to the driver for the duration of
 * probe, remove. If called during probe, packets will still go to the
 * driver after probe is complete. This function should only be called
 * by the thread calling probe or remove.
 */
static inline void hid_device_io_stop(struct hid_device *hid) {
	if (!hid->io_started) {
		dev_warn(&hid->dev, "io already stopped");
		return;
	}
	hid->io_started = false;
	down(&hid->driver_input_lock);
}

/**
 * hid_map_usage - map usage input bits
 *
 * @hidinput: hidinput which we are interested in
 * @usage: usage to fill in
 * @bit: pointer to input->{}bit (out parameter)
 * @max: maximal valid usage->code to consider later (out parameter)
 * @type: input event type (EV_KEY, EV_REL, ...)
 * @c: code which corresponds to this usage and type
 */
static inline void hid_map_usage(struct hid_input *hidinput,
		struct hid_usage *usage, unsigned long **bit, int *max,
		__u8 type, __u16 c)
{
	struct input_dev *input = hidinput->input;

	usage->type = type;
	usage->code = c;

	switch (type) {
	case EV_ABS:
		*bit = input->absbit;
		*max = ABS_MAX;
		break;
	case EV_REL:
		*bit = input->relbit;
		*max = REL_MAX;
		break;
	case EV_KEY:
		*bit = input->keybit;
		*max = KEY_MAX;
		break;
	case EV_LED:
		*bit = input->ledbit;
		*max = LED_MAX;
		break;
	}
}

/**
 * hid_map_usage_clear - map usage input bits and clear the input bit
 *
 * The same as hid_map_usage, except the @c bit is also cleared in supported
 * bits (@bit).
 */
static inline void hid_map_usage_clear(struct hid_input *hidinput,
		struct hid_usage *usage, unsigned long **bit, int *max,
		__u8 type, __u16 c)
{
	hid_map_usage(hidinput, usage, bit, max, type, c);
	clear_bit(c, *bit);
}

/**
 * hid_parse - parse HW reports
 *
 * @hdev: hid device
 *
 * Call this from probe after you set up the device (if needed). Your
 * report_fixup will be called (if non-NULL) after reading raw report from
 * device before passing it to hid layer for real parsing.
 */
static inline int __must_check hid_parse(struct hid_device *hdev)
{
	return hid_open_report(hdev);
}

/**
 * hid_hw_start - start underlaying HW
 *
 * @hdev: hid device
 * @connect_mask: which outputs to connect, see HID_CONNECT_*
 *
 * Call this in probe function *after* hid_parse. This will setup HW buffers
 * and start the device (if not deffered to device open). hid_hw_stop must be
 * called if this was successful.
 */
static inline int __must_check hid_hw_start(struct hid_device *hdev,
		unsigned int connect_mask)
{
	int ret = hdev->ll_driver->start(hdev);
	if (ret || !connect_mask)
		return ret;
	ret = hid_connect(hdev, connect_mask);
	if (ret)
		hdev->ll_driver->stop(hdev);
	return ret;
}

/**
 * hid_hw_stop - stop underlaying HW
 *
 * @hdev: hid device
 *
 * This is usually called from remove function or from probe when something
 * failed and hid_hw_start was called already.
 */
static inline void hid_hw_stop(struct hid_device *hdev)
{
	hid_disconnect(hdev);
	hdev->ll_driver->stop(hdev);
}

/**
 * hid_hw_open - signal underlaying HW to start delivering events
 *
 * @hdev: hid device
 *
 * Tell underlying HW to start delivering events from the device.
 * This function should be called sometime after successful call
 * to hid_hiw_start().
 */
static inline int __must_check hid_hw_open(struct hid_device *hdev)
{
	return hdev->ll_driver->open(hdev);
}

/**
 * hid_hw_close - signal underlaying HW to stop delivering events
 *
 * @hdev: hid device
 *
 * This function indicates that we are not interested in the events
 * from this device anymore. Delivery of events may or may not stop,
 * depending on the number of users still outstanding.
 */
static inline void hid_hw_close(struct hid_device *hdev)
{
	hdev->ll_driver->close(hdev);
}

/**
 * hid_hw_power - requests underlying HW to go into given power mode
 *
 * @hdev: hid device
 * @level: requested power level (one of %PM_HINT_* defines)
 *
 * This function requests underlying hardware to enter requested power
 * mode.
 */

static inline int hid_hw_power(struct hid_device *hdev, int level)
{
	return hdev->ll_driver->power ? hdev->ll_driver->power(hdev, level) : 0;
}


/**
 * hid_hw_request - send report request to device
 *
 * @hdev: hid device
 * @report: report to send
 * @reqtype: hid request type
 */
static inline void hid_hw_request(struct hid_device *hdev,
				  struct hid_report *report, int reqtype)
{
	if (hdev->ll_driver->request)
		hdev->ll_driver->request(hdev, report, reqtype);
}

/**
 * hid_hw_idle - send idle request to device
 *
 * @hdev: hid device
 * @report: report to control
 * @idle: idle state
 * @reqtype: hid request type
 */
static inline int hid_hw_idle(struct hid_device *hdev, int report, int idle,
		int reqtype)
{
	if (hdev->ll_driver->idle)
		return hdev->ll_driver->idle(hdev, report, idle, reqtype);

	return 0;
}

/**
 * hid_hw_wait - wait for buffered io to complete
 *
 * @hdev: hid device
 */
static inline void hid_hw_wait(struct hid_device *hdev)
{
	if (hdev->ll_driver->wait)
		hdev->ll_driver->wait(hdev);
}

int hid_report_raw_event(struct hid_device *hid, int type, u8 *data, int size,
		int interrupt);

/* HID quirks API */
u32 usbhid_lookup_quirk(const u16 idVendor, const u16 idProduct);
int usbhid_quirks_init(char **quirks_param);
void usbhid_quirks_exit(void);
void usbhid_set_leds(struct hid_device *hid);

#ifdef CONFIG_HID_PID
int hid_pidff_init(struct hid_device *hid);
#else
#define hid_pidff_init NULL
#endif

#define dbg_hid(format, arg...)						\
do {									\
	if (hid_debug)							\
		printk(KERN_DEBUG "%s: " format, __FILE__, ##arg);	\
} while (0)

#define hid_printk(level, hid, fmt, arg...)		\
	dev_printk(level, &(hid)->dev, fmt, ##arg)
#define hid_emerg(hid, fmt, arg...)			\
	dev_emerg(&(hid)->dev, fmt, ##arg)
#define hid_crit(hid, fmt, arg...)			\
	dev_crit(&(hid)->dev, fmt, ##arg)
#define hid_alert(hid, fmt, arg...)			\
	dev_alert(&(hid)->dev, fmt, ##arg)
#define hid_err(hid, fmt, arg...)			\
	dev_err(&(hid)->dev, fmt, ##arg)
#define hid_notice(hid, fmt, arg...)			\
	dev_notice(&(hid)->dev, fmt, ##arg)
#define hid_warn(hid, fmt, arg...)			\
	dev_warn(&(hid)->dev, fmt, ##arg)
#define hid_info(hid, fmt, arg...)			\
	dev_info(&(hid)->dev, fmt, ##arg)
#define hid_dbg(hid, fmt, arg...)			\
	dev_dbg(&(hid)->dev, fmt, ##arg)

#endif