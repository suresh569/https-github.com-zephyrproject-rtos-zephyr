/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
/*
 * This inclusion is needed for nRF51 for two reasons:
 * - for GPIO peripheral, MDK defines only the legacy NRF_GPIO symbol;
 *   the GPIO HAL included below maps this symbol to NRF_P0, for consistency
 *   with all other nRF SoCs
 * - currently for nRF51, <soc.h> is not included through <kernel.h>,
 *   consequently <nrfx.h> is not included as well and MDK definitions
 *   are missing; the below inclusion fills this gap
 */
#include <hal/nrf_gpio.h>
/*
 * Provide translation of symbols for peripherals that for some SoCs got names
 * without the index.
 */
#ifndef NRF_I2S0
#define NRF_I2S0 NRF_I2S
#endif
#ifndef NRF_PDM0
#define NRF_PDM0 NRF_PDM
#endif
#ifndef NRF_WDT0
#define NRF_WDT0 NRF_WDT
#endif

#define CHECK_ADDRESS(dts, mdk)  BUILD_ASSERT((u32_t)(dts) == (u32_t)(mdk))

#if defined(DT_NORDIC_NRF_ADC_ADC_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_ADC_ADC_0_BASE_ADDRESS, NRF_ADC);
#endif

#if defined(DT_NORDIC_NRF_CC310_CC310_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_CC310_CC310_BASE_ADDRESS, NRF_CRYPTOCELL);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_clock), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_clock)), NRF_CLOCK);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_dppic), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_dppic)), NRF_DPPIC);
#endif

#if defined(DT_NORDIC_NRF_EGU_EGU_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_EGU_EGU_0_BASE_ADDRESS, NRF_EGU0);
#endif
#if defined(DT_NORDIC_NRF_EGU_EGU_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_EGU_EGU_1_BASE_ADDRESS, NRF_EGU1);
#endif
#if defined(DT_NORDIC_NRF_EGU_EGU_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_EGU_EGU_2_BASE_ADDRESS, NRF_EGU2);
#endif
#if defined(DT_NORDIC_NRF_EGU_EGU_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_EGU_EGU_3_BASE_ADDRESS, NRF_EGU3);
#endif
#if defined(DT_NORDIC_NRF_EGU_EGU_4_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_EGU_EGU_4_BASE_ADDRESS, NRF_EGU4);
#endif
#if defined(DT_NORDIC_NRF_EGU_EGU_5_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_EGU_EGU_5_BASE_ADDRESS, NRF_EGU5);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_ficr), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_ficr)), NRF_FICR);
#endif

#if defined(DT_NORDIC_NRF_GPIO_GPIO_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_GPIO_GPIO_0_BASE_ADDRESS, NRF_P0);
#endif
#if defined(DT_NORDIC_NRF_GPIO_GPIO_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_GPIO_GPIO_1_BASE_ADDRESS, NRF_P1);
#endif

#if defined(DT_NORDIC_NRF_GPIOTE_GPIOTE_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_GPIOTE_GPIOTE_0_BASE_ADDRESS, NRF_GPIOTE);
#endif

#if defined(DT_NORDIC_NRF_I2S_I2S_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_I2S_I2S_0_BASE_ADDRESS, NRF_I2S0);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_ipc), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_ipc)), NRF_IPC);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_kmu), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_kmu)), NRF_KMU);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf51_flash_controller), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf51_flash_controller)), NRF_NVMC);
#endif
#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf52_flash_controller), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf52_flash_controller)), NRF_NVMC);
#endif
#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf91_flash_controller), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf91_flash_controller)), NRF_NVMC);
#endif

#if defined(DT_NORDIC_NRF_PDM_PDM_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_PDM_PDM_0_BASE_ADDRESS, NRF_PDM0);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_power), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_power)), NRF_POWER);
#endif

#if defined(DT_NORDIC_NRF_PWM_PWM_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_PWM_PWM_0_BASE_ADDRESS, NRF_PWM0);
#endif
#if defined(DT_NORDIC_NRF_PWM_PWM_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_PWM_PWM_1_BASE_ADDRESS, NRF_PWM1);
#endif
#if defined(DT_NORDIC_NRF_PWM_PWM_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_PWM_PWM_2_BASE_ADDRESS, NRF_PWM2);
#endif
#if defined(DT_NORDIC_NRF_PWM_PWM_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_PWM_PWM_3_BASE_ADDRESS, NRF_PWM3);
#endif

#if defined(DT_NORDIC_NRF_QDEC_QDEC_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_QDEC_QDEC_0_BASE_ADDRESS, NRF_QDEC);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_regulators), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_regulators)), NRF_REGULATORS);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_rng), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_rng)), NRF_RNG);
#endif

#if defined(DT_NORDIC_NRF_RTC_RTC_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_RTC_RTC_0_BASE_ADDRESS, NRF_RTC0);
#endif
#if defined(DT_NORDIC_NRF_RTC_RTC_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_RTC_RTC_1_BASE_ADDRESS, NRF_RTC1);
#endif
#if defined(DT_NORDIC_NRF_RTC_RTC_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_RTC_RTC_2_BASE_ADDRESS, NRF_RTC2);
#endif

#if defined(DT_NORDIC_NRF_SAADC_ADC_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SAADC_ADC_0_BASE_ADDRESS, NRF_SAADC);
#endif

#if defined(DT_NORDIC_NRF_SPI_SPI_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPI_SPI_0_BASE_ADDRESS, NRF_SPI0);
#endif
#if defined(DT_NORDIC_NRF_SPI_SPI_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPI_SPI_1_BASE_ADDRESS, NRF_SPI1);
#endif
#if defined(DT_NORDIC_NRF_SPI_SPI_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPI_SPI_2_BASE_ADDRESS, NRF_SPI2);
#endif

