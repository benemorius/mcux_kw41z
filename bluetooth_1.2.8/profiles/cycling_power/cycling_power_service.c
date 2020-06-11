/*! *********************************************************************************
* \addtogroup Cycling Power Service
* @{
 ********************************************************************************** */
/*!
* Copyright (c) 2014, Freescale Semiconductor, Inc.
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
#include "FunctionLib.h"
#include "Messaging.h"

#include "ble_general.h"
#include "gatt_db_app_interface.h"
#include "gatt_server_interface.h"
#include "gap_interface.h"

#include "cycling_power_interface.h"

/************************************************************************************
*************************************************************************************
* Private constants & macros
*************************************************************************************
************************************************************************************/
#define Cps_SuppportMultipleSensorLoc(serviceCfg)\
        (serviceCfg->cpsFeatures & gCps_MultipleSensorLocationsSupported_c)

#define Cps_BothTorqueAndForce(measurementFlags)\
        ((measurementFlags & gCps_ExtremeForceMagnitudesPresent_c) &&\
         (measurementFlags & gCps_ExtremeTorqueMagnitudesPresent_c))

#define Cps_BothTorqueAndForceVector(vectorFlags)\
        ((vectorFlags & gCps_VectorInstantForceMagArrayPresent_c) &&\
         (vectorFlags & gCps_VectorInstantTorqueMagArrayPresent_c))
          
#define Cps_TorqueOrForceVector(vectorFlags)\
        (vectorFlags & (gCps_VectorInstantForceMagArrayPresent_c | gCps_VectorInstantTorqueMagArrayPresent_c))
/***********************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
********************************************************************************** */

/***********************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
********************************************************************************** */
/*! Cycling Power Service - Subscribed Client */
static deviceId_t mCps_ClientDeviceId = gInvalidDeviceId_c;

static const  ctsDateTime_t mFactoryCalibrationDate = {gCps_FactoryCalibrationDate_c};
/***********************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
********************************************************************************** */
static bleResult_t Cps_UpdateMeasurementCharacteristic
(
    uint16_t handle,
    cpsMeasurement_t *pMeasurement
);

static bleResult_t Cps_UpdatePowerVectorCharacteristic
(
    uint16_t handle,
    cpsPowerVector_t *pPowerVector
);

static void Cps_SendNotification
(
     uint16_t handle
);

static bleResult_t Cps_SetFeatures
(
    uint16_t        handle,
    cpsFeature_t    features
);

static void Cps_SendProcedureResponse
(
     cpsConfig_t *pServiceConfig,
     gattServerAttributeWrittenEvent_t* pEvent
);

static bool_t Cps_ValidateSensorLocation
(
     cpsSensorLocation_t sensorLoc,
     cpsSensorLocation_t* aSensorLoc,
     uint8_t count
);

/***********************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
********************************************************************************** */

