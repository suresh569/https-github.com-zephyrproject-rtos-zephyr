/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/net/socket.h>

/* Currently only one Websocket connection is possible */
static int ws_sock = -1;

static char line[CONFIG_WS_LINE_LEN];
static size_t pos;

/** @brief Wait for fixed period.
 *
 */
static void wait(void)
{
	k_msleep(CONFIG_WS_TX_RETRY_DELAY_MS);
}

static int ws_send_all(int sock, const char *output, size_t len)
{
	int ret;

	while (len > 0) {
		ret = zsock_send(sock, output, len, ZSOCK_MSG_DONTWAIT);
		if ((ret < 0) && (errno == EAGAIN)) {
			return -EAGAIN;
		}

		if (ret < 0) {
			ret = -errno;
			return ret;
		}

		output += ret;
		len -= ret;
	}

	return 0;
}

static int ws_console_out(int c)
{
	static int max_cnt = CONFIG_WS_TX_RETRY_CNT;
	bool printnow = false;
	unsigned int cnt = 0;
	int ret;

	if ((c != '\n') && (c != '\r')) {
		line[pos++] = c;
	} else {
		printnow = true;
	}

	if (pos >= (sizeof(line) - 1)) {
		printnow = true;
	}

	if (printnow) {
		while (ws_sock >= 0 && cnt < max_cnt) {
			ret = ws_send_all(ws_sock, line, pos);
			if (ret < 0) {
				if (ret == -EAGAIN) {
					wait();
					cnt++;
					continue;
				}
			}

			break;
		}

		if (ws_sock >= 0 && ret == 0) {
			/* We could send data */
			pos = 0;
		} else {
			/* If the line is full and we cannot send, then
			 * ignore the output data in buffer.
			 */
			if (pos >= (sizeof(line) - 1)) {
				pos = 0;
			}
		}
	}

	return c;
}

static int websocket_console_init(void)
{
#ifdef CONFIG_PRINTK
	extern void __printk_hook_install(int (*fn)(int));
	__printk_hook_install(ws_console_out);
#endif

#ifdef CONFIG_STDOUT_CONSOLE
	extern void __stdout_hook_install(int (*fn)(int));
	__stdout_hook_install(ws_console_out);
#endif

	return 0;
}

int websocket_console_register(int fd)
{
	ws_sock = fd;

	return 0;
}

int websocket_console_unregister(void)
{
	ws_sock = -1;

	return 0;
}

SYS_INIT(websocket_console_init, PRE_KERNEL_1, CONFIG_CONSOLE_INIT_PRIORITY);
