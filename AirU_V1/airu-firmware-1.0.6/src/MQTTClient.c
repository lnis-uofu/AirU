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

#include "MQTTClient.h"

/*************************************************
 * Define global variables
 ************************************************/
MQTTMessage* MsgOutline;
static volatile unsigned char flag = 0;
static volatile unsigned char bRebootFlag = 0;

// Topics that the board can be subscribed to
//      These topic paths are under airu/all and airu/deviceID
//          i.e. airu/+/
// * When adding topic don't forget to increate 'NUM_SUB_TOPICS' and add to enum States in .h
#define NUM_SUB_TOPICS 7    // I use this elsewhere, that's why I define it
char g_cSubTopics[NUM_SUB_TOPICS][4] = {
                    {"ota"},    /* OTA topic */
                    {"dbg"},    /* Debug topic */
                    {"pub"},    /* Publish something besides data */
                    {"dnl"},    /* Download a file (html, etc.)*/
                    {"sub"},    /* Subscribe to one of these channels */
                    {"cpt"},    /* Change Publish Topic (if we no longer want it in /influx */
                    {"cpf"}     /* Change Publish Frequency */
};


//TLSConnectionInfo TLSci;

char writeBuf[MAX_PLD_LENGTH];
char readBuf[MAX_PLD_LENGTH];

unsigned int g_uiSubscribedTopics = 0;

unsigned char getMQTTPubFlag()
{
    return flag;
}

void setMQTTPubFlag(PublishTopics val)
{
    flag |= val;
}

void setMQTTRebootFlag()
{
    bRebootFlag = 1;
}

unsigned char getMQTTRebootFlag()
{
    return bRebootFlag;
}

void clearMQTTRebootFlag()
{
    bRebootFlag = 0;
}

void resetMQTTPubFlag(PublishTopics val)
{
    flag &= ~(val);
}

void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessage) {
    md->topicName = aTopicName;
    md->message = aMessage;
}

int getNextPacketId(Client *c) {
    return c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
}


int sendPacket(Client* c, int length, Timer* timer)
{
    int rc = FAILURE,
        sent = 0;

    while (sent < length && !expired(timer))
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &c->buf[sent], length, left_ms(timer));
        if (rc < 0)  // there was an error writing the data
            break;
        sent += rc;
    }
    if (sent == length)
    {
        countdown(&c->ping_timer, c->keepAliveInterval); // record the fact that we have successfully sent the packet
        rc = SUCCESS;
    }
    else
        rc = FAILURE;
    return rc;
}


void MQTTClient(Client* c, Network* network, unsigned int command_timeout_ms, unsigned char* buf, size_t buf_size, unsigned char* readbuf, size_t readbuf_size)
{
    int i;
    c->ipstack = network;

    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        c->messageHandlers[i].topicFilter = 0;
    c->command_timeout_ms = command_timeout_ms;
    c->buf = buf;
    c->buf_size = buf_size;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
    c->defaultMessageHandler = NULL;
    InitTimer(&c->ping_timer);
}


int decodePacket(Client* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1)
            goto exit;
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);
exit:
    return len;
}


int readPacket(Client* c, Timer* timer)
{
    int rc = FAILURE;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    rc = c->ipstack->mqttread(c->ipstack, c->readbuf, 1, left_ms(timer));

    if(rc != 1)
    {
        goto exit;
    }

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(c, &rem_len, left_ms(timer));
    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, left_ms(timer)) != rem_len))

        goto exit;

    header.byte = c->readbuf[0];
    rc = header.bits.type;
exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;

    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };

    return (curn == curn_end) && (*curf == '\0');
}


int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAILURE;

    // we have to find the right message handler - indexed by topic
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {

        if (c->messageHandlers[i].topicFilter != 0 && (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter) ||
                isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
            if (c->messageHandlers[i].fp != NULL)
            {
                MessageData md;
                NewMessageData(&md, topicName, message);
                c->messageHandlers[i].fp(&md);
                rc = SUCCESS;
            }
        }
    }

    if (rc == FAILURE && c->defaultMessageHandler != NULL)
    {
        MessageData md;
        NewMessageData(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = SUCCESS;
    }

    return rc;
}


