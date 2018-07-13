/*
 * gps_if.c
 *
 *  Created on: Jun 5, 2017
 *      Author: Tom
 */

#include "gps_if.h"
#include "utils.h"
#include "uart_if.h"
#include "app_utils.h"
#include "common.h"     /* to use UART_PRINT() */

//*****************************************************************************

// how long are max NMEA lines to parse?
#define MAXLINELENGTH 500
#define TIME2017                3692217600u      /* 117 years + 29 leap days since 1900 */
#define YEAR2017                2017

//*****************************************************************************
//
//! Local functions declaration
//
//*****************************************************************************
static void _GPSIntHandler(void);
static long _receive_packet(char* packet, unsigned int i);
static long _parse_packet(char* nmea);
static int _checksum(char *nmea);

//*****************************************************************************
//
//! Global variables declaration
//
//*****************************************************************************
unsigned char g_ucGPSRingBuf[MAXLINELENGTH];
unsigned char g_ucGPSReadyFlag;
static unsigned char g_RTCDataReady = 0;
char packet[MAXLINELENGTH];
sGPS_data g_sGPS_data;

//*****************************************************************************

//*****************************************************************************
//
//! Interrupt handler for UART interupt
//!
//! \param  None
//!
//! \return None
//!
//! TODO: How to handle multiple good transmissions in the ring buffer?
//!         Right now it will only parse the LATEST good transmission.
//!         We would need a queue of pointers to the '$' and a function
//!             that's always parsing them (and a big ring buffer)
//!
//*****************************************************************************
static void _GPSIntHandler()
{
    static unsigned char ucRingBufIndex = 0;
    static char bFoundMoney = 0;
    static unsigned int uiTransStart = 0;
    long lRetVal = -1;
    char cChar;
    //
    // Clear the UART Interrupt
    //
    MAP_UARTIntClear(GPS,UART_INT_RX);

    cChar = MAP_UARTCharGetNonBlocking(GPS);
    g_ucGPSRingBuf[ucRingBufIndex] = cChar;

    // if we found the start of a transmission then mark its index
    // TODO: Do I want to add ( && !bFoundMoney) -- probably not
    if(cChar=='$')
    {
        bFoundMoney = 1;
        uiTransStart = (int)ucRingBufIndex;
    }

    // if we found the start and end of a transmission parse the message
    if(cChar=='\n' && bFoundMoney) // '\n'
    {

        bFoundMoney = 0;
        lRetVal = _receive_packet(packet,uiTransStart);

        if (lRetVal == 0) {
            lRetVal = _parse_packet(packet);
            memset(packet,0,MAXLINELENGTH);
        }
    }

    ucRingBufIndex++;
    if (ucRingBufIndex == MAXLINELENGTH)
    {
        // TODO: these should be rolled into a state machine
        //    --> found '$' --> found '\r\n' --> parse -->
        ucRingBufIndex = 0;
    }


}

//*****************************************************************************
//
//!    Initialize the GPS
//!
//! This function
//!         1. intializes UART0 at 9600 baud
//!         2. short delay while GPS warms up
//!         3. TODO: Interrupt
//!
//! \return none
//
//*****************************************************************************
void GPS_Init(int iGPS_NO_INT){
    //
    // Configure GPS clock (9600)
    //
    MAP_UARTConfigSetExpClk(GPS,MAP_PRCMPeripheralClockGet(GPS_PERIPH),
                    UART_BAUD_RATE, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                     UART_CONFIG_PAR_NONE));

    if(!iGPS_NO_INT){
        //
        // Register interrupt handler for UART0
        //
        MAP_UARTIntRegister(GPS, _GPSIntHandler);
        //
        // Enable UART0 Rx not empty interrupt
        //
        MAP_UARTIntEnable(GPS,UART_INT_RX);
    }
    //
    // Configure the UART0 Tx and Rx FIFO level to 1/8 i.e 2 characters
    //
    UARTFIFODisable(GPS);
    //
    // Turn off RESET
    //
    GPIO_IF_Set(GPS_nRST_PIN,GPIOA1_BASE,0x4,1);

    g_ucGPSReadyFlag = 0;
    //
    // Send GPS the structure we'd like
    //
