#include <stdio.h>

#include "ztimer.h"
#include "periph/adc.h"
#include "periph/gpio.h"
#include "board.h"

#define RES             ADC_RES_10BIT
#define DELAY_MS        100U
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

int main(void)
{
    int sample = 0;

    puts("This test will sample all available ADC lines once every 100ms with\n"
         "a 10-bit resolution and print the sampled results to STDIO\n\n");

    int result;

    result = gpio_init_out_highdrive(RUN_MIC_PIN);

    if (result == 0) {
        printf("Success!\n");
    }
    else {
        printf("Failure!\n");
    }
    gpio_set(RUN_MIC_PIN);

    /* initialize all available ADC lines */
    if (adc_init(ADC_LINE(3)) < 0) {
            printf("Initialization of ADC_LINE(%u) failed\n", 3);
            return 1;
        } else {
            printf("Successfully initialized ADC_LINE(%u)\n", 3);
        }


    while (1) {
            const int BIAS_10_BITS = 100;
            sample = adc_sample(ADC_LINE(3), RES) - BIAS_10_BITS;
            // float f_val = (sample * ADC_REF_V) / (1 << ADC_BITS);
            if (sample < 0) {
                printf("ADC_LINE(%u): selected resolution not applicable\n", 3);
            } else {
                printf("ADC_LINE(%u): %i\n", 3, sample);
                // printf("adc volt: %f \n", f_val);
            }
        ztimer_sleep(ZTIMER_MSEC, DELAY_MS);
    }

    return 0;
}
