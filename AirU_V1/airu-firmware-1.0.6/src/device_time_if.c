/*
 * device_time_if.c
 *
 *  Created on: Sep 24, 2017
 *      Author: Tom
 */

#include "device_time_if.h"
//#include "sd_time_if.h"
#include "osi.h"

#define NUM_SNTP_SERVERS 8

static long _sntp_server_timestamp_request(void);
static long _get_ntp_time(void);
static int _open_socket(void);
static int _close_socket(int);
static void _update_dt_struct_from_ntp(void);

//!    ######################### list of SNTP servers ##################################
//!    ##
//!    ##          hostname         |        IP       |       location
//!    ## -----------------------------------------------------------------------------
//!    ##   nist1-nj2.ustiming.org  | 165.193.126.229 |  Weehawken, NJ
//!    ##   nist1-pa.ustiming.org   | 206.246.122.250 |  Hatfield, PA
//!    ##   time-a.nist.gov         | 129.6.15.28     |  NIST, Gaithersburg, Maryland
//!    ##   time-b.nist.gov         | 129.6.15.29     |  NIST, Gaithersburg, Maryland
//!    ##   time-c.nist.gov         | 129.6.15.30     |  NIST, Gaithersburg, Maryland
//!    ##   ntp-nist.ldsbc.edu      | 198.60.73.8     |  LDSBC, Salt Lake City, Utah
//!    ##   nist1-macon.macon.ga.us | 98.175.203.200  |  Macon, Georgia
//!
//!    ##   For more SNTP server link visit 'http://tf.nist.gov/tf-cgi/servers.cgi'
//!    ###################################################################################
unsigned char g_ucSNTPServerListIndex = 0;
const char g_acSNTPserverList[NUM_SNTP_SERVERS][30] = {
                                        {"wwv.nist.gov"},
                                        {"time-a.nist.gov"},
                                        {"time-b.nist.gov"},
                                        {"time-c.nist.gov"},
                                        {"ntp-nist.ldsbc.edu"},
                                        {"nist1-macon.macon.ga.us"},
                                        {"nist1-pa.ustiming.org"},
                                        {"nist1-nj2.ustiming.org"}};
char *g_acSNTPserver = (char*)g_acSNTPserverList[0];

unsigned char g_sSNTPDataInUse;

// Sunday is the 1st day in 2017 - the relative year
unsigned char g_SundayIndex;
const char g_acDaysOfWeek2017[7][3] = { { "Sun" }, { "Mon" }, { "Tue" }, { "Wed" },
                                        { "Thu" }, { "Fri" }, { "Sat" } };
const char g_acMonthOfYear[12][3]   = { { "Jan" }, { "Feb" }, { "Mar" }, { "Apr" },
                                        { "May" }, { "Jun" }, { "Jul" }, { "Aug" },
                                        { "Sep" }, { "Oct" }, { "Nov" }, { "Dec" } };
const char g_acNumOfDaysPerMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31,30, 31 };

// socket variables
SlSockAddr_t sAddr;
SlSockAddrIn_t sLocalAddr;


static unsigned long MilliTimer = 0;
unsigned char g_bGPSRdyFlag = 0;

struct {
    unsigned long ulDestinationIP;
    int iSockID;
//    unsigned long ulSecActive;
    unsigned long ulElapsedSec1900;
    unsigned long ulFirstTimestamp;
//    short isGeneralVar;
//    unsigned long ulGeneralVar;
//    unsigned long ulGeneralVar1;
//    char acTimeStore[30];
//    char *pcCCPtr;
//    char dateISO8601[11];
//    char localTimeString[9];
//    unsigned char ucDayOfWeek;
//    unsigned short uisCCLen;
//    unsigned short uisYear;
//    unsigned short uisMonth;
//    unsigned short uisDay;
//    unsigned short uisHour;
//    unsigned short uisMinute;
//    unsigned short uisSecond;
} g_sSNTPDateTime;

typedef struct
{
   unsigned long tm_sec;
   unsigned long tm_min;
   unsigned long tm_hour;
   unsigned long tm_day;    /* (1-31) */
   unsigned long tm_mon;    /* (1-12) */
   unsigned long tm_year;
   unsigned long tm_week_day; //not required
   unsigned long tm_year_day; //not required
   unsigned long reserved[3];
}SlDateTime;
SlDateTime g_SlDateTime;

// This struct is used externally (in main.c vSyncTime())
sTimeInfo g_sTimeInfo; // TODO: should be named g_sNTPTimeInfo

