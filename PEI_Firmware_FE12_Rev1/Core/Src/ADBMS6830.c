/*
 * ADBMS6830.c
 *
 *  Created on: Aug 24, 2024
 *      Author: Abhineet
 */


#include "ADBMS6830.h"
#include "delays.h"
#include "word_processing.h"
#include "main.h"

uint8_t ADCV[2]; // Cell voltage conversion command
uint8_t ADSV[2]; // Redundant cell voltage conversion command
uint8_t ADAX[2]; // Auxiliary ADC conversion command

uint8_t tx_cfga[N_OF_ADBMS][6]; // Stores CFGA data to be written to each IC
uint8_t tx_cfgb[N_OF_ADBMS][6]; // Stores CFGB data to be written to each IC
uint8_t tx_pwma[N_OF_ADBMS][6]; // Stores PWMA data to be written to each IC
uint8_t tx_pwmb[N_OF_ADBMS][6]; // Stores PWMB data to be written to each IC

/*
 \brief Calculates and returns the CRC10 of a given register group

 @param[in] uint8_t data[]: the array of data that the PEC will be generated from (6 bytes)

 @returns The calculated pec10 as an unsigned int16_t
*/
uint16_t data_pec_calc(uint8_t data[]) {
	uint16_t remainder;
	uint8_t addr;

	remainder = 16; // initialize the PEC
	for (uint8_t i = 0; i < 6; i++) { // loops for each byte in data array (there are 6 bytes in a register group)
		addr = (uint8_t)(((remainder >> 2) ^ data[i]) & 0xFF); // calculate PEC table address
		remainder = (remainder << 8) ^ crc10Table[addr];
	}
	return remainder;
}

/*
 \brief Calculates and returns the CRC15 of a given command

 @param[in] uint8_t cmd[2]: the command array that the PEC will be generated from (2 bytes)

 @returns The calculated pec15 as an unsigned int16_t
*/
uint16_t cmd_pec_calc(uint8_t cmd[2]) {
	uint16_t remainder;
	uint8_t addr;

	remainder = 16; // initialize the PEC
	for (uint8_t i = 0; i < 2; i++) { // loops for each byte in command array
		addr = (uint8_t)(((remainder >> 7) ^ cmd[i]) & 0xFF); // calculate PEC table address
		remainder = (remainder << 8) ^ crc15Table[addr];
	}
	return remainder * 2; // The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2
}


/*
 \brief Writes an array of bytes out of the SPI port

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
 @param[in] uint8_t cmd[2] the command array to be written on the SPI port (2 bytes)
 @param[in] uint8_t* data[N_OF_ADBMS][6] the 2D array storing the data arrays to be written on the SPI port
*/
void spi_write(SPI_HandleTypeDef* hspi_ptr, // Pointer to the SPI handle
			   TIM_HandleTypeDef* htim_ptr, // Pointer to a timer handle
			   uint8_t cmd[2],              // Array of command bytes to be written on the SPI port
			   uint8_t data[N_OF_ADBMS][6]  // 2D array storing data bytes for each IC (can be set to NULL if not a write command)
			   )
{
	// Data prep (putting everything into a single array + PEC calculations)

	uint8_t tx_len = 4; // 2 command bytes + 2 PEC bytes
	if (data != NULL) tx_len += N_OF_ADBMS * 8; // For write commands, there will always be 8 bytes per IC (6 register bytes + 2 PEC bytes)
	uint8_t tx_data[tx_len];
	uint16_t cmd_pec;

	tx_data[0] = cmd[0];
	tx_data[1] = cmd[1];

	cmd_pec = cmd_pec_calc(cmd);
	tx_data[2] = HI8(cmd_pec);
	tx_data[3] = LO8(cmd_pec);

	if (data != NULL) {
		uint8_t idx = 4;

		for (uint8_t ic = N_OF_ADBMS - 1; ic >= 0; ic--) { // Data written to last IC in the chain first and first IC in the chain last (see bus protocols in datasheet)
			uint16_t data_pec;

			for (uint8_t i = 0; i < 6; i++) {
				tx_data[idx] = data[ic][i];
				idx++;
			}

			data_pec = data_pec_calc(data[ic]);
			tx_data[idx] = HI8(data_pec);
			tx_data[idx + 1] = LO8(data_pec);
			idx += 2;
		}
	}

	// Data transmission

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_RESET);
	delay_500_ns(htim_ptr);

	HAL_SPI_Transmit(hspi_ptr, tx_data, tx_len, 100);
	delay_500_ns(htim_ptr);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_SET);
	delay_us(htim_ptr, 2);
}


