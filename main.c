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

#include "sos_filter.h"
#include "resample.h"
#include "min_max_scaler.h"
#include "signal_energy.h"

// #define NUM_OF_SOS 5
// const sos_coeff_t highpass_coeff[NUM_OF_SOS][6] = {
//     {1.55854712e-07, -3.11709423e-07, 1.55854712e-07,
//      1.00000000e+00, 6.68178638e-01, 0.00000000e+00},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, 1.35904130e+00, 4.71015698e-01},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, 1.42887946e+00, 5.46607979e-01},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, 1.55098998e+00, 6.78779458e-01},
//     {1.00000000e+00, -1.00000000e+00, 0.00000000e+00,
//      1.00000000e+00, 1.73262236e+00, 8.75376926e-01},
// }; // butterworth hp 9 order, cutoff 7000 Hz

#define NUM_OF_SOS 5
const sos_coeff_t highpass_coeff[NUM_OF_SOS][6] =
{{ 0.31971562,-0.31971562,0.0,1.0,-0.66817864,0.0},
 { 1.0,-2.0,1.0,1.0,-1.3590413,0.4710157 },
 { 1.0,-2.0,1.0,1.0,-1.42887946,0.54660798},
 { 1.0,-2.0,1.0,1.0,-1.55098998,0.67877946},
 { 1.0,-2.0,1.0,1.0,-1.73262236,0.87537693},
}; //// butterworth hp 9 order, cutoff 1000 Hz

#define NUM_OF_LP_SOS 5
const sos_coeff_t lowpass_coeff[NUM_OF_LP_SOS][6] = {
    { 6.71705220e-04,1.34341044e-03,6.71705220e-04,1.00000000e+00
    ,-1.98912367e-01,0.00000000e+00},
     { 1.00000000e+00,2.00000000e+00,1.00000000e+00,1.00000000e+00
    ,-4.09689602e-01,7.05705211e-02},
     { 1.00000000e+00,2.00000000e+00,1.00000000e+00,1.00000000e+00
    ,-4.48177181e-01,1.71143414e-01},
     { 1.00000000e+00,2.00000000e+00,1.00000000e+00,1.00000000e+00
    ,-5.23528317e-01,3.68045419e-01},
     { 1.00000000e+00,1.00000000e+00,0.00000000e+00,1.00000000e+00
    ,-6.59554533e-01,7.23499052e-01}
}; //butter lp 9 order, cutoff 3000

// #define NUM_OF_SOS 9
// const sos_coeff_t highpass_coeff[NUM_OF_SOS][6] = {
//     {3.48262720e-05, -6.96525440e-05, 3.48262720e-05,
//      1.00000000e+00, -2.67560228e-02, 2.08456815e-03},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -2.71679039e-02, 1.75106169e-02},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -2.80173964e-02, 4.93263821e-02},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -2.93594494e-02, 9.95898520e-02},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -3.12860424e-02, 1.71745912e-01},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -3.39403078e-02, 2.71155244e-01},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -3.75409102e-02, 4.06007429e-01},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -4.24244000e-02, 5.88907176e-01},
//     {1.00000000e+00, -2.00000000e+00, 1.00000000e+00,
//      1.00000000e+00, -4.91210710e-02, 8.39715402e-01}
//     };

// Initialize the SOS filters
static SOS sos_filters[NUM_OF_SOS];
static SOS lp_sos_filters[NUM_OF_LP_SOS];

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

static real_t sum_energy = 0;
static real_t hp_energy = 0;
static real_t lp_energy = 0;

