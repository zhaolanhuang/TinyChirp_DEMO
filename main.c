#include <stdio.h>

#include "ztimer.h"
#include "thread.h"
#include "mutex.h"

#include "common.h"
#include "cnn_time.h"
#include "cnn_time_parameter.h"
#include "utils.h"
#include "mic.h"
#include "adc_dma.h"

#define SUB_THREAD_NUM 2
#define RING_BUFFER_NUM 3
static char stacks[SUB_THREAD_NUM][THREAD_STACKSIZE_DEFAULT];

static real_t ring_buffer[RING_BUFFER_NUM][RING_BUFFER_SIZE];

static int16_t ring_dma_buffer[RING_BUFFER_NUM][RING_BUFFER_SIZE];

static void* ring_buffer_ptrs[RING_BUFFER_NUM];

static real_t output[2];

static kernel_pid_t pid_cnn_time;
static kernel_pid_t pid_audio_sampling;
static kernel_pid_t pid_main;

static uint32_t time_to_result = 0;
static uint32_t time_last_rslt = 0;

static void *worker_cnn_time(void* arg) {

    int channel_number1 = CHANNEL_NUM1;
    int kernelSize1 = KERNEL_SIZE1;
    int channel_number2 = CHANNEL_NUM2;
    int kernelSize2 = KERNEL_SIZE2;
    int tile_size = TILE_SIZE;
    int input_size = RING_BUFFER_SIZE;
    
    int outputSize = (48000 -kernelSize1 +1)/2 - kernelSize2 + 1;
    printf("OutputSize: %d\n", outputSize);
    
    (void) arg;
    // unsigned int i = 0;
    unsigned int j = 0;

    real_t output_tile[channel_number2];
    memset(output_tile, 0, sizeof(output_tile));
    // for (int k = 0; k< channel_number2;k++){
    //     output_tile[k] = 0.0f;
    // }

    unsigned int ckpt_idx = 0;
    real_t output_tile_ckpt[CHECKPOINT_NUM][channel_number2];
    memset(output_tile_ckpt, 0, sizeof(output_tile_ckpt));
    // for (int i = 0; i < CHECKPOINT_NUM; i++) {
    //     for (int k = 0; k< channel_number2;k++){
    //         output_tile_ckpt[i][k] = 0.0f;
    //     }
    // }


    while (1)
    {

        msg_t m;
        msg_receive(&m);

        if (m.type != RING_BUFFER_FULL)
            continue;
        size_t r_idx = m.content.value;

        // printf("Inference Begin: j = %d, r_idx= %d\n", j, r_idx);
#ifndef SIMULATE_ADC
        for (int i = 0; i < RING_BUFFER_SIZE; i++) {
            ring_buffer[r_idx][i] = (ring_dma_buffer[r_idx][i] / 4096.0 - 0.39) * 2.5;
        }
#endif
            // Model works with 3 sec audio but buffer has only 1 sec signal. So we use partial convolution - we aggregate output_tile
        CNN_model_inference((real_t*)ring_buffer[r_idx], output, conv1weight, channel_number1, kernelSize1, conv2weight, channel_number2, kernelSize2, 
                            tile_size, input_size, fc1weight, fc2weight,fc1bias,fc2bias, conv1bias, conv2bias, output_tile_ckpt[ckpt_idx]);
            // print_array_output_tile(output_tile, channel_number2);

        j++;
        // printf("Inference End of Conv: j = %d\n", j);


        // If output_tile has data from 3 seconds (3 buffers) do a prediction
        if (j == SLIDING_WINDOW_STEP / RING_BUFFER_SIZE) {
            // printf("outputSize: %d \n", outputSize);
            
            for (int i = 0; i < CHECKPOINT_NUM; i++) {
                for (int k = 0; k< channel_number2;k++){
                    output_tile[k] += output_tile_ckpt[i][k];
                }
            }

            for(int l = 0; l< channel_number2;l++){
                output_tile[l] /= outputSize;
                output_tile[l] += conv2bias[l];
            }
            // printf("MLP Begin\n");
            mlp(output_tile, output, channel_number2, 64, 2, fc1weight, fc2weight,fc1bias,fc2bias);

            time_to_result = ztimer_now(ZTIMER_USEC) - time_last_rslt;
            time_last_rslt = ztimer_now(ZTIMER_USEC);
            // printf("MLP End\n");
            msg_t m_rslt;
            m_rslt.type = INFERENCE_RESULT;
            
            msg_send(&m_rslt, pid_main);

            j = 0;

            ckpt_idx++;
            if (ckpt_idx >= CHECKPOINT_NUM ) {
                ckpt_idx = 0;
            }

            for (int k = 0; k< channel_number2;k++){
               output_tile_ckpt[ckpt_idx][k] = 0.0f;
            }

            for (int k = 0; k< channel_number2;k++){
                output_tile[k] = 0.0f;
            }

        }
    }
    
    return NULL;
}

