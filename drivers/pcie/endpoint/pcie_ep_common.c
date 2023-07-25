/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2020 Broadcom
 *
 */

#include <zephyr/drivers/pcie/endpoint/pcie_ep.h>
#include <string.h>
#include <zephyr/sys/util.h>

/*
 * During DEVICE_TO_HOST data transfer, in order to make sure that all
 * PCIe writes (posted) have reached Host, i.e. to flush PCIe writes,
 * we need to add a dummy PCIe read (non posted transaction) after each
 * DEVICE_TO_HOST data transfer.
 *
 * There are a few possible scenarios, where we need to *place*
 * a dummy PCIe read.
 * All possible scenarios are captured in the table below.
 *
 * As can be seen in the table, for 64-bit systems, we could just do sys_read8
 * on mapped Host address to generate a dummy PCIe read, before unmapping the
 * address - irrespective of low/high outbound memory usage as core is capable
 * of accessing highmem.
 * Basically, we issue single byte PCIe read with sys_read8.
 *
 * For 32-bit systems, if using low outbound memory for memcpy/DMA,
 * we could do sys_read8 on the mapped address.
 * But, for 32-bit systems using high outbound memory for DMA operation,
 * sys_read8 is not possible, as the core cannot access highmem.
 * In this case, we need to *explicitly* perform PCIe read.
 *
 * +-------------+----------------------+-------------------------------------+
 * |     Core    |  Data transfer with  | OB memory type | Dummy PCIe read    |
 * +-------------+----------------------+----------------+--------------------+
 * | 64-bit      |                      |    highmem     |     sys_read8      |
 * |             |       memcpy         |----------------+--------------------+
 * |             |                      |    lowmem      |     sys_read8      |
 * | (e.g.       +----------------------+----------------+--------------------+
 * | Cortex-A72) |                      |    highmem     |     sys_read8      |
 * |             |       DMA            |----------------+--------------------+
 * |             |                      |    lowmem      |     sys_read8      |
 * +-------------+----------------------+----------------+--------------------+
 * | 32-bit      |                      |    highmem     |        NA          |
 * |             |       memcpy         |----------------+--------------------+
 * |             |                      |    lowmem      |     sys_read8      |
 * | (e.g.       +----------------------+----------------+--------------------+
 * | Cortex-M7)  |                      |    highmem     | Explicit PCIe read |
 * |             |       DMA            |----------------+--------------------+
 * |             |                      |    lowmem      |     sys_read8      |
 * +-------------+----------------------+----------------+--------------------+
 *
 * Based on this explanation, the 2 common APIs below, namely
 * pcie_ep_xfer_data_memcpy and pcie_ep_xfer_data_dma
 * are implemented with dummy PCIe read, phew!
 */

static int pcie_ep_mapped_copy(uint64_t mapped_addr, uintptr_t local_addr,
			       const uint32_t size,
			       const enum xfer_direction dir)
{
	/*
	 * Make sure that address can be generated by core, this condition
	 * would not hit if proper pcie_ob_mem_type is passed by core
	 */
	if ((!IS_ENABLED(CONFIG_64BIT)) && (mapped_addr >> 32)) {
		return -EINVAL;
	}

	if (dir == DEVICE_TO_HOST) {
		memcpy(UINT_TO_POINTER(mapped_addr),
		       UINT_TO_POINTER(local_addr), size);
	} else {
		memcpy(UINT_TO_POINTER(local_addr),
		       UINT_TO_POINTER(mapped_addr), size);
	}

	return 0;
}

/*
 * Helper API to achieve data transfer with memcpy operation
 * through PCIe outbound memory
 */
int pcie_ep_xfer_data_memcpy(const struct device *dev, uint64_t pcie_addr,
			     uintptr_t *local_addr, uint32_t size,
			     enum pcie_ob_mem_type ob_mem_type,
			     enum xfer_direction dir)
{
	uint64_t mapped_addr;
	int mapped_size, ret;
	uint32_t xfer_size, unmapped_size;

	/* Map pcie_addr to outbound memory */
	mapped_size = pcie_ep_map_addr(dev, pcie_addr, &mapped_addr,
				       size, ob_mem_type);
	/* Check if outbound memory mapping succeeded */
	if (mapped_size < 0) {
		return mapped_size;
	}

	ret = pcie_ep_mapped_copy(mapped_addr, (uintptr_t)local_addr,
				  mapped_size, dir);

	/* Check if mapped_copy succeeded */
	if (ret < 0) {
		goto out_unmap;
	}

	/* Flush the PCIe writes upon successful memcpy */
	if (dir == DEVICE_TO_HOST) {
		sys_read8(mapped_addr);
	}

	/* Check if we achieved data transfer for given size */
	if (mapped_size == size) {
		ret = 0;
		goto out_unmap;
	}

	/*
	 * In normal case, we are done with data transfer by now,
	 * but some PCIe address translation hardware requires us to
	 * align Host address to be mapped to the translation window size.
	 * So, even though translation window size is good enough for
	 * size of Host buffer, we may not be able to map entire Host buffer
	 * to given outbound window in one time, and we may need to map
	 * remaining size and complete remaining data transfer
	 */

