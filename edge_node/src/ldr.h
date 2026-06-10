/*
 * ldr.h -- Iduino LDR light sensor read via nRF52840 SAADC.
 * The LDR forms a divider with a fixed pull-up; the divider midpoint is wired
 * to analog input AIN0 (P0.02 on the nRF52840 DK). See the board overlay.
 */
#ifndef LDR_H
#define LDR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint16_t millivolts; /* measured divider voltage */
	uint16_t lux;        /* estimated illuminance (calibrate constants) */
	bool     valid;
} ldr_sample_t;

int ldr_init(void);
int ldr_read(ldr_sample_t *out);

#endif /* LDR_H */
