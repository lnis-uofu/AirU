/*
 * pms3003.h
 *
 *  Created on: Jan 31, 2017
 *      Author: Tom Becnel <thomas.becnel@utah.edu>
 */

#ifndef PMS3003_H_
#define PMS3003_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define UART1_BAUD_RATE 9600
#define SYSCLK          80000000
#define PMS             UARTA1_BASE
#define PMS_PERIPH      PRCM_UARTA1

typedef enum
{
    PMS_DATA_VALID = 0,
    PMS_DATA_INVALID = -1
} e_PMStatus;

typedef struct sPMData
{
    double PM1,PM2_5,PM10;            // Holds averaged PM samples
}sPMData;

typedef struct sPMTest
{
    double PM2_avg;
    _u16 fail_cnt;
    _i16 PM2_last;
} sPMTest;

extern void PMS_Init(void);
extern void PMS_Tally(void);
extern long PMS_GetPMData(sPMData* dat);
extern long PMS_DEBUGPM(sPMTest* dat);
extern long PMS_GetPMDataWithDBG(sPMData*, sPMTest*);
extern void PMS_Enable(void);
extern void PMS_Disable(void);
extern void PMS_ITOARingBuffer(_u8* p);
extern void PMS_DEBUG_ITOARingBuffer(_i8* str);

#ifdef __cplusplus
}
#endif

#endif /* PMS3003_H_ */
