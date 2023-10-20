/*
 * Copyright (c) 2022 HPMicro
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef ZEPHYR_INCLUDE_DRIVERS_PINCTRL_PINCTRL_HPMICRO_COMMON_H_
#define ZEPHYR_INCLUDE_DRIVERS_PINCTRL_PINCTRL_HPMICRO_COMMON_H_

#include <zephyr/dt-bindings/pinctrl/hpmicro-pinctrl-common.h>

/**
 * @brief Location of configuration items in the code
 *
 */
#define HPMICRO_OPEN_DRAIN              1U
#define HPMICRO_OPEN_DRAIN_SHIFT        0U

#define HPMICRO_NO_PULL                 1U
#define HPMICRO_NO_PULL_SHIFT           1U

#define HPMICRO_PULL_DOWN               1U
#define HPMICRO_PULL_DOWN_SHIFT         2U

#define HPMICRO_PULL_UP                 1U
#define HPMICRO_PULL_UP_SHIFT           3U

#define HPMICRO_FORCE_INPUT             1U
#define HPMICRO_FORCE_INPUT_SHIFT       4U

#define HPMICRO_DRIVER_STRENGTH         7U
#define HPMICRO_DRIVER_STRENGTH_SHIFT   5U

#define HPMICRO_SCHMITT_ENABLE          1U
#define HPMICRO_SCHMITT_ENABLE_SHIFT    12U

#define HPMICRO_POWER                   1U
#define HPMICRO_POWER_SHIFT             13U

#define HPMICRO_PAD_NUM(_mux)						\
	(((_mux) >> HPMICRO_PIN_NUM_SHIFT) & HPMICRO_PIN_NUM_MASK)

#define HPMICRO_FUNC_ANALOG(_mux)					\
	(((_mux) >> HPMICRO_PIN_ANALOG_SHIFT) & HPMICRO_PIN_ANALOG_MASK)

#define HPMICRO_FUNC_ALT_SELECT(_mux)					\
	(((_mux) >> HPMICRO_PIN_ALT_SHIFT) & HPMICRO_PIN_ALT_MASK)

#define HPMICRO_FUNC_IOC_SELECT(_mux)					\
	(((_mux) >> HPMICRO_PIN_IOC_SHIFT) & HPMICRO_PIN_IOC_MASK)

#define HPMICRO_FUNC_LOOPBACK(_cfg)					\
	(((_cfg) >> HPMICRO_FORCE_INPUT_SHIFT) & HPMICRO_FORCE_INPUT)

#define HPMICRO_PAD_CTL_MS(_cfg)				\
	(((_cfg) >> HPMICRO_POWER_SHIFT) & HPMICRO_POWER)

#define HPMICRO_PAD_CTL_OD(_cfg)					\
	(((_cfg) >> HPMICRO_OPEN_DRAIN_SHIFT) & HPMICRO_OPEN_DRAIN)

#define HPMICRO_PAD_CTL_SMT(_cfg)					\
	(((_cfg) >> HPMICRO_SCHMITT_ENABLE_SHIFT) & HPMICRO_SCHMITT_ENABLE)

#define HPMICRO_PAD_CTL_PULL_UP(_cfg)				\
	(((_cfg) >> HPMICRO_PULL_UP_SHIFT) & HPMICRO_PULL_UP)

#define HPMICRO_PAD_CTL_PULL_DN(_cfg)					\
	(((_cfg) >> HPMICRO_PULL_DOWN_SHIFT) & HPMICRO_PULL_DOWN)

#define HPMICRO_PAD_CTL_PS(_cfg)		\
	HPMICRO_PAD_CTL_PULL_UP(_cfg)

#define HPMICRO_PAD_CTL_PE(_cfg)					\
	((!(((_cfg) >> HPMICRO_NO_PULL_SHIFT) & HPMICRO_NO_PULL)) &	\
	 (HPMICRO_PAD_CTL_PULL_UP(_cfg) ^ HPMICRO_PAD_CTL_PULL_DN(_cfg)))

#define HPMICRO_PAD_CTL_DS(_cfg)					\
	(((_cfg) >> HPMICRO_DRIVER_STRENGTH_SHIFT) & HPMICRO_DRIVER_STRENGTH)

#endif /* ZEPHYR_INCLUDE_DRIVERS_PINCTRL_PINCTRL_HPMICRO_COMMON_H_ */