bleResult_t Cps_Start (cpsConfig_t *pServiceConfig)
{
    bleResult_t result;
    uint16_t    handle;
    uint16_t    uuid = gBleSig_CpFeature_d;

    /* Get handle of characteristic */
    GattDb_FindCharValueHandleInService(pServiceConfig->serviceHandle,
        gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    result = Cps_SetFeatures(handle, pServiceConfig->cpsFeatures);

    if (result != gBleSuccess_c)
        return result;

    /* Get handle of characteristic */
    result = Cps_SetSensorLocation(pServiceConfig->serviceHandle, pServiceConfig->sensorLocation);

    if (result != gBleSuccess_c)
        return result;

    return gBleSuccess_c;

}

bleResult_t Cps_Stop (cpsConfig_t *pServiceConfig)
{
    return Cps_Unsubscribe(mCps_ClientDeviceId);
}

bleResult_t Cps_Subscribe(deviceId_t clientdeviceId)
{
    mCps_ClientDeviceId = clientdeviceId;

    return gBleSuccess_c;
}

bleResult_t Cps_Unsubscribe()
{
    mCps_ClientDeviceId = gInvalidDeviceId_c;
    return gBleSuccess_c;
}

bleResult_t Cps_RecordMeasurement(uint16_t serviceHandle, cpsMeasurement_t *pMeasurement)
{
    uint16_t    handle;
    bleResult_t result;
    uint16_t    uuid = gBleSig_CpMeasurement_d;

    /* Get handle of characteristic */
    result = GattDb_FindCharValueHandleInService(serviceHandle,
        gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    if (result != gBleSuccess_c)
        return result;

    /* Update characteristic value and send indication */
    if (!Cps_UpdateMeasurementCharacteristic(handle, pMeasurement))
    {
        Cps_SendNotification(handle);
    }
    return gBleSuccess_c;
}

bleResult_t Cps_RecordPowerVector(uint16_t serviceHandle, cpsPowerVector_t *pPowerVector)
{
    uint16_t    handle;
    bleResult_t result;
    uint16_t    uuid = gBleSig_CpVector_d;

    /* Get handle of characteristic */
    result = GattDb_FindCharValueHandleInService(serviceHandle,
        gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    if (result != gBleSuccess_c)
        return result;

    /* Update characteristic value and send indication */
    if (!Cps_UpdatePowerVectorCharacteristic(handle, pPowerVector))
    {
        Cps_SendNotification(handle);
    }
    return gBleSuccess_c;
}

bleResult_t Cps_SetSensorLocation(uint16_t serviceHandle, cpsSensorLocation_t sensorLocation)
{
    uint16_t    handle;
    bleResult_t result;
    uint16_t    uuid = gBleSig_SensorLocation_d;

    /* Get handle of characteristic */
    result = GattDb_FindCharValueHandleInService(serviceHandle,
        gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    if (result != gBleSuccess_c)
        return result;

    /* Write attribute value*/
    return GattDb_WriteAttribute(handle, sizeof(cpsSensorLocation_t), &sensorLocation);
}

void Cps_ControlPointHandler (cpsConfig_t *pServiceConfig, gattServerAttributeWrittenEvent_t *pEvent)
{
    bool_t  fIndicationActive = FALSE;
    uint16_t  handleCccd;
   
    /* Get handle of CCCD */
    if (GattDb_FindCccdHandleForCharValueHandle(pEvent->handle, &handleCccd) != gBleSuccess_c)
        return;

    /* Check if indications are properly configured */
    Gap_CheckIndicationStatus(mCps_ClientDeviceId, handleCccd, &fIndicationActive);

    if(!fIndicationActive)
    {
        GattServer_SendAttributeWrittenStatus(mCps_ClientDeviceId, pEvent->handle,
                                                 gAttErrCodeCccdImproperlyConfigured_c);
        return;
    }

    /* Check if another procedure is in progress */
    if (pServiceConfig->procInProgress)
    {
        GattServer_SendAttributeWrittenStatus(mCps_ClientDeviceId, pEvent->handle,
                                                 gAttErrCodeProcedureAlreadyInProgress_c);
        return;
    }

    pServiceConfig->procInProgress = TRUE;

    /* Procedure received successfully */
    GattServer_SendAttributeWrittenStatus(mCps_ClientDeviceId, pEvent->handle,
                                                     gAttErrCodeNoError_c);

    Cps_SendProcedureResponse(pServiceConfig, pEvent);
}

bleResult_t Cps_FinishOffsetCompensation(cpsConfig_t *pServiceConfig, uint16_t rawForceTorque)
{
    uint16_t    handle;
    bleResult_t result;
    uint16_t    uuid = gBleSig_CpControlPoint_d;
    uint8_t data[5] = {gCps_RspCode_c, gCps_StartOffsetCompensation_c,
                        gCps_Success_c, 0x00, 0x00};

    data[3] = (uint8_t)(rawForceTorque & 0xFF);
    data[4] = (uint8_t)(rawForceTorque >> 8);

    /* Get handle of characteristic */
    result = GattDb_FindCharValueHandleInService(pServiceConfig->serviceHandle,
        gBleUuidType16_c, (bleUuid_t*)&uuid, &handle);

    if (result != gBleSuccess_c)
        return result;

    /* Write response in characteristic */
    GattDb_WriteAttribute(handle, sizeof(data), data);

    pServiceConfig->offsetCompensationOngoing = FALSE;
    
    /* Indicate value to client */
    return GattServer_SendIndication(mCps_ClientDeviceId, handle);
}

void Cps_PowerMeasurementSccdWritten (cpsConfig_t *pServiceConfig, gattServerAttributeWrittenEvent_t *pEvent)
{
    bool_t  fIndicationActive = FALSE;
    uint16_t  handleCccd;

    /* Get handle of CCCD */
    if (GattDb_FindCccdHandleForCharValueHandle(pEvent->handle, &handleCccd) != gBleSuccess_c)
        return;

    /* Check if indications are properly configured */
    Gap_CheckIndicationStatus(mCps_ClientDeviceId, handleCccd, &fIndicationActive);

    if(!fIndicationActive)
    {
        GattServer_SendAttributeWrittenStatus(mCps_ClientDeviceId, pEvent->handle,
                                                 gAttErrCodeCccdImproperlyConfigured_c);
        return;
    }

    /* Check if another procedure is in progress */
    if (pServiceConfig->procInProgress)
    {
        GattServer_SendAttributeWrittenStatus(mCps_ClientDeviceId, pEvent->handle,
                                                 gAttErrCodeProcedureAlreadyInProgress_c);
        return;
    }

    /* Procedure received successfully */
    GattServer_SendAttributeWrittenStatus(mCps_ClientDeviceId, pEvent->handle,
                                                     gAttErrCodeNoError_c);

    Cps_SendProcedureResponse(pServiceConfig, pEvent);
}

/***********************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/
static bleResult_t Cps_UpdateMeasurementCharacteristic
(
    uint16_t handle,
    cpsMeasurement_t *pMeasurement
)
{
    uint8_t charValue[31];
    uint8_t index = 0;

    if (Cps_BothTorqueAndForce(pMeasurement->flags))
    {
        return gBleInvalidParameter_c;
    }

    /* Copy flags and instananeous power (4 bytes) */
    FLib_MemCpy(&charValue[index], &pMeasurement->flags, 2*sizeof(uint16_t));
    index += 2 * sizeof(uint16_t);

    if (pMeasurement->flags & gCps_PedalPowerBalancePresent_c)
    {
        charValue[index] = pMeasurement->pedalPowerBalance;
        index += 1;
    }

    if (pMeasurement->flags & gCps_AccumulatedTorquePresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->accumulatedTorque, sizeof(uint16_t));
        index += sizeof(uint16_t);
    }

    if (pMeasurement->flags & gCps_WheelRevolutionDataPresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->cumulativeWheelRevs,
            sizeof(uint16_t) + sizeof(uint32_t));
        index += (sizeof(uint16_t) + sizeof(uint32_t));
    }

    if (pMeasurement->flags & gCps_CrankRevolutionDataPresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->cumulativeCrankRevs,
                    2 * sizeof(uint16_t));
        index += (2 * sizeof(uint16_t));
    }

    if ((pMeasurement->flags & gCps_ExtremeForceMagnitudesPresent_c) ||
        (pMeasurement->flags & gCps_ExtremeTorqueMagnitudesPresent_c))
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->magnitude, 2 * sizeof(uint16_t));
        index += (2 * sizeof(uint16_t));
    }

    if (pMeasurement->flags & gCps_ExtremeAnglesPresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->extremeAngles, 3 * sizeof(uint8_t));
        index += (3 * sizeof(uint8_t));
    }

    if (pMeasurement->flags & gCps_TopDeadSpotAnglesPresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->topDeadSpotAngle, sizeof(uint16_t));
        index += sizeof(uint16_t);
    }

    if (pMeasurement->flags & gCps_BotomDeadSpotAnglesPresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->bottomDeadSpotAngle, sizeof(uint16_t));
        index += sizeof(uint16_t);
    }

    if (pMeasurement->flags & gCps_AccumulatedEnergyPresent_c)
    {
        FLib_MemCpy(&charValue[index], &pMeasurement->accumulatedEnergy, sizeof(uint16_t));
        index += sizeof(uint16_t);
    }

    return GattDb_WriteAttribute(handle, index, &charValue[0]);
}

