		return -ENODEV;
}

static int ata_acpi_find_dummy(struct device *dev, acpi_handle *handle)
{
	return -ENODEV;
}

static struct acpi_bus_type ata_acpi_bus = {
	.find_bridge = ata_acpi_find_dummy,
	.find_device = ata_acpi_find_device,
};

int ata_acpi_register(void)