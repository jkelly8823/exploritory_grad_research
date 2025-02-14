// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat, Inc.  All rights reserved.
 *     Author: Alex Williamson <alex.williamson@redhat.com>
 *
 * Derived from original vfio:
 * Copyright 2010 Cisco Systems, Inc.  All rights reserved.
 * Author: Tom Lyon, pugs@cisco.com
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/device.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vfio.h>
#include <linux/vgaarb.h>
#include <linux/nospec.h>

#include "vfio_pci_private.h"

#define DRIVER_VERSION  "0.2"
#define DRIVER_AUTHOR   "Alex Williamson <alex.williamson@redhat.com>"
#define DRIVER_DESC     "VFIO PCI - User Level meta-driver"

static char ids[1024] __initdata;
module_param_string(ids, ids, sizeof(ids), 0);
MODULE_PARM_DESC(ids, "Initial PCI IDs to add to the vfio driver, format is \"vendor:device[:subvendor[:subdevice[:class[:class_mask]]]]\" and multiple comma separated entries can be specified");

static bool nointxmask;
module_param_named(nointxmask, nointxmask, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(nointxmask,
		  "Disable support for PCI 2.3 style INTx masking.  If this resolves problems for specific devices, report lspci -vvvxxx to linux-pci@vger.kernel.org so the device can be fixed automatically via the broken_intx_masking flag.");

#ifdef CONFIG_VFIO_PCI_VGA
static bool disable_vga;
module_param(disable_vga, bool, S_IRUGO);
MODULE_PARM_DESC(disable_vga, "Disable VGA resource access through vfio-pci");
#endif

static bool disable_idle_d3;
module_param(disable_idle_d3, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_idle_d3,
		 "Disable using the PCI D3 low power state for idle, unused devices");

static bool enable_sriov;
#ifdef CONFIG_PCI_IOV
module_param(enable_sriov, bool, 0644);
MODULE_PARM_DESC(enable_sriov, "Enable support for SR-IOV configuration.  Enabling SR-IOV on a PF typically requires support of the userspace PF driver, enabling VFs without such support may result in non-functional VFs or PF.");
#endif

static inline bool vfio_vga_disabled(void)
{
#ifdef CONFIG_VFIO_PCI_VGA
	return disable_vga;
#else
	return true;
#endif
}
						&dummy_res->resource)) {
				kfree(dummy_res);
				goto no_mmap;
			}
			dummy_res->index = bar;
			list_add(&dummy_res->res_next,
					&vdev->dummy_resources_list);
			vdev->bar_mmap_supported[bar] = true;
			continue;
		}
		/*
		 * Here we don't handle the case when the BAR is not page
		 * aligned because we can't expect the BAR will be
		 * assigned into the same location in a page in guest
		 * when we passthrough the BAR. And it's hard to access
		 * this BAR in userspace because we have no way to get
		 * the BAR's location in a page.
		 */
no_mmap:
		vdev->bar_mmap_supported[bar] = false;
	}
}

static void vfio_pci_try_bus_reset(struct vfio_pci_device *vdev);
static void vfio_pci_disable(struct vfio_pci_device *vdev);

/*
 * INTx masking requires the ability to disable INTx signaling via PCI_COMMAND
 * _and_ the ability detect when the device is asserting INTx via PCI_STATUS.
 * If a device implements the former but not the latter we would typically
 * expect broken_intx_masking be set and require an exclusive interrupt.
 * However since we do have control of the device's ability to assert INTx,
 * we can instead pretend that the device does not implement INTx, virtualizing
 * the pin register to report zero and maintaining DisINTx set on the host.
 */
static bool vfio_pci_nointx(struct pci_dev *pdev)
{
	switch (pdev->vendor) {
	case PCI_VENDOR_ID_INTEL:
		switch (pdev->device) {
		/* All i40e (XL710/X710/XXV710) 10/20/25/40GbE NICs */
		case 0x1572:
		case 0x1574:
		case 0x1580 ... 0x1581:
		case 0x1583 ... 0x158b:
		case 0x37d0 ... 0x37d2:
			return true;
		default:
			return false;
		}
	}

	return false;
}
{
	struct vfio_pci_region *region;

	region = krealloc(vdev->region,
			  (vdev->num_regions + 1) * sizeof(*region),
			  GFP_KERNEL);
	if (!region)
		return -ENOMEM;

	vdev->region = region;
	vdev->region[vdev->num_regions].type = type;
	vdev->region[vdev->num_regions].subtype = subtype;
	vdev->region[vdev->num_regions].ops = ops;
	vdev->region[vdev->num_regions].size = size;
	vdev->region[vdev->num_regions].flags = flags;
	vdev->region[vdev->num_regions].data = data;

	vdev->num_regions++;

	return 0;
}

static long vfio_pci_ioctl(void *device_data,
			   unsigned int cmd, unsigned long arg)
{
	struct vfio_pci_device *vdev = device_data;
	unsigned long minsz;

	if (cmd == VFIO_DEVICE_GET_INFO) {
		struct vfio_device_info info;

		minsz = offsetofend(struct vfio_device_info, num_irqs);

		if (copy_from_user(&info, (void __user *)arg, minsz))
			return -EFAULT;

		if (info.argsz < minsz)
			return -EINVAL;

		info.flags = VFIO_DEVICE_FLAGS_PCI;

		if (vdev->reset_works)
			info.flags |= VFIO_DEVICE_FLAGS_RESET;

		info.num_regions = VFIO_PCI_NUM_REGIONS + vdev->num_regions;
		info.num_irqs = VFIO_PCI_NUM_IRQS;

		return copy_to_user((void __user *)arg, &info, minsz) ?
			-EFAULT : 0;

	} else if (cmd == VFIO_DEVICE_GET_REGION_INFO) {
		struct pci_dev *pdev = vdev->pdev;
		struct vfio_region_info info;
		struct vfio_info_cap caps = { .buf = NULL, .size = 0 };
		int i, ret;

		minsz = offsetofend(struct vfio_region_info, offset);

		if (copy_from_user(&info, (void __user *)arg, minsz))
			return -EFAULT;

		if (info.argsz < minsz)
			return -EINVAL;

		switch (info.index) {
		case VFIO_PCI_CONFIG_REGION_INDEX:
			info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
			info.size = pdev->cfg_size;
			info.flags = VFIO_REGION_INFO_FLAG_READ |
				     VFIO_REGION_INFO_FLAG_WRITE;
			break;
		case VFIO_PCI_BAR0_REGION_INDEX ... VFIO_PCI_BAR5_REGION_INDEX:
			info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
			info.size = pci_resource_len(pdev, info.index);
			if (!info.size) {
				info.flags = 0;
				break;
			}

			info.flags = VFIO_REGION_INFO_FLAG_READ |
				     VFIO_REGION_INFO_FLAG_WRITE;
			if (vdev->bar_mmap_supported[info.index]) {
				info.flags |= VFIO_REGION_INFO_FLAG_MMAP;
				if (info.index == vdev->msix_bar) {
					ret = msix_mmappable_cap(vdev, &caps);
					if (ret)
						return ret;
				}
			}

			break;
		case VFIO_PCI_ROM_REGION_INDEX:
		{
			void __iomem *io;
			size_t size;
			u16 orig_cmd;

			info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
			info.flags = 0;

			/* Report the BAR size, not the ROM size */
			info.size = pci_resource_len(pdev, info.index);
			if (!info.size) {
				/* Shadow ROMs appear as PCI option ROMs */
				if (pdev->resource[PCI_ROM_RESOURCE].flags &
							IORESOURCE_ROM_SHADOW)
					info.size = 0x20000;
				else
					break;
			}

			/*
			 * Is it really there?  Enable memory decode for
			 * implicit access in pci_map_rom().
			 */
			pci_read_config_word(pdev, PCI_COMMAND, &orig_cmd);
			pci_write_config_word(pdev, PCI_COMMAND,
					      orig_cmd | PCI_COMMAND_MEMORY);

			io = pci_map_rom(pdev, &size);
			if (io) {
				info.flags = VFIO_REGION_INFO_FLAG_READ;
				pci_unmap_rom(pdev, io);
			} else {
				info.size = 0;
			}

			pci_write_config_word(pdev, PCI_COMMAND, orig_cmd);
			break;
		}
		case VFIO_PCI_VGA_REGION_INDEX:
			if (!vdev->has_vga)
				return -EINVAL;

			info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
			info.size = 0xc0000;
			info.flags = VFIO_REGION_INFO_FLAG_READ |
				     VFIO_REGION_INFO_FLAG_WRITE;

			break;
		default:
		{
			struct vfio_region_info_cap_type cap_type = {
					.header.id = VFIO_REGION_INFO_CAP_TYPE,
					.header.version = 1 };

			if (info.index >=
			    VFIO_PCI_NUM_REGIONS + vdev->num_regions)
				return -EINVAL;
			info.index = array_index_nospec(info.index,
							VFIO_PCI_NUM_REGIONS +
							vdev->num_regions);

			i = info.index - VFIO_PCI_NUM_REGIONS;

			info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
			info.size = vdev->region[i].size;
			info.flags = vdev->region[i].flags;

			cap_type.type = vdev->region[i].type;
			cap_type.subtype = vdev->region[i].subtype;

			ret = vfio_info_add_capability(&caps, &cap_type.header,
						       sizeof(cap_type));
			if (ret)
				return ret;

			if (vdev->region[i].ops->add_capability) {
				ret = vdev->region[i].ops->add_capability(vdev,
						&vdev->region[i], &caps);
				if (ret)
					return ret;
			}
		}
		}

		if (caps.size) {
			info.flags |= VFIO_REGION_INFO_FLAG_CAPS;
			if (info.argsz < sizeof(info) + caps.size) {
				info.argsz = sizeof(info) + caps.size;
				info.cap_offset = 0;
			} else {
				vfio_info_cap_shift(&caps, sizeof(info));
				if (copy_to_user((void __user *)arg +
						  sizeof(info), caps.buf,
						  caps.size)) {
					kfree(caps.buf);
					return -EFAULT;
				}
				info.cap_offset = sizeof(info);
			}

			kfree(caps.buf);
		}

		return copy_to_user((void __user *)arg, &info, minsz) ?
			-EFAULT : 0;

	} else if (cmd == VFIO_DEVICE_GET_IRQ_INFO) {
		struct vfio_irq_info info;

		minsz = offsetofend(struct vfio_irq_info, count);

		if (copy_from_user(&info, (void __user *)arg, minsz))
			return -EFAULT;

		if (info.argsz < minsz || info.index >= VFIO_PCI_NUM_IRQS)
			return -EINVAL;

		switch (info.index) {
		case VFIO_PCI_INTX_IRQ_INDEX ... VFIO_PCI_MSIX_IRQ_INDEX:
		case VFIO_PCI_REQ_IRQ_INDEX:
			break;
		case VFIO_PCI_ERR_IRQ_INDEX:
			if (pci_is_pcie(vdev->pdev))
				break;
		/* fall through */
		default:
			return -EINVAL;
		}

		info.flags = VFIO_IRQ_INFO_EVENTFD;

		info.count = vfio_pci_get_irq_count(vdev, info.index);

		if (info.index == VFIO_PCI_INTX_IRQ_INDEX)
			info.flags |= (VFIO_IRQ_INFO_MASKABLE |
				       VFIO_IRQ_INFO_AUTOMASKED);
		else
			info.flags |= VFIO_IRQ_INFO_NORESIZE;

		return copy_to_user((void __user *)arg, &info, minsz) ?
			-EFAULT : 0;

	} else if (cmd == VFIO_DEVICE_SET_IRQS) {
		struct vfio_irq_set hdr;
		u8 *data = NULL;
		int max, ret = 0;
		size_t data_size = 0;

		minsz = offsetofend(struct vfio_irq_set, count);

		if (copy_from_user(&hdr, (void __user *)arg, minsz))
			return -EFAULT;

		max = vfio_pci_get_irq_count(vdev, hdr.index);

		ret = vfio_set_irqs_validate_and_prepare(&hdr, max,
						 VFIO_PCI_NUM_IRQS, &data_size);
		if (ret)
			return ret;

		if (data_size) {
			data = memdup_user((void __user *)(arg + minsz),
					    data_size);
			if (IS_ERR(data))
				return PTR_ERR(data);
		}

		mutex_lock(&vdev->igate);

		ret = vfio_pci_set_irqs_ioctl(vdev, hdr.flags, hdr.index,
					      hdr.start, hdr.count, data);

		mutex_unlock(&vdev->igate);
		kfree(data);

		return ret;

	} else if (cmd == VFIO_DEVICE_RESET) {
		return vdev->reset_works ?
			pci_try_reset_function(vdev->pdev) : -EINVAL;

	} else if (cmd == VFIO_DEVICE_GET_PCI_HOT_RESET_INFO) {
		struct vfio_pci_hot_reset_info hdr;
		struct vfio_pci_fill_info fill = { 0 };
		struct vfio_pci_dependent_device *devices = NULL;
		bool slot = false;
		int ret = 0;

		minsz = offsetofend(struct vfio_pci_hot_reset_info, count);

		if (copy_from_user(&hdr, (void __user *)arg, minsz))
			return -EFAULT;

		if (hdr.argsz < minsz)
			return -EINVAL;

		hdr.flags = 0;

		/* Can we do a slot or bus reset or neither? */
		if (!pci_probe_reset_slot(vdev->pdev->slot))
			slot = true;
		else if (pci_probe_reset_bus(vdev->pdev->bus))
			return -ENODEV;

		/* How many devices are affected? */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_count_devs,
						    &fill.max, slot);
		if (ret)
			return ret;

		WARN_ON(!fill.max); /* Should always be at least one */

		/*
		 * If there's enough space, fill it now, otherwise return
		 * -ENOSPC and the number of devices affected.
		 */
		if (hdr.argsz < sizeof(hdr) + (fill.max * sizeof(*devices))) {
			ret = -ENOSPC;
			hdr.count = fill.max;
			goto reset_info_exit;
		}

		devices = kcalloc(fill.max, sizeof(*devices), GFP_KERNEL);
		if (!devices)
			return -ENOMEM;

		fill.devices = devices;

		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_fill_devs,
						    &fill, slot);

		/*
		 * If a device was removed between counting and filling,
		 * we may come up short of fill.max.  If a device was
		 * added, we'll have a return of -EAGAIN above.
		 */
		if (!ret)
			hdr.count = fill.cur;

