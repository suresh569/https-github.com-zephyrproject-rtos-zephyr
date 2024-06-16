/*
 * Copyright (c) 2024 Yann Gaillard
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_SDP3X_SDP3X_H_
#define ZEPHYR_DRIVERS_SENSOR_SDP3X_SDP3X_H_

#include <zephyr/device.h>

#define SDP3X_CMD_READ_SERIAL	0x89
#define SDP3X_CMD_RESET		0x0006

#define SDP3X_RESET_WAIT_MS	25

/*
 * CRC parameters were taken from the
 * "Checksum Calculation" section of the datasheet.
 */
#define SDP3X_CRC_POLY		0x31
#define SDP3X_CRC_INIT		0xFF

struct sdp3x_config {
	struct i2c_dt_spec bus;
	uint8_t mesure_mode;
};

struct sdp3x_data {
	uint16_t t_sample;
	uint16_t p_sample;
};

#ifdef CONFIG_SDP3X_PERIODIC_MODE
static const uint16_t measure_cmd[4] = {
	0x3603,0x3608,0x3615,0x361E
};
#endif

#ifdef CONFIG_SDP3X_SINGLE_SHOT_MODE
static const uint16_t measure_cmd[4] = {
	0x3624,0x3726,0x362F,0x372D
};
#endif

#endif /* ZEPHYR_DRIVERS_SENSOR_SDP3X_SDP3X_H_ */
