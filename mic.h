#ifndef MIC_H
#define MIC_H

#ifndef SIMULATE_ADC

#include "periph/adc.h"
#include "periph/gpio.h"
#include "board.h"
#include "adc_dma.h"

#define RES             ADC_RES_12BIT
#define BIAS_10_BITS 102
#define BIAS_VOLT 0.1f
#define RESOLUTION_12BIT 4096.0f

#define ADC_BITS 12
#define GPIO_OUT_HIGHDRIVE GPIO_MODE(1, 1, 0, 3)

const float ADC_REF_V = 3.3 / 4;

#define PORT_BIT            (1 << 5)
#define PIN_MASK            (0x1f)

/* Compatibility wrapper defines for nRF9160 */
#ifdef NRF_P0_S
#define NRF_P0 NRF_P0_S
#endif

#ifdef NRF_P1_S
#define NRF_P1 NRF_P1_S
#endif
/**
 * @brief   Get the port's base address
 */
static inline NRF_GPIO_Type *port(gpio_t pin)
{
#if (CPU_FAM_NRF51)
    (void) pin;
    return NRF_GPIO;
#elif defined(NRF_P1)
    return (pin & PORT_BIT) ? NRF_P1 : NRF_P0;
#else
    (void) pin;
    return NRF_P0;
#endif
}

/**
 * @brief   Get a pin's offset
 */
static inline int pin_num(gpio_t pin)
{
#if GPIO_COUNT > 1
    return (pin & PIN_MASK);
#else
    return (int)pin;
#endif
}

int gpio_init_out_highdrive(gpio_t pin)
{

    port(pin)->PIN_CNF[pin_num(pin)] = GPIO_OUT_HIGHDRIVE;
    return 0;
}

#else

#include "blob/resampled_audio.bin.h"

#endif


static inline int mic_init(void) {

#ifndef SIMULATE_ADC
    int result;

    result = gpio_init_out_highdrive(RUN_MIC_PIN);
    // result = gpio_init(RUN_MIC_PIN, GPIO_OUT);

    if (result == 0) {
        printf("Successfully initialized MIC GPIO!\n");
    }
    else {
        printf("Initialization of MIC GPIO Failed!\n");
        return -1;
    }
    // gpio_clear(RUN_MIC_PIN);

    gpio_set(RUN_MIC_PIN);

    /* initialize all available ADC lines */
    if (adc_dma_init(ADC_LINE(3)) < 0) {
            printf("Initialization of ADC_LINE(%u) failed\n", 3);
            return -1;
        } else {
            printf("Successfully initialized ADC_LINE(%u)\n", 3);
    }

#endif
    return 0;
}

#ifndef SIMULATE_ADC

static inline void start_continuous_sample() {
    adc_start_continuous_sample(ADC_LINE(3), RES, SAMPLE_RATE);
}

#endif

static inline int16_t get_sample_value(void) {

#ifndef SIMULATE_ADC
    return adc_sample(ADC_LINE(3), RES);
#endif
    return 0;
}

static inline real_t get_amplitude(void) {
#ifndef SIMULATE_ADC
    // return ((adc_sample(ADC_LINE(3), RES)) * ADC_REF_V / 4.0f / RESOLUTION_12BIT - BIAS_VOLT) * 1000; // mV
    // return ((adc_sample(ADC_LINE(3), RES)) * ADC_REF_V / 4.0f / RESOLUTION_12BIT) * 1000; // mV
    return ((adc_sample(ADC_LINE(3), RES)) / 4096.0 - 0.39) * 2.5; // Empircal fomular deduced from measurement..
#else
    static int i = 0;
    real_t *input = (real_t*) &resampled_audio_bin;
    real_t amplitude = input[i];
    i++;
    if (i == MODEL_INPUT_SIZE)  i = 0;
    return amplitude;
#endif
}


#endif