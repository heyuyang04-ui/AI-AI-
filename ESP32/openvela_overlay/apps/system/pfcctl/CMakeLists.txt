# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SYSTEM_PFCCTL)
  nuttx_add_application(
    MODULE
    ${CONFIG_SYSTEM_PFCCTL}
    NAME
    ${CONFIG_SYSTEM_PFCCTL_PROGNAME}
    SRCS
    pfcctl_main.c
    pfcctl_protocol.c
    STACKSIZE
    ${CONFIG_SYSTEM_PFCCTL_STACKSIZE}
    PRIORITY
    ${CONFIG_SYSTEM_PFCCTL_PRIORITY})
endif()
