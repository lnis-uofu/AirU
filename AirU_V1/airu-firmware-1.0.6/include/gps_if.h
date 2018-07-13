/*
 * gps_if.h
 *
 *  Created on: Jun 5, 2017
 *      Author: Kara
 */

#ifndef GPS_H_
#define GPS_H_

#include "pinmux.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_gpio.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "gpio.h"
#include "prcm.h"
#include "uart.h"

#include "uart_if.h"
#include "gpio_if.h"
#include "device_time_if.h"
#include "app_utils.h"

//Can I use these??
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"

#define GPS             UARTA0_BASE
#define GPS_PERIPH      PRCM_UARTA0
#define GPS_nRST_PIN    10

//
#define ConsoleGetChar()     MAP_UARTCharGet(CONSOLE)
#define ConsolePutChar(c)    MAP_UARTCharPut(CONSOLE,c)
#define GPSGetChar()         MAP_UARTCharGet(GPS)
#define GPSPutChar(c)        MAP_UARTCharPut(GPS,c)

// how long to wait when we're looking for a response
#define MAXWAITSENTENCE 5
//Set GLL output frequency to be outputting once every 1 position fix and RMC to be outputting once every 1 position fix
#define PMTK_API_SET_NMEA_OUTPUT "$PMTK314,1,1,1,1,1,5,0,0,0,0,0,0,0,0,0,0,0,1,0*2D\r\n"
// turn on only the second sentence (GPRMC)
#define PMTK_SET_NMEA_OUTPUT_RMCONLY "$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n"
// turn on GPRMC and GGA
#define PMTK_SET_NMEA_OUTPUT_RMCGGA "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"
// turn on GPGGA
#define PMTK_SET_NMEA_OUTPUT_GGA "$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n"
// turn on ALL THE DATA
#define PMTK_SET_NMEA_OUTPUT_ALLDATA "$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"
// turn off output
#define PMTK_SET_NMEA_OUTPUT_OFF "$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"
// standby command & boot successful message
#define PMTK_STANDBY "$PMTK161,0*28\r\n"
#define PMTK_AWAKE "$PMTK010,002*2D\r\n"
//set RTC UTC time
#define PMTK_API_SET_RTC_TIME "$PMTK335,2007,1,1,0,0,0*02\r\n"
//get RTC UTC time
#define PMTK_API_GET_RTC_TIME "$PMTK435*30\r\n"
#define PMTK_CMD_HOT_START "$PMTK101*32\r\n"
#define PMTK_CMD_WARM_START "$PMTK102*31\r\n"
#define PMTK_CMD_COLD_START "$PMTK103*30\r\n"
#define PMTK_CMD_FULL_COLD_START "$PMTK104*37\r\n"
#define PMTK_CMD_CLEAR_FLASH_AID "$PMTK120*31\r\n"
#define PMTK_CMD_CLEAR_EPO "$PMTK127*36\r\n"

typedef struct
{
    double latitude_degrees;
    double latitude_minutes;
    double longitude_degrees;
    double longitude_minutes;
    double altitude_meters;
}sGlobalPosition;

typedef struct
{
    sGlobalPosition coordinates;
    unsigned char satellites;
    unsigned long ulPOSIX_GMT;
    unsigned char pos_ind;
    float HDOP;
    float geoidheight;
}sGPS_data;

extern void GPS_Init(int);
extern long GPS_SendCommand(const char*);
extern long GPS_GetDateAndTime(char *cDate, char *cTime);
extern void GPS_GetGlobalCoords(sGlobalPosition *coords);
extern double GPS_DMm2Dd(double D, double Mm);
extern long GPS_SetPOSIXFromRTC(tBoolean wait_for_ack);
extern tBoolean GPS_Ready(void);
extern void GPS_ClearReady(void);


//extern void common_init(void);
//extern int newNMEAreceived();
//extern char *lastNMEA(void);
//extern int waitForSentence(const char *);
//extern int standby(void);
//extern int wakeup(void);

#endif /* GPS_H_ */

