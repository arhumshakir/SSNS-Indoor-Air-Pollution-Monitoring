/*
 * ccs811.c -- AMS CCS811 driver (raw I2C).
 *
 * Register map:
 *   0x00 STATUS        (bit7 FW_MODE, bit4 APP_VALID, bit3 DATA_READY)
 *   0x01 MEAS_MODE     write drive mode
 *   0x02 ALG_RESULT_DATA  (eCO2 hi/lo, TVOC hi/lo, status, error, ...)
 *   0x20 HW_ID         should read 0x81
 *   0xF4 APP_START     (no data) -- switch from boot to application mode
 *
 * Notes:
 *  - First valid readings appear after ~20 min of run-in conditioning.
 *    Until then the numbers are coarse; that is normal for this part.
 */
#include "ccs811.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ccs811, LOG_LEVEL_INF);

#define CCS811_ADDR 0x5A

#define REG_STATUS      0x00
#define REG_MEAS_MODE   0x01
#define REG_ALG_RESULT  0x02
#define REG_HW_ID       0x20
#define REG_APP_START   0xF4

#define DRIVE_MODE_1SEC 0x10 /* MEAS_MODE: measurement every 1 s, INT off */

static const struct i2c_dt_spec ccs811 = {
	.bus  = DEVICE_DT_GET(DT_NODELABEL(i2c0)),
	.addr = CCS811_ADDR,
};

static int read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
	return i2c_write_read_dt(&ccs811, &reg, 1, buf, len);
}

int ccs811_init(void)
{
	if (!device_is_ready(ccs811.bus)) {
		return -ENODEV;
	}

	uint8_t hw_id = 0;
	int rc = read_reg(REG_HW_ID, &hw_id, 1);
	if (rc || hw_id != 0x81) {
		LOG_ERR("HW_ID = 0x%02x (expected 0x81), rc=%d", hw_id, rc);
		return -ENODEV;
	}

	/* Switch from boot mode to application mode (APP_START, no payload). */
	uint8_t app_start = REG_APP_START;
	rc = i2c_write_dt(&ccs811, &app_start, 1);
	if (rc) {
		return rc;
	}
	k_msleep(20);

	/* Configure continuous 1 s measurement mode. */
	uint8_t meas[2] = { REG_MEAS_MODE, DRIVE_MODE_1SEC };
	rc = i2c_write_dt(&ccs811, meas, sizeof(meas));
	if (rc) {
		return rc;
	}
	LOG_INF("CCS811 in app mode (allow ~20 min run-in for accuracy)");
	return 0;
}

bool ccs811_data_ready(void)
{
	uint8_t status = 0;
	if (read_reg(REG_STATUS, &status, 1)) {
		return false;
	}
	return (status & 0x08) != 0; /* DATA_READY */
}

int ccs811_read(ccs811_sample_t *out)
{
	uint8_t d[4];
	out->valid = false;

	int rc = read_reg(REG_ALG_RESULT, d, sizeof(d));
	if (rc) {
		return rc;
	}
	out->eco2_ppm = (uint16_t)((d[0] << 8) | d[1]);
	out->tvoc_ppb = (uint16_t)((d[2] << 8) | d[3]);
	out->valid    = true;
	return 0;
}
