/*
 * Copyright (C) 2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     cpu_samr21
 * @{
 *
 * @file
 * @brief       ADC driver implementation
 *
 * @author      Michael Andersen <m.andersen@berkeley.edu>
 * @author      Hyun Sin Kim <hs.kim@berkeley.edu>
 * @author      Rane Balslev (SAMR21) <ranebalslev@gmail.com>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Mark Solters <msolters@driblet.io>
 *
 * @}
 */

#include <stdint.h>
#include "cpu.h"
#include "periph/adc.h"
#include "periph_conf.h"
#define ENABLE_DEBUG    (0)
#include "debug.h"

/* guard in case that no ADC device is defined */
#if ADC_NUMOF

int adc_init(adc_t channel) {

    /*  Disable ADC Module before init. */
    ADC_DEV->CTRLA.bit.ENABLE = 0;

  	/* Setup generic clock mask for adc */
  	PM->APBCMASK.reg |= PM_APBCMASK_ADC;

    /* GCLK Setup*/
    GCLK->CLKCTRL.reg = (uint32_t)(GCLK_CLKCTRL_CLKEN     |
                                   GCLK_CLKCTRL_GEN_GCLK0 |
                                   (ADC_GCLK_ID << GCLK_CLKCTRL_ID_Pos));

    /* Reset */
    ADC_DEV->CTRLA.bit.SWRST = 1;
    while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    /* Set RUN_IN_STANDBY */
    ADC_DEV->CTRLA.bit.RUNSTDBY = 0;

    /* Set Voltage Reference */
    ADC_DEV->REFCTRL.bit.REFSEL  = ADC_REFCTRL_REFSEL_INT1V_Val;
    ADC_DEV->REFCTRL.bit.REFCOMP = 1;

    /* Set the accumulation and divide result */
    ADC_DEV->AVGCTRL.bit.SAMPLENUM = 6;
	ADC_DEV->AVGCTRL.bit.ADJRES    = 4;

    /* Set Sample length */
    ADC_DEV->SAMPCTRL.bit.SAMPLEN = 32;
	while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    /* Configure CTRLB Register HERE IS THE RESOLUTION SET!*/
    ADC_DEV->CTRLB.bit.DIFFMODE  = 0;
    ADC_DEV->CTRLB.bit.FREERUN   = 0;
    ADC_DEV->CTRLB.bit.CORREN    = 0;
    ADC_DEV->CTRLB.bit.LEFTADJ   = 0; // Right-adjusted results
    ADC_DEV->CTRLB.bit.RESSEL    = ADC_CTRLB_RESSEL_12BIT_Val;
    ADC_DEV->CTRLB.bit.PRESCALER = ADC_CTRLB_PRESCALER_DIV512_Val;
    while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    ADC_DEV->INPUTCTRL.bit.GAIN        = ADC_INPUTCTRL_GAIN_1X_Val;
    ADC_DEV->INPUTCTRL.bit.INPUTOFFSET = 0;

    /* Port configuration */
    PM->APBBMASK.reg |= PM_APBBMASK_PORT;

    int pin = ADC_GET_PIN(channel);
    PortGroup* pg = ADC_GET_PORT_GROUP(channel);

    volatile void* t1 = (void*)pg;
    volatile int vpin = pin;
    printf("t1 is %x\n", (unsigned int) t1);
    printf("pin is %d\n", vpin);
    pg->DIRCLR.reg = (1 << pin);
    pg->PINCFG[pin].bit.INEN = 1;
    pg->PINCFG[pin].bit.PMUXEN = 1;
    pg->PMUX[pin >> 1].reg &= ~(0xf << (4 * (pin & 0x1)));
  	pg->PMUX[pin >> 1].reg |= (PORT_PMUX_PMUXE_B_Val << (4 * (pin & 0x1)));

    ADC_DEV->INPUTCTRL.bit.MUXNEG      = ADC_INPUTCTRL_MUXNEG_IOGND_Val;
    while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    /* Configure Window Mode Register */
    ADC_DEV->WINCTRL.reg = ADC_WINCTRL_WINMODE_DISABLE;
    while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    /* Enable interrupts and events related to result */
    ADC_DEV->EVCTRL.bit.RESRDYEO = 1;
    ADC_DEV->INTENSET.bit.RESRDY = 1;

	/* ADC runs during debug mode */
    ADC_DEV->DBGCTRL.bit.DBGRUN = 1;
    while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    return 0;
}



int adc_sample(adc_t channel, adc_res_t res){
	int output;

	/* Set input channel for ADC */
    int chan = ADC_GET_CHANNEL(channel);
    ADC_DEV->INPUTCTRL.bit.MUXPOS = chan;

	/*  Enable bandgap: It is enabled temporarily during sampling procedure to minimize idle current */
	SYSCTRL->VREF.reg |= SYSCTRL_VREF_BGOUTEN;

	/* Set resolution */
	switch (res) {
		case ADC_RES_8BIT:
			ADC_DEV->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_8BIT_Val;
			break;
		case ADC_RES_10BIT:
			ADC_DEV->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_10BIT_Val;
			break;
		case ADC_RES_12BIT:
			ADC_DEV->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_12BIT_Val;
			break;
		case ADC_RES_16BIT:
			ADC_DEV->CTRLB.bit.RESSEL = ADC_CTRLB_RESSEL_16BIT_Val;
			break;
		default:
			return -1;
			break;
	}

	/* Wait for sync. */
	while (ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

	/* Enable ADC Module. */
    ADC_DEV->CTRLA.bit.ENABLE = 1;
	while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

	/* Start the conversion. */
	ADC_DEV->SWTRIG.reg = ADC_SWTRIG_START;

	/* Wait for the result. */
	while (!(ADC_DEV->INTFLAG.reg & ADC_INTFLAG_RESRDY));

	/* Read result */
	output = (int)ADC_DEV->RESULT.reg;
	while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

    ADC_DEV->CTRLA.bit.ENABLE = 0;
	while(ADC_DEV->STATUS.reg & ADC_STATUS_SYNCBUSY);

	/*  Disable bandgap */
	SYSCTRL->VREF.reg &= ~SYSCTRL_VREF_BGOUTEN;

	/* Return result. */
	return output;
}


#endif /* ADC_NUMOF */