reset_info_exit:
		if (copy_to_user((void __user *)arg, &hdr, minsz))
			ret = -EFAULT;

		if (!ret) {
			if (copy_to_user((void __user *)(arg + minsz), devices,
					 hdr.count * sizeof(*devices)))
				ret = -EFAULT;
		}

		kfree(devices);
		return ret;

	} else if (cmd == VFIO_DEVICE_PCI_HOT_RESET) {
		struct vfio_pci_hot_reset hdr;
		int32_t *group_fds;
		struct vfio_pci_group_entry *groups;
		struct vfio_pci_group_info info;
		bool slot = false;
		int i, count = 0, ret = 0;

		minsz = offsetofend(struct vfio_pci_hot_reset, count);

		if (copy_from_user(&hdr, (void __user *)arg, minsz))
			return -EFAULT;

		if (hdr.argsz < minsz || hdr.flags)
			return -EINVAL;

		/* Can we do a slot or bus reset or neither? */
		if (!pci_probe_reset_slot(vdev->pdev->slot))
			slot = true;
		else if (pci_probe_reset_bus(vdev->pdev->bus))
			return -ENODEV;

		/*
		 * We can't let userspace give us an arbitrarily large
		 * buffer to copy, so verify how many we think there
		 * could be.  Note groups can have multiple devices so
		 * one group per device is the max.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_count_devs,
						    &count, slot);
		if (ret)
			return ret;

		/* Somewhere between 1 and count is OK */
		if (!hdr.count || hdr.count > count)
			return -EINVAL;

		group_fds = kcalloc(hdr.count, sizeof(*group_fds), GFP_KERNEL);
		groups = kcalloc(hdr.count, sizeof(*groups), GFP_KERNEL);
		if (!group_fds || !groups) {
			kfree(group_fds);
			kfree(groups);
			return -ENOMEM;
		}

		if (copy_from_user(group_fds, (void __user *)(arg + minsz),
				   hdr.count * sizeof(*group_fds))) {
			kfree(group_fds);
			kfree(groups);
			return -EFAULT;
		}

		/*
		 * For each group_fd, get the group through the vfio external
		 * user interface and store the group and iommu ID.  This
		 * ensures the group is held across the reset.
		 */
		for (i = 0; i < hdr.count; i++) {
			struct vfio_group *group;
			struct fd f = fdget(group_fds[i]);
			if (!f.file) {
				ret = -EBADF;
				break;
			}

			group = vfio_group_get_external_user(f.file);
			fdput(f);
			if (IS_ERR(group)) {
				ret = PTR_ERR(group);
				break;
			}

			groups[i].group = group;
			groups[i].id = vfio_external_user_iommu_id(group);
		}

		kfree(group_fds);

		/* release reference to groups on error */
		if (ret)
			goto hot_reset_release;

		info.count = hdr.count;
		info.groups = groups;

		/*
		 * Test whether all the affected devices are contained
		 * by the set of groups provided by the user.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_validate_devs,
						    &info, slot);
		if (!ret)
			/* User has access, do the reset */
			ret = pci_reset_bus(vdev->pdev);

hot_reset_release:
		for (i--; i >= 0; i--)
			vfio_group_put_external_user(groups[i].group);

		kfree(groups);
		return ret;
	} else if (cmd == VFIO_DEVICE_IOEVENTFD) {
		struct vfio_device_ioeventfd ioeventfd;
		int count;

		minsz = offsetofend(struct vfio_device_ioeventfd, fd);

		if (copy_from_user(&ioeventfd, (void __user *)arg, minsz))
			return -EFAULT;

		if (ioeventfd.argsz < minsz)
			return -EINVAL;

		if (ioeventfd.flags & ~VFIO_DEVICE_IOEVENTFD_SIZE_MASK)
			return -EINVAL;

		count = ioeventfd.flags & VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

		if (hweight8(count) != 1 || ioeventfd.fd < -1)
			return -EINVAL;

		return vfio_pci_ioeventfd(vdev, ioeventfd.offset,
					  ioeventfd.data, count, ioeventfd.fd);
	} else if (cmd == VFIO_DEVICE_FEATURE) {
		struct vfio_device_feature feature;
		uuid_t uuid;

		minsz = offsetofend(struct vfio_device_feature, flags);

		if (copy_from_user(&feature, (void __user *)arg, minsz))
			return -EFAULT;

		if (feature.argsz < minsz)
			return -EINVAL;

		/* Check unknown flags */
		if (feature.flags & ~(VFIO_DEVICE_FEATURE_MASK |
				      VFIO_DEVICE_FEATURE_SET |
				      VFIO_DEVICE_FEATURE_GET |
				      VFIO_DEVICE_FEATURE_PROBE))
			return -EINVAL;

		/* GET & SET are mutually exclusive except with PROBE */
		if (!(feature.flags & VFIO_DEVICE_FEATURE_PROBE) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_SET) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_GET))
			return -EINVAL;

		switch (feature.flags & VFIO_DEVICE_FEATURE_MASK) {
		case VFIO_DEVICE_FEATURE_PCI_VF_TOKEN:
			if (!vdev->vf_token)
				return -ENOTTY;

			/*
			 * We do not support GET of the VF Token UUID as this
			 * could expose the token of the previous device user.
			 */
			if (feature.flags & VFIO_DEVICE_FEATURE_GET)
				return -EINVAL;

			if (feature.flags & VFIO_DEVICE_FEATURE_PROBE)
				return 0;

			/* Don't SET unless told to do so */
			if (!(feature.flags & VFIO_DEVICE_FEATURE_SET))
				return -EINVAL;

			if (feature.argsz < minsz + sizeof(uuid))
				return -EINVAL;

			if (copy_from_user(&uuid, (void __user *)(arg + minsz),
					   sizeof(uuid)))
				return -EFAULT;

			mutex_lock(&vdev->vf_token->lock);
			uuid_copy(&vdev->vf_token->uuid, &uuid);
			mutex_unlock(&vdev->vf_token->lock);

			return 0;
		default:
			return -ENOTTY;
		}
	}

	return -ENOTTY;
}

static ssize_t vfio_pci_rw(void *device_data, char __user *buf,
			   size_t count, loff_t *ppos, bool iswrite)
{
	unsigned int index = VFIO_PCI_OFFSET_TO_INDEX(*ppos);
	struct vfio_pci_device *vdev = device_data;

	if (index >= VFIO_PCI_NUM_REGIONS + vdev->num_regions)
		return -EINVAL;

	switch (index) {
	case VFIO_PCI_CONFIG_REGION_INDEX:
		return vfio_pci_config_rw(vdev, buf, count, ppos, iswrite);

	case VFIO_PCI_ROM_REGION_INDEX:
		if (iswrite)
			return -EINVAL;
		return vfio_pci_bar_rw(vdev, buf, count, ppos, false);

	case VFIO_PCI_BAR0_REGION_INDEX ... VFIO_PCI_BAR5_REGION_INDEX:
		return vfio_pci_bar_rw(vdev, buf, count, ppos, iswrite);

	case VFIO_PCI_VGA_REGION_INDEX:
		return vfio_pci_vga_rw(vdev, buf, count, ppos, iswrite);
	default:
		index -= VFIO_PCI_NUM_REGIONS;
		return vdev->region[index].ops->rw(vdev, buf,
						   count, ppos, iswrite);
	}

	return -EINVAL;
}

static ssize_t vfio_pci_read(void *device_data, char __user *buf,
			     size_t count, loff_t *ppos)
{
	if (!count)
		return 0;

	return vfio_pci_rw(device_data, buf, count, ppos, false);
}

static ssize_t vfio_pci_write(void *device_data, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	if (!count)
		return 0;

	return vfio_pci_rw(device_data, (char __user *)buf, count, ppos, true);
}

static int vfio_pci_add_vma(struct vfio_pci_device *vdev,
			    struct vm_area_struct *vma)
{
	struct vfio_pci_mmap_vma *mmap_vma;

	mmap_vma = kmalloc(sizeof(*mmap_vma), GFP_KERNEL);
	if (!mmap_vma)
		return -ENOMEM;

	mmap_vma->vma = vma;

	mutex_lock(&vdev->vma_lock);
	list_add(&mmap_vma->vma_next, &vdev->vma_list);
	mutex_unlock(&vdev->vma_lock);

	return 0;
}

/*
 * Zap mmaps on open so that we can fault them in on access and therefore
 * our vma_list only tracks mappings accessed since last zap.
 */
static void vfio_pci_mmap_open(struct vm_area_struct *vma)
{
	zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
}

static void vfio_pci_mmap_close(struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = vma->vm_private_data;
	struct vfio_pci_mmap_vma *mmap_vma;

	mutex_lock(&vdev->vma_lock);
	list_for_each_entry(mmap_vma, &vdev->vma_list, vma_next) {
		if (mmap_vma->vma == vma) {
			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);
			break;
		}
	}
	mutex_unlock(&vdev->vma_lock);
}

static vm_fault_t vfio_pci_mmap_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct vfio_pci_device *vdev = vma->vm_private_data;

	if (vfio_pci_add_vma(vdev, vma))
		return VM_FAULT_OOM;

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

static const struct vm_operations_struct vfio_pci_mmap_ops = {
	.open = vfio_pci_mmap_open,
	.close = vfio_pci_mmap_close,
	.fault = vfio_pci_mmap_fault,
};

static int vfio_pci_mmap(void *device_data, struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = device_data;
	struct pci_dev *pdev = vdev->pdev;
	unsigned int index;
	u64 phys_len, req_len, pgoff, req_start;
	int ret;

	index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;
	if (index >= VFIO_PCI_NUM_REGIONS) {
		int regnum = index - VFIO_PCI_NUM_REGIONS;
		struct vfio_pci_region *region = vdev->region + regnum;

		if (region && region->ops && region->ops->mmap &&
		    (region->flags & VFIO_REGION_INFO_FLAG_MMAP))
			return region->ops->mmap(vdev, region, vma);
		return -EINVAL;
	}
	if (index >= VFIO_PCI_ROM_REGION_INDEX)
		return -EINVAL;
	if (!vdev->bar_mmap_supported[index])
		return -EINVAL;

	phys_len = PAGE_ALIGN(pci_resource_len(pdev, index));
	req_len = vma->vm_end - vma->vm_start;
	pgoff = vma->vm_pgoff &
		((1U << (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT)) - 1);
	req_start = pgoff << PAGE_SHIFT;

	if (req_start + req_len > phys_len)
		return -EINVAL;

	/*
	 * Even though we don't make use of the barmap for the mmap,
	 * we need to request the region and the barmap tracks that.
	 */
	if (!vdev->barmap[index]) {
		ret = pci_request_selected_regions(pdev,
						   1 << index, "vfio-pci");
		if (ret)
			return ret;

		vdev->barmap[index] = pci_iomap(pdev, index, 0);
		if (!vdev->barmap[index]) {
			pci_release_selected_regions(pdev, 1 << index);
			return -ENOMEM;
		}
	}

	vma->vm_private_data = vdev;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff = (pci_resource_start(pdev, index) >> PAGE_SHIFT) + pgoff;

	/*
	 * See remap_pfn_range(), called from vfio_pci_fault() but we can't
	 * change vm_flags within the fault handler.  Set them now.
	 */
	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &vfio_pci_mmap_ops;

	return 0;
}

