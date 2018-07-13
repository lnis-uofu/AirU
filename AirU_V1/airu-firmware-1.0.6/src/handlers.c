#include "simplelink.h"
#include "netcfg.h"

#include "common.h"
#include "handlers.h"

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************


//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    if(pWlanEvent == NULL)
    {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER();
    }
    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);

            //
            // Information about the connected AP (like name, MAC etc) will be
            // available in 'slWlanConnectAsyncResponse_t'
            // Applications can use it if required
            //
            //  slWlanConnectAsyncResponse_t *pEventData = NULL;
            // pEventData = &pWlanEvent->EventData.STAandP2PModeWlanConnected;
            //

            // Copy new connection SSID and BSSID to global parameters
            memcpy(g_ucConnectionSSID,pWlanEvent->EventData.
                   STAandP2PModeWlanConnected.ssid_name,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
            memcpy(g_ucConnectionBSSID,
                   pWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                   SL_BSSID_LENGTH);

            UART_PRINT("[WLAN EVENT] Device Connected to the AP: %s , "
                       "BSSID: %x:%x:%x:%x:%x:%x\n\r",
                      g_ucConnectionSSID,g_ucConnectionBSSID[0],
                      g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                      g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                      g_ucConnectionBSSID[5]);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;

            // If the user has initiated 'Disconnect' request,
            //'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
            if(SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                UART_PRINT("[WLAN EVENT] Device disconnected from the AP: %s, "
                           "BSSID: %x:%x:%x:%x:%x:%x on application's "
                           "request \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            else
            {
                UART_PRINT("[WLAN ERROR] Device disconnected from the AP AP: %s, "
                           "BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                           g_ucConnectionSSID,g_ucConnectionBSSID[0],
                           g_ucConnectionBSSID[1],g_ucConnectionBSSID[2],
                           g_ucConnectionBSSID[3],g_ucConnectionBSSID[4],
                           g_ucConnectionBSSID[5]);
            }
            memset(g_ucConnectionSSID,0,sizeof(g_ucConnectionSSID));
            memset(g_ucConnectionBSSID,0,sizeof(g_ucConnectionBSSID));
        }
        break;

        case SL_WLAN_STA_CONNECTED_EVENT:
        {
            // when device is in AP mode and any client connects to device cc3xxx
            //SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            //CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION_FAILED);

            //
            // Information about the connected client (like SSID, MAC etc) will
            // be available in 'slPeerInfoAsyncResponse_t' - Applications
            // can use it if required
            //
            // slPeerInfoAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.APModeStaConnected;
            //

            UART_PRINT("[WLAN EVENT] Station connected to device\n\r");
        }
        break;

        case SL_WLAN_STA_DISCONNECTED_EVENT:
        {
            // when client disconnects from device (AP)
            //CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
            //CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

            //
            // Information about the connected client (like SSID, MAC etc) will
            // be available in 'slPeerInfoAsyncResponse_t' - Applications
            // can use it if required
            //
            // slPeerInfoAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.APModestaDisconnected;
            //
            UART_PRINT("[WLAN EVENT] Station disconnected from device\n\r");
        }
        break;

        case SL_WLAN_SMART_CONFIG_COMPLETE_EVENT:
        {
            //SET_STATUS_BIT(g_ulStatus, STATUS_BIT_SMARTCONFIG_START);

            //
            // Information about the SmartConfig details (like Status, SSID,
            // Token etc) will be available in 'slSmartConfigStartAsyncResponse_t'
            // - Applications can use it if required
            //
            //  slSmartConfigStartAsyncResponse_t *pEventData = NULL;
            //  pEventData = &pSlWlanEvent->EventData.smartConfigStartResponse;
            //

        }
        break;

        case SL_WLAN_SMART_CONFIG_STOP_EVENT:
        {
            // SmartConfig operation finished
            //CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_SMARTCONFIG_START);

            //
            // Information about the SmartConfig details (like Status, padding
            // etc) will be available in 'slSmartConfigStopAsyncResponse_t' -
            // Applications can use it if required
            //
            // slSmartConfigStopAsyncResponse_t *pEventData = NULL;
            // pEventData = &pSlWlanEvent->EventData.smartConfigStopResponse;
            //
        }
        break;

        default:
        {
            UART_PRINT("[WLAN EVENT] Unexpected event [0x%x]\n\r",
                       pWlanEvent->Event);
        }
        break;
    }
}

