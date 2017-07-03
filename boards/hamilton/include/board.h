/*
 * Copyright (C) 2016 University of California, Berkeley
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/**
 * @defgroup    boards_hamilton
 * @ingroup     boards
 * @brief       Support for the Hamilton mote
 * @{
 *
 * @file
 * @brief       Board specific definitions for the Hamilton mote
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 * @author      Thomas Eichinger <thomas.eichinger@fu-berlin.de>
 * @author      Hyun Sin Kim <hs.kim@berkeley.edu>
 */

#ifndef BOARD_H_
#define BOARD_H_

#include "cpu.h"
#include "periph_conf.h"
#include "periph_cpu.h"
#include "div.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Define the nominal CPU core clock in this board
 */
#define F_CPU               (CLOCK_CORECLOCK)


/**
 * Assign the xtimer source
 */
#define XTIMER_DEV                     TIMER_RTT
#define XTIMER_CHAN                    (0)
#define XTIMER_HZ                      32768UL
#define XTIMER_USEC_TO_TICKS(value)    ( div_u32_by_15625div512(value) )
#define XTIMER_TICKS_TO_USEC(value)    ( ((uint64_t)value * 15625)>>9 )

#define STIMER_DEV                     TIMER_1 /* This timer is to support low-power/slow XTIMER */
#define STIMER_HZ                      1000000UL

#ifdef STIMER_DEV
#define XTIMER_BACKOFF                 30  /* ticks: Threshold to determine spin or not 
                                              It takes 150~200us to get the current time */
#define XTIMER_OVERHEAD                6   /* ticks: How much earlier does a timer expires? */
#define XTIMER_ISR_BACKOFF             20  
#define XTIMER_PERIODIC_RELATIVE       100
#endif

 /**
  * @name AT86RF233 configuration
  *
  * {spi bus, spi speed, cs pin, int pin, reset pin, sleep pin}
  */
#if CLOCK_USE_FLL
#define AT86RF2XX_PARAMS_BOARD      {.spi = SPI_DEV(0), \
                                     .spi_clk = SPI_CLK_5MHZ, \
                                     .cs_pin = GPIO_PIN(PB, 31), \
                                     .int_pin = GPIO_PIN(PB, 0), \
                                     .sleep_pin = GPIO_PIN(PA, 20), \
                                     .reset_pin = GPIO_PIN(PB, 15)}
#else
#define AT86RF2XX_PARAMS_BOARD      {.spi = SPI_DEV(0), \
                                     .spi_clk = SPI_CLK_1MHZ, \
                                     .cs_pin = GPIO_PIN(PB, 31), \
                                     .int_pin = GPIO_PIN(PB, 0), \
                                     .sleep_pin = GPIO_PIN(PA, 20), \
                                     .reset_pin = GPIO_PIN(PB, 15)}
#endif
/**
 * @name LED pin definitions
 * @{
 */
#define LED_PORT            PORT->Group[0]
#define LED_PIN             (19)
#define LED_GPIO            GPIO_PIN(0, 19)
/** @} */

/**
 * @name Macros for controlling the on-board LEDs.
 * @{
 */
#define LED_ON              (LED_PORT.OUTSET.reg = (1 << LED_PIN))
#define LED_OFF             (LED_PORT.OUTCLR.reg = (1 << LED_PIN))
#define LED_TOGGLE          (LED_PORT.OUTTGL.reg = (1 << LED_PIN))

/* for compatability to other boards */
#define LED_GREEN_ON        /* not available */
#define LED_GREEN_OFF       /* not available */
#define LED_GREEN_TOGGLE    /* not available */
#define LED_ORANGE_ON       /* not available */
#define LED_ORANGE_OFF      /* not available */
#define LED_ORANGE_TOGGLE   /* not available */
#define LED_RED_ON          LED_ON
#define LED_RED_OFF         LED_OFF
#define LED_RED_TOGGLE      LED_TOGGLE
/** @} */

/**
 * @brief Initialize board specific hardware, including clock, LEDs and std-IO
 */
void board_init(void);

extern const uint64_t* const fb_sentinel;
extern const uint64_t* const fb_flashed_time;
extern const uint8_t*  const fb_eui64;
extern const uint16_t* const fb_device_id;
extern const uint64_t* const fb_designator;
extern const uint8_t*  const fb_aes128_key;
extern const uint8_t*  const fb_25519_pub;
extern const uint8_t*  const fb_25519_priv;
#define FB_SENTINEL_VALUE 0x27c83f60f6b6e7c8
#define HAS_FACTORY_BLOCK (*fb_sentinel == FB_SENTINEL_VALUE)

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
/** @} */
