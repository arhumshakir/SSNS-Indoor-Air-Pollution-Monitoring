/*
 * mcp9808.c -- Microchip MCP9808 driver (raw I2C).
 *
 * Ambient temperature register 0x05 returns 2 bytes:
 *   bits 12..0 = temperature in 1/16 degC, bit 12 = sign.
 */
#include "mcp9808.h"
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mcp9808, LOG_LEVEL_INF);

#define MCP9808_ADDR     0x18
#define REG_AMBIENT_TEMP 0x05

static const struct i2c_dt_spec mcp = {
	.bus  = DEVICE_DT_GET(DT_NODELABEL(i2c0)),
	.addr = MCP9808_ADDR,
};

int mcp9808_init(void)
{
	if (!device_is_ready(mcp.bus)) {
		return -ENODEV;
	}
	return 0;
}

int mcp9808_read(mcp9808_sample_t *out)
{
	uint8_t reg = REG_AMBIENT_TEMP;
	uint8_t d[2];
	out->valid = false;

	int rc = i2c_write_read_dt(&mcp, &reg, 1, d, sizeof(d));
	if (rc) {
		return rc;
	}

	uint16_t raw = (uint16_t)((d[0] << 8) | d[1]);
	raw &= 0x1FFF;          /* keep 13 data bits */
	float t = (float)raw / 16.0f;
	if (d[0] & 0x10) {      /* sign bit */
		t -= 256.0f;
	}
	out->temp_c = t;
	out->valid  = true;
	return 0;
}
