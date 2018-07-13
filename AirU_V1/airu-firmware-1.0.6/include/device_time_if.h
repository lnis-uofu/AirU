/*
 * device_time_if.h
 *
 *  Created on: Sep 24, 2017
 *      Author: Tom
 */

#ifndef INCLUDE_DEVICE_TIME_IF_H_
#define INCLUDE_DEVICE_TIME_IF_H_

#include "hw_types.h"
#include "hw_memmap.h"
#include "prcm.h"
#include "timer.h"
#include "internet_if.h"
#include "common.h"
#include "sd_if.h"

// NTP Time #defines
#define TIME2017                3692217600u      /* 117 years + 29 leap days since 1900 */
#define YEAR2017                2017
#define SEC_IN_MIN              60
#define SEC_IN_HOUR             3600
#define SEC_IN_DAY              86400
#define SEC_IN_YEAR             31536000u
#define SEC_IN_LEAP_YEAR        31622400u
#define GMT_DIFF_TIME_HRS       7
#define SERVER_RESPONSE_TIMEOUT 10

typedef enum
{
    TIME_NTP    = 0,
    TIME_GPS    = 1,
    TIME_EITHER = 2
} eTimeSource;

typedef struct
{
    unsigned char  ucDayOfWeek;
    unsigned short uisYear;
    unsigned short uisMonth;
    unsigned short uisDay;
    unsigned short uisHour;
    unsigned short uisMinute;
    unsigned short uisSecond;
} sDateTime;

typedef struct
{
    unsigned long ulElapsedSec1900;
    unsigned long ulFirstTimestamp;
    unsigned long ulMSActive;
    unsigned long ulSecActive;
    unsigned long ulLastTimestamp;
    sDateTime datetime;
} sTimeInfo;

int TIME_SetSLDeviceTime(void);

void TIME_TimerIntHandler(void);
void TIME_InitHWTimer(void);
//void TIME_UpdateNTPTimeStruct(void);
void TIME_UpdateSystemTimeFromPOSIX(unsigned long ulElapsedSec1900, eTimeSource src);
void TIME_GetLocalTime(char* pTime, sDateTime* sDT);
void TIME_GetDateISO8601(char* pDate, sDateTime* sDT);
void TIME_posix2lt(unsigned long ulPOSIX_GMT, sDateTime* sDT, unsigned char time_zone_diff);
void TIME_GetDateFromSource(char* pDate, eTimeSource src, tBoolean update);
void TIME_GetTimeFromSource(char* pTime, eTimeSource src, tBoolean update);
void TIME_GetDTStructPtr(sTimeInfo **sTI);

long TIME_Sync(void);
long TIME_SyncNTP(void);
long TIME_SyncGPS(void);
long TIME_SetGPSStruct(char* dt_str);

tBoolean TIME_Exists(eTimeSource);

eTimeSource TIME_BestTimeSource(void);

unsigned long TIME_dt2posix(sDateTime* sDT);
unsigned long TIME_GetSecondsActive(eTimeSource);
unsigned long TIME_GetPosix(eTimeSource);
unsigned long TIME_millis(void);
unsigned long TIME_SecondsSinceUpdate(eTimeSource src);

#endif /* INCLUDE_DEVICE_TIME_IF_H_ */
