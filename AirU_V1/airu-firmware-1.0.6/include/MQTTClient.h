/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/

#ifndef __MQTT_CLIENT_C_
#define __MQTT_CLIENT_C_

#include "MQTTPacket.h"
#include "internet_if.h"
#include "MQTTCC3200.h" //Platform specific implementation header file

#define min(x,y) ((x)<(y) ? (x) : (y))
#define CLR_STATUS_BIT_ALL(status_variable)  (status_variable = 0)
#define SET_STATUS_BIT(status_variable, bit) status_variable |= (1<<(bit))
#define CLR_STATUS_BIT(status_variable, bit) status_variable &= ~(1<<(bit))
#define CLR_STATUS_BIT_ALL(status_variable)   (status_variable = 0)
#define GET_STATUS_BIT(status_variable, bit) (0 != (status_variable & (1<<(bit))))

#define USE_SSL      // Uncomment for Secure TLS connection with server

#ifdef USE_SSL
#define SL_SSL_CA_CERT_FILE_NAME        "/cert/ca.der"
#endif

#define MQTT_USER   "user-placeholder"
#define MQTT_PWD    "password-placeholder"

#define BUFF_SIZE 250
#define MAX_TPC_LENGTH 50   // max topic length for message queue
#define MAX_PLD_LENGTH 250  // max payload length for message queue
#define MAX_PACKET_ID 65535
#define MAX_MESSAGE_HANDLERS 5
#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}

#ifdef PRISMS
#define MQTT_BROKER_ADDR "broker-prisms-p1.bmi.utah.edu"
#else
#define MQTT_BROKER_ADDR "air.eng.utah.edu"
#endif

// #define MQTT_INFLUX_TOPIC "airu/test"
// #define MQTT_INFLUX_TOPIC "airu/usu"

// all failure return codes must be negative
#define BUFFER_OVERFLOW     -2
#define FAILURE             -1
#define SUCCESS              0

enum QoS { QOS0, QOS1, QOS2 };

typedef enum
{
    OTA,    /* Over The Air Updates */
    DBG,    /* Debug Info */
    PUB,    /* New Publish topic (time alive, Network info, SD card space, available subscription topics) */
    DNL,    /* Download a file from the server */
    SUB,    /* New Subscription topic */
    CPT,    /* Change Publish Topic */
    CPF     /* What the hell is this??? */
} Topics;   // TODO: Lots of topic parsing
typedef enum
{
    ALL,
    DevID,
    BOTH
} Channel; // TODO: Ability to subscribe to channel on /all/ or /devID/ independently
typedef enum
{
    DATAPOINT = ( 1 << 0 ),
    HELP_MSG  = ( 1 << 1 ),
    DEBUG     = ( 1 << 2 )
}PublishTopics;

typedef struct Client Client;
typedef struct MQTTMessage MQTTMessage;
typedef struct MessageData MessageData;
typedef struct MQTTTopicList MQTTTopicList;
typedef struct MQTTConnectionInfo MQTTConnectionInfo;
typedef struct MQTTPubData MQTTPubData;
typedef void (*messageHandler)(MessageData*);


struct Client {
    unsigned int next_packetid;
    unsigned int command_timeout_ms;
    size_t buf_size, readbuf_size;
    unsigned char *buf;
    unsigned char *readbuf;
    unsigned int keepAliveInterval;
    char ping_outstanding;
    int isconnected;

    struct MessageHandlers
    {
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic (UNUSED)

    void (*defaultMessageHandler) (MessageData*);

    Network* ipstack;
    Timer ping_timer;
};

struct MQTTMessage
{
    enum QoS qos;
    char retained;
    char dup;
    unsigned short id;
    void *payload;
    size_t payloadlen;
    char topic[MAX_TPC_LENGTH];         // Space is tight! Don't be using superfluous topic names! Set with strcpy
    size_t topiclen;
};

struct MessageData
{
    MQTTMessage* message;
    MQTTString* topicName;
};

struct MQTTConnectionInfo
{
    Client hMQTTClient;
    Network n;
    char* addr;
    int port;
    unsigned int timeout_ms;
    char* clientID;
    MQTTPacket_connectData cdata;
};

//struct TLSConnectionInfo
//{
//    SlSockSecureFiles_t sockSecureFiles;
//    unsigned char sec_method;
//    unsigned int cipher;
//    char server_verify;
//};

unsigned char getMQTTPubFlag(void);
void setMQTTPubFlag(PublishTopics);
void resetMQTTPubFlag(PublishTopics);
void setMQTTRebootFlag(void);
unsigned char getMQTTRebootFlag(void);
void clearMQTTRebootFlag(void);
int MQTTInitialize(MQTTConnectionInfo*);
int MQTTConnect (Client*, MQTTPacket_connectData*);
int MQTTPublish (Client*, MQTTMessage*);
int MQTTPublishNext(OsiMsgQ_t*, Client*);
int MQTTSubscribe (Client*, const char*, enum QoS, messageHandler);
int MQTTUnsubscribeOne(Client*, unsigned int);
int MQTTUnsubscribeActive(Client*);
int MQTTUnsubscribeAll(Client*);
int MQTTSubscribeActive(Client*, enum QoS, messageHandler);
int MQTTSubscribeAll(Client*, enum QoS, messageHandler);
int MQTTUnsubscribe (Client*, const char*);
int MQTTDisconnect (Client*);
int MQTTReconnect (MQTTConnectionInfo*);
int MQTTYield (Client*, int);
int MQTTRead();

void NewTimer(Timer*);
void setDefaultMessageHandler(Client*, messageHandler);
void MQTTClient(Client*, Network*, unsigned int, unsigned char*, size_t, unsigned char*, size_t);

OsiReturnVal_e MQTTPubQAdd(OsiMsgQ_t*, MQTTMessage*);

#endif
