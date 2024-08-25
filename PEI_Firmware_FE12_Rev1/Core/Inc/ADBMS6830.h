/*
 * ADBMS6830.h
 *
 *  Created on: Aug 23, 2024
 *      Author: Abhineet
 */

#ifndef INC_ADBMS6830_H_
#define INC_ADBMS6830_H_

#include <stdint.h>
#include "stm32f7xx_hal.h"

#include "data.h"

#define WAKE_UP_DELAY_US 3653 // number of microseconds to wait after sending wakeup signal

static const unsigned int crc15Table[256] = {0x0,0xc599, 0xceab, 0xb32, 0xd8cf, 0x1d56, 0x1664, 0xd3fd, 0xf407, 0x319e, 0x3aac,  // precomputed CRC15 Table
0xff35, 0x2cc8, 0xe951, 0xe263, 0x27fa, 0xad97, 0x680e, 0x633c, 0xa6a5, 0x7558, 0xb0c1,
0xbbf3, 0x7e6a, 0x5990, 0x9c09, 0x973b, 0x52a2, 0x815f, 0x44c6, 0x4ff4, 0x8a6d, 0x5b2e,
0x9eb7, 0x9585, 0x501c, 0x83e1, 0x4678, 0x4d4a, 0x88d3, 0xaf29, 0x6ab0, 0x6182, 0xa41b,
0x77e6, 0xb27f, 0xb94d, 0x7cd4, 0xf6b9, 0x3320, 0x3812, 0xfd8b, 0x2e76, 0xebef, 0xe0dd,
0x2544, 0x2be, 0xc727, 0xcc15, 0x98c, 0xda71, 0x1fe8, 0x14da, 0xd143, 0xf3c5, 0x365c,
0x3d6e, 0xf8f7,0x2b0a, 0xee93, 0xe5a1, 0x2038, 0x7c2, 0xc25b, 0xc969, 0xcf0, 0xdf0d,
0x1a94, 0x11a6, 0xd43f, 0x5e52, 0x9bcb, 0x90f9, 0x5560, 0x869d, 0x4304, 0x4836, 0x8daf,
0xaa55, 0x6fcc, 0x64fe, 0xa167, 0x729a, 0xb703, 0xbc31, 0x79a8, 0xa8eb, 0x6d72, 0x6640,
0xa3d9, 0x7024, 0xb5bd, 0xbe8f, 0x7b16, 0x5cec, 0x9975, 0x9247, 0x57de, 0x8423, 0x41ba,
0x4a88, 0x8f11, 0x57c, 0xc0e5, 0xcbd7, 0xe4e, 0xddb3, 0x182a, 0x1318, 0xd681, 0xf17b,
0x34e2, 0x3fd0, 0xfa49, 0x29b4, 0xec2d, 0xe71f, 0x2286, 0xa213, 0x678a, 0x6cb8, 0xa921,
0x7adc, 0xbf45, 0xb477, 0x71ee, 0x5614, 0x938d, 0x98bf, 0x5d26, 0x8edb, 0x4b42, 0x4070,
0x85e9, 0xf84, 0xca1d, 0xc12f, 0x4b6, 0xd74b, 0x12d2, 0x19e0, 0xdc79, 0xfb83, 0x3e1a, 0x3528,
0xf0b1, 0x234c, 0xe6d5, 0xede7, 0x287e, 0xf93d, 0x3ca4, 0x3796, 0xf20f, 0x21f2, 0xe46b, 0xef59,
0x2ac0, 0xd3a, 0xc8a3, 0xc391, 0x608, 0xd5f5, 0x106c, 0x1b5e, 0xdec7, 0x54aa, 0x9133, 0x9a01,
0x5f98, 0x8c65, 0x49fc, 0x42ce, 0x8757, 0xa0ad, 0x6534, 0x6e06, 0xab9f, 0x7862, 0xbdfb, 0xb6c9,
0x7350, 0x51d6, 0x944f, 0x9f7d, 0x5ae4, 0x8919, 0x4c80, 0x47b2, 0x822b, 0xa5d1, 0x6048, 0x6b7a,
0xaee3, 0x7d1e, 0xb887, 0xb3b5, 0x762c, 0xfc41, 0x39d8, 0x32ea, 0xf773, 0x248e, 0xe117, 0xea25,
0x2fbc, 0x846, 0xcddf, 0xc6ed, 0x374, 0xd089, 0x1510, 0x1e22, 0xdbbb, 0xaf8, 0xcf61, 0xc453,
0x1ca, 0xd237, 0x17ae, 0x1c9c, 0xd905, 0xfeff, 0x3b66, 0x3054, 0xf5cd, 0x2630, 0xe3a9, 0xe89b,
0x2d02, 0xa76f, 0x62f6, 0x69c4, 0xac5d, 0x7fa0, 0xba39, 0xb10b, 0x7492, 0x5368, 0x96f1, 0x9dc3,
0x585a, 0x8ba7, 0x4e3e, 0x450c, 0x8095};

