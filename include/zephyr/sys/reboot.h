/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Common target reboot functionality
 *
 * @details See subsys/os/Kconfig and the reboot help for details.
 */

#ifndef ZEPHYR_INCLUDE_SYS_REBOOT_H_
#define ZEPHYR_INCLUDE_SYS_REBOOT_H_

#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_REBOOT_WARM 0
#define SYS_REBOOT_COLD 1

/**
 * @brief Reboot the system
 *
 * Reboot the system in the manner specified by @a type.  Not all architectures
 * or platforms support the various reboot types (SYS_REBOOT_COLD,
 * SYS_REBOOT_WARM).
 *
 * When successful, this routine does not return.
 */
extern FUNC_NORETURN void sys_reboot(int type);

/**
 * @brief Obtain reboot type
 *
 * Fetch reboot type which caused system startup.
 * This call extract reboot type vale which was used in previous call to the
 * sys_rebot() call, if the platform allow to store the value in
 * the reset retention storage.
 * Once value is read, the reboot type in the system is cleared to
 * the default value (SYS_REBOOT_WARM).
 *
 * Otherwise it returns the default value which is SYS_REBOOT_WARM.
 *
 * @retval reboot type
 */
int sys_get_reboot_type(void);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_REBOOT_H_ */