static void vfio_pci_request(void *device_data, unsigned int count)
{
	struct vfio_pci_device *vdev = device_data;
	struct pci_dev *pdev = vdev->pdev;

	mutex_lock(&vdev->igate);

	if (vdev->req_trigger) {
		if (!(count % 10))
			pci_notice_ratelimited(pdev,
				"Relaying device request to user (#%u)\n",
				count);
		eventfd_signal(vdev->req_trigger, 1);
	} else if (count == 0) {
		pci_warn(pdev,
			"No device request channel registered, blocked until released by user\n");
	}

	mutex_unlock(&vdev->igate);
}

static int vfio_pci_validate_vf_token(struct vfio_pci_device *vdev,
				      bool vf_token, uuid_t *uuid)
{
	/*
	 * There's always some degree of trust or collaboration between SR-IOV
	 * PF and VFs, even if just that the PF hosts the SR-IOV capability and
	 * can disrupt VFs with a reset, but often the PF has more explicit
	 * access to deny service to the VF or access data passed through the
	 * VF.  We therefore require an opt-in via a shared VF token (UUID) to
	 * represent this trust.  This both prevents that a VF driver might
	 * assume the PF driver is a trusted, in-kernel driver, and also that
	 * a PF driver might be replaced with a rogue driver, unknown to in-use
	 * VF drivers.
	 *
	 * Therefore when presented with a VF, if the PF is a vfio device and
	 * it is bound to the vfio-pci driver, the user needs to provide a VF
	 * token to access the device, in the form of appending a vf_token to
	 * the device name, for example:
	 *
	 * "0000:04:10.0 vf_token=bd8d9d2b-5a5f-4f5a-a211-f591514ba1f3"
	 *
	 * When presented with a PF which has VFs in use, the user must also
	 * provide the current VF token to prove collaboration with existing
	 * VF users.  If VFs are not in use, the VF token provided for the PF
	 * device will act to set the VF token.
	 *
	 * If the VF token is provided but unused, an error is generated.
	 */
	if (!vdev->pdev->is_virtfn && !vdev->vf_token && !vf_token)
		return 0; /* No VF token provided or required */

	if (vdev->pdev->is_virtfn) {
		struct vfio_device *pf_dev;
		struct vfio_pci_device *pf_vdev = get_pf_vdev(vdev, &pf_dev);
		bool match;

		if (!pf_vdev) {
			if (!vf_token)
				return 0; /* PF is not vfio-pci, no VF token */

			pci_info_ratelimited(vdev->pdev,
				"VF token incorrectly provided, PF not bound to vfio-pci\n");
			return -EINVAL;
		}

		if (!vf_token) {
			vfio_device_put(pf_dev);
			pci_info_ratelimited(vdev->pdev,
				"VF token required to access device\n");
			return -EACCES;
		}

		mutex_lock(&pf_vdev->vf_token->lock);
		match = uuid_equal(uuid, &pf_vdev->vf_token->uuid);
		mutex_unlock(&pf_vdev->vf_token->lock);

		vfio_device_put(pf_dev);

		if (!match) {
			pci_info_ratelimited(vdev->pdev,
				"Incorrect VF token provided for device\n");
			return -EACCES;
		}
	} else if (vdev->vf_token) {
		mutex_lock(&vdev->vf_token->lock);
		if (vdev->vf_token->users) {
			if (!vf_token) {
				mutex_unlock(&vdev->vf_token->lock);
				pci_info_ratelimited(vdev->pdev,
					"VF token required to access device\n");
				return -EACCES;
			}

			if (!uuid_equal(uuid, &vdev->vf_token->uuid)) {
				mutex_unlock(&vdev->vf_token->lock);
				pci_info_ratelimited(vdev->pdev,
					"Incorrect VF token provided for device\n");
				return -EACCES;
			}
		} else if (vf_token) {
			uuid_copy(&vdev->vf_token->uuid, uuid);
		}

		mutex_unlock(&vdev->vf_token->lock);
	} else if (vf_token) {
		pci_info_ratelimited(vdev->pdev,
			"VF token incorrectly provided, not a PF or VF\n");
		return -EINVAL;
	}

	return 0;
}

#define VF_TOKEN_ARG "vf_token="

static int vfio_pci_match(void *device_data, char *buf)
{
	struct vfio_pci_device *vdev = device_data;
	bool vf_token = false;
	uuid_t uuid;
	int ret;

	if (strncmp(pci_name(vdev->pdev), buf, strlen(pci_name(vdev->pdev))))
		return 0; /* No match */

	if (strlen(buf) > strlen(pci_name(vdev->pdev))) {
		buf += strlen(pci_name(vdev->pdev));

		if (*buf != ' ')
			return 0; /* No match: non-whitespace after name */

		while (*buf) {
			if (*buf == ' ') {
				buf++;
				continue;
			}

			if (!vf_token && !strncmp(buf, VF_TOKEN_ARG,
						  strlen(VF_TOKEN_ARG))) {
				buf += strlen(VF_TOKEN_ARG);

				if (strlen(buf) < UUID_STRING_LEN)
					return -EINVAL;

				ret = uuid_parse(buf, &uuid);
				if (ret)
					return ret;

				vf_token = true;
				buf += UUID_STRING_LEN;
			} else {
				/* Unknown/duplicate option */
				return -EINVAL;
			}
		}
	}

	ret = vfio_pci_validate_vf_token(vdev, vf_token, &uuid);
	if (ret)
		return ret;

	return 1; /* Match */
}

static const struct vfio_device_ops vfio_pci_ops = {
	.name		= "vfio-pci",
	.open		= vfio_pci_open,
	.release	= vfio_pci_release,
	.ioctl		= vfio_pci_ioctl,
	.read		= vfio_pci_read,
	.write		= vfio_pci_write,
	.mmap		= vfio_pci_mmap,
	.request	= vfio_pci_request,
	.match		= vfio_pci_match,
};

static int vfio_pci_reflck_attach(struct vfio_pci_device *vdev);
static void vfio_pci_reflck_put(struct vfio_pci_reflck *reflck);
static struct pci_driver vfio_pci_driver;

static int vfio_pci_bus_notifier(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct vfio_pci_device *vdev = container_of(nb,
						    struct vfio_pci_device, nb);
	struct device *dev = data;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_dev *physfn = pci_physfn(pdev);

	if (action == BUS_NOTIFY_ADD_DEVICE &&
	    pdev->is_virtfn && physfn == vdev->pdev) {
		pci_info(vdev->pdev, "Captured SR-IOV VF %s driver_override\n",
			 pci_name(pdev));
		pdev->driver_override = kasprintf(GFP_KERNEL, "%s",
						  vfio_pci_ops.name);
	} else if (action == BUS_NOTIFY_BOUND_DRIVER &&
		   pdev->is_virtfn && physfn == vdev->pdev) {
		struct pci_driver *drv = pci_dev_driver(pdev);

		if (drv && drv != &vfio_pci_driver)
			pci_warn(vdev->pdev,
				 "VF %s bound to driver %s while PF bound to vfio-pci\n",
				 pci_name(pdev), drv->name);
	}

	return 0;
}

static int vfio_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct vfio_pci_device *vdev;
	struct iommu_group *group;
	int ret;

	if (pdev->hdr_type != PCI_HEADER_TYPE_NORMAL)
		return -EINVAL;

	/*
	 * Prevent binding to PFs with VFs enabled, the VFs might be in use
	 * by the host or other users.  We cannot capture the VFs if they
	 * already exist, nor can we track VF users.  Disabling SR-IOV here
	 * would initiate removing the VFs, which would unbind the driver,
	 * which is prone to blocking if that VF is also in use by vfio-pci.
	 * Just reject these PFs and let the user sort it out.
	 */
	if (pci_num_vf(pdev)) {
		pci_warn(pdev, "Cannot bind to PF with SR-IOV enabled\n");
		return -EBUSY;
	}

	group = vfio_iommu_group_get(&pdev->dev);
	if (!group)
		return -EINVAL;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		ret = -ENOMEM;
		goto out_group_put;
	}

	vdev->pdev = pdev;
	vdev->irq_type = VFIO_PCI_NUM_IRQS;
	mutex_init(&vdev->igate);
	spin_lock_init(&vdev->irqlock);
	mutex_init(&vdev->ioeventfds_lock);
	INIT_LIST_HEAD(&vdev->ioeventfds_list);
	mutex_init(&vdev->vma_lock);
	INIT_LIST_HEAD(&vdev->vma_list);

	ret = vfio_add_group_dev(&pdev->dev, &vfio_pci_ops, vdev);
	if (ret)
		goto out_free;

	ret = vfio_pci_reflck_attach(vdev);
	if (ret)
		goto out_del_group_dev;

	if (pdev->is_physfn) {
		vdev->vf_token = kzalloc(sizeof(*vdev->vf_token), GFP_KERNEL);
		if (!vdev->vf_token) {
			ret = -ENOMEM;
			goto out_reflck;
		}

		mutex_init(&vdev->vf_token->lock);
		uuid_gen(&vdev->vf_token->uuid);

		vdev->nb.notifier_call = vfio_pci_bus_notifier;
		ret = bus_register_notifier(&pci_bus_type, &vdev->nb);
		if (ret)
			goto out_vf_token;
	}

	if (vfio_pci_is_vga(pdev)) {
		vga_client_register(pdev, vdev, NULL, vfio_pci_set_vga_decode);
		vga_set_legacy_decoding(pdev,
					vfio_pci_set_vga_decode(vdev, false));
	}

	vfio_pci_probe_power_state(vdev);

	if (!disable_idle_d3) {
		/*
		 * pci-core sets the device power state to an unknown value at
		 * bootup and after being removed from a driver.  The only
		 * transition it allows from this unknown state is to D0, which
		 * typically happens when a driver calls pci_enable_device().
		 * We're not ready to enable the device yet, but we do want to
		 * be able to get to D3.  Therefore first do a D0 transition
		 * before going to D3.
		 */
		vfio_pci_set_power_state(vdev, PCI_D0);
		vfio_pci_set_power_state(vdev, PCI_D3hot);
	}

	return ret;

out_vf_token:
	kfree(vdev->vf_token);
out_reflck:
	vfio_pci_reflck_put(vdev->reflck);
out_del_group_dev:
	vfio_del_group_dev(&pdev->dev);
out_free:
	kfree(vdev);
out_group_put:
	vfio_iommu_group_put(group, &pdev->dev);
	return ret;
}

static void vfio_pci_remove(struct pci_dev *pdev)
{
	struct vfio_pci_device *vdev;

	pci_disable_sriov(pdev);

	vdev = vfio_del_group_dev(&pdev->dev);
	if (!vdev)
		return;

	if (vdev->vf_token) {
		WARN_ON(vdev->vf_token->users);
		mutex_destroy(&vdev->vf_token->lock);
		kfree(vdev->vf_token);
	}

	if (vdev->nb.notifier_call)
		bus_unregister_notifier(&pci_bus_type, &vdev->nb);

	vfio_pci_reflck_put(vdev->reflck);

	vfio_iommu_group_put(pdev->dev.iommu_group, &pdev->dev);
	kfree(vdev->region);
	mutex_destroy(&vdev->ioeventfds_lock);

	if (!disable_idle_d3)
		vfio_pci_set_power_state(vdev, PCI_D0);

	kfree(vdev->pm_save);
	kfree(vdev);

	if (vfio_pci_is_vga(pdev)) {
		vga_client_register(pdev, NULL, NULL, NULL);
		vga_set_legacy_decoding(pdev,
				VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM |
				VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM);
	}
}

