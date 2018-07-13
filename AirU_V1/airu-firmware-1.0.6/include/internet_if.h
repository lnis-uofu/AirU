//*****************************************************************************
// internet_if.h
//
//*****************************************************************************

#ifndef __INTERNET_IF__H__
#define __INTERNET_IF__H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>
#include <stdlib.h>

// Simplelink includes
#include "simplelink.h"
#include "simplelink_if.h"

// driverlib includes
#include "hw_types.h"
#include "timer.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "osi.h"
#include "device_status.h"
#include "flc.h"
#include "fs.h"

// common interface includes
#ifndef NOTERM
#include "uart_if.h"
#endif
#include "gpio_if.h"
#include "timer_if.h"
#include "common.h"

// application specific includes
#include "app_utils.h"
#include "influxdb.h"
#include "smartconfig.h"

// HTTP Client lib
#include "http/client/httpcli.h"
#include "http/client/common.h"

// JSON Parser
#include "jsmn.h"

/*
 * Uncomment for factory (faster upload, different subscribes/publishes) and PRISMS versions (different MQTT password and broker)
 */
//#define FACTORY
//#define PRISMS

#define CHECK_PROFILE_BIT       0
#define CONNECT_PRESSED_BIT     1

// macros
#define FW_VER "1.0.6"
#define HW_VER "1.1"

#define TASKSTATUS_GET(status_variable,bit) (0 != (status_variable & ~(1<<(bit))))
#define TASKSTATUS_SET(status_variable,bit) (status_variable |= (1<<(bit)))
#define TASKSTATUS_CLR(status_variable,bit) (status_variable &= ~(1<<(bit)))

#define AUTO_CONNECTION_TIMEOUT_COUNT   500      /* 5 Sec */
#define SL_STOP_TIMEOUT                 200
#define SH_GPIO_3                       3       /* P58 - Device Mode */
#define READ_SIZE                       1450
#define MAX_BUFF_SIZE                   1460

enum States
{
    NetAppConnect = (1<<0),
    MQTTHandle    = (1<<1),
    MQTTPub       = (1<<2),
    uSDStore      = (1<<3),
    NTPGetTime    = (1<<4)
};

//*****************************************************************************
//
// API Function prototypes
//
//*****************************************************************************
extern unsigned char getNetworkProfileRequestFlag(void);
extern void setNetworkProfileRequestFlag(unsigned char ucValue);
extern void setUSERConnectPressedFlag(unsigned char ucValue);
extern unsigned char getUSERConnectPressedFlag(void);
extern void getLastAddedProfile(char* ssid);
extern unsigned char getSLStatus(void);
extern void setLastProfileGoodFlag(unsigned char ucValue);
extern unsigned char getLastProfileGoodFlag(void);
extern unsigned char* getIP(void);
extern void setSLStatus(unsigned char status);
extern long sl_SStart(void);
extern long sl_SStop(void);
extern long sl_Restart(void);
extern int getLastConnectionError(void);
extern void setLastConnectionError(int err);
extern unsigned long getStatus(void);
extern void SimpleLinkWlanEventHandler(SlWlanEvent_t *pSlWlanEvent);
extern void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent);
extern void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent);
extern void SimpleLinkSockEventHandler(SlSockEvent_t *pSock);
extern void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pSlHttpServerEvent,
                                         SlHttpServerResponse_t *pSlHttpServerResponse);

extern long Wlan_Connect(SlWlanMode_e, unsigned char scan, unsigned char full_start);
extern long Wlan_ConnectTest();
extern long Wlan_Disconnect(void);
extern long Wlan_ConnectToNetwork(SlWlanMode_e mode);
extern int  Wlan_ConfigureMode(int iMode);
extern void Wlan_InitializeAppVariables();
extern void Wlan_ReadDeviceConfiguration(void);
extern long Wlan_QuickConnect(void);

extern char Net_Status(void);
extern void Net_ConnectionTest(void);
extern void Net_RSTInternetAccess(void);
extern long Net_GetHostIP( char* pcHostName,unsigned long * pDestinationIP);
extern long Net_IpConfigGet(unsigned long *aucIP, unsigned long *aucSubnetMask, unsigned long *aucDefaultGateway, unsigned long *aucDNSServer);

extern long HTTP_ReadResponse(HTTPCli_Handle httpClient);
extern long HTTP_FlushHTTPResponse(HTTPCli_Handle httpClient);
extern long HTTP_PostMethod(HTTPCli_Handle httpClient, influxDBDataPoint data);
extern long HTTP_GETDownloadFile(HTTPCli_Handle httpClient, char *src, char *dst, char *dns_ip);

extern int  JSMN_ParseJSONData(char *ptr);

extern void RebootMCU();
extern void RebootMCU_RestartNWP();
//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif
