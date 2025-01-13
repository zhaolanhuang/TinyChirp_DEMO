APPLICATION = TinyChirp_DEMO

BOARD ?= microbit-v2

RIOTBASE= $(HOME)/RIOT

FEATURES_REQUIRED = periph_adc periph_gpio
USEMODULE += ztimer
USEMODULE += ztimer_msec ztimer_usec
USEMODULE += printf_float

CFLAGS += -DADC_GAIN=SAADC_CH_CONFIG_GAIN_Gain4

include $(RIOTBASE)/Makefile.include
