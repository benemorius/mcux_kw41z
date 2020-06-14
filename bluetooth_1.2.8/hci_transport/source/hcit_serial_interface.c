/*! *********************************************************************************
* \addtogroup HCI
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
#include "MemManager.h"

#include "ble_general.h"
#include "hci_transport.h"
#include "od.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/************************************************************************************
*************************************************************************************
* Private macros
*************************************************************************************
************************************************************************************/

/************************************************************************************
*************************************************************************************
* Private type definitions
*************************************************************************************
************************************************************************************/
typedef PACKED_STRUCT hciCommandPacketHeader_tag
{
    uint16_t    opCode;
    uint8_t     parameterTotalLength;
}hciCommandPacketHeader_t;

typedef PACKED_STRUCT hciAclDataPacketHeader_tag
{
    uint16_t    handle      :12;
    uint16_t    pbFlag      :2;
    uint16_t    bcFlag      :2;
    uint16_t    dataTotalLength;
}hciAclDataPacketHeader_t;

typedef PACKED_STRUCT hciEventPacketHeader_tag
{
    hciEventCode_t  eventCode;
    uint8_t     dataTotalLength;
}hciEventPacketHeader_t;

typedef PACKED_STRUCT hcitPacketHdr_tag
{
    hciPacketType_t packetTypeMarker;
    PACKED_UNION
    {
        hciAclDataPacketHeader_t    aclDataPacket;
        hciEventPacketHeader_t      eventPacket;
        hciCommandPacketHeader_t    commandPacket;
    };
}hcitPacketHdr_t;

typedef PACKED_STRUCT hcitPacketStructured_tag
{
    hcitPacketHdr_t header;
    uint8_t         payload[gHcitMaxPayloadLen_c];
} hcitPacketStructured_t;

typedef PACKED_UNION hcitPacket_tag
{
    /* The entire packet as unformatted data. */
    uint8_t raw[sizeof(hcitPacketStructured_t)];
}hcitPacket_t;

typedef struct hcitComm_tag
{
    hcitPacket_t        *pPacket;
    hcitPacketHdr_t     pktHeader;
    uint16_t            bytesReceived;
    uint16_t            expectedLength;
}hcitComm_t;

typedef uint8_t detectState_t;
typedef enum{
    mDetectMarker_c       = 0,
    mDetectHeader_c,
    mPacketInProgress_c
}detectState_tag;

/************************************************************************************
*************************************************************************************
* Private memory declarations
*************************************************************************************
************************************************************************************/
static bool_t   mHcitInit = FALSE;
static uint8_t  gHcitSerMgrIf;

static hcitComm_t mHcitData;
hciTransportInterface_t  mTransportInterface;

static detectState_t  mPacketDetectStep;

static hcitPacket_t mHcitPacketRaw;
/************************************************************************************
*************************************************************************************
* Private functions prototypes
*************************************************************************************
************************************************************************************/
void Hcit_RxCallBack(void *pData);

/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
************************************************************************************/
#if gUseHciTransportDownward_d
    osaSemaphoreId_t gHciDataBufferingSem;
#endif /* gUseHciTransportDownward_d */

/************************************************************************************
*************************************************************************************
* Public functions
*************************************************************************************
************************************************************************************/
extern bleResult_t Ble_HciRecv
    (
        hciPacketType_t packetType,
        void* pHciPacket,
        uint16_t hciPacketLength
    );