//    GPS_SendCommand(PMTK_SET_NMEA_OUTPUT_GGA);
//    GPS_sendCommand(PMTK_SET_NMEA_OUTPUT_OFF);
}

//*****************************************************************************
//
//!    Sends a NMEA Sentence to configure GPS
//!
//! This function sends an NMEA sentence over UART0 (Nonblocking)
//!
//! \return  >=0: Number of bytes not put into FIFO, -1: NULL/too large packet
//!                 -2: No start command received (2 packets needed)
//
//*****************************************************************************
long
GPS_SendCommand(const char *packet){
    long sum = 0;
    unsigned short bytes;
    MAP_UARTFIFODisable(GPS);
    MAP_UARTFIFOEnable(GPS);

    if(g_ucGPSReadyFlag< 2){
        return -2;
    }

    if(packet != NULL && g_ucGPSReadyFlag >= 2)
    {
        bytes = 0;
        while(*packet!='\0')
        {
            sum += !MAP_UARTCharPutNonBlocking(GPS,*packet++);
            if(++bytes == MAXLINELENGTH) return -1;
        }
        return sum;
    }
    else{
        return -1;
    }

}

//! bad packet: -1
// \param i: ring buffer pointer (start of transmission '$')
static long _receive_packet(char *packet, unsigned int i)
{
    int j = 0;          // packet pointer
    char cChar = '$';   // character from ring buffer

    // copy packet up to '*'
    while(cChar != '\n')
    {
        packet[j++] = cChar;
        if (++i==MAXLINELENGTH) i=0;          // loop back to start of buffer
        if (j==MAXLINELENGTH) return -1;    // filled up packet array, it's not good
        cChar = g_ucGPSRingBuf[i];
    }
    packet[j++] = '\n';
    packet[j] = '\0';

    return _checksum(packet);
}

tBoolean GPS_Ready()
{
    return !!g_ucGPSReadyFlag;
}


//*****************************************************************************
//
//!    Calculates the NMEA Sentence Checksum
//!
//! Checksum is all bytes XOR'd together
//!
//! \return SUCCESS (0): checksum matches | FAILURE (-1): checksum doesn't match
//
//*****************************************************************************
static int _checksum(char *nmea){

    if (nmea[strlen(nmea)-5] == '*'){
        unsigned char sum = parseHex(nmea[strlen(nmea)-4]) * 16;
        sum += parseHex(nmea[strlen(nmea)-3]);

        // check checksum
        int i;
        for (i = 1; i < (strlen(nmea)-5); i++){
            sum ^= nmea[i];
        }
        return (!!sum)*(-1);
    }
    else{
        return FAILURE;
    }
}

