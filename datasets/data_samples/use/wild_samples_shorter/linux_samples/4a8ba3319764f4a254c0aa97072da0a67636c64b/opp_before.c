/* Lock to allow exclusive modification to the device and opp lists */
static DEFINE_MUTEX(dev_opp_list_lock);

/**
 * find_device_opp() - find device_opp struct using device pointer
 * @dev:	device pointer used to lookup device OPPs
 *
 * This function returns the number of available opps if there are any,
 * else returns 0 if none or the corresponding error value.
 *
 * Locking: This function must be called under rcu_read_lock(). This function
 * internally references two RCU protected structures: device_opp and opp which
 * are safe as long as we are under a common RCU locked section.
 */
int dev_pm_opp_get_opp_count(struct device *dev)
{
	struct device_opp *dev_opp;
	struct dev_pm_opp *temp_opp;
	int count = 0;

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp)) {
		int r = PTR_ERR(dev_opp);
		dev_err(dev, "%s: device OPP not found (%d)\n", __func__, r);
		return r;
	}

	list_for_each_entry_rcu(temp_opp, &dev_opp->opp_list, node) {
		if (temp_opp->available)
			count++;
	}

	return count;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_get_opp_count);

	struct device_opp *dev_opp;
	struct dev_pm_opp *temp_opp, *opp = ERR_PTR(-ERANGE);

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp)) {
		int r = PTR_ERR(dev_opp);
		dev_err(dev, "%s: device OPP not found (%d)\n", __func__, r);
	struct device_opp *dev_opp;
	struct dev_pm_opp *temp_opp, *opp = ERR_PTR(-ERANGE);

	if (!dev || !freq) {
		dev_err(dev, "%s: Invalid argument freq=%p\n", __func__, freq);
		return ERR_PTR(-EINVAL);
	}
	struct device_opp *dev_opp;
	struct dev_pm_opp *temp_opp, *opp = ERR_PTR(-ERANGE);

	if (!dev || !freq) {
		dev_err(dev, "%s: Invalid argument freq=%p\n", __func__, freq);
		return ERR_PTR(-EINVAL);
	}

	/* Check for existing list for 'dev' */
	dev_opp = find_device_opp(dev);
	if (WARN(IS_ERR(dev_opp), "%s: dev_opp: %ld\n", dev_name(dev),
		 PTR_ERR(dev_opp)))
		return;

	/* Hold our list modification lock here */
	mutex_lock(&dev_opp_list_lock);
