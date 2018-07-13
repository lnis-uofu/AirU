//*****************************************************************************
//*****************************************************************************
#include "internet_if.h"

#define MAX_SSID_ENTRIES 9

typedef struct
{
    Sl_WlanNetworkEntry_t netEntries[20]; // Wlan Network Entry
    unsigned char sortedRSSI[20];
    unsigned char num_entries;
}netEntries_t;

typedef struct
{
    _u8 last_added_profile[30];
    _u8 profile_good;
} lastProfileInfo_t;
lastProfileInfo_t lastProfileInfo;



netEntries_t ScanResults;
//Sl_WlanNetworkEntry_t netEntries[20]; // Wlan Network Entry

// globals
static int g_iInternetAccess = -1;
static int g_iLastConnectionError = 0;
static unsigned char g_ucSLStatus;
static unsigned char g_httpResponseBuff[MAX_BUFF_SIZE + 1];
unsigned char g_numEntries;
static volatile unsigned char IP_str[20];
// define the tokens
static const unsigned char POST_token[]      = "__SL_P_ULD";
static const unsigned char POST_token_CON[]  = "__SL_P_S.R";
static const unsigned char GET_token_NET[]   = "__SL_G_UTP";
static const unsigned char GET_token_SSID[]  = "__SL_G_UAC";
static const unsigned char GET_token_SSID2[] = "__SL_G_UIC";
volatile unsigned char errorFlag;
static volatile unsigned char g_ucNetworkCheckProfiles;

// Network App specific status/error codes which are used only in this file
typedef enum {
    // Choosing this number to avoid overlap w/ host-driver's error codes
    DEVICE_NOT_IN_STATION_MODE = -0x7F0,
    DEVICE_NOT_IN_AP_MODE = DEVICE_NOT_IN_STATION_MODE - 1,
    DEVICE_NOT_IN_P2P_MODE = DEVICE_NOT_IN_AP_MODE - 1,
    DEVICE_START_FAILED = DEVICE_NOT_IN_STATION_MODE - 1,
    INVALID_HEX_STRING = DEVICE_START_FAILED - 1,
    TCP_RECV_ERROR = INVALID_HEX_STRING - 1,
    TCP_SEND_ERROR = TCP_RECV_ERROR - 1,
    FILE_NOT_FOUND_ERROR = TCP_SEND_ERROR - 1,
    INVALID_SERVER_RESPONSE = FILE_NOT_FOUND_ERROR - 1,
    FORMAT_NOT_SUPPORTED = INVALID_SERVER_RESPONSE - 1,
    FILE_OPEN_FAILED = FORMAT_NOT_SUPPORTED - 1,
    FILE_WRITE_ERROR = FILE_OPEN_FAILED - 1,
    INVALID_FILE = FILE_WRITE_ERROR - 1,
    SERVER_CONNECTION_FAILED = INVALID_FILE - 1,
    GET_HOST_IP_FAILED = SERVER_CONNECTION_FAILED - 1,
    SERVER_GET_TIME_FAILED = GET_HOST_IP_FAILED - 1,

    STATUS_CODE_MAX = -0xBB8
} e_NetAppStatusCodes;

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile unsigned long g_ulStatus = 0;              /* SimpleLink Status */
unsigned long g_ulStaIp = 0;                        /* Station IP address */
unsigned long g_ulGatewayIP = 0;                    /* Network Gateway IP address */
unsigned char g_ucConnectionSSID[SSID_LEN_MAX + 1]; /* Connection SSID */
unsigned char g_ucConnectionBSSID[BSSID_LEN_MAX];   /* Connection BSSID */
volatile unsigned short g_usConnectIndex;           /* Connection time delay index */
static volatile _u32 g_ulSSIDGETIndices;
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

static long Wlan_Scan(SlWlanMode_e mode);
static long Wlan_AutoConnect(void);

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- Start
//*****************************************************************************

#define CHECK_PROFILE_BIT       0
#define CONNECT_PRESSED_BIT     1

unsigned char getNetworkProfileRequestFlag()
{
    if(g_ucNetworkCheckProfiles){
        UART_PRINT("getNetworkProfileRequest: 0x%08x\n\r",g_ucNetworkCheckProfiles);
    }
    return GET_STATUS_BIT(g_ucNetworkCheckProfiles,CHECK_PROFILE_BIT);
}

unsigned char getUSERConnectPressedFlag()
{
    UART_PRINT("getNetworkProfileRequest: %i\n\r",g_ucNetworkCheckProfiles);
    return GET_STATUS_BIT(g_ucNetworkCheckProfiles,CONNECT_PRESSED_BIT);
}

void setNetworkProfileRequestFlag(unsigned char ucValue)
{
    if(ucValue)
    {
        SET_STATUS_BIT(g_ucNetworkCheckProfiles,CHECK_PROFILE_BIT);
    }
    else
    {
        CLR_STATUS_BIT(g_ucNetworkCheckProfiles,CHECK_PROFILE_BIT);
    }
}

void setUSERConnectPressedFlag(unsigned char ucValue)
{
    if(ucValue)
    {
        SET_STATUS_BIT(g_ucNetworkCheckProfiles,CONNECT_PRESSED_BIT);
    }
    else
    {
        CLR_STATUS_BIT(g_ucNetworkCheckProfiles,CONNECT_PRESSED_BIT);
    }
}

void setLastProfileGoodFlag(unsigned char ucValue)
{
    lastProfileInfo.profile_good = !!ucValue;
}

unsigned char getLastProfileGoodFlag()
{
    return lastProfileInfo.profile_good;
}

void getLastAddedProfile(char* ssid)
{
    UART_PRINT("Inside getLastAddedProfile(): [ %s ] \n\n\r",lastProfileInfo.last_added_profile);
    strcpy(ssid,(const char*)lastProfileInfo.last_added_profile);
}
unsigned long getStatus()
{
    return g_ulStatus;
}

// safe start
long sl_SStart()
{
    long lRetVal;
    if(!getSLStatus()){
        lRetVal = sl_Start(NULL,NULL,NULL);
        ERROR_LOOP(lRetVal);
        setSLStatus(1);
    }
}

// safe stop
long sl_SStop()
{
    long lRetVal = -1;
    if(getSLStatus()){
        lRetVal = sl_Stop(0xFF);
        ERROR_LOOP(lRetVal);
        setSLStatus(0);
    }
    return lRetVal;
}

long sl_Restart()
{
    sl_SStop();
    return sl_SStart();
}

unsigned char* getIP()
{
    return IP_str;
}

unsigned char getSLStatus()
{
    return g_ucSLStatus;
}

void setSLStatus(unsigned char status)
{
    g_ucSLStatus = status;
}

int getLastConnectionError(void)
{
    return g_iLastConnectionError;
}

void setLastConnectionError(int err)
{
    g_iLastConnectionError = err;
}

