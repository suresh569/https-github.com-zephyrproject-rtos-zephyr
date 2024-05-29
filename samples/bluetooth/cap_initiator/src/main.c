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

	/* Broadcast is started first as the unicast part is run as loop */
	if (IS_ENABLED(CONFIG_SAMPLE_BROADCAST)) {
		err = cap_initiator_broadcast();

		if (err != 0) {
			LOG_ERR("Failed to run CAP Initiator as broadcaster: %d", err);
		}
	}

	if (IS_ENABLED(CONFIG_SAMPLE_UNICAST)) {
		err = cap_initiator_unicast();

		if (err != 0) {
			LOG_ERR("Failed to run CAP Initiator as unicast: %d", err);
		}
	}

	return 0;
}