static pci_ers_result_t vfio_pci_aer_err_detected(struct pci_dev *pdev,
						  pci_channel_state_t state)
{
	struct vfio_pci_device *vdev;
	struct vfio_device *device;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (device == NULL)
		return PCI_ERS_RESULT_DISCONNECT;

	vdev = vfio_device_data(device);
	if (vdev == NULL) {
		vfio_device_put(device);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	mutex_lock(&vdev->igate);

	if (vdev->err_trigger)
		eventfd_signal(vdev->err_trigger, 1);

	mutex_unlock(&vdev->igate);

	vfio_device_put(device);

	return PCI_ERS_RESULT_CAN_RECOVER;
}

static int vfio_pci_sriov_configure(struct pci_dev *pdev, int nr_virtfn)
{
	struct vfio_pci_device *vdev;
	struct vfio_device *device;
	int ret = 0;

	might_sleep();

	if (!enable_sriov)
		return -ENOENT;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return -ENODEV;

	vdev = vfio_device_data(device);
	if (!vdev) {
		vfio_device_put(device);
		return -ENODEV;
	}

	if (nr_virtfn == 0)
		pci_disable_sriov(pdev);
	else
		ret = pci_enable_sriov(pdev, nr_virtfn);

	vfio_device_put(device);

	return ret < 0 ? ret : nr_virtfn;
}

static const struct pci_error_handlers vfio_err_handlers = {
	.error_detected = vfio_pci_aer_err_detected,
};

static struct pci_driver vfio_pci_driver = {
	.name			= "vfio-pci",
	.id_table		= NULL, /* only dynamic ids */
	.probe			= vfio_pci_probe,
	.remove			= vfio_pci_remove,
	.sriov_configure	= vfio_pci_sriov_configure,
	.err_handler		= &vfio_err_handlers,
};

static DEFINE_MUTEX(reflck_lock);

static struct vfio_pci_reflck *vfio_pci_reflck_alloc(void)
{
	struct vfio_pci_reflck *reflck;

	reflck = kzalloc(sizeof(*reflck), GFP_KERNEL);
	if (!reflck)
		return ERR_PTR(-ENOMEM);

	kref_init(&reflck->kref);
	mutex_init(&reflck->lock);

	return reflck;
}

static void vfio_pci_reflck_get(struct vfio_pci_reflck *reflck)
{
	kref_get(&reflck->kref);
}

static int vfio_pci_reflck_find(struct pci_dev *pdev, void *data)
{
	struct vfio_pci_reflck **preflck = data;
	struct vfio_device *device;
	struct vfio_pci_device *vdev;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return 0;

	if (pci_dev_driver(pdev) != &vfio_pci_driver) {
		vfio_device_put(device);
		return 0;
	}

	vdev = vfio_device_data(device);

	if (vdev->reflck) {
		vfio_pci_reflck_get(vdev->reflck);
		*preflck = vdev->reflck;
		vfio_device_put(device);
		return 1;
	}

	vfio_device_put(device);
	return 0;
}

static int vfio_pci_reflck_attach(struct vfio_pci_device *vdev)
{
	bool slot = !pci_probe_reset_slot(vdev->pdev->slot);

	mutex_lock(&reflck_lock);

	if (pci_is_root_bus(vdev->pdev->bus) ||
	    vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_reflck_find,
					  &vdev->reflck, slot) <= 0)
		vdev->reflck = vfio_pci_reflck_alloc();

	mutex_unlock(&reflck_lock);

	return PTR_ERR_OR_ZERO(vdev->reflck);
}

static void vfio_pci_reflck_release(struct kref *kref)
{
	struct vfio_pci_reflck *reflck = container_of(kref,
						      struct vfio_pci_reflck,
						      kref);

	kfree(reflck);
	mutex_unlock(&reflck_lock);
}

static void vfio_pci_reflck_put(struct vfio_pci_reflck *reflck)
{
	kref_put_mutex(&reflck->kref, vfio_pci_reflck_release, &reflck_lock);
}

struct vfio_devices {
	struct vfio_device **devices;
	int cur_index;
	int max_index;
};

static int vfio_pci_get_unused_devs(struct pci_dev *pdev, void *data)
{
	struct vfio_devices *devs = data;
	struct vfio_device *device;
	struct vfio_pci_device *vdev;

	if (devs->cur_index == devs->max_index)
		return -ENOSPC;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return -EINVAL;

	if (pci_dev_driver(pdev) != &vfio_pci_driver) {
		vfio_device_put(device);
		return -EBUSY;
	}

	vdev = vfio_device_data(device);

	/* Fault if the device is not unused */
	if (vdev->refcnt) {
		vfio_device_put(device);
		return -EBUSY;
	}

	devs->devices[devs->cur_index++] = device;
	return 0;
}

/*
 * If a bus or slot reset is available for the provided device and:
 *  - All of the devices affected by that bus or slot reset are unused
 *    (!refcnt)
 *  - At least one of the affected devices is marked dirty via
 *    needs_reset (such as by lack of FLR support)
 * Then attempt to perform that bus or slot reset.  Callers are required
 * to hold vdev->reflck->lock, protecting the bus/slot reset group from
 * concurrent opens.  A vfio_device reference is acquired for each device
 * to prevent unbinds during the reset operation.
 *
 * NB: vfio-core considers a group to be viable even if some devices are
 * bound to drivers like pci-stub or pcieport.  Here we require all devices
 * to be bound to vfio_pci since that's the only way we can be sure they
 * stay put.
 */
static void vfio_pci_try_bus_reset(struct vfio_pci_device *vdev)
{
	struct vfio_devices devs = { .cur_index = 0 };
	int i = 0, ret = -EINVAL;
	bool slot = false;
	struct vfio_pci_device *tmp;

	if (!pci_probe_reset_slot(vdev->pdev->slot))
		slot = true;
	else if (pci_probe_reset_bus(vdev->pdev->bus))
		return;

	if (vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_count_devs,
					  &i, slot) || !i)
		return;

	devs.max_index = i;
	devs.devices = kcalloc(i, sizeof(struct vfio_device *), GFP_KERNEL);
	if (!devs.devices)
		return;

	if (vfio_pci_for_each_slot_or_bus(vdev->pdev,
					  vfio_pci_get_unused_devs,
					  &devs, slot))
		goto put_devs;

	/* Does at least one need a reset? */
	for (i = 0; i < devs.cur_index; i++) {
		tmp = vfio_device_data(devs.devices[i]);
		if (tmp->needs_reset) {
			ret = pci_reset_bus(vdev->pdev);
			break;
		}
	}

put_devs:
	for (i = 0; i < devs.cur_index; i++) {
		tmp = vfio_device_data(devs.devices[i]);

		/*
		 * If reset was successful, affected devices no longer need
		 * a reset and we should return all the collateral devices
		 * to low power.  If not successful, we either didn't reset
		 * the bus or timed out waiting for it, so let's not touch
		 * the power state.
		 */
		if (!ret) {
			tmp->needs_reset = false;

			if (tmp != vdev && !disable_idle_d3)
				vfio_pci_set_power_state(tmp, PCI_D3hot);
		}

		vfio_device_put(devs.devices[i]);
	}

	kfree(devs.devices);
}

static void __exit vfio_pci_cleanup(void)
{
	pci_unregister_driver(&vfio_pci_driver);
	vfio_pci_uninit_perm_bits();
}

static void __init vfio_pci_fill_ids(void)
{
	char *p, *id;
	int rc;

	/* no ids passed actually */
	if (ids[0] == '\0')
		return;

	/* add ids specified in the module parameter */
	p = ids;
	while ((id = strsep(&p, ","))) {
		unsigned int vendor, device, subvendor = PCI_ANY_ID,
			subdevice = PCI_ANY_ID, class = 0, class_mask = 0;
		int fields;

		if (!strlen(id))
			continue;

		fields = sscanf(id, "%x:%x:%x:%x:%x:%x",
				&vendor, &device, &subvendor, &subdevice,
				&class, &class_mask);

		if (fields < 2) {
			pr_warn("invalid id string \"%s\"\n", id);
			continue;
		}

		rc = pci_add_dynid(&vfio_pci_driver, vendor, device,
				   subvendor, subdevice, class, class_mask, 0);
		if (rc)
			pr_warn("failed to add dynamic id [%04x:%04x[%04x:%04x]] class %#08x/%08x (%d)\n",
				vendor, device, subvendor, subdevice,
				class, class_mask, rc);
		else
			pr_info("add [%04x:%04x[%04x:%04x]] class %#08x/%08x\n",
				vendor, device, subvendor, subdevice,
				class, class_mask);
	}
}

static int __init vfio_pci_init(void)
{
	int ret;

	/* Allocate shared config space permision data used by all devices */
	ret = vfio_pci_init_perm_bits();
	if (ret)
		return ret;

	/* Register and scan for devices */
	ret = pci_register_driver(&vfio_pci_driver);
	if (ret)
		goto out_driver;

	vfio_pci_fill_ids();

	return 0;

out_driver:
	vfio_pci_uninit_perm_bits();
	return ret;
}

module_init(vfio_pci_init);
module_exit(vfio_pci_cleanup);

MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
		{
			void __iomem *io;
			size_t size;
			u16 orig_cmd;

			info.offset = VFIO_PCI_INDEX_TO_OFFSET(info.index);
			info.flags = 0;

			/* Report the BAR size, not the ROM size */
			info.size = pci_resource_len(pdev, info.index);
			if (!info.size) {
				/* Shadow ROMs appear as PCI option ROMs */
				if (pdev->resource[PCI_ROM_RESOURCE].flags &
							IORESOURCE_ROM_SHADOW)
					info.size = 0x20000;
				else
					break;
			}
			if (!info.size) {
				/* Shadow ROMs appear as PCI option ROMs */
				if (pdev->resource[PCI_ROM_RESOURCE].flags &
							IORESOURCE_ROM_SHADOW)
					info.size = 0x20000;
				else
					break;
			}

			/*
			 * Is it really there?  Enable memory decode for
			 * implicit access in pci_map_rom().
			 */
			pci_read_config_word(pdev, PCI_COMMAND, &orig_cmd);
			pci_write_config_word(pdev, PCI_COMMAND,
					      orig_cmd | PCI_COMMAND_MEMORY);

			io = pci_map_rom(pdev, &size);
			if (io) {
				info.flags = VFIO_REGION_INFO_FLAG_READ;
				pci_unmap_rom(pdev, io);
			} else {
				info.size = 0;
			}

			pci_write_config_word(pdev, PCI_COMMAND, orig_cmd);
			break;
		}
		if (data_size) {
			data = memdup_user((void __user *)(arg + minsz),
					    data_size);
			if (IS_ERR(data))
				return PTR_ERR(data);
		}

		mutex_lock(&vdev->igate);

		ret = vfio_pci_set_irqs_ioctl(vdev, hdr.flags, hdr.index,
					      hdr.start, hdr.count, data);

		mutex_unlock(&vdev->igate);
		kfree(data);

		return ret;

	} else if (cmd == VFIO_DEVICE_RESET) {
		return vdev->reset_works ?
			pci_try_reset_function(vdev->pdev) : -EINVAL;

	} else if (cmd == VFIO_DEVICE_GET_PCI_HOT_RESET_INFO) {
		struct vfio_pci_hot_reset_info hdr;
		struct vfio_pci_fill_info fill = { 0 };
		struct vfio_pci_dependent_device *devices = NULL;
		bool slot = false;
		int ret = 0;

		minsz = offsetofend(struct vfio_pci_hot_reset_info, count);

		if (copy_from_user(&hdr, (void __user *)arg, minsz))
			return -EFAULT;

		if (hdr.argsz < minsz)
			return -EINVAL;

		hdr.flags = 0;

		/* Can we do a slot or bus reset or neither? */
		if (!pci_probe_reset_slot(vdev->pdev->slot))
			slot = true;
		else if (pci_probe_reset_bus(vdev->pdev->bus))
			return -ENODEV;

		/* How many devices are affected? */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_count_devs,
						    &fill.max, slot);
		if (ret)
			return ret;

		WARN_ON(!fill.max); /* Should always be at least one */

		/*
		 * If there's enough space, fill it now, otherwise return
		 * -ENOSPC and the number of devices affected.
		 */
		if (hdr.argsz < sizeof(hdr) + (fill.max * sizeof(*devices))) {
			ret = -ENOSPC;
			hdr.count = fill.max;
			goto reset_info_exit;
		}

		devices = kcalloc(fill.max, sizeof(*devices), GFP_KERNEL);
		if (!devices)
			return -ENOMEM;

		fill.devices = devices;

		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_fill_devs,
						    &fill, slot);

		/*
		 * If a device was removed between counting and filling,
		 * we may come up short of fill.max.  If a device was
		 * added, we'll have a return of -EAGAIN above.
		 */
		if (!ret)
			hdr.count = fill.cur;