/*
 \brief Writes and reads a set number of bytes using the SPI port.

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
 @param[in] uint8_t cmd[2] the command array to be written on the SPI port (2 bytes)

 @param[out] uint8_t rx_data[N_OF_ADBMS][6] 2D array that read data will be written to (6 bytes per IC)
 @param[out] uint8_t pec_mismatches Array containing flags indicating which nodes had PEC mismatches

 @returns Whether the transaction occurred without any PEC mismatches
*/
uint8_t spi_write_read(SPI_HandleTypeDef* hspi_ptr,        // Pointer to the SPI handle
					   TIM_HandleTypeDef* htim_ptr,        // Pointer to a timer handle
					   uint8_t cmd[2],                     // Array of command bytes to be written on SPI port
					   uint8_t rx_data[N_OF_ADBMS][6],     // Input: 2D array that will store the data read by the SPI port
					   uint8_t pec_mismatches[N_OF_ADBMS]  // Input: Array containing flags indicating which nodes had PEC mismatches
					   )
{
	uint8_t rx_len = N_OF_ADBMS * 8; // 6 register bytes + 2 PEC bytes = 8 bytes per IC
	uint8_t rx_data_flattened[rx_len]; // array to store data from all ICs
	uint8_t tx_data[4];
	uint16_t cmd_pec;
	uint8_t no_errors = 1;

	tx_data[0] = cmd[0];
	tx_data[1] = cmd[1];

	cmd_pec = cmd_pec_calc(cmd);
	tx_data[2] = HI8(cmd_pec);
	tx_data[3] = LO8(cmd_pec);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_RESET);
	delay_500_ns(htim_ptr);

	HAL_SPI_Transmit(hspi_ptr, tx_data, 4, 100);
	HAL_SPI_Receive(hspi_ptr, rx_data_flattened, rx_len, 100);
	delay_500_ns(htim_ptr);

	HAL_GPIO_WritePin(SPI5_CS_GPIO_Port, SPI5_CS_Pin, GPIO_PIN_SET);
	delay_us(htim_ptr, 2);

	// Package data (excluding PEC) into 2D array and process PECs
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) { // Data read from first IC in the chain first and last IC in the chain last (see bus protocols in datasheet)
		uint16_t data_pec, received_pec;

		for (uint8_t i = 0; i < 6; i++) {
			rx_data[ic][i] = rx_data_flattened[(ic * 8) + i];
		}

		data_pec = data_pec_calc(rx_data[ic]);
		received_pec = rx_data_flattened[(ic * 8) + 6] << 8;
		received_pec += rx_data_flattened[(ic * 8) + 7];

		if (data_pec != received_pec) {
			no_errors = 0;
			pec_mismatches[ic] = 1;
		}
		else {
			pec_mismatches[ic] = 0;
		}
	}

	return no_errors;
}

// Set default configs
void ADBMS6830_initialize(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		tx_cfga[ic][0] = CFGA0;
		tx_cfga[ic][1] = CFGA1;
		tx_cfga[ic][2] = CFGA2;
		tx_cfga[ic][3] = CFGA3;
		tx_cfga[ic][4] = CFGA4;
		tx_cfga[ic][5] = CFGA5;

		tx_cfgb[ic][0] = CFGB0;
		tx_cfgb[ic][1] = CFGB1;
		tx_cfgb[ic][2] = CFGB2;
		tx_cfgb[ic][3] = CFGB3;
		tx_cfgb[ic][4] = CFGB4;
		tx_cfgb[ic][5] = CFGB5;

		tx_pwma[ic][0] = PWMA0;
		tx_pwma[ic][1] = PWMA1;
		tx_pwma[ic][2] = PWMA2;
		tx_pwma[ic][3] = PWMA3;
		tx_pwma[ic][4] = PWMA4;
		tx_pwma[ic][5] = PWMA5;

		tx_pwmb[ic][0] = PWMB0;
		tx_pwmb[ic][1] = PWMB1;
		tx_pwmb[ic][2] = PWMB2;
		tx_pwmb[ic][3] = PWMB3;
		tx_pwmb[ic][4] = PWMB4;
		tx_pwmb[ic][5] = PWMB5;
	}

	ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	ADBMS6830_wrcfga(hspi_ptr, htim_ptr);
	ADBMS6830_wrcfgb(hspi_ptr, htim_ptr);

	// See Cell Discharge With Cell Measurements and Cell Diagnostics section of datasheet
	ADBMS6830_set_C_ADC(0, 1, 0, 0, CELL_OW_DISABLED);
	ADBMS6830_adcv(hspi_ptr, htim_ptr);
}

