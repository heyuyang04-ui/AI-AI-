/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>

#include "stm32f407zg-pfcllc.h"

void stm32_boardinitialize(void)
{
  /* Clock, vector table and the configured serial console are initialized by
   * the STM32 architecture layer before this hook.  Keep this early hook free
   * of blocking device operations. */
}

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  (void)stm32_bringup();
}
#endif
