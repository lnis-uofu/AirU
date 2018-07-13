/*******************************************************************
 * Copyright (c) 2018 University of Utah
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************/
// Standard includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Simplelink includes
#include "simplelink.h"
#include "netcfg.h"

// Driverlib includes
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "utils.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
#include "pin.h"
#include "timer.h"
#include "hw_common_reg.h"
#include "hw_nvic.h"
#include "hw_wdt.h"
#include "wdt.h"

// OS includes
#include "osi.h"
#include "flc_api.h"
#include "ota_api.h"
#include "freertos.h"
#include "task.h"
 #include "FreeRTOSConfig.h"

// Common interface includes
#include "common.h"
#include "gpio_if.h"
#include "wdt_if.h"
#include "uart_if.h"
#include "i2c_if.h"
#include "internet_if.h"
#include "timer_if.h"
#include "server_if.h"
#include "simplelink_if.h"
#include "sd_if.h"

// App Includes
#include "hooks.h"
#include "handlers.h"
#include "sdhost.h"
#include "ff.h"
#include "HDC1080.h"
#include "OPT3001.h"
#include "ADS1015.h"
#include "pms_if.h"
#include "gps_if.h"
#include "app_utils.h"
#include "MQTTPacket.h"
#include "MQTTCC3200.h"
#include "MQTTClient.h"

//#include "handlers.h"
#include "device_status.h"
#include "smartconfig.h"
#include "pinmux.h"

// HTTP Client lib
#include "http/client/httpcli.h"
#include "http/client/common.h"

//#include "bloat.h"

#define CONNECTED 1

#define WD_PERIOD_MS 1000

                            /* min | s  |  ms  */
#define NETWORK_CONFIG_SLEEP         10 * 1000
#define PUB_DATA_SLEEP           1 * 60 * 1000
#define DATA_GATHER_SLEEP            60 * 1000
#define SD_WRITE_SLEEP           1 * 60 * 1000
#define SYNC_TIME_SLEEP         10 * 60 * 1000

#define NUM_SAMPLES     5

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
HTTPCli_Struct InfluxDBhttpClient;
HTTPCli_Struct OTAhttpClient;
influxDBDataPoint dataPoint,testPoint;
char g_cUniqueID[6*2+1];
uint32_t last_data_post_time = 0;
#ifdef FACTORY
char MQTT_INFLUX_TOPIC[MAX_TPC_LENGTH] = "airu/factory"; // boards publish to "factory" before being changed via OTA
#else
char MQTT_INFLUX_TOPIC[MAX_TPC_LENGTH] = "airu/influx"; // boards publish to "influx" in production variant
#endif

/************************************************************
 * MQTT globals
 ************************************************************/
char payload[MAX_PLD_LENGTH];
char dbg_payload[MAX_PLD_LENGTH];
static MQTTMessage MQTTInfluxDataPoint;
static MQTTMessage MQTTDebug;
static MQTTMessage MQTTHelpMsg;
unsigned char g_cpInfluxDataString[250] = {0};
char g_pDatetime[50];

volatile unsigned char g_ucMICSPreheatFlag;
unsigned char g_ucTimeSynced = 0;

Network n;

/************************************************************
 * OSI Semaphores
 ************************************************************/
static OsiSyncObj_t g_WLANReconnectRequest;
static OsiSyncObj_t g_WLANConnectedMQTT;
static OsiSyncObj_t g_WLANConnectedNTP;
static OsiSyncObj_t g_WLANConnectedPublish;
static OsiSyncObj_t g_MQTTDebugPub;
static OsiSyncObj_t g_MICSPreheat;
static OsiSyncObj_t g_FileDNLObj;
static OsiLockObj_t g_NetworkLockRequest;
#ifdef USE_SSL
static OsiLockObj_t g_TimeUpdate;
#endif
/************************************************************
 * main.c flags
 ************************************************************/
volatile unsigned long g_ulErrorCodes = 0;
volatile unsigned long g_ulTaskStatus = 0;
volatile unsigned char g_ucRequestPingTest = 0;
volatile unsigned char g_ucConnectionReset = 0;
volatile long g_lResetCause = -1;
volatile unsigned char g_ucMode = 0;    // 0 = unconnected | 1 = STA | 2 = AP
volatile unsigned char flag;
volatile unsigned char g_NetworkCheckProfiles;

/************************************************************
 * Data Gather variables
 ************************************************************/
static float        g_fTemp[NUM_SAMPLES]   = {0};
static float        g_fHumi[NUM_SAMPLES]   = {0};
static unsigned int g_uiPM01[NUM_SAMPLES]  = {0};
static unsigned int g_uiPM2_5[NUM_SAMPLES] = {0};
static unsigned int g_uiPM10[NUM_SAMPLES]  = {0};

static char g_cTimestamp[9];
static char g_cDatestampFN[11];          // file name date stamp

typedef enum {
    LED_OFF = 0,
    LED_ON,
    LED_BLINK
} eLEDStatus;

typedef enum {
   ONE_SEC = 1000,
   ONE_MIN = 60*ONE_SEC,
   ONE_HR  = 60*ONE_MIN
} eTime;

static eLEDStatus g_ucInternetLEDStatus; // STAT 3
static eLEDStatus g_ucLEDStatus;         // STAT 2
static eLEDStatus g_ucMQTTLEDStatus;     // STAT 1

unsigned char bInitialConnectionFinished = 0;

static OtaOptServerInfo_t g_otaOptServerInfo;
static void *pvOtaApp;

typedef struct
{
    char src[50];
    char dst[50];
    tBoolean data_valid;
}sFileDownload;
sFileDownload g_sFileDownload;

// Global flags //
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//#ifdef USE_FREERTOS
////*****************************************************************************
////
////! Application defined hook (or callback) function - the tick hook.
////! The tick interrupt can optionally call this
////!
////! \param  none
////!
////! \return none
////!
////*****************************************************************************
void
vApplicationTickHook( void )
{
    // MQTT library needs a millisecond tick too :)
//    milliInterrupt();
}
//#endif

//*****************************************************************************
//
//! Routine to feed to watchdog
//!
//! \param  none
//!
//! \return none
//
//*****************************************************************************
static void WatchdogAck()
{
  //
  // Acknowledge watchdog by clearing interrupt
  //
  WatchdogIntClear(WDT_BASE);
}

float wonky(int adc, float pm)
{
    return pm * ((float)adc/4096);
}

void vSyncTime_NTP(void *pvParameters)
{
    const unsigned long WAIT_SUCCESS = 10*ONE_MIN;
    const unsigned long WAIT_FAILURE = 1*ONE_MIN;
    const unsigned char NUM_RETRIES = 3;

    long lRetVal;
    static unsigned long WAIT_TIME;
    static unsigned char retry_counter = 0;

    osi_SyncObjWait(&g_WLANConnectedNTP,OSI_WAIT_FOREVER);

    while(1)
    {
        lRetVal = 0;
        LOG("vSyncTime_NTP");

        osi_LockObjLock(&g_NetworkLockRequest, OSI_WAIT_FOREVER);

        lRetVal = TIME_SyncNTP();

        osi_LockObjUnlock(&g_NetworkLockRequest);

        if (lRetVal == SUCCESS){
            g_ucTimeSynced = 1;
            osi_SyncObjSignal(&g_TimeUpdate);

            LOG("TIME - NTP Sync Successful");

            WAIT_TIME = WAIT_SUCCESS;
            retry_counter = 0;
        }
        else{

            LOG("TIME - NTP Sync Failed");

            if(++retry_counter == NUM_RETRIES){
                WAIT_TIME = WAIT_SUCCESS;
            }
            else{
                WAIT_TIME = WAIT_FAILURE;
            }
        }

//        TIME_GetDateFromSource(date, TIME_NTP, FALSE);
//        TIME_GetTimeFromSource(time, TIME_NTP, FALSE);
//
//        UART_PRINT("\n\n\r[ NTP ] %s %s \n\r",date,time);
//        UART_PRINT("\tPOSIX:  \t[ %lu ]\n\r",TIME_GetPosix(TIME_NTP));
//        UART_PRINT("\tUptime: \t[ %lu ]\n\r",TIME_GetSecondsActive(TIME_NTP));
//        UART_PRINT("\tLast update:\t[ %lu ]\n\r",TIME_SecondsSinceUpdate(TIME_GPS));
//        UART_PRINT("\tIteration:\t[ %lu ]\n\n\r",iteration++);

        osi_Sleep(WAIT_TIME);

    }
}