/*
 \brief Sets the configuration for the cell ADCs

 @param[in] uint8_t RD Whether or not to run the conversion with redundancy
 @param[in] uint8_t CONT Whether or not the ADC should run continuously
 @param[in] uint8_t DCP Whether or not PWM discharge is permitted during ADC conversion
 @param[in] uint8_t RSTF Whether or not to reset the digital filter
 @param[in] uint8_t OW Determines open wire detection mode
*/
void ADBMS6830_set_C_ADC(uint8_t RD, uint8_t CONT, uint8_t DCP, uint8_t RSTF, uint8_t OW) {
	ADCV[0] = 0x02;
	ADCV[0] += RD;

	ADCV[1] = 0x60;
	ADCV[1] += CONT << 7;
	ADCV[1] += DCP << 4;
	ADCV[1] += RSTF << 2;
	ADCV[1] += OW;
}

/*
 \brief Sets the configuration for the redundant cell ADCs

 @param[in] uint8_t CONT Whether or not the ADC should run continuously
 @param[in] uint8_t DCP Whether or not PWM discharge is permitted during ADC conversion
 @param[in] uint8_t RSTF Whether or not to reset the IIR filter
 @param[in] uint8_t OW Determines open wire detection mode
*/
void ADBMS6830_set_S_ADC(uint8_t CONT, uint8_t DCP, uint8_t RSTF, uint8_t OW) {
	ADSV[0] = 0x01;

	ADSV[1] = 0x68;
	ADSV[1] += CONT << 7;
	ADSV[1] += DCP << 4;
	ADSV[1] += OW;
}

/*
 \brief Sets the configuration for the redundant cell ADCs

 @param[in] uint8_t OW Determines whether conversion runs with open wire detection or not
 @param[in] uint8_t PUP Determines whether pull-up or pull-down current sources are applied during conversion
 @param[in] uint8_t CH Determines which channels are converted
*/
void ADBMS6830_set_Aux_ADC(uint8_t OW, uint8_t PUP, uint8_t CH) {
	ADAX[0] = 0x04;
	ADAX[0] += OW;

	ADAX[1] = 0x10;
	ADAX[1] += PUP << 7;
	ADAX[1] += (CH >> 4) << 6;
	ADAX[1] += CH & 0x0F;
}

void ADBMS6830_set_discharge(uint8_t ic_num, uint8_t cell_num) {
	uint8_t reg_idx;
	uint8_t config_val = 0b1001; // Set to 60% duty cycle
	cell_num++; // Cell numbering in PWM register group is 1-based
	if ((cell_num % 2) == 0) config_val <<= 4;

	if (cell_num < 13) {
		reg_idx = (cell_num - 1) / 2;

		if ((cell_num % 2) == 0) config_val += LO4(tx_pwma[ic_num][reg_idx]);
		else config_val += HI4(tx_pwma[ic_num][reg_idx]) << 4;

		tx_pwma[ic_num][reg_idx] = config_val;
	}
	else {
		reg_idx = (cell_num - 13) / 2;

		if ((cell_num % 2) == 0) config_val += LO4(tx_pwmb[ic_num][reg_idx]);
		else config_val += HI4(tx_pwmb[ic_num][reg_idx]) << 4;

		tx_pwmb[ic_num][reg_idx] = config_val;
	}
}

// Resets PWM registers to default values
void ADBMS6830_reset_discharge() {
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		tx_pwma[ic][0] = PWMA0;
		tx_pwma[ic][1] = PWMA1;
		tx_pwma[ic][2] = PWMA2;
		tx_pwma[ic][3] = PWMA3;
		tx_pwma[ic][4] = PWMA4;
		tx_pwma[ic][5] = PWMA5;

		tx_pwmb[ic][0] = PWMB0;
		tx_pwmb[ic][1] = PWMB1;
		tx_pwmb[ic][2] = PWMB2;
		tx_pwmb[ic][3] = PWMB3;
		tx_pwmb[ic][4] = PWMB4;
		tx_pwmb[ic][5] = PWMB5;
	}
}