int keepalive(Client* c)
{
    int rc = FAILURE;

    if (c->keepAliveInterval == 0)
    {
        rc = SUCCESS;
        goto exit;
    }

    if (expired(&c->ping_timer))
    {
        if (!c->ping_outstanding)
        {

            Timer timer;
            InitTimer(&timer);
            countdown_ms(&timer, 1000);
            int len = MQTTSerialize_pingreq(c->buf, c->buf_size);
            if (len > 0 && (rc = sendPacket(c, len, &timer)) == SUCCESS){ // send the ping packet
                c->ping_outstanding = 1;
            }
        }
    }

exit:
    return rc;
}


int cycle(Client* c, Timer* timer)
{
    int len = 0;
    int rc = FAILURE;
    unsigned short packet_type;

    // read the socket, see what work is due
    rc = readPacket(c, timer);
    if(rc == FAILURE)
        goto exit;
    else
        packet_type = (unsigned short) rc;

    switch (packet_type)
    {
        case CONNACK:
        case PUBACK:
        case SUBACK:
            break;
        case PUBLISH:
        {
            MQTTString topicName;
            MQTTMessage msg;
            if (MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (unsigned short*)&msg.id, &topicName,
               (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
                goto exit;
            deliverMessage(c, &topicName, &msg);
            if (msg.qos != QOS0)
            {
                if (msg.qos == QOS1)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msg.id);
                else if (msg.qos == QOS2)
                    len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msg.id);
                if (len <= 0)
                    rc = FAILURE;
                   else
                       rc = sendPacket(c, len, timer);
                if (rc == FAILURE)
                    goto exit; // there was a problem
            }
            break;
        }
        case PUBREC:
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
            else if ((len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, mypacketid)) <= 0)
                rc = FAILURE;
            else if ((rc = sendPacket(c, len, timer)) != SUCCESS) // send the PUBREL packet
                rc = FAILURE; // there was a problem
            if (rc == FAILURE)
                goto exit; // there was a problem
            break;
        }
        case PUBCOMP:
        {
            break;
        }
        case PINGRESP:
        {
            c->ping_outstanding = 0;
            break;
        }
    }
    keepalive(c);
exit:
    if (rc == SUCCESS)
        rc = packet_type;
    return rc;
}


int MQTTYield(Client* c, int timeout_ms)
{
    int rc = SUCCESS;
    Timer timer;
    InitTimer(&timer);
    countdown_ms(&timer, timeout_ms);
    while (!expired(&timer))
    {
        if (cycle(c, &timer) == FAILURE)
        {
            rc = FAILURE;
            break;
        }
    }

    return rc;
}

// only used in single-threaded mode where one command at a time is in process
int waitfor(Client* c, int packet_type, Timer* timer)
{
    int rc = FAILURE;

    do
    {
        if (expired(timer))
            break; // we timed out
    }
    while ((rc = cycle(c, timer)) != packet_type);

    return rc;
}

