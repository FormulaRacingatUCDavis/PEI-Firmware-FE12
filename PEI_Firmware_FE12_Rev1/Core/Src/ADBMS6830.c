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
 \brief Calculates and returns the CRC15


@param[in]  uint8_t len: the length of the data array being passed to the function

@param[in]  uint8_t data[] : the array of data that the PEC will be generated from


@return  The calculated pec15 as an unsigned int16_t
***********************************************************/
uint16_t pec15_calc(uint8_t len, uint8_t *data)
{
	uint16_t remainder;
	uint8_t addr;

	remainder = 16; // initialize the PEC
	for(uint8_t i = 0; i < len; i++) // loops for each byte in data array
	{
		addr = (uint8_t)(((remainder >> 7) ^ data[i]) & 0xFF); // calculate PEC table address
		remainder = (remainder << 8) ^ crc15Table[addr];
	}
	return remainder * 2; // The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2
}


/*
 \brief Writes an array of bytes out of the SPI port

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
 @param[in] uint8_t len length of the data array being written on the SPI port
 @param[in] uint8_t data[] the data array to be written on the SPI port

*/
void spi_write(SPI_HandleTypeDef* hspi_ptr, // Pointer to the SPI handle
			   TIM_HandleTypeDef* htim_ptr, // Pointer to a timer handle
			   uint8_t data[],              // Array of bytes to be written on the SPI port
			   uint8_t len                  // Option: Number of bytes to be written on the SPI port
			   )
{
	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_RESET);
	delay_500_ns(htim_ptr);

	HAL_SPI_Transmit(hspi_ptr, data, len, 100);
	delay_500_ns(htim_ptr);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_SET);
	delay_us(htim_ptr, 2);
}


/*
 \brief Writes and read a set number of bytes using the SPI port.

@param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
@param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
@param[in] uint8_t tx_data[] array of data to be written on the SPI port
@param[in] uint8_t tx_len length of the tx_data array
@param[out] uint8_t rx_data array that read data will be written too.
@param[in] uint8_t rx_len number of bytes to be read from the SPI port.

*/
void spi_write_read(SPI_HandleTypeDef* hspi_ptr, // pointer to the SPI handle
					TIM_HandleTypeDef* htim_ptr, // pointer to a timer handle
					uint8_t tx_data[], // array of data to be written on SPI port
					uint8_t tx_len, // length of the tx data array
					uint8_t *rx_data, // Input: array that will store the data read by the SPI port
					uint8_t rx_len // Option: number of bytes to be read from the SPI port
					)
{
	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_RESET);
	delay_500_ns(htim_ptr);

	HAL_SPI_Transmit(hspi_ptr, tx_data, tx_len, 100);
	HAL_SPI_Receive(hspi_ptr, rx_data, rx_len, 100);
	delay_500_ns(htim_ptr);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_SET);
	delay_us(htim_ptr, 2);
}
