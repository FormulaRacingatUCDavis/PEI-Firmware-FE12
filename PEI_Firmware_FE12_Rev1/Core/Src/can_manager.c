/*
 * can_manager.c
 *
 *  Created on: Sep 13, 2024
 *      Author: Abhineet
 */


#include "can_manager.h"
#include "fsm.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

extern uint8_t hv_requested;
extern uint8_t vcu_state;
extern uint16_t vcu_attached;
extern uint32_t ticks_since_vcu_message;

extern int16_t mc_voltage;
extern uint8_t mc_vsm_state;
extern uint8_t mc_discharge_state;
extern uint32_t mc_post_faults;
extern uint32_t mc_run_faults;
extern uint32_t ticks_since_mc_message;

extern uint8_t charger_attached;
extern uint8_t charger_status;
extern uint16_t charger_max_current;
extern uint32_t ticks_since_charger_message;

// TODO: Make external declaration of pack struct

uint32_t vcu_tickstart = 0;
uint32_t mc_tickstart = 0;
uint32_t charger_tickstart = 0;

CAN_RxHeaderTypeDef rx0_header;
CAN_RxHeaderTypeDef rx1_header;

uint32_t PEI_CURRENT_TX_MAILBOX;
uint32_t PEI_CHARGER_TX_MAILBOX;
uint32_t BMS_STATUS_TX_MAILBOX;
uint32_t BMS_DATA_TX_MAILBOX;
uint8_t CAN_RX0_BUFFER[8];
uint8_t CAN_RX1_BUFFER[8];

void set_tx_header_defaults(CAN_TxHeaderTypeDef* tx_header_ptr) {
	tx_header_ptr->IDE = CAN_ID_STD;
	tx_header_ptr->RTR = CAN_RTR_DATA;
}

void CAN_SendMsg(CAN_HandleTypeDef* hcan_ptr, CAN_TxHeaderTypeDef* tx_header_ptr, uint8_t tx_data[], uint32_t* tx_mailbox_ptr) {
	if (HAL_CAN_AddTxMessage(hcan_ptr, tx_header_ptr, tx_data, tx_mailbox_ptr) != HAL_OK) {
		Error_Handler();
	}
}

void can_send_CHARGER(uint8_t charge_start) {
	// Configure TX header
	CAN_TxHeaderTypeDef tx_header;
	tx_header.IDE = CAN_ID_EXT;
	tx_header.RTR = CAN_RTR_DATA;
	tx_header.ExtId = 0x1806E5F4;
	tx_header.DLC = 5;

	uint8_t tx_data[8];
	tx_data[0] = HI8(CHARGER_MAX_VOLTAGE);
	tx_data[1] = LO8(CHARGER_MAX_VOLTAGE);
	tx_data[2] = HI8(charger_max_current);
	tx_data[3] = LO8(charger_max_current);
	tx_data[4] = charge_start;

	CAN_SendMsg(&hcan2, &tx_header, tx_data, &PEI_CHARGER_TX_MAILBOX);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan_ptr) {
	if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &rx0_header, CAN_RX0_BUFFER) != HAL_OK) {
		Error_Handler();
	}
	else {
		if (hcan_ptr == &hcan1) {
			vcu_tickstart = HAL_GetTick();
			ticks_since_vcu_message = 0;
			vcu_attached = 1;

			hv_requested = CAN_RX0_BUFFER[1];
			vcu_state = CAN_RX0_BUFFER[5];

			// TODO: Un-comment after merge with BMS branch
			//bat_pack.status &= ~CHARGEMODE;
		}
		else if (hcan_ptr == &hcan2) {
			charger_tickstart = HAL_GetTick();
			ticks_since_charger_message = 0;
			charger_attached = 1;

			// TODO: Un-comment after merge with BMS branch
			//bat_pack.status |= CHARGEMODE;

			charger_status = CAN_RX1_BUFFER[4];
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
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef* hcan_ptr) {
	if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO1, &rx1_header, CAN_RX1_BUFFER) != HAL_OK) {
		Error_Handler();
	}

	uint32_t id = rx1_header.StdId;
	if (id == 0x0A7) {
		mc_voltage = CAN_RX1_BUFFER[1] << 8;
		mc_voltage += CAN_RX1_BUFFER[0];
	}
	else if (id == 0x0AA) {
		mc_vsm_state = CAN_RX1_BUFFER[0];
		mc_discharge_state = (CAN_RX1_BUFFER[4] >> 5) & 0x07; // bits 5-7
	}
	else if (id == 0x0AB) {
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
