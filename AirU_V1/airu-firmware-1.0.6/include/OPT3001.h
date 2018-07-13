#include <stdint.h>

#ifndef __OPT3001_H__
#define __OPT3001_H__

//*****************************************************************************
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

#define OPT3001_DEV_ADDR 0x44 // Addr Pin tied to GND
#define OPT3001_CONV_TIME 850 // milliseconds

// Register Map
#define OPT3001_RESULT_REG   0x00 // Result Register Address
#define OPT3001_CONFIG_REG   0x01 // Configuration Register Address
#define OPT3001_LOW_LIM_REG  0x02 // Low Limit Register Address
#define OPT3001_HIGH_LIM_REG 0x03 // High Limit Register Address
#define OPT3001_MANU_ID_REG  0x7E // Manufacturer ID Register Address
#define OPT3001_DEV_ID_REG   0x7F // Device ID Register Address

#define OPT3001_MODE_SHUTDOWN_MASK 0xF9FF // Shutdown Mode - Bits 10:9 [0, 0]
#define OPT3001_MODE_SINGLE_MASK   0xFBFF // Single-shot Mode - Bits 10:9 [0, 1]
#define OPT3001_MODE_CONTIN_MASK   0xFDFF // Continuous Mode - Bits 10:9 [1, 0]
#define OPT3001_CONFIG_800		   0xCC10 // 0b1100_1100_0001_0000
#define OPT3001_CONFIG_800_OS      0xCA10 // 0b1100_1010_0001_0000

void ConfigureOPT3001Mode(void);
float GetOPT3001Result(void);
uint16_t GetOPT3001Configuration(void);
float GetOPT3001LowLimit(void);
float GetOPT3001HighLimit(void);
uint16_t GetOPT3001ManufacturerID(void);
uint16_t GetOPT3001DeviceID(void);

float ConvertResultToLux(uint16_t);

//*****************************************************************************
// Mark the end of the C bindings section for C++ compilers.
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif /* __OPT3001_H__ */
