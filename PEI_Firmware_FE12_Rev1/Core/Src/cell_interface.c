/*
 * cell_interface.c
 *
 *  Created on: Sep 1, 2024
 *      Author: Abhineet
 */


#include <math.h>

#include "cell_interface.h"
#include "ADBMS6830.h"

BAT_PACK_t bat_pack;

void pack_init() {

	// Initialize cell voltages and temps
	for (uint8_t subpack = 0; subpack < N_OF_SUBPACK; subpack++) {
		for (uint8_t cell = 0; cell < CELLS_PER_SUBPACK; cell++) {
			bat_pack.subpacks[subpack].cells[cell].voltage_raw = 0;
			bat_pack.subpacks[subpack].cells[cell].voltage = 0;
			bat_pack.subpacks[subpack].cells[cell].bad_counter = 0;
		}

		for (uint8_t temp = 0; cell < CELL_TEMPS_PER_SUBPACK; temp++) {
			bat_pack.subpacks[subpack].cell_temps[temp].temp_raw = 0;
			bat_pack.subpacks[subpack].cell_temps[temp].temp_c = 0;
			bat_pack.subpacks[subpack].cell_temps[temp].bad_counter = 0;
		}
	}

	// Initialize pack values
	bat_pack.total_voltage_raw = 0;
	bat_pack.total_voltage = 0;
	bat_pack.LO_voltage = 0;
	bat_pack.current_raw = 0;
	bat_pack.current = 0;
	bat_pack.HI_temp_raw = 0;
	bat_pack.HI_temp_c = 0;
	bat_pack.SOC_percent = 0;

	// Initialize error trackers
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		bat_pack.spi_error_counters[ic] = 0;
	}
	bat_pack.spi_fault_addresses = 0;
	bat_pack.status = NO_ERROR;
}

void cell_interface_init(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	ADBMS6830_initialize(hspi_ptr, htim_ptr);
	pack_init();
}

void set_voltage(uint8_t subpack_num, uint8_t cell_num, int16_t voltage_raw) {
	bat_pack.subpacks[subpack_num].cells[cell_num].voltage_raw = voltage_raw;
	bat_pack.subpacks[subpack_num].cells[cell_num].voltage = (voltage_raw * 0.00015) + 1.5;
}

double get_voltage(uint8_t subpack_num, uint8_t cell_num) {
	return bat_pack.subpacks[subpack_num].cells[cell_num].voltage;
}

int16_t get_voltage_raw(uint8_t subpack_num, uint8_t cell_num) {
	return bat_pack.subpacks[subpack_num].cells[cell_num].voltage_raw;
}

void set_cell_temp(uint8_t subpack_num, uint8_t temp_num, int16_t temp_raw) {
	bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_raw = temp_raw;

	double temp_voltage = (temp_raw * 0.00015) + 1.5;
	double temp = (1.0 / ((1.0 / 298.15) + ((1.0 / 3428.0) * log(temp_voltage / (3 - temp_voltage))))) - 273.15;

	bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_c = temp;
}

double get_cell_temp(uint8_t subpack_num, uint8_t temp_num) {
	return bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_c;
}

int16_t get_cell_temp_raw(uint8_t subpack_num, uint8_t temp_num) {
	return bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_raw;
}

void update_spi_errors(uint8_t spi_errors[N_OF_ADBMS]) {
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		bat_pack.spi_error_counters[ic] += spi_errors[ic];
		if (bat_pack.spi_error_counters[ic] > SPI_ERROR_LIMIT) {
			bat_pack.status |= SPI_FAULT;
			bat_pack.spi_fault_addresses |= 1u << ic;
		}
	}
}

void get_voltages(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	int16_t cell_voltages[N_OF_ADBMS][CELLS_PER_ADBMS];
	uint8_t spi_errors[N_OF_ADBMS];

	ADBMS6830_rdfc_all(hspi_ptr, htim_ptr, cell_voltages, spi_errors);
	update_spi_errors(spi_errors);

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		// Move voltages into bat_pack if no SPI error
		if (!spi_errors[ic]) {
			for (uint8_t cell = 0; cell < CELLS_PER_ADBMS; cell++) {
				set_voltage(ic / IC_PER_SUBPACK,
							((ic % IC_PER_SUBPACK) * CELLS_PER_ADBMS) + cell,
							cell_voltages[ic][cell]
							);
			}
		}
	}
}

void get_temps(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	int16_t cell_temps[N_OF_ADBMS][CELL_TEMPS_PER_ADBMS];
	uint8_t spi_errors[N_OF_ADBMS];

	ADBMS6830_rdaux_raw_temp_voltages(hspi_ptr, htim_ptr, cell_temps, spi_errors);
	update_spi_errors(spi_errors);

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		// Move temps into bat_pack if no SPI error
		if (!spi_errors[ic]) {
			for (uint8_t temp = 0; temp < CELL_TEMPS_PER_ADBMS; temp++) {
				set_cell_temp(ic / IC_PER_SUBPACK,
							  ((ic % IC_PER_SUBPACK) * CELL_TEMPS_PER_ADBMS) + temp,
							  cell_temps[ic][temp]
							  );
			}
		}
	}
}