/*! *********************************************************************************
* \brief  
*
* \param[in]    
*
* \param[out]   
*
* \return  
*
* \pre 
*
* \remarks 
*
********************************************************************************** */
bleResult_t Hcit_Init( hcitConfigStruct_t* hcitConfigStruct )
{
    bleResult_t result = gHciSuccess_c;
//     serialStatus_t serialStatus = gSerial_Success_c;

    if( mHcitInit == FALSE )
    {
#if gUseHciTransportDownward_d
        gHciDataBufferingSem = OSA_SemaphoreCreate(0);    
        
        if (gHciDataBufferingSem == NULL)
        {
            return gHciTransportError_c;
        }
#endif /* gUseHciTransportDownward_d */
    
        /* Initialize HCI Transport interface */
        mTransportInterface = hcitConfigStruct->transportInterface;

//         SerialManager_Init();
//
//         /* Initialize HCI Transport */
//         serialStatus = Serial_InitInterface(
//             &gHcitSerMgrIf,
//             hcitConfigStruct->interfaceType,
//             hcitConfigStruct->interfaceChannel );
//
//         if (serialStatus != gSerial_Success_c)
//         {
//             return gHciTransportError_c;
//         }
//
//         Serial_SetBaudRate(gHcitSerMgrIf, hcitConfigStruct->interfaceBaudrate);
//
//         /* Install Controller Events Callback handler */
//         Serial_SetRxCallBack(gHcitSerMgrIf, Hcit_RxCallBack, NULL);

        /* Flag initialization on module */
        mHcitInit = TRUE;
    }
    else
    {
        /* Module has already been initialized */
        result = gHciAlreadyInit_c;
    }

    return result;
}

/*! *********************************************************************************
* \brief  
*
* \param[in]    
*
* \param[out]   
*
* \return  
*
* \pre 
*
* \remarks 
*
********************************************************************************** */

extern int (*ble_hci_ram_rx_cmd_hs_cb)(uint8_t*, void*);
extern void *ble_hci_ram_rx_cmd_hs_arg;

extern int _ble_hci_ram_rx_cmd_hs_cb(uint8_t *data, uint16_t len);
extern int _ble_hci_ram_rx_acl_hs_cb(uint8_t *data, uint16_t len);

bleResult_t Hcit_SendPacket
    (
        hciPacketType_t packetType,
        void*           pPacket,
        uint16_t        packetSize
    )
{
    bleResult_t result = gBleSuccess_c;

    if (packetType == 0x4) { /* CMD */
        _ble_hci_ram_rx_cmd_hs_cb(pPacket, packetSize);

    } else if (packetType == 0x2) { /* ACL */
        _ble_hci_ram_rx_acl_hs_cb(pPacket, packetSize);
    }

    return result;
}

/*! *********************************************************************************
* \brief  
*
* \param[in]    
*
* \param[out]   
*
* \return  
*
* \pre 
*
* \remarks 
*
********************************************************************************** */
bleResult_t Hcit_RecvPacket
    (
        void*           pPacket,
        uint16_t        packetSize
    )
{
    bleResult_t result = gHciSuccess_c;

    uint8_t* aData = (uint8_t*) pPacket;
    uint8_t type = aData[0];
    
    if (type != 0x01 && type != 0x02 && type != 0x04)
    {
        result = /* Something more meaningful? */ gHciTransportError_c;
    }
    else
    {
        hciPacketType_t packetType = (hciPacketType_t) type;    
        result = Ble_HciRecv
        (
            packetType,
            aData + 1,
            packetSize - 1
        );

        MEM_BufferFree( pPacket );  ///TODO: Verify here in FSCI
    }

    return result;
}

/************************************************************************************
*************************************************************************************
* Private functions
*************************************************************************************
************************************************************************************/

static inline void Hcit_SendMessage(void)
{
    /* Send the message to HCI */
    mTransportInterface( mHcitData.pktHeader.packetTypeMarker,
                                mHcitData.pPacket,
                                mHcitData.bytesReceived);

    mHcitData.pPacket = NULL;
    mPacketDetectStep = mDetectMarker_c;  
}

