/*
 * Copyright (c) 2017, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <soc.h>
#include <zephyr/dt-bindings/rdc/imx_rdc.h>
#include <zephyr/dt-bindings/clock/nxp_imx7d_ccm.h>
#include <zephyr/devicetree.h>
#include "wdog_imx.h"

/* Initialize clock. */
void SOC_ClockInit(void)
{
	/* OSC/PLL is already initialized by Cortex-A7 (u-boot) */

	/*
	 * Disable WDOG3
	 *	Note : The WDOG clock Root is shared by all the 4 WDOGs,
	 *	so Zephyr code should avoid closing it
	 */
	CCM_UpdateRoot(CCM, ccmRootWdog, ccmRootmuxWdogOsc24m, 0, 0);
	CCM_EnableRoot(CCM, ccmRootWdog);
	CCM_ControlGate(CCM, ccmCcgrGateWdog3, ccmClockNeededRun);

	RDC_SetPdapAccess(RDC, rdcPdapWdog3,
			RDC_DOMAIN_PERM(M4_DOMAIN_ID, RDC_DOMAIN_PERM_RW),
			false, false);

	WDOG_DisablePowerdown(WDOG3);

	CCM_ControlGate(CCM, ccmCcgrGateWdog3, ccmClockNotNeeded);

	/* We need system PLL Div2 to run M4 core */
	CCM_ControlGate(CCM, ccmPllGateSys, ccmClockNeededRun);
	CCM_ControlGate(CCM, ccmPllGateSysDiv2, ccmClockNeededRun);

	/* Enable clock gate for IP bridge and IO mux */
	CCM_ControlGate(CCM, ccmCcgrGateIpmux1, ccmClockNeededRun);
	CCM_ControlGate(CCM, ccmCcgrGateIpmux2, ccmClockNeededRun);
	CCM_ControlGate(CCM, ccmCcgrGateIpmux3, ccmClockNeededRun);
	CCM_ControlGate(CCM, ccmCcgrGateIomux, ccmClockNeededRun);
	CCM_ControlGate(CCM, ccmCcgrGateIomuxLpsr, ccmClockNeededRun);

	/* Enable clock gate for RDC */
	CCM_ControlGate(CCM, ccmCcgrGateRdc, ccmClockNeededRun);
}

void SOC_RdcInit(void)
{
	/* Move M4 core to specific RDC domain */
	RDC_SetDomainID(RDC, rdcMdaM4, M4_DOMAIN_ID, false);
}

#ifdef CONFIG_GPIO_IMX
static void nxp_mcimx7_gpio_config(void)
{

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio1), okay)
	RDC_SetPdapAccess(RDC, rdcPdapGpio1, RDC_DT_VAL(gpio1), false, false);
	/* Enable gpio clock gate */
	CCM_ControlGate(CCM, ccmCcgrGateGpio1, ccmClockNeededRunWait);
#endif


#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio2), okay)
	RDC_SetPdapAccess(RDC, rdcPdapGpio2, RDC_DT_VAL(gpio2), false, false);
	/* Enable gpio clock gate */
	CCM_ControlGate(CCM, ccmCcgrGateGpio2, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio3), okay)
	RDC_SetPdapAccess(RDC, rdcPdapGpio3, RDC_DT_VAL(gpio3), false, false);
	/* Enable gpio clock gate */
	CCM_ControlGate(CCM, ccmCcgrGateGpio3, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio4), okay)
	RDC_SetPdapAccess(RDC, rdcPdapGpio4, RDC_DT_VAL(gpio4), false, false);
	/* Enable gpio clock gate */
	CCM_ControlGate(CCM, ccmCcgrGateGpio4, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio7), okay)
	RDC_SetPdapAccess(RDC, rdcPdapGpio7, RDC_DT_VAL(gpio7), false, false);
	/* Enable gpio clock gate */
	CCM_ControlGate(CCM, ccmCcgrGateGpio7, ccmClockNeededRunWait);
#endif

}
#endif /* CONFIG_GPIO_IMX */

