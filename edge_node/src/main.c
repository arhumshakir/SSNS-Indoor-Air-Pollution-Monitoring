/*
 * main.c -- Edge node (nRF52840 DK) for the Indoor Air-Pollution Monitor.
 *
 * Flow:
 *   1. Bring up Bluetooth and start non-connectable advertising (beacon).
 *   2. Initialise every sensor (a failing sensor is non-fatal -- its field
 *      simply stays 0, so you can bring the system up one sensor at a time).
 *   3. Every SAMPLE_INTERVAL_S: read sensors, EMA-filter the slow gases,
 *      run the derivative spike check (proposal eq. 2/3), bit-pack the
 *      18-byte payload, refresh the advertisement, and print a JSON line
 *      over the console (RTT / UART) for local debugging.
 *
 * BRING-UP TIP: start with only ENABLE_SCD41 defined to 1 and the rest 0.
 * Get CO2 printing + advertising first, THEN switch the others on one by one.
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <string.h>

#include "payload.h"
#include "ema.h"
#include "scd41.h"
#include "sps30.h"
#include "ccs811.h"
#include "mcp9808.h"
#include "ldr.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ---- Feature switches (set to 0 to disable a sensor during bring-up) ---- */
#define ENABLE_SCD41   1
#define ENABLE_MCP9808 1
#define ENABLE_SPS30   1
#define ENABLE_CCS811  1
#define ENABLE_LDR     1

/* ---- Tunables ---- */
#define SAMPLE_INTERVAL_S 5     /* proposal target is 60 s for power saving */
#define ALPHA_CO2  0.15f        /* proposal: slow gases */
#define ALPHA_PM   0.40f        /* proposal: particulate */
#define GAMMA_CO2  10.0f        /* ppm/s spike threshold (eq. 3) */

/* ---- Advertising data ---- */
static uint8_t mfg[IAQ_PAYLOAD_LEN];

static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg, sizeof(mfg)),
};

static void put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xFF);
	p[1] = (uint8_t)(v >> 8);
}

int main(void)
{
	int rc;
	uint8_t seq = 0;

	ema_t ema_co2, ema_pm, ema_tvoc;
	ema_init(&ema_co2, ALPHA_CO2);
	ema_init(&ema_pm, ALPHA_PM);
	ema_init(&ema_tvoc, ALPHA_CO2);
	float prev_co2 = 0.0f;
	bool have_prev = false;

	LOG_INF("=== IAQ edge node starting ===");

	/* Company ID is constant -- write it once. */
	put_le16(&mfg[0], IAQ_COMPANY_ID);

	rc = bt_enable(NULL);
	if (rc) {
		LOG_ERR("bt_enable failed (%d)", rc);
		return 0;
	}
	rc = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (rc) {
		LOG_ERR("adv start failed (%d)", rc);
		return 0;
	}
	LOG_INF("Advertising as non-connectable beacon");

#if ENABLE_SCD41
	if (scd41_init())   LOG_WRN("SCD41 init failed");
#endif
#if ENABLE_MCP9808
	if (mcp9808_init()) LOG_WRN("MCP9808 init failed");
#endif
#if ENABLE_SPS30
	if (sps30_init())   LOG_WRN("SPS30 init failed");
#endif
#if ENABLE_CCS811
	if (ccs811_init())  LOG_WRN("CCS811 init failed");
#endif
#if ENABLE_LDR
	if (ldr_init())     LOG_WRN("LDR init failed");
#endif

	while (1) {
		k_sleep(K_SECONDS(SAMPLE_INTERVAL_S));

		uint16_t co2 = 0, rh_centi = 0, pm25_deci = 0;
		uint16_t tvoc = 0, eco2 = 0, lux = 0;
		int16_t  temp_centi = 0;

#if ENABLE_SCD41
		scd41_sample_t s;
		if (scd41_data_ready() && scd41_read(&s) == 0 && s.valid) {
			float f = ema_update(&ema_co2, (float)s.co2_ppm);
			co2 = (uint16_t)f;
			rh_centi = (uint16_t)(s.rh_percent * 100.0f);

			/* Spike detection: dy/Ts > gamma  (eqs. 2 & 3). */
			if (have_prev) {
				float dy = (f - prev_co2) / (float)SAMPLE_INTERVAL_S;
				if (dy > GAMMA_CO2) {
					LOG_WRN("CO2 SPIKE  dy=%.1f ppm/s", (double)dy);
				}
			}
			prev_co2 = f;
			have_prev = true;
		}
#endif
#if ENABLE_MCP9808
		mcp9808_sample_t t;
		if (mcp9808_read(&t) == 0 && t.valid) {
			temp_centi = (int16_t)(t.temp_c * 100.0f);
		}
#endif
#if ENABLE_SPS30
		sps30_sample_t p;
		if (sps30_data_ready() && sps30_read(&p) == 0 && p.valid) {
			float f = ema_update(&ema_pm, p.pm2_5);
			pm25_deci = (uint16_t)(f * 10.0f);
		}
#endif
#if ENABLE_CCS811
		ccs811_sample_t c;
		if (ccs811_data_ready() && ccs811_read(&c) == 0 && c.valid) {
			tvoc = (uint16_t)ema_update(&ema_tvoc, (float)c.tvoc_ppb);
			eco2 = c.eco2_ppm;
		}
#endif
#if ENABLE_LDR
		ldr_sample_t l;
		if (ldr_read(&l) == 0 && l.valid) {
			lux = l.lux;
		}
#endif

		/* ---- Pack payload (offsets per payload.h) ---- */
		put_le16(&mfg[2],  co2);
		put_le16(&mfg[4],  (uint16_t)temp_centi);
		put_le16(&mfg[6],  rh_centi);
		put_le16(&mfg[8],  pm25_deci);
		put_le16(&mfg[10], tvoc);
		put_le16(&mfg[12], eco2);
		put_le16(&mfg[14], lux);
		mfg[16] = seq++;
		mfg[17] = iaq_crc8(&mfg[2], 15);

		rc = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
		if (rc) {
			LOG_WRN("adv update failed (%d)", rc);
		}

		/* Local debug line (mirrors the gateway JSON). */
		LOG_INF("{\"co2\":%u,\"temp\":%d.%02d,\"rh\":%u.%02u,"
			"\"pm25\":%u.%u,\"tvoc\":%u,\"eco2\":%u,\"lux\":%u,\"pkt\":%u}",
			co2, temp_centi / 100, (temp_centi % 100 + 100) % 100,
			rh_centi / 100, rh_centi % 100,
			pm25_deci / 10, pm25_deci % 10, tvoc, eco2, lux, mfg[16]);
	}
	return 0;
}