void ADBMS6830_wakeup(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	// Send dummy RCFGA command to wake up ICs

	uint8_t cmd[2] = {0x00, 0x02};
	uint8_t rx_data[N_OF_ADBMS][6];
	uint8_t pec_mismatches[N_OF_ADBMS];

	spi_write_read(hspi_ptr, htim_ptr, cmd, rx_data, pec_mismatches);
	delay_us(htim_ptr, WAKE_UP_DELAY_US);
}

// Write to configuration register group A
void ADBMS6830_wrcfga(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	uint8_t cmd[2] = {0x00, 0x01};
	spi_write(hspi_ptr, htim_ptr, cmd, tx_cfga);
}

// Write to configuration register group B
void ADBMS6830_wrcfgb(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	uint8_t cmd[2] = {0x00, 0x04};
	spi_write(hspi_ptr, htim_ptr, cmd, tx_cfgb);
}

// Write to PWM register group A
void ADBMS6830_wrpwma(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	uint8_t cmd[2] = {0x00, 0x20};
	spi_write(hspi_ptr, htim_ptr, cmd, tx_pwma);
}

// Write to PWM register group B
void ADBMS6830_wrpwmb(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	uint8_t cmd[2] = {0x00, 0x21};
	spi_write(hspi_ptr, htim_ptr, cmd, tx_pwmb);
}

// Start cell voltage ADC conversions
void ADBMS6830_adcv(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	spi_write(hspi_ptr, htim_ptr, ADCV, NULL);
	HAL_Delay(5);
}

// Start redundant cell voltage ADC conversions
void ADBMS6830_adsv(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	spi_write(hspi_ptr, htim_ptr, ADSV, NULL);
	HAL_Delay(8);
}

// Start auxiliary ADC conversions
void ADBMS6830_adax(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	spi_write(hspi_ptr, htim_ptr, ADAX, NULL);
	HAL_Delay(5);
}

// Freeze result registers
void ADBMS6830_freeze_results(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		tx_cfga[ic][5] = CFGA5 + 0x20; // turn on snapshot bit
	}
	ADBMS6830_wrcfga(hspi_ptr, htim_ptr);
}

// Allow result registers to be updated again
void ADBMS6830_unfreeze_results(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		tx_cfga[ic][5] = CFGA5; // reset config to default
	}
	ADBMS6830_wrcfga(hspi_ptr, htim_ptr);
}

/*
 \brief Reads back filtered cell voltages from one register group for each IC

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
 @param[in] RegGroup_t reg filtered cell voltage register group to read from

 @param[out] uint8_t data[N_OF_ADBMS][6] 2D array containing the data read back (6 bytes per register)
 @param[out] uint8_t spi_errors[N_OF_ADBMS] Array containing flags indicating which nodes had SPI errors
 */
void ADBMS6830_rdfc_reg(SPI_HandleTypeDef* hspi_ptr,   // Pointer to the SPI handle
					   	TIM_HandleTypeDef* htim_ptr,   // Pointer to a timer handle
						RegGroup_t reg,                // Option: filtered cell voltage register group to read from
						uint8_t data[N_OF_ADBMS][6],   // Input: 2D array containing the data read back
						uint8_t spi_errors[N_OF_ADBMS] // Input: Array containing flags indicating which nodes had SPI errors
					   	)
{
	uint8_t cmd[2];
	cmd[0] = 0x00;
	cmd[1] = 0x12 + reg;

	uint8_t num_tries = 0;
	uint8_t try_again = 0;

	do {
		try_again = !spi_write_read(hspi_ptr, htim_ptr, cmd, data, spi_errors);

		num_tries++;
		if ((num_tries > 2) && try_again) return;
	} while (try_again);
}

/*
 \brief Reads back filtered cell voltages from all register groups from all ICs

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle

 @param[out] uint8_t voltages[N_OF_ADBMS][CELLS_PER_ADBMS] 2D array containing the voltages
 @param[out] uint8_t spi_errors[N_OF_ADBMS] Array containing flags indicating which nodes had SPI errors
 */