#ifdef CONFIG_UART_IMX
static void nxp_mcimx7_uart_config(void)
{

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart2), okay)
	/* We need to grasp board uart exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapUart2, RDC_DT_VAL(uart2), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootUart2, ccmRootmuxUartOsc24m, 0, 0);
	/* Enable uart clock */
	CCM_EnableRoot(CCM, ccmRootUart2);
	/*
	 * IC Limitation
	 * M4 stop will cause A7 UART lose functionality
	 * So we need UART clock all the time
	 */
	CCM_ControlGate(CCM, ccmCcgrGateUart2, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart3), okay)
	/* We need to grasp board uart exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapUart3, RDC_DT_VAL(uart3), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootUart3, ccmRootmuxUartOsc24m, 0, 0);
	/* Enable uart clock */
	CCM_EnableRoot(CCM, ccmRootUart3);
	/*
	 * IC Limitation
	 * M4 stop will cause A7 UART lose functionality
	 * So we need UART clock all the time
	 */
	CCM_ControlGate(CCM, ccmCcgrGateUart3, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart4), okay)
	/* We need to grasp board uart exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapUart4, RDC_DT_VAL(uart4), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootUart4, ccmRootmuxUartOsc24m, 0, 0);
	/* Enable uart clock */
	CCM_EnableRoot(CCM, ccmRootUart4);
	/*
	 * IC Limitation
	 * M4 stop will cause A7 UART lose functionality
	 * So we need UART clock all the time
	 */
	CCM_ControlGate(CCM, ccmCcgrGateUart4, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart5), okay)
	/* We need to grasp board uart exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapUart5, RDC_DT_VAL(uart5), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootUart5, ccmRootmuxUartOsc24m, 0, 0);
	/* Enable uart clock */
	CCM_EnableRoot(CCM, ccmRootUart5);
	/*
	 * IC Limitation
	 * M4 stop will cause A7 UART lose functionality
	 * So we need UART clock all the time
	 */
	CCM_ControlGate(CCM, ccmCcgrGateUart5, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart6), okay)
	/* We need to grasp board uart exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapUart6, RDC_DT_VAL(uart6), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootUart6, ccmRootmuxUartOsc24m, 0, 0);
	/* Enable uart clock */
	CCM_EnableRoot(CCM, ccmRootUart6);
	/*
	 * IC Limitation
	 * M4 stop will cause A7 UART lose functionality
	 * So we need UART clock all the time
	 */
	CCM_ControlGate(CCM, ccmCcgrGateUart6, ccmClockNeededAll);
#endif
}
#endif /* CONFIG_UART_IMX */


#ifdef CONFIG_I2C_IMX
static void nxp_mcimx7_i2c_config(void)
{

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c1), okay)
	/* In this example, we need to grasp board I2C exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapI2c1, RDC_DT_VAL(i2c1), false, false);
	/* Select I2C clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootI2c1, ccmRootmuxI2cOsc24m, 0, 0);
	/* Enable I2C clock */
	CCM_EnableRoot(CCM, ccmRootI2c1);
	CCM_ControlGate(CCM, ccmCcgrGateI2c1, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c2), okay)
	/* In this example, we need to grasp board I2C exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapI2c2, RDC_DT_VAL(i2c2), false, false);
	/* Select I2C clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootI2c2, ccmRootmuxI2cOsc24m, 0, 0);
	/* Enable I2C clock */
	CCM_EnableRoot(CCM, ccmRootI2c2);
	CCM_ControlGate(CCM, ccmCcgrGateI2c2, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c3), okay)
	/* In this example, we need to grasp board I2C exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapI2c3, RDC_DT_VAL(i2c3), false, false);
	/* Select I2C clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootI2c3, ccmRootmuxI2cOsc24m, 0, 0);
	/* Enable I2C clock */
	CCM_EnableRoot(CCM, ccmRootI2c3);
	CCM_ControlGate(CCM, ccmCcgrGateI2c3, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c4), okay)
	/* In this example, we need to grasp board I2C exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapI2c4, RDC_DT_VAL(i2c4), false, false);
	/* Select I2C clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootI2c4, ccmRootmuxI2cOsc24m, 0, 0);
	/* Enable I2C clock */
	CCM_EnableRoot(CCM, ccmRootI2c4);
	CCM_ControlGate(CCM, ccmCcgrGateI2c4, ccmClockNeededRunWait);
#endif

}
#endif /* CONFIG_I2C_IMX */

#ifdef CONFIG_PWM_IMX
static void nxp_mcimx7_pwm_config(void)
{

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm1), okay)
	/* We need to grasp board pwm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapPwm1, RDC_DT_VAL(pwm1), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootPwm1, ccmRootmuxPwmOsc24m, 0, 0);
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootPwm1);
	CCM_ControlGate(CCM, ccmCcgrGatePwm1, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm2), okay)
	/* We need to grasp board pwm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapPwm2, RDC_DT_VAL(pwm2), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootPwm2, ccmRootmuxPwmOsc24m, 0, 0);
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootPwm2);
	CCM_ControlGate(CCM, ccmCcgrGatePwm2, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm3), okay)
	/* We need to grasp board pwm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapPwm3, RDC_DT_VAL(pwm3), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootPwm3, ccmRootmuxPwmOsc24m, 0, 0);
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootPwm3);
	CCM_ControlGate(CCM, ccmCcgrGatePwm3, ccmClockNeededAll);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(pwm4), okay)
	/* We need to grasp board pwm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapPwm4, RDC_DT_VAL(pwm4), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_UpdateRoot(CCM, ccmRootPwm4, ccmRootmuxPwmOsc24m, 0, 0);
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootPwm4);
	CCM_ControlGate(CCM, ccmCcgrGatePwm4, ccmClockNeededAll);
#endif

}
#endif /* CONFIG_PWM_IMX */

