/*
 * charger.c
 *
 *  Created on: Sep 19, 2024
 *      Author: Abhineet
 */


#include "charger.h"

const Charge_Profile_t ESDC = {
		.charger_max_current = 36, // 120V * 15A = 1800W | 1800W / 504V = 3.6A
		.attenuation_threshold = 95 // 95% SOC
};

const Charge_Profile_t COMP = {
		.charger_max_current = 82, // 208V * 20A = 4160W | 4160W / 504V = 8.2A
		.attenuation_threshold = 80 // 80% SOC
};

volatile uint8_t charger_attached = 0;
volatile uint8_t charger_status = 0;
uint16_t charger_max_current = 0;
uint8_t charge_control = CHARGE_STOP;
Charge_Profile_t selected_profile;
