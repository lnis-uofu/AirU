/*
 * sd_time_if.h
 *
 *  Created on: Jun 18, 2018
 *      Author: Tom
 */

#ifndef INCLUDE_SD_TIME_IF_H_
#define INCLUDE_SD_TIME_IF_H_

// Standard includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TIME2017    3692217600u      /* 117 years + 29 leap days since 1900 */
#define SEC_IN_DAY  86400

typedef struct
{
    unsigned long ulElapsedSec1900;
    unsigned long ulSecActive;
} sSDTimeInfo;

void sd_time_if_init();
void sd_time_if_get(sSDTimeInfo* ti);
void sd_time_if_set(sSDTimeInfo* ti);

#endif /* INCLUDE_SD_TIME_IF_H_ */
