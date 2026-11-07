#ifndef FW_CONFIG_H
#define FW_CONFIG_H

#define FW_BNO_I2C_HZ 100000u
#define FW_I2C_PROBE_TRIALS 1u
#define FW_IO_TIMEOUT_MS 100u

/* Use the BNO055's reset default oscillator. Enabling a crystal explicitly
   falls back to the internal oscillator if CLK_SEL cannot be verified. */
#define BNO_INTERNAL_USE_CRYSTAL 0u
#define BNO_EXTERNAL_USE_CRYSTAL 0u

/* Bosch P1: X->X, Y->Y, Z->Z. Change an individual mapping only when the
   sensor package axes are different relative to the enclosure. */
#define BNO_INTERNAL_AXIS_MAP  0x24u
#define BNO_INTERNAL_AXIS_SIGN 0x00u
#define BNO_EXTERNAL_AXIS_MAP  0x24u
#define BNO_EXTERNAL_AXIS_SIGN 0x00u

/* Trial directions towards the foot, in the remapped sensor axes.
   1 = X, 2 = Y, 3 = Z; the sign gives the direction along the axis. */
#define BNO_INTERNAL_LEG_AXIS (-3)
#define BNO_EXTERNAL_LEG_AXIS (-3)

#define BNO_SAMPLE_PERIOD_MS 100u

#endif