//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void DisplayBanner(char * AppName) {
    UART_PRINT("\n\n\n\r");
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\t\t                %s       \n\r", AppName);
    UART_PRINT("\t\t *************************************************\n\r");
    UART_PRINT("\n\n\n\r");
}




//****************************************************************************
//
//!    \brief DataGather Application Task - Samples the sensors every 1 minute.
//! \param[in]                  pvParameters is the data passed to the Task
//!
//! \return                        None
//
//****************************************************************************
static void vDataGatherTask(void *pvParameters)
{
    long lRetVal = 0;
    static unsigned int uiSampleCount = 0;
//    static unsigned char bad_pm = 0;
    static unsigned char bad_pm_cnt = 0;
    double temperature = 0;
    double humidity = 0;
    _u16 co;
    char date[15],time[15];
//    double last_temp = 0;
//    double last_hum = 0;
//    double fGPSCoords[5] = {0};

    static sPMData s_PMData;
    static sPMTest s_PMTest;
    sGlobalPosition sGP;
    int i = 0;

    unsigned int WAIT_TIME = 56*ONE_SEC;
    // Enter the task loop
    while (1)
    {

        osi_Sleep(WAIT_TIME);

/*************************************************************************
 * PMS DATA ACQUISITION
 **************************************************************************/

        lRetVal = PMS_GetPMData(&s_PMData);

        // If we get 3 consecutive bad pm readings in a row reset the pm sensor
        if (lRetVal < 0){
            if (++bad_pm_cnt == 3){
                LOG("\n\r [ Data Gather ] - RESET PM SENSOR\n\r");
                RebootMCU();
            }
        }
        else{
            bad_pm_cnt = 0;
        }
//
        dataPoint.pm1   = s_PMData.PM1;
        dataPoint.pm2_5 = s_PMData.PM2_5;
        dataPoint.pm10  = s_PMData.PM10;

//        UART_PRINT("( %.3f, %.3f, %.3f )\n\n\r", dataPoint.pm1, dataPoint.pm2_5, dataPoint.pm10);

//        testPoint.pm1     = s_PMTest.fail_cnt;
//        testPoint.pm2_avg = s_PMTest.PM2_avg;
//        testPoint.pm10    = s_PMTest.PM2_last;
//
//        UART_PRINT("***************************************************\n\r");
//        UART_PRINT("\t Failures: %i\n\r\t"
//                     " PM2 Avg:  %.3f\n\r\t"
//                     " Last 2.5: %i\n\n\r",
//                     testPoint.pm1,testPoint.pm2_avg,testPoint.pm10);

//        UART_PRINT("\t PM 01:  %.3f\n\r\t"
//                     " PM 2.5: %.3f\n\r\t"
//                     " PM 10:  %.3f\n\n\r",
//                     dataPoint.pm1,dataPoint.pm2_5,dataPoint.pm10);


/*************************************************************************
 * TEMPERATURE & HUMIDITY DATA ACQUISITION
 **************************************************************************/

        I2C_IF_Open(I2C_MASTER_MODE_STD);

        // Get temperature
        lRetVal = GetTemperature(&temperature);
        if(SUCCESS != lRetVal)
        {
            temperature = -1000;
        }

        // Get humidity
        lRetVal = GetHumidity(&humidity);
        if(SUCCESS != lRetVal)
        {
            humidity = -1000;
        }

        I2C_IF_Close(I2C_MASTER_MODE_STD);

        dataPoint.temperature = temperature;
        dataPoint.humidity    = humidity;

/*************************************************************************
 * AMBIENT LIGHT DATA ACQUISITION | TODO: get light module working
 **************************************************************************/

/*************************************************************************
 * GAS SENSOR DATA ACQUISITION
 **************************************************************************/

        I2C_IF_Open(I2C_MASTER_MODE_STD);

        /* Don't know why I was doing this... */
//        if(!GPIO_MICS_Preheat_GET()){
//            dataPoint.co = GetReduceResult();
//            dataPoint.nox = GetOxidizeResult();
//        }
//        else
//        {
//            dataPoint.co = 0;
//            dataPoint.nox = 0;
//        }

        dataPoint.co = GetReduceResult();
        dataPoint.nox = GetOxidizeResult();

//        UART_PRINT("MICS CO:  %d\n\n\r",dataPoint.co);
//        UART_PRINT("MICS NOX: %d\n\n\r",dataPoint.nox);

        I2C_IF_Close(I2C_MASTER_MODE_STD);

/*************************************************************************
 * GPS DATA ACQUISITION
 **************************************************************************/
        GPS_GetGlobalCoords( &sGP );

        // lat & lon are hhmm.mmm

        if (sGP.latitude_degrees != 0 && sGP.longitude_degrees != 0)    // only grab if lat/lon are valid
        {                                                               //  ( 0,0 is in ocean, so don't have to
                                                                        //    worry about it being a valid point (I hope)
            // Set the altitude
            dataPoint.altitude  = sGP.altitude_meters;

            // Set the latitude
            dataPoint.latitude = GPS_DMm2Dd(sGP.latitude_degrees, sGP.latitude_minutes);

            // Set the longitude
            dataPoint.longitude = GPS_DMm2Dd(sGP.longitude_degrees, sGP.longitude_minutes);

        }

//        UART_PRINT("GPS Coordinates: (%f, %f)\n\n\r",dataPoint.latitude,dataPoint.longitude);

        TIME_GetTimeFromSource(time, TIME_GPS, TRUE);
        TIME_GetDateFromSource(date, TIME_GPS, TRUE);

//        UART_PRINT("GPS Date & Time: [ %s %s ]\n\n\r",date,time);


/*************************************************************************
 * WRITE ERROR CODE | TODO: write error code from all the sensors
 **************************************************************************/

    } // end while(1)
}


