struct usbhid_device {
	struct hid_device *hid;						/* pointer to corresponding HID dev */

	struct usb_interface *intf;                                     /* USB interface */
	int ifnum;                                                      /* USB interface number */

	unsigned int bufsize;                                           /* URB buffer size */

	struct urb *urbin;                                              /* Input URB */
	char *inbuf;                                                    /* Input buffer */
	dma_addr_t inbuf_dma;                                           /* Input buffer dma */

	struct urb *urbctrl;                                            /* Control URB */
	struct usb_ctrlrequest *cr;                                     /* Control request struct */
	struct hid_control_fifo ctrl[HID_CONTROL_FIFO_SIZE];  		/* Control fifo */
	unsigned char ctrlhead, ctrltail;                               /* Control fifo head & tail */
	char *ctrlbuf;                                                  /* Control buffer */
	dma_addr_t ctrlbuf_dma;                                         /* Control buffer dma */
	unsigned long last_ctrl;						/* record of last output for timeouts */

	struct urb *urbout;                                             /* Output URB */
	struct hid_output_fifo out[HID_CONTROL_FIFO_SIZE];              /* Output pipe fifo */
	unsigned char outhead, outtail;                                 /* Output pipe fifo head & tail */
	char *outbuf;                                                   /* Output buffer */
	dma_addr_t outbuf_dma;                                          /* Output buffer dma */
	unsigned long last_out;							/* record of last output for timeouts */

	spinlock_t lock;						/* fifo spinlock */
	unsigned long iofl;                                             /* I/O flags (CTRL_RUNNING, OUT_RUNNING) */
	struct timer_list io_retry;                                     /* Retry timer */
	unsigned long stop_retry;                                       /* Time to give up, in jiffies */
	unsigned int retry_delay;                                       /* Delay length in ms */
	struct work_struct reset_work;                                  /* Task context for resets */
	wait_queue_head_t wait;						/* For sleeping */
	int ledcount;							/* counting the number of active leds */

	struct work_struct led_work;					/* Task context for setting LEDs */
};

#define	hid_to_usb_dev(hid_dev) \
	container_of(hid_dev->dev.parent->parent, struct usb_device, dev)

#endif
