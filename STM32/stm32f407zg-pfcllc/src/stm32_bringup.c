/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <syslog.h>

#include "stm32f407zg-pfcllc.h"

int stm32_bringup(void)
{
  int ret = 0;

#ifdef CONFIG_STM32_CAN_CHARDRIVER
  ret = stm32_can_setup();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: failed to register /dev/can0: %d\n", ret);
      return ret;
    }
#endif

  syslog(LOG_INFO,
         "F407ZG PFC/LLC board ready: console=USART1, gateway=USART6, CAN1=125k\n");
  return ret;
}
