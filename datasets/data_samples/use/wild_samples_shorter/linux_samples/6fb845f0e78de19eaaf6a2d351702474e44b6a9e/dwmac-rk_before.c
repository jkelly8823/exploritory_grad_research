	}

	ret = phy_power_on(bsp_priv, true);
	if (ret)
		return ret;

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