// Holds reference from GPS time. Can be used when no NTP
sTimeInfo g_sGPSTimeInfo;

// SD card time info
sSDTimeInfo ti;


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

/***************************************************************************
 *
 * Hardware timer for keeping device time (ms interrupt period)
 *
 ****************************************************************************/
void TIME_TimerIntHandler()
{
    Timer_IF_InterruptClear(TIMERA1_BASE);

    // I don't even know why I have 2 ms timers...
    MilliTimer++;
    g_sTimeInfo.ulMSActive++;

    if ( !(MilliTimer%1000) ){

        g_sGPSTimeInfo.ulElapsedSec1900++;
        g_sGPSTimeInfo.ulSecActive++;

        ti.ulElapsedSec1900 = TIME_GetPosix(TIME_EITHER);
        ti.ulSecActive = TIME_GetSecondsActive(TIME_EITHER);

        sd_time_if_set(&ti);

        if ( !g_sSNTPDataInUse ){   // Don't want to update during datetime calc
            g_sTimeInfo.ulElapsedSec1900++;
            g_sTimeInfo.ulSecActive++;
        }
    }
}

/***************************************************************************
 *
 * Reset the timers and set the timer hardware
 *
 ****************************************************************************/
void TIME_InitHWTimer()
{
    // Reset the timers
    g_sTimeInfo.ulFirstTimestamp = 0;
    g_sTimeInfo.ulMSActive = 0;
    g_sTimeInfo.ulSecActive = 0;
    g_sTimeInfo.ulLastTimestamp = 0;
    g_sTimeInfo.ulElapsedSec1900 = 0;
    g_sSNTPDataInUse = 0;

    // Update time struct used by uSD card logger
    ti.ulElapsedSec1900 = 0;
    ti.ulSecActive = 0;

    sd_time_if_set(&ti);

    // Clear the GPS time info. It will be updated internally every second
    //  and can be set externally through functions
    g_sGPSTimeInfo.ulFirstTimestamp = 0;
    g_sGPSTimeInfo.ulElapsedSec1900 = 0;
    g_sGPSTimeInfo.ulSecActive = 0;
    g_sGPSTimeInfo.ulLastTimestamp = 0;

    for(g_SundayIndex = 0; g_SundayIndex < 7; g_SundayIndex++){
        if(0 == strcmp(g_acDaysOfWeek2017[g_SundayIndex],"Sun"))
            break;
    }

    // Set the timer hardware
    Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC, TIMER_BOTH, 0);
    Timer_IF_IntSetup(TIMERA1_BASE, TIMER_BOTH, TIME_TimerIntHandler);
    Timer_IF_Start(TIMERA1_BASE, TIMER_BOTH, 1);
}

/**
 *  \brief: Update this internal GPS time struct with the time struct from another lib
 *              (probably from gps_if)
 *
 *  \param [in] sTI: gps time info struct from external library
 *
 *  \return: N/A
 *
 */
void TIME_UpdateSystemTimeFromPOSIX(unsigned long ulElapsedSec1900, eTimeSource src)
{
    sTimeInfo *sTI = (src == TIME_NTP) ? &g_sTimeInfo : &g_sGPSTimeInfo;

    sTI->ulElapsedSec1900 = ulElapsedSec1900;
    sTI->ulLastTimestamp = sTI->ulElapsedSec1900;

    // set the time we first got a timestamp as a baseline reference for device time active
    if (sTI->ulFirstTimestamp == 0){
        sTI->ulFirstTimestamp = sTI->ulElapsedSec1900 - sTI->ulSecActive;
    }

    // Seconds Active is perfectly atomic
    else{
        sTI->ulSecActive = sTI->ulElapsedSec1900 - sTI->ulFirstTimestamp;
    }

    TIME_posix2lt(sTI->ulElapsedSec1900, &(sTI->datetime), GMT_DIFF_TIME_HRS);

}

/***************************************************************************
 *
 *  \brief: Get the number of seconds elapsed since the last  timestamp.
 *
 *  \param src: The time source (TIME_NTP or TIME_GPS)
 *
 *  \return seconds since last timestamp
 *
 *  \NOTE: if you send it TIME_EITHER it will choose TIME_GPS, so be aware.
 *
 ****************************************************************************/
unsigned long TIME_SecondsSinceUpdate(eTimeSource src)
{
    sTimeInfo *sTI = ( src == TIME_NTP ) ? &g_sTimeInfo : &g_sGPSTimeInfo;

    return sTI->ulElapsedSec1900 - sTI->ulLastTimestamp;
}

