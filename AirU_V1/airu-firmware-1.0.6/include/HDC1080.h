#include <stdint.h>

#ifndef __HDC1080_H__
#define __HDC1080_H__

//*****************************************************************************
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

#define HDC1080_DEV_ADDR    0x40

#define HDC1080_TEMP_REG    0x00
#define HDC1080_HUM_REG     0x01
#define HDC1080_CONFIG_REG  0x02
#define HDC1080_MANU_ID_REG 0xFE
#define HDC1080_DEV_ID_REG  0xFF

void ConfigureHDC1080Mode(void);
long GetTemperature(double *temperature);
long GetHumidity(double *humidity);
uint16_t GetHDC1080Configuration(void);
uint16_t GetHDC1080ManufacturerID(void);
uint16_t GetHDC1080DeviceID(void);

//*****************************************************************************
// Mark the end of the C bindings section for C++ compilers.
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif /* __HDC1080_H__ */
