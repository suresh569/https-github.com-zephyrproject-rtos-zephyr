/*
 * Copyright (c) 2023-2024 Chen Xingyu <hi@xingrz.me>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SOC_H__
#define __SOC_H__

#include <zephyr/device.h>

#define CH32V_SYS_BASE DT_REG_ADDR(DT_NODELABEL(syscon))

#if defined(CONFIG_SOC_CH565) || defined(CONFIG_SOC_CH569)
#include "syscon_regs_ch56x.h"
#endif /* CONFIG_SOC_CH565 || CONFIG_SOC_CH569 */

void ch32v_sys_unlock(void);
void ch32v_sys_relock(void);

#endif /* __SOC_H__ */
