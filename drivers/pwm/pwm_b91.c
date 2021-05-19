/*
 * Copyright (c) 2021 Telink Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT telink_b91_pwm

#include "pwm.h"
#include "clock.h"
#include <drivers/pwm.h>


#define PWM_PCLK_SPEED      DT_INST_PROP(0, clock_frequency)
#define NUM_OF_CHANNELS     DT_INST_PROP(0, channels)


/* API implementation: init */
static int pwm_b91_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	uint8_t clk_32k_en = 0;

	/* Set PWM Peripheral clock */
	pwm_set_clk((unsigned char) (sys_clk.pclk * 1000 * 1000 / PWM_PCLK_SPEED - 1));

	/* Set PWM 32k Channel clock if enabled */
	clk_32k_en |= DT_INST_PROP(0, clk32k_ch0_enable) ? PWM_CLOCK_32K_CHN_PWM0 : 0;
	clk_32k_en |= DT_INST_PROP(0, clk32k_ch1_enable) ? PWM_CLOCK_32K_CHN_PWM1 : 0;
	clk_32k_en |= DT_INST_PROP(0, clk32k_ch2_enable) ? PWM_CLOCK_32K_CHN_PWM2 : 0;
	clk_32k_en |= DT_INST_PROP(0, clk32k_ch3_enable) ? PWM_CLOCK_32K_CHN_PWM3 : 0;
	clk_32k_en |= DT_INST_PROP(0, clk32k_ch4_enable) ? PWM_CLOCK_32K_CHN_PWM4 : 0;
	clk_32k_en |= DT_INST_PROP(0, clk32k_ch5_enable) ? PWM_CLOCK_32K_CHN_PWM5 : 0;
	pwm_32k_chn_en(clk_32k_en);

	return 0;
}

/* API implementation: pin_set */
static int pwm_b91_pin_set(const struct device *dev, uint32_t pwm,
			   uint32_t period_cycles, uint32_t pulse_cycles,
			   pwm_flags_t flags)
{
	ARG_UNUSED(dev);

	/* check pwm channel */
	if (pwm >= NUM_OF_CHANNELS) {
		return -EINVAL;
	}

	/* check size of pulse and period (2 bytes) */
	if ((period_cycles > 0xFFFFu) ||
	    (pulse_cycles  > 0xFFFFu)) {
		return -EINVAL;
	}

	/* set polarity */
	if (flags & PWM_POLARITY_INVERTED) {
		pwm_invert_en(pwm);
	} else {
		pwm_invert_dis(pwm);
	}

	/* set pulse and period */
	pwm_set_tcmp(pwm, pulse_cycles);
	pwm_set_tmax(pwm, period_cycles);

	/* start pwm */
	pwm_start(pwm);

	return 0;
}

/* API implementation: get_cycles_per_sec */
static int pwm_b91_get_cycles_per_sec(const struct device *dev, uint32_t pwm,
				      uint64_t *cycles)
{
	ARG_UNUSED(dev);

	/* check pwm channel */
	if (pwm >= NUM_OF_CHANNELS) {
		return -EINVAL;
	}

	if (
		((pwm == 0u) && DT_INST_PROP(0, clk32k_ch0_enable)) ||
		((pwm == 1u) && DT_INST_PROP(0, clk32k_ch1_enable)) ||
		((pwm == 2u) && DT_INST_PROP(0, clk32k_ch2_enable)) ||
		((pwm == 3u) && DT_INST_PROP(0, clk32k_ch3_enable)) ||
		((pwm == 4u) && DT_INST_PROP(0, clk32k_ch4_enable)) ||
		((pwm == 5u) && DT_INST_PROP(0, clk32k_ch5_enable))
		) {
		*cycles = 32000u;
	} else {
		*cycles = PWM_PCLK_SPEED;
	}

	return 0;
}

/* PWM driver APIs structure */
static const struct pwm_driver_api pwm_b91_driver_api = {
	.pin_set = pwm_b91_pin_set,
	.get_cycles_per_sec = pwm_b91_get_cycles_per_sec,
};

/* PWM driver registration */
DEVICE_DT_INST_DEFINE(0, pwm_b91_init,			  \
		      NULL,		  \
		      NULL, NULL,			  \
		      POST_KERNEL,			  \
		      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
		      &pwm_b91_driver_api);
