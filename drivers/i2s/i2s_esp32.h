/*
 * Copyright (c) 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_types.h"
#include "esp_err.h"
#include "soc/i2s_periph.h"
#include "soc/rtc_periph.h"
#include "hal/i2s_hal.h"

struct queue_item {
	void *mem_block;
	size_t size;
};

/* Minimal ring buffer implementation */
struct ring_buf {
	struct queue_item *buf;
	uint16_t len;
	uint16_t head;
	uint16_t tail;
};

struct i2s_esp32_stream {
	int32_t state;
	struct k_sem sem;
	struct i2s_config i2s_cfg;
	bool master;

	const struct device *dma_dev;
	uint32_t dma_channel;

	struct ring_buf mem_block_queue;
	void *mem_block;
	bool last_block;
	int (*stream_start)(const struct device *dev);
	void (*stream_stop)(const struct device *dev);
	void (*queue_drop)(struct i2s_esp32_stream *stream);
};

/* Device constant configuration parameters */
struct i2s_esp32_cfg {
	const int i2s_num;
	const struct pinctrl_dev_config *pcfg;
	const struct device *clock_dev;
	clock_control_subsys_t clock_subsys;
	/*void (*irq_config)(const struct device *dev);*/
};

/* Device run time data */
struct i2s_esp32_data {
	i2s_hal_context_t hal_cxt;
	i2s_hal_clock_info_t clk_info;
	struct i2s_esp32_stream rx;
	struct i2s_esp32_stream tx;
};

#ifdef __cplusplus
}
#endif
