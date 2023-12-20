/*
 * Copyright (c) 2023 Yonatan Schachter
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/bindesc.h>
#include <zephyr/drivers/flash.h>

BINDESC_STR_DEFINE(my_string, 1, "Hello world!");
BINDESC_UINT_DEFINE(my_int, 2, 5);
BINDESC_BYTES_DEFINE(my_bytes, 3, ({1, 2, 3, 4}));

const struct device *flash = DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller));
extern const uint32_t _bindesc_start;
int dump_descriptors_callback(const struct bindesc_entry *entry, void *user_data)
{
	ARG_UNUSED(user_data);
	size_t i;

	printk("tag: %hu len: %hu data: ", entry->tag, entry->len);
	switch (BINDESC_GET_TAG_TYPE(entry->tag)) {
	case BINDESC_TYPE_UINT:
		printk("%u\n", *(const uint32_t *)entry->data);
		break;
	case BINDESC_TYPE_STR:
		printk("%s\n", (const char *)entry->data);
		break;
	case BINDESC_TYPE_BYTES:
		for (i = 0; i < entry->len; i++) {
			printk("%02x ", ((const uint8_t *)entry->data)[i]);
		}
		printk("\n");
		break;

	default:
		printk("\n");
		break;
	}
	return 0;
}

int main(void)
{
	struct bindesc_handle handle;
	size_t bindesc_offset = 0xffff;
	const uint32_t *number;
	uint8_t buffer[200];
	const uint8_t *bytes;
	const char *version;
	int retval;
	size_t size;
	size_t i;

	for (i = 0; i < 0x400; i += 4) {
		if (*(uint64_t *)(CONFIG_FLASH_BASE_ADDRESS + i) == BINDESC_MAGIC) {
			printk("Found descriptors at 0x%x\n", i);
			bindesc_offset = i;
		}
	}
	if (i == 0xffff) {
		printk("Descriptors not found\n");
		return 1;
	}

	printk("\nReading using memory mapped flash:\n");

	bindesc_open_memory_mapped_flash(&handle, bindesc_offset);
	bindesc_foreach(&handle, dump_descriptors_callback, NULL);

	bindesc_find_str(&handle, BINDESC_ID_KERNEL_VERSION_STRING, &version);
	printk("Zephyr version: %s\n", version);

	bindesc_get_size(&handle, &size);
	printk("Bindesc size: %u\n", size);


	printk("\nReading using RAM:\n");

	flash_read(flash, bindesc_offset, buffer, sizeof(buffer));

	bindesc_open_ram(&handle, buffer, sizeof(buffer));

	/* Search for a non-existent descriptor */
	retval = bindesc_find_str(&handle, 123, &version);
	if (retval) {
		printk("Descriptor not found!\n");
	}

	bindesc_find_uint(&handle, BINDESC_ID_KERNEL_VERSION_MAJOR, &number);
	printk("Zephyr version number: %u\n", *number);


	printk("\nReading using flash:\n");

	bindesc_open_flash(&handle, bindesc_offset, flash);

	bindesc_find_bytes(&handle, 3, &bytes, &size);
	printk("my_bytes: ");
	for (i = 0; i < size; i++) {
		printk("%02x ", bytes[i]);
	}
	printk("\n");

	return 0;
}
