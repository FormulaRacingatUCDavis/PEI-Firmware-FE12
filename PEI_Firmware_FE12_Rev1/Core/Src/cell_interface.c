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

void update_voltages(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
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

void update_temps(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
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

void process_voltages() {
	double max_voltage = -4; // Smallest voltage that can be read by ADC is -3.4152 V
	int16_t max_voltage_raw;
	double min_voltage = 7; // Largest voltage that can be read by ADC is 6.41505 V
	int16_t min_voltage_raw;
	double total_voltage = 0;

	// Check each cell
	for (uint8_t subpack = 0; subpack < N_OF_SUBPACK; subpack++) {
		for (uint8_t cell = 0; cell < CELLS_PER_SUBPACK; cell++) {
			double voltage = get_voltage(subpack, cell);

			if (max_voltage < voltage) {
				max_voltage = voltage;
				max_voltage_raw = get_voltage_raw(subpack, cell);
			}
			if (min_voltage > voltage) {
				min_voltage = voltage;
				min_voltage_raw = get_voltage_raw(subpack, cell);
			}
			total_voltage += voltage;

			if (voltage > OVER_VOLTAGE) {
				bat_pack.subpacks[subpack].cells[cell].bad_counters[OVERVOLT]++;
				if (bat_pack.subpacks[subpack].cells[cell].bad_counters[OVERVOLT] > ERROR_VOLTAGE_LIMIT) {
					bat_pack.status |= CELL_VOLT_OVER;
				}
			}
			else if (voltage < UNDER_VOLTAGE) {
				bat_pack.subpacks[subpack].cells[cell].bad_counters[UNDERVOLT]++;
				if (bat_pack.subpacks[subpack].cells[cell].bad_counters[UNDERVOLT] > ERROR_VOLTAGE_LIMIT) {
					bat_pack.status |= CELL_VOLT_UNDER;
				}
			}
			else {
				bat_pack.subpacks[subpack].cells[cell].bad_counters[OVERVOLT] = 0;
				bat_pack.subpacks[subpack].cells[cell].bad_counters[UNDERVOLT] = 0;
			}
		}
	}

	bat_pack.HI_voltage_raw = max_voltage_raw;
	bat_pack.LO_voltage_raw = min_voltage_raw;
	bat_pack.LO_voltage = min_voltage;
	bat_pack.total_voltage = total_voltage;
	bat_pack.total_voltage_raw = (total_voltage - (1.5 * N_OF_CELL)) / (0.00015 * N_OF_CELL);
}

void process_temps() {
	double avg_temp_c = 0;
	double max_temp_c = -TEMP_IGNORE_LIMIT;
	int16_t max_temp_raw;
	double min_temp_c = TEMP_IGNORE_LIMIT;
	uint8_t num_bad_temp = 0;

	// Check each cell temp
	for (uint8_t subpack = 0; subpack < N_OF_SUBPACK; subpack++) {
		for (uint8_t temp = 0; temp < CELL_TEMPS_PER_SUBPACK; temp++) {
			double temp_c = get_cell_temp(subpack, temp);

			if (temp_c < TEMP_IGNORE_LIMIT) {
				if (max_temp_c < temp_c) {
					max_temp_c = temp_c;
					max_temp_raw = get_cell_temp_raw(subpack, temp);
				}
				if (temp_c != 0) {
					if (min_temp_c > temp_c) min_temp_c = temp_c;
					avg_temp_c += temp_c;
				}
				else {
					num_bad_temp++;
				}

				if (temp_c > OVER_TEMP) {
					bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[OVERTEMP]++;
					if (bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[OVERTEMP] > ERROR_TEMPERATURE_LIMIT) {
						bat_pack.status |= PACK_TEMP_OVER;
					}
				}
				else if (temp_c < UNDER_TEMP) {
					bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[UNDERTEMP]++;
					if (bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[UNDERTEMP] > ERROR_TEMPERATURE_LIMIT) {
						bat_pack.status |= PACK_TEMP_UNDER;
					}
				}
				else {
					bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[OVERTEMP] = 0;
					bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[UNDERTEMP] = 0;
				}
			}
			else {
				num_bad_temp++;
			}
		}
	}
	avg_temp_c /= N_OF_TEMP_CELL - num_bad_temp;

	bat_pack.AVG_temp_c = avg_temp_c;
	bat_pack.HI_temp_raw = max_temp_raw;
	bat_pack.HI_temp_c = max_temp_c;
	bat_pack.LO_temp_c = min_temp_c;
}

// Make sure no cells will discharge
void disable_cell_balancing(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	ADBMS6830_reset_discharge();
	ADBMS6830_wrpwma(hspi_ptr, htim_ptr);
}

// Update ICs to discharge cells that are too far above the lowest voltage in the pack
void balance_cells(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr) {
	double target_voltage = bat_pack.LO_voltage;

	ADBMS6830_reset_discharge(); // clear previous discharges

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		for (uint8_t cell = 0; cell < CELLS_PER_ADBMS; cell++) {
			double voltage = get_voltage(ic / IC_PER_SUBPACK,
										 ((ic % IC_PER_SUBPACK) * CELLS_PER_ADBMS) + cell
										 );
			double difference = voltage - target_voltage;
			if (difference > BALANCE_THRESHOLD) ADBMS6830_set_discharge(ic, cell); // discharge cell
		}
	}

	ADBMS6830_wrpwma(hspi_ptr, htim_ptr);
}
