
#include "simplelink_if.h"
#include "uart_if.h"

#include <stdio.h>

//*****************************************************************************
// Globals used by mDNS
//*****************************************************************************
static char mdnsServiceName[40] = "";
static char mdnsText[70] = "";

static char g_ucMacAddress[20];
static char g_ucUniqueID[20];

//*****************************************************************************
//! Check if any SimpleLink Profiles exist
//!
//! Returns 1 if there are stored profiles, 0 otherwise
//!
//****************************************************************************
unsigned char sl_ProfilesExist()
{
    _i16 i;
    _i8 ssid[32];
    _i16 ssidLen;
    _u8 macAddr[6];
    SlSecParams_t secParams;
    SlGetSecParamsExt_t secExtParams;
    _u32 priority;
    unsigned char prof = 0;

    Report("Saved Connection Profiles:\r\n");

    for(i = 0; i < 7; i++){
        if(sl_WlanProfileGet(i, ssid, &ssidLen, macAddr, &secParams, &secExtParams, &priority) >= 0){
            Report("%i: %.*s\r\n", i, ssidLen, ssid);
            prof = 1;
        }
        else
        {
            Report("No profile at index [%i]\n\r",i);
        }
    }

    return prof;

}

//*****************************************************************************
//! Finds index of SimpleLink WIFI profile
//!
//! Returns index of profile if it exists, else returns -1
//!
//****************************************************************************
int sl_GetProfileIndex(const char *cProfID)
{
    _i16 i;
    _i8 ssid[32];
    _i16 ssidLen;
    _u8 macAddr[6];
    SlSecParams_t secParams;
    SlGetSecParamsExt_t secExtParams;
    _u32 priority;
    int profile_index = -1;

    Report("Saved Connection Profiles:\r\n");

    for(i = 0; i < 7; i++){
        if(sl_WlanProfileGet(i, ssid, &ssidLen, macAddr, &secParams, &secExtParams, &priority) >= 0){
            Report("\t%i: %.*s\r\n", i, ssidLen, ssid);
            ssid[ssidLen] = '\0';
            Report("Comparing \n\r\tMine:\t[%s] \n\r\tStored:\t[%s]\n\n\r",cProfID,ssid);
            // strstr instead of strcmp b/c sometimes there's shit attached to the profile
            //  I haven't figured out where this comes from yet and haven't fixed it
            //  so this is what we're going to do for now.
            if (strstr(ssid,cProfID != NULL));   // if it was found
            {
                profile_index = i;
                return profile_index;
            }
        }
    }

    return profile_index;

}

//*****************************************************************************
//! getMacAddress
//!
//! Returns the MAC Address as string
//!
//****************************************************************************
long setMacAddress()
{
	long lRetVal = 0;
    int i;

	unsigned char macAddressVal[SL_MAC_ADDR_LEN];
	unsigned char macAddressLen = SL_MAC_ADDR_LEN;
	unsigned char macAddressPart[2];

	memset(g_ucUniqueID,0,sizeof(g_ucUniqueID));
    memset(g_ucMacAddress,0,sizeof(g_ucMacAddress));

	lRetVal = sl_NetCfgGet(SL_MAC_ADDRESS_GET,NULL,&macAddressLen,(unsigned char *)macAddressVal);

	for (i = 0 ; i < 6 ; i++)
	{
		sprintf(macAddressPart, "%02X", macAddressVal[i]);
		strcat(g_ucMacAddress, (const unsigned char *)macAddressPart);
		strcat(g_ucUniqueID, (const unsigned char *)macAddressPart);
		strcat(g_ucMacAddress, ":");
	}

	g_ucMacAddress[17] = '\0'; // Replace the the last : with a zero termination

	return lRetVal;
}

void cpyMacAddress(char *cpystr)
{
    strcpy(cpystr,g_ucMacAddress);
}

void cpyUniqueID(char *cpystr)
{
    strcpy(cpystr,g_ucUniqueID);
}
//*****************************************************************************
//! getDeviceName
//!
//! Returns the Device Name as a string
//!
//****************************************************************************
char * getDeviceName()
{
    char my_device_name[35];
    sl_NetAppGet (SL_NET_APP_DEVICE_CONFIG_ID, NETAPP_SET_GET_DEV_CONF_OPT_DEVICE_URN, strlen(my_device_name), (_u8 *)my_device_name);
	return my_device_name;
}

//*****************************************************************************
//! getApDomainName
//!
//! Returns the Access Point Domain Name as a string
//!
//****************************************************************************
char * getApDomainName()
{
	static char strDomainName[35];
	sl_NetAppGet (SL_NET_APP_DEVICE_CONFIG_ID, NETAPP_SET_GET_DEV_CONF_OPT_DOMAIN_NAME, (unsigned char *)strlen(strDomainName), (unsigned char *)strDomainName);
	return strDomainName;
}

//*****************************************************************************
//! getSsidName
//!
//! Returns the SSID Name for the device when in Access Point Mode
//!
//****************************************************************************
int getSsidName(unsigned char* ssidName)
{
    unsigned int len = 32;
	unsigned int  config_opt = WLAN_AP_OPT_SSID;
	return sl_WlanGet(SL_WLAN_CFG_AP_ID, &config_opt , &len, (unsigned char *)ssidName);

}

