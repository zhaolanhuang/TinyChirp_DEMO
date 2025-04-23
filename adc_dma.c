#include <inttypes.h>
#include <stddef.h>

#include "common.h"
#include "cpu.h"
#include "mutex.h"
#include "periph/adc.h"
#include "periph_conf.h"

#include "thread.h"
#include "mutex.h"

/**
 * @name    Default ADC reference, gain configuration and acquisition time
 *
 * Can be overridden by the board configuration if needed. The default
 * configuration uses the full VDD (typically 3V3) as reference and samples for
 * 10us.
 * @{
 */
#ifndef ADC_REF
#define ADC_REF             SAADC_CH_CONFIG_REFSEL_VDD1_4
#endif
#ifndef ADC_GAIN
#define ADC_GAIN            SAADC_CH_CONFIG_GAIN_Gain1_4
#endif
#ifndef ADC_TACQ
#define ADC_TACQ            SAADC_CH_CONFIG_TACQ_10us
#endif
/** @} */

/**
 * @brief   Lock to prevent concurrency issues when used from different threads
 */
static mutex_t lock = MUTEX_INIT;

/**
 * @brief   We use a static result buffer so we do not have to reprogram the
 *          result pointer register
 */
static int16_t** _result_buffers;
static size_t _buffer_num = 0;
static size_t _buffer_size = 0;
static size_t _current_buf_idx = 0;
static size_t _prev_buf_idx = 0;
static kernel_pid_t _pid_notifyee;

static size_t next_free_buf_index(void)
{
    static size_t buffer_index = -1;
    buffer_index = (buffer_index + 1) % _buffer_num;
    return buffer_index;
}


static inline void saadc_enable(void)
{
    NRF_SAADC->ENABLE = 1;
}

static inline void saadc_disable(void)
{
    NRF_SAADC->ENABLE = 0;
}

static inline void prep(void)
{
    mutex_lock(&lock);
    saadc_enable();
}

static inline void done(void)
{
    saadc_disable();
    mutex_unlock(&lock);
}

void set_dma_buffer(int16_t** result_buffers, size_t buffer_num, size_t buffer_size) {
    _result_buffers = result_buffers;
    _buffer_num = buffer_num;
    _buffer_size = buffer_size;
}

void set_sampling_end_notifyee_pid(kernel_pid_t pid_notifyee) {
    _pid_notifyee = pid_notifyee;
}

int adc_dma_init(adc_t line)
{
    if (line >= ADC_NUMOF) {
        return -1;
    }

    prep();

    /* prevent multiple initialization by checking the result ptr register */
    if (NRF_SAADC->RESULT.PTR != (uint32_t)_result_buffers[0]) {
        /* set data pointer and the single channel we want to convert */
        NRF_SAADC->RESULT.MAXCNT = _buffer_size;
        NRF_SAADC->RESULT.PTR = (uint32_t)_result_buffers[0];

        /* configure the first channel (the only one we use):
         * - bypass resistor ladder+
         * - acquisition time as defined by board (or 10us as default)
         * - reference and gain as defined by board (or VDD as default)
         * - no oversampling */
        NRF_SAADC->CH[0].CONFIG = ((ADC_GAIN << SAADC_CH_CONFIG_GAIN_Pos) |
                                   (ADC_REF << SAADC_CH_CONFIG_REFSEL_Pos) |
                                   (ADC_TACQ << SAADC_CH_CONFIG_TACQ_Pos));
        NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC;
        NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;

        /* calibrate SAADC */
        NRF_SAADC->EVENTS_CALIBRATEDONE = 0;
        NRF_SAADC->TASKS_CALIBRATEOFFSET = 1;
        while (NRF_SAADC->EVENTS_CALIBRATEDONE == 0) {}
    }

    done();

    return 0;
}

static inline void nrf_saadc_task_start_trigger(void)
{
    NRF_SAADC->TASKS_START = 0x1UL;
}

static inline void nrf_saadc_task_sample_trigger(void)
{
    NRF_SAADC->TASKS_SAMPLE = 0x1UL;
}

static inline void nrf_saadc_task_stop_trigger(void)
{
    NRF_SAADC->TASKS_STOP = 0x1UL;
}

static inline void nrf_saadc_int_enable(uint32_t mask)
{
    NRF_SAADC->INTEN = mask;
}

static inline void nrf_saadc_int_disable(uint32_t mask)
{
    NRF_SAADC->INTENCLR = mask;
}


int32_t adc_start_continuous_sample(adc_t line, adc_res_t res, uint32_t sample_rate)
{
    assert(line < ADC_NUMOF);

    /* check if resolution is valid */
    if (res > 2) {
        return -1;
    }

#ifdef SAADC_CH_PSELP_PSELP_VDDHDIV5
    if (line == NRF52_VDDHDIV5) {
        line = SAADC_CH_PSELP_PSELP_VDDHDIV5;
    } else {
        line += 1;
    }
#else
    line += 1;
#endif

    /* prepare device */
    saadc_enable();

    /* set resolution */
    NRF_SAADC->RESOLUTION = res;
    /* set line to sample */
    NRF_SAADC->CH[0].PSELP = line;

    NRF_SAADC->SAMPLERATE = (SAADC_SAMPLERATE_MODE_Timers << SAADC_SAMPLERATE_MODE_Pos)
    | ((uint32_t)(16000000 / sample_rate) << SAADC_SAMPLERATE_CC_Pos);

    _current_buf_idx = next_free_buf_index();
    NRF_SAADC->RESULT.PTR = (uint32_t)_result_buffers[_current_buf_idx];
    nrf_saadc_int_enable(SAADC_INTEN_STARTED_Msk | SAADC_INTEN_STOPPED_Msk |  SAADC_INTEN_END_Msk);

    printf("SADDC-> INTEN: %" PRIx32 "\n", NRF_SAADC->INTEN);
    NVIC_EnableIRQ(SAADC_IRQn);

    nrf_saadc_task_start_trigger();

}

//Override the default isr function of SAADC
void isr_saadc(void) {
    if (NRF_SAADC-> EVENTS_STARTED) {
        NRF_SAADC-> EVENTS_STARTED = 0;
        _prev_buf_idx = _current_buf_idx;
        _current_buf_idx = next_free_buf_index();
        NRF_SAADC->RESULT.PTR = (uint32_t)_result_buffers[_current_buf_idx];
        nrf_saadc_task_sample_trigger();

    } else if (NRF_SAADC-> EVENTS_END) {
        NRF_SAADC-> EVENTS_END = 0;
        msg_t m;
        m.type = RING_BUFFER_FULL;
        m.content.value = _prev_buf_idx;
        msg_send(&m, _pid_notifyee);
        nrf_saadc_task_start_trigger();

    } else if(NRF_SAADC-> EVENTS_STOPPED) {

    }
    
}