/***************************************************************************
 *
 * Gets the current time from the selected SNTP server
 *
 * \return 0 : success, -ve : failure
 *
 ****************************************************************************/
static long _sntp_server_timestamp_request() {

    char cDataBuf[48];
    long lRetVal = 0;
    int iAddrSize;

    memset(cDataBuf, 0, sizeof(cDataBuf));
    cDataBuf[0] = '\x1b';

    sAddr.sa_family = AF_INET;

    // the source port
    sAddr.sa_data[0] = 0x00;
    sAddr.sa_data[1] = 0x7B;    // UDP port number for NTP is 123
    sAddr.sa_data[2] = (char) ((g_sSNTPDateTime.ulDestinationIP >> 24) & 0xff);
    sAddr.sa_data[3] = (char) ((g_sSNTPDateTime.ulDestinationIP >> 16) & 0xff);
    sAddr.sa_data[4] = (char) ((g_sSNTPDateTime.ulDestinationIP >> 8) & 0xff);
    sAddr.sa_data[5] = (char) (g_sSNTPDateTime.ulDestinationIP & 0xff);

    lRetVal = sl_SendTo(g_sSNTPDateTime.iSockID, cDataBuf, sizeof(cDataBuf), 0,
            &sAddr, sizeof(sAddr));
    if (lRetVal != sizeof(cDataBuf)) {
        // could not send SNTP request
        ASSERT_ON_ERROR(SERVER_GET_TIME_FAILED);
    }

    //
    // Wait to receive the NTP time from the server
    //
    sLocalAddr.sin_family = SL_AF_INET;
    sLocalAddr.sin_port = 0;
    sLocalAddr.sin_addr.s_addr = 0;
    lRetVal = sl_Bind(g_sSNTPDateTime.iSockID, (SlSockAddr_t *) &sLocalAddr,
            sizeof(SlSockAddrIn_t));

    iAddrSize = sizeof(SlSockAddrIn_t);

    /* For use with nonblocking socket */
//    int cnt = 0;
//    int timeout = 50;
//    while(lRetVal < 0 && cnt++<timeout){
//        lRetVal = sl_RecvFrom(g_sSNTPDateTime.iSockID, cDataBuf, sizeof(cDataBuf), 0,
//                    (SlSockAddr_t *) &sLocalAddr, (SlSocklen_t*) &iAddrSize);
//    }

    lRetVal = sl_RecvFrom(g_sSNTPDateTime.iSockID, cDataBuf, sizeof(cDataBuf), 0,
            (SlSockAddr_t *) &sLocalAddr, (SlSocklen_t*) &iAddrSize);
    ASSERT_ON_ERROR(lRetVal);


    //
    // Confirm that the MODE is 4 --> server
    //
    if ((cDataBuf[0] & 0x7) != 4)    // expect only server response
    {
        ASSERT_ON_ERROR(SERVER_GET_TIME_FAILED);  // MODE is not server, abort
    } else {
        g_sSNTPDataInUse = 1;

        //
        // Getting the data from the Transmit Timestamp (seconds) field
        // This is the time at which the reply departed the
        // server for the client
        //
        g_sTimeInfo.ulElapsedSec1900 = cDataBuf[40];
        g_sTimeInfo.ulElapsedSec1900 <<= 8;
        g_sTimeInfo.ulElapsedSec1900 += cDataBuf[41];
        g_sTimeInfo.ulElapsedSec1900 <<= 8;
        g_sTimeInfo.ulElapsedSec1900 += cDataBuf[42];
        g_sTimeInfo.ulElapsedSec1900 <<= 8;
        g_sTimeInfo.ulElapsedSec1900 += cDataBuf[43];

        g_sSNTPDataInUse = 0;
    }

    return SUCCESS;
}

/***************************************************************************
 *
 * Open a socket to the selected SNTP server
 *
 * \return 0 : success, -ve : failure
 *
 ****************************************************************************/