//*****************************************************************************
//! getDeviceTimeDate
//!
//! Gets the device time and date
//!
//! Returns: On success, zero is returned. On error, -1 is returned
//!
//****************************************************************************
//int getDeviceTimeDate()
//{
//	int iretVal;
//	//dateTime =  {0};
//	unsigned char configLen = (unsigned char)sizeof(SlDateTime_t);
//	unsigned char configOpt = (unsigned char)SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME;
//	iretVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &configOpt, &configLen, (unsigned char *)&dateTime);
//	return iretVal;
//}

//*****************************************************************************
//! setDeviceName
//!
//! Sets the name of the Device
//!
//! Returns: On success, zero is returned. On error, -1 is returned
//!
//****************************************************************************
int setDeviceName()
{
	int iretVal;
	unsigned char strDeviceName[32] = DEVICE_NAME;
	iretVal = sl_NetAppSet (SL_NET_APP_DEVICE_CONFIG_ID, NETAPP_SET_GET_DEV_CONF_OPT_DEVICE_URN, strlen((const char *)strDeviceName), (unsigned char *) strDeviceName);
	return iretVal;
}

//*****************************************************************************
//! setApDomainName
//!
//! Sets the name of the Access Point's Domain Name
//!
//! Returns: On success, zero is returned. On error, -1 is returned
//!
//****************************************************************************
long setAPDomainName()
{
	long lRetVal;
	unsigned char strDomain[32] = DEVICE_AP_DOMAIN_NAME;
	unsigned char lenDomain = strlen((const char *)strDomain);
	lRetVal = sl_NetAppSet(SL_NET_APP_DEVICE_CONFIG_ID, NETAPP_SET_GET_DEV_CONF_OPT_DOMAIN_NAME, lenDomain, (unsigned char*)strDomain);
	return lRetVal;
}

//*****************************************************************************
//! setSsidName
//!
//! Sets the SSID name for AP mode
//!
//! Returns: On success, zero is returned. On error one of the following error codes returned:
//!    CONF_ERROR (-1)
//!    CONF_NVMEM_ACCESS_FAILED (-2)
//!    CONF_OLD_FILE_VERSION (-3)
//!    CONF_ERROR_NO_SUCH_COUNTRY_CODE (-4)
//!
//****************************************************************************
int setSSIDName()
{
	int iretVal;
	unsigned char  str[33] = "AirU-";
	strcat(str, &g_ucUniqueID[8]);
	unsigned short  length = strlen((const char *)str);
	iretVal = sl_WlanSet(SL_WLAN_CFG_AP_ID, WLAN_AP_OPT_SSID, length, str);
	return iretVal;
}


//*****************************************************************************
//! setDeviceTimeDate
//!
//! Gets the device time and date
//!
//! Returns: On success, zero is returned. On error, -1 is returned
//!
//****************************************************************************
//int setDeviceTimeDate()
//{
//	int iretVal;
//	iretVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION, SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME, sizeof(SlDateTime_t), (_u8 *)(&dateTime));
//	return iretVal;
//}

//*****************************************************************************
//! registerMdnsService
//!
//! Registers the mDNS Service.
//!
//! Service settings for type and port are set in simplelinklibrary.h
//!
//! Returns: On success returns 0
//!
//****************************************************************************
int registerMdnsService(){
	int iretVal;
	unsigned int i;
	unsigned char macAddress[18];
	// Create mDNS Service Name
	for (i = 0; i < 40; i++)
		mdnsServiceName[i] = 0x00;

	// Obtain the device name
	char deviceName[20];
	getDeviceName(&deviceName,20);

	strcat(mdnsServiceName, (const char *)deviceName);
	strcat(mdnsServiceName, MDNS_SERVICE);

	// Create mDNS Text
	for (i = 0; i < 50; i++)
		mdnsText[i] = 0x00;

	// Obtain the MAC Address
	getMacAddress(macAddress);

	strcat(mdnsText, "mac=");
	strcat(mdnsText, (const char *)macAddress);
	strcat(mdnsText, ";ver=");
	strcat(mdnsText, DEVICE_VERSION);
	strcat(mdnsText, ";man=");
	strcat(mdnsText, DEVICE_MANUFACTURE);
	strcat(mdnsText, ";mod=");
	strcat(mdnsText, DEVICE_MODEL);
	strcat(mdnsText, "\0");

	int strSvrLength = strlen(mdnsServiceName);
	int strTxtLength = strlen(mdnsText);

	//Unregisters the mDNS service.
	unregisterMdnsService();

	//Registering for the mDNS service.
	iretVal = sl_NetAppMDNSRegisterService((const signed char *)mdnsServiceName,strlen(mdnsServiceName),
			(const signed char *)mdnsText,strlen(mdnsText)+1, UDPPORT, TTL, UNIQUE_SERVICE);

	return iretVal;
}

//*****************************************************************************
//! unregisterMdnsService
//!
//! Unregisters the mDNS Service.
//!
//! Returns: On success returns 0
//!
//****************************************************************************
int unregisterMdnsService()
{
	int iretVal;
	iretVal = sl_NetAppMDNSUnRegisterService((const signed char *)mdnsServiceName,strlen(mdnsServiceName));
	return iretVal;
}

