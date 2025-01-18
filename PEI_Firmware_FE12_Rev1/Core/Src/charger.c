/*
 * charger.c
 *
 *  Created on: Sep 19, 2024
 *      Author: Abhineet
 */


#include "charger.h"
#include "cell_interface.h"

const Charge_Profile_t ESDC = {
		.charger_max_current = 36, // 120V * 15A = 1800W | 1800W / 504V = 3.6A
		.attenuation_threshold = 95 // 95% SOC
};

const Charge_Profile_t COMP = {
		.charger_max_current = 82, // 208V * 20A = 4160W | 4160W / 504V = 8.2A
		.attenuation_threshold = 80 // 80% SOC
};

extern volatile BAT_PACK_t bat_pack;

volatile uint8_t charger_attached = 0;
volatile uint8_t charger_status = 0;
uint16_t charger_max_current = 0;
uint8_t charge_control = CHARGE_STOP;
uint8_t charge_profile_received = 0;
Charge_Profile_t selected_profile;

void update_max_charge_current() {
	uint8_t soc_diff = bat_pack.SOC_percent - selected_profile.attenuation_threshold;

	if (soc_diff > 0) {
		charger_max_current = (uint16_t)(selected_profile.charger_max_current / soc_diff);
	}
	else {
		charger_max_current = selected_profile.charger_max_current;
	}
}
