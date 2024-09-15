/*
 * SOC.h
 *
 *  Created on: Sep 15, 2024
 *      Author: Abhineet
 */

#ifndef INC_SOC_H_
#define INC_SOC_H_

#include <stdint.h>

// TODO: Update for new pack
// SOC Constants
#define dt 0.1 // Sampling Period
#define R0 0.0145
#define Rc 0.009
#define Cbat (4.1 * 60 * 60)
#define Cc 1666

uint8_t EKF();
void init_SOC_vars();
void update_SOC_input();

#endif /* INC_SOC_H_ */
