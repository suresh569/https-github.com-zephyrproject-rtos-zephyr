/*
 * Copyright (c) 2024 Vogl Electronic GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

#include "spi_context.h"
#include <soc.h>

#define LITEX_CPU_FREQ DT_PROP(DT_NODELABEL(cpu0), clock_frequency)

static inline uint8_t get_dfs_value(struct spi_context *ctx)
{
	switch (SPI_WORD_SIZE_GET(ctx->config->operation)) {
	case 8:
		return 1;
	case 16:
		return 2;
	case 32:
		return 4;
	default:
		return 1;
	}
}
