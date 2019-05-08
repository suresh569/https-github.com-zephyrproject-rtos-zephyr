#
# Copyright (c) 2019, NXP
#
# SPDX-License-Identifier: Apache-2.0
#


## DAP Link implementation in pyocd is underway,
## until then jlink can be used or copy image to storage

set_ifndef(OPENSDA_FW jlink)

if(OPENSDA_FW STREQUAL jlink)
  set_ifndef(BOARD_DEBUG_RUNNER jlink)
  set_ifndef(BOARD_FLASH_RUNNER jlink)
endif()

if(CONFIG_BOARD_LPCXPRESSO55S69_CPU0)
board_runner_args(jlink "--device=LPC55S69_core0")
endif()
if(CONFIG_BOARD_LPCXPRESSO55S69_CPU1)
board_runner_args(jlink "--device=LPC55S69_core1")
endif()

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