#ifdef CONFIG_GPT_IMX
static void nxp_mcimx7_gpt_config(void)
{

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpt1), okay)
	/* We need to grasp board ftm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapGpt1, RDC_DT_VAL(gpt1), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_DisableRoot(CCM, ccmRootGpt1);
	CCM_UpdateRoot(CCM, ccmRootGpt1, ccmRootmuxGptOsc24m,
					CCM_DT_ROOT_PRE(gpt1), CCM_DT_ROOT_POST(gpt1));
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootGpt1);
	CCM_ControlGate(CCM, ccmCcgrGateGpt1, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpt2), okay)
	/* We need to grasp board ftm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapGpt2, RDC_DT_VAL(gpt2), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_DisableRoot(CCM, ccmRootGpt2);
	CCM_UpdateRoot(CCM, ccmRootGpt2, ccmRootmuxGptOsc24m,
					CCM_DT_ROOT_PRE(gpt2), CCM_DT_ROOT_POST(gpt2));
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootGpt2);
	CCM_ControlGate(CCM, ccmCcgrGateGpt2, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpt3), okay)
	/* We need to grasp board ftm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapGpt3, RDC_DT_VAL(gpt3), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_DisableRoot(CCM, ccmRootGpt3);
	CCM_UpdateRoot(CCM, ccmRootGpt3, ccmRootmuxGptOsc24m,
					CCM_DT_ROOT_PRE(gpt3), CCM_DT_ROOT_POST(gpt3));
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootGpt3);
	CCM_ControlGate(CCM, ccmCcgrGateGpt3, ccmClockNeededRunWait);
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpt4), okay)
	/* We need to grasp board ftm exclusively */
	RDC_SetPdapAccess(RDC, rdcPdapGpt4, RDC_DT_VAL(gpt4), false, false);
	/* Select clock derived from OSC clock(24M) */
	CCM_DisableRoot(CCM, ccmRootGpt4);
	CCM_UpdateRoot(CCM, ccmRootGpt4, ccmRootmuxGptOsc24m,
					CCM_DT_ROOT_PRE(gpt4), CCM_DT_ROOT_POST(gpt4));
	/* Enable pwm clock */
	CCM_EnableRoot(CCM, ccmRootGpt4);
	CCM_ControlGate(CCM, ccmCcgrGateGpt4, ccmClockNeededRunWait);
#endif
}
#endif /* CONFIG_GPT_IMX */

#ifdef CONFIG_IPM_IMX
static void nxp_mcimx7_mu_config(void)
{
	/* Set access to MU B for M4 core */
	RDC_SetPdapAccess(RDC, rdcPdapMuB, RDC_DT_VAL(mub), false, false);

	/* Enable clock gate for MU*/
	CCM_ControlGate(CCM, ccmCcgrGateMu, ccmClockNeededRun);
}
#endif /* CONFIG_IPM_IMX */

static int nxp_mcimx7_init(void)
{

	/* SoC specific RDC settings */
	SOC_RdcInit();

	/* BoC specific clock settings */
	SOC_ClockInit();

#ifdef CONFIG_GPIO_IMX
	nxp_mcimx7_gpio_config();
#endif /* CONFIG_GPIO_IMX */

#ifdef CONFIG_UART_IMX
	nxp_mcimx7_uart_config();
#endif /* CONFIG_UART_IMX */

#ifdef CONFIG_I2C_IMX
	nxp_mcimx7_i2c_config();
#endif /* CONFIG_I2C_IMX */

#ifdef CONFIG_PWM_IMX
	nxp_mcimx7_pwm_config();
#endif /* CONFIG_PWM_IMX */

#ifdef CONFIG_GPT_IMX
	nxp_mcimx7_gpt_config();
#endif /* CONFIG_GPT_IMX */

#ifdef CONFIG_IPM_IMX
	nxp_mcimx7_mu_config();
#endif /* CONFIG_IPM_IMX */

	return 0;
}

SYS_INIT(nxp_mcimx7_init, PRE_KERNEL_1, 0);