//*****************************************************************************
//
//!    Parse the NMEA Sentence
//!
//! This function
//!         1. Can parse $GPGGA Sentences
//!         2. CANNOT parse any other sentence
//!             Needs functionality to parse $GPRMC sentences if date is needed
//!             However, RMCGGA command is giving bad RMC data at the moment
//!                 (5/30/17)
//!
//!     TODO: $GPRMC parsing functionality
//!
//! \return lRetVal 0: no error | -1: error
//
//*****************************************************************************
static long _parse_packet(char *nmea) {

    long lRetVal = -1;
    unsigned long ulPOSIX;
    char coord_buff[8];

    //
    // $GPGGA:
    //     time hhmmss.sss |  lat  |      long    |pos|sats|HDOP|altit|geo-sep| cs |
    // $GPGGA,064951.000,2307.1256,N,12016.4438,E,  1,  8,  0.95,39.9,M,17.8,M,,*65
    //
    if (strstr(nmea, "$GPGGA")) {
        // found GGA

        char *p = nmea;
        // get time
        p = strchr(p, ',')+1;
        float timef = atof(p);
        int time = timef;

        // parse out latitude (DDmm.mmmm)
        p = strchr(p, ',')+1;

        // make sure the data exists
        if (',' != *p){
            // parse latitude degrees (DD)
            strncpy(coord_buff, p, 2);
            coord_buff[2] = '\0';
            g_sGPS_data.coordinates.latitude_degrees = atof(coord_buff);

            // parse latitude minutes (mm.mmmm)
            p += 2;
            strncpy(coord_buff,p,7);
            coord_buff[7] = '\0';
            g_sGPS_data.coordinates.latitude_minutes = atof(coord_buff);
        }

        // get latitude N/S
        p = strchr(p, ',')+1;
        if (',' != *p){
            if (p[0] == 'S') g_sGPS_data.coordinates.latitude_degrees *= -1;
            else if (p[0] != 'N') return -1;
        }

        // parse out longitude (DDDmm.mmmm)
        p = strchr(p, ',')+1;
        if (',' != *p){
            // parse longitude (DDD)
            strncpy(&coord_buff[0], p, 3);
            coord_buff[3] = '\0';
            g_sGPS_data.coordinates.longitude_degrees = atof(coord_buff);

            // parse longitude minutes (mm.mmmm)
            p += 3;
            strncpy(coord_buff, p, 7); // minutes
            coord_buff[7] = '\0';
            g_sGPS_data.coordinates.longitude_minutes = atof(coord_buff);
        }

        // longitude E/W
        p = strchr(p, ',')+1;
        if (',' != *p){
            if (p[0] == 'W') g_sGPS_data.coordinates.longitude_degrees *= -1;
            else if (p[0] != 'E') return -1;
        }

        // Position Indicator
        //      0: Fix not available
        //      1: GPS fix
        //      2: Differential GPS fix
        p = strchr(p, ',')+1;
        if (',' != *p){
            g_sGPS_data.pos_ind = atoi(p);
        }

        // Satellites used
        p = strchr(p, ',')+1;
        if (',' != *p){
            g_sGPS_data.satellites = atoi(p);
        }

        // Horizontal dilution of precision
        p = strchr(p, ',')+1;
        if (',' != *p){
            g_sGPS_data.HDOP = atof(p);
        }

        // Altitude (meters)
        p = strchr(p, ',')+1;
        if (',' != *p){
            g_sGPS_data.coordinates.altitude_meters = atof(p);
        }

        // Geoidal Separation (meters)
        p = strchr(p, ',')+1;
        p = strchr(p, ',')+1;
        if (',' != *p){
            g_sGPS_data.geoidheight = atof(p);
        }

        return 0;
    }

    //
    // $GPRMC
    //
    else if (strstr(nmea, "$GPRMC")) {
        //Report("Found $GPRMC Header...\n\r");
        return -1;
    }

    //
    // $PMTK535 (RTC UTC Time)
    // ex) $PMTK535,2017,7,1,23,7,59*09\r\n
    //
    else if(strstr(nmea, "$PMTK535")){

        sDateTime sDT;

        char *p = nmea;
        // seek first comma
        p = strchr(p, ',')+1;

        // get year
        unsigned short year = atoi(p);
        if(year>3017) return -1;
        sDT.uisYear = year;

        // get month
        p = strchr(p, ',')+1;
        unsigned short month = atoi(p);
        if(month>12) return -1;
        sDT.uisMonth = month;

        // get day
        p = strchr(p, ',')+1;
        unsigned short day = atoi(p);
        if(day>31) return -1;
        sDT.uisDay = day;

        // get hour
        p = strchr(p, ',')+1;
        unsigned short hour = atoi(p);
        if(hour>23) return -1;
        sDT.uisHour = hour;

        // get minute
        p = strchr(p, ',')+1;
        unsigned short minute = atoi(p);
        if(minute>59) return -1;
        sDT.uisMinute = minute;

        // get second
        p = strchr(p, ',')+1;
        unsigned short sec = atoi(p);
        if(sec>59) return -1;
        sDT.uisSecond = sec;

        // Get time elapsed since January 1, 1900 00:00
        g_sGPS_data.ulPOSIX_GMT = TIME_dt2posix(&sDT);

//        // Update the references held in the TIME library. These can be used for
//        //      seconds elapsed, seconds active, printing datetime, etc;
        TIME_UpdateSystemTimeFromPOSIX(g_sGPS_data.ulPOSIX_GMT,TIME_GPS);

        // Signal that the deed has been done
        g_RTCDataReady = 1;

        return SUCCESS;
    }
//    $PMTK011,MTKGPS*08<CR><LF>
//    $PMTK010,001*2E<CR><LF>
    else if (strstr(nmea, "$PMTK011,MTKGPS*08")){
        g_ucGPSReadyFlag = (g_ucGPSReadyFlag<2) ? g_ucGPSReadyFlag++ : 2;
        return SUCCESS;
    }

    else if (strstr(nmea,"$PMTK010,001*2E")){
        g_ucGPSReadyFlag = (g_ucGPSReadyFlag<2) ? g_ucGPSReadyFlag++ : 2;
        return SUCCESS;
    }

    //
    // didn't find good header
    //
    else{
        //Report("No GPS header found...\n\r");
        return FAILURE;
    }
}

