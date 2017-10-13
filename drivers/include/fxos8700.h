/*
 * Copyright (C) 2016 Michael Andersen
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_fxos8700 3-axis accelerometer/magnetometer
 * @ingroup     drivers_sensors
 *
 * The connection between the MCU and the FXOS8700 is based on the
 * I2C-interface.
 *
 * @{
 *
 * @file
 * @brief       Driver for the FXOS8700 3-axis accelerometer/magnetometer
 *
 * @author      Michael Andersen <m.andersen@cs.berkeley.edu>
 */

#ifndef FXOS8700_H_
#define FXOS8700_H_

#include <stdint.h>
#include "periph/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @name FXOS8700 register addresses
 * @{
 */
#define FXOS8700_REG__STATUS            (0x00)
#define FXOS8700_REG_OUT_X_MSB          (0x01)
#define FXOS8700_REG_OUT_X_LSB          (0x02)
#define FXOS8700_REG_OUT_Y_MSB          (0x03)
#define FXOS8700_REG_OUT_Y_LSB          (0x04)
#define FXOS8700_REG_OUT_Z_MSB          (0x05)
#define FXOS8700_REG_OUT_Z_LSB          (0x06)
#define FXOS8700_REG_F_SETUP            (0x09)
#define FXOS8700_REG_TRIG_CFG           (0x0A)
#define FXOS8700_REG_SYSMOD             (0x0B)
#define FXOS8700_REG_INT_SOURCE         (0x0C)
#define FXOS8700_REG_WHO_AM_I           (0x0D)
#define FXOS8700_REG_XYZ_DATA_CFG       (0x0E)
#define FXOS8700_REG_HP_FILTER_CUTOFF   (0x0F)
#define FXOS8700_REG_PL_STATUS          (0x10)
#define FXOS8700_REG_PL_CFG             (0x11)
#define FXOS8700_REG_PL_COUNT           (0x12)
#define FXOS8700_REG_PL_BF_ZCOMP        (0x13)
#define FXOS8700_REG_PL_THS_REG         (0x14)
#define FXOS8700_REG_A_FFMT_CFG         (0x15)
#define FXOS8700_REG_A_FFMT_SRC         (0x16)
#define FXOS8700_REG_A_FFMT_THS         (0x17)
#define FXOS8700_REG_A_FFMT_COUNT       (0x18)
#define FXOS8700_REG_TRANSIENT_CFG      (0x1D)
#define FXOS8700_REG_TRANSIENT_SRC      (0x1E)
#define FXOS8700_REG_TRANSIENT_THS      (0x1F)
#define FXOS8700_REG_TRANSIENT_COUNT    (0x20)
#define FXOS8700_REG_PULSE_CFG          (0x21)
#define FXOS8700_REG_PULSE_SRC          (0x22)
#define FXOS8700_REG_PULSE_THSX         (0x23)
#define FXOS8700_REG_PULSE_THSY         (0x24)
#define FXOS8700_REG_PULSE_THSZ         (0x25)
#define FXOS8700_REG_PULSE_TMLT         (0x26)
#define FXOS8700_REG_PULSE_LTCY         (0x27)
#define FXOS8700_REG_PULSE_WIND         (0x28)
#define FXOS8700_REG_ASLP_COUNT         (0x29)
#define FXOS8700_REG_CTRL_REG1          (0x2A)
#define FXOS8700_REG_CTRL_REG2          (0x2B)
#define FXOS8700_REG_CTRL_REG3          (0x2C)
#define FXOS8700_REG_CTRL_REG4          (0x2D)
#define FXOS8700_REG_CTRL_REG5          (0x2E)
#define FXOS8700_REG_OFF_X              (0x2F)
#define FXOS8700_REG_OFF_Y              (0x30)
#define FXOS8700_REG_OFF_Z              (0x31)
#define FXOS8700_REG_M_DR_STATUS        (0x32)
#define FXOS8700_REG_M_OUT_X_MSB        (0x33)
#define FXOS8700_REG_M_OUT_X_LSB        (0x34)
#define FXOS8700_REG_M_OUT_Y_MSB        (0x35)
#define FXOS8700_REG_M_OUT_Y_LSB        (0x36)
#define FXOS8700_REG_M_OUT_Z_MSB        (0x37)
#define FXOS8700_REG_M_OUT_Z_LSB        (0x38)
#define FXOS8700_REG_CMP_X_MSB          (0x39)
#define FXOS8700_REG_CMP_X_LSB          (0x3A)
#define FXOS8700_REG_CMP_Y_MSB          (0x3B)
#define FXOS8700_REG_CMP_Y_LSB          (0x3C)
#define FXOS8700_REG_CMP_Z_MSB          (0x3D)
#define FXOS8700_REG_CMP_Z_LSB          (0x3E)
#define FXOS8700_REG_M_OFF_X_MSB        (0x3F)
#define FXOS8700_REG_M_OFF_X_LSB        (0x40)
#define FXOS8700_REG_M_OFF_Y_MSB        (0x41)
#define FXOS8700_REG_M_OFF_Y_LSB        (0x42)
#define FXOS8700_REG_M_OFF_Z_MSB        (0x43)
#define FXOS8700_REG_M_OFF_Z_LSB        (0x44)
#define FXOS8700_REG_MAX_X_MSB          (0x45)
#define FXOS8700_REG_MAX_X_LSB          (0x46)
#define FXOS8700_REG_MAX_Y_MSB          (0x47)
#define FXOS8700_REG_MAX_Y_LSB          (0x48)
#define FXOS8700_REG_MAX_Z_MSB          (0x49)
#define FXOS8700_REG_MAX_Z_LSB          (0x4A)
#define FXOS8700_REG_MIN_X_MSB          (0x4B)
#define FXOS8700_REG_MIN_X_LSB          (0x4C)
#define FXOS8700_REG_MIN_Y_MSB          (0x4D)
#define FXOS8700_REG_MIN_Y_LSB          (0x4E)
#define FXOS8700_REG_MIN_Z_MSB          (0x4F)
#define FXOS8700_REG_MIN_Z_LSB          (0x50)
#define FXOS8700_REG_TEMP               (0x51)
#define FXOS8700_REG_M_THS_CFG          (0x52)
#define FXOS8700_REG_M_THS_SRC          (0x53)
#define FXOS8700_REG_M_THS_X_MSB        (0x54)
#define FXOS8700_REG_M_THS_X_LSB        (0x55)
#define FXOS8700_REG_M_THS_Y_MSB        (0x56)
#define FXOS8700_REG_M_THS_Y_LSB        (0x57)
#define FXOS8700_REG_M_THS_Z_MSB        (0x58)
#define FXOS8700_REG_M_THS_Z_LSB        (0x59)
#define FXOS8700_REG_M_THS_COUNT        (0x5A)
#define FXOS8700_REG_M_CTRL_REG1        (0x5B)
#define FXOS8700_REG_M_CTRL_REG2        (0x5C)
#define FXOS8700_REG_M_CTRL_REG3        (0x5D)
#define FXOS8700_REG_M_INT_SRC          (0x5E)
#define FXOS8700_REG_A_VECM_CFG         (0x5F)
#define FXOS8700_REG_A_VECM_THS_MSB     (0x60)
#define FXOS8700_REG_A_VECM_THS_LSB     (0x61)
#define FXOS8700_REG_A_VECM_CNT         (0x62)
#define FXOS8700_REG_A_VECM_INITX_MSB   (0x63)
#define FXOS8700_REG_A_VECM_INITX_LSB   (0x64)
#define FXOS8700_REG_A_VECM_INITY_MSB   (0x65)
#define FXOS8700_REG_A_VECM_INITY_LSB   (0x66)
#define FXOS8700_REG_A_VECM_INITZ_MSB   (0x67)
#define FXOS8700_REG_A_VECM_INITZ_LSB   (0x68)
#define FXOS8700_REG_M_VECM_CFG         (0x69)
#define FXOS8700_REG_M_VECM_THS_MSB     (0x6A)
#define FXOS8700_REG_M_VECM_THS_LSB     (0x6B)
#define FXOS8700_REG_M_VECM_CNT         (0x6C)
#define FXOS8700_REG_M_VECM_INITX_MSB   (0x6D)
#define FXOS8700_REG_M_VECM_INITX_LSB   (0x6E)
#define FXOS8700_REG_M_VECM_INITY_MSB   (0x6F)
#define FXOS8700_REG_M_VECM_INITY_LSB   (0x70)
#define FXOS8700_REG_M_VECM_INITZ_MSB   (0x71)
#define FXOS8700_REG_M_VECM_INITZ_LSB   (0x72)
#define FXOS8700_REG_A_FFMT_THS_X_MSB   (0x73)
#define FXOS8700_REG_A_FFMT_THS_X_LSB   (0x74)
#define FXOS8700_REG_A_FFMT_THS_Y_MSB   (0x75)
#define FXOS8700_REG_A_FFMT_THS_Y_LSB   (0x76)
#define FXOS8700_REG_A_FFMT_THS_Z_MSB   (0x77)
#define FXOS8700_REG_A_FFMT_THS_Z_LSB   (0x78)
/** @} */


/**
 * @brief   Parameters needed for device initialization
 */
typedef struct {
    i2c_t i2c;              /**< I2C device that sensor is connected to */
    uint8_t addr;           /**< I2C address of this particular sensor */
} fxos8700_params_t;

/**
  * @brief   Device descriptor for a AT30TSE75x device
  * @{
  */
typedef struct {
    fxos8700_params_t p;
    uint8_t whoami;
} fxos8700_t;

/**
  * @brief   Individual accel measurement
  * @{
  */
typedef struct {
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
} fxos8700_measurement_acc_t;

/**
  * @brief   Individual mag measurement
  * @{
  */
typedef struct {
    int16_t mag_x;
    int16_t mag_y;
    int16_t mag_z;
} fxos8700_measurement_mag_t;

typedef union {
  struct {
    uint8_t  zyxow:1;        /*!< bit:      0  Start Conversion Event In          */
    uint8_t  zow:1;           /*!< bit:      1  Synchronization Event In           */
    uint8_t  yow:1;          /*!< bit:  2.. 3  Reserved                           */
    uint8_t  xow:1;        /*!< bit:      0  Start Conversion Event In          */
    uint8_t  zyxdr:1;         /*!< bit:      1  Synchronization Event In           */
    uint8_t  zdr:1;               /*!< bit:  2.. 3  Reserved                           */
    uint8_t  ydr:1;        /*!< bit:      0  Start Conversion Event In          */
    uint8_t  xdr:1;         /*!< bit:      1  Synchronization Event In           */
  } bit;                       /*!< Structure used for bit  access                  */
  uint8_t reg;                 /*!< Type      used for register access              */
} FXOS8700_STATUS_Type;

typedef union {
  struct {
    uint8_t  aslp_rate:2;     /*!< bit:      auto-wake sample frequency (for auto-sleep mode) */
    uint8_t  dr:3;            /*!< bit:      Output data rate (ODR) selection      */
    uint8_t  lnoise:1;        /*!< bit:      normal (0) / reduced noise (1)        */
    uint8_t  f_read:1;        /*!< bit:      normal (0) / fast read with 8 bit output (1)     */
    uint8_t  active:1;        /*!< bit:      standby (0) / active (1)              */
  } bit;                      /*!< Structure used for bit  access                  */
  uint8_t reg;                /*!< Type      used for register access              */
} FXOS8700_CTRL_REG1_Type;

typedef union {
  struct {
    uint8_t  st:1;            /*!< bit:      self-test  */
    uint8_t  rst:1;           /*!< bit:      software reset      */
    uint8_t  :1;              /*!< bit:      normal (0) / reduced noise (1)        */
    uint8_t  smods:2;         /*!< bit:      normal (0) / fast read with 8 bit output (1)     */
    uint8_t  slpe:1;          /*!< bit:      standby (0) / active (1)              */
    uint8_t  modes:2;         /*!< bit:      standby (0) / active (1)              */
  } bit;                      /*!< Structure used for bit  access                  */
  uint8_t reg;                /*!< Type      used for register access              */
} FXOS8700_CTRL_REG2_Type;

typedef union {
  struct {
    uint8_t  fgerr:1;        /*!< bit:      0  Start Conversion Event In          */
    uint8_t  figt:5;         /*!< bit:      1  Synchronization Event In           */
    uint8_t  sysmod:2;               /*!< bit:  2.. 3  Reserved                           */
  } bit;                       /*!< Structure used for bit  access                  */
  uint8_t reg;                 /*!< Type      used for register access              */
} FXOS8700_SYSMOD_Type;

/** @} */

/**
 * @brief   Initialize an FXOS8700 device
 *
 * @param[out] dev          device descriptor
 * @param[in] i2c           I2C bus the device is connected to
 * @param[in] speed         I2C speed to use
 * @param[in] addr          I2C address of the device
 *
 * The valid address range is 0x1E - 0x1F depending on the configuration of the
 * address pins SA0 and SA1.
 *
 * @return                   0 on success
 * @return                  -1 on error
 * @return                  -2 on invalid address
 */
int fxos8700_init(fxos8700_t* dev, const fxos8700_params_t* params);

int fxos8700_set_active(fxos8700_t* dev);

int fxos8700_set_idle(fxos8700_t* dev);

int fxos8700_read(fxos8700_t* dev);

int fxos8700_read_mag(fxos8700_t* dev, fxos8700_measurement_mag_t* m);

int fxos8700_read_acc(fxos8700_t* dev, fxos8700_measurement_acc_t* m);
#ifdef __cplusplus
}
#endif

/** @} */
#endif /* FXOS8700_H_ */
