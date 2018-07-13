/*
 * pms3003.c
 *
 *  Created on: Jan 31, 2017
 *      Author: Tom Becnel <thomas.becnel@utah.edu>
 */


/*
 * TODO: PM 10 last packet
 *       Push # failures in interval (as PM1)
 *       Average as double PM2.5
 */
// Standard includes
#include <device_time_if.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "prcm.h"
#include "pin.h"
#include "uart.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "interrupt.h"
#include "uart_if.h"
#include "gpio_if.h"
#include "common.h"
#include "app_utils.h"

#if defined(USE_FREERTOS) || defined(USE_TI_RTOS)
#include "osi.h"
#endif

// Local includes
#include "pms_if.h"

#define GPIO_PM_RST      8
#define PM_PKT_LEN      24
#define PM01_HIGH        4
#define PM01_LOW         5
#define PM2_5_HIGH       6
#define PM2_5_LOW        7
#define PM10_HIGH        8
#define PM10_LOW         9
#define TIMEOUT_PERIOD   50
#define NUM_PKT          5


//*****************************************************************************
//
//! Begin Global Parameters
//
//*****************************************************************************
typedef struct _spmdata {
    _u16 _cnt;                      // (private) Number of samples
    _u32 _sum01,_sum02,_sum10;      // (private) Sum of samples
    _u16 _last_pm2;                 // (private) latest good PM10 measurement
    _u16 _fail_cnt;                 // (private) num failures since wiping struct (updated by Tally())
}_spmdata;

unsigned int  g_uiPMRSTPort;
unsigned char g_ucPMRSTPin;
unsigned char last_sample[24];
_spmdata g_spmdata;
sPMData g_sPMData;

unsigned char g_rb[NUM_PKT][PM_PKT_LEN];
unsigned char r_ptr;

static void _buf_parse(_u8* pm_buf);
static _i8 _find_last_good_pm();
static void _srst();
//static void _set_pm_struct(unsigned char *buf);
//static void _reset_pm_struct();
static _u8 _checksum(_u8* buf);

//*****************************************************************************
//
//! End Global Parameters
//
//*****************************************************************************

/**
 * @brief: Interrupt handler for UART connected to PM sensor
 *          We handle packets in a 2D ring buffer, 1 packet per row.
 *          The more rows, the further back in time we can check for
 *          a valid packet.
 *
 */
static void PMSIntHandler()
{
    static _u8 recv = 0;
    static _u8 c = 0;
    static _u8 c_ptr = 0;
    static _u32 timer = 0;

    c = MAP_UARTCharGetNonBlocking(PMS);
    UART_PRINT("%c",c);

    // Start the transmission. Transmission will be shut off
    //  if 24 bytes are reached or timeout occurs
    if(c=='B' && !recv){
        recv = 1;                   // Start of transmission flag
        g_rb[r_ptr][c_ptr++] = c;   // Set the 'B'
        timer = TIME_millis();      // Start the timer
    }
    else if(recv){
        g_rb[r_ptr][c_ptr++] = c;
    }

    // Handle the end of transmission
    //  - Increment row pointer
    //  - Reset column pointer
    //  - Deactivate the transmission flag
    if( c_ptr==PM_PKT_LEN || ( recv && ((TIME_millis()-timer)>50)) ){

        GPIO_IF_LedOff(MCU_STAT_2_LED_GPIO);
        r_ptr = (r_ptr+1) % NUM_PKT;
        c_ptr = 0;
        recv = 0;
    }

   // Clear ISR at end of handler. We may (unlikely) miss a char
   //   but it's safer than letting the flags build up
    MAP_UARTIntClear(PMS,UART_INT_RX);
}


/*
 * Find the last good pm value and add it to the running total
 */
void PMS_Tally(){
    _i8 row = _find_last_good_pm();

    if(row>-1){
        _buf_parse(&g_rb[row][0]);
    }
    else{
        g_spmdata._fail_cnt++;
    }
}

static _u8 _checksum(_u8* buf)
{
    _u16 check_sum;
    _u16 sum;
    _u8 i;

    if(!(buf[0]=='B'&&buf[1]=='M')){
        return 0;
    }

    check_sum = ( (_u16) buf[PM_PKT_LEN-2] ) << 8; // TODO
    check_sum += (_u16) buf[PM_PKT_LEN-1];

    sum = 0;
    for(i=0;i<PM_PKT_LEN-2;i++){
        sum += buf[i];
    }

    return (sum == check_sum);
}
/*
 * Caller must appropriate space for p
 */
void PMS_ITOARingBuffer(_u8* str){
    _u8 tmp[5];
    _u8* p = str;
    int r,c;

    p = strcat(p,"START\n\r");
    for(r=0;r<NUM_PKT;r++){
        for(c=0;c<PM_PKT_LEN;c++){
            sprintf(tmp,"%d-",g_rb[r][c]);
            p = strcat(p,tmp);
        }
        p = strcat(p,"\n\r");
    }
    p = strcat(p,"STOP\n\r");
}

void PMS_DEBUG_ITOARingBuffer(_i8* str){
    _i8 tmp[5];
    _i8* p = str;
    int r,c;

    for(r=0;r<NUM_PKT;r++){

        if(r==r_ptr){
            p = strcat(p,"->");
        }
        else{
            p = strcat(p,"  ");
        }

        for(c=0;c<PM_PKT_LEN;c++){
            sprintf(tmp,"%d-",g_rb[r][c]);
            p = strcat(p,tmp);
        }

        str[strlen(str)-1] = 0; // get rid of that last '-' it's uggo
        p = strcat(p,"\n\r");
    }
}
/**
 *  @brief: Pull the PMS RESET pin LOW
 *
 */
