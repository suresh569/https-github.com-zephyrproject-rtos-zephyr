# SPDX-License-Identifier: Apache-2.0

function(${SYSBUILD_CURRENT_MODULE_NAME}_pre_cmake)
  set_property(TARGET ${DEFAULT_IMAGE} APPEND_STRING PROPERTY CONFIG "CONFIG_TEST_FOO_VAL=${SB_CONFIG_TEST_FOO_VAL}\n")
endfunction()

function(${SYSBUILD_CURRENT_MODULE_NAME}_post_cmake)
  ExternalProject_Get_Property(${DEFAULT_IMAGE} BINARY_DIR)
  import_kconfig(CONFIG_ ${BINARY_DIR}/zephyr/.config)

  if("${SB_CONFIG_TEST_FOO_VAL}" STREQUAL "${CONFIG_TEST_FOO_VAL}")
    message(NOTICE "Values match (sysbuild and app)")
  else()
    message(FATAL_ERROR "Values diverge (sysbuild and app)")
  endif()

  if("${TEST_VALUE}" STREQUAL "${SB_CONFIG_TEST_FOO_VAL}")
    message(NOTICE "Values match (sysbuild and snippet)")
  else()
    message(FATAL_ERROR "Values diverge (sysbuild and snippet)")
  endif()
endfunction()
