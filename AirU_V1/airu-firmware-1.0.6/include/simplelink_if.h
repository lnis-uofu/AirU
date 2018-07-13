#ifndef SIMPLELINK_IF_H_
#define SIMPLELINK_IF_H_

#include "simplelink.h"

 //*****************************************************************************
 // Device Defines
 //*****************************************************************************
#define DEVICE_VERSION			"1.1.0"
#define DEVICE_MANUFACTURE		"UOFU"
#define DEVICE_NAME 			"airu"
#define DEVICE_MODEL			"CC3200"
#define DEVICE_AP_DOMAIN_NAME	"www.myairu.local"

 //*****************************************************************************
 // mDNS Defines
 //*****************************************************************************
#define MDNS_SERVICE  "._control._udp.local"
#define TTL             120
#define UNIQUE_SERVICE  1       /* Set 1 for unique services, 0 otherwise */

 //*****************************************************************************
 // SimpleLink/WiFi Defines
 //*****************************************************************************
#define UDPPORT         4000 /* Port number to which the connection is bound */
#define UDPPACKETSIZE   1024
#define SPI_BIT_RATE    14000000

#define TIMEOUT 5

//*****************************************************************************
// Date and Time Global
//*****************************************************************************
//extern SlDateTime_t dateTime;

 //*****************************************************************************
 // Function Declarations
 //*****************************************************************************

int setDeviceName(void);
int setSSIDName(void);
int setDeviceTimeDate();
int registerMdnsService();
int unregisterMdnsService();
int getSsidName();
int getDeviceTimeDate();
int sl_GetProfileIndex(const char *cProfID);

long setAPDomainName(void);
long setMacAddress();

void cpyMacAddress(char *cpystr);
void cpyUniqueID(char *cpystr);

char * getDeviceName();
char * getApDomainName();

unsigned char sl_ProfilesExist(void);


#endif /* SIMPLELINK_IF_H_ */
