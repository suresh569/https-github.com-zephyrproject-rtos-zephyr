# Copyright (c) 2021 Yonatan Schachter
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(soc.c)
zephyr_library_sources(binary_info.c)
zephyr_library_sources(binary_info_header.S)

zephyr_linker_sources(ROM_START SORT_KEY 0x0binary_info_header binary_info_header.ld)
zephyr_linker_sources(RODATA binary_info.ld)
