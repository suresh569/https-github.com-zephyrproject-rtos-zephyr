set_ifndef(XTOOLS_TOOLCHAIN_PATH "$ENV{XTOOLS_TOOLCHAIN_PATH}")
set(       XTOOLS_TOOLCHAIN_PATH    ${XTOOLS_TOOLCHAIN_PATH} CACHE PATH "")
assert(    XTOOLS_TOOLCHAIN_PATH     "XTOOLS_TOOLCHAIN_PATH is not set")

set(TOOLCHAIN_HOME ${XTOOLS_TOOLCHAIN_PATH})

set(COMPILER gcc)      # Ensures compiler/${COMPILER}.cmake is run
set(LINKER   ld)       # Ensures linker/${LINKER}.cmake is run
set(BINTOOLS binutils) # Ensures bintools/${BINTOOLS}.cmake is run

set(CROSS_COMPILE_TARGET_arm arm-zephyr-eabi)
if("${ARCH}" STREQUAL "x86")
  if(CONFIG_X86_IAMCU)
    set(CROSS_COMPILE_TARGET_x86  i586-zephyr-elfiamcu)
  else()
    set(CROSS_COMPILE_TARGET_x86 i586-zephyr-elf)
  endif()
endif()
set(CROSS_COMPILE_TARGET_nios2 nios2-zephyr-elf)
set(CROSS_COMPILE_TARGET_riscv32 riscv32-zephyr-elf)
set(CROSS_COMPILE_TARGET_mips mipsel-zephyr-elf)
set(CROSS_COMPILE_TARGET_xtensa xtensa-zephyr-elf)
set(CROSS_COMPILE_TARGET_arc arc-zephyr-elf)


set(CROSS_COMPILE_TARGET     ${CROSS_COMPILE_TARGET_${ARCH}})
set(SYSROOT_TARGET           ${CROSS_COMPILE_TARGET})

set(CROSS_COMPILE ${TOOLCHAIN_HOME}/${CROSS_COMPILE_TARGET}/bin/${CROSS_COMPILE_TARGET}-)
set(SYSROOT_DIR   ${TOOLCHAIN_HOME}/${SYSROOT_TARGET}/${SYSROOT_TARGET})