void ADBMS6830_rdfc_all(SPI_HandleTypeDef* hspi_ptr,                   // Pointer to the SPI handle
						TIM_HandleTypeDef* htim_ptr,                   // Pointer to a timer handle
						int16_t voltages[N_OF_ADBMS][CELLS_PER_ADBMS], // Input: 2D array containing voltages
						uint8_t spi_errors[N_OF_ADBMS]                 // Input: Array containing flags indicating which nodes had SPI errors
						)
{
	const uint8_t CELL_IN_REG = 3; // 6 bytes per register / 2 bytes per cell = 3 cell voltages per register

	// Freeze all result registers for data coherence
	ADBMS6830_freeze_results(hspi_ptr, htim_ptr);

	for (uint8_t reg = 0; reg < 4; reg++) {
		uint8_t data[N_OF_ADBMS][6];
		ADBMS6830_rdfc_reg(hspi_ptr, htim_ptr, reg, data, spi_errors);

		// Parse voltages and package them into 2D array
		for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
			if (!spi_errors[ic]) {
				for (uint8_t cell = 0; cell < CELL_IN_REG; cell++) {
					int16_t parsed_voltage = (int16_t)((data[ic][(cell * 2) + 1] << 8) + data[ic][cell * 2]);
					voltages[ic][(reg * CELL_IN_REG) + cell] = parsed_voltage;
				}
			}
		}
	}

	ADBMS6830_unfreeze_results(hspi_ptr, htim_ptr);
}

/*
 \brief Reads back auxiliary voltages from one register group for each IC

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle
 @param[in] RegGroup_t reg auxiliary register group to read from

 @param[out] uint8_t data[N_OF_ADBMS][6] 2D array containing the data read back (6 bytes per register)
 @param[out] uint8_t spi_errors[N_OF_ADBMS] Array containing flags indicating which nodes had SPI errors
 */
void ADBMS6830_rdaux_reg(SPI_HandleTypeDef* hspi_ptr,   // Pointer to the SPI handle
						 TIM_HandleTypeDef* htim_ptr,   // Pointer to a timer handle
						 RegGroup_t reg,                // Option: filtered cell voltage register group to read from
						 uint8_t data[N_OF_ADBMS][6],   // Input: 2D array containing the data read back
						 uint8_t spi_errors[N_OF_ADBMS] // Input: Array containing flags indicating which nodes had SPI errors
						 )
{
	uint8_t cmd[2];
	cmd[0] = 0x00;
	cmd[1] = 0x19 + reg;

	uint8_t num_tries = 0;
	uint8_t try_again = 0;

	do {
		try_again = !spi_write_read(hspi_ptr, htim_ptr, cmd, data, spi_errors);

		num_tries++;
		if ((num_tries > 2) && try_again) return;
	} while (try_again);
}

/*
 \brief Reads ADC reading from one aux pin for each IC

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandletypeDef* htim_ptr pointer to a timer handle
 @param[in] AuxPin_t pin the auxiliary pin to read from

 @param[out] int16_t aux[N_OF_ADBMS] Array to read ADC conversions into
 @param[out] uint8_t spi_errors[N_OF_ADBMS] Array containing flags indicating which nodes had SPI errors
 */
void ADBMS6830_rdaux_pin(SPI_HandleTypeDef* hspi_ptr,   // Pointer to the SPI handle
						 TIM_HandleTypeDef* htim_ptr,   // Pointer to a timer handle
						 AuxPin_t pin,                  // Option: Auxiliary pin to read from
						 int16_t aux[N_OF_ADBMS],       // Input: Array to read ADC conversions into
						 uint8_t spi_errors[N_OF_ADBMS] // Input: Array containing flags indicating which nodes had SPI errors
						 )
{
	uint8_t cmd[2];
	cmd[0] = 0x00;

	uint8_t data[N_OF_ADBMS][6]; // 6 bytes per register
	uint8_t lsb_idx;
	uint8_t try_again = 0;
	uint8_t num_tries = 0;

	// Check if pin is a GPIO pin
	if (pin < VREF2) {
		uint8_t reg = pin / 3;
		cmd[1] = 0x19 + reg;

		uint8_t offset = reg * 3;
		lsb_idx = (pin - offset) * 2;
	}
	else if ((pin == VREF2) || (pin == ITEMP) || (pin == RESERVED)) {
		cmd[1] = 0x30;

		if (pin == VREF2) lsb_idx = 0;
		else if (pin == ITEMP) lsb_idx = 2;
		else lsb_idx = 4;
	}
	else if ((pin == VD) || (pin == VA) || (pin == VRES)) {
		cmd[1] = 0x31;

		if (pin == VD) lsb_idx = 0;
		else if (pin == VA) lsb_idx = 2;
		else lsb_idx = 4;
	}

	else if ((pin == VPV) || (pin == VMV)) {
		cmd[1] = 0x1F;

		if (pin == VMV) lsb_idx = 2;
		else lsb_idx = 4;
	}

	do {
		try_again = !spi_write_read(hspi_ptr, htim_ptr, cmd, data, spi_errors);

		num_tries++;
		if ((num_tries > 2) && try_again) break;
	} while (try_again);

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		if (!spi_errors[ic]) {
			aux[ic] = (int16_t)((data[ic][lsb_idx + 1] << 8) + data[ic][lsb_idx]);
		}
	}
}

