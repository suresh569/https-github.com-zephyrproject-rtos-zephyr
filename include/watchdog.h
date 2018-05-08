/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for watchdog drivers.
 */

#ifndef _WDT_H_
#define _WDT_H_

/**
 * @brief Watchdog Interface
 * @defgroup watchdog_interface Watchdog Interface
 * @ingroup timer_counter_interfaces
 * @{
 */

#include <zephyr/types.h>
#include <misc/util.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name Deprecated Watchdog API types.
 * @{
 */
#define WDT_MODE		(BIT(1))
#define WDT_MODE_OFFSET		(1)
#define WDT_TIMEOUT_MASK	(0xF)

enum wdt_mode {
	WDT_MODE_RESET = 0,
	WDT_MODE_INTERRUPT_RESET
};

/**
 * WDT clock cycles for timeout type.
 */
enum wdt_clock_timeout_cycles {
	WDT_2_16_CYCLES,
	WDT_2_17_CYCLES,
	WDT_2_18_CYCLES,
	WDT_2_19_CYCLES,
	WDT_2_20_CYCLES,
	WDT_2_21_CYCLES,
	WDT_2_22_CYCLES,
	WDT_2_23_CYCLES,
	WDT_2_24_CYCLES,
	WDT_2_25_CYCLES,
	WDT_2_26_CYCLES,
	WDT_2_27_CYCLES,
	WDT_2_28_CYCLES,
	WDT_2_29_CYCLES,
	WDT_2_30_CYCLES,
	WDT_2_31_CYCLES
};

/**
 * WDT configuration struct.
 */
struct wdt_config {
	u32_t timeout;
	enum wdt_mode mode;
	void (*interrupt_fn)(struct device *dev);
};
/**
 * @}
 */


/**
 * @brief Pause watchdog timer when CPU is in sleep state.
 */
#define WDT_OPT_PAUSE_IN_SLEEP	BIT(0)

/**
 * @brief Pause watchdog timer when CPU is halted by the debugger.
 */
#define WDT_OPT_PAUSE_HALTED_BY_DBG	BIT(1)

/**
 * @brief Watchdog reset flag bit field mask shift.
 */
#define WDT_FLAG_RESET_SHIFT		(0)
/**
 * @brief Watchdog reset flag bit field mask.
 */
#define WDT_FLAG_RESET_MASK		(0x3 << WDT_FLAG_RESET_SHIFT)

/**
 * @name Watchdog Reset Behavior.
 * Reset behavior after timeout.
 * @{
 */
/** No reset */
#define WDT_FLAG_RESET_NONE		(0 << WDT_FLAG_RESET_SHIFT)
/** CPU core reset */
#define WDT_FLAG_RESET_CPU_CORE		(1 << WDT_FLAG_RESET_SHIFT)
/** Global SoC reset */
#define WDT_FLAG_RESET_SOC		(2 << WDT_FLAG_RESET_SHIFT)
/**
 * @}
 */

/**
 * @brief Watchdog timeout window.
 *
 * Each installed timeout needs feeding within the specified time window,
 * otherwise the watchdog will trigger. If the watchdog instance does not
 * support window timeouts then min value must be equal to 0.
 *
 * @param min Lower limit of watchdog feed timeout in milliseconds.
 * @param max Upper limit of watchdog feed timeout in milliseconds.
 *
 * @note If specified values can not be precisely set they are always
 *	 rounded up.
 */
struct wdt_window {
	u32_t min;
	u32_t max;
};

/** Watchdog callback. */
typedef void (*wdt_callback_t)(struct device *dev, void *handle);

/**
 * @brief Watchdog timeout configuration struct.
 *
 * @param window Timing parameters of watchdog timeout.
 * @param handle Unique handle used to feed specific watchdog timeout.
 *		 The same handle may be used for multistage timeouts.
 *		 This handle is filled by the user, and will be supplied to
 *		 callback. A good example is a handle pointing to the id of the
 *		 thread that is responsible for feeding the corresponding
 *		 timeout.
 * @param callback Timeout callback. Passing NULL means that no callback
 *		   will be run.
 * @param next Pointer to the next timeout configuration. This pointer is used
 *	       for watchdogs with staged timeouts functionality. Value must be
 *	       NULL for single stage timeout.
 * @param flags Bit field with following parts:
 *
 *     reset        [ 0 : 1 ]   - perform specified reset after timeout/callback
 */
struct wdt_timeout_cfg {
	struct wdt_window window;
	void *handle;
	wdt_callback_t callback;
#ifdef CONFIG_WDT_MULTISTAGE
	struct wdt_timeout_cfg *next;
#endif
	u8_t flags;
};

/** @cond INTERNAL_HIDDEN */
struct wdt_driver_api {
	int (*setup)(struct device *dev, u8_t options);
	int (*disable)(struct device *dev);
	int (*install_timeout)(struct device *dev, struct wdt_timeout_cfg *cfg);
	int (*feed)(struct device *dev, void *handle);
	/** Deprecated functions */
	void (*enable)(struct device *dev);
	void (*get_config)(struct device *dev, struct wdt_config *config);
	int (*set_config)(struct device *dev, struct wdt_config *config);
	void (*reload)(struct device *dev);
};
/**
 * @endcond
 */

/**
 * @brief Set up watchdog instance.
 *
 * This function is used for configuring global watchdog settings that
 * affect all timeouts. It should be called after installing timeouts.
 * After successful return, all installed timeouts are valid and must be
 * serviced periodically by callig wdt_feed().
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param options Configuration options as defined by the WDT_OPT_* constants
 *
 * @retval 0 If successful.
 * @retval -ENOTSUP If any of the set options is not supported.
 * @retval -EBUSY If watchdog instance has been already setup.
 */
static inline int wdt_setup(struct device *dev, u8_t options)
{
	const struct wdt_driver_api *api =
		(const struct wdt_driver_api *)dev->driver_api;

	return api->setup(dev, options);
}

/**
 * @brief Disable watchdog instance.
 *
 * This function disables the watchdog instance and automatically uninstalls all
 * timeouts. To set up a new watchdog, install timeouts and call wdt_setup()
 * again. Not all watchdogs can be restarted after they are disabled.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @retval 0 If successful.
 * @retval -EFAULT If watchdog instance is not enabled.
 * @retval -EPERM If watchdog can not be disabled directly by application code.
 */
static inline int wdt_disable(struct device *dev)
{
	const struct wdt_driver_api *api =
		(const struct wdt_driver_api *)dev->driver_api;

	return api->disable(dev);
}

/**
 * @brief Install new timeout.
 *
 * This function must be used before wdt_setup(). Changes applied here
 * have no effects until wdt_setup() is called.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param cfg Pointer to timeout configuration structure.
 *
 * @retval 0 If successful.
 * @retval -EBUSY If timeout can not be installed while watchdog has already
 *		  been setup.
 * @retval -ENOMEM If no more timeouts can be installed.
 * @retval -EFAULT If timeout with given handle has already been installed.
 *		   Multistage watchdogs are allowed to use the same handle
 *		   for different stages
 * @retval -ENOTSUP If any of the set flags is not supported.
 * @retval -EINVAL If any of the window timeout value is out of possible range.
 *		   This value is also returned if watchdog supports only one
 *		   timeout value for all timeouts and the supplied timeout
 *		   window differs from windows for alarms installed so far.
 */
static inline int wdt_install_timeout(struct device *dev,
				      struct wdt_timeout_cfg *cfg)
{
	const struct wdt_driver_api *api = dev->driver_api;

	return api->install_timeout(dev, cfg);
}

/**
 * @brief Feed specified watchdog timeout.
 *
 * @param dev Pointer to the device structure for the driver instance.
 * @param handle Handle of timeout that should be fed.
 *
 * @retval 0 If successful.
 * @retval -EINVAL If there is no installed timeout for supplied handle.
 */
static inline int wdt_feed(struct device *dev, void *handle)
{
	const struct wdt_driver_api *api = dev->driver_api;

	return api->feed(dev, handle);
}

static inline void __deprecated wdt_enable(struct device *dev)
{
	const struct wdt_driver_api *api = dev->driver_api;

	api->enable(dev);
}

static inline void __deprecated wdt_get_config(struct device *dev,
				  struct wdt_config *config)
{
	const struct wdt_driver_api *api =
		(const struct wdt_driver_api *)dev->driver_api;

	api->get_config(dev, config);
}

static inline int __deprecated wdt_set_config(struct device *dev,
				 struct wdt_config *config)
{
	const struct wdt_driver_api *api =
		(const struct wdt_driver_api *)dev->driver_api;

	return api->set_config(dev, config);
}

static inline void __deprecated wdt_reload(struct device *dev)
{
	const struct wdt_driver_api *api =
		(const struct wdt_driver_api *)dev->driver_api;

	api->reload(dev);
}
#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __WDT_H__ */
