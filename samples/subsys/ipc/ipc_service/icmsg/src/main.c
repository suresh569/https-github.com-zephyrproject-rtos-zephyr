/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_MULTITHREADING)
#include <zephyr/kernel.h>
#else
#include <zephyr/sys/printk.h>
#endif
#include <zephyr/device.h>

#include <zephyr/ipc/ipc_service.h>
#if defined(CONFIG_SOC_NRF5340_CPUAPP)
#include <nrf53_cpunet_mgmt.h>
#endif
#include <string.h>

#include "common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(host, LOG_LEVEL_INF);

#if defined(CONFIG_MULTITHREADING)
K_SEM_DEFINE(bound_sem, 0, 1);
#else
volatile uint32_t bound_sem = 1;
volatile uint32_t recv_sem = 1;
#endif

static unsigned char expected_message = 'A';
static size_t expected_len = PACKET_SIZE_START;
static size_t received;

static void ep_bound(void *priv)
{
	received = 0;
#if defined(CONFIG_MULTITHREADING)
	k_sem_give(&bound_sem);
#else
	bound_sem = 0;
#endif
	INF("Ep bounded");
}

static void ep_recv(const void *data, size_t len, void *priv)
{
#if defined(CONFIG_ASSERT)
	struct data_packet *packet = (struct data_packet *)data;

	__ASSERT(packet->data[0] == expected_message, "Unexpected message. Expected %c, got %c",
		expected_message, packet->data[0]);
	__ASSERT(len == expected_len, "Unexpected length. Expected %zu, got %zu",
		expected_len, len);
#endif

#ifndef CONFIG_MULTITHREADING
	recv_sem = 0;
#endif

	received += len;
	expected_message++;
	expected_len++;

	if (expected_message > 'Z') {
		expected_message = 'A';
	}

	if (expected_len > sizeof(struct data_packet)) {
		expected_len = PACKET_SIZE_START;
	}
}

static int send_for_time(struct ipc_ept *ep, const int64_t sending_time_ms)
{
	struct data_packet msg = {.data[0] = 'a'};
	size_t mlen = PACKET_SIZE_START;
	size_t bytes_sent = 0;
	int ret = 0;

	INF("Perform sends for %lld [ms]", sending_time_ms);

	int64_t start = k_uptime_get();

	while ((k_uptime_get() - start) < sending_time_ms) {
		ret = ipc_service_send(ep, &msg, mlen);
		if (ret == -ENOMEM) {
			/* No space in the buffer. Retry. */
			ret = 0;
			continue;
		} else if (ret < 0) {
			ERR("Failed to send (%c) failed with ret %d", msg.data[0], ret);
			break;
		}
#if !defined(CONFIG_MULTITHREADING)
		else {
			recv_sem = 1;
		}
#endif

		msg.data[0]++;
		if (msg.data[0] > 'z') {
			msg.data[0] = 'a';
		}

		bytes_sent += mlen;
		mlen++;

		if (mlen > sizeof(struct data_packet)) {
			mlen = PACKET_SIZE_START;
		}

#if defined(CONFIG_MULTITHREADING)
		k_usleep(1);
#else
		while ((recv_sem != 0) && ((k_uptime_get() - start) < sending_time_ms)) {
		};
#endif
	}

	INF("Sent %zu [Bytes] over %lld [ms]", bytes_sent, sending_time_ms);

	return ret;
}

static struct ipc_ept_cfg ep_cfg = {
	.cb = {
		.bound    = ep_bound,
		.received = ep_recv,
	},
};

int main(void)
{
	const struct device *ipc0_instance;
	struct ipc_ept ep;
	int ret;

	INF("IPC-service HOST demo started");

	ipc0_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));

	ret = ipc_service_open_instance(ipc0_instance);
	if ((ret < 0) && (ret != -EALREADY)) {
		ERR("ipc_service_open_instance() failure");
		return ret;
	}

	ret = ipc_service_register_endpoint(ipc0_instance, &ep, &ep_cfg);
	if (ret < 0) {
		ERR("ipc_service_register_endpoint() failure");
		return ret;
	}

#if defined(CONFIG_MULTITHREADING)
	k_sem_take(&bound_sem, K_FOREVER);
#else
	while (bound_sem != 0) {
	};
#endif

	ret = send_for_time(&ep, SENDING_TIME_MS);
	if (ret < 0) {
		ERR("send_for_time() failure");
		return ret;
	}

	INF("Wait 500ms. Let remote core finish its sends");
#if defined(CONFIG_MULTITHREADING)
	k_msleep(500);
#else
	k_busy_wait(500000);
#endif

	INF("Received %zu [Bytes] in total", received);

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
	INF("Stop network core");
	nrf53_cpunet_enable(false);

	INF("Reset IPC service");

	ret = ipc_service_deregister_endpoint(&ep);
	if (ret != 0) {
		ERR("ipc_service_register_endpoint() failure");
		return ret;
	}

	/* Reset message and expected message value and len. */
	expected_message = 'A';
	expected_len = PACKET_SIZE_START;

	/* Reset bound sem. */
	ret = k_sem_init(&bound_sem, 0, 1);
	if (ret != 0) {
		ERR("k_sem_init() failure");
		return ret;
	}

	ret = ipc_service_register_endpoint(ipc0_instance, &ep, &ep_cfg);
	if (ret != 0) {
		INF("ipc_service_register_endpoint() failure");
		return ret;
	}

	INF("Run network core");
	nrf53_cpunet_enable(true);

	k_sem_take(&bound_sem, K_FOREVER);

	ret = send_for_time(&ep, SENDING_TIME_MS);
	if (ret < 0) {
		ERR("send_for_time() failure");
		return ret;
	}
#endif /* CONFIG_SOC_NRF5340_CPUAPP */

	INF("IPC-service HOST demo ended");

	return 0;
}
