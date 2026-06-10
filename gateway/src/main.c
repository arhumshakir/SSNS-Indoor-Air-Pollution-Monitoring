/*
 * main.c -- Gateway (nRF52840 Dongle) for the IAQ monitor.
 *
 * Acts as a BLE Observer: passively scans advertising channels, finds packets
 * whose Manufacturer-Specific Data starts with our company ID (0xFFFF) and the
 * expected length, unpacks the fields and prints ONE JSON object per packet to
 * the host over the dongle's native USB serial port (CDC ACM).
 *
 * The host-side backend/parser.py reads these JSON lines.
 *
 * NOTE: the dongle enumerates its OWN USB serial port -- you do NOT need the
 * FTDI bridge for this path. (The FTDI/Waveshare option is documented in the
 * repo README as an alternative if you instead take UART pins out.)
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include "payload.h"

struct parse_ctx {
	bool     found;
	uint8_t  data[IAQ_PAYLOAD_LEN];
};

static uint16_t get_le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

/* Called once per AD structure inside an advertisement. */
static bool ad_cb(struct bt_data *data, void *user_data)
{
	struct parse_ctx *ctx = user_data;

	if (data->type != BT_DATA_MANUFACTURER_DATA) {
		return true; /* keep walking */
	}
	if (data->data_len != IAQ_PAYLOAD_LEN) {
		return true;
	}
	if (get_le16(data->data) != IAQ_COMPANY_ID) {
		return true;
	}
	memcpy(ctx->data, data->data, IAQ_PAYLOAD_LEN);
	ctx->found = true;
	return false; /* stop -- we have it */
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	struct parse_ctx ctx = { .found = false };
	bt_data_parse(buf, ad_cb, &ctx);
	if (!ctx.found) {
		return;
	}

	const uint8_t *d = ctx.data;

	/* Verify the air-link CRC before trusting the values. */
	if (iaq_crc8(&d[2], 15) != d[17]) {
		printk("{\"error\":\"crc\"}\n");
		return;
	}

	uint16_t co2  = get_le16(&d[2]);
	int16_t  tc   = (int16_t)get_le16(&d[4]);
	uint16_t rh   = get_le16(&d[6]);
	uint16_t pm   = get_le16(&d[8]);
	uint16_t tvoc = get_le16(&d[10]);
	uint16_t eco2 = get_le16(&d[12]);
	uint16_t lux  = get_le16(&d[14]);
	uint8_t  pkt  = d[16];

	printk("{\"co2\":%u,\"temp\":%d.%02d,\"rh\":%u.%02u,"
	       "\"pm25\":%u.%u,\"tvoc\":%u,\"eco2\":%u,\"lux\":%u,"
	       "\"rssi\":%d,\"pkt\":%u}\n",
	       co2, tc / 100, (tc % 100 + 100) % 100,
	       rh / 100, rh % 100, pm / 10, pm % 10,
	       tvoc, eco2, lux, rssi, pkt);
}

int main(void)
{
	int rc;

	/* Bring up native USB so the host sees a serial port. */
	rc = usb_enable(NULL);
	if (rc) {
		/* Continue anyway: RTT logging still works for debugging. */
	}
	k_msleep(1500); /* give the host time to enumerate the port */

	rc = bt_enable(NULL);
	if (rc) {
		printk("bt_enable failed (%d)\n", rc);
		return 0;
	}

	struct bt_le_scan_param scan = {
		.type     = BT_LE_SCAN_TYPE_PASSIVE,
		.options  = BT_LE_SCAN_OPT_NONE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window   = BT_GAP_SCAN_FAST_WINDOW,
	};

	rc = bt_le_scan_start(&scan, scan_cb);
	if (rc) {
		printk("scan start failed (%d)\n", rc);
		return 0;
	}

	printk("{\"status\":\"gateway scanning\"}\n");
	return 0;
}
