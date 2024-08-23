/*
 * matrix_mult.h
 *
 *  Created on: Aug 23, 2024
 *      Author: Abhineet
 */

#ifndef INC_MATRIX_MULT_H_
#define INC_MATRIX_MULT_H_

#include <stdint.h>

// Constant(s)

#define MAX_ELEMENTS 4

// Type(s)

typedef struct MatrixStruct {
  uint8_t rows;
  uint8_t columns;
  float elements[MAX_ELEMENTS];
} Matrix;

// Function declarations

void mat_init(uint8_t rows, uint8_t columns, Matrix *matrix);
void mat_set(uint8_t row, uint8_t column, float element,
             Matrix *matrix);
float mat_get(uint8_t row, uint8_t column, Matrix *);
void mat_fill_zero(Matrix *);
void mat_multiply(Matrix *matrixA, Matrix *matrixB, Matrix *matrixC);
void mat_add(Matrix *A, Matrix *B, Matrix *result);
void mat_scale(Matrix *A, float k, Matrix *result);
void mat_transpose(Matrix *matrix,
                   Matrix *result); // Function to transpose a 2x2 matrix
                                    //                         ^^^

#endif /* INC_MATRIX_MULT_H_ */
