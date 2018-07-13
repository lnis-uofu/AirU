/*
 * sd_if.c
 *
 *  Created on: Nov 2, 2017
 *      Author: Tom
 */

#ifndef SRC_SD_IF_C_
#define SRC_SD_IF_C_

// Standard includes
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
#include "hw_common_reg.h"

// Common interface includes
#include "common.h"
#include "sdhost.h"
#include "ff.h"
#include "pinmux.h"

#include "sd_time_if.h"

//#define NOLOG

extern void SD_Init(void);
extern long LOG(const char *pcFormat, ...);
extern void SD_DateFileAppend(char *date, char *csv);
extern long SD_FileRead(char* date, char* buff, size_t len);



#endif /* SRC_SD_IF_C_ */