reset_info_exit:
		if (copy_to_user((void __user *)arg, &hdr, minsz))
			ret = -EFAULT;

		if (!ret) {
			if (copy_to_user((void __user *)(arg + minsz), devices,
					 hdr.count * sizeof(*devices)))
				ret = -EFAULT;
		}

		kfree(devices);
		return ret;

	} else if (cmd == VFIO_DEVICE_PCI_HOT_RESET) {
		struct vfio_pci_hot_reset hdr;
		int32_t *group_fds;
		struct vfio_pci_group_entry *groups;
		struct vfio_pci_group_info info;
		bool slot = false;
		int i, count = 0, ret = 0;

		minsz = offsetofend(struct vfio_pci_hot_reset, count);

		if (copy_from_user(&hdr, (void __user *)arg, minsz))
			return -EFAULT;

		if (hdr.argsz < minsz || hdr.flags)
			return -EINVAL;

		/* Can we do a slot or bus reset or neither? */
		if (!pci_probe_reset_slot(vdev->pdev->slot))
			slot = true;
		else if (pci_probe_reset_bus(vdev->pdev->bus))
			return -ENODEV;

		/*
		 * We can't let userspace give us an arbitrarily large
		 * buffer to copy, so verify how many we think there
		 * could be.  Note groups can have multiple devices so
		 * one group per device is the max.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_count_devs,
						    &count, slot);
		if (ret)
			return ret;

		/* Somewhere between 1 and count is OK */
		if (!hdr.count || hdr.count > count)
			return -EINVAL;

		group_fds = kcalloc(hdr.count, sizeof(*group_fds), GFP_KERNEL);
		groups = kcalloc(hdr.count, sizeof(*groups), GFP_KERNEL);
		if (!group_fds || !groups) {
			kfree(group_fds);
			kfree(groups);
			return -ENOMEM;
		}

		if (copy_from_user(group_fds, (void __user *)(arg + minsz),
				   hdr.count * sizeof(*group_fds))) {
			kfree(group_fds);
			kfree(groups);
			return -EFAULT;
		}

		/*
		 * For each group_fd, get the group through the vfio external
		 * user interface and store the group and iommu ID.  This
		 * ensures the group is held across the reset.
		 */
		for (i = 0; i < hdr.count; i++) {
			struct vfio_group *group;
			struct fd f = fdget(group_fds[i]);
			if (!f.file) {
				ret = -EBADF;
				break;
			}

			group = vfio_group_get_external_user(f.file);
			fdput(f);
			if (IS_ERR(group)) {
				ret = PTR_ERR(group);
				break;
			}

			groups[i].group = group;
			groups[i].id = vfio_external_user_iommu_id(group);
		}

		kfree(group_fds);

		/* release reference to groups on error */
		if (ret)
			goto hot_reset_release;

		info.count = hdr.count;
		info.groups = groups;

		/*
		 * Test whether all the affected devices are contained
		 * by the set of groups provided by the user.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_validate_devs,
						    &info, slot);
		if (!ret)
			/* User has access, do the reset */
			ret = pci_reset_bus(vdev->pdev);

hot_reset_release:
		for (i--; i >= 0; i--)
			vfio_group_put_external_user(groups[i].group);

		kfree(groups);
		return ret;
	} else if (cmd == VFIO_DEVICE_IOEVENTFD) {
		struct vfio_device_ioeventfd ioeventfd;
		int count;

		minsz = offsetofend(struct vfio_device_ioeventfd, fd);

		if (copy_from_user(&ioeventfd, (void __user *)arg, minsz))
			return -EFAULT;

		if (ioeventfd.argsz < minsz)
			return -EINVAL;

		if (ioeventfd.flags & ~VFIO_DEVICE_IOEVENTFD_SIZE_MASK)
			return -EINVAL;

		count = ioeventfd.flags & VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

		if (hweight8(count) != 1 || ioeventfd.fd < -1)
			return -EINVAL;

		return vfio_pci_ioeventfd(vdev, ioeventfd.offset,
					  ioeventfd.data, count, ioeventfd.fd);
	} else if (cmd == VFIO_DEVICE_FEATURE) {
		struct vfio_device_feature feature;
		uuid_t uuid;

		minsz = offsetofend(struct vfio_device_feature, flags);

		if (copy_from_user(&feature, (void __user *)arg, minsz))
			return -EFAULT;

		if (feature.argsz < minsz)
			return -EINVAL;

		/* Check unknown flags */
		if (feature.flags & ~(VFIO_DEVICE_FEATURE_MASK |
				      VFIO_DEVICE_FEATURE_SET |
				      VFIO_DEVICE_FEATURE_GET |
				      VFIO_DEVICE_FEATURE_PROBE))
			return -EINVAL;

		/* GET & SET are mutually exclusive except with PROBE */
		if (!(feature.flags & VFIO_DEVICE_FEATURE_PROBE) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_SET) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_GET))
			return -EINVAL;

		switch (feature.flags & VFIO_DEVICE_FEATURE_MASK) {
		case VFIO_DEVICE_FEATURE_PCI_VF_TOKEN:
			if (!vdev->vf_token)
				return -ENOTTY;

			/*
			 * We do not support GET of the VF Token UUID as this
			 * could expose the token of the previous device user.
			 */
			if (feature.flags & VFIO_DEVICE_FEATURE_GET)
				return -EINVAL;

			if (feature.flags & VFIO_DEVICE_FEATURE_PROBE)
				return 0;

			/* Don't SET unless told to do so */
			if (!(feature.flags & VFIO_DEVICE_FEATURE_SET))
				return -EINVAL;

			if (feature.argsz < minsz + sizeof(uuid))
				return -EINVAL;

			if (copy_from_user(&uuid, (void __user *)(arg + minsz),
					   sizeof(uuid)))
				return -EFAULT;

			mutex_lock(&vdev->vf_token->lock);
			uuid_copy(&vdev->vf_token->uuid, &uuid);
			mutex_unlock(&vdev->vf_token->lock);

			return 0;
		default:
			return -ENOTTY;
		}
	}

	return -ENOTTY;
}

static ssize_t vfio_pci_rw(void *device_data, char __user *buf,
			   size_t count, loff_t *ppos, bool iswrite)
{
	unsigned int index = VFIO_PCI_OFFSET_TO_INDEX(*ppos);
	struct vfio_pci_device *vdev = device_data;

	if (index >= VFIO_PCI_NUM_REGIONS + vdev->num_regions)
		return -EINVAL;

	switch (index) {
	case VFIO_PCI_CONFIG_REGION_INDEX:
		return vfio_pci_config_rw(vdev, buf, count, ppos, iswrite);

	case VFIO_PCI_ROM_REGION_INDEX:
		if (iswrite)
			return -EINVAL;
		return vfio_pci_bar_rw(vdev, buf, count, ppos, false);

	case VFIO_PCI_BAR0_REGION_INDEX ... VFIO_PCI_BAR5_REGION_INDEX:
		return vfio_pci_bar_rw(vdev, buf, count, ppos, iswrite);

	case VFIO_PCI_VGA_REGION_INDEX:
		return vfio_pci_vga_rw(vdev, buf, count, ppos, iswrite);
	default:
		index -= VFIO_PCI_NUM_REGIONS;
		return vdev->region[index].ops->rw(vdev, buf,
						   count, ppos, iswrite);
	}

	return -EINVAL;
}

static ssize_t vfio_pci_read(void *device_data, char __user *buf,
			     size_t count, loff_t *ppos)
{
	if (!count)
		return 0;

	return vfio_pci_rw(device_data, buf, count, ppos, false);
}

static ssize_t vfio_pci_write(void *device_data, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	if (!count)
		return 0;

	return vfio_pci_rw(device_data, (char __user *)buf, count, ppos, true);
}

static int vfio_pci_add_vma(struct vfio_pci_device *vdev,
			    struct vm_area_struct *vma)
{
	struct vfio_pci_mmap_vma *mmap_vma;

	mmap_vma = kmalloc(sizeof(*mmap_vma), GFP_KERNEL);
	if (!mmap_vma)
		return -ENOMEM;

	mmap_vma->vma = vma;

	mutex_lock(&vdev->vma_lock);
	list_add(&mmap_vma->vma_next, &vdev->vma_list);
	mutex_unlock(&vdev->vma_lock);

	return 0;
}

/*
 * Zap mmaps on open so that we can fault them in on access and therefore
 * our vma_list only tracks mappings accessed since last zap.
 */
static void vfio_pci_mmap_open(struct vm_area_struct *vma)
{
	zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
}

static void vfio_pci_mmap_close(struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = vma->vm_private_data;
	struct vfio_pci_mmap_vma *mmap_vma;

	mutex_lock(&vdev->vma_lock);
	list_for_each_entry(mmap_vma, &vdev->vma_list, vma_next) {
		if (mmap_vma->vma == vma) {
			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);
			break;
		}
	}
	mutex_unlock(&vdev->vma_lock);
}

static vm_fault_t vfio_pci_mmap_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct vfio_pci_device *vdev = vma->vm_private_data;

	if (vfio_pci_add_vma(vdev, vma))
		return VM_FAULT_OOM;

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

static const struct vm_operations_struct vfio_pci_mmap_ops = {
	.open = vfio_pci_mmap_open,
	.close = vfio_pci_mmap_close,
	.fault = vfio_pci_mmap_fault,
};

static int vfio_pci_mmap(void *device_data, struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = device_data;
	struct pci_dev *pdev = vdev->pdev;
	unsigned int index;
	u64 phys_len, req_len, pgoff, req_start;
	int ret;

	index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;
	if (index >= VFIO_PCI_NUM_REGIONS) {
		int regnum = index - VFIO_PCI_NUM_REGIONS;
		struct vfio_pci_region *region = vdev->region + regnum;

		if (region && region->ops && region->ops->mmap &&
		    (region->flags & VFIO_REGION_INFO_FLAG_MMAP))
			return region->ops->mmap(vdev, region, vma);
		return -EINVAL;
	}
	if (index >= VFIO_PCI_ROM_REGION_INDEX)
		return -EINVAL;
	if (!vdev->bar_mmap_supported[index])
		return -EINVAL;

	phys_len = PAGE_ALIGN(pci_resource_len(pdev, index));
	req_len = vma->vm_end - vma->vm_start;
	pgoff = vma->vm_pgoff &
		((1U << (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT)) - 1);
	req_start = pgoff << PAGE_SHIFT;

	if (req_start + req_len > phys_len)
		return -EINVAL;

	/*
	 * Even though we don't make use of the barmap for the mmap,
	 * we need to request the region and the barmap tracks that.
	 */
	if (!vdev->barmap[index]) {
		ret = pci_request_selected_regions(pdev,
						   1 << index, "vfio-pci");
		if (ret)
			return ret;

		vdev->barmap[index] = pci_iomap(pdev, index, 0);
		if (!vdev->barmap[index]) {
			pci_release_selected_regions(pdev, 1 << index);
			return -ENOMEM;
		}
	}

	vma->vm_private_data = vdev;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff = (pci_resource_start(pdev, index) >> PAGE_SHIFT) + pgoff;

	/*
	 * See remap_pfn_range(), called from vfio_pci_fault() but we can't
	 * change vm_flags within the fault handler.  Set them now.
	 */
	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &vfio_pci_mmap_ops;

	return 0;
}

static void vfio_pci_request(void *device_data, unsigned int count)
{
	struct vfio_pci_device *vdev = device_data;
	struct pci_dev *pdev = vdev->pdev;

	mutex_lock(&vdev->igate);

	if (vdev->req_trigger) {
		if (!(count % 10))
			pci_notice_ratelimited(pdev,
				"Relaying device request to user (#%u)\n",
				count);
		eventfd_signal(vdev->req_trigger, 1);
	} else if (count == 0) {
		pci_warn(pdev,
			"No device request channel registered, blocked until released by user\n");
	}

	mutex_unlock(&vdev->igate);
}

