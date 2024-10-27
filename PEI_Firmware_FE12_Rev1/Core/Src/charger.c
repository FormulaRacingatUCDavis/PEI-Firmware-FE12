/*
 * charger.c
 *
 *  Created on: Sep 19, 2024
 *      Author: Abhineet
 */


#include "charger.h"

volatile uint8_t charger_attached = 0;
volatile uint8_t charger_status = 0;
uint16_t charger_max_current = 0;
uint8_t charge_control = CHARGE_STOP;
