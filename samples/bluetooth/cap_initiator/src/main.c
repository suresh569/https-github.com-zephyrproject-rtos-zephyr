/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "cap_initiator.h"

LOG_MODULE_REGISTER(cap_initiator, LOG_LEVEL_INF);

int main(void)
{
	int err;

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth enable failed: %d", err);

		return 0;
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_BT_CAP_INITIATOR_UNICAST)) {
		err = cap_initiator_unicast();
	}
	/* TODO: Add CAP initiator broadcast support*/

	if (err != 0) {
		LOG_ERR("Failed to run CAP Initiator: %d", err);
	}

	return 0;
}
