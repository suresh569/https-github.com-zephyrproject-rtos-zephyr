/*
 * Copyright (c) 2017-2021 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SUBSYS_BLUETOOTH_HOST_SCAN_H_
#define SUBSYS_BLUETOOTH_HOST_SCAN_H_

#include <stdint.h>

#include <zephyr/sys/atomic.h>
#include <zephyr/bluetooth/bluetooth.h>

/**
 * Reasons why a scanner can be running.
 * Used as input to @ref bt_le_scan_update_and_reconfigure.
 */
enum scan_enabled_reason {
	/** The application explicitly instructed the stack to scan for advertisers
	 * using the API @ref bt_le_scan_start().
	 * May not be used explicitly.
	 */
	SCAN_ENABLED_REASON_EXPLICIT_SCAN,

	/**
	 * Syncing to a periodic advertiser.
	 */
	SCAN_ENABLED_REASON_SYNC_SYNCING,

	/**
	 * Scanning to find devices to connect to.
	 */
	SCAN_ENABLED_REASON_SCAN_BEFORE_INITIATE,

	/**
	 * Special state for a NOP for @ref bt_le_scan_update_and_reconfigure to not add/remove any
	 * reason why the scanner should be running.
	 */
	SCAN_UPDATE_REASON_NONE,
	SCAN_ENABLED_REASON_NUM_FLAGS,
};

void bt_scan_reset(void);

bool bt_id_scan_random_addr_check(void);
bool bt_le_scan_active_scanner_running(void);

int bt_le_scan_set_enable(uint8_t enable);

struct bt_le_per_adv_sync *bt_hci_get_per_adv_sync(uint16_t handle);

void bt_periodic_sync_disable(void);

/**
 * Start / stop / update the scanner.
 *
 * This API updates the reasons why the scanner is running.
 * Depending on all the reasons why the scanner is enabled, scan parameters are selected
 * and the scanner is started or stopped, if needed.
 * This API may update the scan parameters, for example if the scanner is already running
 * when another reason that demands higher duty-cycle is being added.
 * If all reasons for the scanner to run are removed, this API will stop the scanner.
 *
 * @param enable_reason reason why the scanner should be running
 * @param enable enable or disable the reason in previous argument. Use @ref BT_HCI_LE_SCAN_ENABLE
 *               or @ref BT_HCI_LE_SCAN_DISABLE.
 *
 * @return 0 in case of success, or a negative error code on failure.
 */
int bt_le_scan_update_and_reconfigure(enum scan_enabled_reason enable_reason, uint8_t enable);

/**
 * Check if the explicit scanner was enabled.
 */
bool bt_le_explicit_scanner_running(void);
#endif /* defined SUBSYS_BLUETOOTH_HOST_SCAN_H_ */
