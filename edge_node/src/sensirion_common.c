/*
 * sensirion_common.c  -- see header for description.
 */
#include "sensirion_common.h"
#include <zephyr/kernel.h>

uint8_t sensirion_crc8(const uint8_t *data, uint16_t count)
{
	uint8_t crc = 0xFF;
	for (uint16_t i = 0; i < count; i++) {
		crc ^= data[i];
		for (uint8_t bit = 0; bit < 8; bit++) {
			if (crc & 0x80) {
				crc = (uint8_t)((crc << 1) ^ 0x31);
			} else {
				crc = (uint8_t)(crc << 1);
			}
		}
	}
	return crc;
}

int sensirion_write_cmd(const struct i2c_dt_spec *dev, uint16_t cmd)
{
	uint8_t buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };
	return i2c_write_dt(dev, buf, sizeof(buf));
}

int sensirion_write_cmd_arg(const struct i2c_dt_spec *dev, uint16_t cmd,
			    uint16_t arg)
{
	uint8_t buf[5];
	buf[0] = (uint8_t)(cmd >> 8);
	buf[1] = (uint8_t)(cmd & 0xFF);
	buf[2] = (uint8_t)(arg >> 8);
	buf[3] = (uint8_t)(arg & 0xFF);
	buf[4] = sensirion_crc8(&buf[2], 2);
	return i2c_write_dt(dev, buf, sizeof(buf));
}

int sensirion_read_words(const struct i2c_dt_spec *dev, uint16_t cmd,
			 uint32_t delay_ms, uint16_t *out, uint8_t num_words)
{
	int rc;
	uint8_t cmd_buf[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };

	rc = i2c_write_dt(dev, cmd_buf, sizeof(cmd_buf));
	if (rc) {
		return rc;
	}

	if (delay_ms) {
		k_msleep(delay_ms);
	}

	/* Each word on the wire is 3 bytes: MSB, LSB, CRC. */
	uint8_t rx[3 * 32];
	if (num_words > 32) {
		return -EINVAL;
	}
	rc = i2c_read_dt(dev, rx, (size_t)num_words * 3);
	if (rc) {
		return rc;
	}

	for (uint8_t i = 0; i < num_words; i++) {
		uint8_t *w = &rx[i * 3];
		if (sensirion_crc8(w, 2) != w[2]) {
			return -EIO; /* CRC mismatch */
		}
		out[i] = (uint16_t)((w[0] << 8) | w[1]);
	}
	return 0;
}
