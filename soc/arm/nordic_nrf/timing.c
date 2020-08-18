/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm/aarch32/arch.h>
#include <kernel.h>
#include <sys_clock.h>
#include <timing/timing.h>
#include <nrfx.h>

#if defined(CONFIG_NRF_RTC_TIMER)


#define CYCLES_PER_SEC (16000000 / (1 << NRF_TIMER2->PRESCALER))

void timing_init(void)
{
	NRF_TIMER2->TASKS_CLEAR = 1; /* Clear Timer */
	NRF_TIMER2->MODE = 0; /* Timer Mode */
	NRF_TIMER2->PRESCALER = 0; /* 16M Hz */
	NRF_TIMER2->BITMODE = 3; /* 32 - bit */
}

void timing_start(void)
{
	NRF_TIMER2->TASKS_START = 1;
}

void timing_stop(void)
{
	NRF_TIMER2->TASKS_STOP = 1; /* Stop Timer */
}

timing_t timing_counter_get(void)
{
	NRF_TIMER2->TASKS_CAPTURE[0] = 1;
	return NRF_TIMER2->CC[0] * ((SystemCoreClock) / CYCLES_PER_SEC);
}

uint64_t timing_cycles_get(volatile timing_t *const start,
			   volatile timing_t *const end)
{
	return (*end - *start);
}

uint64_t timing_freq_get(void)
{
	return SystemCoreClock;
}

uint64_t timing_cycles_to_ns(uint64_t cycles)
{
	return (cycles) * (NSEC_PER_SEC) / (SystemCoreClock);
}

uint64_t timing_cycles_to_ns_avg(uint64_t cycles, uint32_t count)
{
	return timing_cycles_to_ns(cycles) / count;
}

uint32_t timing_freq_get_mhz(void)
{
	return (uint32_t)(timing_freq_get() / 1000000);
}

#endif