#if defined(DT_NORDIC_NRF_SPIM_SPI_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIM_SPI_0_BASE_ADDRESS, NRF_SPIM0);
#endif
#if defined(DT_NORDIC_NRF_SPIM_SPI_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIM_SPI_1_BASE_ADDRESS, NRF_SPIM1);
#endif
#if defined(DT_NORDIC_NRF_SPIM_SPI_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIM_SPI_2_BASE_ADDRESS, NRF_SPIM2);
#endif
#if defined(DT_NORDIC_NRF_SPIM_SPI_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIM_SPI_3_BASE_ADDRESS, NRF_SPIM3);
#endif
#if defined(DT_NORDIC_NRF_SPIM_SPI_4_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIM_SPI_4_BASE_ADDRESS, NRF_SPIM4);
#endif

#if defined(DT_NORDIC_NRF_SPIS_SPI_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIS_SPI_0_BASE_ADDRESS, NRF_SPIS0);
#endif
#if defined(DT_NORDIC_NRF_SPIS_SPI_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIS_SPI_1_BASE_ADDRESS, NRF_SPIS1);
#endif
#if defined(DT_NORDIC_NRF_SPIS_SPI_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIS_SPI_2_BASE_ADDRESS, NRF_SPIS2);
#endif
#if defined(DT_NORDIC_NRF_SPIS_SPI_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_SPIS_SPI_3_BASE_ADDRESS, NRF_SPIS3);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_spu), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_spu)), NRF_SPU);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_temp), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_temp)), NRF_TEMP);
#endif

#if defined(DT_NORDIC_NRF_TIMER_TIMER_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TIMER_TIMER_0_BASE_ADDRESS, NRF_TIMER0);
#endif
#if defined(DT_NORDIC_NRF_TIMER_TIMER_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TIMER_TIMER_1_BASE_ADDRESS, NRF_TIMER1);
#endif
#if defined(DT_NORDIC_NRF_TIMER_TIMER_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TIMER_TIMER_2_BASE_ADDRESS, NRF_TIMER2);
#endif
#if defined(DT_NORDIC_NRF_TIMER_TIMER_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TIMER_TIMER_3_BASE_ADDRESS, NRF_TIMER3);
#endif
#if defined(DT_NORDIC_NRF_TIMER_TIMER_4_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TIMER_TIMER_4_BASE_ADDRESS, NRF_TIMER4);
#endif

#if defined(DT_NORDIC_NRF_TWI_I2C_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWI_I2C_0_BASE_ADDRESS, NRF_TWI0);
#endif
#if defined(DT_NORDIC_NRF_TWI_I2C_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWI_I2C_1_BASE_ADDRESS, NRF_TWI1);
#endif

#if defined(DT_NORDIC_NRF_TWIM_I2C_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIM_I2C_0_BASE_ADDRESS, NRF_TWIM0);
#endif
#if defined(DT_NORDIC_NRF_TWIM_I2C_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIM_I2C_1_BASE_ADDRESS, NRF_TWIM1);
#endif
#if defined(DT_NORDIC_NRF_TWIM_I2C_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIM_I2C_2_BASE_ADDRESS, NRF_TWIM2);
#endif
#if defined(DT_NORDIC_NRF_TWIM_I2C_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIM_I2C_3_BASE_ADDRESS, NRF_TWIM3);
#endif

#if defined(DT_NORDIC_NRF_TWIS_I2C_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIS_I2C_0_BASE_ADDRESS, NRF_TWIS0);
#endif
#if defined(DT_NORDIC_NRF_TWIS_I2C_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIS_I2C_1_BASE_ADDRESS, NRF_TWIS1);
#endif
#if defined(DT_NORDIC_NRF_TWIS_I2C_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIS_I2C_2_BASE_ADDRESS, NRF_TWIS2);
#endif
#if defined(DT_NORDIC_NRF_TWIS_I2C_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_TWIS_I2C_3_BASE_ADDRESS, NRF_TWIS3);
#endif

#if defined(DT_NORDIC_NRF_UART_UART_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_UART_UART_0_BASE_ADDRESS, NRF_UART0);
#endif

#if defined(DT_NORDIC_NRF_UARTE_UART_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_UARTE_UART_0_BASE_ADDRESS, NRF_UARTE0);
#endif
#if defined(DT_NORDIC_NRF_UARTE_UART_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_UARTE_UART_1_BASE_ADDRESS, NRF_UARTE1);
#endif
#if defined(DT_NORDIC_NRF_UARTE_UART_2_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_UARTE_UART_2_BASE_ADDRESS, NRF_UARTE2);
#endif
#if defined(DT_NORDIC_NRF_UARTE_UART_3_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_UARTE_UART_3_BASE_ADDRESS, NRF_UARTE3);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_uicr), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_uicr)), NRF_UICR);
#endif

#if defined(DT_NORDIC_NRF_USBD_USBD_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_USBD_USBD_0_BASE_ADDRESS, NRF_USBD);
#endif

#if DT_NODE_HAS_PROP(DT_INST(0, nordic_nrf_vmc), reg)
CHECK_ADDRESS(DT_REG_ADDR(DT_INST(0, nordic_nrf_vmc)), NRF_VMC);
#endif

#if defined(DT_NORDIC_NRF_WATCHDOG_WDT_0_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_WATCHDOG_WDT_0_BASE_ADDRESS, NRF_WDT0);
#endif

#if defined(DT_NORDIC_NRF_WATCHDOG_WDT_1_BASE_ADDRESS)
CHECK_ADDRESS(DT_NORDIC_NRF_WATCHDOG_WDT_1_BASE_ADDRESS, NRF_WDT1);
#endif
