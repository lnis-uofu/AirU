/*
 * influxdb.h
 *
 *  Created on: Jun 23, 2017
 *      Author: Tom
 */

#ifndef INCLUDE_INFLUXDB_H_
#define INCLUDE_INFLUXDB_H_

#define INFLUXDB_PORT       8086
#define PROXY_IP            <proxy_ip>
#define PROXY_PORT          <proxy_port>
#define POST_REQUEST_URI    "/write?db=airU&u=airuSensor&p=YBK2bCkCK5qWwDts"
#define POST_DATA_AIR   \
"airQuality\,ID\=%s\,SensorModel\=H%s+S%s\ POSIX\=%lu\,SecActive\=%lu\,Altitude\=%.2f\,Latitude\=%f\,Longitude\=%f\,PM1\=%.3f\,PM2.5\=%.3f\,PM10\=%.3f\,Temperature\=%.2f\,Humidity\=%.2f\,CO\=%lu,NO\=%lu"

#define POST_DATA_FACTORY   \
"airQuality\,ID\=%s\,SensorModel\=H%s+S%s\ POSIX\=%lu\,SecActive\=%lu\,Altitude\=%.2f\,Latitude\=%f\,Longitude\=%f\,PM1\=%.3f\,PM2.5\=%.3f\,PM10\=%.3f\,Temperature\=%.2f\,Humidity\=%.2f\,CO\=%lu [ %i ]"

#define INFLUXDB_DNS_NAME   "air.eng.utah.edu"
#define SENSOR_MAC          "TOM0123"

typedef struct influxDBDataPoint {

    long timeStamp;
    double latitude,longitude, altitude;
    float ambientLight;
    float temperature;
    float humidity;
    double pm1,pm2_5, pm10;
    double pm2_avg;
    unsigned int co;
    unsigned int nox;

} influxDBDataPoint;

#endif /* INCLUDE_INFLUXDB_H_ */
