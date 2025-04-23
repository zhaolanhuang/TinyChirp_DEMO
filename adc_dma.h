#ifndef ADC_DMA_H
#define ADC_DMA_H

#include <inttypes.h>
#include <stddef.h>

#include "common.h"
#include "cpu.h"
#include "mutex.h"
#include "periph/adc.h"
#include "periph_conf.h"

#include "thread.h"
#include "mutex.h"

void set_dma_buffer(int16_t** result_buffers, size_t buffer_num, size_t buffer_size);

void set_sampling_end_notifyee_pid(kernel_pid_t pid_notifyee);

int adc_dma_init(adc_t line);

int32_t adc_start_continuous_sample(adc_t line, adc_res_t res, uint32_t sample_rate);

//Override the default isr function of SAADC
void isr_saadc(void);

#endif