static int _open_socket() {
    int iSocketDesc;
    long lRetVal = -1;
    //
    // Create UDP socket
    //
    iSocketDesc = sl_Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (iSocketDesc < 0) {
        ASSERT_ON_ERROR(iSocketDesc);
    }

    g_sSNTPDateTime.iSockID = iSocketDesc;

    // look up the IP from dns if it hasn't been done previously
    if (g_sSNTPDateTime.ulDestinationIP == 0) {
        //
        // Get the NTP server host IP address using the DNS lookup
        //
        lRetVal = Net_GetHostIP((char*) g_acSNTPserver,
                &g_sSNTPDateTime.ulDestinationIP);
        if (lRetVal < 0) {
            ASSERT_ON_ERROR(lRetVal);
        }
    } else {
        lRetVal = 0;
    }

    /* Try Nonblocking - Uncomment osi_Sleep */
//    SlSockNonblocking_t enableOption;
//    enableOption.NonblockingEnabled = 1;
//    sl_SetSockOpt(g_sSNTPDateTime.iSockID ,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption)); // Enable/disable nonblocking mode

    struct SlTimeval_t timeVal;
    timeVal.tv_sec = 3; //SERVER_RESPONSE_TIMEOUT;    // Seconds
    timeVal.tv_usec = 0;          // Microseconds. 10000 microseconds resolution

    lRetVal = sl_SetSockOpt(g_sSNTPDateTime.iSockID, SL_SOL_SOCKET, SL_SO_RCVTIMEO,
            (unsigned char*) &timeVal, sizeof(timeVal));
    if (lRetVal < 0) {
        ASSERT_ON_ERROR(lRetVal);
    }

    return iSocketDesc;
}

/***************************************************************************
 *
 * Close the active SNTP socket
 *
 * \return 0 : success, -ve : failure
 *
 ****************************************************************************/
static int _close_socket(int iSocketDesc)
{
   return sl_Close(iSocketDesc);
}

/***************************************************************************
 *
 * Set the device time using the NTP server
 *
 * \return   0 - Success
 *          -1 - SimpleLink error
 *
 ****************************************************************************/
int TIME_SetSLDeviceTime()
{
    TIME_UpdateSystemTimeFromPOSIX(g_sTimeInfo.ulElapsedSec1900,TIME_NTP);

    g_SlDateTime.tm_day  = g_sTimeInfo.datetime.uisDay+1;
    g_SlDateTime.tm_mon  = g_sTimeInfo.datetime.uisMonth+1;
    g_SlDateTime.tm_year = g_sTimeInfo.datetime.uisYear;
    g_SlDateTime.tm_sec  = g_sTimeInfo.datetime.uisSecond;
    g_SlDateTime.tm_hour = g_sTimeInfo.datetime.uisHour;
    g_SlDateTime.tm_min  = g_sTimeInfo.datetime.uisMinute;

    return (int)sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                     SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                     sizeof(SlDateTime),(unsigned char *)(&g_SlDateTime));

}

/***************************************************************************
 *
 * Gets the current time from the selected SNTP server
 *
 * \return 0 : success, -ve : failure
 *
 ****************************************************************************/
static long _get_ntp_time() {
    long lRetVal;
    int iSocketDesc;

    iSocketDesc = _open_socket();
    if(iSocketDesc<0){
        return FAILURE;
    }

    lRetVal = _sntp_server_timestamp_request();

    _close_socket(iSocketDesc);

    return lRetVal;
}

/***************************************************************************
 *
 * Attempts every SNTP server until one responds. If no response attempts
 * GPS. Source will change dynamically to GPS if NTP isn't available.
 *
 * \return 0 : success
 *        <0 : failure - time not set using external reference
 *
 ****************************************************************************/
long TIME_SyncNTP()
{
    long lRetVal;
    int i;

    lRetVal = -1;
    if (Net_Status()){

        // Start with the last working server and try them all until one works
        unsigned char start_ind = g_ucSNTPServerListIndex;
        for(i=g_ucSNTPServerListIndex;i<(start_ind+NUM_SNTP_SERVERS);i++)
        {
            g_acSNTPserver = (char*)g_acSNTPserverList[i%NUM_SNTP_SERVERS];

            UART_PRINT("Using NTP Server: [ %s ]\n\r", g_acSNTPserver);

            LOG("TIME - Retreiving timestamp from NTP Server: [ %s ]\n\r", g_acSNTPserver);

            lRetVal = _get_ntp_time();

            UART_PRINT("Finished NTP call with return value: [ %i ]\n\r", lRetVal);
            LOG("Finished NTP call with return value: [ %i ]\n\r", lRetVal);


            if (lRetVal == SUCCESS)
            {
                g_ucSNTPServerListIndex = i % NUM_SNTP_SERVERS;
                UART_PRINT("Working Server: [ %s ]\n\r", g_acSNTPserver);
                break;
            }

            osi_Sleep(1000);

//            if(!Net_Status() && lRetVal < SUCCESS){
//                break;
//            }

        }
    }
    else{
        UART_PRINT("No Internet Connection. Didn't query NTP server.\n\r");
    }

    if(lRetVal == SUCCESS){

        TIME_UpdateSystemTimeFromPOSIX(g_sTimeInfo.ulElapsedSec1900, TIME_NTP);

    }

    return lRetVal;
}

