APPLICATION = TinyChirp_DEMO

BOARD ?= microbit-v2

RIOTBASE= $(HOME)/RIOT

WERROR=0

SIMULATE_ADC=1

ifneq ($(BOARD), native)
FEATURES_REQUIRED = periph_adc periph_gpio
else
SIMULATE_ADC=1
endif

ifeq ($(SIMULATE_ADC), 1)
CFLAGS += -DSIMULATE_ADC
endif

USEMODULE += ztimer
USEMODULE += ztimer_usec

USEMODULE += printf_float
BLOBS += resampled_audio.bin

CFLAGS += -DADC_GAIN=SAADC_CH_CONFIG_GAIN_Gain4
CFLAGS += -DTHREAD_STACKSIZE_DEFAULT=2048

include $(RIOTBASE)/Makefile.include