/*
  |CHG   | Dec  |Channels to convert   |
  |------|------|----------------------|
  |00000 | 0    | Everything           |
  |00001 | 1    | GPIO 1 			   |
  |00010 | 2    | GPIO 2               |
  |00011 | 3    | GPIO 3 			   |
  |00100 | 4    | GPIO 4 			   |
  |00101 | 5    | GPIO 5 			   |
  |00110 | 6    | GPIO 6  			   |
  |00111 | 7    | GPIO 7               |
  |01000 | 8    | GPIO 8               |
  |01001 | 9    | GPIO 9               |
  |01010 | 10   | GPIO 10              |
  |10000 | 16   | VREF2                |
  |10001 | 17   | VD                   |
  |10010 | 18   | VA                   |
  |10011 | 19   | ITEMP                |
  |10100 | 20   | VPV                  |
  |10101 | 21   | VMV                  |
  |10110 | 22   | VRES                 |
  |10111 | 23   | Reserved             |
*/
#define AUX_CH_ALL 0
#define AUX_CH_GPIO1 1
#define AUX_CH_GPIO2 2
#define AUX_CH_GPIO3 3
#define AUX_CH_GPIO4 4
#define AUX_CH_GPIO5 5
#define AUX_CH_GPIO6 6
#define AUX_CH_GPIO7 7
#define AUX_CH_GPIO8 8
#define AUX_CH_GPIO9 9
#define AUX_CH_GPIO10 10
#define AUX_CH_VREF2 16
#define AUX_CH_VD 17
#define AUX_CH_VA 18
#define AUX_CH_ITEMP 19
#define AUX_CH_VPV 20
#define AUX_CH_VMV 21
#define AUX_CH_VRES 22
#define AUX_CH_RESERVED 23

/*****************************************************
  Brief controls if discharging is enabled
  or disabled during Cell conversions.

 |DCP | Discharge Permitted During conversion |
 |----|----------------------------------------|
 |0   | No - discharge is not permitted         |
 |1   | Yes - discharge is permitted           |

*******************************************************/
#define DCP_DISABLED 0
#define DCP_ENABLED 1

typedef enum {
    GPIO1,
    GPIO2,
    GPIO3,
    GPIO4,
    GPIO5,
	GPIO6,
	GPIO7,
	GPIO8,
	GPIO9,
	GPIO10,
    VREF2,
	VD,
	VA,
	ITEMP,
	VPV,
	VMV,
	VRES,
	RESERVED
} AuxPin_t;

// Default values for CFGA registers
#define CFGA0 0x80 // REFON enabled (same as last year)
#define CFGA1 0x00
#define CFGA2 0x80 // Soak time enabled for Aux GPIO
#define CFGA3 0xFF // Pull-down resistor disabled for Aux GPIO 1-8  <-- same as last year
#define CFGA4 0x03 // Pull-down resistor disabled for Aux GPIO 9-10 <--/
#define CFGA5 0x00

// Default values for CFGB registers
#define CFGB0 0x00
#define CFGB1 0x00
#define CFGB2 0x00
#define CFGB3 0x82 // DTMEN enabled, discharge timer set to 1 minute
#define CFGB4 0x00
#define CFGB5 0x00

void ADBMS6830_initialize(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);

void ADBMS6830_set_C_ADC(uint8_t RD, uint8_t CONT, uint8_t DCP, uint8_t RSTF, uint8_t OW);
void ADBMS6830_set_S_ADC(uint8_t CONT, uint8_t DCP, uint8_t RSTF, uint8_t OW);
void ADBMS6830_set_Aux_ADC(uint8_t OW, uint8_t PUP, uint8_t CH);

void ADBMS6830_set_discharge(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr, uint8_t discharge_enable, uint8_t cell_num);

void ADBMS6830_wakeup(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void ADBMS6830_wrcfga(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void ADBMS6830_wrcfgb(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void ADBMS6830_wrpwma(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void ADBMS6830_wrpwmb(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);

void ADBMS6830_adcv(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void ADBMS6830_adsv(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void ADBMS6830_adax(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);

void ADBMS6830_rdcv(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr, uint16_t* voltages);
void ADBMS6830_rdaux_pin(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr, AuxPin_t pin, uint16_t* aux);
void ADBMS6830_rdaux_GPIO(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr, uint16_t* temps);
void ADBMS6830_rdstatc_mismatch(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr, uint8_t* mismatches);

#endif /* INC_ADBMS6830_H_ */
