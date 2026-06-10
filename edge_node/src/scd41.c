/*
 * scd41.c -- Sensirion SCD41 driver (raw I2C).
 *
 * Command reference (from SCD4x datasheet):
 *   0x21B1  start_periodic_measurement   (new sample every 5 s)
 *   0xE4B8  get_data_ready_status
 *   0xEC05  read_measurement             (CO2, T, RH, each word + CRC)
 *   0x3F86  stop_periodic_measurement
 *   0x3646  reinit
 *
 * Conversion (datasheet):
 *   CO2 [ppm] = word0
 *   T  [degC] = -45 + 175 * word1 / 65535
 *   RH [%]    =       100 * word2 / 65535
 */
#include "scd41.h"
#include "sensirion_common.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(scd41, LOG_LEVEL_INF);

#define SCD41_ADDR 0x62

/* i2c0 is the I2C controller enabled in the board overlay. */
static const struct i2c_dt_spec scd41 = {
	.bus  = DEVICE_DT_GET(DT_NODELABEL(i2c0)),
	.addr = SCD41_ADDR,
};

int scd41_init(void)
{
	if (!device_is_ready(scd41.bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	/* Make sure we are not already measuring, then (re)start. */
	(void)sensirion_write_cmd(&scd41, 0x3F86); /* stop_periodic_measurement */
	k_msleep(500);
	(void)sensirion_write_cmd(&scd41, 0x3646); /* reinit */
	k_msleep(30);

	int rc = sensirion_write_cmd(&scd41, 0x21B1); /* start_periodic_measurement */
	if (rc) {
		LOG_ERR("start_periodic_measurement failed (%d)", rc);
		return rc;
	}
	LOG_INF("SCD41 measuring (first sample ~5 s)");
	return 0;
}

bool scd41_data_ready(void)
{
	uint16_t status;
	int rc = sensirion_read_words(&scd41, 0xE4B8, 1, &status, 1);
	if (rc) {
		return false;
	}
	/* Bits 0..10 == 0 means "not ready". */
	return (status & 0x07FF) != 0;
}

int scd41_read(scd41_sample_t *out)
{
	uint16_t w[3];
	int rc = sensirion_read_words(&scd41, 0xEC05, 1, w, 3);
	if (rc) {
		out->valid = false;
		return rc;
	}
	out->co2_ppm    = w[0];
	out->temp_c     = -45.0f + 175.0f * (float)w[1] / 65535.0f;
	out->rh_percent =          100.0f * (float)w[2] / 65535.0f;
	out->valid      = true;
	return 0;
}