//****************************************************************************
//
//! \brief DataUpload Application Task - Uploads collected data every 90 minutes.
//! \param[in]                  pvParameters is the data passed to the Task
//!
//! \return                        None
//
//****************************************************************************
static void vSDStoreTask(void *pvParameters)
{
    char date[15];
    char time[9];
    char csv[250];
    tBoolean data_post_good = 0;

    while(1){

        osi_Sleep(60*ONE_SEC);

        LOG("SD - Writing to the SD card\n");

        memset(date,0,sizeof(date));
        TIME_GetTimeFromSource(time,TIME_EITHER,TRUE);

        // if we've previously retrieved the date from the server
        if( TIME_Exists(TIME_EITHER) )
        {
            TIME_GetDateFromSource(date,TIME_EITHER,TRUE);
        }
        else
        {
            strcpy(date,"NODATE");
        }


/*************************************************************************
 * WRITE TO FILE
 **************************************************************************/

        data_post_good = (abs(TIME_GetPosix(TIME_EITHER)-last_data_post_time) < 70);

        cpyUniqueID(g_cUniqueID);
        sprintf(csv, "%s,%s,%.3f,%.3f,%.3f,%.2f,%.2f,%.3f,%.3f,%.2f,%d,%d,%d\n",time,g_cUniqueID,dataPoint.pm1, dataPoint.pm2_5, dataPoint.pm10,   \
                                                                    dataPoint.temperature,dataPoint.humidity,                    \
                                                                    dataPoint.latitude, dataPoint.longitude,dataPoint.altitude,dataPoint.co,dataPoint.nox,data_post_good);

        UART_PRINT("SD Save:\n\r");
        UART_PRINT("%s\n\r",csv);
        UART_PRINT("Filename: %s\n\n\r",date);
        SD_DateFileAppend(date,csv);

    }   /* end while(1) */
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void) {

    // Set vector table base
#if defined(ccs) || defined(gcc)
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif

    // Enable Processor
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}