int MQTTInitialize(MQTTConnectionInfo* ci)
{
    int rc;

    rc = (int)TIME_SetSLDeviceTime();
    UART_PRINT("\t$ Setting Device Time\n\r");
    if (rc != SUCCESS)
    {
        UART_PRINT("In MQTTInitialize() -- Couldn't get time [%i]\n\r",rc);
        return rc;
    }

    MsgOutline->dup = 0;
    MsgOutline->id = 0;
    MsgOutline->qos = QOS1;
    MsgOutline->retained = 0;
    MsgOutline->payload = NULL;
    MsgOutline->payloadlen = 0;

    memset(MsgOutline->topic,0,20);
    MsgOutline->topiclen = 20;

    UART_PRINT("\t$ Closing stagnant sockets\n\r");
    cc3200_disconnect(&ci->n);
    NewNetwork(&(ci->n));
    cc3200_disconnect(&ci->n);

// Open socket and bind
#ifdef USE_SSL

    ci->port = 8883;
    rc = TLSConnectNetwork(&ci->n, ci->addr, ci->port,
                           (char*)SL_SSL_CA_CERT_FILE_NAME,
                           SL_SO_SEC_METHOD_SSLv3_TLSV1_2,
                           SL_SEC_MASK_SSL_RSA_WITH_RC4_128_SHA, 0);
    UART_PRINT("RC1 = %i\n\r",rc);
    if (rc < 0)
        return rc;

#else
    ci->port = 1883;
    rc = ConnectNetwork(&ci->n, ci->addr, ci->port);
    if (rc < 0){
        return rc;
    }
#endif

    // Setup the client parameters
    MQTTClient(&ci->hMQTTClient, &ci->n, ci->timeout_ms, (unsigned char*)writeBuf,
               sizeof(writeBuf), (unsigned char*)readBuf, sizeof(readBuf));

    MQTTPacket_connectData MQTTPacket_connectData_init = MQTTPacket_connectData_initializer;

    ci->cdata = MQTTPacket_connectData_init;
    ci->cdata.MQTTVersion = 3;
    ci->cdata.keepAliveInterval = 120;
    ci->cdata.clientID.cstring = ci->clientID;
    ci->cdata.cleansession = 1;
#ifdef PRISMS
    ci->cdata.username.cstring = MQTT_USER;
    ci->cdata.password.cstring = MQTT_PWD;

    // Send connection information to server
    rc = MQTTConnect(&ci->hMQTTClient, &(ci->cdata));
    UART_PRINT("RC2 = %i\n\r",rc);
    return rc;
}

int MQTTConnect(Client* c, MQTTPacket_connectData* options)
{
    Timer connect_timer;
    int rc = FAILURE;
    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;

    InitTimer(&connect_timer);
    countdown_ms(&connect_timer, c->command_timeout_ms);

    if (c->isconnected){ // don't send connect packet again if we are already connected
        rc = 1;          // is already connected (don't want it to be an error)
        goto exit;
    }

    if (options == 0){
        options = &default_options; // set default options if none were supplied
    }

    c->keepAliveInterval = options->keepAliveInterval;
    countdown(&c->ping_timer, c->keepAliveInterval);
    if ((len = MQTTSerialize_connect(c->buf, c->buf_size, options)) <= 0){
        goto exit;
    }
    if ((rc = sendPacket(c, len, &connect_timer)) != SUCCESS){  // send the connect packet
        goto exit; // there was a problem
    }
    // this will be a blocking call, wait for the connack
    if (waitfor(c, CONNACK, &connect_timer) == CONNACK)
    {
        unsigned char connack_rc = 255;
        char sessionPresent = 0;
        if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1){
            rc = connack_rc;
        }
        else
            rc = FAILURE;
    }
    else
        rc = FAILURE;

exit:
    if (rc == SUCCESS){
        c->isconnected = 1;
    }
    return rc;
}


int MQTTSubscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{

    int rc = FAILURE;
    Timer timer;
    int len = 0;

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic, (int*)&qos);

    if (len <= 0)
        goto exit;

    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem

    if (waitfor(c, SUBACK, &timer) == SUBACK)      // wait for suback
    {

        int count = 0, grantedQoS = -1;
        unsigned short mypacketid;
        if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
            rc = grantedQoS; // 0, 1, 2 or 0x80
        if (rc != 0x80)
        {
            c->defaultMessageHandler = messageHandler;
        }
    }
    else
    {
        rc = FAILURE;
    }

exit:
    return rc;
}

/*
 * Unscribes from all active subscriptions
 *      To be used after socket reset
 */
int MQTTUnsubscribeActive(Client* c){
    int rc = FAILURE;
    int i;

    for (i=0;i<sizeof(g_uiSubscribedTopics);i++){
        if(GET_STATUS_BIT(g_uiSubscribedTopics,i)){
            rc = MQTTUnsubscribeOne(c, i);
            if (rc < 0)
                break;
        }
    }
    return rc;
}

int MQTTUnsubscribeAll(Client* c){
    int rc = FAILURE;
    int i;

    for (i=0;i<sizeof(g_uiSubscribedTopics);i++){
        CLR_STATUS_BIT(g_uiSubscribedTopics,i);
        rc = MQTTUnsubscribeOne(c, i);
        if (rc < 0)
            break;
    }
    return rc;
}

