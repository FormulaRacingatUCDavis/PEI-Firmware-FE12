/*
 * cell_interface.c
 *
 *  Created on: Sep 1, 2024
 *      Author: Abhineet
 */


#include <math.h>

#include "cell_interface.h"
#include "ADBMS6830.h"
#include "main.h"

static const uint8_t ERROR_VOLTAGE_LIMIT = 4u;
static const uint8_t ERROR_TEMPERATURE_LIMIT = 4u;
static const uint8_t SPI_ERROR_LIMIT = 100u;

int8_t comm_bk_id = -1;

volatile BAT_PACK_t bat_pack;

static void pack_init() {

	// Initialize cell voltages and temps
	for (uint8_t subpack = 0; subpack < N_OF_SUBPACK; subpack++) {
		for (uint8_t cell = 0; cell < CELLS_PER_SUBPACK; cell++) {
			bat_pack.subpacks[subpack].cells[cell].voltage_raw = 0;
			bat_pack.subpacks[subpack].cells[cell].voltage = 0;

			for (uint8_t i = 0; i < 4; i++) {
				bat_pack.subpacks[subpack].cells[cell].bad_counters[i] = 0;
			}
		}

		for (uint8_t temp = 0; temp < CELL_TEMPS_PER_SUBPACK; temp++) {
			bat_pack.subpacks[subpack].cell_temps[temp].temp_raw = 0;
			bat_pack.subpacks[subpack].cell_temps[temp].temp_c = 0;

			for (uint8_t i = 0; i < 2; i++) {
				bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[i] = 0;
			}
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

void cell_interface_init(SPI_HandleTypeDef* const hspi_ptr, TIM_HandleTypeDef* const htim_ptr) {
	ADBMS6830_initialize(hspi_ptr, htim_ptr);
	pack_init();
}

void set_voltage(uint8_t subpack_num, uint8_t cell_num, int16_t voltage_raw) {
	bat_pack.subpacks[subpack_num].cells[cell_num].voltage_raw = voltage_raw;
	bat_pack.subpacks[subpack_num].cells[cell_num].voltage = (voltage_raw * 0.00015) + 1.5;
}

float get_voltage(uint8_t subpack_num, uint8_t cell_num) {
	return bat_pack.subpacks[subpack_num].cells[cell_num].voltage;
}

int16_t get_voltage_raw(uint8_t subpack_num, uint8_t cell_num) {
	return bat_pack.subpacks[subpack_num].cells[cell_num].voltage_raw;
}

void set_cell_temp(uint8_t subpack_num, uint8_t temp_num, int16_t temp_raw) {
	bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_raw = temp_raw;

	float temp_voltage = (temp_raw * 0.00015) + 1.5;
	float temp = (1.0 / ((1.0 / 298.15) + ((1.0 / 3428.0) * log(temp_voltage / (3 - temp_voltage)))))
				  - 273.15;

	bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_c = temp;
}

float get_cell_temp(uint8_t subpack_num, uint8_t temp_num) {
	return bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_c;
}

int16_t get_cell_temp_raw(uint8_t subpack_num, uint8_t temp_num) {
	return bat_pack.subpacks[subpack_num].cell_temps[temp_num].temp_raw;
}

static void update_spi_errors(SPI_HandleTypeDef* const hspi_ptr,
							  TIM_HandleTypeDef* const htim_ptr,
							  uint8_t spi_errors[N_OF_ADBMS]) {
	static uint8_t comm_bk_present = 0;
	static uint8_t comm_bk_enabled = 0;

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		bat_pack.spi_error_counters[ic] += spi_errors[ic];
		if (bat_pack.spi_error_counters[ic] > SPI_ERROR_LIMIT) {
			bat_pack.status |= SPI_FAULT;
			bat_pack.spi_fault_addresses |= 0x01 << ic;
		}

		/*
		 * Communication break is present if there is a string
		 * of PEC mismatches until the end of the chain.
		 *
		 * We don't want to change the communication break status
		 * if a communication break was identified in a previous call.
		 *
		 * Note: the comm_bk_present flag will also be true if there is a PEC
		 * mismatch on the last node. This might not necessarily mean
		 * a communication break, but it might be worth trying to reach
		 * it from the other end of the chain anyways.
		 */
		if (!comm_bk_enabled) {
			comm_bk_present = spi_errors[ic];

			/*
			 * If there is a communication break, then we want to halt transmission
			 * on the last node before the communication break.
			 */
			if ((ic > 0)) {
				if (comm_bk_present && !spi_errors[ic - 1]) comm_bk_id = ic - 1;
				if (!comm_bk_present && spi_errors[ic - 1]) comm_bk_id = -1;
			}
		}
	}

	if (comm_bk_present) {
		if (!comm_bk_enabled) {
			ADBMS6830_enable_comm_bk();
			ADBMS6830_wrcfga(hspi_ptr, htim_ptr);

			comm_bk_enabled = 1;
		}
	}
}

void update_voltages(SPI_HandleTypeDef* const hspi_ptr, TIM_HandleTypeDef* const htim_ptr) {
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

void update_temps(SPI_HandleTypeDef* const hspi_ptr, TIM_HandleTypeDef* const htim_ptr) {
	int16_t cell_temps[N_OF_ADBMS][CELL_TEMPS_PER_ADBMS];
	uint8_t spi_errors[N_OF_ADBMS];

	if (!HAL_GPIO_ReadPin(Wake_6822_GPIO_Port, Wake_6822_Pin)) {
		ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	}
	ADBMS6830_adax(hspi_ptr, htim_ptr); // run ADC conversion

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

/*
 * Checks for two types of cell disconnects:
 *
 * Disconnect on the node PCB itself, detected through a mismatch between
 * S-ADC and C-ADC measurements (RD_FAIL/MISMATCH)
 *
 * Broken connection between cell and the node PCB, detected using the
 * ADBMS6830's open wire detection functionality (FUSE_BLOWN/OPEN_WIRE)
 *
 * See Cell Discharge With Cell Measurements and Cell Diagnostics section of datasheet
 * for high-level implementation explanation
 */
void cell_disconnect_check(SPI_HandleTypeDef* const hspi_ptr, TIM_HandleTypeDef* const htim_ptr) {
	int16_t baseline_voltages[N_OF_ADBMS][CELLS_PER_ADBMS]; // voltages from S-ADCs with open-wire switches closed
	int16_t even_open_wire_voltages[N_OF_ADBMS][CELLS_PER_ADBMS]; // voltages from S-ADCs with even-channel open-wire switches open
	int16_t odd_open_wire_voltages[N_OF_ADBMS][CELLS_PER_ADBMS]; // voltages from S-ADCS with odd-channel open-wire switches open
	uint8_t mismatch_flags[N_OF_ADBMS][CELLS_PER_ADBMS];
	uint8_t spi_errors[N_OF_ADBMS];

	if (!HAL_GPIO_ReadPin(Wake_6822_GPIO_Port, Wake_6822_Pin)) {
		ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	}
	ADBMS6830_set_S_ADC(1, 0, 0, CELL_OW_DISABLED); // DCP = 0, CONT = 1, OW = 0
	ADBMS6830_adsv(hspi_ptr, htim_ptr);
	HAL_Delay(8);

	ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	ADBMS6830_rdstatc_mismatch(hspi_ptr, htim_ptr, mismatch_flags, spi_errors);

	ADBMS6830_rdsv_all(hspi_ptr, htim_ptr, baseline_voltages, spi_errors);

	if (!HAL_GPIO_ReadPin(Wake_6822_GPIO_Port, Wake_6822_Pin)) {
		ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	}
	ADBMS6830_set_S_ADC(0, 0, 0, CELL_OW_CH_EVEN); // DCP = 0, CONT = 0, OW = 1
	ADBMS6830_adsv(hspi_ptr, htim_ptr);

	ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	ADBMS6830_rdsv_all(hspi_ptr, htim_ptr, even_open_wire_voltages, spi_errors);

	if (!HAL_GPIO_ReadPin(Wake_6822_GPIO_Port, Wake_6822_Pin)) {
		ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	}
	ADBMS6830_set_S_ADC(0, 0, 0, CELL_OW_CH_ODD); // DCP = 0, CONT = 0, OW = 2
	ADBMS6830_adsv(hspi_ptr, htim_ptr);

	ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	ADBMS6830_rdsv_all(hspi_ptr, htim_ptr, odd_open_wire_voltages, spi_errors);

	// Check each cell
	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		// Update bat_pack if no SPI error
		if (!spi_errors[ic]) {
			for (uint8_t cell = 0; cell < CELLS_PER_ADBMS; cell++) {
				float open_wire_voltage = 0;
				float baseline_voltage = (baseline_voltages[ic][cell] * 0.00015) + 1.5;
				uint8_t subpack = ic / IC_PER_SUBPACK;
				uint8_t subpack_cell_num = ((ic % IC_PER_SUBPACK) * CELLS_PER_ADBMS) + cell;

				if (((cell + 1) % 2) == 0) {
					open_wire_voltage = (even_open_wire_voltages[ic][cell] * 0.00015) + 1.5;
				}
				else {
					open_wire_voltage = (odd_open_wire_voltages[ic][cell] * 0.00015) + 1.5;
				}

				bat_pack.subpacks[subpack].cells[subpack_cell_num].bad_counters[RD_FAIL] += mismatch_flags[ic][cell];
				if (bat_pack.subpacks[subpack].cells[subpack_cell_num].bad_counters[RD_FAIL]
					> ERROR_VOLTAGE_LIMIT) {
					bat_pack.status |= MISMATCH;
				}

				float percent_difference = (open_wire_voltage - baseline_voltage) / baseline_voltage;
				if (percent_difference < 0) percent_difference = -percent_difference;
				if (percent_difference > 0.1) {
					bat_pack.subpacks[subpack].cells[subpack_cell_num].bad_counters[FUSE_BLOWN]++;
					if (bat_pack.subpacks[subpack].cells[subpack_cell_num].bad_counters[FUSE_BLOWN]
						> ERROR_VOLTAGE_LIMIT) {
						bat_pack.status |= OPEN_WIRE;
					}
				}
			}
		}
	}
}

void process_voltages() {
	const float OVER_VOLTAGE = 4.2f; // 4.2 V
	const float UNDER_VOLTAGE = 2.5f; // 2.5 V

	float max_voltage = -4; // Smallest voltage that can be read by ADC is -3.4152 V
	int16_t max_voltage_raw = 0;
	float min_voltage = 7; // Largest voltage that can be read by ADC is 6.41505 V
	int16_t min_voltage_raw = 0;
	float total_voltage = 0;

	// Check each cell
	for (uint8_t subpack = 0; subpack < N_OF_SUBPACK; subpack++) {
		for (uint8_t cell = 0; cell < CELLS_PER_SUBPACK; cell++) {
			float voltage = get_voltage(subpack, cell);

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
				if (bat_pack.subpacks[subpack].cells[cell].bad_counters[OVERVOLT]
					> ERROR_VOLTAGE_LIMIT) {
					bat_pack.status |= CELL_VOLT_OVER;
				}
			}
			else if (voltage < UNDER_VOLTAGE) {
				bat_pack.subpacks[subpack].cells[cell].bad_counters[UNDERVOLT]++;
				if (bat_pack.subpacks[subpack].cells[cell].bad_counters[UNDERVOLT]
					> ERROR_VOLTAGE_LIMIT) {
					bat_pack.status |= CELL_VOLT_UNDER;
				}
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
	const float OVER_TEMP = 60.0f; // 60 C
	const float UNDER_TEMP = 0.0f; // 0 C

	const float TEMP_IGNORE_LIMIT = 500.0f; // Ignore temps over this value, probably a bad thermistor

	float avg_temp_c = 0;
	float max_temp_c = -TEMP_IGNORE_LIMIT;
	int16_t max_temp_raw = 0;
	float min_temp_c = TEMP_IGNORE_LIMIT;
	uint8_t num_bad_temp = 0;

	// Check each cell temp
	for (uint8_t subpack = 0; subpack < N_OF_SUBPACK; subpack++) {
		for (uint8_t temp = 0; temp < CELL_TEMPS_PER_SUBPACK; temp++) {
			float temp_c = get_cell_temp(subpack, temp);

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
					if (bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[OVERTEMP]
					    > ERROR_TEMPERATURE_LIMIT) {
						bat_pack.status |= PACK_TEMP_OVER;
					}
				}
				else if (temp_c < UNDER_TEMP) {
					bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[UNDERTEMP]++;
					if (bat_pack.subpacks[subpack].cell_temps[temp].bad_counters[UNDERTEMP]
					    > ERROR_TEMPERATURE_LIMIT) {
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
void disable_cell_balancing(SPI_HandleTypeDef* const hspi_ptr, TIM_HandleTypeDef* const htim_ptr) {
	ADBMS6830_reset_discharge();

	if (!HAL_GPIO_ReadPin(Wake_6822_GPIO_Port, Wake_6822_Pin)) {
		ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	}
	ADBMS6830_wrpwma(hspi_ptr, htim_ptr);
}

// Update ICs to discharge cells that are too far above the lowest voltage in the pack
void balance_cells(SPI_HandleTypeDef* const hspi_ptr, TIM_HandleTypeDef* const htim_ptr) {
	const float BALANCE_THRESHOLD = 0.02f; // Balance to within 20 mV

	float target_voltage = bat_pack.LO_voltage;

	ADBMS6830_reset_discharge(); // clear previous discharges

	for (uint8_t ic = 0; ic < N_OF_ADBMS; ic++) {
		for (uint8_t cell = 0; cell < CELLS_PER_ADBMS; cell++) {
			float voltage = get_voltage(ic / IC_PER_SUBPACK,
										((ic % IC_PER_SUBPACK) * CELLS_PER_ADBMS) + cell
										);
			float difference = voltage - target_voltage;
			if (difference > BALANCE_THRESHOLD) ADBMS6830_set_discharge(ic, cell); // discharge cell
		}
	}

	if (!HAL_GPIO_ReadPin(Wake_6822_GPIO_Port, Wake_6822_Pin)) {
		ADBMS6830_wakeup(hspi_ptr, htim_ptr);
	}
	ADBMS6830_wrpwma(hspi_ptr, htim_ptr);
}

uint8_t get_max_fault_ic_addr() {
	uint8_t max_faults = bat_pack.spi_error_counters[0];
	uint8_t max_fault_addr = 0;

	for (uint8_t ic = 1; ic < N_OF_ADBMS; ic++) {
		uint8_t num_faults = bat_pack.spi_error_counters[ic];

		if (num_faults > max_faults) {
			max_faults = num_faults;
			max_fault_addr = ic;
		}
	}

	return max_fault_addr;
}

BMS_MODE_t bat_health_check() {
	if ((bat_pack.status & PACK_TEMP_OVER) ||
		(bat_pack.status & PACK_TEMP_UNDER) ||
		(bat_pack.status & OPEN_WIRE) ||
		(bat_pack.status & MISMATCH) ||
		(bat_pack.status & CELL_VOLT_OVER) ||
		(bat_pack.status & CELL_VOLT_UNDER) ||
		(bat_pack.status & SPI_FAULT)
		) {
		return BMS_FAULT;
	}

	return BMS_NORMAL;
}
