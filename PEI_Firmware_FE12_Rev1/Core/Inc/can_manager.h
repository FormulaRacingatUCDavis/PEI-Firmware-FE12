/*
 * can_manager.h
 *
 *  Created on: Sep 13, 2024
 *      Author: Abhineet
 */

#ifndef INC_CAN_MANAGER_H_
#define INC_CAN_MANAGER_H_

#include <stdint.h>
#include "stm32f7xx_hal.h"

#define CAN_TIMEOUT_TICK_COUNT 5000u

void can_send_PEI_Current(uint8_t shutdown_flags);

void can_send_Charger();

void can_send_BMS_Status();
void can_send_BMS_Data();

void update_can_status();

#endif /* INC_CAN_MANAGER_H_ */