/*********************************************************
 *
 *  \brief: Point caller's struct to struct saved here
 *  \param **coords: pp to GPS coordinate struct
 *
 *  \NOTE: example:
 *      sGlobalPosition *sGP;
 *      GPS_GetGlobalCoords( &sGP );
 *
 ********************************************************/
void GPS_GetGlobalCoords(sGlobalPosition *coords)
{
    coords->altitude_meters = g_sGPS_data.coordinates.altitude_meters;
    coords->latitude_degrees = g_sGPS_data.coordinates.latitude_degrees;
    coords->latitude_minutes = g_sGPS_data.coordinates.latitude_minutes;
    coords->longitude_degrees = g_sGPS_data.coordinates.longitude_degrees;
    coords->longitude_minutes = g_sGPS_data.coordinates.longitude_minutes;

    //(*coords) = &(g_sGPS_data.coordinates);
}

/*********************************************************
 *
 * Convert DM.m coordinates to D.d coordinates.
 * Current GPS module returns coordinates in DM.m,
 * but we'd like to publish them as D.d
 *
 * \param   D portion of DM.m
 * \param   M.m portion of DM.m
 *
 * \return  coordinate in D.d format
 ********************************************************/
double GPS_DMm2Dd(double D, double Mm)
{
    return (SGN(D)) * ( fabs(D) + ( Mm / 60 ) );
}

/**
 *  \brief: Retrieve RTC packet and store it in
 *              the datetime struct
 *
 *  \return: 0: SUCCESS, -1: FAILURE
 *
 */
long GPS_SetPOSIXFromRTC(tBoolean wait_for_ack)
{
    long lRetVal;
    unsigned short cnt;
    g_RTCDataReady = 0;

    lRetVal = GPS_SendCommand(PMTK_API_GET_RTC_TIME);

    if (lRetVal > 0) return FAILURE;

    if ( wait_for_ack ) {
        cnt = 0;

        while(!g_RTCDataReady && ++cnt<300 /* ONE SECOND */){
            osi_Sleep(10);
        }

        UART_PRINT("\tDONE\n\r");
        return g_RTCDataReady ? SUCCESS : FAILURE;
    }
    else return SUCCESS;
}

void GPS_ClearReady()
{
    g_ucGPSReadyFlag = 0;
}