///**
// *
// *  \brief: Update the time struct. If we're using NTP (default) that means converting
// *              NTP POSIX time and setting DateTime struct. If we're using GPS, the
// *              data just needs to be copied over
// *
// */
//void TIME_UpdateNTPTimeStruct(){
//
//    _update_dt_struct_from_ntp();
//    g_sTimeInfo.ulLastTimestamp = g_sTimeInfo.ulElapsedSec1900;
//
//    // set the time we first got a time as a baseline reference for device time active
//    if (g_sTimeInfo.ulFirstTimestamp == 0){
//        g_sTimeInfo.ulFirstTimestamp = g_sTimeInfo.ulElapsedSec1900 - g_sTimeInfo.ulSecActive;
//    }
//
//    // Seconds Active is perfectly atomic
//    else{
//        g_sTimeInfo.ulSecActive = g_sTimeInfo.ulElapsedSec1900 - g_sTimeInfo.ulFirstTimestamp;
//    }
//}

/***************************************************************************
 *
 * Update the local date and time using elapsed seconds since 1900 variable
 *
 * NOTE: Doesn't update 'sSNTPData.ulElapsedSec1900' using external reference.
 *          That should be done through TIME_Sync()
 *
 ****************************************************************************/
static void _update_dt_struct_from_ntp()
{
    int iIndex;
    short isGeneralVar;
    unsigned long ulGeneralVar,ulGeneralVar1,ulElapsedSecTmp;

    // Just a random known reference that has already passed (updated just below)
    g_sTimeInfo.datetime.uisYear = YEAR2017;

    ulElapsedSecTmp = g_sSNTPDateTime.ulElapsedSec1900;

    // seconds are relative to 00:00 on January 1, 1900
    ulElapsedSecTmp -= TIME2017;

    // Find current year
    while(ulElapsedSecTmp>SEC_IN_LEAP_YEAR)
    {
        g_sTimeInfo.datetime.uisYear++;
        if(g_sTimeInfo.datetime.uisYear%4==0)
            ulElapsedSecTmp -= SEC_IN_DAY;
        ulElapsedSecTmp -= SEC_IN_YEAR;
    }

    // in order to correct the timezone
    ulElapsedSecTmp -= (GMT_DIFF_TIME_HRS * SEC_IN_HOUR);

//    g_sSNTPDateTime.pcCCPtr = &g_sSNTPDateTime.acTimeStore[0];

    // day, number of days since beginning of 2017
    isGeneralVar = ulElapsedSecTmp / SEC_IN_DAY;
    g_sTimeInfo.datetime.ucDayOfWeek = isGeneralVar % 7;
//    memcpy(g_sSNTPDateTime.pcCCPtr,
//            g_acDaysOfWeek2017[g_sSNTPDateTime.ucDayOfWeek], 3);
//    g_sSNTPDateTime.pcCCPtr += 3;
//    *g_sSNTPDateTime.pcCCPtr++ = '\x20';

    // month
    isGeneralVar %= ((g_sTimeInfo.datetime.uisYear)%4 ? 365 : 366);
    for (iIndex = 0; iIndex < 12; iIndex++) {
        isGeneralVar -= g_acNumOfDaysPerMonth[iIndex];
        if (isGeneralVar < 0)
            break;
    }
    if (iIndex == 12) {
        iIndex = 0;
    }
    g_sTimeInfo.datetime.uisMonth = iIndex;
//    memcpy(g_sSNTPDateTime.pcCCPtr, g_acMonthOfYear[iIndex], 3);
//    g_sSNTPDateTime.pcCCPtr += 3;
//    *g_sSNTPDateTime.pcCCPtr++ = '\x20';

    // date
    // restore the day in current month
    isGeneralVar += g_acNumOfDaysPerMonth[iIndex];
    g_sTimeInfo.datetime.uisDay = isGeneralVar + 1;
//    g_sSNTPDateTime.uisCCLen = itoa(g_sSNTPDateTime.isGeneralVar + 1,
//                                g_sSNTPDateTime.pcCCPtr);
//    g_sSNTPDateTime.pcCCPtr += g_sSNTPDateTime.uisCCLen;
//    *g_sSNTPDateTime.pcCCPtr++ = '\x20';

    // time
    ulGeneralVar = ulElapsedSecTmp % SEC_IN_DAY;

    // number of seconds per hour
    ulGeneralVar1 = ulGeneralVar % SEC_IN_HOUR;

    // number of hours
    ulGeneralVar /= SEC_IN_HOUR;
    g_sTimeInfo.datetime.uisHour = ulGeneralVar;
//    g_sSNTPDateTime.uisCCLen = itoa(ulGeneralVar,
//            g_sSNTPDateTime.pcCCPtr);
//    g_sSNTPDateTime.pcCCPtr += g_sSNTPDateTime.uisCCLen;
//    *g_sSNTPDateTime.pcCCPtr++ = ':';

    // number of minutes per hour
    ulGeneralVar = ulGeneralVar1 / SEC_IN_MIN;

    // number of seconds per minute
    ulGeneralVar1 %= SEC_IN_MIN;
    g_sTimeInfo.datetime.uisMinute = ulGeneralVar;
//    g_sSNTPDateTime.uisCCLen = itoa(g_sSNTPDateTime.ulGeneralVar,
//            g_sSNTPDateTime.pcCCPtr);
//    g_sSNTPDateTime.pcCCPtr += g_sSNTPDateTime.uisCCLen;
//    *g_sSNTPDateTime.pcCCPtr++ = ':';

    g_sTimeInfo.datetime.uisSecond = ulGeneralVar1;

//    g_sSNTPDateTime.uisCCLen = itoa(g_sSNTPDateTime.ulGeneralVar1,
//            g_sSNTPDateTime.pcCCPtr);
//    g_sSNTPDateTime.pcCCPtr += g_sSNTPDateTime.uisCCLen;
//    *g_sSNTPDateTime.pcCCPtr++ = '\x20';

    // year
    // number of days since beginning of 2017
//    g_sSNTPDateTime.uisCCLen = itoa(g_sSNTPDateTime.uisYear,
//            g_sSNTPDateTime.pcCCPtr);
//    g_sSNTPDateTime.pcCCPtr += g_sSNTPDateTime.uisCCLen;
//    *g_sSNTPDateTime.pcCCPtr++ = '\0';

    /*
     *  finally check for daylight savings change. If there's a change
     *  update the UTC time and start over
     *  In the United States:
     *        DST start (adjust clocks forward) : 2nd Sunday, March,    1:59am --> 3:00am
     *        DST end (adjust clocks backward)  : 1st Sunday, November, 1:59am --> 1:00am
     *
     *  NOTE: No longer using DST, because it's dumb and I hate it
     */
}


