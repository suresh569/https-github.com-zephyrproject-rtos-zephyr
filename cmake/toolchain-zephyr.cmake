if(NOT ZEPHYR_SDK_INSTALL_DIR)
  set(ZEPHYR_SDK_INSTALL_DIR $ENV{ZEPHYR_SDK_INSTALL_DIR})
endif()
set(ZEPHYR_SDK_INSTALL_DIR ${ZEPHYR_SDK_INSTALL_DIR} CACHE PATH "Zephyr SDK install directory")
if(NOT ZEPHYR_SDK_INSTALL_DIR)
  message(FATAL_ERROR "ZEPHYR_SDK_INSTALL_DIR is not set")
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ${ARCH})

#TODO: SDK version check
set(TOOLCHAIN_VENDOR zephyr)
set(TOOLCHAIN_ARCH x86_64)

if(MINGW)
  set(TOOLCHAIN_HOME ${ZEPHYR_SDK_INSTALL_DIR}/sysroots/i686-pokysdk-mingw32)
else()
  set(TOOLCHAIN_HOME ${ZEPHYR_SDK_INSTALL_DIR}/sysroots/${TOOLCHAIN_ARCH}-pokysdk-linux)
endif()

# TODO fetch from environment
if("${ARCH}" STREQUAL "arm")
  if    (CONFIG_CPU_CORTEX_M0)
    set(GCC_M_CPU cortex-m0)
  elseif(CONFIG_CPU_CORTEX_M0PLUS)
    set(GCC_M_CPU cortex-m0plus)
  elseif(CONFIG_CPU_CORTEX_M3)
    set(GCC_M_CPU cortex-m3)
  elseif(CONFIG_CPU_CORTEX_M4)
    set(GCC_M_CPU cortex-m4)
  elseif(CONFIG_CPU_CORTEX_M7)
    set(GCC_M_CPU cortex-m7)
  elseif(CONFIG_CPU_CORTEX_M23)
    set(GCC_M_CPU cortex-m23)
  elseif(CONFIG_CPU_CORTEX_M33)
    set(GCC_M_CPU cortex-m33)
  else()
    message(FATAL_ERROR "Expected CONFIG_CPU_CORTEX_x to be defined")
  endif()

  set(FPU_FOR_cortex-m4      fpv4-sp-d16)
  set(FPU_FOR_cortex-m7      fpv5-d16)
  set(FPU_FOR_cortex-m33     fpv5-d16)

  set(TOOLCHAIN_C_FLAGS
    -mthumb
    -mcpu=${GCC_M_CPU}
    )

  if(CONFIG_FLOAT)
    list(APPEND TOOLCHAIN_C_FLAGS -mfpu=${FPU_FOR_${GCC_M_CPU}})
    if    (CONFIG_FP_SOFTABI)
      list(APPEND TOOLCHAIN_C_FLAGS -mfloat-abi=soft)
    elseif(CONFIG_FP_HARDABI)
      list(APPEND TOOLCHAIN_C_FLAGS -mfloat-abi=hard)
    endif()
  endif()

  set(CROSS_COMPILE_TARGET arm-${TOOLCHAIN_VENDOR}-eabi)
  set(SYSROOT_TARGET armv5-${TOOLCHAIN_VENDOR}-eabi)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

elseif("${ARCH}" STREQUAL "arc")
  set(CROSS_COMPILE_TARGET arc-${TOOLCHAIN_VENDOR}-elf)
  set(SYSROOT_TARGET arc-${TOOLCHAIN_VENDOR}-elf)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

elseif(CONFIG_TOOLCHAIN_VARIANT STREQUAL "iamcu")
  set(CROSS_COMPILE_TARGET i586-${TOOLCHAIN_VENDOR}-elfiamcu)
  set(SYSROOT_TARGET iamcu-${TOOLCHAIN_VENDOR}-elfiamcu)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

elseif("${ARCH}" STREQUAL "x86")
  set(CROSS_COMPILE_TARGET i586-${TOOLCHAIN_VENDOR}-elf)
  set(SYSROOT_TARGET i586-${TOOLCHAIN_VENDOR}-elf)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

elseif("${ARCH}" STREQUAL "nios2")
  set(CROSS_COMPILE_TARGET nios2-${TOOLCHAIN_VENDOR}-elf)
  set(SYSROOT_TARGET nios2-${TOOLCHAIN_VENDOR}-elf)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

