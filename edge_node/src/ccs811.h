/*
 * ccs811.h -- AMS CCS811 air-quality sensor (TVOC + eCO2).
 * Default I2C address 0x5A (ADDR pin low). WAKE pin MUST be tied to GND
 * (or driven low) for I2C to respond.
 */
#ifndef CCS811_H
#define CCS811_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint16_t eco2_ppm;  /* equivalent CO2, 400..8192 ppm  */
	uint16_t tvoc_ppb;  /* total VOC, 0..1187 ppb         */
	bool     valid;
} ccs811_sample_t;

int  ccs811_init(void);
bool ccs811_data_ready(void);
int  ccs811_read(ccs811_sample_t *out);

#endif /* CCS811_H */