//*****************************************************************************
//
//! \brief The Function Handles WLAN Events
//!
//! \param[in]  pSlWlanEvent - Pointer to WLAN Event Info
//!
//! \return None
//!
//*****************************************************************************
void SimpleLinkWlanEventHandler(SlWlanEvent_t *pSlWlanEvent) {
    if (pSlWlanEvent == NULL) {
        return;
    }

//    UART_PRINT("A SimpleLinkWlanEvent is happening...[%lu]\n\r",(unsigned long)((SlWlanEvent_t*) pSlWlanEvent)->Event);

    switch (((SlWlanEvent_t*) pSlWlanEvent)->Event) {
    case SL_WLAN_CONNECT_EVENT: {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION_FAILED);

        //
        // Information about the connected AP (like name, MAC etc) will be
        // available in 'slWlanConnectAsyncResponse_t'-Applications
        // can use it if required
        //
        //  slWlanConnectAsyncResponse_t *pEventData = NULL;
        // pEventData = &pSlWlanEvent->EventData.STAandP2PModeWlanConnected;
        //

        // Copy new connection SSID and BSSID to global parameters
        memcpy(g_ucConnectionSSID,
                pSlWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_name,
                pSlWlanEvent->EventData.STAandP2PModeWlanConnected.ssid_len);
        memcpy(g_ucConnectionBSSID,
                pSlWlanEvent->EventData.STAandP2PModeWlanConnected.bssid,
                SL_BSSID_LENGTH);

        LOG("[WLAN EVENT: %lu] Device in STATION Mode and connected to AP.\n\r",SL_WLAN_CONNECT_EVENT);
        LOG("                >AP:    %s\n\r",g_ucConnectionSSID);
        LOG("                >BSSID: %x:%x:%x:%x:%x:%x\n\r",
                                            g_ucConnectionBSSID[0], g_ucConnectionBSSID[1],
                                            g_ucConnectionBSSID[2], g_ucConnectionBSSID[3],
                                            g_ucConnectionBSSID[4], g_ucConnectionBSSID[5]);
    }
        break;

    case SL_WLAN_DISCONNECT_EVENT: {
        slWlanConnectAsyncResponse_t* pEventData = NULL;

        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

        pEventData = &pSlWlanEvent->EventData.STAandP2PModeDisconnected;

        // If the user has initiated 'Disconnect' request,
        //'reason_code' is SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
        if (SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION
                == pEventData->reason_code) {
            LOG("[WLAN EVENT: %lu] Device disconnected from AP on the application's request.\n\r",
                       SL_WLAN_DISCONNECT_EVENT);
            LOG("                   AP:    %s\n\r", g_ucConnectionSSID);
            LOG("                   BSSID: %x:%x:%x:%x:%x:%x",
                    g_ucConnectionBSSID[0],g_ucConnectionBSSID[1],
                    g_ucConnectionBSSID[2],g_ucConnectionBSSID[3],
                    g_ucConnectionBSSID[4],g_ucConnectionBSSID[5]);
        } else {
            LOG("[WLAN ERROR] Device disconnected from the AP: %s,"
                    " BSSID: %x:%x:%x:%x:%x:%x on an ERROR..!! \n\r",
                    g_ucConnectionSSID, g_ucConnectionBSSID[0],
                    g_ucConnectionBSSID[1], g_ucConnectionBSSID[2],
                    g_ucConnectionBSSID[3], g_ucConnectionBSSID[4],
                    g_ucConnectionBSSID[5]);
        }
        memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
        memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
    }
        break;

    case SL_WLAN_STA_CONNECTED_EVENT: {

        // when device is in AP mode and any client connects to device cc3xxx
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION_FAILED);

        //
        // Information about the connected client (like SSID, MAC etc) will
        // be available in 'slPeerInfoAsyncResponse_t' - Applications
        // can use it if required
        //
         slPeerInfoAsyncResponse_t *pEventData = NULL;
         pEventData = &pSlWlanEvent->EventData.APModeStaConnected;

         LOG("[WLAN EVENT: %lu] A Client has connected to the Device.\n\r",
                                        SL_WLAN_STA_CONNECTED_EVENT);


    }
        break;

    case SL_WLAN_STA_DISCONNECTED_EVENT: {
        // when client disconnects from device (AP)
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION);
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

        //
        // Information about the connected client (like SSID, MAC etc) will
        // be available in 'slPeerInfoAsyncResponse_t' - Applications
        // can use it if required
        //
         slPeerInfoAsyncResponse_t *pEventData = NULL;
         pEventData = &pSlWlanEvent->EventData.APModestaDisconnected;
         LOG("[WLAN EVENT: %lu] A Client has disconnected from the Device.\n\r",
                                       SL_WLAN_STA_CONNECTED_EVENT);

    }
        break;

    case SL_WLAN_SMART_CONFIG_COMPLETE_EVENT: {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_SMARTCONFIG_START);

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

    case SL_WLAN_SMART_CONFIG_STOP_EVENT: {
        // SmartConfig operation finished
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_SMARTCONFIG_START);

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

    case SL_WLAN_P2P_DEV_FOUND_EVENT: {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_P2P_DEV_FOUND);

        //
        // Information about P2P config details (like Peer device name, own
        // SSID etc) will be available in 'slPeerInfoAsyncResponse_t' -
        // Applications can use it if required
        //
        // slPeerInfoAsyncResponse_t *pEventData = NULL;
        // pEventData = &pSlWlanEvent->EventData.P2PModeDevFound;
        //
    }
        break;

    case SL_WLAN_P2P_NEG_REQ_RECEIVED_EVENT: {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_P2P_REQ_RECEIVED);

        //
        // Information about P2P Negotiation req details (like Peer device
        // name, own SSID etc) will be available in 'slPeerInfoAsyncResponse_t'
        //  - Applications can use it if required
        //
        // slPeerInfoAsyncResponse_t *pEventData = NULL;
        // pEventData = &pSlWlanEvent->EventData.P2PModeNegReqReceived;
        //
    }
        break;

    case SL_WLAN_CONNECTION_FAILED_EVENT: {
        // If device gets any connection failed event
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_CONNECTION_FAILED);
        LOG("[WLAN EVENT - %lu] Connection failed.\n\r",SL_WLAN_CONNECTION_FAILED_EVENT);

    }
        break;

    default: {
        LOG("[WLAN EVENT] Unexpected event. \n\r");
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
void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent) {
    if (pNetAppEvent == NULL) {
        UART_PRINT("Null pointer\n\r");
        LOOP_FOREVER()
        ;
    }

    //UART_PRINT("A SimpleLinkNetAppEvent is happening...[%lu]\n\r",(unsigned long) pNetAppEvent->Event);


    switch (pNetAppEvent->Event) {
    case SL_NETAPP_IPV4_IPACQUIRED_EVENT: {
        SlIpV4AcquiredAsync_t *pEventData = NULL;

        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_AQUIRED);

        //Ip Acquired Event Data
        pEventData = &pNetAppEvent->EventData.ipAcquiredV4;

        LOG("[NETAPP EVENT: %lu] IP Acquired.\n\r",SL_NETAPP_IPV4_IPACQUIRED_EVENT);

        sprintf(IP_str,"%d.%d.%d.%d",
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 3),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 2),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 1),
                SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.ip, 0));

        LOG("                  IP:      %s\n\r",IP_str);

        LOG("                  Gateway: %d.%d.%d.%d\n\r",
                   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 3),
                   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 2),
                   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 1),
                   SL_IPV4_BYTE(pNetAppEvent->EventData.ipAcquiredV4.gateway, 0));

        UNUSED(pEventData);

    }
        break;

    case SL_NETAPP_IP_LEASED_EVENT: {
        SET_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

        //
        // Information about the IP-Leased details(like IP-Leased,lease-time,
        // mac etc) will be available in 'SlIpLeasedAsync_t' - Applications
        // can use it if required
        //
         SlIpLeasedAsync_t *pEventData = NULL;
         pEventData = &pNetAppEvent->EventData.ipLeased;

         LOG("[NETAPP EVENT: %lu] IP Leased.\n\r",SL_NETAPP_IP_LEASED_EVENT);
         LOG("                    Client IP:  %d.%d.%d.%d\n\r",
                SL_IPV4_BYTE(pEventData->ip_address, 3),
                SL_IPV4_BYTE(pEventData->ip_address, 2),
                SL_IPV4_BYTE(pEventData->ip_address, 1),
                SL_IPV4_BYTE(pEventData->ip_address, 0));
         LOG("                    Client MAC: %x:%x:%x:%x:%x:%x\n\r",
                                       pEventData->mac[0], pEventData->mac[1],
                                       pEventData->mac[2], pEventData->mac[3],
                                       pEventData->mac[4], pEventData->mac[5]);

    }
        break;

    case SL_NETAPP_IP_RELEASED_EVENT: {
        CLR_STATUS_BIT(g_ulStatus, STATUS_BIT_IP_LEASED);

        //
        // Information about the IP-Released details (like IP-address, mac
        // etc) will be available in 'SlIpReleasedAsync_t' - Applications
        // can use it if required
        //
         SlIpReleasedAsync_t *pEventData = NULL;
         pEventData = &pNetAppEvent->EventData.ipReleased;

         LOG("[NETAPP EVENT: %lu] IP Released.\n\r",SL_NETAPP_IP_LEASED_EVENT);
         LOG("                    Client IP:  %d.%d.%d.%d\n\r",
                SL_IPV4_BYTE(pEventData->ip_address, 3),
                SL_IPV4_BYTE(pEventData->ip_address, 2),
                SL_IPV4_BYTE(pEventData->ip_address, 1),
                SL_IPV4_BYTE(pEventData->ip_address, 0));
         LOG("                    Client MAC: %x:%x:%x:%x:%x:%x\n\r",
                                       pEventData->mac[0], pEventData->mac[1],
                                       pEventData->mac[2], pEventData->mac[3],
                                       pEventData->mac[4], pEventData->mac[5]);
    }
        break;

    default: {
        LOG("[NETAPP EVENT] Unexpected event [0x%x] \n\r",
                pNetAppEvent->Event);
    }
        break;
    }
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
void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent) {

    if (pDevEvent == NULL) {
        return;
    }

    //
    // Most of the general errors are not FATAL are are to be handled
    // appropriately by the application
    //
    LOG("[GENERAL EVENT] - ID = [%d] Sender = [%d]\n\n\r",
            pDevEvent->EventData.deviceEvent.status,
            pDevEvent->EventData.deviceEvent.sender);

    // General event errors trump other Wlan errors
    setLastConnectionError(pDevEvent->EventData.deviceEvent.status);

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
void SimpleLinkSockEventHandler(SlSockEvent_t *pSock) {
    if (pSock == NULL) {
        return;
    }

    LOG("SimpleLinkSockEvent is happening... [%lu]\n\r",pSock->Event);

    switch (pSock->Event) {
    case SL_SOCKET_TX_FAILED_EVENT:
        switch (pSock->socketAsyncEvent.SockTxFailData.status) {
        case SL_ECLOSE:
            LOG("[SOCK ERROR] - close socket (%d) operation "
                    "failed to transmit all queued packets\n\n",
                    pSock->socketAsyncEvent.SockTxFailData.sd);
            break;
        default:
            LOG("[SOCK ERROR] - TX FAILED  :  socket %d , reason "
                    "(%d) \n\n", pSock->socketAsyncEvent.SockTxFailData.sd,
                    pSock->socketAsyncEvent.SockTxFailData.status);
            break;
        }
        break;

    case SL_SOCKET_ASYNC_EVENT:

        switch (pSock->socketAsyncEvent.SockAsyncData.type) {
        case SSL_ACCEPT:/*accept failed due to ssl issue ( tcp pass)*/
            LOG("[SOCK ERROR] - close socket (%d) operation"
                    "accept failed due to ssl issue\n\r",
                    pSock->socketAsyncEvent.SockAsyncData.sd);
            break;
        case RX_FRAGMENTATION_TOO_BIG:
            LOG("[SOCK ERROR] -close scoket (%d) operation"
                    "connection less mode, rx packet fragmentation\n\r"
                    "> 16K, packet is being released",
                    pSock->socketAsyncEvent.SockAsyncData.sd);
            break;
        case OTHER_SIDE_CLOSE_SSL_DATA_NOT_ENCRYPTED:
            LOG("[SOCK ERROR] -close socket (%d) operation"
                    "remote side down from secure to unsecure\n\r",
                    pSock->socketAsyncEvent.SockAsyncData.sd);
            break;
        default:
            LOG("unknown sock async event: %d\n\r",
                    pSock->socketAsyncEvent.SockAsyncData.type);
        }
        break;
    default:
        LOG("[SOCK EVENT] - Unexpected Event [%x0x]\n\n", pSock->Event);
        break;
    }
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
    unsigned char *ptr;
    int i,j;
    static unsigned long timer = 0;
    static unsigned char timer_active = 0;

    if(timer_active && (TIME_GetPosix(TIME_EITHER) - timer) > 60){
        timer_active = 0;
    }

    if(!timer_active){
        LOG("HTTP %s [ %s ] \n\r",(pSlHttpServerEvent->Event==1)?"GET":"POST",pSlHttpServerEvent->EventData.httpTokenName.data);
    }

    switch (pSlHttpServerEvent->Event) {
    case SL_NETAPP_HTTPGETTOKENVALUE_EVENT: {

        ptr = pSlHttpServerResponse->ResponseData.token_value.data;
        pSlHttpServerResponse->ResponseData.token_value.len = 0;

/********************************************************************************
 *
 * [ GET ] NET Status and Last STATION Connection Error
 *
 *******************************************************************************/
        if (memcmp(pSlHttpServerEvent->EventData.httpTokenName.data,
                GET_token_NET, strlen((const char *) GET_token_NET)) == 0)
        {
            int sTempLen = sprintf(ptr,"%i:%i", !Net_Status(), getLastConnectionError());

            // Set the packet length;
            pSlHttpServerResponse->ResponseData.token_value.len = sTempLen;
        }

/********************************************************************************
 *
 * [ GET ] JSON formatted list of available network (Token 1)
 *
 *******************************************************************************/
        if (memcmp(pSlHttpServerEvent->EventData.httpTokenName.data,
            GET_token_SSID, strlen((const char *) GET_token_SSID)) == 0)
        {

            // This should be the first token called, so reset the indices
            g_ulSSIDGETIndices = 0;

            ptr += sprintf(ptr,"{\"SSID\":[");

            // Load SSIDs into response body
            for(i=0;i<ScanResults.num_entries;i++){

                // Check if duplicate
                if(ScanResults.netEntries[i].ssid_len == 0){
                    continue;
                }

                // Check if adding this SSID will bring us over the 64 byte limit
                //  6 is for: ,"<ssid>"]}\0
                if((6 + ptr + ScanResults.netEntries[i].ssid_len - pSlHttpServerResponse->ResponseData.token_value.data)>MAX_TOKEN_VALUE_LEN){
                    continue;
                }

                // Mark the index as used
                g_ulSSIDGETIndices |= (1 << i);

                // Add the first part of the response if it's the first iteration
                if(i==0){
                    ptr+= sprintf(ptr,"\"%s\"",ScanResults.netEntries[i].ssid);
                }

                else{
                    ptr+= sprintf(ptr,",\"%s\"",ScanResults.netEntries[i].ssid);
                }
            }

            // Add the ending of the response
            strcat(ptr,"]}");

            // Set the packet length
            pSlHttpServerResponse->ResponseData.token_value.len = strlen(pSlHttpServerResponse->ResponseData.token_value.data);
        }
    }

/********************************************************************************
 *
 * [ GET ] JSON formatted list of available network (Token 2)
 *
 *******************************************************************************/
        if (memcmp(pSlHttpServerEvent->EventData.httpTokenName.data,
                   GET_token_SSID2, strlen((const char *) GET_token_SSID2)) == 0)
        {

            int cntr = 0;

            ptr += sprintf(ptr,"{\"SSID\":[");

            // Load SSIDs into response body
            for(i=0;i<ScanResults.num_entries;i++){

                // Check if duplicate
                if(ScanResults.netEntries[i].ssid_len == 0){
                    continue;
                }

                // Check if this SSID was already used in GET_token_SSID
                if(g_ulSSIDGETIndices & (1 << i)){
                    continue;
                }

                // Check if adding this SSID will bring us over the 64 byte limit
                //  6 is for: ,"<ssid>"]}\0
                if((6 + ptr + ScanResults.netEntries[i].ssid_len - pSlHttpServerResponse->ResponseData.token_value.data)>MAX_TOKEN_VALUE_LEN){
                    continue;
                }

                // Add the first part of the response if it's the first iteration
                if(cntr==0){
                    ptr += sprintf(ptr,"\"%s\"",ScanResults.netEntries[i].ssid);
                }

                else{
                    ptr+= sprintf(ptr,",\"%s\"",ScanResults.netEntries[i].ssid);
                }
                cntr++;
            }

            // Add the ending of the response
            strcat(ptr,"]}");

            // Set the packet length
            pSlHttpServerResponse->ResponseData.token_value.len = strlen(pSlHttpServerResponse->ResponseData.token_value.data);

            // Clear the array once it's been used
            g_ulSSIDGETIndices = 0;
        }

        if(!timer_active)
        {
            LOG("\tHTTP SERVER CALLBACK: %s\n\n\r", pSlHttpServerResponse->ResponseData.token_value.data);
        }
        break;

    case SL_NETAPP_HTTPPOSTTOKENVALUE_EVENT: {

        unsigned char *ptr =
                pSlHttpServerEvent->EventData.httpPostData.token_name.data;

        LOG("HTTP Callback: \n\r[ %s ]\n\r",ptr);

        if (memcmp(ptr, POST_token, strlen((const char *) POST_token)) == 0)
        {
            ptr = pSlHttpServerEvent->EventData.httpPostData.token_value.data;
            char dlm[] = " ";
            char* tok;

            // Connect button was clicked
            if(NULL != strstr((const char*)ptr, (const char*)"update"))
            {
                // "update <SSID>"
                tok = strtok(ptr,dlm); // tok is "update"
                if(tok==NULL){
                    return;
                }

                tok = strtok(NULL,dlm); // tok is "<SSID>"
                if(tok==NULL){
                    UART_PRINT("[ CALLBACK ] Bad SSID\n\r");
                    setLastConnectionError(-2); // -2 is bad SSID
                    return;
                }
                strcpy(lastProfileInfo.last_added_profile,tok);
                lastProfileInfo.profile_good = FALSE;
                UART_PRINT("[ CALLBACK ] Profile added: [ %s ]\n\r",lastProfileInfo.last_added_profile);
                setNetworkProfileRequestFlag(1);
                setUSERConnectPressedFlag(1);

                UART_PRINT("[ CALLBACK ] Profile Check Flag: [ %d ]\n\r",getNetworkProfileRequestFlag());
            }
        }

        else if (memcmp(ptr, POST_token_CON, strlen((const char *) POST_token_CON)) == 0) {
            ptr = pSlHttpServerEvent->EventData.httpPostData.token_value.data;
            UART_PRINT("POST_token_CON: %s\n\r",ptr);
        }
    }
        break;
    default:
        break;
    }

    if(!timer_active)
    {
        timer_active = 1;
        timer = TIME_GetPosix(TIME_EITHER);
    }
}

//*****************************************************************************
// SimpleLink Asynchronous Event Handlers -- End
//*****************************************************************************

//****************************************************************************
//
//!    \brief This function initializes the application variables
//!
//!    \param[in]  None
//!
//!    \return     0 on success, negative error-code on error
//
//****************************************************************************
void Wlan_InitializeAppVariables()
{
    g_ulStatus = 0;
    g_ulStaIp = 0;
    g_ulGatewayIP = 0;
//    g_uiDeviceModeConfig = ROLE_STA;
    g_iInternetAccess = -1;
    memset(g_ucConnectionSSID, 0, sizeof(g_ucConnectionSSID));
    memset(g_ucConnectionBSSID, 0, sizeof(g_ucConnectionBSSID));
}

//*****************************************************************************
//! \brief This function puts the device in its default state. It:
//!           - Set the mode to STATION
//!           - Configures connection policy to Auto and AutoSmartConfig
//!           - Deletes all the stored profiles
//!           - Enables DHCP
//!           - Disables Scan policy
//!           - Sets Tx power to maximum
//!           - Sets power policy to normal
//!           - Unregister mDNS services
//!           - Remove all filters
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//*****************************************************************************
static long ConfigureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lRetVal = -1;
    long lMode = -1;

    lMode = sl_SStart();

    // If the device is not in station-mode, try configuring it in station-mode
    if (ROLE_STA != lMode)
    {
        if (ROLE_AP == lMode)
        {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while(!IS_IP_ACQUIRED(g_ulStatus));
        }

        // Switch to STA role and restart
        lRetVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(lRetVal);

        sl_Restart();

        // Check if the device is in station again
        if (ROLE_STA != lRetVal)
        {
            // We don't want to proceed if the device is not coming up in STA-mode
            return DEVICE_NOT_IN_STATION_MODE;
        }
    }

//    // Get the device's version-information
//    ucConfigOpt = SL_DEVICE_GENERAL_VERSION;
//    ucConfigLen = sizeof(ver);
//    lRetVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &ucConfigOpt,
//                                &ucConfigLen, (unsigned char *)(&ver));
//    ASSERT_ON_ERROR(lRetVal);
//
//    UART_PRINT("Host Driver Version: %s\n\r",SL_DRIVER_VERSION);
//    UART_PRINT("Build Version %d.%d.%d.%d.31.%d.%d.%d.%d.%d.%d.%d.%d\n\r",
//    ver.NwpVersion[0],ver.NwpVersion[1],ver.NwpVersion[2],ver.NwpVersion[3],
//    ver.ChipFwAndPhyVersion.FwVersion[0],ver.ChipFwAndPhyVersion.FwVersion[1],
//    ver.ChipFwAndPhyVersion.FwVersion[2],ver.ChipFwAndPhyVersion.FwVersion[3],
//    ver.ChipFwAndPhyVersion.PhyVersion[0],ver.ChipFwAndPhyVersion.PhyVersion[1],
//    ver.ChipFwAndPhyVersion.PhyVersion[2],ver.ChipFwAndPhyVersion.PhyVersion[3]);

    // Set connection policy to Auto + SmartConfig
    //      (Device's default connection policy)
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,
                                SL_CONNECTION_POLICY(0, 0, 0, 0, 0), NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    //
    // Device in station-mode. Disconnect previous connection if any
    // The function returns 0 if 'Disconnected done', negative number if already
    // disconnected Wait for 'disconnection' event if 0 is returned, Ignore
    // other return-codes
    //
    lRetVal = sl_WlanDisconnect();
    if(0 == lRetVal)
    {
        // Wait
        while(IS_CONNECTED(g_ulStatus));
    }

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to normal
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    lRetVal = sl_SStop();

    return lRetVal; // Success
}

