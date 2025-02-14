			if (port_mask & i) {
				hso_dev = hso_create_mux_serial_device(
						interface, i, shared_int);
				if (!hso_dev)
					goto exit;
			}
		}

		if (tmp_dev)
			hso_dev = tmp_dev;
		break;

	case HSO_INTF_BULK:
		/* It's a regular bulk interface */
		if ((port_spec & HSO_PORT_MASK) == HSO_PORT_NETWORK) {
			if (!disable_net)
				hso_dev =
				    hso_create_net_device(interface, port_spec);
		} else {
			hso_dev =
			    hso_create_bulk_serial_device(interface, port_spec);
		}
		if (!hso_dev)
			goto exit;
		break;
	default:
		goto exit;
	}