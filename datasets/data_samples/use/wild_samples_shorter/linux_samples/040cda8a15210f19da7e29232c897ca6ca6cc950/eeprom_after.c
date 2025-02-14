	dev_info(dev->mt76.dev, "EEPROM ver:%02hhx fae:%02hhx\n",
		 version, fae);

	memcpy(dev->mt76.macaddr, (u8 *)dev->mt76.eeprom.data + MT_EE_MAC_ADDR,
	       ETH_ALEN);
	mt76_eeprom_override(&dev->mt76);
	mt76x02_mac_setaddr(dev, dev->mt76.macaddr);

	mt76x0_set_chip_cap(dev);
	mt76x0_set_freq_offset(dev);
	mt76x0_set_temp_offset(dev);