void _printNetEntries()
{
    int i;
    for(i=0;i<ScanResults.num_entries;i++){
        UART_PRINT("RSSI: %i\tSSID Length: %i\tSSID: %s\n\r",ScanResults.netEntries[i].rssi,ScanResults.netEntries[i].ssid_len,ScanResults.netEntries[i].ssid);
    }
}

int _cmp_rssi(const void *p, const void *q)
{
    Sl_WlanNetworkEntry_t *a = (Sl_WlanNetworkEntry_t *)p;
    Sl_WlanNetworkEntry_t *b = (Sl_WlanNetworkEntry_t *)q;

    return ( b->rssi - a->rssi );
}

void _sortByRSSI()
{
    qsort((void*)ScanResults.netEntries, ScanResults.num_entries, sizeof(ScanResults.netEntries[0]), _cmp_rssi);
}

void _removeDuplicates()
{
    // Remove duplicates (keep best RSSI)
    _u8 tmp1,tmp2;
    for(tmp1=0;tmp1<ScanResults.num_entries;tmp1++){
        if(ScanResults.netEntries[tmp1].ssid[0] != NULL){
            for(tmp2=0;tmp2<ScanResults.num_entries;tmp2++){
                if(tmp2!=tmp1 && ScanResults.netEntries[tmp2].ssid[0] != NULL){
                    if(strcmp(ScanResults.netEntries[tmp2].ssid,ScanResults.netEntries[tmp1].ssid) == 0){
                        if(ScanResults.netEntries[tmp2].rssi>ScanResults.netEntries[tmp1].rssi){
                            ScanResults.netEntries[tmp1].rssi = -128;
                            ScanResults.netEntries[tmp1].ssid_len = 0;
                        }else{
                            ScanResults.netEntries[tmp2].rssi = -128;
                            ScanResults.netEntries[tmp2].ssid_len = 0;
                        }
                    }
                }
            }
        }
    }
}
//*****************************************************************************
//
//! the aim of this example code is to demonstrate how scan policy is set in the
//! device.
//! The procedure includes the following steps:
//! 1) make sure the connection policy is not set (so no scan is run in the
//!    background)
//! 2) enable scan, set scan cycle to 10 seconds and set scan policy
//! This starts the scan
//! 3) get scan results - all 20 entries in one transaction
//! 4) get scan results - 4 transactions of 5 entries
//! 5) disable scan
//!
//! \param  None
//!
//! \return 0 on success else error code
//!  Also, LED1 is turned solid in case of success
//!  LED8 is turned solid in case of failure
//!
//*****************************************************************************
static long Wlan_Scan(SlWlanMode_e mode)
{

    long lRetVal = -1;

    unsigned short ucIndex;
    unsigned char ucpolicyOpt;
    union
    {
        unsigned char ucPolicy[4];
        unsigned int uiPolicyLen;
    }policyVal;

    //
    // Following function configure the device to default state by cleaning
    // the persistent settings stored in NVMEM (viz. connection profiles &
    // policies, power policy etc)
    //
    // Applications may choose to skip this step if the developer is sure
    // that the device is in its default state at start of applicaton
    //
    // Note that all profiles and persistent settings that were done on the
    // device will be lost
    //
    lRetVal = ConfigureSimpleLinkToDefaultState();
    if(lRetVal < 0)
    {
        if (DEVICE_NOT_IN_STATION_MODE == lRetVal)
        {
            UART_PRINT("Failed to configure the device in its default state\n\r");
        }
        return lRetVal;
    }

    UART_PRINT("Device is configured in default state \n\r");

    //
    // Assumption is that the device is configured in station mode already
    // and it is in its default state
    //
    lRetVal = sl_Start(0, 0, 0); ERROR_LOOP(lRetVal);
    setSLStatus(1);

    if (ROLE_STA != lRetVal)
    {
        UART_PRINT("SCAN: Failed to start the device \n\r");
        return lRetVal;
    }

//    UART_PRINT("Device started as STATION \n\r");

    //
    // make sure the connection policy is not set (so no scan is run in the
    // background)
    //
    ucpolicyOpt = SL_CONNECTION_POLICY(0, 0, 0, 0, 0);
    ERROR_LOOP(sl_WlanPolicySet(SL_POLICY_CONNECTION , ucpolicyOpt, NULL, 0));

    //
    // enable scan
    //
    ucpolicyOpt = SL_SCAN_POLICY(1);
    //
    // set scan cycle to 1 seconds
    //
    policyVal.uiPolicyLen = 1;
    //
    // set scan policy - this starts the scan
    //
    ERROR_LOOP(sl_WlanPolicySet(SL_POLICY_SCAN , ucpolicyOpt,
                               (unsigned char*)(policyVal.ucPolicy), sizeof(policyVal)));

    osi_Sleep(1000);

    //
    // get scan results - all 20 entries in one transaction
    //
    // retVal indicates the valid number of entries
    // The scan results are occupied in netEntries[]
    //
    lRetVal = sl_WlanGetNetworkList(0, (unsigned char)20,
                                    &(ScanResults.netEntries[0]));
    ScanResults.num_entries = (unsigned char) lRetVal;
    if(lRetVal==0)
    {
        UART_PRINT("Unable to retreive the network list\n\r");
        return lRetVal;
    }

    // Remove duplicates first (set rssi = -128 so they get sorted to bottom)
    _removeDuplicates();
    _sortByRSSI();
    _printNetEntries();

    //
    // disable scan
    //
    ucpolicyOpt = SL_SCAN_POLICY(0);
    ERROR_LOOP(sl_WlanPolicySet(SL_POLICY_SCAN , ucpolicyOpt, NULL, 0));

    // Set SimpleLink back to AP Mode if necessary
    if(mode != ROLE_STA){
        lRetVal = Wlan_ConfigureMode(mode);
    }

    return lRetVal;
}

