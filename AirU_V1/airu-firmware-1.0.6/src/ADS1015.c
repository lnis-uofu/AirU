/*
 * ADS1015.c
 *
 *  Created on: May 30, 2017
 *      Author: Kyle Tingey
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "i2c_if.h"
#include "uart_if.h"
#include "rom_map.h"

#include "ADS1015.h"
#include "gpio_if.h"
#include "utils.h"

#define FAILURE                -1
#define SUCCESS                 0
#define DEBUG					0


//****************************************************************************
//
// Take the ADC reading from channel 0 (oxidize result)
//
//
// \return 0 if failure, else the results from ADC reading
//
//********************************************************************
uint16_t GetOxidizeResult()
{

	uint16_t config = ADS1015_REG_CONFIG_CQUE_NONE | // Disable the comparator (default val)
			ADS1015_REG_CONFIG_CLAT_NONLAT | // Non-latching (default val)
			ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
			ADS1015_REG_CONFIG_CMODE_TRAD | // Traditional comparator (default val)
			ADS1015_REG_CONFIG_DR_128SPS | // 1600 samples per second (default)
			ADS1015_REG_CONFIG_MODE_CONTIN;   // Single-shot mode (default)
	config |= ADS1015_REG_CONFIG_OS_SINGLE;
	config |= ADS1015_REG_CONFIG_MUX_SINGLE_0; // Channel 0
	// Set Gain
	config |= ADS1015_REG_CONFIG_PGA_1_024V;

	if (DEBUG) {
		Report("\r\nwriteRegister\r\n");
	}
	writeRegister(ADS1015_ADDRESS, ADS1015_REG_POINTER_CONFIG, config);
	//	I2C_IF_Write(ADS1015_ADDRESS, config, 2, 1);
	if (DEBUG) {
		Report("passed\r\n");
	}
	MAP_UtilsDelay(30 * 60 * 1000);
	return readRegister(ADS1015_ADDRESS, ADS1015_REG_POINTER_CONVERT) >> 4;
}
//****************************************************************************
//
// Take the ADC reading from channel 1 (reduce result)
//
//
// \return 0 if failure, else the results from ADC reading
//
//********************************************************************
uint16_t GetReduceResult()
{
	uint16_t config = ADS1015_REG_CONFIG_CQUE_NONE | // Disable the comparator (default val)
			ADS1015_REG_CONFIG_CLAT_NONLAT | // Non-latching (default val)
			ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
			ADS1015_REG_CONFIG_CMODE_TRAD | // Traditional comparator (default val)
			ADS1015_REG_CONFIG_DR_128SPS | // 1600 samples per second (default)
			ADS1015_REG_CONFIG_MODE_CONTIN;   // Single-shot mode (default)

	config |= ADS1015_REG_CONFIG_OS_SINGLE;
	config |= ADS1015_REG_CONFIG_MUX_SINGLE_1; // Channel 1
	// Set Gain
	config |= ADS1015_REG_CONFIG_PGA_4_096V;

	if (DEBUG) {
		Report("\r\nwriteRegister\r\n");
	}
	writeRegister(ADS1015_ADDRESS, ADS1015_REG_POINTER_CONFIG, config);
	//	I2C_IF_Write(ADS1015_ADDRESS, config, 2, 1);
	if (DEBUG) {
		Report("passed\r\n");
	}
	MAP_UtilsDelay(30 * 60 * 1000);
	return readRegister(ADS1015_ADDRESS, ADS1015_REG_POINTER_CONVERT) >> 4;
}

uint16_t ReadADC(uint8_t channel) {
	uint16_t config = ADS1015_REG_CONFIG_CQUE_NONE | // Disable the comparator (default val)
			ADS1015_REG_CONFIG_CLAT_NONLAT | // Non-latching (default val)
			ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
			ADS1015_REG_CONFIG_CMODE_TRAD | // Traditional comparator (default val)
			ADS1015_REG_CONFIG_DR_128SPS |   // 1600 samples per second (default)
			ADS1015_REG_CONFIG_MODE_CONTIN;   // Single-shot mode (default)

	// Set Gain
	config |= ADS1015_REG_CONFIG_PGA_4_096V;

	switch (channel){
		case 0:
			config |= ADS1015_REG_CONFIG_MUX_SINGLE_0;
			break;
		case 1:
			config |= ADS1015_REG_CONFIG_MUX_SINGLE_1;
			break;
		case 2:
			config |= ADS1015_REG_CONFIG_MUX_SINGLE_2;
			break;
		case 3:
			config |= ADS1015_REG_CONFIG_MUX_SINGLE_3;
			break;
		default:
			break;
	}

	config |= ADS1015_REG_CONFIG_OS_SINGLE;

	if(DEBUG) { Report("\r\nwriteRegister\r\n"); }
	writeRegister(ADS1015_ADDRESS, ADS1015_REG_POINTER_CONFIG, config);
//	I2C_IF_Write(ADS1015_ADDRESS, config, 2, 1);
	if(DEBUG) { Report("passed\r\n"); }
	MAP_UtilsDelay(30*60*1000);
	return readRegister(ADS1015_ADDRESS, ADS1015_REG_POINTER_CONVERT) >> 4;
}

//****************************************************************************
//**                       I2C CONNECTION HELPER FUNCTIONS                  **
//****************************************************************************
//****************************************************************************
//
//! Writes a 16-bits value to a specified register on the ADC board
//!
//! \param
//          i2cAddress: specfies the i2c address of the ADC board
//          reg:    specifies the address of the register
//          value:  the value to write into the register
//
//!  \return 0: SUCCESS; >0 Failure
//****************************************************************************
uint16_t writeRegister(uint8_t i2cAddress, uint8_t reg, uint16_t value)
{
    unsigned char DataBuf[3];
    int iRetVal;

    //Construct the Data buffer befor sending
    DataBuf[0] = (unsigned char)reg;
    DataBuf[1] = (unsigned char)(value>>8);
    DataBuf[2] = (unsigned char)(value & 0xFF);

    // Write the data to the specified address
    iRetVal = I2C_IF_Write((unsigned char)i2cAddress, DataBuf, 3, 1);

    if(iRetVal == SUCCESS)
    {
        #ifdef ADC_DEBUG
        //UART_PRINT("I2C Write complete\n\r");
        #endif
        return SUCCESS;
    }
    else
    {
        #ifdef ADC_DEBUG
//        UART_PRINT("I2C Write failed\n\r");
        #endif
        return FAILURE;
    }
}
//****************************************************************************
//
//! Read a 16-bits value from a specified register on ADC board
//!
//! \param
//          i2cAddress: specfies the i2c address of the ADC board
//          reg:    specifies the address of the register
//          value:  the value to write into the register
//
//!  \return 0: SUCCESS; >0 Failure
//****************************************************************************
uint16_t readRegister(uint8_t i2cAddress, uint8_t reg)
{
    unsigned char DataBuf[3];
    int iRetVal;
    //Construct the data buffer
    DataBuf[0] = (unsigned char) ADS1015_REG_POINTER_CONVERT;

    //Send command to the ADC board
    iRetVal = I2C_IF_Write((unsigned char)i2cAddress, DataBuf, 1, 1);
    if(iRetVal == FAILURE)
    {
        #ifdef ADC_DEBUG
//        UART_PRINT("I2C Read failed\n\r");
        #endif
        return FAILURE;
    }
    //Read the value from the register
    iRetVal = I2C_IF_Read((unsigned char)i2cAddress, DataBuf, 2);
    if(iRetVal == SUCCESS)
    {
        #ifdef ADC_DEBUG
        //UART_PRINT("I2C Read complete\n\r");
        #endif
        return (DataBuf[0] << 8 | DataBuf[1]);
    }
    else
    {
        #ifdef ADC_DEBUG
//        UART_PRINT("I2C Read failed\n\r");
        #endif
        return FAILURE;
    }
}