static int vfio_pci_validate_vf_token(struct vfio_pci_device *vdev,
				      bool vf_token, uuid_t *uuid)
{
	/*
	 * There's always some degree of trust or collaboration between SR-IOV
	 * PF and VFs, even if just that the PF hosts the SR-IOV capability and
	 * can disrupt VFs with a reset, but often the PF has more explicit
	 * access to deny service to the VF or access data passed through the
	 * VF.  We therefore require an opt-in via a shared VF token (UUID) to
	 * represent this trust.  This both prevents that a VF driver might
	 * assume the PF driver is a trusted, in-kernel driver, and also that
	 * a PF driver might be replaced with a rogue driver, unknown to in-use
	 * VF drivers.
	 *
	 * Therefore when presented with a VF, if the PF is a vfio device and
	 * it is bound to the vfio-pci driver, the user needs to provide a VF
	 * token to access the device, in the form of appending a vf_token to
	 * the device name, for example:
	 *
	 * "0000:04:10.0 vf_token=bd8d9d2b-5a5f-4f5a-a211-f591514ba1f3"
	 *
	 * When presented with a PF which has VFs in use, the user must also
	 * provide the current VF token to prove collaboration with existing
	 * VF users.  If VFs are not in use, the VF token provided for the PF
	 * device will act to set the VF token.
	 *
	 * If the VF token is provided but unused, an error is generated.
	 */
	if (!vdev->pdev->is_virtfn && !vdev->vf_token && !vf_token)
		return 0; /* No VF token provided or required */

	if (vdev->pdev->is_virtfn) {
		struct vfio_device *pf_dev;
		struct vfio_pci_device *pf_vdev = get_pf_vdev(vdev, &pf_dev);
		bool match;

		if (!pf_vdev) {
			if (!vf_token)
				return 0; /* PF is not vfio-pci, no VF token */

			pci_info_ratelimited(vdev->pdev,
				"VF token incorrectly provided, PF not bound to vfio-pci\n");
			return -EINVAL;
		}

		if (!vf_token) {
			vfio_device_put(pf_dev);
			pci_info_ratelimited(vdev->pdev,
				"VF token required to access device\n");
			return -EACCES;
		}

		mutex_lock(&pf_vdev->vf_token->lock);
		match = uuid_equal(uuid, &pf_vdev->vf_token->uuid);
		mutex_unlock(&pf_vdev->vf_token->lock);

		vfio_device_put(pf_dev);

		if (!match) {
			pci_info_ratelimited(vdev->pdev,
				"Incorrect VF token provided for device\n");
			return -EACCES;
		}
	} else if (vdev->vf_token) {
		mutex_lock(&vdev->vf_token->lock);
		if (vdev->vf_token->users) {
			if (!vf_token) {
				mutex_unlock(&vdev->vf_token->lock);
				pci_info_ratelimited(vdev->pdev,
					"VF token required to access device\n");
				return -EACCES;
			}

			if (!uuid_equal(uuid, &vdev->vf_token->uuid)) {
				mutex_unlock(&vdev->vf_token->lock);
				pci_info_ratelimited(vdev->pdev,
					"Incorrect VF token provided for device\n");
				return -EACCES;
			}
		} else if (vf_token) {
			uuid_copy(&vdev->vf_token->uuid, uuid);
		}

		mutex_unlock(&vdev->vf_token->lock);
	} else if (vf_token) {
		pci_info_ratelimited(vdev->pdev,
			"VF token incorrectly provided, not a PF or VF\n");
		return -EINVAL;
	}

	return 0;
}

#define VF_TOKEN_ARG "vf_token="

static int vfio_pci_match(void *device_data, char *buf)
{
	struct vfio_pci_device *vdev = device_data;
	bool vf_token = false;
	uuid_t uuid;
	int ret;

	if (strncmp(pci_name(vdev->pdev), buf, strlen(pci_name(vdev->pdev))))
		return 0; /* No match */

	if (strlen(buf) > strlen(pci_name(vdev->pdev))) {
		buf += strlen(pci_name(vdev->pdev));

		if (*buf != ' ')
			return 0; /* No match: non-whitespace after name */

		while (*buf) {
			if (*buf == ' ') {
				buf++;
				continue;
			}

			if (!vf_token && !strncmp(buf, VF_TOKEN_ARG,
						  strlen(VF_TOKEN_ARG))) {
				buf += strlen(VF_TOKEN_ARG);

				if (strlen(buf) < UUID_STRING_LEN)
					return -EINVAL;

				ret = uuid_parse(buf, &uuid);
				if (ret)
					return ret;

				vf_token = true;
				buf += UUID_STRING_LEN;
			} else {
				/* Unknown/duplicate option */
				return -EINVAL;
			}
		}
	}

	ret = vfio_pci_validate_vf_token(vdev, vf_token, &uuid);
	if (ret)
		return ret;

	return 1; /* Match */
}

static const struct vfio_device_ops vfio_pci_ops = {
	.name		= "vfio-pci",
	.open		= vfio_pci_open,
	.release	= vfio_pci_release,
	.ioctl		= vfio_pci_ioctl,
	.read		= vfio_pci_read,
	.write		= vfio_pci_write,
	.mmap		= vfio_pci_mmap,
	.request	= vfio_pci_request,
	.match		= vfio_pci_match,
};

static int vfio_pci_reflck_attach(struct vfio_pci_device *vdev);
static void vfio_pci_reflck_put(struct vfio_pci_reflck *reflck);
static struct pci_driver vfio_pci_driver;

static int vfio_pci_bus_notifier(struct notifier_block *nb,
				 unsigned long action, void *data)
{
	struct vfio_pci_device *vdev = container_of(nb,
						    struct vfio_pci_device, nb);
	struct device *dev = data;
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pci_dev *physfn = pci_physfn(pdev);

	if (action == BUS_NOTIFY_ADD_DEVICE &&
	    pdev->is_virtfn && physfn == vdev->pdev) {
		pci_info(vdev->pdev, "Captured SR-IOV VF %s driver_override\n",
			 pci_name(pdev));
		pdev->driver_override = kasprintf(GFP_KERNEL, "%s",
						  vfio_pci_ops.name);
	} else if (action == BUS_NOTIFY_BOUND_DRIVER &&
		   pdev->is_virtfn && physfn == vdev->pdev) {
		struct pci_driver *drv = pci_dev_driver(pdev);

		if (drv && drv != &vfio_pci_driver)
			pci_warn(vdev->pdev,
				 "VF %s bound to driver %s while PF bound to vfio-pci\n",
				 pci_name(pdev), drv->name);
	}

	return 0;
}

static int vfio_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct vfio_pci_device *vdev;
	struct iommu_group *group;
	int ret;

	if (pdev->hdr_type != PCI_HEADER_TYPE_NORMAL)
		return -EINVAL;

	/*
	 * Prevent binding to PFs with VFs enabled, the VFs might be in use
	 * by the host or other users.  We cannot capture the VFs if they
	 * already exist, nor can we track VF users.  Disabling SR-IOV here
	 * would initiate removing the VFs, which would unbind the driver,
	 * which is prone to blocking if that VF is also in use by vfio-pci.
	 * Just reject these PFs and let the user sort it out.
	 */
	if (pci_num_vf(pdev)) {
		pci_warn(pdev, "Cannot bind to PF with SR-IOV enabled\n");
		return -EBUSY;
	}

	group = vfio_iommu_group_get(&pdev->dev);
	if (!group)
		return -EINVAL;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		ret = -ENOMEM;
		goto out_group_put;
	}

	vdev->pdev = pdev;
	vdev->irq_type = VFIO_PCI_NUM_IRQS;
	mutex_init(&vdev->igate);
	spin_lock_init(&vdev->irqlock);
	mutex_init(&vdev->ioeventfds_lock);
	INIT_LIST_HEAD(&vdev->ioeventfds_list);
	mutex_init(&vdev->vma_lock);
	INIT_LIST_HEAD(&vdev->vma_list);

	ret = vfio_add_group_dev(&pdev->dev, &vfio_pci_ops, vdev);
	if (ret)
		goto out_free;

	ret = vfio_pci_reflck_attach(vdev);
	if (ret)
		goto out_del_group_dev;

	if (pdev->is_physfn) {
		vdev->vf_token = kzalloc(sizeof(*vdev->vf_token), GFP_KERNEL);
		if (!vdev->vf_token) {
			ret = -ENOMEM;
			goto out_reflck;
		}

		mutex_init(&vdev->vf_token->lock);
		uuid_gen(&vdev->vf_token->uuid);

		vdev->nb.notifier_call = vfio_pci_bus_notifier;
		ret = bus_register_notifier(&pci_bus_type, &vdev->nb);
		if (ret)
			goto out_vf_token;
	}

	if (vfio_pci_is_vga(pdev)) {
		vga_client_register(pdev, vdev, NULL, vfio_pci_set_vga_decode);
		vga_set_legacy_decoding(pdev,
					vfio_pci_set_vga_decode(vdev, false));
	}

	vfio_pci_probe_power_state(vdev);

	if (!disable_idle_d3) {
		/*
		 * pci-core sets the device power state to an unknown value at
		 * bootup and after being removed from a driver.  The only
		 * transition it allows from this unknown state is to D0, which
		 * typically happens when a driver calls pci_enable_device().
		 * We're not ready to enable the device yet, but we do want to
		 * be able to get to D3.  Therefore first do a D0 transition
		 * before going to D3.
		 */
		vfio_pci_set_power_state(vdev, PCI_D0);
		vfio_pci_set_power_state(vdev, PCI_D3hot);
	}

	return ret;

out_vf_token:
	kfree(vdev->vf_token);
out_reflck:
	vfio_pci_reflck_put(vdev->reflck);
out_del_group_dev:
	vfio_del_group_dev(&pdev->dev);
out_free:
	kfree(vdev);
out_group_put:
	vfio_iommu_group_put(group, &pdev->dev);
	return ret;
}

static void vfio_pci_remove(struct pci_dev *pdev)
{
	struct vfio_pci_device *vdev;

	pci_disable_sriov(pdev);

	vdev = vfio_del_group_dev(&pdev->dev);
	if (!vdev)
		return;

	if (vdev->vf_token) {
		WARN_ON(vdev->vf_token->users);
		mutex_destroy(&vdev->vf_token->lock);
		kfree(vdev->vf_token);
	}

	if (vdev->nb.notifier_call)
		bus_unregister_notifier(&pci_bus_type, &vdev->nb);

	vfio_pci_reflck_put(vdev->reflck);

	vfio_iommu_group_put(pdev->dev.iommu_group, &pdev->dev);
	kfree(vdev->region);
	mutex_destroy(&vdev->ioeventfds_lock);

	if (!disable_idle_d3)
		vfio_pci_set_power_state(vdev, PCI_D0);

	kfree(vdev->pm_save);
	kfree(vdev);

	if (vfio_pci_is_vga(pdev)) {
		vga_client_register(pdev, NULL, NULL, NULL);
		vga_set_legacy_decoding(pdev,
				VGA_RSRC_NORMAL_IO | VGA_RSRC_NORMAL_MEM |
				VGA_RSRC_LEGACY_IO | VGA_RSRC_LEGACY_MEM);
	}
}

static pci_ers_result_t vfio_pci_aer_err_detected(struct pci_dev *pdev,
						  pci_channel_state_t state)
{
	struct vfio_pci_device *vdev;
	struct vfio_device *device;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (device == NULL)
		return PCI_ERS_RESULT_DISCONNECT;

	vdev = vfio_device_data(device);
	if (vdev == NULL) {
		vfio_device_put(device);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	mutex_lock(&vdev->igate);

	if (vdev->err_trigger)
		eventfd_signal(vdev->err_trigger, 1);

	mutex_unlock(&vdev->igate);

	vfio_device_put(device);

	return PCI_ERS_RESULT_CAN_RECOVER;
}

static int vfio_pci_sriov_configure(struct pci_dev *pdev, int nr_virtfn)
{
	struct vfio_pci_device *vdev;
	struct vfio_device *device;
	int ret = 0;

	might_sleep();

	if (!enable_sriov)
		return -ENOENT;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return -ENODEV;

	vdev = vfio_device_data(device);
	if (!vdev) {
		vfio_device_put(device);
		return -ENODEV;
	}

	if (nr_virtfn == 0)
		pci_disable_sriov(pdev);
	else
		ret = pci_enable_sriov(pdev, nr_virtfn);

	vfio_device_put(device);

	return ret < 0 ? ret : nr_virtfn;
}

static const struct pci_error_handlers vfio_err_handlers = {
	.error_detected = vfio_pci_aer_err_detected,
};

static struct pci_driver vfio_pci_driver = {
	.name			= "vfio-pci",
	.id_table		= NULL, /* only dynamic ids */
	.probe			= vfio_pci_probe,
	.remove			= vfio_pci_remove,
	.sriov_configure	= vfio_pci_sriov_configure,
	.err_handler		= &vfio_err_handlers,
};

static DEFINE_MUTEX(reflck_lock);

static struct vfio_pci_reflck *vfio_pci_reflck_alloc(void)
{
	struct vfio_pci_reflck *reflck;

	reflck = kzalloc(sizeof(*reflck), GFP_KERNEL);
	if (!reflck)
		return ERR_PTR(-ENOMEM);

	kref_init(&reflck->kref);
	mutex_init(&reflck->lock);

	return reflck;
}

static void vfio_pci_reflck_get(struct vfio_pci_reflck *reflck)
{
	kref_get(&reflck->kref);
}

static int vfio_pci_reflck_find(struct pci_dev *pdev, void *data)
{
	struct vfio_pci_reflck **preflck = data;
	struct vfio_device *device;
	struct vfio_pci_device *vdev;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return 0;

	if (pci_dev_driver(pdev) != &vfio_pci_driver) {
		vfio_device_put(device);
		return 0;
	}

	vdev = vfio_device_data(device);

	if (vdev->reflck) {
		vfio_pci_reflck_get(vdev->reflck);
		*preflck = vdev->reflck;
		vfio_device_put(device);
		return 1;
	}

	vfio_device_put(device);
	return 0;
}

static int vfio_pci_reflck_attach(struct vfio_pci_device *vdev)
{
	bool slot = !pci_probe_reset_slot(vdev->pdev->slot);

	mutex_lock(&reflck_lock);

	if (pci_is_root_bus(vdev->pdev->bus) ||
	    vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_reflck_find,
					  &vdev->reflck, slot) <= 0)
		vdev->reflck = vfio_pci_reflck_alloc();

	mutex_unlock(&reflck_lock);

	return PTR_ERR_OR_ZERO(vdev->reflck);
}