//****************************************************************************
//
//! \brief Connecting to a WLAN Accesspoint
//!
//!  This function connects to the required AP (SSID_NAME) with Security
//!  parameters specified in te form of macros at the top of this file
//!
//! \param  None
//!
//! \return  None
//!
//! \warning    If the WLAN connection fails or we don't aquire an IP
//!            address, It will be stuck in this function forever.
//
//****************************************************************************
static long Wlan_ManualConnect()
{
    SlSecParams_t secParams = {0};
    long lRetVal = 0;
    _u16 cnt;

    const char* KEY = "clean air";
    secParams.Key = (signed char*)KEY;
    secParams.KeyLen = strlen(KEY);
    secParams.Type = SL_SEC_TYPE_WPA_WPA2;

    g_ulStatus = 0;
    errorFlag = 0;
    lRetVal = sl_WlanConnect((signed char*)"airu", strlen("airu"), NULL, &secParams, NULL);
    UART_PRINT("Wlan connect: %d\n\r",lRetVal);
    ASSERT_ON_ERROR(lRetVal);

    // Wait for WLAN Event
    cnt = 0;
    lRetVal = SUCCESS;
    while(IS_CONNECTED(g_ulStatus)==0 && IS_IP_ACQUIRED(g_ulStatus)==0){

        if(errorFlag){
            lRetVal = -109;
            UART_PRINT("Invalid Password\n\r");
            break;
        }
        // 5 seconds to acquire IP
        if(cnt++ == 50){
            lRetVal = FAILURE;
            break;
        }

        // 1 second to lock on to SSID
        if(cnt == 30 && !IS_CONNECTED(g_ulStatus)){
            lRetVal = FAILURE;
            break;
        }

        osi_Sleep(100);
        UART_PRINT("%i.\t0x%08x \t [ %d ]\n\r",cnt,g_ulStatus,IS_CONNECTED(g_ulStatus));
    }

    UART_PRINT("cnt: %d\n\r",cnt);
    return lRetVal;

}

