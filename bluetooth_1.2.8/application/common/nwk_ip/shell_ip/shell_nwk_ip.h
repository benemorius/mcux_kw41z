/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* Copyright 2016-2017 NXP
* All rights reserved.
* 
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifndef _SHELL_IP_H
#define _SHELL_IP_H
/*!=================================================================================================
\file       shell_ip.h
\brief      This is a header file for the shell application for the IP stack.
==================================================================================================*/

/*==================================================================================================
Include Files
==================================================================================================*/
#include "network_utils.h"
#include "shell_config.h"
#include "shell.h"
#include "ip.h"
#include "ip6.h"

/*==================================================================================================
Public macros
==================================================================================================*/

/*==================================================================================================
Public type definitions
==================================================================================================*/
typedef enum
{
    gAllIpAddr_c,
    gLinkLocalAddr_c,
    gMeshLocalAddr_c,
    gGlobalAddr_c
}ipAddrTypes_t;

typedef enum
{
    gShellIp_Success_c,
    gShellIp_Failure_c   
}shellIpStatus_t;
/*==================================================================================================
Public global variables declarations
==================================================================================================*/

/*==================================================================================================
Public function prototypes
==================================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*!*************************************************************************************************
\fn     void SHELLComm_Init(void)
\brief  This function is used to initialize the SHELL commands module.

\return         void
***************************************************************************************************/
void SHELLComm_Init(void);

#ifdef __cplusplus
}
#endif
/*================================================================================================*/
#endif  /* _SHELL_IP_H */


