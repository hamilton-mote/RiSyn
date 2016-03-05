/*
* Copyright (C) 2016 University of California, Berkeley
*
* This file is subject to the terms and conditions of the GNU Lesser
* General Public License v2.1. See the file LICENSE in the top level
* directory for more details.
 */

/**
 * @defgroup    drivers_mma7660 MMA7660 Accelerometer
 * @ingroup     drivers_sensors
 * @brief       Driver for the Freescale MMA7660 3-Axis accelerometer.
 *              This driver only implements basic functionality.
 *
 * @{
 *
 * @file
 * @brief       Interface definition for the MMA7660 accelerometer driver.
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 */

#ifndef MMA7660_H
#define MMA7660_H

#include <stdint.h>
#include <stdbool.h>
#include "periph/i2c.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
  * @brief   Device descriptor for an MMA7660 device
  * @{
  */
typedef struct
{
  i2c_t i2c;      /**< the I2C handle */
  uint8_t addr;   /**< the device I2C address */
} mma7660_t;
/**@}*/

/**
  * @brief   Parameters for an MMA7660 device
  * @{
  */
typedef struct
{
  i2c_t i2c;      /**< the I2C handle */
  uint8_t addr;   /**< the device I2C address */
  uint8_t amsr;   /**< active mode sample rate */
  uint8_t awsr;   /**< auto wake sample rate */
  uint8_t filt;   /**< filter samples */
} mma7660_params_t;
/**@}*/

#define MODE_ACTIVE_SHIFT    0
#define MODE_AUTOWAKE_SHIFT  3
#define MODE_AUTOSLEEP_SHIFT 4
#define MODE_PRESCALE_SHIFT  5
#define MODE_INTERRUPT_DEFAULT 0x40 /* Active low, push-pull */

#define MMA7660_INTSOURCE_NONE        0x00
#define MMA7660_INTSOURCE_FB          0x01
#define MMA7660_INTSOURCE_UDLR        0x02
#define MMA7660_INTSOURCE_TAP         0x04
#define MMA7660_INTSOURCE_AUTOSLEEP   0x08
#define MMA7660_INTSOURCE_MEASURE     0x10
#define MMA7660_INTSOURCE_SHX         0x20
#define MMA7660_INTSOURCE_SHY         0x40
#define MMA7660_INTSOURCE_SHZ         0x80

#define MMA7660_SR_AMPD   0x00 <
#define MMA7660_SR_AM64   0x01
#define MMA7660_SR_AM32   0x02
#define MMA7660_SR_AM16   0x03
#define MMA7660_SR_AM8    0x04
#define MMA7660_SR_AM4    0x05
#define MMA7660_SR_AM2    0x06
#define MMA7660_SR_AM1    0x07
#define MMA7660_SR_AW32   0x00
#define MMA7660_SR_AW16   0x08
#define MMA7660_SR_AW8    0x10
#define MMA7660_SR_AW1    0x18
#define MMA7660_PDET_X 0x20
#define MMA7660_PDET_Y 0x40
#define MMA7660_PDET_Z 0x80

#define MMA7660_ADDR    0x4C

/* This is actually 46.9 but the sensor is not accurate enough
 * for that to matter
 */
#define MMA7660_MG_PER_COUNT 47

/**
 * @brief   Initialize an MMA7660 device
 *
 * @param[out] dev          device descriptor
 * @param[in] params        device configuration parameters
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_init(mma7660_t *dev, const mma7660_params_t *params);


/**
 * @brief   Set the mode register
 *
 * @param[in] dev          device descriptor
 * @param[in] active       0=sleep 1=active
 * @param[in] autowake     see datasheet
 * @param[in] autosleep    see datasheet
 * @param[in] prescale     main clock prescalar
 *
 * See page 17 of http://www.nxp.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
 * for information about the parameters
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_set_mode(mma7660_t *dev, uint8_t active,
    uint8_t autowake, uint8_t autosleep, uint8_t prescale);

/**
 * @brief   Read the tilt register
 *
 * @param[in] dev          device descriptor
 * @param[out] res         tilt register contents
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_read_tilt(mma7660_t *dev, uint8_t *res);

/**
 * @brief   Write the sleep count register
 *
 * @param[in] dev          device descriptor
 * @param[in] sleep        sleep count
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_write_sleep_count(mma7660_t *dev, uint8_t sleep);

/**
 * @brief   Configure the interrupt sources
 *
 * @param[in] dev             device descriptor
 * @param[in] isource_flags   interrupt source flags
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_config_interrupts(mma7660_t *dev, uint8_t isource_flags);

/**
 * @brief   Configure the sample rate
 *
 * @param[in] dev             device descriptor
 * @param[in] amsr            active mode sample rate (pg 18 of DS)
 * @param[in] awsr            auto wake sample rate (pg 19 of DS)
 * @param[in] filt            filter samples (pg 19 of DS)
 *
 * See datasheet http://www.nxp.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
 * for details about the parameters
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_config_samplerate(mma7660_t *dev, uint8_t amsr, uint8_t awsr, uint8_t filt);

/**
 * @brief   Configure the tap detection
 *
 * @param[in] dev             device descriptor
 *
 * See page 21 of http://www.nxp.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
 * for details about the parameters
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_config_pdet(mma7660_t *dev, uint8_t pdth, uint8_t enabled_axes);

/**
 * @brief   Configure the tap detection debounce count
 *
 * @param[in] dev             device descriptor
 *
 * See page 21 of http://www.nxp.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
 * for details about the debouncer
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_config_pd(mma7660_t *dev, uint8_t pd) ;

/**
 * @brief   Read the acceleration counts converted to mG
 *
 * @param[in] dev             device descriptor
 * @param[out] x              the X axis value in mG
 * @param[out] y              the Y axis value in mG
 * @param[out] z              the Z axis value in mG
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_read(mma7660_t *dev, int16_t *x, int16_t *y, int16_t *z);

/**
 * @brief   Read the acceleration counts (unconverted)
 *
 * @param[in] dev             device descriptor
 * @param[out] x              the X axis value
 * @param[out] y              the Y axis value
 * @param[out] z              the Z axis value
 *
 * See page 28 of http://www.nxp.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
 * for conversion of acceleration counts to angles or G forces.
 *
 * @return                   0 on success
 * @return                  -1 on error
 */
int mma7660_read_counts(mma7660_t *dev, int8_t *x, int8_t *y, int8_t *z);

#ifdef __cplusplus
}
#endif

#endif
/** @} */
