/*
 * sd_time_if.c
 *
 *  Created on: Jun 18, 2018
 *      Author: Tom
 */

#include "sd_time_if.h"

sSDTimeInfo g_sSDTimeInfo;

void sd_time_if_init()
{
    g_sSDTimeInfo.ulElapsedSec1900 = 0;
    g_sSDTimeInfo.ulSecActive = 0;
}

void sd_time_if_get(sSDTimeInfo* ti)
{
    ti->ulElapsedSec1900 = g_sSDTimeInfo.ulElapsedSec1900;
    ti->ulSecActive = g_sSDTimeInfo.ulSecActive;
}

void sd_time_if_set(sSDTimeInfo* ti)
{
    g_sSDTimeInfo.ulElapsedSec1900 = ti->ulElapsedSec1900;
    g_sSDTimeInfo.ulSecActive = ti->ulSecActive;
}
