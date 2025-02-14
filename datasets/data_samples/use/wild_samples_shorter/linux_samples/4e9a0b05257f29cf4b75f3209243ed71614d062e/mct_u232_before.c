
static int mct_u232_port_probe(struct usb_serial_port *port)
{
	struct mct_u232_private *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* Use second interrupt-in endpoint for reading. */
	priv->read_urb = port->serial->port[1]->interrupt_in_urb;
	priv->read_urb->context = port;

	spin_lock_init(&priv->lock);