static long Wlan_AutoConnect()
{
    SlSecParams_t secParams = {0};
    long lRetVal = 0;
    _u16 cnt;
    _u8 ucConfigOpt;

    const char* KEY = "clean air";
    secParams.Key = (signed char*)KEY;
    secParams.KeyLen = strlen(KEY);
    secParams.Type = SL_SEC_TYPE_WPA_WPA2;

    g_ulStatus = 0;
    errorFlag = 0;
    // pName, NameLen, pMacAddr, pSecParams, pSecExtParams, Priority, Options
    lRetVal = sl_WlanProfileAdd((signed char*)"airu", strlen("airu"), NULL, &secParams, NULL, 1, NULL);
    ucConfigOpt = SL_CONNECTION_POLICY(1,1,0,0,0);
    sl_WlanPolicySet(SL_POLICY_CONNECTION,ucConfigOpt,NULL, 1);
    UART_PRINT("Wlan Profile Add: %d\n\r",lRetVal);
    ASSERT_ON_ERROR(lRetVal);

    // Wait for WLAN Event
    cnt = 0;
    lRetVal = SUCCESS;
    while(IS_CONNECTED(g_ulStatus)==0 && IS_IP_ACQUIRED(g_ulStatus)==0){

        if(errorFlag){
            lRetVal = -109;
            UART_PRINT("Invalid Password\n\r");
            break;
        }
        // 5 seconds to acquire IP
        if(cnt++ == 50){
            lRetVal = FAILURE;
            break;
        }

        // lock on to SSID
        if(cnt == 30 && !IS_CONNECTED(g_ulStatus)){
            lRetVal = FAILURE;
            break;
        }

        osi_Sleep(100);
        UART_PRINT("%i.\t0x%08x \t [ %d ]\n\r",cnt,g_ulStatus,IS_CONNECTED(g_ulStatus));
    }

    UART_PRINT("cnt: %d\n\r",cnt);
    return lRetVal;

}
long Wlan_QuickConnect()
{
    long lRetVal;

    ERROR_LOOP(ConfigureSimpleLinkToDefaultState());
    lRetVal = sl_SStart();

    UART_PRINT("Trying AutoConnect...\n\r");
    lRetVal = Wlan_AutoConnect();
//
//    UART_PRINT("Trying manual connect...\n\r");
//    lRetVal = Wlan_ManualConnect();
//    UART_PRINT("\tReturned %lu\n\r",lRetVal);
//
    if(lRetVal != SUCCESS){

        UART_PRINT("Reconnecting in AP MODE\n\r");
        lRetVal = Wlan_Connect(ROLE_AP,0,1);
        UART_PRINT("\tReturned %lu\n\r",lRetVal);

    }
    return lRetVal;
}

static long netShit()
{
    long lRetVal;
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};
    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucPower = 0;

    //Stop Internal HTTP Server
    lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR(lRetVal);

    //Start Internal HTTP Server
    lRetVal = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
    ASSERT_ON_ERROR(lRetVal);

    // Enable DHCP client
    lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
    ASSERT_ON_ERROR(lRetVal);

    // Disable scan
    ucConfigOpt = SL_SCAN_POLICY(0);
    lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Set PM policy to always on
    lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_ALWAYS_ON_POLICY, NULL, 0);
    ASSERT_ON_ERROR(lRetVal);

    // Enable connection policies: enable auto connect, fast connect, smart config
    lRetVal = sl_WlanPolicySet(SL_POLICY_CONNECTION,SL_CONNECTION_POLICY(1,1,0,0,0),NULL,0);
    ASSERT_ON_ERROR(lRetVal);

    // Set Tx power level for station mode
    // Number between 0-15, as dB offset from max power - 0 will set max power
    ucPower = 0;
    lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
            WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 0, (unsigned char *)&ucPower);
    ASSERT_ON_ERROR(lRetVal);

    // Remove  all 64 filters (8*8)
    memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(lRetVal);

    return lRetVal;
}

//****************************************************************************
//
//!    \brief Connects to the Network in AP or STA Mode
//!
//! \return  0 - Success (Device connected to an AP and has internet connection)
//!         -1 - Failure
//!          2 - AP Mode
//
//****************************************************************************
long Wlan_Connect(SlWlanMode_e mode, _u8 scan, _u8 full_start)
{
    unsigned int uiConnectTimeoutCnt = 0;
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};
    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucPower = 0;
    long lRetVal = -1;

    g_iInternetAccess = FAILURE;

//    lRetVal = ConfigureSimpleLinkToDefaultState();

    // Fast connect connected us already, wait it out
    if(IS_CONNECTED(g_ulStatus) && mode == ROLE_STA && !full_start){

        uiConnectTimeoutCnt = 0;
        while(uiConnectTimeoutCnt++<100 && !IS_IP_ACQUIRED(g_ulStatus)){
            osi_Sleep(10);
        }

        if(IS_IP_ACQUIRED(g_ulStatus)) {
            lRetVal = netShit();
            ASSERT_ON_ERROR(lRetVal);
            goto PING_TEST;
        }
    }

/*************************************************************************************
 * Scan the surrounding available 2.4GHz networks
*************************************************************************************/
    if(mode == ROLE_AP && scan){
        lRetVal = Wlan_Scan(mode);
    }

/*************************************************************************************
 * Configure Device in appropriate Mode
*************************************************************************************/
    lRetVal = Wlan_ConfigureMode(mode); ERROR_LOOP(lRetVal);

    // Not in STA MODE but want STA MODE
    if (lRetVal != ROLE_STA && mode == ROLE_STA){
        if (lRetVal == ROLE_AP){
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while (!IS_IP_ACQUIRED(g_ulStatus));
        }
        lRetVal = Wlan_ConfigureMode(mode); ERROR_LOOP(lRetVal);
    }

    // In AP MODE and want AP MODE
    if (lRetVal == ROLE_AP && mode == ROLE_AP)
    {
        //waiting for the AP to acquire IP address from Internal DHCP Server
        // If the device is in AP mode, we need to wait for this event
        // before doing anything
        while (!IS_IP_ACQUIRED(g_ulStatus));

        //Restart Internal HTTP Server
        ERROR_LOOP(sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID));
        ERROR_LOOP(sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID));

        return ROLE_AP;
    }

/*************************************************************************************
 * Device In STATION Mode --> Set Parameters
*************************************************************************************/
    else{

        UART_PRINT("Trying to connect as STATION\n\r");
        //Restart Internal HTTP Server
        ERROR_LOOP(sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID));
        ERROR_LOOP(sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID));

        // Enable DHCP client
        ERROR_LOOP(sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal));

        // Disable scan
        ucConfigOpt = SL_SCAN_POLICY(0);
        ERROR_LOOP(sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0));

        // Set PM policy to always on
        ERROR_LOOP(sl_WlanPolicySet(SL_POLICY_PM , SL_ALWAYS_ON_POLICY, NULL, 0));

        // Enable connection policies: enable auto connect, fast connect
        ucConfigOpt = SL_CONNECTION_POLICY(1,1,0,0,0);
        ERROR_LOOP(sl_WlanPolicySet(SL_POLICY_CONNECTION,ucConfigOpt,NULL,0));

        // Set Tx power level for station mode to max
        ucPower = 0;
        ERROR_LOOP(sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 0, (unsigned char *)&ucPower));

        // Remove  all 64 filters (8*8)
        memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
        ERROR_LOOP(sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                           sizeof(_WlanRxFilterOperationCommandBuff_t)));

/*************************************************************************************
 * Let SimpleLink Autoconnect the Device to an AP
*************************************************************************************/
        //waiting for the device to Auto Connect
        while (uiConnectTimeoutCnt++ < AUTO_CONNECTION_TIMEOUT_COUNT
                && ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus)))){
            osi_Sleep(10);
        }


/*************************************************************************************
 * Couldn't Connect To An AP Using Auto Profile
*************************************************************************************/
        if (!(IS_CONNECTED(g_ulStatus) & IS_IP_ACQUIRED(g_ulStatus))) {
            CLR_STATUS_BIT_ALL(g_ulStatus);
            return FAILURE;
        }

/*************************************************************************************
 * Connected to an AP --> Do a Ping Test
*************************************************************************************/
        else{
PING_TEST:
            g_iInternetAccess = ConnectionTest();
            UART_PRINT("   Internet Status: [%s]",(g_iInternetAccess==SUCCESS ? "GOOD" : "NOT CONNECTED"));
        }
    }

    return g_iInternetAccess;
}

//****************************************************************************
//
//! Confgiures the mode in which the device will work
//!
//! \param iMode is the current mode of the device
//!
//!
//! \return   SlWlanMode_t
//!
//
//****************************************************************************
int Wlan_ConfigureMode(int iMode) {
    long lRetVal = -1;

    lRetVal = sl_WlanSetMode(iMode);

    ASSERT_ON_ERROR(lRetVal);

    // Restart Network processor
    lRetVal = sl_Stop(SL_STOP_TIMEOUT);

    // Reset status bits
    CLR_STATUS_BIT_ALL(g_ulStatus);

    return sl_Start(NULL, NULL, NULL);
}

/**
 *  \brief: Do we have internet access? IS_IP_ACQUIRED is True if we are successfully
 *              in AP mode or STA mode and connected to a router (it has assigned the
 *              board an IP address) IS_CONNECTED is just if it's connected to a router.
 *              g_iInternetAccess gets set negative if a ping test fails. g_ulStatus is
 *              set by the SimpleLink API.
 *
 *  \return: Boolean True for internet connection, False otherwise
 */