void PMS_Disable()
{
    GPIO_IF_Set(GPIO_PM_RST, g_uiPMRSTPort, g_ucPMRSTPin, 0);
}

/**
 *  @brief: Pull the PMS RESET pin HIGH
 *
 */
void PMS_Enable()
{
    GPIO_IF_Set(GPIO_PM_RST, g_uiPMRSTPort, g_ucPMRSTPin, 1);
}

long PMS_DEBUGPM(sPMTest* dat)
{
    _i32 rc;

    dat->fail_cnt = g_spmdata._fail_cnt;

    if(g_spmdata._cnt>0)
    {
        dat->PM2_avg = (double)g_spmdata._sum02 / (double)g_spmdata._cnt;
        dat->PM2_last = g_spmdata._last_pm2;
        rc = SUCCESS;
    }
    else
    {
        dat->PM2_avg = -1;
        dat->PM2_last = -1;
        rc = FAILURE;
    }

    _srst();

    return rc;
}

/**
 *  @brief: Retrieve the PM Data struct
 *
 */
long PMS_GetPMData(sPMData *dat)
{
    long rc;

    if (g_spmdata._cnt > 0){
        dat->PM1   = (double)g_spmdata._sum01 / (double)g_spmdata._cnt;
        dat->PM2_5 = (double)g_spmdata._sum02 / (double)g_spmdata._cnt;
        dat->PM10  = (double)g_spmdata._sum10 / (double)g_spmdata._cnt;
        rc = SUCCESS;
    }
    else{
        dat->PM1   = -1;
        dat->PM2_5 = -1;
        dat->PM10  = -1;
        rc = FAILURE;
    }

    _srst();

    return rc;
}

long PMS_GetPMDataWithDBG(sPMData *dat, sPMTest *test)
{
    long rc;

    if (g_spmdata._cnt > 0){

        // Real Data
        dat->PM1   = (double)g_spmdata._sum01 / (double)g_spmdata._cnt;
        dat->PM2_5 = (double)g_spmdata._sum02 / (double)g_spmdata._cnt;
        dat->PM10  = (double)g_spmdata._sum10 / (double)g_spmdata._cnt;

        // Test Data
        test->PM2_avg = (double)g_spmdata._sum02 / (double)g_spmdata._cnt;
        test->PM2_last = g_spmdata._last_pm2;

        rc = SUCCESS;
    }
    else{
        // Real data
        dat->PM1   = -1;
        dat->PM2_5 = -1;
        dat->PM10  = -1;

        // Test data
        test->PM2_avg = -1;
        test->PM2_last = -1;
        rc = FAILURE;
    }

    _srst();
}


/**
 *  @brief:
 *      1. Configures the UART to be used.
 *      2. Sets up UART Interrupt and ISR
 *      3. Disables UART FIFO
*/
void PMS_Init()
{

    // Zero out the 2-D array, and reset the pointer
    memset(g_rb, 0, sizeof g_rb);
    r_ptr = 0;

    // Configure Core Clock for UART1 (PMS)
    MAP_UARTConfigSetExpClk(PMS,MAP_PRCMPeripheralClockGet(PMS_PERIPH),
                     UART1_BAUD_RATE, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                     UART_CONFIG_PAR_NONE));

    // Register interrupt handler for UART
    MAP_UARTIntRegister(PMS,PMSIntHandler);

    // Enable UART Rx not empty interrupt
    MAP_UARTIntEnable(PMS,UART_INT_RX);

    //UARTFIFOLevelSet(PMS,UART_FIFO_TX1_8,UART_FIFO_RX7_8);//TODO
    UARTFIFODisable(PMS);

    GPIO_IF_GetPortNPin(GPIO_PM_RST,&g_uiPMRSTPort,&g_ucPMRSTPin);
    PMS_Enable();
}

static _i8 _find_last_good_pm()
{
    _u8 cnt;
    _u8 row;

    cnt = 0;
    do{
        row = (_u8)(r_ptr-cnt) % NUM_PKT;
        if(_checksum(&g_rb[row][0])){
            return (_i8) row;
        }
    } while(++cnt<NUM_PKT);

    return -1;

}

/**
 *  @brief: Fill the PM data struct with values from buf
 *
 *  @param [in]: buf - PM data packet
 *
 *  @note: Ensure the PM data packet passes checksum
 *          before calling this function
 */
static void _buf_parse(_u8* pm_buf)
{
    g_spmdata._cnt++;

    g_spmdata._sum01 += (((_u16)pm_buf[PM01_HIGH])  * 256) + ((_u16)pm_buf[PM01_LOW]);
    g_spmdata._sum02 += (((_u16)pm_buf[PM2_5_HIGH]) * 256) + ((_u16)pm_buf[PM2_5_LOW]);
    g_spmdata._sum10 += (((_u16)pm_buf[PM10_HIGH])  * 256) + ((_u16)pm_buf[PM10_LOW]);

    g_spmdata._last_pm2 = (((_u16)pm_buf[PM2_5_HIGH]) * 256) + ((_u16)pm_buf[PM2_5_LOW]);

    memset(pm_buf,0,PM_PKT_LEN);

    // Fail safe to avoid overflow
    if (g_spmdata._cnt == 10000){
        _srst();
    }
}

/**
 * @brief: Reset PM data flags
 *
 */
static void _srst()
{
    g_spmdata._cnt   = 0;
    g_spmdata._sum01 = 0;
    g_spmdata._sum02 = 0;
    g_spmdata._sum10 = 0;
    g_spmdata._last_pm2 = -1;
    g_spmdata._fail_cnt = 0;
}


