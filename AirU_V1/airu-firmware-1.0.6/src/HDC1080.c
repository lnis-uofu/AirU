// Common Interface includes
#include "i2c_if.h"
#include "gpio_if.h"
#include "rom_map.h"
#include "utils.h"
#include "uart_if.h"
#include "osi.h"
#include "common.h"
// Application includes
#include "HDC1080.h"


uint16_t GetHDC1080Configuration(void)
{
    // Get the current device mode configuration
    uint8_t regaddr = HDC1080_CONFIG_REG;
    uint8_t regval[2];
    I2C_IF_ReadFrom(HDC1080_DEV_ADDR, &regaddr, 1, regval, 2);
    // TODO: Handle error case here.

    uint16_t config = (regval[0] << 8) | regval[1];

    return config;
}

void ConfigureHDC1080Mode(void)
{
    uint16_t config = GetHDC1080Configuration();

    // Configure the acquisition parameters in the Configuration register
    //  (a) Set the acquisition mode to independently measure temperature or humidity by setting Bit[12] to 0.
    //  (b) Set the desired temperature measurement resolution:
    //      - Set Bit[10] to 0 for 14 bit resolution.
    //      - Set Bit[10] to 1 for 11 bit resolution.
    //  (c) Set the desired humidity measurement resolution:
    //      - Set Bit[9:8] to 00 for 14 bit resolution.
    //      - Set Bit[9:8] to 01 for 11 bit resolution.
    //      - Set Bit[9:8] to 10 for 8 bit resolution.
    uint8_t data[2];
    data[0] = HDC1080_CONFIG_REG;
    uint16_t mode_mask = ~(1 << 12);           // Independent (single shot) mode
    uint16_t temp_mask = ~(1 << 10);           // 14 bit resolution
    uint16_t hum_mask = ~(1 << 9) & ~(1 << 8); // 14 bit resolution
    uint16_t mask = mode_mask & temp_mask & hum_mask;
    data[1] = config & mask;

    I2C_IF_Write(HDC1080_DEV_ADDR, data, 2, 1);
    // TODO: Handle the error case here.
}

long GetTemperature(double *temperature)
{
    // Trigger the measurement by executing a pointer write transaction to the Temperature register.
    uint8_t regaddr = HDC1080_TEMP_REG;
    uint8_t regval[2];
    int iRetVal = -1;

    unsigned long res = 0;

    iRetVal = I2C_IF_Write(HDC1080_DEV_ADDR, &regaddr, 1, 0);

    MAP_UtilsDelay((60*30*1000)); // 7.5msec

    iRetVal |= I2C_IF_Read(HDC1080_DEV_ADDR, regval, 2);

    res = (regval[0]<<8) | regval[1];

    *temperature = ( (float)res / (1<<16) ) * 165.0 - 40.0;
    return (long)iRetVal;
}

long GetHumidity(double *humidity)
{
    // Trigger the measurement by executing a pointer write transaction to the Humidity register.
    int iRetVal = -1;
    uint8_t regaddr = HDC1080_HUM_REG;
    uint8_t regval[2];

    iRetVal = I2C_IF_Write(HDC1080_DEV_ADDR, &regaddr, 1, 0);

    MAP_UtilsDelay((60*30*1000)); // 7.5msec

    iRetVal |= I2C_IF_Read(HDC1080_DEV_ADDR, regval, 2);

    uint16_t res = (regval[0] << 8) | regval[1];

    *humidity = ((double) res / (1 << 16))*100;
    return (long)iRetVal;
}

uint16_t GetHDC1080ManufacturerID(void)
{
    uint8_t regaddr = HDC1080_MANU_ID_REG;
    uint8_t regval[2];

    I2C_IF_ReadFrom(HDC1080_DEV_ADDR, &regaddr, 1, regval, 2);

    uint16_t id = (regval[0] << 8) | regval[1];

    return id;
}

uint16_t GetHDC1080DeviceID(void)
{
    // TODO: Fill out this implementation.
    return 0;
}
