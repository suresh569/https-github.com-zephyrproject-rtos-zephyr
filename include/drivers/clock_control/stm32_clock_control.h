/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2016 BayLibre, SAS
 * Copyright (c) 2017 Linaro Limited.
 * Copyright (c) 2017 RnDity Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_STM32_CLOCK_CONTROL_H_
#define ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_STM32_CLOCK_CONTROL_H_

#include <drivers/clock_control.h>
#include <dt-bindings/clock/stm32_clock.h>

/* common clock control device name for all STM32 chips */
#define STM32_CLOCK_CONTROL_NAME "stm32-cc"

struct stm32_pclken {
	uint32_t bus;
	uint32_t enr;
};

/* function to make reconfiguration of clock after wake up from low power
 * mode possible, see issue #28572
 */
int stm32_clock_control_real_init(const struct device *dev);

#endif /* ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_STM32_CLOCK_CONTROL_H_ */