char Net_Status()
{
    return IS_IP_ACQUIRED(g_ulStatus) && IS_CONNECTED(g_ulStatus) && (g_iInternetAccess == SUCCESS);
}

long Wlan_ConnectTest()
{
    long lRetVal;

    // Connect in STATION mode
    ERROR_LOOP(lRetVal = Wlan_ConfigureMode(ROLE_STA));

    // Device is in AP Mode
    if (ROLE_STA != lRetVal){
        while ((lRetVal == ROLE_AP) && !IS_IP_ACQUIRED(g_ulStatus));
        ERROR_LOOP(lRetVal = Wlan_ConfigureMode(ROLE_STA));
    }

    // Set PM policy to normal
    ERROR_LOOP(lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0));

    return lRetVal;
}

//****************************************************************************
//
//!    \brief Connects to the Network in AP or STA Mode
//!
//! \return  0 - Success (Device connected to an AP and has internet connection)
//!         -1 - Failure
//!          2 - AP Mode
//
//****************************************************************************
long Wlan_ConnectToNetwork(SlWlanMode_e mode) {

    unsigned int uiConnectTimeoutCnt = 0;
    char mac[20], ID[20];
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    unsigned char ucVal = 1;
    unsigned char ucConfigOpt = 0;
    unsigned char ucConfigLen = 0;
    unsigned char ucPower = 0;

    long lMode = -1;
    long lRetVal = -1;


/*************************************************************************************
 * Configure Device in STATION Mode
*************************************************************************************/
        UART_PRINT("Configure to station...\n\r");
        lRetVal = Wlan_ConfigureMode(ROLE_STA);
        ERROR_LOOP(lRetVal);

    // Device is in AP Mode
    if (ROLE_STA != lRetVal)
    {
        UART_PRINT("Wasn't station...\n\r");
        if (ROLE_AP == lRetVal)
        {
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while (!IS_IP_ACQUIRED(g_ulStatus)) {}
        }
        //Switch to STA Mode
        UART_PRINT("Configure station...\n\r");
        lRetVal = Wlan_ConfigureMode(ROLE_STA);
        ERROR_LOOP(lRetVal);
    }

    //No Mode Change Required (This will only get called if we're actively trying to set AP Mode)
    if (lRetVal == ROLE_AP)
    {
        //waiting for the AP to acquire IP address from Internal DHCP Server
        // If the device is in AP mode, we need to wait for this event
        // before doing anything
        while (!IS_IP_ACQUIRED(g_ulStatus)) {}

        //Stop Internal HTTP Server
        lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
        ERROR_LOOP(lRetVal);

        //Start Internal HTTP Server
        lRetVal = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
        ERROR_LOOP(lRetVal);
    }

/*************************************************************************************
 * Device In STATION Mode --> Set Parameters
*************************************************************************************/
    else // sl_Start() returned STA [0]
    {

        //Stop Internal HTTP Server
        lRetVal = sl_NetAppStop(SL_NET_APP_HTTP_SERVER_ID);
        ERROR_LOOP(lRetVal);

        //Start Internal HTTP Server
        lRetVal = sl_NetAppStart(SL_NET_APP_HTTP_SERVER_ID);
        ERROR_LOOP(lRetVal);

        // Enable DHCP client
        lRetVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&ucVal);
        ERROR_LOOP(lRetVal);

        // Disable scan
        ucConfigOpt = SL_SCAN_POLICY(0);
        lRetVal = sl_WlanPolicySet(SL_POLICY_SCAN , ucConfigOpt, NULL, 0);
        ERROR_LOOP(lRetVal);

        // Set Tx power level for station mode
        // Number between 0-15, as dB offset from max power - 0 will set max power
        ucPower = 0;
        lRetVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID,
                WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (unsigned char *)&ucPower);
        ERROR_LOOP(lRetVal);

        // Set PM policy to normal
        lRetVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
        ERROR_LOOP(lRetVal);

        // Remove  all 64 filters (8*8)
        memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
        lRetVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                           sizeof(_WlanRxFilterOperationCommandBuff_t));
        ERROR_LOOP(lRetVal);

/*************************************************************************************
 * Let SimpleLink Autoconnect the Device to an AP
*************************************************************************************/
        //waiting for the device to Auto Connect
        while (uiConnectTimeoutCnt < AUTO_CONNECTION_TIMEOUT_COUNT
                && ((!IS_CONNECTED(g_ulStatus)) || (!IS_IP_ACQUIRED(g_ulStatus))))
        {
            osi_Sleep(100);
            uiConnectTimeoutCnt++;
        }


/*************************************************************************************
 * Couldn't Connect To An AP Using Auto Profile --> Put in AP Mode
*************************************************************************************/
        if (uiConnectTimeoutCnt == AUTO_CONNECTION_TIMEOUT_COUNT) {

AP_CONNECT:
            CLR_STATUS_BIT_ALL(g_ulStatus);

            // Put the station into AP mode
            lRetVal = Wlan_ConfigureMode(ROLE_AP);
            ERROR_LOOP(lRetVal);

            // Waiting for the AP to acquire IP address from Internal DHCP Server
            // If the device is in AP mode, we need to wait for this event
            // before doing anything
            while (!IS_IP_ACQUIRED(g_ulStatus)){}

            UART_PRINT("Device Status: [AP Mode].\n\r");

            return lRetVal;

        }

/*************************************************************************************
 * Connected to an AP --> Do a Ping Test
*************************************************************************************/
        else
        {
            g_iInternetAccess = ConnectionTest();
            UART_PRINT("Device Status: [STATION Mode].\n\r");
            UART_PRINT("   Internet Status: [%s]",(g_iInternetAccess==SUCCESS ? "GOOD" : "NOT CONNECTED"));
            if(g_iInternetAccess < SUCCESS)
            {
                UART_PRINT("   Internet Status: [NOT CONNECTED]\n\r");
            }
            else
            {
                UART_PRINT("   Internet Status: [GOOD]\n\r");
            }

        }

    }

    return g_iInternetAccess;
}

// Wrapper for ConnectionTest because I don't want to move the entire 'device_status.c" file
void Net_ConnectionTest()
{
    g_iInternetAccess = ConnectionTest();
    return;
}

void Net_RSTInternetAccess()
{
    g_iInternetAccess = FAILURE;
    return;
}
//*****************************************************************************
//
//! Disconnect  Disconnects from an Access Point
//!
//! \param  none
//!
//! \return 0 disconnected done, other already disconnected
//
//*****************************************************************************
long Wlan_Disconnect() {
    long lRetVal = 0;

    if (IS_CONNECTED(g_ulStatus)) {
        lRetVal = sl_WlanDisconnect();
        if (0 == lRetVal) {
            // Wait
            while (IS_CONNECTED(g_ulStatus)) {
#ifndef SL_PLATFORM_MULTI_THREADED
                _SlNonOsMainLoopTask();
#else
                osi_Sleep(1);
#endif
            }
            return lRetVal;
        } else {
            return lRetVal;
        }
    } else {
        return lRetVal;
    }

}


//*****************************************************************************
//
//! Network_IF_IpConfigGet  Get the IP Address of the device.
//!
//! \param  pulIP IP Address of Device
//! \param  pulSubnetMask Subnetmask of Device
//! \param  pulDefaultGateway Default Gateway value
//! \param  pulDNSServer DNS Server
//!
//! \return On success, zero is returned. On error, -1 is returned
//
//*****************************************************************************
long Net_IpConfigGet(unsigned long *pulIP, unsigned long *pulSubnetMask,
        unsigned long *pulDefaultGateway, unsigned long *pulDNSServer) {
    unsigned char isDhcp;
    unsigned char len = sizeof(SlNetCfgIpV4Args_t);
    long lRetVal = -1;
    SlNetCfgIpV4Args_t ipV4 = { 0 };

    lRetVal = sl_NetCfgGet(SL_IPV4_STA_P2P_CL_GET_INFO, &isDhcp, &len,
            (unsigned char *) &ipV4);
    ASSERT_ON_ERROR(lRetVal);

    *pulIP = ipV4.ipV4;
    *pulSubnetMask = ipV4.ipV4Mask;
    *pulDefaultGateway = ipV4.ipV4Gateway;
    *pulDefaultGateway = ipV4.ipV4DnsServer;

    return lRetVal;
}

//*****************************************************************************
//
//! Network_IF_GetHostIP
//!
//! \brief  This function obtains the server IP address using a DNS lookup
//!
//! \param[in]  pcHostName        The server hostname
//! \param[out] pDestinationIP    This parameter is filled with host IP address.
//!
//! \return On success, +ve value is returned. On error, -ve value is returned
//!
//
//*****************************************************************************
long Net_GetHostIP(char* pcHostName, unsigned long * pDestinationIP) {
    long lStatus = 0;

    lStatus = sl_NetAppDnsGetHostByName((signed char *) pcHostName,
            strlen(pcHostName), pDestinationIP, SL_AF_INET);
    ASSERT_ON_ERROR(lStatus);

    UART_PRINT("Get Host IP succeeded.\n\rHost: %s IP: %d.%d.%d.%d \n\r\n\r",
            pcHostName, SL_IPV4_BYTE(*pDestinationIP, 3),
            SL_IPV4_BYTE(*pDestinationIP, 2), SL_IPV4_BYTE(*pDestinationIP, 1),
            SL_IPV4_BYTE(*pDestinationIP, 0));
    return lStatus;

}

