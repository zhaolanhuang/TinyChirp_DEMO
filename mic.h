#ifndef MIC_H
#define MIC_H

#ifndef BOARD_NATIVE

#include "periph/adc.h"
#include "periph/gpio.h"
#include "board.h"

#define RES             ADC_RES_10BIT
#define BIAS_10_BITS 400

#define ADC_BITS 10
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
#endif


static inline int mic_init(void) {

#ifndef BOARD_NATIVE
    int result;

    result = gpio_init_out_highdrive(RUN_MIC_PIN);

    if (result == 0) {
        printf("Successfully initialized MIC GPIO!\n");
    }
    else {
        printf("Initialization of MIC GPIO Failed!\n");
        return -1;
    }
    gpio_set(RUN_MIC_PIN);

    /* initialize all available ADC lines */
    if (adc_init(ADC_LINE(3)) < 0) {
            printf("Initialization of ADC_LINE(%u) failed\n", 3);
            return -1;
        } else {
            printf("Successfully initialized ADC_LINE(%u)\n", 3);
    }

#endif
    return 0;
}

static inline int get_sample_value(void) {
    
#ifndef BOARD_NATIVE
    return adc_sample(ADC_LINE(3), RES) - BIAS_10_BITS;
#endif
    return 0;
}

#endif