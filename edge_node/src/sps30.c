/*
 * sps30.c -- Sensirion SPS30 driver (raw I2C, IEEE-754 float output mode).
 *
 * Command reference (SPS30 I2C datasheet):
 *   0x0010 start_measurement   arg 0x0300 => big-endian float output
 *   0x0104 stop_measurement
 *   0x0202 read_data_ready_flag        (1 word)
 *   0x0300 read_measured_values        (10 floats, 60 bytes, float mode)
 *   0xD033 device_reset
 *
 * In float mode a value occupies 6 bytes on the wire:
 *   [word_hi_MSB][word_hi_LSB][CRC][word_lo_MSB][word_lo_LSB][CRC]
 * The float (big-endian IEEE754) is bytes: hi_MSB hi_LSB lo_MSB lo_LSB.
 * The first four values are PM1.0, PM2.5, PM4.0, PM10 mass conc. (ug/m3).
 */
#include "sps30.h"
#include "sensirion_common.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sps30, LOG_LEVEL_INF);

#define SPS30_ADDR 0x69

static const struct i2c_dt_spec sps30 = {
	.bus  = DEVICE_DT_GET(DT_NODELABEL(i2c0)),
	.addr = SPS30_ADDR,
};

int sps30_init(void)
{
	if (!device_is_ready(sps30.bus)) {
		return -ENODEV;
	}

	(void)sensirion_write_cmd(&sps30, 0xD033); /* reset */
	k_msleep(100);

	/* start_measurement, float output (0x03), dummy 0x00 */
	int rc = sensirion_write_cmd_arg(&sps30, 0x0010, 0x0300);
	if (rc) {
		LOG_ERR("start_measurement failed (%d)", rc);
		return rc;
	}
	/* Fan spin-up: data becomes stable after a few seconds. */
	k_msleep(1000);
	LOG_INF("SPS30 measuring");
	return 0;
}

bool sps30_data_ready(void)
{
	uint16_t flag;
	int rc = sensirion_read_words(&sps30, 0x0202, 5, &flag, 1);
	if (rc) {
		return false;
	}
	return (flag & 0x0001) != 0;
}

static float be_bytes_to_float(const uint8_t b[4])
{
	uint32_t u = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
		     ((uint32_t)b[2] << 8) | (uint32_t)b[3];
	float f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

int sps30_read(sps30_sample_t *out)
{
	uint8_t cmd[2] = { 0x03, 0x00 };
	uint8_t rx[60];

	out->valid = false;

	int rc = i2c_write_dt(&sps30, cmd, sizeof(cmd));
	if (rc) {
		return rc;
	}
	k_msleep(5);
	rc = i2c_read_dt(&sps30, rx, sizeof(rx));
	if (rc) {
		return rc;
	}

	/* Verify CRC on every 2-byte word (20 words total). */
	for (int i = 0; i < 60; i += 3) {
		if (sensirion_crc8(&rx[i], 2) != rx[i + 2]) {
			LOG_WRN("SPS30 CRC error at byte %d", i);
			return -EIO;
		}
	}

	/* Reconstruct float i from 6-byte block starting at 6*i. */
	float vals[4];
	for (int i = 0; i < 4; i++) {
		const uint8_t *p = &rx[i * 6];
		uint8_t fb[4] = { p[0], p[1], p[3], p[4] };
		vals[i] = be_bytes_to_float(fb);
	}
	out->pm1_0 = vals[0];
	out->pm2_5 = vals[1];
	out->pm4_0 = vals[2];
	out->pm10  = vals[3];
	out->valid = true;
	return 0;
}