static void vfio_pci_reflck_release(struct kref *kref)
{
	struct vfio_pci_reflck *reflck = container_of(kref,
						      struct vfio_pci_reflck,
						      kref);

	kfree(reflck);
	mutex_unlock(&reflck_lock);
}

static void vfio_pci_reflck_put(struct vfio_pci_reflck *reflck)
{
	kref_put_mutex(&reflck->kref, vfio_pci_reflck_release, &reflck_lock);
}

struct vfio_devices {
	struct vfio_device **devices;
	int cur_index;
	int max_index;
};

static int vfio_pci_get_unused_devs(struct pci_dev *pdev, void *data)
{
	struct vfio_devices *devs = data;
	struct vfio_device *device;
	struct vfio_pci_device *vdev;

	if (devs->cur_index == devs->max_index)
		return -ENOSPC;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return -EINVAL;

	if (pci_dev_driver(pdev) != &vfio_pci_driver) {
		vfio_device_put(device);
		return -EBUSY;
	}

	vdev = vfio_device_data(device);

	/* Fault if the device is not unused */
	if (vdev->refcnt) {
		vfio_device_put(device);
		return -EBUSY;
	}

	devs->devices[devs->cur_index++] = device;
	return 0;
}

/*
 * If a bus or slot reset is available for the provided device and:
 *  - All of the devices affected by that bus or slot reset are unused
 *    (!refcnt)
 *  - At least one of the affected devices is marked dirty via
 *    needs_reset (such as by lack of FLR support)
 * Then attempt to perform that bus or slot reset.  Callers are required
 * to hold vdev->reflck->lock, protecting the bus/slot reset group from
 * concurrent opens.  A vfio_device reference is acquired for each device
 * to prevent unbinds during the reset operation.
 *
 * NB: vfio-core considers a group to be viable even if some devices are
 * bound to drivers like pci-stub or pcieport.  Here we require all devices
 * to be bound to vfio_pci since that's the only way we can be sure they
 * stay put.
 */
static void vfio_pci_try_bus_reset(struct vfio_pci_device *vdev)
{
	struct vfio_devices devs = { .cur_index = 0 };
	int i = 0, ret = -EINVAL;
	bool slot = false;
	struct vfio_pci_device *tmp;

	if (!pci_probe_reset_slot(vdev->pdev->slot))
		slot = true;
	else if (pci_probe_reset_bus(vdev->pdev->bus))
		return;

	if (vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_count_devs,
					  &i, slot) || !i)
		return;

	devs.max_index = i;
	devs.devices = kcalloc(i, sizeof(struct vfio_device *), GFP_KERNEL);
	if (!devs.devices)
		return;

	if (vfio_pci_for_each_slot_or_bus(vdev->pdev,
					  vfio_pci_get_unused_devs,
					  &devs, slot))
		goto put_devs;

	/* Does at least one need a reset? */
	for (i = 0; i < devs.cur_index; i++) {
		tmp = vfio_device_data(devs.devices[i]);
		if (tmp->needs_reset) {
			ret = pci_reset_bus(vdev->pdev);
			break;
		}
	}

put_devs:
	for (i = 0; i < devs.cur_index; i++) {
		tmp = vfio_device_data(devs.devices[i]);

		/*
		 * If reset was successful, affected devices no longer need
		 * a reset and we should return all the collateral devices
		 * to low power.  If not successful, we either didn't reset
		 * the bus or timed out waiting for it, so let's not touch
		 * the power state.
		 */
		if (!ret) {
			tmp->needs_reset = false;

			if (tmp != vdev && !disable_idle_d3)
				vfio_pci_set_power_state(tmp, PCI_D3hot);
		}

		vfio_device_put(devs.devices[i]);
	}

	kfree(devs.devices);
}

static void __exit vfio_pci_cleanup(void)
{
	pci_unregister_driver(&vfio_pci_driver);
	vfio_pci_uninit_perm_bits();
}

static void __init vfio_pci_fill_ids(void)
{
	char *p, *id;
	int rc;

	/* no ids passed actually */
	if (ids[0] == '\0')
		return;

	/* add ids specified in the module parameter */
	p = ids;
	while ((id = strsep(&p, ","))) {
		unsigned int vendor, device, subvendor = PCI_ANY_ID,
			subdevice = PCI_ANY_ID, class = 0, class_mask = 0;
		int fields;

		if (!strlen(id))
			continue;

		fields = sscanf(id, "%x:%x:%x:%x:%x:%x",
				&vendor, &device, &subvendor, &subdevice,
				&class, &class_mask);

		if (fields < 2) {
			pr_warn("invalid id string \"%s\"\n", id);
			continue;
		}

		rc = pci_add_dynid(&vfio_pci_driver, vendor, device,
				   subvendor, subdevice, class, class_mask, 0);
		if (rc)
			pr_warn("failed to add dynamic id [%04x:%04x[%04x:%04x]] class %#08x/%08x (%d)\n",
				vendor, device, subvendor, subdevice,
				class, class_mask, rc);
		else
			pr_info("add [%04x:%04x[%04x:%04x]] class %#08x/%08x\n",
				vendor, device, subvendor, subdevice,
				class, class_mask);
	}
}

static int __init vfio_pci_init(void)
{
	int ret;

	/* Allocate shared config space permision data used by all devices */
	ret = vfio_pci_init_perm_bits();
	if (ret)
		return ret;

	/* Register and scan for devices */
	ret = pci_register_driver(&vfio_pci_driver);
	if (ret)
		goto out_driver;

	vfio_pci_fill_ids();

	return 0;

out_driver:
	vfio_pci_uninit_perm_bits();
	return ret;
}

module_init(vfio_pci_init);
module_exit(vfio_pci_cleanup);

MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
	} else if (cmd == VFIO_DEVICE_PCI_HOT_RESET) {
		struct vfio_pci_hot_reset hdr;
		int32_t *group_fds;
		struct vfio_pci_group_entry *groups;
		struct vfio_pci_group_info info;
		bool slot = false;
		int i, count = 0, ret = 0;

		minsz = offsetofend(struct vfio_pci_hot_reset, count);

		if (copy_from_user(&hdr, (void __user *)arg, minsz))
			return -EFAULT;

		if (hdr.argsz < minsz || hdr.flags)
			return -EINVAL;

		/* Can we do a slot or bus reset or neither? */
		if (!pci_probe_reset_slot(vdev->pdev->slot))
			slot = true;
		else if (pci_probe_reset_bus(vdev->pdev->bus))
			return -ENODEV;

		/*
		 * We can't let userspace give us an arbitrarily large
		 * buffer to copy, so verify how many we think there
		 * could be.  Note groups can have multiple devices so
		 * one group per device is the max.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_count_devs,
						    &count, slot);
		if (ret)
			return ret;

		/* Somewhere between 1 and count is OK */
		if (!hdr.count || hdr.count > count)
			return -EINVAL;

		group_fds = kcalloc(hdr.count, sizeof(*group_fds), GFP_KERNEL);
		groups = kcalloc(hdr.count, sizeof(*groups), GFP_KERNEL);
		if (!group_fds || !groups) {
			kfree(group_fds);
			kfree(groups);
			return -ENOMEM;
		}
				   hdr.count * sizeof(*group_fds))) {
			kfree(group_fds);
			kfree(groups);
			return -EFAULT;
		}

		/*
		 * For each group_fd, get the group through the vfio external
		 * user interface and store the group and iommu ID.  This
		 * ensures the group is held across the reset.
		 */
		for (i = 0; i < hdr.count; i++) {
			struct vfio_group *group;
			struct fd f = fdget(group_fds[i]);
			if (!f.file) {
				ret = -EBADF;
				break;
			}

			group = vfio_group_get_external_user(f.file);
			fdput(f);
			if (IS_ERR(group)) {
				ret = PTR_ERR(group);
				break;
			}

			groups[i].group = group;
			groups[i].id = vfio_external_user_iommu_id(group);
		}
			if (IS_ERR(group)) {
				ret = PTR_ERR(group);
				break;
			}

			groups[i].group = group;
			groups[i].id = vfio_external_user_iommu_id(group);
		}

		kfree(group_fds);

		/* release reference to groups on error */
		if (ret)
			goto hot_reset_release;

		info.count = hdr.count;
		info.groups = groups;

		/*
		 * Test whether all the affected devices are contained
		 * by the set of groups provided by the user.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_validate_devs,
						    &info, slot);
		if (!ret)
			/* User has access, do the reset */
			ret = pci_reset_bus(vdev->pdev);

hot_reset_release:
		for (i--; i >= 0; i--)
			vfio_group_put_external_user(groups[i].group);

		kfree(groups);
		return ret;
	} else if (cmd == VFIO_DEVICE_IOEVENTFD) {
		struct vfio_device_ioeventfd ioeventfd;
		int count;

		minsz = offsetofend(struct vfio_device_ioeventfd, fd);

		if (copy_from_user(&ioeventfd, (void __user *)arg, minsz))
			return -EFAULT;

		if (ioeventfd.argsz < minsz)
			return -EINVAL;

		if (ioeventfd.flags & ~VFIO_DEVICE_IOEVENTFD_SIZE_MASK)
			return -EINVAL;

		count = ioeventfd.flags & VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

		if (hweight8(count) != 1 || ioeventfd.fd < -1)
			return -EINVAL;

		return vfio_pci_ioeventfd(vdev, ioeventfd.offset,
					  ioeventfd.data, count, ioeventfd.fd);
	} else if (cmd == VFIO_DEVICE_FEATURE) {
		struct vfio_device_feature feature;
		uuid_t uuid;

		minsz = offsetofend(struct vfio_device_feature, flags);

		if (copy_from_user(&feature, (void __user *)arg, minsz))
			return -EFAULT;

		if (feature.argsz < minsz)
			return -EINVAL;

		/* Check unknown flags */
		if (feature.flags & ~(VFIO_DEVICE_FEATURE_MASK |
				      VFIO_DEVICE_FEATURE_SET |
				      VFIO_DEVICE_FEATURE_GET |
				      VFIO_DEVICE_FEATURE_PROBE))
			return -EINVAL;

		/* GET & SET are mutually exclusive except with PROBE */
		if (!(feature.flags & VFIO_DEVICE_FEATURE_PROBE) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_SET) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_GET))
			return -EINVAL;

		switch (feature.flags & VFIO_DEVICE_FEATURE_MASK) {
		case VFIO_DEVICE_FEATURE_PCI_VF_TOKEN:
			if (!vdev->vf_token)
				return -ENOTTY;

			/*
			 * We do not support GET of the VF Token UUID as this
			 * could expose the token of the previous device user.
			 */
			if (feature.flags & VFIO_DEVICE_FEATURE_GET)
				return -EINVAL;

			if (feature.flags & VFIO_DEVICE_FEATURE_PROBE)
				return 0;

			/* Don't SET unless told to do so */
			if (!(feature.flags & VFIO_DEVICE_FEATURE_SET))
				return -EINVAL;

			if (feature.argsz < minsz + sizeof(uuid))
				return -EINVAL;

			if (copy_from_user(&uuid, (void __user *)(arg + minsz),
					   sizeof(uuid)))
				return -EFAULT;

			mutex_lock(&vdev->vf_token->lock);
			uuid_copy(&vdev->vf_token->uuid, &uuid);
			mutex_unlock(&vdev->vf_token->lock);

			return 0;
		default:
			return -ENOTTY;
		}
			if (IS_ERR(group)) {
				ret = PTR_ERR(group);
				break;
			}

			groups[i].group = group;
			groups[i].id = vfio_external_user_iommu_id(group);
		}

		kfree(group_fds);

		/* release reference to groups on error */
		if (ret)
			goto hot_reset_release;

		info.count = hdr.count;
		info.groups = groups;

		/*
		 * Test whether all the affected devices are contained
		 * by the set of groups provided by the user.
		 */
		ret = vfio_pci_for_each_slot_or_bus(vdev->pdev,
						    vfio_pci_validate_devs,
						    &info, slot);
		if (!ret)
			/* User has access, do the reset */
			ret = pci_reset_bus(vdev->pdev);