/***************************************************************************
 *
 * Create date and time (YYYYMMDDHHmmss)
 *
 *  \param [in] ulPOSIX_GMT : POSIX time - seconds since Jan 1, 1900 00:00 GMT
 *  \param [in] time_zone_diff : Timezone offset from GMT.
 *                               (probably want 'GMT_DIFF_TIME_HRS' here)
 *  \param [out] sDT : DateTime struct to be set with the local time values
 *
 *  \NOTE: Does not include daylight savings time, because it is just terrible.
 *
 ****************************************************************************/
void TIME_posix2lt(unsigned long ulPOSIX_GMT, sDateTime* sDT, unsigned char time_zone_diff)
{
    int iIndex;
    short isGeneralVar;
    unsigned long ulGeneralVar,ulGeneralVar1;

    if (ulPOSIX_GMT < TIME2017){
        sDT->uisYear   = 0;
        sDT->uisMonth  = 0;
        sDT->uisDay    = 0;
        sDT->uisHour   = 0;
        sDT->uisMinute = 0;
        sDT->uisSecond = 0;
        return;
    }

    // Just an arbitrary known reference that has already passed
    sDT->uisYear = YEAR2017;

    // seconds are relative to 00:00 on January 1, 1900
    ulPOSIX_GMT -= TIME2017;

    // Find current year
    while(ulPOSIX_GMT>SEC_IN_LEAP_YEAR){
        sDT->uisYear++;
        if(sDT->uisYear%4==0)
            ulPOSIX_GMT -= SEC_IN_DAY;
        ulPOSIX_GMT -= SEC_IN_YEAR;
    }

    // in order to correct the timezone
    ulPOSIX_GMT -= (GMT_DIFF_TIME_HRS * SEC_IN_HOUR);

    // day, number of days since beginning of 2017
    isGeneralVar = ulPOSIX_GMT / SEC_IN_DAY;
    sDT->ucDayOfWeek = isGeneralVar % 7;

    // month
    isGeneralVar %= ((sDT->uisYear)%4 ? 365 : 366);
    for (iIndex = 0; iIndex < 12; iIndex++) {
        isGeneralVar -= g_acNumOfDaysPerMonth[iIndex];
        if (isGeneralVar < 0)
            break;
    }
    iIndex %= 12;
    sDT->uisMonth = iIndex;

    // restore the day in current month
    isGeneralVar += g_acNumOfDaysPerMonth[iIndex];
    sDT->uisDay = isGeneralVar + 1;

    // time
    ulGeneralVar = ulPOSIX_GMT % SEC_IN_DAY;

    // number of seconds per hour
    ulGeneralVar1 = ulGeneralVar % SEC_IN_HOUR;

    // number of hours
    ulGeneralVar /= SEC_IN_HOUR;
    sDT->uisHour = ulGeneralVar;

    // number of minutes per hour
    ulGeneralVar = ulGeneralVar1 / SEC_IN_MIN;

    // number of seconds per minute
    ulGeneralVar1 %= SEC_IN_MIN;
    sDT->uisMinute = ulGeneralVar;

    // finally seconds
    sDT->uisSecond = ulGeneralVar1;

}

