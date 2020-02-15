/*
 * Copyright (c) 2020 Philémon Jaermann
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_SHT21_SHT21_H_
#define ZEPHYR_DRIVERS_SENSOR_SHT21_SHT21_H_

#include <device.h>
#include <sys/util.h>
#include <zephyr/types.h>

#define SHT21_TEMPERATURE_MEAS_NO_HOLD	0xF3
#define SHT21_HUMIDITY_MEAS_NO_HOLD	0xF5
#define SHT21_WRITE_USER_REG		0xE6
#define SHT21_READ_USER_REG		0xE7
#define SHT21_SOFT_RESET		0xFE

#define SHT21_RH_RESOLUTION_BIT_POS	(7)
#define SHT21_ON_CHIP_HEATER_BIT_POS	(2)
#define SHT21_OTP_RELOAD_BIT_POS	(1)
#define SHT21_TEMP_RESOLUTION_BIT_POS	(0)

#define SHT21_STATUS_BIT_RH_MEAS	(BIT(1))

#if defined(CONFIG_SHT21_MEAS_RES_RH_8_BITS)
#define SHT21_RH_RESOLUTION		(0 << SHT21_RH_RESOLUTION_BIT_POS)
#define SHT21_TEMP_RESOLUTION		(1 << SHT21_TEMP_RESOLUTION_BIT_POS)
#if DT_INST_0_SENSIRION_SHT21_VARIANT_HTU21D
#define SHT21_MEAS_RH_WAIT_TIME		(3)
#define SHT21_MEAS_TEMP_WAIT_TIME	(13)
#elif DT_INST_0_SENSIRION_SHT21_VARIANT_SI7021
#define SHT21_MEAS_RH_WAIT_TIME		(4)
#define SHT21_MEAS_TEMP_WAIT_TIME	(4)
#else
#define SHT21_MEAS_RH_WAIT_TIME		(4)
#define SHT21_MEAS_TEMP_WAIT_TIME	(22)
#endif
#elif defined(CONFIG_SHT21_MEAS_RES_RH_10_BITS)
#define SHT21_RH_RESOLUTION		(1 << SHT21_RH_RESOLUTION_BIT_POS)
#define SHT21_TEMP_RESOLUTION		(0 << SHT21_TEMP_RESOLUTION_BIT_POS)
#if DT_INST_0_SENSIRION_SHT21_VARIANT_HTU21D
#define SHT21_MEAS_RH_WAIT_TIME		(5)
#define SHT21_MEAS_TEMP_WAIT_TIME	(25)
#elif DT_INST_0_SENSIRION_SHT21_VARIANT_SI7021
#define SHT21_MEAS_RH_WAIT_TIME		(5)
#define SHT21_MEAS_TEMP_WAIT_TIME	(7)
#else
#define SHT21_MEAS_RH_WAIT_TIME		(9)
#define SHT21_MEAS_TEMP_WAIT_TIME	(43)
#endif
#elif defined(CONFIG_SHT21_MEAS_RES_RH_11_BITS)
#define SHT21_RH_RESOLUTION		(1 << SHT21_RH_RESOLUTION_BIT_POS)
#define SHT21_TEMP_RESOLUTION		(1 << SHT21_TEMP_RESOLUTION_BIT_POS)
#if DT_INST_0_SENSIRION_SHT21_VARIANT_HTU21D
#define SHT21_MEAS_RH_WAIT_TIME		(8)
#define SHT21_MEAS_TEMP_WAIT_TIME	(7)
#elif DT_INST_0_SENSIRION_SHT21_VARIANT_SI7021
#define SHT21_MEAS_RH_WAIT_TIME		(7)
#define SHT21_MEAS_TEMP_WAIT_TIME	(3)
#else
#define SHT21_MEAS_RH_WAIT_TIME		(15)
#define SHT21_MEAS_TEMP_WAIT_TIME	(11)
#endif
#else
#define SHT21_RH_RESOLUTION		(0 << SHT21_RH_RESOLUTION_BIT_POS)
#define SHT21_TEMP_RESOLUTION		(0 << SHT21_TEMP_RESOLUTION_BIT_POS)
#if DT_INST_0_SENSIRION_SHT21_VARIANT_HTU21D
#define SHT21_MEAS_RH_WAIT_TIME		(16)
#define SHT21_MEAS_TEMP_WAIT_TIME	(50)
#elif DT_INST_0_SENSIRION_SHT21_VARIANT_SI7021
#define SHT21_MEAS_RH_WAIT_TIME		(12)
#define SHT21_MEAS_TEMP_WAIT_TIME	(11)
#else
#define SHT21_MEAS_RH_WAIT_TIME		(29)
#define SHT21_MEAS_TEMP_WAIT_TIME	(85)
#endif
#endif

#if defined(CONFIG_SHT21_ENABLE_HEATER)
#define SHT21_HEATER			(1 << SHT21_ON_CHIP_HEATER_BIT_POS)
#else
#define SHT21_HEATER			(0 << SHT21_ON_CHIP_HEATER_BIT_POS)
#endif

#if defined(CONFIG_SHT21_ENABLE_OTP_RELOAD)
#define SHT21_OTP_RELOAD		(0 << SHT21_OTP_RELOAD_BIT_POS)
#else
#define SHT21_OTP_RELOAD		(1 << SHT21_OTP_RELOAD_BIT_POS)
#endif

struct sht21_data {
	struct device *i2c;
	u16_t rh_sample;
	u16_t t_sample;
};

struct sht21_config {
	const char *i2c_label;
	u16_t i2c_addr;
};
#endif /* __SENSOR_SHT21__ */