long HTTP_GETDownloadFile(HTTPCli_Handle httpClient, char *src, char *dst, char* dns_ip) {
    long lRetVal = 0;
    long fileHandle = -1;
    unsigned int bytesReceived = 0;
    unsigned long Token = 0;
    int id;
    int len = 0;
    bool moreFlag = 0;
    char *ptr;
    unsigned long fileSize = 0;

    HTTPCli_Field fields[3] = { { HTTPCli_FIELD_NAME_HOST, (const char*) dns_ip }, {
    HTTPCli_FIELD_NAME_ACCEPT, "text/html, application/xhtml+xml, */*" }, {
    NULL, NULL } };
    const char *ids[4] = {
    HTTPCli_FIELD_NAME_CONTENT_LENGTH,
    HTTPCli_FIELD_NAME_TRANSFER_ENCODING,
    HTTPCli_FIELD_NAME_CONNECTION,
    NULL };

    UART_PRINT("Start downloading the file...\n\r");

    /* Set request header fields to be send for HTTP request. */
    HTTPCli_setRequestFields(httpClient, fields);

    memset(g_httpResponseBuff, 0, sizeof(g_httpResponseBuff));

    // Make HTTP 1.1 GET request
    UART_PRINT("Source: %s\n\r", src);
    UART_PRINT("Dest:   %s\n\r", dst);
    lRetVal = HTTPCli_sendRequest(httpClient, HTTPCli_METHOD_GET, src, 0);
    if (lRetVal < 0) {
        DBG_PRINT("Couldn't make GET request\n\r\t");
        ASSERT_ON_ERROR(lRetVal);
    }

    // Get response status
    lRetVal = HTTPCli_getResponseStatus(httpClient);
    if (lRetVal != 200) {
        HTTP_FlushHTTPResponse(httpClient);
        UART_PRINT("[ERROR] on http client response [%i]\n\r\t",lRetVal);
        return lRetVal;
    }

    // Read Response Headers
    HTTPCli_setResponseFields(httpClient, ids);
    while ((id = HTTPCli_getResponseField(httpClient,
            (char*) g_httpResponseBuff, sizeof(g_httpResponseBuff), &moreFlag))
            != HTTPCli_FIELD_ID_END) {

        if (id == 0) {
            UART_PRINT("Content length: %s\n\r", g_httpResponseBuff);
            fileSize = strtoul((const char*) g_httpResponseBuff, &ptr, 10);
        } else if (id == 1) {
            if (!strncmp((const char*) g_httpResponseBuff, "chunked",
                    sizeof("chunked"))) {
                UART_PRINT("Chunked transfer encoding\n\r");
            }
        } else if (id == 2) {
            if (!strncmp((const char*) g_httpResponseBuff, "close",
                    sizeof("close"))) {
                return lRetVal;
            }
        }
    }

    // Remove the old file ( so it can be created with appropriate fileSize )
    //  Delete before opening
    lRetVal = sl_FsDel((_u8 *) dst, 0);
    UART_PRINT("File Deleted. lRetVal = %d\n\r",lRetVal);

    UART_PRINT("Creating new file with filesize: %lu\n\n\r",fileSize);
    // File Doesn't exist create a new one with size fileSize
    lRetVal = sl_FsOpen((_u8 *) dst,
            FS_MODE_OPEN_CREATE(fileSize, /* COMMIT creates failsafe (duplicate) */
                    /*_FS_FILE_OPEN_FLAG_COMMIT |*/ _FS_FILE_PUBLIC_WRITE),
            &Token, &fileHandle);
    ASSERT_ON_ERROR(lRetVal);

//    // Open file to save the downloaded file
//    lRetVal = sl_FsOpen((_u8 *) dst, FS_MODE_OPEN_WRITE, &Token, &fileHandle);
//    if (lRetVal < 0) {
//
//        // File Doesn't exist create a new one with size fileSize
//        lRetVal = sl_FsOpen((unsigned char *) dst,
//                FS_MODE_OPEN_CREATE(fileSize,
//                        /*_FS_FILE_OPEN_FLAG_COMMIT |*/ _FS_FILE_PUBLIC_WRITE),
//                &Token, &fileHandle);
//        ASSERT_ON_ERROR(lRetVal);
//
//    }

    while (1) {
        len = HTTPCli_readResponseBody(httpClient, (char *) g_httpResponseBuff,
                sizeof(g_httpResponseBuff) - 1, &moreFlag);
        if (len < 0) {
            // Close file without saving
            lRetVal = sl_FsClose(fileHandle, 0, (unsigned char*) "A", 1);
            return lRetVal;
        }

        lRetVal = sl_FsWrite(fileHandle, bytesReceived,
                (unsigned char *) g_httpResponseBuff, len);

        if (lRetVal < len) {
            UART_PRINT(
                    "Failed during writing the file, Error-code: %d\r\nlen: %d\r\n",
                    lRetVal, len);
            // Close file without saving
            lRetVal = sl_FsClose(fileHandle, 0, (unsigned char*) "A", 1);
            return lRetVal;
        }
        bytesReceived += len;

        if ((len - 2) >= 0 && g_httpResponseBuff[len - 2] == '\r'
                && g_httpResponseBuff[len - 1] == '\n') {
            break;
        }

        if (!moreFlag) {
            break;
        }
    }

    //
    // If user file has checksum which can be used to verify the temporary
    // file then file should be verified
    // In case of invalid file (FILE_NAME) should be closed without saving to
    // recover the previous version of file
    //

    // Save and close file
    UART_PRINT("Total bytes received: %d\n\r", bytesReceived);

    if(bytesReceived != fileSize){
        lRetVal = sl_FsClose(fileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(lRetVal);
        UART_PRINT("fileSize [ %lu ] and bytesReceived [ %lu ] not equal. Returning FAILURE\n\n\r",fileSize,bytesReceived);
        return FAILURE;
    }

    lRetVal = sl_FsClose(fileHandle, 0, 0, 0);
    ASSERT_ON_ERROR(lRetVal);

    return SUCCESS;
}

//*****************************************************************************
//
//! \brief Flush response body.
//!
//! \param[in]  httpClient - Pointer to HTTP Client instance
//!
//! \return 0 on success else error code on failure
//!
//*****************************************************************************
long HTTP_FlushHTTPResponse(HTTPCli_Handle httpClient) {
    const char *ids[2] = {
    HTTPCli_FIELD_NAME_CONNECTION, /* App will get connection header value. all others will skip by lib */
    NULL };
    char buf[128];
    int id;
    int len = 1;
    bool moreFlag = 0;
    char ** prevRespFilelds = NULL;

    /* Store previosly store array if any */
    prevRespFilelds = HTTPCli_setResponseFields(httpClient, ids);

    /* Read response headers */
    while ((id = HTTPCli_getResponseField(httpClient, buf, sizeof(buf),
            &moreFlag)) != HTTPCli_FIELD_ID_END) {

        if (id == 0) {
            if (!strncmp(buf, "close", sizeof("close"))) {
                UART_PRINT("Connection terminated by server\n\r");
            }
        }

    }

    /* Restore previosuly store array if any */
    HTTPCli_setResponseFields(httpClient, (const char **) prevRespFilelds);

    while (1) {
        /* Read response data/body */
        /* Note:
         moreFlag will be set to 1 by HTTPCli_readResponseBody() call, if more
         data is available Or in other words content length > length of buffer.
         The remaining data will be read in subsequent call to HTTPCli_readResponseBody().
         Please refer HTTP Client Libary API documenation @ref HTTPCli_readResponseBody
         for more information.
         */
        HTTPCli_readResponseBody(httpClient, buf, sizeof(buf) - 1, &moreFlag);
        ASSERT_ON_ERROR(len);

        if ((len - 2) >= 0 && buf[len - 2] == '\r' && buf[len - 1] == '\n') {
            break;
        }

        if (!moreFlag) {
            /* There no more data. break the loop. */
            break;
        }
    }
    return 0;
}

//*****************************************************************************
//
//! \brief HTTP POST Demonstration
//!
//! \param[in]  httpClient - Pointer to http client
//!
//! \return 0 on success else error code on failure
//!
//*****************************************************************************
long HTTP_PostMethod(HTTPCli_Handle httpClient, influxDBDataPoint data) {
    bool moreFlags = 1;
    bool lastFlag = 1;
    char ID[20];
    char tmpBuf[4];
    long lRetVal = 0;
    HTTPCli_Field fields[4] =
            { { HTTPCli_FIELD_NAME_HOST, INFLUXDB_DNS_NAME }, {
            HTTPCli_FIELD_NAME_ACCEPT, "*/*" },
                    {
                    HTTPCli_FIELD_NAME_CONTENT_TYPE,
                            "application/x-www-form-urlencoded" },
                    { NULL, NULL } };

    cpyUniqueID(ID);

    /* Set request header fields to be send for HTTP request. */
    HTTPCli_setRequestFields(httpClient, fields);

    /* Send POST method request. */
    /* Here we are setting moreFlags = 1 as there are some more header fields need to send
     other than setted in previous call HTTPCli_setRequestFields() at later stage.
     Please refer HTTP Library API documentation @ref HTTPCli_sendRequest for more information.
     */
    moreFlags = 1;
    lRetVal = HTTPCli_sendRequest(httpClient, HTTPCli_METHOD_POST,
    POST_REQUEST_URI, moreFlags);
    if (lRetVal < 0) {
        UART_PRINT("\t* Failed to send HTTP POST request header.\n\r");
        return lRetVal;
    }

    // Construct HTTP POST Body
    unsigned char postString[250];
    sprintf((char*) postString, POST_DATA_AIR, ID, HW_VER, FW_VER,                \
                                               data.altitude, data.latitude, data.longitude,\
                                               data.pm1, data.pm2_5, data.pm10,             \
                                               data.temperature, data.humidity);


    UART_PRINT("\tPOST DATA: \n\n\r%s\n\n\r", postString); //- debugging

    sprintf((char *) tmpBuf, "%d", (strlen((char*) postString)));

    /* Here we are setting lastFlag = 1 as it is last header field.
     Please refer HTTP Library API documentaion @ref HTTPCli_sendField for more information.
     */
    lastFlag = 1;
    lRetVal = HTTPCli_sendField(httpClient, HTTPCli_FIELD_NAME_CONTENT_LENGTH,
            (const char *) tmpBuf, lastFlag);
    if (lRetVal < 0) {
        UART_PRINT("Failed to send HTTP POST request header.\n\r");
        return lRetVal;
    }

    /* Send POST data/body */
    lRetVal = HTTPCli_sendRequestBody(httpClient, (char*) postString,
            (strlen((char*) postString))); // changed! // maybe change char* to const char*
    // UART_PRINT("sendRequestBody Return Value: %d \n\r",lRetVal); - Debugging
    if (lRetVal < 0) {
        UART_PRINT("Failed to send HTTP POST request body.\n\r");
        return lRetVal;
    }

    lRetVal = HTTP_ReadResponse(httpClient);

    return lRetVal;
}

/*!
 \brief This function read respose from server and dump on console
 \param[in]      httpClient - HTTP Client object
 \return         0 on success else -ve
 \note
 \warning
 */
long HTTP_ReadResponse(HTTPCli_Handle httpClient) {
    long lRetVal = 0;
    int bytesRead = 0;
    int id = 0;
    unsigned long len = 0;
    int json = 0;
    char *dataBuffer = NULL;
    bool moreFlags = 1;
    const char *ids[4] = {
    HTTPCli_FIELD_NAME_CONTENT_LENGTH,
    HTTPCli_FIELD_NAME_CONNECTION,
    HTTPCli_FIELD_NAME_CONTENT_TYPE,
    NULL };

    /* Read HTTP POST request status code */
    lRetVal = HTTPCli_getResponseStatus(httpClient);
    UART_PRINT("\tReturn Value: [%d] \n\r", (int) lRetVal); // Debugging - Return Value
    if (lRetVal > 0) {
        switch (lRetVal) {
        case 200: {
            //UART_PRINT("HTTP Status 200\n\r");
            /*
             Set response header fields to filter response headers. All
             other than set by this call we be skipped by library.
             */
            HTTPCli_setResponseFields(httpClient, (const char **) ids);

            /* Read filter response header and take appropriate action. */
            /* Note:
             1. id will be same as index of fileds in filter array setted
             in previous HTTPCli_setResponseFields() call.
             2. moreFlags will be set to 1 by HTTPCli_getResponseField(), if  field
             value could not be completely read. A subsequent call to
             HTTPCli_getResponseField() will read remaining field value and will
             return HTTPCli_FIELD_ID_DUMMY. Please refer HTTP Client Libary API
             documenation @ref HTTPCli_getResponseField for more information.
             */
            while ((id = HTTPCli_getResponseField(httpClient,
                    (char *) g_httpResponseBuff, sizeof(g_httpResponseBuff),
                    &moreFlags)) != HTTPCli_FIELD_ID_END) {

                switch (id) {
                case 0: /* HTTPCli_FIELD_NAME_CONTENT_LENGTH */
                {
                    len = strtoul((char *) g_httpResponseBuff, NULL, 0);
                }
                    break;
                case 1: /* HTTPCli_FIELD_NAME_CONNECTION */
                {
                }
                    break;
                case 2: /* HTTPCli_FIELD_NAME_CONTENT_TYPE */
                {
                    if (!strncmp((const char *) g_httpResponseBuff,
                            "application/json", sizeof("application/json"))) {
                        json = 1;
                    } else {
                        /* Note:
                         Developers are advised to use appropriate
                         content handler. In this example all content
                         type other than json are treated as plain text.
                         */
                        json = 0;
                    }
                    UART_PRINT(HTTPCli_FIELD_NAME_CONTENT_TYPE);
                    UART_PRINT(" : ");
                    UART_PRINT("application/json\n\r");
                }
                    break;
                default: {
                    UART_PRINT("Wrong filter id\n\r");
                    lRetVal = -1;
                    goto end;
                }
                }
            }
            bytesRead = 0;
            if (len > sizeof(g_httpResponseBuff)) {
                dataBuffer = (char *) malloc(len);
                if (dataBuffer) {
                    UART_PRINT("Failed to allocate memory\n\r");
                    lRetVal = -1;
                    goto end;
                }
            } else {
                dataBuffer = (char *) g_httpResponseBuff;
            }

            /* Read response data/body */
            /* Note:
             moreFlag will be set to 1 by HTTPCli_readResponseBody() call, if more
             data is available Or in other words content length > length of buffer.
             The remaining data will be read in subsequent call to HTTPCli_readResponseBody().
             Please refer HTTP Client Libary API documenation @ref HTTPCli_readResponseBody
             for more information
             */
            bytesRead = HTTPCli_readResponseBody(httpClient,
                    (char *) dataBuffer, len, &moreFlags);
            if (bytesRead < 0) {
                UART_PRINT("Failed to received response body\n\r");
                lRetVal = bytesRead;
                goto end;
            } else if (bytesRead < len || moreFlags) {
                UART_PRINT(
                        "Mismatch in content length and received data length\n\r");
                goto end;
            }
            dataBuffer[bytesRead] = '\0';

            if (json) {
                /* Parse JSON data */
                lRetVal = JSMN_ParseJSONData(dataBuffer);
                if (lRetVal < 0) {
                    goto end;
                }
            } else {
                /* treating data as a plain text */
            }

        }
            break;

        case 404:
            UART_PRINT("File not found. \r\n");
            /* Handle response body as per requirement.
             Note:
             Developers are advised to take appopriate action for HTTP
             return status code else flush the response body.
             In this example we are flushing response body in default
             case for all other than 200 HTTP Status code.
             */
        default:
            /* Note:
             Need to flush received buffer explicitly as library will not do
             for next request.Apllication is responsible for reading all the
             data.
             */
            HTTP_FlushHTTPResponse(httpClient);
            break;
        }
    } else {
        UART_PRINT("Failed to receive data from server.\r\n");
        goto end;
    }

    lRetVal = 0;

    end: if (len > sizeof(g_httpResponseBuff) && (dataBuffer != NULL)) {
        free(dataBuffer);
    }
    return lRetVal;
}

//*****************************************************************************
//
//! \brief Handler for parsing JSON data
//!
//! \param[in]  ptr - Pointer to http response body data
//!
//! \return 0 on success else error code on failure
//!
//*****************************************************************************
int JSMN_ParseJSONData(char *ptr) {
    long lRetVal = 0;
    int noOfToken;
    jsmn_parser parser;
    jsmntok_t *tokenList;

    /* Initialize JSON PArser */
    jsmn_init(&parser);

    /* Get number of JSON token in stream as we we dont know how many tokens need to pass */
    noOfToken = jsmn_parse(&parser, (const char *) ptr,
            strlen((const char *) ptr), NULL, 10);
    if (noOfToken <= 0) {
        UART_PRINT("Failed to initialize JSON parser\n\r");
        return -1;

    }

    /* Allocate memory to store token */
    tokenList = (jsmntok_t *) malloc(noOfToken * sizeof(jsmntok_t));
    if (tokenList == NULL) {
        UART_PRINT("Failed to allocate memory\n\r");
        return -1;
    }

    /* Initialize JSON Parser again */
    jsmn_init(&parser);
    noOfToken = jsmn_parse(&parser, (const char *) ptr,
            strlen((const char *) ptr), tokenList, noOfToken);
    if (noOfToken < 0) {
        UART_PRINT("Failed to parse JSON tokens\n\r");
        lRetVal = noOfToken;
    } else {
        UART_PRINT("Successfully parsed %ld JSON tokens\n\r", noOfToken);
    }

    free(tokenList);

    return lRetVal;
}


void RebootMCU_RestartNWP()
{
    LOG("Stopping NWP...\n\r");
    sl_Stop(100);

    RebootMCU();
}

//****************************************************************************
//
//! Reboot the MCU by requesting hibernate for a short duration
//!
//! \return None
//
//****************************************************************************
void RebootMCU() {

    LOG("Rebooting...\n\r");

//    // A dirt-nasty-low reboot
//    PRCMMCUReset(1);

    // Hibernate Preamble
    //
    HWREG(0x400F70B8) = 1;
    UtilsDelay(800000/5);
    HWREG(0x400F70B0) = 1;
    UtilsDelay(800000/5);
    HWREG(0x4402E16C) |= 0x2;
    UtilsDelay(800);
    HWREG(0x4402F024) &= 0xF7FFFFFF;

    //
    // Call Hibernate
    //
    MAP_PRCMOCRRegisterWrite(0,1);
    MAP_PRCMHibernateWakeupSourceEnable(PRCM_HIB_SLOW_CLK_CTR);
    MAP_PRCMHibernateIntervalSet(330);
    MAP_PRCMHibernateEnter();

}