/*
 \brief Reads back raw ADC readings from GPIOs, which are connected to thermistors

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle

 @param[out] int16_t raw_temp_voltages[N_OF_ADBMS][CELL_TEMPS_PER_ADBMS] 2D array containing the ADC readings
 @param[out] uint8_t spi_errors[N_OF_ADBMS] Array containing flags indicating which nodes had SPI errors
 */
void ADBMS6830_rdaux_raw_temp_voltages(SPI_HandleTypeDef* hspi_ptr,                                 // Pointer to the SPI handle
							           TIM_HandleTypeDef* htim_ptr,                                 // Pointer to a timer handle
							           int16_t raw_temp_voltages[N_OF_ADBMS][CELL_TEMPS_PER_ADBMS], // Input: 2D array containing the ADC readings
							           uint8_t spi_errors[N_OF_ADBMS]                               // Input: Array containing flags indicating which nodes had SPI errors
							           )
{
	const uint8_t TEMP_IN_REG = 3; // 6 register bytes / 2 bytes per voltage = 3 temp voltages per register

	// Freeze all result registers for data coherence
	ADBMS6830_freeze_results(hspi_ptr, htim_ptr);

	for (uint8_t reg = 0; reg < 3; reg++) {
		uint8_t data[N_OF_ADBMS][6];
		ADBMS6830_rdaux_reg(hspi_ptr, htim_ptr, reg, data, spi_errors);

		// Parse voltages and package them into 2D array
		for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
			if (!spi_errors[ic]) {
				for (uint8_t temp = 0; temp < TEMP_IN_REG; temp++) {
					if ((reg == 2) && (temp == 2)) break; // Ignore G9V since there are only 8 cell temps per IC
					int16_t parsed_voltage = (int16_t)((data[ic][(temp * 2) + 1] << 8) + data[ic][temp * 2]);
					raw_temp_voltages[ic][(reg * TEMP_IN_REG) + temp] = parsed_voltage;
				}
			}
		}
	}

	ADBMS6830_unfreeze_results(hspi_ptr, htim_ptr);
}

/*
 \brief Reads back all cell voltage mismatches detected during the redundancy check

 @param[in] SPI_HandleTypeDef* hspi_ptr pointer to the SPI handle
 @param[in] TIM_HandleTypeDef* htim_ptr pointer to a timer handle

 @param[out] uint8_t mismatches[N_OF_ADBMS][CELLS_PER_ADBMS] 2D array containing the mismatch flags for each IC
 @param[out] uint8_t spi_errors[N_OF_ADBMS] Array containing flags indicating which nodes had SPI errors
 */
void ADBMS6830_rdstatc_mismatch(SPI_HandleTypeDef* hspi_ptr,                     // Pointer to the SPI handle
								TIM_HandleTypeDef* htim_ptr,                     // Pointer to a timer handle
								uint8_t mismatches[N_OF_ADBMS][CELLS_PER_ADBMS], // Input: 2D array containing the mismatch flags for each IC
								uint8_t spi_errors[N_OF_ADBMS]                   // Input: Array containing flags indicating which nodes had SPI errors
								)
{
	uint8_t data[N_OF_ADBMS][6]; // 6 bytes per register
	uint8_t cmd[2] = {0x00, 0x32};
	uint8_t try_again = 0;
	uint8_t num_tries = 0;

	do {
		try_again = !spi_write_read(hspi_ptr, htim_ptr, cmd, data, spi_errors);

		num_tries++;
		if ((num_tries > 2) && try_again) break;
	} while (try_again);

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		if (!spi_errors[ic]) {
			for (uint8_t reg_idx = 0; reg_idx < 2; reg_idx++) {
				for (uint8_t bit = 0; bit < 8; bit++) {
					if ((reg_idx == 1) && (bit == 4)) break; // Ignore flags past CS12FLT since we only have 12 cells per IC
					mismatches[ic][(reg_idx * 8) + bit] = (data[ic][reg_idx] >> bit) & 0x01;
				}
			}
		}
	}
}
