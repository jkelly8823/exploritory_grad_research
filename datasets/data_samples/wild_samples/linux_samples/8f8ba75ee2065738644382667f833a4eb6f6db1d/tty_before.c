{
	struct rfcomm_dev *dev;
	struct rfcomm_dev_list_req *dl;
	struct rfcomm_dev_info *di;
	int n = 0, size, err;
	u16 dev_num;

	BT_DBG("");

	if (get_user(dev_num, (u16 __user *) arg))
		return -EFAULT;

	if (!dev_num || dev_num > (PAGE_SIZE * 4) / sizeof(*di))
		return -EINVAL;

	size = sizeof(*dl) + dev_num * sizeof(*di);

	dl = kmalloc(size, GFP_KERNEL);
	if (!dl)
		return -ENOMEM;

	di = dl->dev_info;

	spin_lock(&rfcomm_dev_lock);

	list_for_each_entry(dev, &rfcomm_dev_list, list) {
		if (test_bit(RFCOMM_TTY_RELEASED, &dev->flags))
			continue;
		(di + n)->id      = dev->id;
		(di + n)->flags   = dev->flags;
		(di + n)->state   = dev->dlc->state;
		(di + n)->channel = dev->channel;
		bacpy(&(di + n)->src, &dev->src);
		bacpy(&(di + n)->dst, &dev->dst);
		if (++n >= dev_num)
			break;
	}