/*
 * ADBMS6830.c
 *
 *  Created on: Aug 24, 2024
 *      Author: Abhineet
 */


#include "ADBMS6830.h"
#include "delays.h"
#include "main.h"

/*!**********************************************************
 \brief Calculates and returns the CRC10


@param[in]  uint8_t len: the length of the data array being passed to the function

@param[in]  uint8_t data[]: the array of data that the PEC will be generated from


@return  The calculated pec10 as an unsigned int16_t
***********************************************************/
uint16_t pec10_calc(uint8_t len, uint8_t* data) {
	uint16_t remainder;
	uint8_t addr;

	remainder = 16; // initialize the PEC
	for (uint8_t i = 0; i < len; i++) { // loops for each byte in data array
		addr = (uint8_t)(((remainder >> 2) ^ data[i]) & 0xFF); // calculate PEC table address
		remainder = (remainder << 8) ^ crc10Table[addr];
	}
	return remainder;
}

/*!**********************************************************
 \brief Calculates and returns the CRC15


@param[in]  uint8_t len: the length of the data array being passed to the function

@param[in]  uint8_t data[]: the array of data that the PEC will be generated from


@return  The calculated pec15 as an unsigned int16_t
***********************************************************/
uint16_t pec15_calc(uint8_t len, uint8_t* data) {
	uint16_t remainder;
	uint8_t addr;

	remainder = 16; // initialize the PEC
	for (uint8_t i = 0; i < len; i++) { // loops for each byte in data array
		addr = (uint8_t)(((remainder >> 7) ^ data[i]) & 0xFF); // calculate PEC table address
		remainder = (remainder << 8) ^ crc15Table[addr];
	}
	return remainder * 2; // The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2
}


/*
 \brief Writes an array of bytes out of the SPI port

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
 @param[in] uint8_t cmd[4] the command array to be written on the SPI port (4 bytes)
 @param[in] uint8_t** data the 2D array storing the data arrays to be written on the SPI port

*/
void spi_write(SPI_HandleTypeDef* hspi_ptr, // Pointer to the SPI handle
			   TIM_HandleTypeDef* htim_ptr, // Pointer to a timer handle
			   uint8_t cmd[4],              // Array of command bytes to be written on the SPI port
			   uint8_t** data               // 2D array storing data bytes for each IC (can be set to NULL if not a write command)
			   )
{
	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_RESET);
	delay_500_ns(htim_ptr);

	HAL_SPI_Transmit(hspi_ptr, cmd, 4, 100);

	if (data != NULL) {
		for (uint8_t ic = N_OF_ADBMS - 1; ic >= 0; ic--) { // Data written to last IC in the chain first and first IC in the chain last (see bus protocols in datasheet)
			HAL_SPI_Transmit(hspi_ptr, data[ic], 8, 100); // For write commands, there will always be 8 bytes per IC (6 register bytes + 2 PEC bytes)
		}
	}
	delay_500_ns(htim_ptr);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_SET);
	delay_us(htim_ptr, 2);
}


/*
 \brief Writes and read a set number of bytes using the SPI port.

@param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
@param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
@param[in] uint8_t cmd[4] the command array to be written on the SPI port (4 bytes)
@param[out] uint8_t rx_data[N_OF_ADBMS][8] 2D array that read data will be written to (8 bytes per IC)

*/
void spi_write_read(SPI_HandleTypeDef* hspi_ptr,    // Pointer to the SPI handle
					TIM_HandleTypeDef* htim_ptr,    // Pointer to a timer handle
					uint8_t cmd[4],                 // Array of command bytes to be written on SPI port
					uint8_t rx_data[N_OF_ADBMS][8]  // Input: 2D array that will store the data read by the SPI port
					)
{
	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_RESET);
	delay_500_ns(htim_ptr);

	HAL_SPI_Transmit(hspi_ptr, cmd, 4, 100);
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) { // Data read from first IC in the chain first and last IC in the chain last (see bus protocols in datasheet)
		HAL_SPI_Receive(hspi_ptr, rx_data[ic], 8, 100); // 6 register bytes + 2 PEC bytes = 8 bytes per IC
	}
	delay_500_ns(htim_ptr);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_SET);
	delay_us(htim_ptr, 2);
}
