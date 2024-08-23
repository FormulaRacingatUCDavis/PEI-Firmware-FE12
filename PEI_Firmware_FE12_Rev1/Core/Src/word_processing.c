/*
 * word_processing.c
 *
 *  Created on: Aug 23, 2024
 *      Author: Abhineet
 */


#include "word_processing.h"

uint8_t HI8(uint16_t word) {
	return (uint8_t)(word >> 8); // return upper 8 bits of 16-bit word
}

uint8_t LO8(uint16_t word) {
	return (uint8_t)(word & 0x00FF); // return lower 8 bits of 16-bit word
}