static void Cps_SendNotification
(
  uint16_t handle
)
{
    uint16_t  hCccd;
    bool_t isNotificationActive;

    /* Get handle of CCCD */
    if (GattDb_FindCccdHandleForCharValueHandle(handle, &hCccd) != gBleSuccess_c)
        return;

    if (gBleSuccess_c == Gap_CheckNotificationStatus
        (mCps_ClientDeviceId, hCccd, &isNotificationActive) &&
        TRUE == isNotificationActive)
    {
        GattServer_SendNotification(mCps_ClientDeviceId, handle);
    }
}

static bleResult_t Cps_UpdatePowerVectorCharacteristic
(
    uint16_t handle,
    cpsPowerVector_t *pPowerVector
)
{
    uint8_t *pCursor = (uint8_t *)pPowerVector + 1;
    uint8_t length;

    if (Cps_BothTorqueAndForceVector(pPowerVector->flags))
    {
        return gBleInvalidParameter_c;
    }

    /* Do in-place processing */

    /* Flags remain in place */
    pCursor += 1;

    if (pPowerVector->flags & gCps_VectorCrankRevDataPresent_c)
    {
        /* Crank data remains in place*/
        pCursor += 2 * sizeof(uint16_t);
    }

    if (pPowerVector->flags & gCps_VectorFirstCrankMeasAnglePresent_c)
    {
        /* Crank data remains in place*/
        pCursor += sizeof(uint16_t);
    }

    if (Cps_TorqueOrForceVector(pPowerVector->flags) && (pPowerVector->arrayLength > 0))
    {
        FLib_MemInPlaceCpy(pCursor, pPowerVector->instantMagnitudeArray, pPowerVector->arrayLength * sizeof(uint16_t));
        pCursor += pPowerVector->arrayLength * sizeof(uint16_t);
    }

    length = (uint8_t)(pCursor - (uint8_t *)pPowerVector - 1) ;

    return GattDb_WriteAttribute(handle, length, (uint8_t *)pPowerVector + 1);
}

