/*
 * sd_if.c
 *
 *  Created on: Nov 2, 2017
 *      Author: Tom
 */

#include "sd_if.h"
#include "uart_if.h"
//#include "sd_time_if.h"


FIL fp;
FIL fp_log;
FATFS fs;
FRESULT res;
DIR dir;

const char* header = "Time,MAC,PM1,PM2.5,PM10,Temp,Hum,Lat,Lon,Alt,CO,NO,Uploaded\n";
int btw_header;


void SD_Init() {

    btw_header = strlen(header);

    // Set the SD card clock as output pin
    MAP_PinDirModeSet(PIN_07, PIN_DIR_MODE_OUT);
    // Enable Pull up on data
    MAP_PinConfigSet(PIN_06, PIN_STRENGTH_4MA, PIN_TYPE_STD_PU);
    // Enable Pull up on CMD
    MAP_PinConfigSet(PIN_08, PIN_STRENGTH_4MA, PIN_TYPE_STD_PU);
    // Enable MMCHS
    MAP_PRCMPeripheralClkEnable(PRCM_SDHOST, PRCM_RUN_MODE_CLK);
    // Reset MMCHS
    MAP_PRCMPeripheralReset(PRCM_SDHOST);
    // Configure MMCHS
    MAP_SDHostInit(SDHOST_BASE);
    // Configure card clock
    MAP_SDHostSetExpClk(SDHOST_BASE, MAP_PRCMPeripheralClockGet(PRCM_SDHOST),15000000);
}

#ifdef NOLOG
long LOG(const char *pcFormat, ...)
{
    return 0;
}
#else
long LOG(const char *pcFormat, ...)
{
    int iRet = 0;
    int tmp = 0;
    char *pcBuff, *pcTemp, *pcStart;
    int iSize = 256;
    char fn[25];
    UINT Size;
    UINT rc;
    va_list list;
    sSDTimeInfo ti;

    pcBuff = (char*)malloc(iSize);

    pcStart = pcBuff;
    if(pcBuff == NULL)
    {
      return -1;
    }

    sd_time_if_get(&ti);
    Size = sprintf(pcBuff,"[%lu | %lu] ", ti.ulElapsedSec1900, ti.ulSecActive);
    pcBuff += Size;

    while(1)
    {
        va_start(list,pcFormat);
        iRet = vsnprintf(pcBuff,iSize,pcFormat,list);
        va_end(list);
        if(iRet > -1 && iRet < iSize)
        {
            break;
        }
        else
        {
            iSize*=2;
            if((pcTemp=realloc(pcBuff,iSize))==NULL)
            {
              Message("Could not reallocate memory\n\r");
              iRet = -1;
              break;
            }
            else
            {
              pcBuff=pcTemp;
            }
        }
    }
    Message(pcStart);
    Message("\r\n");

    rc = f_mount(&fs, "", 1);
    if (res == FR_OK)
    {
        memset(fn,0,25);

        sd_time_if_get(&ti);
        if (ti.ulElapsedSec1900 > TIME2017)
        {
            sprintf(fn, "%lu", ti.ulElapsedSec1900 / SEC_IN_DAY);
        }
        else
        {
            strcpy(fn,"NODATE");
        }

        strcat(fn,".log");
        rc = f_append(&fp_log, fn);
        if (res == FR_OK)
        {
            rc = f_write(&fp_log, pcStart ,strlen(pcStart),&Size);
            rc = f_close(&fp_log);
        }
    }
    else
    {
        UART_PRINT("[ERROR] LOG: Could not write/create log file...\n\r");
        free(pcStart);
        return FAILURE;
    }

    free(pcStart);

    return SUCCESS;
}
#endif

void SD_DateFileAppend(char *date, char *csv)
{
    char readTest;
    UINT Size;
    res = f_mount(&fs, "", 1);    /* mount the sD card */

    /* Open or create a log file and ready to append */
    if (res == FR_OK)   // mounted
    {
        strcat(date,".csv\0");

        // Is file empty? If so we need to add a header
        res = f_open(&fp, date, (FA_READ | FA_OPEN_ALWAYS));
        if (res == FR_OK)
        {
            // test if file is empty
            readTest = 0;
            res = f_read(&fp, &readTest, 1, &Size);
            res = f_close(&fp);
        }

        res = f_append(&fp, date);
        if (res == FR_OK)
        {
            if(!readTest)  // if the file was empty add a header
            {
                res = f_write(&fp,header,btw_header,&Size);
            }

            res = f_write(&fp,csv,strlen(csv),&Size);   /* Append a datapoint */

            res = f_close(&fp);                         /* Close the file */
        }
        else
        {
            UART_PRINT("\tCouldn't open file...\n\r");
        }
    }
}

long SD_FileRead(char* date, char* buff, size_t len)
{
    UINT Size;
    res = f_mount(&fs, "", 1);    /* mount the sD card */

    /* Open or create a log file and ready to append */
    if (res == FR_OK)   // mounted
    {

        res = f_open(&fp, date, FA_READ);
        if (res == FR_OK)
        {
            Report("About to read %i bytes from %s\n\r", len, date);
            res = f_read(&fp, &buff, len, &len);
            res = f_close(&fp);
            int i;
            for(i=0;i<len;i++)
            {
                UART_PRINT("\n\r : %s", buff[i]);
            }
        }
    }

    return (long) res;

}