hot_reset_release:
		for (i--; i >= 0; i--)
			vfio_group_put_external_user(groups[i].group);

		kfree(groups);
		return ret;
	} else if (cmd == VFIO_DEVICE_IOEVENTFD) {
		struct vfio_device_ioeventfd ioeventfd;
		int count;

		minsz = offsetofend(struct vfio_device_ioeventfd, fd);

		if (copy_from_user(&ioeventfd, (void __user *)arg, minsz))
			return -EFAULT;

		if (ioeventfd.argsz < minsz)
			return -EINVAL;

		if (ioeventfd.flags & ~VFIO_DEVICE_IOEVENTFD_SIZE_MASK)
			return -EINVAL;

		count = ioeventfd.flags & VFIO_DEVICE_IOEVENTFD_SIZE_MASK;

		if (hweight8(count) != 1 || ioeventfd.fd < -1)
			return -EINVAL;

		return vfio_pci_ioeventfd(vdev, ioeventfd.offset,
					  ioeventfd.data, count, ioeventfd.fd);
	} else if (cmd == VFIO_DEVICE_FEATURE) {
		struct vfio_device_feature feature;
		uuid_t uuid;

		minsz = offsetofend(struct vfio_device_feature, flags);

		if (copy_from_user(&feature, (void __user *)arg, minsz))
			return -EFAULT;

		if (feature.argsz < minsz)
			return -EINVAL;

		/* Check unknown flags */
		if (feature.flags & ~(VFIO_DEVICE_FEATURE_MASK |
				      VFIO_DEVICE_FEATURE_SET |
				      VFIO_DEVICE_FEATURE_GET |
				      VFIO_DEVICE_FEATURE_PROBE))
			return -EINVAL;

		/* GET & SET are mutually exclusive except with PROBE */
		if (!(feature.flags & VFIO_DEVICE_FEATURE_PROBE) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_SET) &&
		    (feature.flags & VFIO_DEVICE_FEATURE_GET))
			return -EINVAL;

		switch (feature.flags & VFIO_DEVICE_FEATURE_MASK) {
		case VFIO_DEVICE_FEATURE_PCI_VF_TOKEN:
			if (!vdev->vf_token)
				return -ENOTTY;

			/*
			 * We do not support GET of the VF Token UUID as this
			 * could expose the token of the previous device user.
			 */
			if (feature.flags & VFIO_DEVICE_FEATURE_GET)
				return -EINVAL;

			if (feature.flags & VFIO_DEVICE_FEATURE_PROBE)
				return 0;

			/* Don't SET unless told to do so */
			if (!(feature.flags & VFIO_DEVICE_FEATURE_SET))
				return -EINVAL;

			if (feature.argsz < minsz + sizeof(uuid))
				return -EINVAL;

			if (copy_from_user(&uuid, (void __user *)(arg + minsz),
					   sizeof(uuid)))
				return -EFAULT;

			mutex_lock(&vdev->vf_token->lock);
			uuid_copy(&vdev->vf_token->uuid, &uuid);
			mutex_unlock(&vdev->vf_token->lock);

			return 0;
		default:
			return -ENOTTY;
		}
	}

	return -ENOTTY;
}
{
	if (!count)
		return 0;

	return vfio_pci_rw(device_data, (char __user *)buf, count, ppos, true);
}

static int vfio_pci_add_vma(struct vfio_pci_device *vdev,
			    struct vm_area_struct *vma)
{
	struct vfio_pci_mmap_vma *mmap_vma;

	mmap_vma = kmalloc(sizeof(*mmap_vma), GFP_KERNEL);
	if (!mmap_vma)
		return -ENOMEM;

	mmap_vma->vma = vma;

	mutex_lock(&vdev->vma_lock);
	list_add(&mmap_vma->vma_next, &vdev->vma_list);
	mutex_unlock(&vdev->vma_lock);

	return 0;
}

/*
 * Zap mmaps on open so that we can fault them in on access and therefore
 * our vma_list only tracks mappings accessed since last zap.
 */
static void vfio_pci_mmap_open(struct vm_area_struct *vma)
{
	zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
}

static void vfio_pci_mmap_close(struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = vma->vm_private_data;
	struct vfio_pci_mmap_vma *mmap_vma;

	mutex_lock(&vdev->vma_lock);
	list_for_each_entry(mmap_vma, &vdev->vma_list, vma_next) {
		if (mmap_vma->vma == vma) {
			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);
			break;
		}
	}
{
	struct vfio_pci_mmap_vma *mmap_vma;

	mmap_vma = kmalloc(sizeof(*mmap_vma), GFP_KERNEL);
	if (!mmap_vma)
		return -ENOMEM;

	mmap_vma->vma = vma;

	mutex_lock(&vdev->vma_lock);
	list_add(&mmap_vma->vma_next, &vdev->vma_list);
	mutex_unlock(&vdev->vma_lock);

	return 0;
}

/*
 * Zap mmaps on open so that we can fault them in on access and therefore
 * our vma_list only tracks mappings accessed since last zap.
 */
static void vfio_pci_mmap_open(struct vm_area_struct *vma)
{
	zap_vma_ptes(vma, vma->vm_start, vma->vm_end - vma->vm_start);
}

static void vfio_pci_mmap_close(struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = vma->vm_private_data;
	struct vfio_pci_mmap_vma *mmap_vma;

	mutex_lock(&vdev->vma_lock);
	list_for_each_entry(mmap_vma, &vdev->vma_list, vma_next) {
		if (mmap_vma->vma == vma) {
			list_del(&mmap_vma->vma_next);
			kfree(mmap_vma);
			break;
		}
	}
{
	struct vm_area_struct *vma = vmf->vma;
	struct vfio_pci_device *vdev = vma->vm_private_data;

	if (vfio_pci_add_vma(vdev, vma))
		return VM_FAULT_OOM;

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return VM_FAULT_SIGBUS;

	return VM_FAULT_NOPAGE;
}

static const struct vm_operations_struct vfio_pci_mmap_ops = {
	.open = vfio_pci_mmap_open,
	.close = vfio_pci_mmap_close,
	.fault = vfio_pci_mmap_fault,
};

static int vfio_pci_mmap(void *device_data, struct vm_area_struct *vma)
{
	struct vfio_pci_device *vdev = device_data;
	struct pci_dev *pdev = vdev->pdev;
	unsigned int index;
	u64 phys_len, req_len, pgoff, req_start;
	int ret;

	index = vma->vm_pgoff >> (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT);

	if (vma->vm_end < vma->vm_start)
		return -EINVAL;
	if ((vma->vm_flags & VM_SHARED) == 0)
		return -EINVAL;
	if (index >= VFIO_PCI_NUM_REGIONS) {
		int regnum = index - VFIO_PCI_NUM_REGIONS;
		struct vfio_pci_region *region = vdev->region + regnum;

		if (region && region->ops && region->ops->mmap &&
		    (region->flags & VFIO_REGION_INFO_FLAG_MMAP))
			return region->ops->mmap(vdev, region, vma);
		return -EINVAL;
	}
	if (index >= VFIO_PCI_ROM_REGION_INDEX)
		return -EINVAL;
	if (!vdev->bar_mmap_supported[index])
		return -EINVAL;

	phys_len = PAGE_ALIGN(pci_resource_len(pdev, index));
	req_len = vma->vm_end - vma->vm_start;
	pgoff = vma->vm_pgoff &
		((1U << (VFIO_PCI_OFFSET_SHIFT - PAGE_SHIFT)) - 1);
	req_start = pgoff << PAGE_SHIFT;

	if (req_start + req_len > phys_len)
		return -EINVAL;

	/*
	 * Even though we don't make use of the barmap for the mmap,
	 * we need to request the region and the barmap tracks that.
	 */
	if (!vdev->barmap[index]) {
		ret = pci_request_selected_regions(pdev,
						   1 << index, "vfio-pci");
		if (ret)
			return ret;

		vdev->barmap[index] = pci_iomap(pdev, index, 0);
		if (!vdev->barmap[index]) {
			pci_release_selected_regions(pdev, 1 << index);
			return -ENOMEM;
		}
	}

	vma->vm_private_data = vdev;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff = (pci_resource_start(pdev, index) >> PAGE_SHIFT) + pgoff;

	/*
	 * See remap_pfn_range(), called from vfio_pci_fault() but we can't
	 * change vm_flags within the fault handler.  Set them now.
	 */
	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &vfio_pci_mmap_ops;

	return 0;
}
	if (!vdev) {
		ret = -ENOMEM;
		goto out_group_put;
	}

	vdev->pdev = pdev;
	vdev->irq_type = VFIO_PCI_NUM_IRQS;
	mutex_init(&vdev->igate);
	spin_lock_init(&vdev->irqlock);
	mutex_init(&vdev->ioeventfds_lock);
	INIT_LIST_HEAD(&vdev->ioeventfds_list);
	mutex_init(&vdev->vma_lock);
	INIT_LIST_HEAD(&vdev->vma_list);

	ret = vfio_add_group_dev(&pdev->dev, &vfio_pci_ops, vdev);
	if (ret)
		goto out_free;

	ret = vfio_pci_reflck_attach(vdev);
	if (ret)
		goto out_del_group_dev;

	if (pdev->is_physfn) {
		vdev->vf_token = kzalloc(sizeof(*vdev->vf_token), GFP_KERNEL);
		if (!vdev->vf_token) {
			ret = -ENOMEM;
			goto out_reflck;
		}

		mutex_init(&vdev->vf_token->lock);
		uuid_gen(&vdev->vf_token->uuid);

		vdev->nb.notifier_call = vfio_pci_bus_notifier;
		ret = bus_register_notifier(&pci_bus_type, &vdev->nb);
		if (ret)
			goto out_vf_token;
	}
{
	kref_put_mutex(&reflck->kref, vfio_pci_reflck_release, &reflck_lock);
}

struct vfio_devices {
	struct vfio_device **devices;
	int cur_index;
	int max_index;
};

static int vfio_pci_get_unused_devs(struct pci_dev *pdev, void *data)
{
	struct vfio_devices *devs = data;
	struct vfio_device *device;
	struct vfio_pci_device *vdev;

	if (devs->cur_index == devs->max_index)
		return -ENOSPC;

	device = vfio_device_get_from_dev(&pdev->dev);
	if (!device)
		return -EINVAL;

	if (pci_dev_driver(pdev) != &vfio_pci_driver) {
		vfio_device_put(device);
		return -EBUSY;
	}

	vdev = vfio_device_data(device);

	/* Fault if the device is not unused */
	if (vdev->refcnt) {
		vfio_device_put(device);
		return -EBUSY;
	}

	devs->devices[devs->cur_index++] = device;
	return 0;
}
	if (vdev->refcnt) {
		vfio_device_put(device);
		return -EBUSY;
	}

	devs->devices[devs->cur_index++] = device;
	return 0;
}

/*
 * If a bus or slot reset is available for the provided device and:
 *  - All of the devices affected by that bus or slot reset are unused
 *    (!refcnt)
 *  - At least one of the affected devices is marked dirty via
 *    needs_reset (such as by lack of FLR support)
 * Then attempt to perform that bus or slot reset.  Callers are required
 * to hold vdev->reflck->lock, protecting the bus/slot reset group from
 * concurrent opens.  A vfio_device reference is acquired for each device
 * to prevent unbinds during the reset operation.
 *
 * NB: vfio-core considers a group to be viable even if some devices are
 * bound to drivers like pci-stub or pcieport.  Here we require all devices
 * to be bound to vfio_pci since that's the only way we can be sure they
 * stay put.
 */
static void vfio_pci_try_bus_reset(struct vfio_pci_device *vdev)
{
	struct vfio_devices devs = { .cur_index = 0 };
	int i = 0, ret = -EINVAL;
	bool slot = false;
	struct vfio_pci_device *tmp;

	if (!pci_probe_reset_slot(vdev->pdev->slot))
		slot = true;
	else if (pci_probe_reset_bus(vdev->pdev->bus))
		return;

	if (vfio_pci_for_each_slot_or_bus(vdev->pdev, vfio_pci_count_devs,
					  &i, slot) || !i)
		return;

	devs.max_index = i;
	devs.devices = kcalloc(i, sizeof(struct vfio_device *), GFP_KERNEL);
	if (!devs.devices)
		return;

	if (vfio_pci_for_each_slot_or_bus(vdev->pdev,
					  vfio_pci_get_unused_devs,
					  &devs, slot))
		goto put_devs;

	/* Does at least one need a reset? */
	for (i = 0; i < devs.cur_index; i++) {
		tmp = vfio_device_data(devs.devices[i]);
		if (tmp->needs_reset) {
			ret = pci_reset_bus(vdev->pdev);
			break;
		}
	}