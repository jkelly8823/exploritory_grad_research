static __u8 easypen_m610x_rdesc_fixed[] = {
	0x06, 0x00, 0xFF,             /*  Usage Page (FF00h),             */
	0x09, 0x01,                   /*  Usage (01h),                    */
	0xA1, 0x01,                   /*  Collection (Application),       */
	0x85, 0x05,                   /*    Report ID (5),                */
	0x09, 0x01,                   /*    Usage (01h),                  */
	0x15, 0x80,                   /*    Logical Minimum (-128),       */
	0x25, 0x7F,                   /*    Logical Maximum (127),        */
	0x75, 0x08,                   /*    Report Size (8),              */
	0x95, 0x07,                   /*    Report Count (7),             */
	0xB1, 0x02,                   /*    Feature (Variable),           */
	0xC0,                         /*  End Collection,                 */
	0x05, 0x0D,                   /*  Usage Page (Digitizer),         */
	0x09, 0x02,                   /*  Usage (Pen),                    */
	0xA1, 0x01,                   /*  Collection (Application),       */
	0x85, 0x10,                   /*    Report ID (16),               */
	0x09, 0x20,                   /*    Usage (Stylus),               */
	0xA0,                         /*    Collection (Physical),        */
	0x14,                         /*      Logical Minimum (0),        */
	0x25, 0x01,                   /*      Logical Maximum (1),        */
	0x75, 0x01,                   /*      Report Size (1),            */
	0x09, 0x42,                   /*      Usage (Tip Switch),         */
	0x09, 0x44,                   /*      Usage (Barrel Switch),      */
	0x09, 0x46,                   /*      Usage (Tablet Pick),        */
	0x95, 0x03,                   /*      Report Count (3),           */
	0x81, 0x02,                   /*      Input (Variable),           */
	0x95, 0x04,                   /*      Report Count (4),           */
	0x81, 0x03,                   /*      Input (Constant, Variable), */
	0x09, 0x32,                   /*      Usage (In Range),           */
	0x95, 0x01,                   /*      Report Count (1),           */
	0x81, 0x02,                   /*      Input (Variable),           */
	0x75, 0x10,                   /*      Report Size (16),           */
	0x95, 0x01,                   /*      Report Count (1),           */
	0xA4,                         /*      Push,                       */
	0x05, 0x01,                   /*      Usage Page (Desktop),       */
	0x55, 0xFD,                   /*      Unit Exponent (-3),         */
	0x65, 0x13,                   /*      Unit (Inch),                */
	0x34,                         /*      Physical Minimum (0),       */
	0x09, 0x30,                   /*      Usage (X),                  */
	0x46, 0x10, 0x27,             /*      Physical Maximum (10000),   */
	0x27, 0x00, 0xA0, 0x00, 0x00, /*      Logical Maximum (40960),    */
	0x81, 0x02,                   /*      Input (Variable),           */
	0x09, 0x31,                   /*      Usage (Y),                  */
	0x46, 0x6A, 0x18,             /*      Physical Maximum (6250),    */
	0x26, 0x00, 0x64,             /*      Logical Maximum (25600),    */
	0x81, 0x02,                   /*      Input (Variable),           */
	0xB4,                         /*      Pop,                        */
	0x09, 0x30,                   /*      Usage (Tip Pressure),       */
	0x26, 0xFF, 0x03,             /*      Logical Maximum (1023),     */
	0x81, 0x02,                   /*      Input (Variable),           */
	0xC0,                         /*    End Collection,               */
	0xC0,                         /*  End Collection,                 */
	0x05, 0x0C,                   /*  Usage Page (Consumer),          */
	0x09, 0x01,                   /*  Usage (Consumer Control),       */
	0xA1, 0x01,                   /*  Collection (Application),       */
	0x85, 0x12,                   /*    Report ID (18),               */
	0x14,                         /*    Logical Minimum (0),          */
	0x25, 0x01,                   /*    Logical Maximum (1),          */
	0x75, 0x01,                   /*    Report Size (1),              */
	0x95, 0x04,                   /*    Report Count (4),             */
	0x0A, 0x1A, 0x02,             /*    Usage (AC Undo),              */
	0x0A, 0x79, 0x02,             /*    Usage (AC Redo Or Repeat),    */
	0x0A, 0x2D, 0x02,             /*    Usage (AC Zoom In),           */
	0x0A, 0x2E, 0x02,             /*    Usage (AC Zoom Out),          */
	0x81, 0x02,                   /*    Input (Variable),             */
	0x95, 0x01,                   /*    Report Count (1),             */
	0x75, 0x14,                   /*    Report Size (20),             */
	0x81, 0x03,                   /*    Input (Constant, Variable),   */
	0x75, 0x20,                   /*    Report Size (32),             */
	0x81, 0x03,                   /*    Input (Constant, Variable),   */
	0xC0                          /*  End Collection                  */
};

