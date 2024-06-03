/* Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/addr.h>

#include "common.h"
#include "zephyr/sys/atomic.h"
#include "zephyr/sys/atomic_builtin.h"
#include <string.h>

LOG_MODULE_REGISTER(bt_bsim_scan_start_stop, LOG_LEVEL_DBG);

#define CHANNEL_ID 0
#define MSG_SIZE 1

static atomic_t flag_adv_report_received;
static atomic_t flag_periodic_sync_established;
static bt_addr_le_t adv_addr;

void bt_sync_established_cb(struct bt_le_per_adv_sync *sync,
			    struct bt_le_per_adv_sync_synced_info *info)
{
	LOG_DBG("Periodic sync established");
	atomic_set(&flag_periodic_sync_established, true);
}

static struct bt_le_per_adv_sync_cb sync_callbacks = {
	.synced = bt_sync_established_cb,
};

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	memcpy(&adv_addr, addr, sizeof(adv_addr));

	bt_addr_le_to_str(&adv_addr, addr_str, sizeof(addr_str));
	LOG_DBG("Device found: %s (RSSI %d), type %u, AD data len %u",
	       addr_str, rssi, type, ad->len);
	atomic_set(&flag_adv_report_received, true);
}

void run_dut(void)
{
	LOG_DBG("start");
	backchannel_init();
	int err;

	LOG_DBG("Starting DUT");

	err = bt_enable(NULL);
	if (err) {
		FAIL("Bluetooth init failed (err %d)\n", err);
	}

	LOG_DBG("Bluetooth initialised");

	/* Try to establish a sync to a periodic advertiser.
	 * This will start the scanner.
	 */
	memset(&adv_addr, 0x00, sizeof(adv_addr));
	struct bt_le_per_adv_sync_param per_sync_param = {.addr = adv_addr,
							  .options = 0x0,
							  .sid = 0x0,
							  .skip = 0x0,
							  .timeout = BT_GAP_PER_ADV_MAX_TIMEOUT};
	struct bt_le_per_adv_sync *p_per_sync;

	bt_le_per_adv_sync_cb_register(&sync_callbacks);

	err = bt_le_per_adv_sync_create(&per_sync_param, &p_per_sync);
	if (err) {
		FAIL("Periodic sync setup failed (err %d)\n", err);
	}
	LOG_DBG("Periodic sync started");

	/* Start scanner. Check that we can start the scanner while it is already
	 * running due to the periodic sync.
	 */
	struct bt_le_scan_param scan_params = {.type = BT_LE_SCAN_TYPE_ACTIVE,
					       .options = 0x0,
					       .interval = 123,
					       .window = 12,
					       .interval_coded = 222,
					       .window_coded = 32};

	err = bt_le_scan_start(&scan_params, device_found);
	if (err) {
		FAIL("Scanner setup failed (err %d)\n", err);
	}
	LOG_DBG("Explicit scanner started");

	LOG_DBG("Wait for an advertising report");
	while (!atomic_get(&flag_adv_report_received)) {
		k_msleep(100);
	}

	/* Stop the scanner. That should not affect the periodic advertising sync. */
	err = bt_le_scan_stop();
	if (err) {
		FAIL("Scanner stop failed (err %d)\n", err);
	}
	LOG_DBG("Explicit scanner stopped");

	/* We should be able to stop the periodic advertising sync. */
	err = bt_le_per_adv_sync_delete(p_per_sync);
	if (err) {
		FAIL("Periodic sync stop failed (err %d)\n", err);
	}
	LOG_DBG("Periodic sync stopped");

	/* Start the periodic advertising sync. This time, provide the address of the advertiser
	 * which it shold connect to.
	 */
	per_sync_param = (struct bt_le_per_adv_sync_param) {
		.addr = adv_addr,
		.options = 0x0,
		.sid = 0x0,
		.skip = 0x0,
		.timeout = BT_GAP_PER_ADV_MAX_TIMEOUT
	};
	err = bt_le_per_adv_sync_create(&per_sync_param, &p_per_sync);
	if (err) {
		FAIL("Periodic sync setup failed (err %d)\n", err);
	}
	LOG_DBG("Periodic sync started");

	/* Start the explicit scanner */
	err = bt_le_scan_start(&scan_params, device_found);
	if (err) {
		FAIL("Scanner setup failed (err %d)\n", err);
	}
	LOG_DBG("Explicit scanner started");

	/* Stop the explicit scanner. This should not stop scanner, since we still try to establish
	 * a sync.
	 */
	err = bt_le_scan_stop();
	if (err) {
		FAIL("Scanner stop failed (err %d)\n", err);
	}
	LOG_DBG("Explicit scanner stopped");

	/* Signal to the tester to start the periodic adv. */
	backchannel_sync_send();

	/* Validate that we can still establish a sync */
	LOG_DBG("Wait for sync to periodic adv");
	while (!atomic_get(&flag_periodic_sync_established)) {
		k_msleep(100);
	}

	/* Signal to the tester to end the test. */
	backchannel_sync_send();

	/* Wait for the workqueue to complete before switching device */
	k_msleep(100);

	PASS("Test passed (DUT)\n");
}

void run_tester(void)
{
	LOG_DBG("start");
	backchannel_init();

	int err;

	LOG_DBG("Starting DUT");

	err = bt_enable(NULL);
	if (err) {
		FAIL("Bluetooth init failed (err %d)\n", err);
	}

	LOG_DBG("Bluetooth initialised");

	struct bt_le_ext_adv *per_adv;

	struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_EXT_ADV,
								BT_GAP_ADV_FAST_INT_MIN_2,
								BT_GAP_ADV_FAST_INT_MAX_2,
								NULL);

	err = bt_le_ext_adv_create(&adv_param, NULL, &per_adv);
	if (err) {
		LOG_DBG("Failed to create advertising set: %d", err);
		return;
	}
	LOG_DBG("Created extended advertising set.");

	err = bt_le_ext_adv_start(per_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		FAIL("Failed to start extended advertising: %d", err);
		return;
	}
	LOG_DBG("Started extended advertising.");

	/* Wait for the DUT before starting the periodic advertising */
	backchannel_sync_wait();
	err = bt_le_per_adv_set_param(per_adv, BT_LE_PER_ADV_DEFAULT);
	if (err) {
		FAIL("Failed to set periodic advertising parameters: %d",
		       err);
		return;
	}
	LOG_DBG("Periodic advertising parameters set.");

	err = bt_le_per_adv_start(per_adv);
	if (err) {
		FAIL("Failed to start periodic advertising: %d", err);
		return;
	}
	LOG_DBG("Periodic advertising started.");

	/* Wait for the signal from the DUT before finishing the test */
	backchannel_sync_wait();

	/* Wait for the workqueue to complete before switching device */
	k_msleep(100);

	PASS("Test passed (Tester)\n");
}

static const struct bst_test_instance test_def[] = {
	{
		.test_id = "scanner",
		.test_descr = "SCANNER",
		.test_post_init_f = test_init,
		.test_tick_f = test_tick,
		.test_main_f = run_dut,
	},
	{
		.test_id = "periodic_adv",
		.test_descr = "PER_ADV",
		.test_post_init_f = test_init,
		.test_tick_f = test_tick,
		.test_main_f = run_tester,
	},
	BSTEST_END_MARKER};

struct bst_test_list *test_scan_start_stop_install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_def);
}

bst_test_install_t test_installers[] = {test_scan_start_stop_install, NULL};

int main(void)
{
	bst_main();
	return 0;
}
