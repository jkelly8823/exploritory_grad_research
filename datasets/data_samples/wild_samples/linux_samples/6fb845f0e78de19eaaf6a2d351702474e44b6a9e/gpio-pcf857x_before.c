struct pcf857x {
	struct gpio_chip	chip;
	struct i2c_client	*client;
	struct mutex		lock;		/* protect 'out' */
	unsigned		out;		/* software latch */
	unsigned		status;		/* current status */
	unsigned int		irq_parent;
	unsigned		irq_enabled;	/* enabled irqs */

	int (*write)(struct i2c_client *client, unsigned data);
	int (*read)(struct i2c_client *client);
};

/*-------------------------------------------------------------------------*/

/* Talk to 8-bit I/O expander */

static int i2c_write_le8(struct i2c_client *client, unsigned data)
{
	return i2c_smbus_write_byte(client, data);
}

static int i2c_read_le8(struct i2c_client *client)
{
	return (int)i2c_smbus_read_byte(client);
}

/* Talk to 16-bit I/O expander */

static int i2c_write_le16(struct i2c_client *client, unsigned word)
{
	u8 buf[2] = { word & 0xff, word >> 8, };
	int status;

	status = i2c_master_send(client, buf, 2);
	return (status < 0) ? status : 0;
}

static int i2c_read_le16(struct i2c_client *client)
{
	u8 buf[2];
	int status;

	status = i2c_master_recv(client, buf, 2);
	if (status < 0)
		return status;
	return (buf[1] << 8) | buf[0];
}

/*-------------------------------------------------------------------------*/

static int pcf857x_input(struct gpio_chip *chip, unsigned offset)
{
	struct pcf857x	*gpio = gpiochip_get_data(chip);
	int		status;

	mutex_lock(&gpio->lock);
	gpio->out |= (1 << offset);
	status = gpio->write(gpio->client, gpio->out);
	mutex_unlock(&gpio->lock);

	return status;
}

static int pcf857x_get(struct gpio_chip *chip, unsigned offset)
{
	struct pcf857x	*gpio = gpiochip_get_data(chip);
	int		value;

	value = gpio->read(gpio->client);
	return (value < 0) ? value : !!(value & (1 << offset));
}

static int pcf857x_output(struct gpio_chip *chip, unsigned offset, int value)
{
	struct pcf857x	*gpio = gpiochip_get_data(chip);
	unsigned	bit = 1 << offset;
	int		status;

	mutex_lock(&gpio->lock);
	if (value)
		gpio->out |= bit;
	else
		gpio->out &= ~bit;
	status = gpio->write(gpio->client, gpio->out);
	mutex_unlock(&gpio->lock);

	return status;
}

static void pcf857x_set(struct gpio_chip *chip, unsigned offset, int value)
{
	pcf857x_output(chip, offset, value);
}

/*-------------------------------------------------------------------------*/

static irqreturn_t pcf857x_irq(int irq, void *data)
{
	struct pcf857x  *gpio = data;
	unsigned long change, i, status;

	status = gpio->read(gpio->client);

	/*
	 * call the interrupt handler iff gpio is used as
	 * interrupt source, just to avoid bad irqs
	 */
	mutex_lock(&gpio->lock);
	change = (gpio->status ^ status) & gpio->irq_enabled;
	gpio->status = status;
	mutex_unlock(&gpio->lock);

	for_each_set_bit(i, &change, gpio->chip.ngpio)
		handle_nested_irq(irq_find_mapping(gpio->chip.irq.domain, i));

	return IRQ_HANDLED;
}

/*
 * NOP functions
 */
static void noop(struct irq_data *data) { }
{
	struct pcf857x *gpio = irq_data_get_irq_chip_data(data);

	mutex_unlock(&gpio->lock);
}

static struct irq_chip pcf857x_irq_chip = {
	.name		= "pcf857x",
	.irq_enable	= pcf857x_irq_enable,
	.irq_disable	= pcf857x_irq_disable,
	.irq_ack	= noop,
	.irq_mask	= noop,
	.irq_unmask	= noop,
	.irq_set_wake	= pcf857x_irq_set_wake,
	.irq_bus_lock		= pcf857x_irq_bus_lock,
	.irq_bus_sync_unlock	= pcf857x_irq_bus_sync_unlock,
};

/*-------------------------------------------------------------------------*/

