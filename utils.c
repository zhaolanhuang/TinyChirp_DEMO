#include <stddef.h>
#include <stdio.h>
#include "common.h"

void print_array_output_tile(const float *array, size_t size) {
    printf("Output Tile:\n");
    for (size_t i = 0; i < size; i++) {
        printf("output_tile[%u] = %.6f\n", i, array[i]);
    }
}


void print_buffer_details(const float *buffer, size_t size) {
    printf("Buffer size: %u\n", size);
    printf("First 10 values:\n");
    for (size_t i = 0; i < 10; i++) {
        printf("ring_buffer[%u] = %.6f\n", i, buffer[i]);
    }
}

void print_array(const real_t* array, int size){
    for (int i = 0; i < size; i++) {
        printf("%.6f", array[i]);
        if (i < size - 1) {
            printf(", ");
        }
    }
    printf("\n");
}