int MQTTSubscribeAll(Client* c, enum QoS qos, messageHandler messageHandler){

    int rc;
    char ID[13], tmp[25];

    //
    // Subscribe to "airu/all"
    //
    rc = MQTTSubscribe(c, "airu/all", qos, messageHandler);
    if (rc < 0){
        return rc;
    }

#ifdef FACTORY
    //
    // Subscribe to "airu/all"
    //
    rc = MQTTSubscribe(c, "airu/allnew", qos, messageHandler);
    if (rc < 0){
        return rc;
    }
#endif
    //
    // Subscribe to "airu/deviceID"
    //
    cpyUniqueID(ID);
    sprintf(tmp,"airu/%s",ID);

    rc = MQTTSubscribe(c, tmp, qos, messageHandler);
    if (rc < 0){
        return rc;
    }

    return rc;
}

/*
 * Subscribes to all active subscription topics
 *      To be use after a socket reset
 */
//int MQTTSubscribeActive(Client* c, enum QoS qos, messageHandler messageHandler){
//
//    int rc = FAILURE;
//    int i;
//
//    for (i=0;i<sizeof(g_uiSubscribedTopics);i++){
//        if(GET_STATUS_BIT(g_uiSubscribedTopics,i)){
//            rc = MQTTSubscribeOne(c, i, qos, messageHandler);
//            if (rc < 0)
//                break;
//        }
//    }
//
//    return rc;
//}

int MQTTUnsubscribe(Client* c, const char* topicFilter)
{
    int rc = FAILURE;
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

    if (waitfor(c, UNSUBACK, &timer) == UNSUBACK)
    {
        unsigned short mypacketid;  // should be the same as the packetid above
        if (MQTTDeserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) == 1)
            rc = 0;
    }
    else
        rc = FAILURE;

exit:
    return rc;
}


int MQTTPublish(Client* c, MQTTMessage* message)
{
    int rc = FAILURE;
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)message->topic;
    int len = 0;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (!c->isconnected)
        goto exit;

    if (message->qos == QOS1 || message->qos == QOS2)
        message->id = getNextPacketId(c);

    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id,
              topic, (unsigned char*)message->payload, message->payloadlen);

    if (len <= 0)
        goto exit;

    if ((rc = sendPacket(c, len, &timer)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

    if (message->qos == QOS1)
    {
        if (waitfor(c, PUBACK, &timer) == PUBACK)
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }
    else if (message->qos == QOS2)
    {
        if (waitfor(c, PUBCOMP, &timer) == PUBCOMP)
        {
            unsigned short mypacketid;
            unsigned char dup, type;
            if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
                rc = FAILURE;
        }
        else
            rc = FAILURE;
    }

exit:
    return rc;
}

int MQTTDisconnect(Client* c)
{
    int rc = FAILURE;
    Timer timer;     // we might wait for incomplete incoming publishes to complete
    int len = MQTTSerialize_disconnect(c->buf, c->buf_size);

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (len > 0)
        rc = sendPacket(c, len, &timer);            // send the disconnect packet

    c->isconnected = 0;
    return rc;
}

/*
 * Recreates socket then reconnects
 */
int MQTTReconnect(MQTTConnectionInfo* ci)
{
    int rc = FAILURE;

    /************************************
    * close the old socket
    ************************************/
    rc = cc3200_disconnect(&(ci->n));

    if(rc<SUCCESS)
    {
        return rc;
    }

    /************************************
    * create and bind a new socket
    ************************************/
    NewNetwork(&(ci->n));

#ifdef USE_SSL
    ci->port = 8883;
    rc = TLSConnectNetwork(&ci->n, ci->addr, ci->port,                  \
                           (char*)SL_SSL_CA_CERT_FILE_NAME,             \
                           SL_SO_SEC_METHOD_SSLv3_TLSV1_2,              \
                           SL_SEC_MASK_SSL_RSA_WITH_RC4_128_SHA, 0);
    if (rc < SUCCESS){
        return rc;
    }
#else
    rc = ConnectNetwork(&(ci->n), ci->addr, 1883);
    if (rc < 0)
        return rc;
    ci->port = 1883;
#endif


    /************************************
    * Connect new client
    ************************************/
    rc = MQTTConnect(&(ci->hMQTTClient), &(ci->cdata));

    return rc;
}
