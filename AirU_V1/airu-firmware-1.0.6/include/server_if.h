/*
 * ota_if.h
 *
 *  Created on: Oct 11, 2017
 *      Author: Tom
 */

#ifndef INCLUDE_SERVER_IF_H_
#define INCLUDE_SERVER_IF_H_

#include "internet_if.h"

#define AIRU_DNS_NAME           "air.eng.utah.edu"
#define OTA_BIN_SERVER_DIR      "/files/updates/"
#define HTTP_DL_PORT            (80)
#define HTTPS_DL_PORT           (443)

extern long AIR_ConnectAIRUServer(void);
extern long AIR_DownloadFile(char *src, char* dst);
//extern void AIR_ChangeHTTPPort(unsigned int port);
//extern void AIR_ChangeHTTPSPort(unsigned int port);
extern long OTA_DownloadBin(void);
extern long OTA_UpdateAppBin(void);
extern void OTA_SetBinFN(char *fn);

#endif /* INCLUDE_SERVER_IF_H_ */