static bleResult_t Cps_SetFeatures
(
    uint16_t        handle,
    cpsFeature_t    features
)
{
    return GattDb_WriteAttribute(handle, sizeof(cpsFeature_t), (uint8_t*)&features);
}

static void Cps_SendProcedureResponse
(
     cpsConfig_t *pServiceConfig,
     gattServerAttributeWrittenEvent_t* pEvent
)
{
    uint8_t rspSize = 3;
    cpsProcedure_t*   pResponse;
    cpsProcedure_t*   pProcedure = (cpsProcedure_t*)pEvent->aValue;
    uint8_t           procDataLength = pEvent->cValueLength - sizeof(cpsOpCode_t);
    
    /* Allocate buffer for biggest response */
    pResponse = MSG_Alloc(rspSize + 
                      FLib_GetMax((sizeof(ctsDateTime_t) - 1), 
                      pServiceConfig->pUserData->cNumOfSupportedSensorLocations));

    if (pResponse == NULL)
    {
        return;
    }

    pResponse->opCode = gCps_RspCode_c;
    pResponse->procedureData.response.reqOpCode = pProcedure->opCode;



    switch (pProcedure->opCode)
    {
        case gCps_SetCummulativeValue_c:
        {
            if (procDataLength == sizeof(uint32_t))
            {
                pResponse->procedureData.response.rspValue = gCps_Success_c;
                pServiceConfig->pUserData->cumulativeWheelRevs = pProcedure->procedureData.cummulativeValue;
            }
            else
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }
            break;
        }

        case gCps_UpdateSensorLocation_c:
        {
            if(!Cps_SuppportMultipleSensorLoc(pServiceConfig))
            {
                pResponse->procedureData.response.rspValue = gCps_OpCodeNotSupported_c;
            }
            else if (!Cps_ValidateSensorLocation(pProcedure->procedureData.sensorLocation,
                                                    (void*)pServiceConfig->pUserData->pSupportedSensorLocations,
                                                    pServiceConfig->pUserData->cNumOfSupportedSensorLocations))
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }
            else
            {
                Cps_SetSensorLocation(pServiceConfig->serviceHandle, pProcedure->procedureData.sensorLocation);
                pResponse->procedureData.response.rspValue = gCps_Success_c;
            }
            break;
        }

        case gCps_ReqSupportedSensorLoc_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;

            /* Copy in supported locations */
            FLib_MemCpy(pResponse->procedureData.response.rspData.sensorLocation,
                        (void*)pServiceConfig->pUserData->pSupportedSensorLocations,
                        pServiceConfig->pUserData->cNumOfSupportedSensorLocations * sizeof(cpsSensorLocation_t));
            rspSize += pServiceConfig->pUserData->cNumOfSupportedSensorLocations;
            break;
        }

        case gCps_SetCrankLength_c:
        {
            if (procDataLength == sizeof(uint16_t))
            {
                pResponse->procedureData.response.rspValue = gCps_Success_c;
                pServiceConfig->pUserData->crankLength = pProcedure->procedureData.crankLength;
            }
            else
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }
            break;
        }
        
        case gCps_RequestCrankLength_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;
            pResponse->procedureData.response.rspData.crankLength = pServiceConfig->pUserData->crankLength;
            rspSize += sizeof(uint16_t);
            break;
        }
        
        case gCps_SetChainLength_c:
        {
            if (procDataLength == sizeof(uint16_t))
            {
                pResponse->procedureData.response.rspValue = gCps_Success_c;
                pServiceConfig->pUserData->chainLength = pProcedure->procedureData.chainLength;
            }
            else
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }
            break;
        }
        
        case gCps_RequestChainLength_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;
            pResponse->procedureData.response.rspData.chainLength = pServiceConfig->pUserData->chainLength;
            rspSize += sizeof(uint16_t);
            break;
        }
        
        case gCps_SetChainWeight_c:
        {
            if (procDataLength == sizeof(uint16_t))
            {
                pResponse->procedureData.response.rspValue = gCps_Success_c;
                pServiceConfig->pUserData->chainWeight = pProcedure->procedureData.chainWeight;
            }
            else
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }
            break;
        }
        
        case gCps_RequestChainWeight_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;
            pResponse->procedureData.response.rspData.chainWeight = pServiceConfig->pUserData->chainWeight;
            rspSize += sizeof(uint16_t);
            break;
        }

        case gCps_SetSpanLength_c:
        {
            if (procDataLength == sizeof(uint16_t))
            {
                pResponse->procedureData.response.rspValue = gCps_Success_c;
                pServiceConfig->pUserData->spanLength = pProcedure->procedureData.spanLength;
            }
            else
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }
            break;
        }

        case gCps_RequestSpanLength_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;
            pResponse->procedureData.response.rspData.spanLength = pServiceConfig->pUserData->spanLength;
            rspSize += sizeof(uint16_t);
            break;
        }

        case gCps_StartOffsetCompensation_c:
        {
            /* Offset Compensation is started */
            pServiceConfig->offsetCompensationOngoing = TRUE;
            MSG_Free(pResponse);
            return;
        }

        case gCps_MaskCpsContent_c:
        {
            if (procDataLength == sizeof(uint16_t) &&
                !(pProcedure->procedureData.contentMask & gCps_Reserved_c))
            {
                pResponse->procedureData.response.rspValue = gCps_Success_c;
                pServiceConfig->maskContent = pProcedure->procedureData.contentMask;
            }
            else
            {
                pResponse->procedureData.response.rspValue = gCps_InvalidParameter_c;
            }

            break;
        }

        case gCps_ReqSamplingRate_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;
            pResponse->procedureData.response.rspData.samplingRate = gCps_SamplingRate_c;
            rspSize += sizeof(uint8_t);
            break;
        }

        case gCps_ReqFactoryCalibrationDate_c:
        {
            pResponse->procedureData.response.rspValue = gCps_Success_c;
            FLib_MemCpy(&pResponse->procedureData.response.rspData.factoryCalibrationDate,
                        (void*)&mFactoryCalibrationDate,sizeof(ctsDateTime_t));
            rspSize += (sizeof(ctsDateTime_t) - 1);
            break;
        }
        
        default:
        {
            pResponse->procedureData.response.rspValue = gCps_OpCodeNotSupported_c;
            break;
        }
    }
    /* Write response in characteristic */
    GattDb_WriteAttribute(pEvent->handle, rspSize, (uint8_t*) pResponse);

    /* Free buffer */
    MSG_Free(pResponse);

    /* Indicate value to client */
    GattServer_SendIndication(mCps_ClientDeviceId, pEvent->handle);
}

static bool_t Cps_ValidateSensorLocation(cpsSensorLocation_t sensorLoc, cpsSensorLocation_t* aSensorLoc, uint8_t count)
{
    uint8_t i;
    for (i = 0; i < count; i++)
    {
        if(aSensorLoc[i] == sensorLoc)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*! *********************************************************************************
* @}
********************************************************************************** */
