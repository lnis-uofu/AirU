//*****************************************************************************
// device_status.c - Device Status
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

//****************************************************************************
//
//! \addtogroup oob
//! @{
//
//****************************************************************************

#include "simplelink.h"
#include "device_status.h"
#include "uart_if.h"
#include "common.h"
#include "osi.h"


//******************************************************************************
//                            GLOBAL VARIABLES
//******************************************************************************
unsigned int g_uiPingPacketsRecv = 0;
unsigned int g_uiPingDone = 0;


//****************************************************************************
//
//!    \brief call back function for the ping test
//!
//!    \param  pPingReport is the pointer to the structure containing the result
//!         for the ping test
//!
//!    \return None
//
//****************************************************************************
void SimpleLinkPingReport(SlPingReport_t *pPingReport)
{
    g_uiPingDone = 1;
    g_uiPingPacketsRecv = pPingReport->PacketsReceived;
}

//****************************************************************************
//
//!    \brief pings to ip address of domain "www.google.com"
//!
//! This function pings to the default gateway to ensure the wlan cannection,
//! then check for the internet connection, if present then get the ip address
//! of Domain name "www.google.com" and pings to it
//!
//!
//!    \return -1 for unsuccessful LAN connection, -2 for problem with internet
//!         conection and 0 for succesful ping to the Domain name
//
//****************************************************************************
int ConnectionTest()
{
	UART_PRINT("\n\rTesting Connection to Internet.\n\r");

    int iStatus = 0;
  
    SlPingStartCommand_t PingParams;
    SlPingReport_t PingReport;
    unsigned long ulIpAddr;
    // Set the ping parameters
    PingParams.PingIntervalTime = 1000;
    PingParams.PingSize = 1;
    PingParams.PingRequestTimeout = 1000;
    PingParams.TotalNumberOfAttempts = 3;
    PingParams.Flags = 2;

    g_uiPingDone = 0;
    g_uiPingPacketsRecv = 0;


    /* Check for Internet connection */
    /* Querying for google.com IP address */
//    UART_PRINT("\t ~ Querying for google.com IP address.\n\r");
    iStatus = sl_NetAppDnsGetHostByName((signed char *)"www.google.com",
                                           14, &ulIpAddr, SL_AF_INET);

    if (iStatus < 0)
    {
        // LAN connection is successful
        // Problem with Internet connection
//    	UART_PRINT("\t [ERROR] Internet Connection Test [Failed].\n\r");
        return -2;
    }


    // Replace the ping address to match google.com IP address
    PingParams.Ip = ulIpAddr;

    // Try to ping www.google.com
//    UART_PRINT("\t ~ Starting PING Test to google.com.\n\r");
    sl_NetAppPingStart((SlPingStartCommand_t*)&PingParams, SL_AF_INET,
             (SlPingReport_t*)&PingReport, SimpleLinkPingReport);


    while (!g_uiPingDone)
    {
        osi_Sleep(100);
    }

    if (g_uiPingPacketsRecv)
    {
        // LAN connection is successful
        // Internet connection is successful
//    	UART_PRINT("\t ~ Internet Connection Test [Success].\n\r");
        g_uiPingPacketsRecv = 0;
        return 0;
    }
    else
    {
        // LAN connection is successful
        // Problem with Internet connection
//    	UART_PRINT("\t [ERROR] Internet Connection Test [Failed].\n\r");
        return -2;
    }
    
}



//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
