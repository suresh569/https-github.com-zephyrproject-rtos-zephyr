zephyr_include_directories(common)

zephyr_library_sources_ifdef(CONFIG_SOC_OPTION_SETTING_MEMORY
  opt_set_mem.c
)

zephyr_linker_sources_ifdef(CONFIG_SOC_OPTION_SETTING_MEMORY
  ROM_START
  ${CMAKE_CURRENT_SOURCE_DIR}/opt_set_mem.ld
)

set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/arm/cortex_m/scripts/linker.ld CACHE INTERNAL "")
