/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT plantower_pms7003

/* sensor pms7003.c - Driver for plantower PMS7003, PMS5003, PMS6003 sensor.
 * Moreover, This driver supports sensor of pmsx003 series.
 *
 * PMS7003 product: http://www.plantower.com/en/content/?110.html
 * PMS7003 spec: http://aqicn.org/air/view/sensor/spec/pms7003.pdf
 *
 * PMS5003 product: https://www.plantower.com/en/products_33/74.html
 * PMS5003 spec:
 * https://www.digikey.com/en/htmldatasheets/production/2903006/0/0/1/pms5003-series-manual
 *
 * PMS6003 product: https://www.plantower.com/en/products_33/75.html
 * PMS6003 spec: https://evelta.com/content/datasheets/203-PMS6003.pdf
 */

#include <errno.h>

#include <zephyr/arch/cpu.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(PMS7003, CONFIG_SENSOR_LOG_LEVEL);

/* wait serial output with 1000ms timeout */
#define CFG_PMS7003_SERIAL_TIMEOUT 1000

struct pms7003_config {
	const struct device *uart_dev;
};

struct pms7003_data {
	uint16_t pm_1_0_cf;
	uint16_t pm_2_5_cf;
	uint16_t pm_10_cf;
	uint16_t pm_1_0_atm;
	uint16_t pm_2_5_atm;
	uint16_t pm_10_0_atm;
	uint16_t pm_0_3_count;
	uint16_t pm_0_5_count;
	uint16_t pm_1_0_count;
	uint16_t pm_2_5_count;
	uint16_t pm_5_0_count;
	uint16_t pm_10_0_count;
};

/**
 * @brief wait for an array data from uart device with a timeout
 *
 * @param dev the uart device
 * @param data the data array to be matched
 * @param len the data array len
 * @param timeout the timeout in milliseconds
 * @return 0 if success; -ETIME if timeout
 */
static int uart_wait_for(const struct device *dev, uint8_t *data, int len, int timeout)
{
	int matched_size = 0;
	int64_t timeout_time = k_uptime_get() + timeout;

	while (1) {
		uint8_t c;

		if (k_uptime_get() > timeout_time) {
			return -ETIME;
		}

		if (uart_poll_in(dev, &c) == 0) {
			if (c == data[matched_size]) {
				matched_size++;

				if (matched_size == len) {
					break;
				}
			} else if (c == data[0]) {
				matched_size = 1;
			} else {
				matched_size = 0;
			}
		}
	}
	return 0;
}

/**
 * @brief read bytes from uart
 *
 * @param data the data buffer
 * @param len the data len
 * @param timeout the timeout in milliseconds
 * @return 0 if success; -ETIME if timeout
 */
static int uart_read_bytes(const struct device *dev, uint8_t *data, int len, int timeout)
{
	int read_size = 0;
	int64_t timeout_time = k_uptime_get() + timeout;

	while (1) {
		uint8_t c;

		if (k_uptime_get() > timeout_time) {
			return -ETIME;
		}

		if (uart_poll_in(dev, &c) == 0) {
			data[read_size++] = c;
			if (read_size == len) {
				break;
			}
		}
	}
	return 0;
}