void Hcit_RxCallBack(void *pData)
{
    uint8_t         recvChar;
    uint16_t        count;
    if( Serial_GetByteFromRxBuffer( gHcitSerMgrIf, &recvChar, &count) != gSerial_Success_c )
    {
        return;
    }

    while( count )
    {
        switch( mPacketDetectStep )
        {
            case mDetectMarker_c:
                if( (recvChar == gHciDataPacket_c) || (recvChar == gHciEventPacket_c) ||
                    (recvChar == gHciCommandPacket_c) )
                {
                    mHcitData.pPacket = (hcitPacket_t*)&mHcitData.pktHeader;

                    mHcitData.pktHeader.packetTypeMarker = (hciPacketType_t)recvChar;
                    mHcitData.bytesReceived = 1;

                    mPacketDetectStep = mDetectHeader_c;
                }
                break;

            case mDetectHeader_c:
                mHcitData.pPacket->raw[mHcitData.bytesReceived++] = recvChar;
                switch( mHcitData.pktHeader.packetTypeMarker ) 
                {
                    case gHciDataPacket_c:
                        /* ACL Data Packet */
                        if( mHcitData.bytesReceived == (gHciAclDataPacketHeaderLength_c + 1) )
                        {
                            /* Validate ACL Data packet length */
                            if( mHcitData.pktHeader.aclDataPacket.dataTotalLength > gHcLeAclDataPacketLengthDefault_c )
                            {
                                mHcitData.pPacket = NULL;
                                mPacketDetectStep = mDetectMarker_c;
                                break;
                            }
                            mHcitData.expectedLength = gHciAclDataPacketHeaderLength_c +
                                                       mHcitData.pktHeader.aclDataPacket.dataTotalLength;
                            
                            mPacketDetectStep = mPacketInProgress_c;
                        }
                        break;

                    case gHciEventPacket_c:
                        /* HCI Event Packet */
                        if( mHcitData.bytesReceived == (gHciEventPacketHeaderLength_c + 1) )
                        {
                            /* Validate HCI Event packet length
                            if( mHcitData.pktHeader.eventPacket.dataTotalLength > gHcEventPacketLengthDefault_c )
                            {
                                mHcitData.pPacket = NULL;
                                mPacketDetectStep = mDetectMarker_c;
                                break;
                            } */
                            mHcitData.expectedLength = gHciEventPacketHeaderLength_c +
                                                       mHcitData.pktHeader.eventPacket.dataTotalLength;
                            mPacketDetectStep = mPacketInProgress_c;
                        }
                        break;

                    case gHciCommandPacket_c:           
                        /* HCI Command Packet */
                        if( mHcitData.bytesReceived == (gHciCommandPacketHeaderLength_c + 1) )
                        {

                            mHcitData.expectedLength = gHciCommandPacketHeaderLength_c +
                                                       mHcitData.pktHeader.commandPacket.parameterTotalLength;
                            mPacketDetectStep = mPacketInProgress_c;
                        }
                        break;
                    case gHciSynchronousDataPacket_c:   
                    default:
                        /* Not Supported */
                        break;
                }

                if( mPacketDetectStep == mPacketInProgress_c )
                {
                    mHcitData.pPacket = &mHcitPacketRaw;
                    FLib_MemCpy(mHcitData.pPacket, (uint8_t*)&mHcitData.pktHeader + 1, sizeof(hcitPacketHdr_t) - 1);
                    mHcitData.bytesReceived -= 1;
                    
                    if( mHcitData.bytesReceived == mHcitData.expectedLength )
                    {
                        Hcit_SendMessage();
                    }                    
                }
                break;

            case mPacketInProgress_c:
                mHcitData.pPacket->raw[mHcitData.bytesReceived++] = recvChar;

                if( mHcitData.bytesReceived == mHcitData.expectedLength )
                {
                    Hcit_SendMessage();
                }
                break;

            default:
                break;
        }

        if( Serial_GetByteFromRxBuffer( gHcitSerMgrIf, &recvChar, &count) != gSerial_Success_c )
        {
            return;
        }
    }
}

/*! *********************************************************************************
* @}
********************************************************************************** */
