/*
 * payload.h -- BLE manufacturer-data payload layout.
 * SHARED between edge_node and gateway: keep both copies identical.
 *
 * All multi-byte fields are LITTLE-ENDIAN. The first two bytes are the BLE
 * "company identifier" (0xFFFF = reserved/for-testing). The gateway uses this
 * ID to recognise our beacons.
 *
 * Offset  Size  Field            Units / encoding
 * ------  ----  ---------------  --------------------------------
 *   0      2    company_id       0xFFFF (LE)
 *   2      2    co2_ppm          u16 ppm                 (SCD41)
 *   4      2    temp_centi       s16, degC * 100         (MCP9808)
 *   6      2    rh_centi         u16, %RH * 100          (SCD41)
 *   8      2    pm25_deci        u16, ug/m3 * 10         (SPS30)
 *  10      2    tvoc_ppb         u16 ppb                 (CCS811)
 *  12      2    eco2_ppm         u16 ppm                 (CCS811)
 *  14      2    lux              u16                     (LDR)
 *  16      1    seq              u8 rolling counter
 *  17      1    crc8             CRC-8 over offsets 2..16
 * ------  ----
 * total   18 bytes
 */
#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <stdint.h>

#define IAQ_COMPANY_ID 0xFFFF
#define IAQ_PAYLOAD_LEN 18

/* CRC-8, poly 0x07, init 0x00 -- simple integrity check for the air link. */
static inline uint8_t iaq_crc8(const uint8_t *d, uint32_t n)
{
	uint8_t crc = 0x00;
	for (uint32_t i = 0; i < n; i++) {
		crc ^= d[i];
		for (int b = 0; b < 8; b++) {
			crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
					   : (uint8_t)(crc << 1);
		}
	}
	return crc;
}

#endif /* PAYLOAD_H */
