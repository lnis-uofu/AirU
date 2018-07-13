/*
 * ota_if.c
 *
 *  Created on: Oct 11, 2017
 *      Author: Tom
 */

#include "server_if.h"

unsigned char firmware_binary[64];
unsigned char* g_ucOTABinFN = "/sys/mcuimg2.bin";

HTTPCli_Struct AirHTTPClient;

typedef enum {
    SERVER_CONNECTION_FAILED = -0x7FB,
    GET_HOST_IP_FAILED       = SERVER_CONNECTION_FAILED - 1,
    SERVER_GET_TIME_FAILED   = GET_HOST_IP_FAILED - 1,
} e_NetAppStatusCodes;
//*****************************************************************************
//
//! Function to connect to OTA server
//!
//! \param  httpClient - Pointer to HTTP Client instance
//!
//! \return Error-code or SUCCESS
//!
//*****************************************************************************

static long _conn_airu_server() {
    long lRetVal = -1;
    struct sockaddr_in addr;
    unsigned long ulAirIP;

#ifdef USE_PROXY
    struct sockaddr_in paddr;
    paddr.sin_family = AF_INET;
    paddr.sin_port = htons(PROXY_PORT);
    paddr.sin_addr.s_addr = sl_Htonl(PROXY_IP);
    HTTPCli_setProxy((struct sockaddr *)&paddr);
#endif

    /* Resolve HOST NAME/IP */
    lRetVal = sl_NetAppDnsGetHostByName((signed char *) AIRU_DNS_NAME,
            strlen((const char *) AIRU_DNS_NAME), &ulAirIP, SL_AF_INET);


    if (lRetVal < 0) {
        ASSERT_ON_ERROR(GET_HOST_IP_FAILED);
    }

    /* Set up the input parameters for HTTP Connection */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HTTP_DL_PORT);
    addr.sin_addr.s_addr = sl_Htonl(ulAirIP);

    /* Testing HTTPCli open call: handle, address params only */
    HTTPCli_disconnect(&AirHTTPClient);
    HTTPCli_construct(&AirHTTPClient);
    lRetVal = HTTPCli_connect(&AirHTTPClient, (struct sockaddr *) &addr, 0, NULL);
    if (lRetVal < 0) {
        ASSERT_ON_ERROR(SERVER_CONNECTION_FAILED);
    }

    return 0;
}

////*****************************************************************************
////
////! \brief This function changes the HTTP port used by the AIR server
////!
////! \param   port - the new port to use for HTTP
////
////*****************************************************************************
//void AIR_ChangeHTTPPort(unsigned int port)
//{
//    HTTP_DL_PORT = port;
//}
//
////*****************************************************************************
////
////! \brief This function changes the HTTPS port used by the AIR server
////!
////! \param   port - the new port to use for HTTPS
////
////*****************************************************************************
//void AIR_ChangeHTTPSPort(unsigned int port)
//{
//    HTTPS_DL_PORT = port;
//}


//*****************************************************************************
//
//! \brief This function initializes the device to be an HTTP client.
//!
//! \param   none
//! \return  On success, zero is returned. On error, negative is returned
//! \warning if failure to connect to AP - Loops forever
//
//*****************************************************************************
long AIR_ConnectAIRUServer() {
    long lRetVal = 0;
    if (Net_Status())
    {
        lRetVal = _conn_airu_server();
        return lRetVal;
    }
    else
    {
        return FAILURE;
    }
}

extern long AIR_DownloadFile(char *src, char* dst)
{
    long lRetVal = FAILURE;

    // TODO: Secure connect
    lRetVal = AIR_ConnectAIRUServer();
    if(lRetVal<SUCCESS)
    {
        return lRetVal;
    }

    lRetVal = HTTP_GETDownloadFile(&AirHTTPClient, src, dst, (char*) AIRU_DNS_NAME);

    return lRetVal;
}



long OTA_DownloadBin()
{
    long lRetVal = FAILURE;

    char *tmp = malloc(strlen((const char*) firmware_binary) +
                                strlen((const char*) OTA_BIN_SERVER_DIR));

    strcpy(tmp,(const char*) OTA_BIN_SERVER_DIR);
    strcat(tmp,(const char*) firmware_binary);

    lRetVal = HTTP_GETDownloadFile(&AirHTTPClient, tmp, g_ucOTABinFN, (char*) AIRU_DNS_NAME);

    free(tmp);

    return lRetVal;

}

//****************************************************************************
//
//! \brief Downloads and updates application binary
//!
//! \return  Success [0] or Failure [<0]
//
//****************************************************************************
long OTA_UpdateAppBin()
{
    sBootInfo_t sBootInfo;
    long lFileHandle;
    long lRetVal = FAILURE;
    unsigned long ulToken;

    lRetVal = AIR_ConnectAIRUServer();
    if(lRetVal < SUCCESS)
    {
        return lRetVal;
    }

    //
    // Read the boot Info
    //
    if( 0 == sl_FsOpen((unsigned char *)IMG_BOOT_INFO, FS_MODE_OPEN_READ,
                       &ulToken, &lFileHandle) )
    {
        if( 0 > sl_FsRead(lFileHandle, 0, (unsigned char *)&sBootInfo,
                         sizeof(sBootInfo_t)) )
        {
          return -1;
        }
        sl_FsClose(lFileHandle, 0, 0, 0);
    }

    if(sBootInfo.ucActiveImg==IMG_ACT_FACTORY){
        sBootInfo.ucActiveImg = IMG_ACT_USER1;
    }
    else{
        sBootInfo.ucActiveImg = IMG_ACT_FACTORY;
        g_ucOTABinFN = "/sys/mcuimg1.bin";
    }
    //
    // Set the factory default
    //
    sBootInfo.ulImgStatus = IMG_STATUS_NOTEST;

    // Download the binary from the server
    lRetVal = OTA_DownloadBin();
    if(lRetVal<0)
    {
        return lRetVal;
    }

    //
    // Save the new configuration
    //
    lRetVal = sl_FsOpen((unsigned char *) IMG_BOOT_INFO, FS_MODE_OPEN_WRITE,
                        &ulToken, &lFileHandle);

    if (0 == lRetVal) {

        sl_FsWrite(lFileHandle, 0, (unsigned char *) &sBootInfo,
                   sizeof(sBootInfo_t));

        lRetVal = sl_FsClose(lFileHandle, 0, 0, 0);
    }

    return lRetVal;
}

void OTA_SetBinFN(char *fn)
{
    strcpy(firmware_binary,fn);
}