//*****************************************************************************
//
//! \brief This function handles network events such as IP acquisition, IP
//!           leased, IP released etc.
//!
//! \param[in]  pNetAppEvent - Pointer to NetApp Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
//    if(pNetAppEvent == NULL)
//    {
//        UART_PRINT("Null pointer\n\r");
//        LOOP_FOREVER();
//    }
//
//    switch(pNetAppEvent->Event)
//    {
//        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
//        {
//            SlIpV4AcquiredAsync_t *pEventData = NULL;
//
//            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);
//
//            //Ip Acquired Event Data
//            pEventData = &pNetAppEvent->EventData.ipAcquiredV4;
//
//            UART_PRINT("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d , "
//                       "Gateway=%d.%d.%d.%d\n\r",
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,3),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,2),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,1),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip,0),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,3),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,2),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,1),
//            SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway,0));
//
//            UNUSED(pEventData);
//        }
//        break;
//
//        case SL_NETAPP_IP_LEASED_EVENT:
//        {
//            SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
//
//            //
//            // Information about the IP-Leased details(like IP-Leased,lease-time,
//            // mac etc) will be available in 'SlIpLeasedAsync_t' - Applications
//            // can use it if required
//            //
//            // SlIpLeasedAsync_t *pEventData = NULL;
//            // pEventData = &pNetAppEvent->EventData.ipLeased;
//            //
//
//        }
//        break;
//
//        case SL_NETAPP_IP_RELEASED_EVENT:
//        {
//            CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);
//
//            //
//            // Information about the IP-Released details (like IP-address, mac
//            // etc) will be available in 'SlIpReleasedAsync_t' - Applications
//            // can use it if required
//            //
//            // SlIpReleasedAsync_t *pEventData = NULL;
//            // pEventData = &pNetAppEvent->EventData.ipReleased;
//            //
//        }
//		break;
//
//        default:
//        {
//            UART_PRINT("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
//                       pNetAppEvent->Event);
//        }
//        break;
//    }
}


