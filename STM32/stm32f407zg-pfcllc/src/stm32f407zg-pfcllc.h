/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_ARM_STM32_STM32F407ZG_PFCLLC_SRC_BOARD_H
#define __BOARDS_ARM_STM32_STM32F407ZG_PFCLLC_SRC_BOARD_H

#include <nuttx/config.h>

int stm32_bringup(void);

#ifdef CONFIG_STM32_CAN_CHARDRIVER
int stm32_can_setup(void);
#endif

#endif