static int pcf857x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct pcf857x_platform_data	*pdata = dev_get_platdata(&client->dev);
	struct device_node		*np = client->dev.of_node;
	struct pcf857x			*gpio;
	unsigned int			n_latch = 0;
	int				status;

	if (IS_ENABLED(CONFIG_OF) && np)
		of_property_read_u32(np, "lines-initial-states", &n_latch);
	else if (pdata)
		n_latch = pdata->n_latch;
	else
		dev_dbg(&client->dev, "no platform data\n");

	/* Allocate, initialize, and register this gpio_chip. */
	gpio = devm_kzalloc(&client->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	mutex_init(&gpio->lock);

	gpio->chip.base			= pdata ? pdata->gpio_base : -1;
	gpio->chip.can_sleep		= true;
	gpio->chip.parent		= &client->dev;
	gpio->chip.owner		= THIS_MODULE;
	gpio->chip.get			= pcf857x_get;
	gpio->chip.set			= pcf857x_set;
	gpio->chip.direction_input	= pcf857x_input;
	gpio->chip.direction_output	= pcf857x_output;
	gpio->chip.ngpio		= id->driver_data;

	/* NOTE:  the OnSemi jlc1562b is also largely compatible with
	 * these parts, notably for output.  It has a low-resolution
	 * DAC instead of pin change IRQs; and its inputs can be the
	 * result of comparators.
	 */

	/* 8574 addresses are 0x20..0x27; 8574a uses 0x38..0x3f;
	 * 9670, 9672, 9764, and 9764a use quite a variety.
	 *
	 * NOTE: we don't distinguish here between *4 and *4a parts.
	 */
	if (gpio->chip.ngpio == 8) {
		gpio->write	= i2c_write_le8;
		gpio->read	= i2c_read_le8;

		if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE))
			status = -EIO;

		/* fail if there's no chip present */
		else
			status = i2c_smbus_read_byte(client);

	/* '75/'75c addresses are 0x20..0x27, just like the '74;
	 * the '75c doesn't have a current source pulling high.
	 * 9671, 9673, and 9765 use quite a variety of addresses.
	 *
	 * NOTE: we don't distinguish here between '75 and '75c parts.
	 */
	} else if (gpio->chip.ngpio == 16) {
		gpio->write	= i2c_write_le16;
		gpio->read	= i2c_read_le16;

		if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
			status = -EIO;

		/* fail if there's no chip present */
		else
			status = i2c_read_le16(client);

	} else {
		dev_dbg(&client->dev, "unsupported number of gpios\n");
		status = -EINVAL;
	}

	if (status < 0)
		goto fail;

	gpio->chip.label = client->name;

	gpio->client = client;
	i2c_set_clientdata(client, gpio);

	/* NOTE:  these chips have strange "quasi-bidirectional" I/O pins.
	 * We can't actually know whether a pin is configured (a) as output
	 * and driving the signal low, or (b) as input and reporting a low
	 * value ... without knowing the last value written since the chip
	 * came out of reset (if any).  We can't read the latched output.
	 *
	 * In short, the only reliable solution for setting up pin direction
	 * is to do it explicitly.  The setup() method can do that, but it
	 * may cause transient glitching since it can't know the last value
	 * written (some pins may need to be driven low).
	 *
	 * Using n_latch avoids that trouble.  When left initialized to zero,
	 * our software copy of the "latch" then matches the chip's all-ones
	 * reset state.  Otherwise it flags pins to be driven low.
	 */
	gpio->out = ~n_latch;
	gpio->status = gpio->out;

	status = devm_gpiochip_add_data(&client->dev, &gpio->chip, gpio);
	if (status < 0)
		goto fail;

	/* Enable irqchip if we have an interrupt */
	if (client->irq) {
		status = gpiochip_irqchip_add_nested(&gpio->chip,
						     &pcf857x_irq_chip,
						     0, handle_level_irq,
						     IRQ_TYPE_NONE);
		if (status) {
			dev_err(&client->dev, "cannot add irqchip\n");
			goto fail;
		}

		status = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, pcf857x_irq, IRQF_ONESHOT |
					IRQF_TRIGGER_FALLING | IRQF_SHARED,
					dev_name(&client->dev), gpio);
		if (status)
			goto fail;

		gpiochip_set_nested_irqchip(&gpio->chip, &pcf857x_irq_chip,
					    client->irq);
		gpio->irq_parent = client->irq;
	}

	/* Let platform code set up the GPIOs and their users.
	 * Now is the first time anyone could use them.
	 */
	if (pdata && pdata->setup) {
		status = pdata->setup(client,
				gpio->chip.base, gpio->chip.ngpio,
				pdata->context);
		if (status < 0)
			dev_warn(&client->dev, "setup --> %d\n", status);
	}

	dev_info(&client->dev, "probed\n");

	return 0;

fail:
	dev_dbg(&client->dev, "probe error %d for '%s'\n", status,
		client->name);

	return status;
}

static int pcf857x_remove(struct i2c_client *client)
{
	struct pcf857x_platform_data	*pdata = dev_get_platdata(&client->dev);
	struct pcf857x			*gpio = i2c_get_clientdata(client);
	int				status = 0;

	if (pdata && pdata->teardown) {
		status = pdata->teardown(client,
				gpio->chip.base, gpio->chip.ngpio,
				pdata->context);
		if (status < 0) {
			dev_err(&client->dev, "%s --> %d\n",
					"teardown", status);
			return status;
		}
	}

	return status;
}