/**
 *  \brief: Retrieves the POSIX Time (GMT) from the given sDateTime
 *
 *  \param [in]: sDT - the DateTime struct
 *
 *  \return: the POSIX Time (GMT)
 */
unsigned long TIME_dt2posix(sDateTime* sDT)
{
    int i;
    unsigned long ulPOSIX;

    // days to start of new year (since 1900)
    ulPOSIX = (unsigned long)(sDT->uisYear-1900)*365 + ((sDT->uisYear-1900)/4);

    // add in months (in days) since new year, until start of this month
    for(i=0;i<sDT->uisMonth-1;i++){
        ulPOSIX += g_acNumOfDaysPerMonth[i];
    }

    // add in days up until today
    ulPOSIX += sDT->uisDay-1;

    // convert days to seconds
    ulPOSIX *= 86400;

    // add in hours
    ulPOSIX += sDT->uisHour*3600;

    // add in minutes
    ulPOSIX += sDT->uisMinute*60;

    // add in seconds
    ulPOSIX += sDT->uisSecond;

    return ulPOSIX;
}

/**
 *  \brief: Point the caller's struct to the global time info struct saved here
 *
 *  \param **sTI: pp to caller's struct
 *
 *  \NOTE: example:
 *      sTimeInfo *sTI;
 *      TIME_GetDTStructPtr( &sTI );
 */
void TIME_GetDTStructPtr(sTimeInfo **sTI)
{
    (*sTI) = &g_sTimeInfo;
}

/***************************************************************************
 *
 * Return seconds that the device has been active to caller
 *
 * \NOTE    This is more accurate than SimpleLink time active function
 *          because it is updated from an external reference (NTP or GPS)
 *          whenever TIME_Sync() is called.
 *
 * \return seconds device has been active
 *
 ****************************************************************************/
unsigned long TIME_GetSecondsActive(eTimeSource src)
{
    switch (src){
    case TIME_NTP:
        return (g_sTimeInfo.ulSecActive);
        break;
    case TIME_GPS:
        return (g_sGPSTimeInfo.ulSecActive);
        break;
    case TIME_EITHER:
        // I'm assuming here that larger means more correct. Seems reasonable.
        return (g_sTimeInfo.ulSecActive > g_sGPSTimeInfo.ulSecActive) ? g_sTimeInfo.ulSecActive : g_sGPSTimeInfo.ulSecActive;
        break;
    default:
        return FALSE;
    }
}

/**
 *  \brief: Fill a date string from a given source
 *
 *  \param pDate:  The string to fill (caller is responsible for correct allocation
 *  \param src:    The source of time (GPS/NTP/Either) to use. If Either is chosen,
 *                      the most recently updated source is used.
 *  \param update: \b True: update the given struct using POSIX \b False: Don't do that
 */
void TIME_GetDateFromSource(char* pDate, eTimeSource src, tBoolean update)
{
    sTimeInfo* sTI;

    src = (src == TIME_EITHER) ? TIME_BestTimeSource() : src;

    switch (src){
    case TIME_NTP:
        sTI = &g_sTimeInfo;
        break;

    case TIME_GPS:
        sTI = &g_sGPSTimeInfo;
        break;
    }

    if ( update ){
        TIME_posix2lt(sTI->ulElapsedSec1900, &(sTI->datetime), GMT_DIFF_TIME_HRS);
    }

    TIME_GetDateISO8601(pDate, &(sTI->datetime));

}

