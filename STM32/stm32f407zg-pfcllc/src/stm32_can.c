/****************************************************************************
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <debug.h>
#include <nuttx/can/can.h>

#include "stm32_can.h"

#ifdef CONFIG_STM32_CAN_CHARDRIVER

int stm32_can_setup(void)
{
#ifdef CONFIG_STM32_CAN1
  struct can_dev_s *can;
  int ret;

  can = stm32_caninitialize(1);
  if (can == NULL)
    {
      canerr("ERROR: failed to initialize CAN1\n");
      return -ENODEV;
    }

  ret = can_register("/dev/can0", can);
  if (ret < 0)
    {
      canerr("ERROR: can_register(/dev/can0) failed: %d\n", ret);
    }

  return ret;
#else
  return -ENODEV;
#endif
}

#endif
