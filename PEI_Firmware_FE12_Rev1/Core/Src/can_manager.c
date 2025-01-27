/*
 * can_manager.c
 *
 *  Created on: Sep 13, 2024
 *      Author: Abhineet
 */


#include "can_manager.h"
#include "cell_interface.h"
#include "charger.h"
#include "fsm.h"
#include "utils.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

extern int8_t comm_bk_id;

extern volatile uint8_t hv_requested;
extern volatile uint8_t vcu_state;
extern volatile uint16_t vcu_attached;
extern volatile uint32_t ticks_since_vcu_message;

extern volatile int16_t mc_voltage;
extern volatile uint8_t mc_vsm_state;
extern volatile uint8_t mc_discharge_state;
extern volatile uint32_t mc_post_faults;
extern volatile uint32_t mc_run_faults;
extern volatile uint32_t ticks_since_mc_message;

extern volatile uint8_t charger_attached;
extern volatile uint8_t charger_status;
extern uint16_t charger_max_current;
extern uint8_t charge_control;
extern volatile uint32_t ticks_since_charger_message;

extern volatile BAT_PACK_t bat_pack;

volatile uint32_t vcu_tickstart = 0;
volatile uint32_t mc_tickstart = 0;
volatile uint32_t charger_tickstart = 0;

static void set_tx_header_defaults(CAN_TxHeaderTypeDef* const tx_header_ptr) {
	tx_header_ptr->IDE = CAN_ID_STD;
	tx_header_ptr->RTR = CAN_RTR_DATA;
}

void can_send_PEI_Shutdown(uint8_t shutdown_flags) {
	const uint32_t PEI_SHUTDOWN_MSG_ID = 0x387;
	static uint32_t PEI_SHUTDOWN_TX_MAILBOX;

	// Configure TX header
	CAN_TxHeaderTypeDef tx_header;
	set_tx_header_defaults(&tx_header);
	tx_header.StdId = PEI_SHUTDOWN_MSG_ID;
	tx_header.DLC = 1;

	uint8_t tx_data[8];
	tx_data[0] = shutdown_flags;

	HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, &PEI_SHUTDOWN_TX_MAILBOX);
}

void can_send_PEI_Current() {
	const uint32_t PEI_CURRENT_MSG_ID = 0x388;
	static uint32_t PEI_CURRENT_TX_MAILBOX;

	// Configure TX header
	CAN_TxHeaderTypeDef tx_header;
	set_tx_header_defaults(&tx_header);
	tx_header.StdId = PEI_CURRENT_MSG_ID;
	tx_header.DLC = 4;

	uint8_t tx_data[8];
	tx_data[0] = HI8(bat_pack.current_raw);
	tx_data[1] = LO8(bat_pack.current_raw);
	tx_data[2] = HI8(bat_pack.current_ref_raw);
	tx_data[3] = LO8(bat_pack.current_ref_raw);

	HAL_CAN_AddTxMessage(&hcan2, &tx_header, tx_data, &PEI_CURRENT_TX_MAILBOX);
}

void can_send_Charger() {
	const uint32_t PEI_CHARGER_TX_MSG_ID = 0x1806E5F4;
	static uint32_t PEI_CHARGER_TX_MAILBOX;

	// Configure TX header
	CAN_TxHeaderTypeDef tx_header;
	tx_header.IDE = CAN_ID_EXT;
	tx_header.RTR = CAN_RTR_DATA;
	tx_header.ExtId = PEI_CHARGER_TX_MSG_ID;
	tx_header.DLC = 5;

	uint8_t tx_data[8];
	tx_data[0] = HI8(CHARGER_MAX_VOLTAGE);
	tx_data[1] = LO8(CHARGER_MAX_VOLTAGE);
	tx_data[2] = HI8(charger_max_current);
	tx_data[3] = LO8(charger_max_current);
	tx_data[4] = charge_control;

	HAL_CAN_AddTxMessage(&hcan2, &tx_header, tx_data, &PEI_CHARGER_TX_MAILBOX);
}

void can_send_BMS_Status() {
	const uint32_t BMS_STATUS_MSG_ID = 0x380;
	static uint32_t BMS_STATUS_TX_MAILBOX;

	// Configure TX header
	CAN_TxHeaderTypeDef tx_header;
	set_tx_header_defaults(&tx_header);
	tx_header.StdId = BMS_STATUS_MSG_ID;
	tx_header.DLC = 5;

	uint8_t tx_data[8];
	tx_data[0] = bat_pack.status;
	tx_data[1] = HI8(bat_pack.spi_fault_addresses);
	tx_data[2] = LO8(bat_pack.spi_fault_addresses);
	tx_data[3] = get_max_fault_ic_addr();
	tx_data[4] = (uint8_t)comm_bk_id;

	HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, &BMS_STATUS_TX_MAILBOX);
}

