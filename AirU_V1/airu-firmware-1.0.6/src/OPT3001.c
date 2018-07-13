// Common Interface includes
#include "i2c_if.h"

// Application includes
#include "OPT3001.h"

static float ConvertResultToLux(uint16_t res)
{
	uint16_t frac = res & 0x0FFF; // Mask the lower 12 bits for the fraction
	uint8_t exp = (res >> 12) & 0x000F; // Mask the upper 4 bits for the exponent

	// This equation is specified in (3) in the datasheet.
	float lux = 0.01 * (1 << exp) * frac;

	return lux;
}

float GetOPT3001Result(void)
{
	uint8_t regaddr = OPT3001_RESULT_REG;
	uint8_t regval[2];

	I2C_IF_ReadFrom(OPT3001_DEV_ADDR, &regaddr, 1, regval, 2);

	// TODO: Check for error conditions (overflow, etc..)

	uint16_t res = (regval[0] << 8) | regval[1];
	return ConvertResultToLux(res);
}

void ConfigureOPT3001Mode(void)
{
	uint16_t config = GetOPT3001Configuration();

	uint8_t data[2];
	data[0] = OPT3001_CONFIG_REG;

	uint16_t mode_mask = OPT3001_MODE_CONTIN_MASK;

	uint16_t mask = mode_mask;
	data[1] = config & mask;

	I2C_IF_Write(OPT3001_DEV_ADDR, data, 2, 1);
}

uint16_t GetOPT3001Configuration(void)
{
	// Get the current device mode configuration
	uint8_t regaddr = OPT3001_CONFIG_REG;
	uint8_t regval[2];
	I2C_IF_ReadFrom(OPT3001_DEV_ADDR, &regaddr, 1, regval, 2);
	// TODO: Handle error case here.

	uint16_t config = (regval[0] << 8) | regval[1];

	return config;
}

float GetOPT3001LowLimit(void)
{
	// TODO: Implement me!
	uint16_t res = 0;

	return ConvertResultToLux(res);
}

float GetOPT3001HighLimit(void)
{
	// TODO: Implement me!
	uint16_t res = 0;

	return ConvertResultToLux(res);
}

uint16_t GetOPT3001ManufacturerID(void)
{
	uint8_t regaddr = OPT3001_MANU_ID_REG;
	uint8_t regval[2];

	I2C_IF_ReadFrom(OPT3001_DEV_ADDR, &regaddr, 1, regval, 2);

	uint16_t id = (regval[0] << 8) | regval[1];
	return id;
}

uint16_t GetOPT3001DeviceID(void)
{
	uint8_t regaddr = OPT3001_DEV_ID_REG;
	uint8_t regval[2];

	I2C_IF_ReadFrom(OPT3001_DEV_ADDR, &regaddr, 1, regval, 2);

	uint16_t id = (regval[0] << 8) | regval[1];
	return id;
}