static real_t lp_filter_output[RING_BUFFER_SIZE];
static real_t filter_output[RING_BUFFER_SIZE];
static real_t output_tile[CHANNEL_NUM2];
static real_t output_tile_ckpt[CHECKPOINT_NUM][CHANNEL_NUM2];
static real_t lp_energy_ckpt[CHECKPOINT_NUM];
static real_t hp_energy_ckpt[CHECKPOINT_NUM];
static real_t energy_ckpt[CHECKPOINT_NUM];


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

    memset(output_tile, 0, sizeof(output_tile));

    unsigned int ckpt_idx = 0;
    memset(output_tile_ckpt, 0, sizeof(output_tile_ckpt));

    memset(filter_output, 0, sizeof(filter_output));

    memset(hp_energy_ckpt, 0, sizeof(hp_energy_ckpt));
    memset(lp_energy_ckpt, 0, sizeof(lp_energy_ckpt));

    memset(energy_ckpt, 0, sizeof(energy_ckpt));

    for (int i = 0; i < NUM_OF_SOS; i++)
    {
        init_sos(&sos_filters[i], &highpass_coeff[i][0], &highpass_coeff[i][3]);
    }

    for (int i = 0; i < NUM_OF_LP_SOS; i++)
    {
        init_sos(&lp_sos_filters[i], &lowpass_coeff[i][0], &lowpass_coeff[i][3]);
    }

    // min_max_scaler_t scaler;
    // init_min_max_scaler(&scaler);

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
        apply_cascaded_sos(sos_filters, NUM_OF_SOS, ring_buffer[r_idx], filter_output, RING_BUFFER_SIZE);
            // Model works with 3 sec audio but buffer has only 1 sec signal. So we use partial convolution - we aggregate output_tile
        CNN_model_inference((real_t*)filter_output, output, conv1weight, channel_number1, kernelSize1, conv2weight, channel_number2, kernelSize2, 
                            tile_size, input_size, fc1weight, fc2weight,fc1bias,fc2bias, conv1bias, conv2bias, output_tile_ckpt[ckpt_idx]);
            // print_array_output_tile(output_tile, channel_number2);
        // init_min_max_scaler(&scaler);
        apply_cascaded_sos(lp_sos_filters, NUM_OF_LP_SOS, ring_buffer[r_idx], lp_filter_output, RING_BUFFER_SIZE);
        

        // find_min_max(filter_output, RING_BUFFER_SIZE, &scaler);
        // apply_min_max_scaler(filter_output, RING_BUFFER_SIZE, filter_output, &scaler);
        lp_energy_ckpt[ckpt_idx] += calculate_energy(lp_filter_output, RING_BUFFER_SIZE);
        hp_energy_ckpt[ckpt_idx] += calculate_energy(filter_output, RING_BUFFER_SIZE);
        energy_ckpt[ckpt_idx] += calculate_energy(ring_buffer[r_idx], RING_BUFFER_SIZE);
        j++;
        // printf("Inference End of Conv: j = %d\n", j);


        // If output_tile has data from 3 seconds (3 buffers) do a prediction
        if (j == SLIDING_WINDOW_STEP / RING_BUFFER_SIZE) {
            // printf("outputSize: %d \n", outputSize);
            
            sum_energy = 0;
            hp_energy = 0;
            lp_energy = 0;
            for (int i = 0; i < CHECKPOINT_NUM; i++) {
                for (int k = 0; k< channel_number2;k++){
                    output_tile[k] += output_tile_ckpt[i][k];
                }
                sum_energy += energy_ckpt[i];
                hp_energy += hp_energy_ckpt[i];
                lp_energy += lp_energy_ckpt[i];
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

        hp_energy_ckpt[ckpt_idx] = 0;
        energy_ckpt[ckpt_idx] = 0;
        lp_energy_ckpt[ckpt_idx] = 0;

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
        printf("Inference output: ");
        print_array(output,2);
        printf("Sum Energy: %.6f \t HP Energy: %.6f \t LP Energy: %.6f \n", sum_energy, hp_energy, lp_energy);
        printf("HP/S: %.6f \t LP/S: %.6f \t HP/LP: %.6f \n", hp_energy/sum_energy, lp_energy/sum_energy, hp_energy/lp_energy);
        
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
