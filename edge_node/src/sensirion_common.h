/*
 * sensirion_common.h
 * Shared helpers for Sensirion-family I2C sensors (SCD41, SPS30).
 *
 * Sensirion sensors append a CRC-8 to every 2-byte word on the I2C bus.
 *   polynomial = 0x31, init = 0xFF, no reflection, final XOR = 0x00.
 *
 * These helpers are written against the Zephyr <zephyr/drivers/i2c.h> API
 * so they work on any nRF Connect SDK / Zephyr version.
 */
#ifndef SENSIRION_COMMON_H
#define SENSIRION_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/i2c.h>

/* Compute Sensirion CRC-8 over `count` bytes. */
uint8_t sensirion_crc8(const uint8_t *data, uint16_t count);

/*
 * Send a 16-bit command to the sensor (MSB first).
 * Returns 0 on success, negative errno on failure.
 */
int sensirion_write_cmd(const struct i2c_dt_spec *dev, uint16_t cmd);

/*
 * Send a 16-bit command followed by one 16-bit argument plus its CRC.
 */
int sensirion_write_cmd_arg(const struct i2c_dt_spec *dev, uint16_t cmd,
			    uint16_t arg);

/*
 * Send a 16-bit command, wait `delay_ms`, then read `num_words` 16-bit words.
 * Each word is validated against its CRC byte. Decoded words are written to
 * `out` (host byte order). Returns 0 on success, -EIO on CRC mismatch.
 */
int sensirion_read_words(const struct i2c_dt_spec *dev, uint16_t cmd,
			 uint32_t delay_ms, uint16_t *out, uint8_t num_words);

#endif /* SENSIRION_COMMON_H */
