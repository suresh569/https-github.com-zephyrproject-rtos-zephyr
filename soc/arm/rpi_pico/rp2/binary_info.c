/*
 * Copyright (c) 2023 TOKITA Hiroshi <tokita.hiroshi@fujitsu.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <soc.h>

#include <pico/binary_info.h>
#include <boot_stage2/config.h>

#include <version.h>

#ifdef CONFIG_RP2_BINARY_INFO_RP_PROGRAM_NAME_OVERRIDE
#define BI_PROGRAM_NAME CONFIG_RP2_BINARY_INFO_RP_PROGRAM_NAME
#else
#define BI_PROGRAM_NAME APPLICATION_SOURCE_DIR_BASE
#endif

#ifdef CONFIG_RP2_BINARY_INFO_RP_PICO_BOARD_OVERRIDE
#define BI_PICO_BOARD CONFIG_RP2_BINARY_INFO_RP_PICO_BOARD
#else
#define BI_PICO_BOARD CONFIG_BOARD
#endif

#ifdef CONFIG_RP2_BINARY_INFO_RP_SDK_VERSION_STRING_OVERRIDE
#define BI_SDK_VERSION_STRING CONFIG_RP2_BINARY_INFO_RP_SDK_VERSION_STRING
#else
#define BI_SDK_VERSION_STRING PICO_SDK_VERSION_STRING
#endif

#ifdef CONFIG_RP2_BINARY_INFO_RP_PROGRAM_VERSION_STRING_OVERRIDE
#define BI_PROGRAM_VERSION_STRING CONFIG_RP2_BINARY_INFO_RP_PROGRAM_VERSION_STRING
#else
#define BI_PROGRAM_VERSION_STRING STRINGIFY(BUILD_VERSION)
#endif

#ifdef CONFIG_RP2_BINARY_INFO_RP_PROGRAM_BUILD_DATE_OVERRIDE
#define BI_PROGRAM_BUILD_DATE CONFIG_RP2_BINARY_INFO_RP_PROGRAM_BUILD_DATE
#else
#define BI_PROGRAM_BUILD_DATE __DATE__
#endif

#ifdef CONFIG_RP2_BINARY_INFO_RP_PROGRAM_DESCRIPTION_OVERRIDE
#define BI_PROGRAM_DESCRIPTION CONFIG_RP2_BINARY_INFO_RP_PROGRAM_DESCRIPTION
#endif

#ifdef CONFIG_RP2_BINARY_INFO_RP_PROGRAM_URL_OVERRIDE
#define BI_PROGRAM_URL CONFIG_RP2_BINARY_INFO_RP_PROGRAM_URL
#endif

extern uint32_t __rom_region_end;
bi_decl(bi_binary_end((intptr_t)&__rom_region_end));

#ifdef BI_PROGRAM_NAME
bi_decl(bi_program_name((uint32_t)BI_PROGRAM_NAME));
#endif

#ifdef BI_PICO_BOARD
bi_decl(bi_string(BINARY_INFO_TAG_RASPBERRY_PI, BINARY_INFO_ID_RP_PICO_BOARD,
		  (uint32_t)BI_PICO_BOARD));
#endif

#ifdef BI_SDK_VERSION_STRING
bi_decl(bi_string(BINARY_INFO_TAG_RASPBERRY_PI, BINARY_INFO_ID_RP_SDK_VERSION,
		  (uint32_t)BI_SDK_VERSION_STRING));
#endif

#ifdef BI_PROGRAM_VERSION_STRING
bi_decl(bi_program_version_string((uint32_t)BI_PROGRAM_VERSION_STRING));
#endif

#ifdef BI_PROGRAM_DESCRIPTION
bi_decl(bi_program_description((uint32_t)BI_PROGRAM_DESCRIPTION));
#endif

#ifdef BI_PROGRAM_URL
bi_decl(bi_program_url((uint32_t)BI_PROGRAM_URL));
#endif

#ifdef BI_PROGRAM_BUILD_DATE
bi_decl(bi_program_build_date_string((uint32_t)BI_PROGRAM_BUILD_DATE));
#endif

#ifdef BI_BOOT_STAGE2_NAME
bi_decl(bi_string(BINARY_INFO_TAG_RASPBERRY_PI, BINARY_INFO_ID_RP_BOOT2_NAME,
		  (uint32_t)BI_BOOT_STAGE2_NAME));
#endif

#ifndef NDEBUG
bi_decl(bi_program_build_attribute((uint32_t) "Debug"));
#else
bi_decl(bi_program_build_attribute((uint32_t) "Release"));
#endif
