#include <stdio.h>

#include "ztimer.h"

#include "common.h"
#include "cnn_time.h"
#include "cnn_time_parameter.h"
#include "utils.h"
#include "mic.h"


static real_t ring_buffer[16000];

int worker_cnn_time(void) {

    int channel_number1 = CHANNEL_NUM1;
    int kernelSize1 = KERNEL_SIZE1;
    int channel_number2 = CHANNEL_NUM2;
    int kernelSize2 = KERNEL_SIZE2;
    int tile_size = TILE_SIZE;
    int input_size = INPUT_SIZE;
    real_t output[2];
    int outputSize = (48000 -kernelSize1 +1)/2 - kernelSize2 + 1;

    unsigned int i = 0;
    unsigned int j = 0;

    real_t output_tile[channel_number2];
    for (int k = 0; k< channel_number2;k++){
        output_tile[k] = 0.0f;
    }

    while (1)
    {
        // If 16000 samples are written to buffer we give buffer to model as for partial convolution
        if (i == sizeof(ring_buffer) / sizeof(float)) {
            printf("Buffer full! Triggering inference...\n");
            print_buffer_details(ring_buffer, sizeof(ring_buffer) / sizeof(float));
            i = 0;

            // Model works with 3 sec audio but buffer has only 1 sec signal. So we use partial convolution - we aggregate output_tile
            CNN_model_inference((real_t*)ring_buffer, output, conv1weight, channel_number1, kernelSize1, conv2weight, channel_number2, kernelSize2, tile_size, input_size, fc1weight, fc2weight,fc1bias,fc2bias, conv1bias, conv2bias, output_tile);
            print_array_output_tile(output_tile, channel_number2);

            j++;
            printf("Debug: j = %d\n", j);
        }

        // If output_tile has data from 3 seconds (3 buffers) do a prediction
        if (j == 3) {
            printf("outputSize: %d \n", outputSize);
            for(int l = 0; l< channel_number2;l++){
                output_tile[l] /= outputSize;;
                output_tile[l] += conv2bias[l];
            }

            mlp(output_tile, output, channel_number2, 64, 2, fc1weight, fc2weight,fc1bias,fc2bias);

            printf("Inference output: \n");
            print_array(output,2);

            j = 0;
            for (int k = 0; k< channel_number2;k++){
                output_tile[k] = 0.0f;
            }

        }
    }
    

    return 0;
}

int worker_audio_sampling(void) {
    


    return 0;
}

int main(void)
{
    int sample = 0;

    // puts("This test will sample all available ADC lines once every 100ms with\n"
    //      "a 10-bit resolution and print the sampled results to STDIO\n\n");

    if (mic_init() < 0)
    {
        printf("Init of MIC Failed! \n");
        return -1;
    } else {
        printf("Init of MIC Success! \n");

    }
    

    unsigned int i = 0;
    while (1) {

            sample = get_sample_value();
            // float f_val = (sample * ADC_REF_V) / (1 << ADC_BITS);
            printf("ADC_LINE(%u): %i\n", 3, sample);
                // printf("adc volt: %f \n", f_val);

            
            // When the ring buffer is not full, add one values to the ring buffer
            if(i < sizeof(ring_buffer) / sizeof(float)) {
                //Normalize the sample
                ring_buffer[i] = sample / 1024.0f;
            } else { // if ring buffer is full, give it to the model
                //Inference part
                //fetch the output blablabla

                //reset the ring buffer
                i = 0;
            }
            i++;

        ztimer_sleep(ZTIMER_USEC, DELAY_US);
    }

    return 0;
}