elseif("${ARCH}" STREQUAL "xtensa")
  set(CROSS_COMPILE_TARGET xtensa-${TOOLCHAIN_VENDOR}-elf)
  set(SYSROOT_TARGET xtensa-${TOOLCHAIN_VENDOR}-elf)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

elseif("${ARCH}" STREQUAL "riscv32")
  set(CROSS_COMPILE_TARGET riscv32-${TOOLCHAIN_VENDOR}-elf)
  set(SYSROOT_TARGET riscv32-${TOOLCHAIN_VENDOR}-elf)
  set(CROSS_COMPILE ${TOOLCHAIN_HOME}/usr/bin/${CROSS_COMPILE_TARGET}/${CROSS_COMPILE_TARGET}-)

endif()

set(CMAKE_C_COMPILER   ${CROSS_COMPILE}gcc     CACHE INTERNAL " " FORCE)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++     CACHE INTERNAL " " FORCE)
set(CMAKE_OBJCOPY      ${CROSS_COMPILE}objcopy CACHE INTERNAL " " FORCE)
set(CMAKE_OBJDUMP      ${CROSS_COMPILE}objdump CACHE INTERNAL " " FORCE)
#set(CMAKE_LINKER      ${CROSS_COMPILE}ld      CACHE INTERNAL " " FORCE) # Not in use yet
set(CMAKE_AR           ${CROSS_COMPILE}ar      CACHE INTERNAL " " FORCE)
set(CMAKE_RANLILB      ${CROSS_COMPILE}ranlib  CACHE INTERNAL " " FORCE)

execute_process(
  COMMAND ${CMAKE_C_COMPILER} --print-file-name=include
  OUTPUT_VARIABLE _OUTPUT
)
string(REGEX REPLACE "\n" "" _OUTPUT ${_OUTPUT})
set(NOSTDINC ${_OUTPUT})

execute_process(
  COMMAND ${CMAKE_C_COMPILER} --print-file-name=include-fixed
  OUTPUT_VARIABLE _OUTPUT
)
string(REGEX REPLACE "\n" "" _OUTPUT ${_OUTPUT})
list(APPEND NOSTDINC ${_OUTPUT})

set(SYSROOT_DIR ${ZEPHYR_SDK_INSTALL_DIR}/sysroots/${SYSROOT_TARGET})

execute_process(
  COMMAND ${CMAKE_C_COMPILER} ${TOOLCHAIN_C_FLAGS} --sysroot ${SYSROOT_DIR} --print-libgcc-file-name
  OUTPUT_VARIABLE LIBGCC_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

execute_process(
  COMMAND ${CMAKE_C_COMPILER} ${TOOLCHAIN_C_FLAGS} --sysroot ${SYSROOT_DIR} --print-multi-directory
  OUTPUT_VARIABLE NEWLIB_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )

get_filename_component(LIBGCC_DIR ${LIBGCC_DIR} DIRECTORY)

set(LIB_INCLUDE_DIR -L${LIBGCC_DIR})
set(TOOLCHAIN_LIBS gcc)

set(DTC       ${TOOLCHAIN_HOME}/usr/bin/dtc)
set(QEMU_BIOS ${TOOLCHAIN_HOME}/usr/share/qemu)

if("${ARCH}" STREQUAL "x86")
  set(QEMU ${TOOLCHAIN_HOME}/usr/bin/qemu-system-i386)
else()
  set(QEMU ${TOOLCHAIN_HOME}/usr/bin/qemu-system-${ARCH})
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BUILD_SHARED_LIBS OFF)

# For CMake to be able to test if a compiler flag is supported by the
# toolchain we need to give CMake the necessary flags to compile and
# link a dummy C file.
#
# CMake checks compiler flags with check_c_compiler_flag() (Which we
# wrap with target_cc_option() in extentions.cmake)
set(CMAKE_REQUIRED_FLAGS "-nostartfiles -nostdlib --sysroot=${SYSROOT_DIR} -Wl,--unresolved-symbols=ignore-in-object-files")


set(LIBC_INCLUDE_DIR ${SYSROOT_DIR}/usr/include)
set(LIBC_LIBRARY_DIR ${SYSROOT_DIR}/usr/lib/${NEWLIB_DIR})

set(COMPILER gcc)