//****************************************************************************
//
//!    \brief MQTT message received callback - Called when a subscribed topic
//!                                            receives a message.
//! \param[in]                  data is the data passed to the callback
//!
//! \return                        None
//
//****************************************************************************
void messageArrived(MessageData* data) {
    static char mbuf[MAX_PLD_LENGTH];
    static char dlm[2] = " ";
#ifdef FACTORY
    char dbg_tpc[10] = "dbg";
#else
    char dbg_tpc[10] = "ack";
#endif

    char *tok, *t2;
    long lRetVal, lMQTTResponseCode;
    int profile_index,cntr;

    unsigned char bRebootFlag = 0;
    unsigned char bMQTTPubFlag = 0;

    // Clear the MQTT response message
    memset(mbuf,0,MAX_PLD_LENGTH);
    memset(dbg_payload,0,MAX_PLD_LENGTH);

    // Check for buffer overflow
    if (data->topicName->lenstring.len >= MAX_TPC_LENGTH)
    {
        UART_PRINT("Topic name too long!\n\r");
        return;
    }
    if (data->message->payloadlen >= MAX_PLD_LENGTH)
    {
        UART_PRINT("Payload too long!\n\r");

        sprintf(dbg_payload,"msg too long [%d]",data->message->payloadlen);
        goto MQTT_PUB_ACK;
    }

    UART_PRINT("\n\rMessage arrived:\n\n\r%s\n\r",data->message->payload);
    // copy the message into the local buffer
    strncpy(mbuf, data->message->payload,
        min(MAX_PLD_LENGTH, data->message->payloadlen));
    mbuf[data->message->payloadlen] = 0;


/****************************************************************
 * Begin Message Parsing
 ****************************************************************/
    tok = strtok(mbuf,dlm);

    if(tok==NULL)
    {
        sprintf(dbg_payload,"no msg");
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * OTA
 **************************************************/
    else if(0 == strcmp(tok,"ota"))
    {
        tok = strtok(NULL,dlm);

        UART_PRINT("OTA filename: %s\n\r",tok);

        // there MUST be a filename attached
        if(tok == NULL){
            UART_PRINT("NULL\n\r");
            sprintf(dbg_payload,"no filename");
            goto MQTT_PUB_ACK;
        }

        if(strstr(tok,".bin") != NULL)
        {
            OTA_SetBinFN(tok);
            lRetVal = OTA_UpdateAppBin();

            sprintf(dbg_payload,"ota %i",lRetVal);
            bMQTTPubFlag = 1;
            bRebootFlag = (lRetVal == SUCCESS);
            goto MQTT_PUB_ACK;
        }

        else
        {
            UART_PRINT("NO .bin\n\r");
            sprintf(dbg_payload,"bad filename");
            goto MQTT_PUB_ACK;
        }
    }

/***************************************************
 * IP ADDRESS
 **************************************************/
    else if(0== strcmp(tok,"ip"))
    {
        sprintf(dbg_payload,"%s", getIP());
        bMQTTPubFlag = 1;
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * MICS PREHEAT
 **************************************************/
    else if(0 == strcmp(tok,"mics"))
    {
        osi_SyncObjSignal(&g_MICSPreheat);

        sprintf(dbg_payload,"mics %i",0);
        bMQTTPubFlag = 1;
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * DOWNLOAD FILE
 **************************************************/
    else if(0 == strcmp(tok,"dnl"))
    {
        // get pointer to source file (server file)
        tok = strtok(NULL,dlm);

        // get pointer to dest file (CC3200 board)
        t2 = strtok(NULL,dlm);

        if (tok != NULL && t2 != NULL)
        {

            lRetVal = AIR_DownloadFile(tok,t2);
        }
        else
        {
            UART_PRINT("Couldn't download file... Not enough input params...\n\r");
            lRetVal = -1;
        }

        // Done in another function (with high prio)
        sprintf(dbg_payload,"dnl %s %i",tok,lRetVal);
        bMQTTPubFlag = 1;

        if(lRetVal == SUCCESS)
        {
            UART_PRINT("File downloaded...\n\r");
        }
        else
        {
            UART_PRINT("File was not downloaded...\n\r");
        }

        goto MQTT_PUB_ACK;

    }

/***************************************************
 * DELETE FILE
 **************************************************/
    else if(0 == strcmp(tok,"del"))
    {
        // get pointer to file to delete (full path)
        tok = strtok(NULL,dlm);

        lRetVal = (tok) ? sl_FsDel(tok,0) : -1 ;

        sprintf(dbg_payload,"del %s %i",tok,lRetVal);
        bMQTTPubFlag = 1;

        goto MQTT_PUB_ACK;

    }

/***************************************************
 * PING
 **************************************************/
    else if(0 == strcmp(tok,"ping"))
    {
        sprintf(dbg_payload,"pong");
        bMQTTPubFlag = 1;
    }

/***************************************************
 * REBOOT
 **************************************************/
    else if (0 == strcmp(tok,"reboot"))
    {
        tok = strtok(NULL,dlm);

        // Don't reboot if there's some crap after it
        if(tok == NULL)
        {
            bRebootFlag = 1;
            lRetVal = 0;
        }
        else
        {
            lRetVal = -1;
        }
        sprintf(dbg_payload,"reboot %d",lRetVal);
        bMQTTPubFlag = 1;
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * REMOVE PROFILE(S)
 **************************************************/
    else if (tok != NULL && 0 == strcmp(tok,"rmp"))
    {
        tok = strtok(NULL,dlm);

        // There's nothing here. They're drunk, let's just go home
        if (tok == NULL || 0 == strcmp(tok,"-r"))
        {
            memset(dbg_payload,0,MAX_PLD_LENGTH);
            sprintf(dbg_payload,"rmp -1");
            goto MQTT_PUB_ACK;
        }

        // remove all profiles
        else if (0 == strcmp(tok,"all"))
        {
            // Remove all profiles
            lRetVal = sl_WlanProfileDel(0xFF);
            UART_PRINT("Delete All: %i\n\r",lRetVal);
            lMQTTResponseCode = lRetVal;

            memset(dbg_payload,0,MAX_PLD_LENGTH);
            sprintf(dbg_payload,"rmp %d",lRetVal);
            goto MQTT_PUB_ACK;
        }

        // remove particular profile
        else
        {
            profile_index = sl_GetProfileIndex((const char*) tok);
            UART_PRINT("Profile index: %i\n\r",profile_index);

            // profile was found, delete it
            if (profile_index > -1)
            {
                lRetVal = sl_WlanProfileDel(profile_index);
                UART_PRINT("Delete One: %i\n\r",lRetVal);
                lMQTTResponseCode = lRetVal;
            }

            // If the profile the user gave isn't in the system, we'll return -2 for them
            else
            {
                lMQTTResponseCode = -2;

                // bad SSID name, don't check for -r flag, just return
                return;
            }
        }

        // check for 'reboot now' flag (-r)
        tok = strtok(NULL,dlm);

        if (tok != NULL && 0 == strcmp(tok,"-r"))
        {
            // This should be a flag that gets sent to the branch at bottom,
            // where MQTT response is delivered.
            bRebootFlag = 1;
        }

        sprintf(dbg_payload,"rmp %i", lMQTTResponseCode);
        bMQTTPubFlag = 1;
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * ACK TOPIC
 **************************************************/
    else if(0 == strcmp(tok,"ack"))
    {
        tok = strtok(NULL,dlm);

        if(tok == NULL){
            sprintf(dbg_payload,"pub %s","no topic");
            bMQTTPubFlag = 1;
            goto MQTT_PUB_ACK;
        }

        if(strlen(tok)<=30){
            strcpy(dbg_tpc,tok);

            cpyUniqueID(g_cUniqueID);
            UART_PRINT("MQTT ACK Topic:\n\r\t [ %s ]\n\n\r",dbg_tpc);

            sprintf(dbg_payload,"ack airu/%s/%s",dbg_tpc,g_cUniqueID);
        }
        else{
            strcpy(dbg_payload,"ack bad topic name");
        }
        bMQTTPubFlag = 1;
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * LED FUNCTION
 **************************************************/
    else if (0 == strcmp(tok,"led"))
    {
        tok = strtok(NULL,dlm);

        if (tok == NULL)
        {
            return;
        }

        else if (0 == strcmp(tok,"on")){
            g_ucMQTTLEDStatus = LED_ON;
        }
        else if (0 == strcmp(tok,"off")){
            g_ucMQTTLEDStatus = LED_OFF;
        }
        else if (0 == strcmp(tok,"blink")){
            g_ucMQTTLEDStatus = LED_BLINK;
        }
    }

/***************************************************
 * REMOVE FILE
 **************************************************/
    else if(0==strcmp(tok,"rm"))
    {
        sprintf(dbg_payload,"no functionality");
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * REMOVE FILE
 **************************************************/
    else
    {
        sprintf(dbg_payload,"bad cmd");
        goto MQTT_PUB_ACK;
    }

/***************************************************
 * ACKNOWLEDGEMENT
 **************************************************/
MQTT_PUB_ACK:

    if(bMQTTPubFlag){

        MQTTDebug.qos = QOS1;

        // Set topic
        cpyUniqueID(g_cUniqueID);
        sprintf(MQTTDebug.topic,"airu/%s/%s",dbg_tpc,g_cUniqueID);
        MQTTDebug.topiclen = strlen(MQTTDebug.topic);

        // Set payload
        MQTTDebug.payload = &dbg_payload;
        MQTTDebug.payloadlen = strlen(MQTTDebug.payload);

        // Set flag for MQTT Task
        setMQTTPubFlag(DEBUG);

        UART_PRINT("Publishing ACK to [ %s ] \n\r",MQTTDebug.topic);
        UART_PRINT("\tPayload: [ %s ]\n\n\r",MQTTDebug.payload);

        LOG("[ MQTT ] - Publishing ACK to [ %s ] \n\r",MQTTDebug.topic);
        LOG("[ MQTT ]\tPayload: [ %s ]\n\n\r", MQTTDebug.payload);
    }

    if(bRebootFlag){
        setMQTTRebootFlag();
    }

    return;
}

static void vBlinkTask(void *pvParameters)
{
    g_ucMQTTLEDStatus = LED_OFF;

    while(1)
    {
        osi_Sleep(100);

        //
        // Acknowledge Watchdog timer to avoid MCU reboot
        //
        WatchdogAck();

        //
        // LED section
        //

        //
        // MQTT LED (STAT 1)
        //
        if ( g_ucMQTTLEDStatus == LED_ON)
        {
            GPIO_IF_LedOn(MCU_STAT_1_LED_GPIO);
        }
        else if ( g_ucMQTTLEDStatus == LED_BLINK)
        {
            GPIO_IF_LedBlink(MCU_STAT_1_LED_GPIO,100);
        }
        else
        {
            GPIO_IF_LedOff(MCU_STAT_1_LED_GPIO);
        }

        // MICS Preheat Active LED (STAT 2)
        if (GPIO_MICS_Preheat_GET())
        {
            GPIO_IF_LedOn(MCU_STAT_2_LED_GPIO);
        }
        else
        {
            GPIO_IF_LedOff(MCU_STAT_2_LED_GPIO);
        }

        // Network LED (STAT 3)
        if ( getStatus() == 0xA ) // STA mode
        {
            GPIO_IF_LedOn(MCU_STAT_3_LED_GPIO);
        }
        // I've seen 0x8 and 0xE (when a device is connected) in AP mode
        else if ( getStatus() & 8 ) // AP mode
        {
            GPIO_IF_LedBlink(MCU_STAT_3_LED_GPIO, 200);
        }
        else // unconnected
        {
            GPIO_IF_LedBlink(MCU_STAT_3_LED_GPIO, 1000);
        }

    }
}

static void _setNetworkInfo()
{
    setMacAddress();
    UART_PRINT("\n\n\r***********************************************************\n\r");
    UART_PRINT("MAC ADDRESS: ");
    char* mac = (char*)malloc(6*2+5+1);
    cpyMacAddress(mac);
    UART_PRINT("%s",mac);
    free(mac);
    UART_PRINT("\n\r***********************************************************\n\n\r");

    setSSIDName();
    setDeviceName();
    setAPDomainName();
    cpyUniqueID(g_cUniqueID);
}

static void _signalTasks()
{
    osi_SyncObjSignal(&g_WLANConnectedNTP);
    osi_SyncObjSignal(&g_WLANConnectedPublish);
    osi_SyncObjSignal(&g_WLANConnectedMQTT);
}

typedef enum {CHECK_PROF,TRY_STA, TRY_AP, IDLE} eNetStates;
static void vNetworkConfigTask(void *pvParameters)
{
    _i32 lRetVal;
    _u16 OSI_SLEEP;
    char ssid[32];
    _u8 scan = 1;   // want to scan initially
    _u8 full_start = FALSE;
    _u32 sta_reconnect_timer;
    _u8 sta_reconnect_timer_active = FALSE;
    _u32 timer;
    int index;
    static _u8 timer_active = 0;
    static _u8 station_tries = 0;

    // Initialize some flags
    setNetworkProfileRequestFlag(1);
    setLastConnectionError(ROLE_AP);

    // Start SimpleLink
    lRetVal = sl_SStart();

    // Set network info
    _setNetworkInfo();

    eNetStates state = IDLE;
    while(1)
    {
        if (Net_Status()){
            setLastConnectionError(0);
        }
        osi_LockObjLock(&g_NetworkLockRequest, OSI_WAIT_FOREVER);

        switch(state){

            // IDLE: PICK STATION or AP MODE accordingly
            case IDLE:{

                station_tries = 0;
                if (sta_reconnect_timer_active){
                    // If the timer's active and it's been 10 minutes let's try to reconnect to STA
                   if ((TIME_GetSecondsActive(TIME_EITHER) - sta_reconnect_timer) % 3 == 0 ){
                       UART_PRINT("\nTIMER COUNT: %lu\n\n\r",TIME_GetSecondsActive(TIME_EITHER) - sta_reconnect_timer);
                   }
                }

                // NOTE: Profile request and user-pressed-connect have been
                //  disconnected in case there is ever a case where
                //  something other than a user-pressed-connect requests
                //  a profile check. If nothing else is ever used, then they
                //  mean the same thing and can be combined.
                if( getNetworkProfileRequestFlag() ){
                    setNetworkProfileRequestFlag(FALSE);

                    // User pressed connect, start timer.
                    //  If profile isn't good in 5 seconds mark profile bad and remove
                    if( getUSERConnectPressedFlag() ){
                        UART_PRINT("Timer is active\n\r");
                        timer = TIME_millis();
                        timer_active = 1;
                        full_start = 1;
                        setUSERConnectPressedFlag(FALSE);

                        // mark the added profile as bad until proven otherwise
                        setLastProfileGoodFlag(FALSE);
                        getLastAddedProfile(ssid);
                        UART_PRINT("\n\n\r[ MAIN ] Last Added Profile: [ %s ]\n\r\tValid? [ %s ]\n\n\r", ssid ,(getLastProfileGoodFlag()) ? "GOOD" : "BAD");
                    }
                    else{
                        full_start = 0;
                    }

                    // Move immediately to STA or AP
                    state = ( sl_ProfilesExist() ) ? TRY_STA : TRY_AP;
                    OSI_SLEEP = 0;

                }

                else if (sta_reconnect_timer_active && (TIME_GetSecondsActive(TIME_EITHER)-sta_reconnect_timer)>30){
                    UART_PRINT("\n\n\rTIMER DONE\n\n\r");
                    sta_reconnect_timer_active = FALSE;
                    state = TRY_STA;
                }

                else{   // Stay in IDLE
                    OSI_SLEEP = 1000;
                }
                break;
            }

            case TRY_STA:{

                UART_PRINT("Attempting STATION MODE...\n\r");
                                                    // always full start
                lRetVal = Wlan_Connect(ROLE_STA, scan, TRUE);     // scan is unused for ROLE_STA

                // Bad Password is the best error for the user to see, but it gets set in the General Event Handler
                if(getLastConnectionError() != -109){
                    setLastConnectionError(lRetVal);
                }

                UART_PRINT("\tTRY_STA returned: %l\n\r",lRetVal);

                // Connected! Mark the profile as good,
                //  and signal all tasks waiting on an
                //  internet connection
                if(lRetVal == ROLE_STA && station_tries++ < 3){
                    UART_PRINT("[ MAIN ] TRY_STA successful. Marking last-added profile as GOOD\n\n\r");
                    setLastProfileGoodFlag(TRUE);
                    _signalTasks();
                    state = IDLE;
                    OSI_SLEEP = 100;
                }

                else{   // go immediately to AP MODE and try
                    state = TRY_AP;
                    OSI_SLEEP = 0;
                    station_tries = 0;
                }

                break;

            }

            case TRY_AP:{

                station_tries = 0;
                UART_PRINT("Attempting ACCESS POINT MODE...\n\r");
                lRetVal = Wlan_Connect(ROLE_AP, scan, FALSE);                 // full_start is unused for ROLE_AP
                UART_PRINT("\tCONNECT AP RETURNED: %i\n\r",lRetVal);

                setNetworkProfileRequestFlag(0);

                scan = 0;   // only scan once after power cycle

                if(lRetVal == ROLE_AP){

                    // If a profile exists, attempt to reconnect in STA mode every 10 minutes.
                    //      Start the timer here.
                    if (sl_ProfilesExist()){
                        UART_PRINT("\n\n\rSTARTING TIMER\n\n\r");
                        sta_reconnect_timer = TIME_GetSecondsActive(TIME_EITHER);
                        sta_reconnect_timer_active = TRUE;
                    }

                    state = IDLE;
                    OSI_SLEEP = 1000;
                }
                else{
                    state = TRY_AP;
                    OSI_SLEEP = ONE_SEC;
                }

                break;
            }

            default:{
                state = IDLE;
                OSI_SLEEP = 1000;
                break;
            }
        }

        // If timer expires and profile is still not good, remove it
        if(TIME_millis()-timer > 6*1000 && timer_active ){
            UART_PRINT("Timer expired\n\r");
            timer_active = 0;

            // remove profile if not good
            if(!getLastProfileGoodFlag()){
                getLastAddedProfile(ssid);
                index = sl_GetProfileIndex(ssid);
                if(index != -1){
                    UART_PRINT("Deleting bad profile\n\r");
                    lRetVal = sl_WlanProfileDel(index);
                }
            }
        }

        osi_LockObjUnlock(&g_NetworkLockRequest);

        osi_Sleep(OSI_SLEEP);

    } // end while(1)
}

//****************************************************************************
//
//!    \brief HTTP Server Application Main Task - Initializes SimpleLink Driver and
//!                                              Handles HTTP Requests
//! \param[in]                  pvParameters is the data passed to the Task
//!
//! \return                        None
//
//****************************************************************************
//static void vNetworkConfigTask(void *pvParameters) {
//
//    long lRetVal = FAILURE;
//    static unsigned char bHasConnected = 0;
//    static unsigned char bProfileExists = 0;
//    static unsigned int timeout = 0;
//    const unsigned int TIMEOUT_MAX = 3;
//    unsigned long PingTimer;
//    unsigned int pingTestCounter;
//    g_ucMode = 0;
//    char *mac;
//
///*************************************************************************************
// * Set Device Name, ID, SSID, and Domain Name
//*************************************************************************************/
//    // This is the ONLY place sl_Start() is called! This is very important
//    //  because SimpleLink can freeze if you call this once SimpleLink has
//    //  already been started... Also will freeze if you call sl_Stop()
//    //  while SimpleLink is not started... If you can find a function or register
//    //  that gives the current state of the SimpleLink state machine that would
//    //  be great.
//    lRetVal = sl_Start(0,0,0);
//
//    // If we can't start SimpleLink lock the RTOS here. The watchdog will reboot
//    //  the board to a good state in ~28 seconds...
//    ERROR_LOOP(lRetVal);
//    setSLStatus(1);
//
//    setMacAddress();
//    setSSIDName();
//    setDeviceName();
//    setAPDomainName();
//    cpyUniqueID(g_cUniqueID);
//
//    UART_PRINT("\n\n\r***********************************************************\n\r");
//    UART_PRINT("MAC ADDRESS: ");
//    mac = (char*)malloc(6*2+5+1);
//    cpyMacAddress(mac);
//    UART_PRINT("%s",mac);
//    free(mac);
//    UART_PRINT("\n\r***********************************************************\n\n\r");
//
//
///*************************************************************************************
// * Try to connect in STA Mode
//*************************************************************************************/
//CONNECT:
//    lRetVal = -1;
//
//    setLastConnectionError(0);
//
//    osi_LockObjLock(&g_NetworkLockRequest, OSI_WAIT_FOREVER);
//    bProfileExists = sl_ProfilesExist();
//    if(bProfileExists)  // If no profiles don't try to connect in STA mode
//    {
//        // TODO: sleep 100ms. check if IP and CONN, then don't do this if it's there
//
//        LOG("WLAN - Profile in system...\n");
//
//        // Try STA mode 3 times
//        for (timeout = 0; timeout<TIMEOUT_MAX; timeout++)
//        {
//            LOG("WLAN - Connecting To Network. [Try %i/3]\n",timeout+1);
////            lRetVal = Wlan_ConnectToNetwork(ROLE_STA);
//            lRetVal = Wlan_Connect(ROLE_STA);
//            if(lRetVal == SUCCESS || getLastConnectionError() == -109)
//            {
//                g_ucMode = 1;
//                break;
//            }
//            osi_Sleep(1000);
//        }
//    }
//    else
//    {
//        UART_PRINT("No profiles in system... AP Mode\n\r");
//    }
//
//
//    // Couldn't connect with STA mode (After 3 tries) --> Connect with AP mode
//    //  Regarding bProfileExists: SimpleLink can be pesky in that it can report
//    //  Net_Status() == TRUE when there's no profile in the system. Don't know
//    //  what causes this, but it was happening when I added a profile and rebooted
//    //  the board before async SimpleLink API could get its grubby paws on the
//    //  profiles. Its state machine would get out of sync, and would report that
//    //  it is connected and has acquired an IP, although was reporting that no
//    //  profiles were in the system, so who leased it an IP is beyond me. Anyway, this
//    //  should fix that.
//    if(lRetVal != SUCCESS || !bProfileExists || getLastConnectionError() == -109)
//    {
//        LOG("WLAN - Connect AP Mode\n");
////        lRetVal = Wlan_ConnectToNetwork(ROLE_AP);
//        lRetVal = Wlan_Connect(ROLE_AP);
//        ERROR_LOOP(lRetVal);
//        g_ucMode = 2;
//    }
//
//    osi_LockObjUnlock(&g_NetworkLockRequest);
//    osi_SyncObjSignal(&g_WLANConnectedNTP);
//    UART_PRINT("\n\r*** You can stop waiting now NTP Task,\n\r\tLove NetworkConfigTask <3\n\n\r");
//
//    // if STA mode established, we can start MQTT
//    if(Net_Status())
//    {
//        bHasConnected = 1;
//
//        osi_SyncObjSignal(&g_WLANConnectedPublish);
//        osi_SyncObjSignal(&g_WLANConnectedMQTT);
//    }
//
//    timeout = 0;
//    // Sit here forever if in STA mode
//    //  Try to get into STA mode periodically if we aren't
//    while(1)
//    {
//        osi_Sleep(100);
//
//        //UART_PRINT("g_ulStatus: [%lu]\n\r",getStatus());
//        timeout++;
//
//        // Do a ping test every 10 minutes [60s * 10m = 600]
//        //      OR if someone is requesting one
//        if (Net_Status() && ((timeout % 6000 == 0) || g_ucRequestPingTest))
//        {
//            osi_LockObjLock(&g_NetworkLockRequest, OSI_WAIT_FOREVER);
//
//            // A task is requesting a ping test
//            if(g_ucRequestPingTest)
//            {
//                LOG("WLAN - Ping Test Requested\n");
//                g_ucRequestPingTest = 0;
//            }
//
//            bHasConnected = 1;
//
//            Net_ConnectionTest();
//
//            if(Net_Status())
//            {
//                g_ucMode = 1;
//                LOG("WLAN - Ping Test SUCCESSFUL\n");
//            }
//            else
//            {
//                LOG("[ERROR] WLAN - Ping Test FAILED\n");
//            }
//
//            osi_LockObjUnlock(&g_NetworkLockRequest);
//        }
//
//        //
//        // no internet connection but device has IP and CON to WLAN
//        // Try a ping test every second until we've reconnected
//        // We'll enter here after SimpleLink Async has reconnected us to AP
//        //
//        if(!Net_Status() && getStatus()==10) // 10 is STATUS_BIT_IP_AQUIRED [0x08] & STATUS_BIT_CONNECTION [0x02]
//        {
//            LOG("WLAN - STA Mode: Ping test\n");
//            Net_ConnectionTest();
//            osi_Sleep(900);
//        }
//
//        //
//        // something happened that brought us offline
//        //  Reset internet access - Async SimpleLink will handle the rest
//        //
//        if(!Net_Status() && bHasConnected)
//        {
//            LOG("WLAN - Disconnected from router... Awaiting reconnect\n");
//            Net_RSTInternetAccess();    // Net_Status() will now evaluate to False (effectively locking all tasks that use the internet)
//            bHasConnected = 0;
//            g_ucMode = 0;
//        }
//
//        // if AP mode, try STA mode every half hour
//        if(!Net_Status() && timeout % 18000 == 0)
//        {
//            LOG("WLAN - AP Mode: Trying STA Mode\n");
//            goto CONNECT;
//        }
//
//    } // end while(1)
//}

//
// Handles MQTT Events
//
static void vMQTTTask(void *pvParameters)
{
    volatile const unsigned int REFRESH_TIME = 100;
    int rc = -1;
    unsigned char ucMQTTReconnectRequest = 1;
    unsigned char ucRetryCounter = 0;
    unsigned char ucInitializeRetryCounter = 0;
    unsigned char MAX_NUM_RETRIES = 5;
    unsigned char bPubCycle = 1;

    // Create the required connection info
    MQTTConnectionInfo ci;
    ci.addr = MQTT_BROKER_ADDR;
    ci.n = n;
    ci.timeout_ms = 1000;


    osi_SyncObjWait(&g_WLANConnectedMQTT, OSI_WAIT_FOREVER);
#ifdef USE_SSL
    osi_SyncObjWait(&g_TimeUpdate,OSI_WAIT_FOREVER);
#endif

    LOG("[ MQTT ] - MQTT Task entered\n");

    while(1)
    {
        osi_Sleep(100);

        if (Net_Status() == CONNECTED)
        {
            // Used to lock the simplelink resources from other tasks
            osi_LockObjLock(&g_NetworkLockRequest, OSI_WAIT_FOREVER);
            //
            // Intial connection and reconnects can be done using MQTTInitialize()
            //
            if(ucMQTTReconnectRequest)
            {
                LOG("[ MQTT ] - Reconnect Request\n\n\r");
                cpyUniqueID(g_cUniqueID);
                LOG("[ MQTT] - Client ID is %s\n\n\r",g_cUniqueID);
                ci.clientID = g_cUniqueID;

                UART_PRINT("***\n\rReconnect Request\n\r***\n\r");
                rc = MQTTInitialize(&ci);
                if(rc == SUCCESS)
                {
                    LOG("[ MQTT ] - MQTT Initialized\n");

                    //
                    // Subscribe to topics
                    //
                    rc = MQTTSubscribeAll(&ci.hMQTTClient, QOS1, messageArrived);
                    if(rc >= SUCCESS)
                    {
                        ucMQTTReconnectRequest = 0;
                        ucRetryCounter = 0;
                        LOG("[ MQTT ] - MQTT Subscribed\n");
                    }
                }
            }

            //
            // Publish or Listen (Trade off between them so we don't miss anything)
            //
            else
            {
                // if it's publish turn and there's something to publish
                if(bPubCycle && (getMQTTPubFlag() > 0))
                {

                    // Publishing datapoint get priority (because it comes up infrequently)
                    if ( (DATAPOINT & getMQTTPubFlag()) == DATAPOINT )
                    {

                        UART_PRINT("[ MQTT ] Publishing datapoint\n\r");
                        LOG("[ MQTT ] Publishing datapoint\n\r");

                        rc = MQTTPublish(&ci.hMQTTClient, &MQTTInfluxDataPoint);
                        if (rc == SUCCESS)
                        {
                            last_data_post_time = TIME_GetPosix(TIME_NTP);
                            resetMQTTPubFlag(DATAPOINT);
                        }
                        else
                        {
                            UART_PRINT("[ERROR] MQTT - Publish Data Point [rc=%d]\n",rc);
                            LOG("[ERROR] MQTT - Publish Data Point [rc=%d]\n",rc);

                        }
                    }

                    //
                    // put other publish messages here
                    //

                    // Debug messages
                    else if ( (DEBUG & getMQTTPubFlag()) == DEBUG )
                    {

                        UART_PRINT("[ MQTT ] Publishing debug message\n\r");
                        LOG("[ MQTT ] Publishing debug message...\n\r");


                        rc = MQTTPublish(&ci.hMQTTClient, &MQTTDebug);
                        if (rc == SUCCESS)
                        {
                            resetMQTTPubFlag(DEBUG);
                        }
                        else
                        {
                            UART_PRINT("[ERROR] MQTT - Publish Debug [rc=%d]\n",rc);
                            LOG("[ERROR] MQTT - Publish Debug [rc=%d]\n",rc);

                        }

                        if(getMQTTRebootFlag()){
                            UART_PRINT("Rebooting\n\r");
                            RebootMCU();
                        }

                    }

                    bPubCycle = 0;  // Yield's turn

                }

                //
                // Yield
                //
                else // nothing to publish --> check incoming messages
                {

                    rc = MQTTYield(&ci.hMQTTClient, 10);
                    if (rc != SUCCESS)
                    {
                        LOG("[ERROR] MQTT - Yield [rc=%d]\n",rc);
                    }

                    bPubCycle = 1;
                }
            }

            //
            // Evaluate the return from publishing/yielding/connecting
            //  *If there was an error with the network or socket
            //
            if (rc < SUCCESS)
            {
                LOG("[ERROR] MQTT - Requesting reconnect\n");

                ucMQTTReconnectRequest = 1;

                if(++ucRetryCounter % MAX_NUM_RETRIES == 0)
                {
                    osi_Sleep(ONE_MIN-REFRESH_TIME);
                    g_ucRequestPingTest = 1;    // failing a ping test will make Net_Status() != TRUE, effectively locking this task
                    ucRetryCounter = 0;         // Reset the counter so the next time there's a failure we try 'MAX_NUM_RETRIES' times
                }

                else
                {
                    osi_Sleep(ONE_SEC-REFRESH_TIME); // Don't retry too often, as errors usually indicate something external needs to fall into the correct state
                }
            }

            osi_LockObjUnlock(&g_NetworkLockRequest);

        }       /* end Net_Status()  */
    }           /* end while(1)     */
}               /* end vMQTTTask    */

/*
 * Signal that the datapoint payload is ready to be published
 */
static void vPushDataPointTask(void *pvParameters)
{
#ifdef FACTORY
    unsigned char gps_time_good = 0;
#endif

    osi_SyncObjWait(&g_WLANConnectedPublish, OSI_WAIT_FOREVER);

    // set the constants for the MQTT Influx datapoint (MQTTMessage struct)
    MQTTInfluxDataPoint.payload = &payload;
    MQTTInfluxDataPoint.qos = QOS1;

    while(1)
    {
#ifdef FACTORY
        osi_Sleep(10*ONE_SEC);
#else
        osi_Sleep(PUB_DATA_SLEEP);
#endif

        // Topic is subject to change from remote MQTT command
        strcpy(MQTTInfluxDataPoint.topic, MQTT_INFLUX_TOPIC);
        MQTTInfluxDataPoint.topiclen = strlen(MQTTInfluxDataPoint.topic);

        if (TIME_Exists(TIME_NTP)) // just to make sure time has been set
        {

#ifdef FACTORY
            gps_time_good = TIME_Exists(TIME_GPS);
#endif

            // clean out the last entry
            memset(payload,0,MAX_PLD_LENGTH);

            cpyUniqueID(g_cUniqueID);
#ifdef FACTORY
            sprintf(payload,POST_DATA_FACTORY,  g_cUniqueID, HW_VER, FW_VER, TIME_GetPosix(TIME_EITHER), TIME_GetSecondsActive(TIME_EITHER),
                                            dataPoint.altitude, dataPoint.latitude, dataPoint.longitude,
                                            dataPoint.pm1, dataPoint.pm2_5, dataPoint.pm10,
                                            dataPoint.temperature, dataPoint.humidity,dataPoint.co, gps_time_good);

#else
            sprintf(payload, POST_DATA_AIR, g_cUniqueID, HW_VER, FW_VER, TIME_GetPosix(TIME_EITHER), TIME_GetSecondsActive(TIME_EITHER),
                                            dataPoint.altitude, dataPoint.latitude, dataPoint.longitude,
                                            dataPoint.pm1, dataPoint.pm2_5, dataPoint.pm10,
                                            dataPoint.temperature, dataPoint.humidity,dataPoint.co, dataPoint.nox);
#endif

            MQTTInfluxDataPoint.payloadlen = strlen(MQTTInfluxDataPoint.payload);

            UART_PRINT("Publish request sent to MQTT Task...\n\r");

            setMQTTPubFlag(DATAPOINT);
        }
    }

}

void vSyncTime_GPS(void *pvParameters)
{
    long lRetVal;
    unsigned int WAIT_TIME;


    WAIT_TIME = 5*ONE_SEC;
    osi_Sleep(WAIT_TIME);
    while(1)
    {
        UART_PRINT("\n\n\rGetting RTC Time\n\n\r");
        lRetVal = GPS_SetPOSIXFromRTC(TRUE);
        if (lRetVal != SUCCESS)
        {
            WAIT_TIME = 10*ONE_SEC;
        }else{
            WAIT_TIME = 10*ONE_MIN;
        }

//        GPS_SendCommand(PMTK_CMD_WARM_START);


        osi_Sleep(WAIT_TIME);
//        UART_PRINT("\n\n\r [ HOT START ]\n\n\r");
//        GPS_SendCommand(PMTK_CMD_HOT_START);
//        osi_Sleep(10000);//*ONE_MIN);
//        UART_PRINT("\n\n\r [ WARM START ]\n\n\r");
//        GPS_SendCommand(PMTK_CMD_WARM_START);
//        osi_Sleep(30000);//*ONE_MIN);
//        UART_PRINT("\n\n\r [ COLD START ]\n\n\r");
//        GPS_SendCommand(PMTK_CMD_COLD_START);
//        osi_Sleep(50000);

//        UART_PRINT("\tPOSIX:  \t[ %lu ]\n\r",TIME_GetPosix(TIME_GPS));
//        UART_PRINT("\tUptime: \t[ %lu ]\n\r",TIME_GetSecondsActive(TIME_GPS));
//        UART_PRINT("\tLast update:\t[ %lu ]\n\r",TIME_SecondsSinceUpdate(TIME_GPS));
//        UART_PRINT("\tIteration:\t[ %lu ]\n\n\r",iteration++);

//        UART_PRINT("\n\n\r [ GPS RTC COMMAND ] \n\n\r");
//
//        osi_Sleep(10000);
//        UART_PRINT("\n\n\r\r [ GPS ALL DATA COMMAND ] \n\n\r");
//        GPS_SendCommand(PMTK_CMD_FULL_COLD_START);


    } // while(1)
}

void PM_Tally(void *pvParameters)
{
    sPMData s_PMData;
    long lRetVal;
    int cnt = 0;
    while(1)
    {
        osi_Sleep(1000);

        PMS_Tally();

    }
}


//void WTF(void *pvParameters)
//{
//    uint32_t i;
//    char str[100];
//    while(1)
//    {
//        i = (i+1) % 20;
//        osi_Sleep(ONE_SEC);
//        strcpy(str, big_ole_array[i]);
//        UART_PRINT("Value: %s\n\r",str);
//    }
//}

void vMICS_Preheat(void *pvParameters)
{
    int MICS_SLEEP_TIME = 30*ONE_SEC;
//    // Preheat every time there's a restart on the board
//    GPIO_MICS_Preheat(1);
//    osi_Sleep(MICS_SLEEP_TIME);
//    GPIO_MICS_Preheat(0);
//    UART_PRINT("MICS PREHEAT DEACTIVATED\n\n\r");

    while(1)
    {
        osi_SyncObjWait(&g_MICSPreheat,OSI_WAIT_FOREVER);
        UART_PRINT("MICS PREHEAT IS ACTIVE\n\n\r");
        GPIO_MICS_Preheat(1);
        osi_Sleep(MICS_SLEEP_TIME);
        GPIO_MICS_Preheat(0);
        UART_PRINT("MICS PREHEAT DEACTIVATED\n\n\r");



//        UART_PRINT("Here comes the preheat\n\n\r");
//        osi_Sleep(3000);
        // Wait until MQTT requests a reheat, then do it

//        if(g_ucMICSPreheatFlag)
//        {

//        g_ucMICSPreheatFlag = 0;
//        }
    }
}

/*
 * If we can't get an internet connection for some time
 *  then reboot the board. The last-ditch effort to get internet.
 */
void WTF(void *pvParameters)
{
    unsigned char NO_INTERNET_MAX_WAIT = 15;
    int no_internet_cntr = 0;
    while(1){
        osi_Sleep(ONE_MIN);

        if(Net_Status() == FALSE){
            if(++no_internet_cntr == NO_INTERNET_MAX_WAIT){
                LOG("No Internet for %uc minutes - Rebooting\n\r", NO_INTERNET_MAX_WAIT);
                RebootMCU_RestartNWP();
            }
        }
        else{
            no_internet_cntr = 0;
        }
    }

}

void vFileDownload(void *pvParameters)
{
    g_sFileDownload.data_valid = false;
    long lRetVal = FAILURE;
    while(1)
    {
        UART_PRINT("vFileDownload about to wait...\n\n\r");
        // Wait for signal
        osi_SyncObjWait(&g_FileDNLObj, OSI_WAIT_FOREVER);
        osi_SyncObjClear(&g_FileDNLObj);

        osi_LockObjLock(&g_NetworkLockRequest, OSI_WAIT_FOREVER);

        lRetVal = AIR_DownloadFile(g_sFileDownload.src,g_sFileDownload.dst);
        g_sFileDownload.data_valid = false;

        UART_PRINT("AIR_DownloadFile returned: [ %d ]\n\n\r",lRetVal);

        osi_LockObjUnlock(&g_NetworkLockRequest);

    }
}


/*
 *  PRISMS Production Version (p1.0.5.3p)
 *  Differences between factory and production versions:
 *      - 5 second sampling
 *      - 10 second uploads
 *      - data publish topic: airu/factory
 *      - ID ACK topic: airu/ack/<ID>
 *      - Data packet contains GPS timestamp boolean
 */
void main() {


    char app_name[25];
    sprintf(app_name,"AIRU Version %s",FW_VER);

/*****************************************************************************
 * Initialization Section
 ****************************************************************************/

    // Board Initialization
    BoardInit();

    // Configure the pinmux settings for the peripherals exercised
    PinMuxConfig();

    PinConfigSet(PIN_58, PIN_STRENGTH_2MA | PIN_STRENGTH_4MA, PIN_TYPE_STD_PD);

    GPIO_MICS_Preheat(0);
    //
    // PM Sensor and CONSOLE
    //
    PMS_Init();
    SD_Init();

/*****************************************************************************
 * Watchdog
 ****************************************************************************/

    //
    // Initialize WDT
    //  This comes out to be about 18-20 seconds
    //
    WDT_IF_Init(NULL,80000000 * 10);

    //
    // Get the reset cause
    //
    g_lResetCause = PRCMSysResetCauseGet();

    //
    // If watchdog triggered reset request hibernate
    // to clean boot the system
    //
    if( g_lResetCause == PRCM_WDT_RESET )
    {
        RebootMCU();
    }

/*****************************************************************************
 ****************************************************************************/

    //
    // Initialize the rest of the peripherals
    //
    g_ucTimeSynced = 0;
    GPS_Init(0);
    InitMQTTTimer();
    TIME_InitHWTimer();

    Wlan_InitializeAppVariables();

    DisplayBanner(app_name);

    //
    // LED Init
    //
    GPIO_IF_InitLEDS();

    //
    //  Retrieve RTC time before we get into things
    //
//    GPS_SetPOSIXFromRTC(FALSE);

/*****************************************************************************
 * RTOS Task Section
 ****************************************************************************/

    //
    // Create the task semaphores
    //
    osi_SyncObjCreate(&g_WLANReconnectRequest);
    osi_SyncObjCreate(&g_WLANConnectedNTP);
    osi_SyncObjCreate(&g_WLANConnectedPublish);
    osi_SyncObjCreate(&g_WLANConnectedMQTT);
    osi_SyncObjCreate(&g_MQTTDebugPub);
    osi_SyncObjCreate(&g_MICSPreheat);
    osi_SyncObjCreate(&g_FileDNLObj);
#ifdef USE_SSL
    osi_SyncObjCreate(&g_TimeUpdate);
#endif
    osi_LockObjCreate(&g_NetworkLockRequest);

    //
    // Create the tasks
    //
    VStartSimpleLinkSpawnTask(SPAWN_TASK_PRIORITY);

    osi_TaskCreate(vNetworkConfigTask, (const signed char*) "ConfNet",
                   2500, NULL, 8, NULL);
    osi_TaskCreate(vDataGatherTask, (const signed char*)"DataGatherTask",
                   2048, NULL, 1, NULL);
    osi_TaskCreate(vSDStoreTask, (const signed char*)"DataUploadTask",
                   2024, NULL, 3, NULL);
    osi_TaskCreate(vPushDataPointTask, (const signed char*)"PubTask",
                   2024, NULL, 3, NULL);
    osi_TaskCreate(vSyncTime_NTP,(const signed char *)"Sync Device Time",
                   1024, NULL, 3, NULL);
    osi_TaskCreate(vSyncTime_GPS, (const signed char*)"gps_time",
                   2048, NULL, 3, NULL);
    osi_TaskCreate(vMQTTTask, (const signed char*)"MQTTTask",
                   4096, NULL, 5, NULL);
    osi_TaskCreate(PM_Tally, (const signed char*)"PM_Tally",
                   1024, NULL, 4, NULL);
//    osi_TaskCreate(vMICS_Preheat, (const signed char*)"MICS_Preheat",
//                   1024, NULL, 3, NULL);
//    osi_TaskCreate(vFileDownload, (const signed char*)"FILE_DNL",
//                   2048, NULL, 7, NULL);

//    osi_TaskCreate(WTF, (const signed char*)"WTF",
//                   2000, NULL, 5, NULL);

    // NOTE: BlinkTask is also used to update Watchdog Timeout, so it's always needed
    osi_TaskCreate(vBlinkTask, (const signed char*)"Blinky",
                   1024, NULL, 1, NULL);

    // Start OS Scheduler
    osi_start();

}
