/*
 * sps30.h -- Sensirion SPS30 particulate matter sensor (I2C mode).
 * I2C address 0x69. MUST be powered from 5V (MB102 rail); its I2C lines are
 * 3.3V-logic compatible, so SDA/SCL go straight to the nRF52840.
 */
#ifndef SPS30_H
#define SPS30_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	float pm1_0;
	float pm2_5;
	float pm4_0;
	float pm10;
	bool  valid;
} sps30_sample_t;

int  sps30_init(void);          /* probe + start measurement */
bool sps30_data_ready(void);
int  sps30_read(sps30_sample_t *out);

#endif /* SPS30_H */
