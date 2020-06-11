/*! *********************************************************************************
 * \addtogroup Alert Notification Service
 * @{
 ********************************************************************************** */
/*!
* Copyright (c) 2015, Freescale Semiconductor, Inc.
* Copyright 2016-2017 NXP
* All rights reserved.
* 
* file
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/************************************************************************************
*************************************************************************************
* Include
*************************************************************************************
************************************************************************************/
#include "EmbeddedTypes.h"

#include "ble_general.h"
#include "gatt_db_app_interface.h"
#include "gatt_server_interface.h"
#include "gap_interface.h"

#include "alert_notification_interface.h"
/************************************************************************************
*************************************************************************************
* Private constants & macros
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
/*! Alert Notification Service - Subscribed Client*/
static deviceId_t mAns_SubscribedClientId = gInvalidDeviceId_c;

/***********************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/
bleResult_t Ans_Start(ansConfig_t *pServiceConfig)
{

    mAns_SubscribedClientId = gInvalidDeviceId_c;
    return gBleSuccess_c;
}

bleResult_t Ans_Stop(ansConfig_t *pServiceConfig)
{
    return gBleSuccess_c;
}

bleResult_t Ans_Subscribe(deviceId_t clientdeviceId)
{
    mAns_SubscribedClientId = clientdeviceId;

    return gBleSuccess_c;
}

bleResult_t Ans_Unsubscribe(void)
{
    mAns_SubscribedClientId = gInvalidDeviceId_c;
    return gBleSuccess_c;
}

bleResult_t Ans_SendNewAlert(uint16_t serviceHandle, ansNewAlert_t *pAlert)
{
    uint16_t  handle, hCccd;
    bleResult_t result;
    uint16_t uuid = gBleSig_NewAlert_d;
    bool_t isNotificationActive;

    /* Get handle of characteristic */
    result = GattDb_FindCharValueHandleInService(serviceHandle,
                gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    if (result != gBleSuccess_c)
        return result;
    
    /* Get handle of CCCD */
    result = GattDb_FindCccdHandleForCharValueHandle(handle, &hCccd);
      
    if (result != gBleSuccess_c)
        return result;    

    /* Indicate value to client */
    if (gBleSuccess_c == Gap_CheckNotificationStatus
        (mAns_SubscribedClientId, hCccd, &isNotificationActive) &&
        TRUE == isNotificationActive)
    {
        result = GattServer_SendInstantValueNotification(mAns_SubscribedClientId, handle,
                    2 + pAlert->textLength, (uint8_t *)pAlert);
    }

    return result;
}

bleResult_t Ans_SendUnreadAlertStatus(uint16_t serviceHandle, ansUnreadAlertStatus_t *pAlert)
{
    uint16_t  handle, hCccd;
    bleResult_t result;
    uint16_t uuid = gBleSig_UnreadAlertStatus_d;
    bool_t isNotificationActive;

    /* Get handle of characteristic */
    result = GattDb_FindCharValueHandleInService(serviceHandle,
                gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    if (result != gBleSuccess_c)
        return result;
    
    /* Get handle of CCCD */
    result = GattDb_FindCccdHandleForCharValueHandle(handle, &hCccd);
      
    if (result != gBleSuccess_c)
        return result;        

    /* Indicate value to client */
    if (gBleSuccess_c == Gap_CheckNotificationStatus
        (mAns_SubscribedClientId, hCccd, &isNotificationActive) &&
        TRUE == isNotificationActive)
    {
        result = GattServer_SendInstantValueNotification(mAns_SubscribedClientId, handle,
                    sizeof(ansUnreadAlertStatus_t), (uint8_t *)pAlert);
    }

    return result;
}


void Ans_ControlPointHandler (ansConfig_t *pServiceConfig, gattServerAttributeWrittenEvent_t *pEvent)
{
    uint8_t retStatus =  gAttErrCodeNoError_c;
    ansCommand_t    command;

    command.commandId = pEvent->aValue[0];
    command.categoryId = pEvent->aValue[1];

    switch (command.commandId)
    {
        case  gAns_EnableNewIncomingAlert_c:
        {
            break;
        }
        case  gAns_EnableUnreadCategoryStatus_c:
        {
            break;
        }
        case  gAns_DisableNewIncomingAlert_c:
        {
            break;
        }
        case  gAns_DisableUnreadCategoryStatus_c:
        {
            break;
        }
        case  gAns_NotifyNewIncomingAlertImmediately_c:
        {
            break;
        }
        case  gAns_NotifyUnreadCategoryStatusImmediately_c:
        {
            break;
        }

        default:
        {
            retStatus = gAttErrCodeCmdNotSupported_c;
        }
        break;
    }

    GattServer_SendAttributeWrittenStatus(mAns_SubscribedClientId, pEvent->handle, retStatus);
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/


/*! *********************************************************************************
* @}
********************************************************************************** */