//*****************************************************************************
//
//! \brief This function handles HTTP server events
//!
//! \param[in]  pServerEvent - Contains the relevant event information
//! \param[in]    pServerResponse - Should be filled by the user with the
//!                                      relevant response information
//!
//! \return None
//!
//****************************************************************************
void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent,
                                SlHttpServerResponse_t *pSlHttpServerResponse)
{
//    switch (pSlHttpServerEvent->Event)
//    {
//        case SL_NETAPP_HTTPGETTOKENVALUE_EVENT:
//        {
//            unsigned char *ptr;
//
//            ptr = pSlHttpServerResponse->ResponseData.token_value.data;
//            pSlHttpServerResponse->ResponseData.token_value.len = 0;
//            if(memcmp(pSlHttpServerEvent->EventData.httpTokenName.data,
//                    GET_token_TEMP, strlen((const char *)GET_token_TEMP)) == 0)
//            {
//                float fCurrentTemp;
//                TMP006DrvGetTemp(&fCurrentTemp);
//                char cTemp = (char)fCurrentTemp;
//                short sTempLen = itoa(cTemp,(char*)ptr);
//                ptr[sTempLen++] = ' ';
//                ptr[sTempLen] = 'F';
//                pSlHttpServerResponse->ResponseData.token_value.len += sTempLen;
//
//            }
//
//            if(memcmp(pSlHttpServerEvent->EventData.httpTokenName.data,
//                      GET_token_UIC, strlen((const char *)GET_token_UIC)) == 0)
//            {
//                if(g_iInternetAccess==0)
//                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"1");
//                else
//                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"0");
//                pSlHttpServerResponse->ResponseData.token_value.len =  1;
//            }
//
//            if(memcmp(pSlHttpServerEvent->EventData.httpTokenName.data,
//                       GET_token_ACC, strlen((const char *)GET_token_ACC)) == 0)
//            {
//
//                ReadAccSensor();
//                if(g_ucDryerRunning)
//                {
//                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"Running");
//                    pSlHttpServerResponse->ResponseData.token_value.len += strlen("Running");
//                }
//                else
//                {
//                    strcpy((char*)pSlHttpServerResponse->ResponseData.token_value.data,"Stopped");
//                    pSlHttpServerResponse->ResponseData.token_value.len += strlen("Stopped");
//                }
//            }
//
//
//
//        }
//            break;
//
//        case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT:
//        {
//            unsigned char led;
//            unsigned char *ptr = pSlHttpServerEvent->EventData.httpPostData.token_name.data;
//
//            //g_ucLEDStatus = 0;
//            if(memcmp(ptr, POST_token, strlen((const char *)POST_token)) == 0)
//            {
//                ptr = pSlHttpServerEvent->EventData.httpPostData.token_value.data;
//                if(memcmp(ptr, "LED", 3) != 0)
//                    break;
//                ptr += 3;
//                led = *ptr;
//                ptr += 2;
//                if(led == '1')
//                {
//                    if(memcmp(ptr, "ON", 2) == 0)
//                    {
//                        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
//                                                g_ucLEDStatus = LED_ON;
//
//                    }
//                    else if(memcmp(ptr, "Blink", 5) == 0)
//                    {
//                        GPIO_IF_LedOn(MCU_RED_LED_GPIO);
//                        g_ucLEDStatus = LED_BLINK;
//                    }
//                    else
//                    {
//                        GPIO_IF_LedOff(MCU_RED_LED_GPIO);
//                                                g_ucLEDStatus = LED_OFF;
//                    }
//                }
//                else if(led == '2')
//                {
//                    if(memcmp(ptr, "ON", 2) == 0)
//                    {
//                        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
//                    }
//                    else if(memcmp(ptr, "Blink", 5) == 0)
//                    {
//                        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO);
//                        g_ucLEDStatus = 1;
//                    }
//                    else
//                    {
//                        GPIO_IF_LedOff(MCU_ORANGE_LED_GPIO);
//                    }
//                }
//
//            }
//        }
//            break;
//        default:
//            break;
//    }
}

//*****************************************************************************
//
//! \brief This function handles General Events
//!
//! \param[in]     pDevEvent - Pointer to General Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
//    if(pDevEvent == NULL)
//    {
//        UART_PRINT("Null pointer\n\r");
//        LOOP_FOREVER();
//    }
//
//    //
//    // Most of the general errors are not FATAL are are to be handled
//    // appropriately by the application
//    //
//    UART_PRINT("[GENERAL EVENT] - ID=[%d] Sender=[%d]\n\n",
//               pDevEvent->EventData.deviceEvent.status,
//               pDevEvent->EventData.deviceEvent.sender);
}


//*****************************************************************************
//
//! This function handles socket events indication
//!
//! \param[in]      pSock - Pointer to Socket Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
//    if(pSock == NULL)
//    {
//        UART_PRINT("Null pointer\n\r");
//        LOOP_FOREVER();
//    }
//    //
//    // This application doesn't work w/ socket - Events are not expected
//    //
//    switch( pSock->Event )
//    {
//        case SL_SOCKET_TX_FAILED_EVENT:
//            switch( pSock->socketAsyncEvent.SockTxFailData.status)
//            {
//                case SL_ECLOSE:
//                    UART_PRINT("[SOCK ERROR] - close socket (%d) operation "
//                                "failed to transmit all queued packets\n\n",
//                                    pSock->socketAsyncEvent.SockTxFailData.sd);
//                    break;
//                default:
//                    UART_PRINT("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
//                                "(%d) \n\n",
//                                pSock->socketAsyncEvent.SockTxFailData.sd, pSock->socketAsyncEvent.SockTxFailData.status);
//                  break;
//            }
//            break;
//
//        default:
//        	UART_PRINT("[SOCK EVENT] - Unexpected Event [%x0x]\n\n",pSock->Event);
//          break;
//    }
}


//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************
