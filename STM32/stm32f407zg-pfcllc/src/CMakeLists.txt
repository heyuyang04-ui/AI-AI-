# SPDX-License-Identifier: Apache-2.0

set(SRCS stm32_boot.c stm32_bringup.c)

if(CONFIG_STM32_CAN_CHARDRIVER)
  list(APPEND SRCS stm32_can.c)
endif()

target_sources(board PRIVATE ${SRCS})
set_property(GLOBAL PROPERTY LD_SCRIPT "${NUTTX_BOARD_DIR}/scripts/ld.script")
set_property(TARGET nuttx APPEND PROPERTY NUTTX_COMPILE_OPTIONS -funwind-tables)
set_property(GLOBAL APPEND PROPERTY COMPILE_OPTIONS -fno-strength-reduce)