static int pms7003_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct pms7003_data *drv_data = dev->data;
	const struct pms7003_config *cfg = dev->config;

	/* sample output */
	/* 42 4D 00 1C 00 01 00 01 00 01 00 01 00 01 00 01 01 92
	 * 00 4E 00 03 00 00 00 00 00 00 71 00 02 06
	 */

	uint8_t pms7003_start_bytes[] = {0x42, 0x4d};
	uint8_t pms7003_receive_buffer[30];

	if (uart_wait_for(cfg->uart_dev, pms7003_start_bytes, sizeof(pms7003_start_bytes),
			  CFG_PMS7003_SERIAL_TIMEOUT) < 0) {
		LOG_WRN("waiting for start bytes is timeout");
		return -ETIME;
	}

	if (uart_read_bytes(cfg->uart_dev, pms7003_receive_buffer, 30, CFG_PMS7003_SERIAL_TIMEOUT) <
	    0) {
		return -ETIME;
	}

	drv_data->pm_1_0_cf = (pms7003_receive_buffer[2] << 8) + pms7003_receive_buffer[3];
	drv_data->pm_2_5_cf = (pms7003_receive_buffer[4] << 8) + pms7003_receive_buffer[5];
	drv_data->pm_10_cf = (pms7003_receive_buffer[6] << 8) + pms7003_receive_buffer[7];
	drv_data->pm_1_0_atm = (pms7003_receive_buffer[8] << 8) + pms7003_receive_buffer[9];
	drv_data->pm_2_5_atm = (pms7003_receive_buffer[10] << 8) + pms7003_receive_buffer[11];
	drv_data->pm_10_0_atm = (pms7003_receive_buffer[12] << 8) + pms7003_receive_buffer[13];
	drv_data->pm_0_3_count = (pms7003_receive_buffer[14] << 8) + pms7003_receive_buffer[15];
	drv_data->pm_0_5_count = (pms7003_receive_buffer[16] << 8) + pms7003_receive_buffer[17];
	drv_data->pm_1_0_count = (pms7003_receive_buffer[18] << 8) + pms7003_receive_buffer[19];
	drv_data->pm_2_5_count = (pms7003_receive_buffer[20] << 8) + pms7003_receive_buffer[21];
	drv_data->pm_5_0_count = (pms7003_receive_buffer[22] << 8) + pms7003_receive_buffer[23];
	drv_data->pm_10_0_count = (pms7003_receive_buffer[24] << 8) + pms7003_receive_buffer[25];

	LOG_DBG("pm1.0_cf = %d", drv_data->pm_1_0_cf);
	LOG_DBG("pm2.5_cf = %d", drv_data->pm_2_5_cf);
	LOG_DBG("pm10_cf = %d", drv_data->pm_10_cf);
	LOG_DBG("pm1.0_atm = %d", drv_data->pm_1_0_atm);
	LOG_DBG("pm2.5_atm = %d", drv_data->pm_2_5_atm);
	LOG_DBG("pm10_atm = %d", drv_data->pm_10_0_atm);
	LOG_DBG("pm0.3_count = %d", drv_data->pm_0_3_count);
	LOG_DBG("pm0.5_count = %d", drv_data->pm_0_5_count);
	LOG_DBG("pm1.0_count = %d", drv_data->pm_1_0_count);
	LOG_DBG("pm2.5_count = %d", drv_data->pm_2_5_count);
	LOG_DBG("pm5.0_count = %d", drv_data->pm_5_0_count);
	LOG_DBG("pm10_count = %d", drv_data->pm_10_0_count);

	return 0;
}

static int pms7003_channel_get(const struct device *dev, enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct pms7003_data *drv_data = dev->data;

	if (chan == SENSOR_CHAN_PM_1_0_CF) {
		val->val1 = drv_data->pm_1_0_cf;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_2_5_CF) {
		val->val1 = drv_data->pm_2_5_cf;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_10_CF) {
		val->val1 = drv_data->pm_10_cf;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_1_0) {
		val->val1 = drv_data->pm_1_0_atm;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_2_5) {
		val->val1 = drv_data->pm_2_5_atm;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_10) {
		val->val1 = drv_data->pm_10_0_atm;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_0_3_COUNT) {
		val->val1 = drv_data->pm_0_3_count;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_0_5_COUNT) {
		val->val1 = drv_data->pm_0_5_count;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_1_0_COUNT) {
		val->val1 = drv_data->pm_1_0_count;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_2_5_COUNT) {
		val->val1 = drv_data->pm_2_5_count;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_5_0_COUNT) {
		val->val1 = drv_data->pm_5_0_count;
		val->val2 = 0;
	} else if (chan == SENSOR_CHAN_PM_10_0_COUNT) {
		val->val1 = drv_data->pm_10_0_count;
		val->val2 = 0;
	} else {
		return -ENOTSUP;
	}
	return 0;
}

static const struct sensor_driver_api pms7003_api = {
	.sample_fetch = &pms7003_sample_fetch,
	.channel_get = &pms7003_channel_get,
};

static int pms7003_init(const struct device *dev)
{
	const struct pms7003_config *cfg = dev->config;

	if (!device_is_ready(cfg->uart_dev)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	return 0;
}

#define PMS7003_DEFINE(inst)                                                                       \
	static struct pms7003_data pmsX003_data_##inst;                                            \
                                                                                                   \
	static const struct pms7003_config pmsX003_config_##inst = {                               \
		.uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),                                      \
	};                                                                                         \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, &pms7003_init, NULL, &pmsX003_data_##inst,              \
				     &pmsX003_config_##inst, POST_KERNEL,                          \
				     CONFIG_SENSOR_INIT_PRIORITY, &pms7003_api);

DT_INST_FOREACH_STATUS_OKAY(PMS7003_DEFINE)