	pcie_ep_unmap_addr(dev, mapped_addr); /* unmap previous Host buffer */
	xfer_size = mapped_size; /* save already transferred data size */

	unmapped_size = size - mapped_size;
	mapped_size = pcie_ep_map_addr(dev, pcie_addr + xfer_size,
				       &mapped_addr, unmapped_size,
				       ob_mem_type);
	/* Check if outbound memory mapping succeeded */
	if (mapped_size < 0) {
		return mapped_size;
	}

	/*
	 * In second attempt, we must have mapped entire size,
	 * if not just quit here before attempting memcpy
	 */
	if (mapped_size != unmapped_size) {
		ret = -EIO;
		goto out_unmap;
	}

	ret = pcie_ep_mapped_copy(mapped_addr,
				  ((uintptr_t)local_addr) + xfer_size,
				  mapped_size, dir);
	/* Flush the PCIe writes upon successful memcpy */
	if (!ret && (dir == DEVICE_TO_HOST)) {
		sys_read8(mapped_addr);
	}
out_unmap:
	pcie_ep_unmap_addr(dev, mapped_addr);
	return ret;
}

/*
 * Helper API to achieve data transfer with DMA operation through
 * PCIe outbound memory, this API is based off pcie_ep_xfer_data_memcpy,
 * here we use "system dma" instead of memcpy
 */
int pcie_ep_xfer_data_dma(const struct device *dev, uint64_t pcie_addr,
			  uintptr_t *local_addr, uint32_t size,
			  enum pcie_ob_mem_type ob_mem_type,
			  enum xfer_direction dir)
{
	uint64_t mapped_addr;
	int mapped_size, ret;
	uint32_t xfer_size, unmapped_size;
	uint32_t dummy_data;	/* For explicit dummy PCIe read */

	/* Map pcie_addr to outbound memory */
	mapped_size = pcie_ep_map_addr(dev, pcie_addr, &mapped_addr,
				       size, ob_mem_type);
	/* Check if outbound memory mapping succeeded */
	if (mapped_size < 0) {
		return mapped_size;
	}

	ret = pcie_ep_dma_xfer(dev, mapped_addr, (uintptr_t)local_addr,
			       mapped_size, dir);
	/* Check if dma succeeded */
	if (ret < 0) {
		goto out_unmap;
	}

	/* Flush the PCIe writes upon successful DMA */
	if (dir == DEVICE_TO_HOST) {
		if (IS_ENABLED(CONFIG_64BIT) || !(mapped_addr >> 32)) {
			sys_read8(mapped_addr);
		}
	}

	pcie_ep_unmap_addr(dev, mapped_addr);

	/*
	 * Explicit PCIe read to flush PCIe writes for 32-bit system
	 * using high outbound memory for DMA operation
	 */
	if (dir == DEVICE_TO_HOST && (!IS_ENABLED(CONFIG_64BIT)) &&
	    (mapped_addr >> 32)) {
		ret = pcie_ep_xfer_data_memcpy(dev, pcie_addr,
					       (uintptr_t *)&dummy_data,
					       sizeof(dummy_data),
					       PCIE_OB_LOWMEM, HOST_TO_DEVICE);
		if (ret < 0) {
			return ret;
		}
	}

	/* Check if we achieved data transfer for given size */
	if (mapped_size == size) {
		return 0;
	}

	/* map remaining size and complete remaining data transfer */

	xfer_size = mapped_size; /* save already transferred data size */

	unmapped_size = size - mapped_size;
	mapped_size = pcie_ep_map_addr(dev, pcie_addr + xfer_size,
				       &mapped_addr, unmapped_size,
				       ob_mem_type);
	/* Check if outbound memory mapping succeeded */
	if (mapped_size < 0) {
		return mapped_size;
	}

	/*
	 * In second attempt, we must have mapped entire size,
	 * if not just quit here before attempting dma
	 */
	if (mapped_size != unmapped_size) {
		ret = -EIO;
		goto out_unmap;
	}

	ret = pcie_ep_dma_xfer(dev, mapped_addr,
			       ((uintptr_t)local_addr) + xfer_size,
			       mapped_size, dir);
	/* Check if dma copy succeeded */
	if (ret < 0) {
		goto out_unmap;
	}

	/* Flush the PCIe writes upon successful DMA */
	if (dir == DEVICE_TO_HOST) {
		if (IS_ENABLED(CONFIG_64BIT) || !(mapped_addr >> 32)) {
			sys_read8(mapped_addr);
		}
	}

	pcie_ep_unmap_addr(dev, mapped_addr);

	/*
	 * Explicit PCIe read to flush PCIe writes for 32-bit system
	 * using high outbound memory for DMA operation
	 */
	if (dir == DEVICE_TO_HOST && (!IS_ENABLED(CONFIG_64BIT)) &&
	    (mapped_addr >> 32)) {
		ret = pcie_ep_xfer_data_memcpy(dev, pcie_addr,
					       (uintptr_t *)&dummy_data,
					       sizeof(dummy_data),
					       PCIE_OB_LOWMEM, HOST_TO_DEVICE);
	}

	return ret;
out_unmap:
	pcie_ep_unmap_addr(dev, mapped_addr);
	return ret;
}
