/*
 * cell_interface.h
 *
 *  Created on: Aug 31, 2024
 *      Author: Abhineet
 */

#ifndef INC_CELL_INTERFACE_H_
#define INC_CELL_INTERFACE_H_

#include <stdint.h>
#include "stm32f7xx_hal.h"

#include "data.h"

#define ERROR_VOLTAGE_LIMIT 4u
#define ERROR_TEMPERATURE_LIMIT 4u
#define SPI_ERROR_LIMIT 100u

#define OVER_VOLTAGE 4.2 // 4.2 V
#define UNDER_VOLTAGE 2.5 // 2.5 V

#define OVER_TEMP 60 // 60 C
#define UNDER_TEMP 0 // 0 C

#define BALANCE_THRESHOLD 0.02 // Balance to within 20 mV

#define TEMP_IGNORE_LIMIT 500 // Ignore temps over this value, probably a bad thermistor

// BMS Status Macros
#define NO_ERROR 0x00
#define CHARGEMODE 0x01
#define PACK_TEMP_OVER 0x02
#define PACK_TEMP_UNDER 0x04
#define CELL_VOLT_OVER 0x08
#define CELL_VOLT_UNDER 0x10
#define FUSE_BLOWN 0x20
#define MISMATCH 0x40
#define SPI_FAULT 0x80

typedef enum {
	BMS_FAULT, BMS_NORMAL
} BMS_MODE_t;

typedef enum {
	OVERVOLT, UNDERVOLT, OPEN_WIRE, RD_FAIL
} Cell_ErrorType_t;

typedef enum {
	OVERTEMP, UNDERTEMP
} Temp_ErrorType_t;

typedef struct {
	int16_t voltage_raw;
	double voltage;
	uint8_t bad_counter;
	Cell_ErrorType_t bad_type;
} BAT_CELL_t;

typedef struct {
	int16_t temp_raw;
	double temp_c;
	uint8_t bad_counter;
	Temp_ErrorType_t bad_type;
} BAT_TEMP_t;

typedef struct {
	BAT_CELL_t cells[CELLS_PER_SUBPACK];
	BAT_TEMP_t cell_temps[CELL_TEMPS_PER_SUBPACK];
} BAT_SUBPACK_t;

typedef struct {
	BAT_SUBPACK_t subpacks[N_OF_SUBPACK];
	int16_t total_voltage_raw;
	double total_voltage;
	double LO_voltage;
	uint16_t current_raw;
	double current;
	int16_t HI_temp_raw;
	uint8_t HI_temp_c;
	uint8_t SOC_percent;
	uint8_t status;
	uint16_t spi_fault_addresses; // Stores the SPI fault flags for all ICs in a single bit string
	uint8_t spi_error_counters[N_OF_ADBMS]; // Stores the number of SPI communication errors for each IC
} BAT_PACK_t;

void cell_interface_init(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);

void set_voltage(uint8_t subpack_num, uint8_t cell_num, int16_t voltage_raw);
double get_voltage(uint8_t subpack_num, uint8_t cell_num);
int16_t get_voltage_raw(uint8_t subpack_num, uint8_t cell_num);

void set_cell_temp(uint8_t subpack_num, uint8_t temp_num, int16_t temp_raw);
double get_cell_temp(uint8_t subpack_num, uint8_t temp_num);
int16_t get_cell_temp_raw(uint8_t subpack_num, uint8_t temp_num);

void get_temps(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void get_voltages(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void cell_redundancy_check(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void cell_open_wire_check(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);

void check_voltages();
void check_temps();

void disable_cell_balancing(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);
void balance_cells(SPI_HandleTypeDef* hspi_ptr, TIM_HandleTypeDef* htim_ptr);

uint8_t get_max_fault_ic_addr();
BMS_MODE_t bat_health_check();

#endif /* INC_CELL_INTERFACE_H_ */