static void *worker_audio_sampling(void* arg) {
    (void) arg;
    unsigned int i = 0;
    unsigned int ring_buffer_idx = 0;
    uint32_t last_wakeup = ztimer_now(ZTIMER_USEC);
    while (1) {
            msg_t m;
                        
            // int sample = get_sample_value();
            // float f_val = (sample * ADC_REF_V) / (1 << ADC_BITS);
            // printf("adc volt: %f \n", f_val);

            //Normalize the sample
            ring_buffer[ring_buffer_idx][i] = get_amplitude();
            // ring_buffer[ring_buffer_idx][i] = 0;
            i++;
            // When the ring buffer is not full, add one values to the ring buffer

            if(i < sizeof(ring_buffer[0]) / (sizeof(float))) {
                ;
            } else { // if ring buffer is full, give it to the model
                m.type = RING_BUFFER_FULL;
                m.content.value = ring_buffer_idx;
                msg_send(&m, pid_cnn_time);
                // printf("ring_buffer_idx = %d \n", ring_buffer_idx);
                // printf("i = %d \n", i);

                ring_buffer_idx++;
                if (ring_buffer_idx > RING_BUFFER_NUM - 1)
                    ring_buffer_idx = 0;

                //reset the ring buffer
                i = 0;
            }
            

        // ztimer_sleep(ZTIMER_USEC, DELAY_US);
        ztimer_periodic_wakeup(ZTIMER_USEC, &last_wakeup, DELAY_US);
    }


    return NULL;
}

int main(void)
{
    // puts("This test will sample all available ADC lines once every 100ms with\n"
    //      "a 10-bit resolution and print the sampled results to STDIO\n\n");
    memset(ring_buffer, 0, sizeof(ring_buffer));
    memset(output, 0, sizeof(output));
#ifndef SIMULATE_ADC
    for(int i = 0; i < RING_BUFFER_NUM; i++) {
        ring_buffer_ptrs[i] = ring_dma_buffer[i];
    }

    set_dma_buffer(&ring_buffer_ptrs, RING_BUFFER_NUM, RING_BUFFER_SIZE);

#endif

    if (mic_init() < 0)
    {
        printf("Init of MIC Failed! \n");
        return -1;
    } else {
        printf("Init of MIC Success! \n");

    }

    pid_main = thread_getpid();

    pid_cnn_time = thread_create(stacks[0], sizeof(stacks[0]),
                            THREAD_PRIORITY_MAIN - 1,
                            THREAD_CREATE_WOUT_YIELD,
                            worker_cnn_time, NULL, "thread_cnn_time");

#ifndef SIMULATE_ADC
    set_sampling_end_notifyee_pid(pid_cnn_time);
    start_continuous_sample();
#else
    pid_audio_sampling = thread_create(stacks[1], sizeof(stacks[1]),
                            THREAD_PRIORITY_MAIN - 2,
                            THREAD_CREATE_WOUT_YIELD,
                            worker_audio_sampling, NULL, "thread_audio_sampling");
#endif
    while (1)
    {
        msg_t m;
        msg_receive(&m);
        if (m.type != INFERENCE_RESULT)
            continue;
        printf("Inference output: \n");
        print_array(output,2);
        // if (output[0] > output[1]) { //0 -> target, 1 -> non-target
        //     printf("Corn buntting sound detected! \n");
        // }

        if (output[0] > -1.0) {
            printf("Corn buntting sound detected! \n");
        }

        printf("time to result: %d \n", time_to_result);


    }

    return 0;
}