static void pcf857x_shutdown(struct i2c_client *client)
{
	struct pcf857x *gpio = i2c_get_clientdata(client);

	/* Drive all the I/O lines high */
	gpio->write(gpio->client, BIT(gpio->chip.ngpio) - 1);
}

static struct i2c_driver pcf857x_driver = {
	.driver = {
		.name	= "pcf857x",
		.of_match_table = of_match_ptr(pcf857x_of_table),
	},
	.probe	= pcf857x_probe,
	.remove	= pcf857x_remove,
	.shutdown = pcf857x_shutdown,
	.id_table = pcf857x_id,
};

static int __init pcf857x_init(void)
{
	return i2c_add_driver(&pcf857x_driver);
}
/* register after i2c postcore initcall and before
 * subsys initcalls that may rely on these GPIOs
 */
subsys_initcall(pcf857x_init);

static void __exit pcf857x_exit(void)
{
	i2c_del_driver(&pcf857x_driver);
}
module_exit(pcf857x_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Brownell");
	} else {
		dev_dbg(&client->dev, "unsupported number of gpios\n");
		status = -EINVAL;
	}

	if (status < 0)
		goto fail;

	gpio->chip.label = client->name;

	gpio->client = client;
	i2c_set_clientdata(client, gpio);

	/* NOTE:  these chips have strange "quasi-bidirectional" I/O pins.
	 * We can't actually know whether a pin is configured (a) as output
	 * and driving the signal low, or (b) as input and reporting a low
	 * value ... without knowing the last value written since the chip
	 * came out of reset (if any).  We can't read the latched output.
	 *
	 * In short, the only reliable solution for setting up pin direction
	 * is to do it explicitly.  The setup() method can do that, but it
	 * may cause transient glitching since it can't know the last value
	 * written (some pins may need to be driven low).
	 *
	 * Using n_latch avoids that trouble.  When left initialized to zero,
	 * our software copy of the "latch" then matches the chip's all-ones
	 * reset state.  Otherwise it flags pins to be driven low.
	 */
	gpio->out = ~n_latch;
	gpio->status = gpio->out;

	status = devm_gpiochip_add_data(&client->dev, &gpio->chip, gpio);
	if (status < 0)
		goto fail;

	/* Enable irqchip if we have an interrupt */
	if (client->irq) {
		status = gpiochip_irqchip_add_nested(&gpio->chip,
						     &pcf857x_irq_chip,
						     0, handle_level_irq,
						     IRQ_TYPE_NONE);
		if (status) {
			dev_err(&client->dev, "cannot add irqchip\n");
			goto fail;
		}

		status = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, pcf857x_irq, IRQF_ONESHOT |
					IRQF_TRIGGER_FALLING | IRQF_SHARED,
					dev_name(&client->dev), gpio);
		if (status)
			goto fail;

		gpiochip_set_nested_irqchip(&gpio->chip, &pcf857x_irq_chip,
					    client->irq);
		gpio->irq_parent = client->irq;
	}
		if (status) {
			dev_err(&client->dev, "cannot add irqchip\n");
			goto fail;
		}

		status = devm_request_threaded_irq(&client->dev, client->irq,
					NULL, pcf857x_irq, IRQF_ONESHOT |
					IRQF_TRIGGER_FALLING | IRQF_SHARED,
					dev_name(&client->dev), gpio);
		if (status)
			goto fail;

		gpiochip_set_nested_irqchip(&gpio->chip, &pcf857x_irq_chip,
					    client->irq);
		gpio->irq_parent = client->irq;
	}

	/* Let platform code set up the GPIOs and their users.
	 * Now is the first time anyone could use them.
	 */
	if (pdata && pdata->setup) {
		status = pdata->setup(client,
				gpio->chip.base, gpio->chip.ngpio,
				pdata->context);
		if (status < 0)
			dev_warn(&client->dev, "setup --> %d\n", status);
	}

	dev_info(&client->dev, "probed\n");

	return 0;

fail:
	dev_dbg(&client->dev, "probe error %d for '%s'\n", status,
		client->name);

	return status;
}

static int pcf857x_remove(struct i2c_client *client)
{
	struct pcf857x_platform_data	*pdata = dev_get_platdata(&client->dev);
	struct pcf857x			*gpio = i2c_get_clientdata(client);
	int				status = 0;

	if (pdata && pdata->teardown) {
		status = pdata->teardown(client,
				gpio->chip.base, gpio->chip.ngpio,
				pdata->context);
		if (status < 0) {
			dev_err(&client->dev, "%s --> %d\n",
					"teardown", status);
			return status;
		}