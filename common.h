#ifndef COMMON_H
#define COMMON_H
typedef float real_t;
#define CHANNEL_NUM1 4
#define CHANNEL_NUM2 8
#define KERNEL_SIZE1 3
#define KERNEL_SIZE2 3
#define TILE_SIZE 128
#define INPUT_SIZE 16000
#define ACTUAL_TILE_SIZE (TILE_SIZE + KERNEL_SIZE1 -1)

#define DELAY_US        62U //

#endif