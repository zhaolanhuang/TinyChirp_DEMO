#ifndef CNN_TIME_H
#define CNN_TIME_H
typedef float real_t;

real_t **allocate_2d_array(int rows, int cols);

real_t*** allocate_3d_array(int first, int second, int third);

void free_2d_array(real_t **array, int rows);

void free_3d_array(real_t ***array, int first, int second);



void conv1d_and_relu_multi_channel(real_t *input, real_t** kernel, real_t ** output, real_t* bias , int channel_number, int inputSize, int kernelSize);



void fill_tile(real_t* tile, real_t* input_data, int i ,int tile_size);


void maxpool1d_channel(real_t** tile, int tile_size, int channel_number, real_t** output_tile);

void multi_channel_aggregation_and_pooling(real_t** input_tile, real_t* output_tile, 
    real_t*** kernel, int input_channels, int output_channels, int tile_size, int kernel_size, int position, int full_input_size);

void mlp(real_t* input, real_t* output, 
    int input_size, int hidden_size, int output_size, real_t** weight1, real_t** weight2,real_t* bias1, real_t* bias2);

void CNN_model_inference(real_t* input_data, real_t* output, 
    real_t** kernel1, int channel_number1, int kernelSize1, 
    real_t *** kernel2, int channel_number2, int kernelSize2, 
    int tile_size, int input_size, 
    real_t** weight1, real_t** weight2, real_t* fcbias1, real_t* fcbias2, real_t* convbias1, real_t* convbias2, 
    real_t* output_tile);

#endif