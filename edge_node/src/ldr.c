/*
 * ldr.c -- LDR read via Zephyr ADC (SAADC) using a devicetree io-channel.
 *
 * The overlay defines:
 *   zephyr,user { io-channels = <&adc 0>; };
 *   &adc { channel@0 { reg=<0>; zephyr,gain="ADC_GAIN_1_6";
 *          zephyr,reference="ADC_REF_INTERNAL"; zephyr,acquisition-time=...;
 *          zephyr,input-positive=<NRF_SAADC_AIN0>; }; };
 *
 * Lux estimate (rough): with the LDR on the top of the divider and Rfixed
 * to GND, brighter light -> lower R_LDR -> higher midpoint voltage.
 * Calibrate LDR_K / LDR_BETA against a reference meter for real lux.
 */
#include "ldr.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <math.h>

LOG_MODULE_REGISTER(ldr, LOG_LEVEL_INF);

#define VDD_MV       3300.0f
#define R_FIXED_OHMS 10000.0f   /* divider resistor to GND */
#define LDR_K        500000.0f  /* empirical -- CALIBRATE */
#define LDR_BETA     1.4f       /* empirical -- CALIBRATE */

static const struct adc_dt_spec adc_ch =
	ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

static int16_t sample_buf;
static struct adc_sequence seq = {
	.buffer      = &sample_buf,
	.buffer_size = sizeof(sample_buf),
};

int ldr_init(void)
{
	if (!adc_is_ready_dt(&adc_ch)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}
	int rc = adc_channel_setup_dt(&adc_ch);
	if (rc) {
		LOG_ERR("adc_channel_setup failed (%d)", rc);
		return rc;
	}
	(void)adc_sequence_init_dt(&adc_ch, &seq);
	return 0;
}

int ldr_read(ldr_sample_t *out)
{
	out->valid = false;

	int rc = adc_read_dt(&adc_ch, &seq);
	if (rc) {
		return rc;
	}

	int32_t mv = sample_buf;
	rc = adc_raw_to_millivolts_dt(&adc_ch, &mv);
	if (rc) {
		return rc;
	}
	if (mv < 1) {
		mv = 1;
	}
	out->millivolts = (uint16_t)mv;

	/* R_LDR from the divider, then lux = K * R_LDR^-beta. */
	float v = (float)mv;
	float r_ldr = R_FIXED_OHMS * (VDD_MV / v - 1.0f);
	if (r_ldr < 1.0f) {
		r_ldr = 1.0f;
	}
	float lux = LDR_K * powf(r_ldr, -LDR_BETA);
	if (lux > 65535.0f) {
		lux = 65535.0f;
	}
	out->lux   = (uint16_t)lux;
	out->valid = true;
	return 0;
}