static __u8 *kye_consumer_control_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize, int offset, const char *device_name) {
	/*
	 * the fixup that need to be done:
	 *   - change Usage Maximum in the Comsumer Control
	 *     (report ID 3) to a reasonable value
	 */
	if (*rsize >= offset + 31 &&
	    /* Usage Page (Consumer Devices) */
	    rdesc[offset] == 0x05 && rdesc[offset + 1] == 0x0c &&
	    /* Usage (Consumer Control) */
	    rdesc[offset + 2] == 0x09 && rdesc[offset + 3] == 0x01 &&
	    /*   Usage Maximum > 12287 */
	    rdesc[offset + 10] == 0x2a && rdesc[offset + 12] > 0x2f) {
		hid_info(hdev, "fixing up %s report descriptor\n", device_name);
		rdesc[offset + 12] = 0x2f;
	}
	return rdesc;
}
		if (*rsize == EASYPEN_M610X_RDESC_ORIG_SIZE) {
			rdesc = easypen_m610x_rdesc_fixed;
			*rsize = sizeof(easypen_m610x_rdesc_fixed);
		}
		break;
	case USB_DEVICE_ID_GENIUS_GILA_GAMING_MOUSE:
		rdesc = kye_consumer_control_fixup(hdev, rdesc, rsize, 104,
					"Genius Gila Gaming Mouse");
		break;
	case USB_DEVICE_ID_GENIUS_GX_IMPERATOR:
		rdesc = kye_consumer_control_fixup(hdev, rdesc, rsize, 83,
					"Genius Gx Imperator Keyboard");
		break;
	}
	return rdesc;
}

/**
 * Enable fully-functional tablet mode by setting a special feature report.
 *
 * @hdev:	HID device
 *
 * The specific report ID and data were discovered by sniffing the
 * Windows driver traffic.
 */
static int kye_tablet_enable(struct hid_device *hdev)
{
	struct list_head *list;
	struct list_head *head;
	struct hid_report *report;
	__s32 *value;

	list = &hdev->report_enum[HID_FEATURE_REPORT].report_list;
	list_for_each(head, list) {
		report = list_entry(head, struct hid_report, list);
		if (report->id == 5)
			break;
	}

	if (head == list) {
		hid_err(hdev, "tablet-enabling feature report not found\n");
		return -ENODEV;
	}

	if (report->maxfield < 1 || report->field[0]->report_count < 7) {
		hid_err(hdev, "invalid tablet-enabling feature report\n");
		return -ENODEV;
	}

	value = report->field[0]->value;

	value[0] = 0x12;
	value[1] = 0x10;
	value[2] = 0x11;
	value[3] = 0x12;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	hid_hw_request(hdev, report, HID_REQ_SET_REPORT);

	return 0;
}

static int kye_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err;
	}

	switch (id->product) {
	case USB_DEVICE_ID_KYE_EASYPEN_I405X:
	case USB_DEVICE_ID_KYE_MOUSEPEN_I608X:
	case USB_DEVICE_ID_KYE_EASYPEN_M610X:
		ret = kye_tablet_enable(hdev);
		if (ret) {
			hid_err(hdev, "tablet enabling failed\n");
			goto enabling_err;
		}
static const struct hid_device_id kye_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE, USB_DEVICE_ID_KYE_ERGO_525V) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_I405X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_MOUSEPEN_I608X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_KYE_EASYPEN_M610X) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_GENIUS_GILA_GAMING_MOUSE) },
	{ HID_USB_DEVICE(USB_VENDOR_ID_KYE,
				USB_DEVICE_ID_GENIUS_GX_IMPERATOR) },
	{ }
};
MODULE_DEVICE_TABLE(hid, kye_devices);

static struct hid_driver kye_driver = {
	.name = "kye",
	.id_table = kye_devices,
	.probe = kye_probe,
	.report_fixup = kye_report_fixup,
};
module_hid_driver(kye_driver);

MODULE_LICENSE("GPL");