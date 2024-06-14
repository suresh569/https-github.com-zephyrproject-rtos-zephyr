/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#define PACKET_SIZE_START	(40)
#define DATA_SIZE		(100)
#define SENDING_TIME_MS		(1000)

struct data_packet {
	unsigned long cnt;
	unsigned long size;
	unsigned char data[DATA_SIZE];
};

#if defined(CONFIG_MULTITHREADING)
#define INF(...) LOG_INF(__VA_ARGS__);
#define ERR(...) LOG_ERR(__VA_ARGS__);
#else
#define INF(...) printk(__VA_ARGS__); printk("\r\n");
#define ERR(...) printk(__VA_ARGS__); printk("\r\n");
#endif

#endif /* __COMMON_H__ */