void can_send_BMS_High_Level_Data() {
	const uint32_t BMS_HI_LVL_DATA_MSG_ID = 0x381;
	static uint32_t BMS_HI_LVL_DATA_DASH_TX_MAILBOX;
	static uint32_t BMS_HI_LVL_DATA_TELEM_TX_MAILBOX;

	// Configure TX header
	CAN_TxHeaderTypeDef tx_header;
	set_tx_header_defaults(&tx_header);
	tx_header.StdId = BMS_HI_LVL_DATA_MSG_ID;
	tx_header.DLC = 2;

	uint8_t tx_data[8];

	// Send diagnostic BMS data to dash

	tx_data[0] = bat_pack.HI_temp_c;
	tx_data[1] = bat_pack.SOC_percent;

	HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, &BMS_HI_LVL_DATA_DASH_TX_MAILBOX);

	// Send raw BMS data to telem host

	tx_header.DLC = 4;

	tx_data[0] = HI8(bat_pack.HI_temp_raw);
	tx_data[1] = LO8(bat_pack.HI_temp_raw);
	tx_data[2] = HI8(bat_pack.total_voltage_raw);
	tx_data[3] = LO8(bat_pack.total_voltage_raw);

	HAL_CAN_AddTxMessage(&hcan2, &tx_header, tx_data, &BMS_HI_LVL_DATA_TELEM_TX_MAILBOX);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan_ptr) {

	static CAN_RxHeaderTypeDef rx0_header;
	static uint8_t CAN_RX0_BUFFER[8];
	HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &rx0_header, CAN_RX0_BUFFER);

	if (hcan_ptr == &hcan1) {
		vcu_tickstart = HAL_GetTick();
		ticks_since_vcu_message = 0;
		vcu_attached = 1;

		hv_requested = CAN_RX0_BUFFER[1];
		vcu_state = CAN_RX0_BUFFER[5];

		bat_pack.status &= ~CHARGEMODE;
	}
	else if (hcan_ptr == &hcan2) {
		charger_tickstart = HAL_GetTick();
		ticks_since_charger_message = 0;
		charger_attached = 1;

		bat_pack.status |= CHARGEMODE;

		charger_status = CAN_RX0_BUFFER[4];
		if ((charger_status & 0b1111) == 0) { // no faults, charger active
			vcu_state = CHARGING;
		}
		else if ((charger_status & 0b1011) == 0) { // no faults, charger inactive
			vcu_state = LV;
		}
		else {
			// Charger faults, handled in fsm.c
		}
	}
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* hcan_ptr) {

	static CAN_RxHeaderTypeDef rx1_header;
	static uint8_t CAN_RX1_BUFFER[8];
	HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO1, &rx1_header, CAN_RX1_BUFFER);

	const uint32_t MC_VOLTAGE_MSG_ID = 0x0A7;
	const uint32_t MC_STATE_MSG_ID = 0x0AA;
	const uint32_t MC_FAULT_MSG_ID = 0x0AB;

	uint32_t id = rx1_header.StdId;
	if (id == MC_VOLTAGE_MSG_ID) {
		mc_voltage = CAN_RX1_BUFFER[1] << 8;
		mc_voltage += CAN_RX1_BUFFER[0];
	}
	else if (id == MC_STATE_MSG_ID) {
		mc_vsm_state = CAN_RX1_BUFFER[0];
		mc_discharge_state = (CAN_RX1_BUFFER[4] >> 5) & 0x07; // bits 5-7
	}
	else if (id == MC_FAULT_MSG_ID) {
		mc_post_faults = CAN_RX1_BUFFER[3] << 24;
		mc_post_faults += CAN_RX1_BUFFER[2] << 16;
		mc_post_faults += CAN_RX1_BUFFER[1] << 8;
		mc_post_faults += CAN_RX1_BUFFER[0];

		mc_run_faults = CAN_RX1_BUFFER[7] << 24;
		mc_run_faults += CAN_RX1_BUFFER[6] << 16;
		mc_run_faults += CAN_RX1_BUFFER[5] << 8;
		mc_run_faults += CAN_RX1_BUFFER[4];
	}
}

void update_can_status() {
	ticks_since_vcu_message = HAL_GetTick() - vcu_tickstart;
	ticks_since_mc_message = HAL_GetTick() - mc_tickstart;
	ticks_since_charger_message = HAL_GetTick() - charger_tickstart;
}
