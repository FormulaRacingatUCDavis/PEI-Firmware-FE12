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

uint8_t HI4(uint8_t byte) {
	return byte >> 4;
}

uint8_t LO4(uint8_t byte) {
	return byte & 0x0F;
}