/**
 *  \brief: Fill a time string from a given source
 *
 *  \param pTime:  The string to fill (caller is responsible for correct allocation
 *  \param src:    The source of time (GPS/NTP/Either) to use. If Either is chosen,
 *                      the most recently updated source is used.
 *  \param update: \b True: update the given struct using POSIX \b False: Don't do that
 */
void TIME_GetTimeFromSource(char* pTime, eTimeSource src, tBoolean update)
{
    sTimeInfo* sTI;

    src = (src == TIME_EITHER) ? TIME_BestTimeSource() : src;

    switch (src){
    case TIME_NTP:
        sTI = &g_sTimeInfo;
        break;

    case TIME_GPS:
        sTI = &g_sGPSTimeInfo;
        break;
    }

    // Update the date and time struct from the POSIX time, which is just continuously
    //  incremented every second inside the timer interrupt
    if ( update ){
        TIME_posix2lt(sTI->ulElapsedSec1900, &(sTI->datetime), GMT_DIFF_TIME_HRS);
    }

    TIME_GetLocalTime(pTime, &(sTI->datetime));
}

/**
 *  \brief: Get the mose recent time source
 *
 *  \return: most recently updated time source
 */
eTimeSource TIME_BestTimeSource()
{
    return (g_sTimeInfo.ulLastTimestamp > g_sGPSTimeInfo.ulLastTimestamp) ? TIME_NTP : TIME_GPS;
}

/***************************************************************************
 *
 * Return date to user. Format is "YY-MM-DD\0" (9 bytes)
 *
 * \param   string to store date
 *
 ****************************************************************************/
void TIME_GetDateISO8601(char* pDate, sDateTime* sDT)
{
    char *p = pDate;

    p += itoa(sDT->uisYear-2000,p);
    *p++ = '-';
    p += itoa(sDT->uisMonth+1,p);
    *p++ = '-';
    p += itoa(sDT->uisDay,p);
    *p = '\0';
    return;
}

/***************************************************************************
 *
 * Return time to user. Format is "HH:MM:SS\0" (9 bytes)
 *
 * \param   string to store time
 *
 ****************************************************************************/
void TIME_GetLocalTime(char* pTime, sDateTime* sDT)
{
    char *p = pTime;

    p += itoa(sDT->uisHour, p);
    *p++ = ':';
    p += itoa(sDT->uisMinute, p);
    *p++ = ':';
    p += itoa(sDT->uisSecond, p);
    *p = '\0';
    return;
}

/***************************************************************************
 *
 * Check if the system time has been set
 *
 *  \return   1 - System time has been set
 *            0 - System time has not been set
 *
 ****************************************************************************/
tBoolean TIME_Exists(eTimeSource src)
{
    switch (src){
    case TIME_NTP:
        return (g_sTimeInfo.ulElapsedSec1900 > TIME2017);
        break;
    case TIME_GPS:
        return (g_sGPSTimeInfo.ulElapsedSec1900 > TIME2017);
        break;
    case TIME_EITHER:

        return (g_sTimeInfo.ulElapsedSec1900 > TIME2017) |
               (g_sGPSTimeInfo.ulElapsedSec1900 > TIME2017);
        break;
    default:
        return FALSE;
    }

}

unsigned long TIME_GetPosix(eTimeSource src)
{
    switch (src){
    case TIME_NTP:
        return g_sTimeInfo.ulElapsedSec1900;
        break;

    case TIME_GPS:
        return g_sGPSTimeInfo.ulElapsedSec1900;
        break;

    case TIME_EITHER:
        // I'm assuming here that larger means more correct. Seems reasonable. Could only go wrong if an incorrect timestamp is received.
        //     Using 'last time stamp' instead of 'posix' because that could be using the CC3200 clock for an unspecified amount of time,
        //     so could be off because of shitty clock drift.
        return (g_sTimeInfo.ulLastTimestamp > g_sGPSTimeInfo.ulLastTimestamp) ? g_sTimeInfo.ulElapsedSec1900 : g_sGPSTimeInfo.ulElapsedSec1900;
        break;

    default:
        return FALSE;
    }
}

unsigned long TIME_millis()
{
    return g_sTimeInfo.ulMSActive;
}
