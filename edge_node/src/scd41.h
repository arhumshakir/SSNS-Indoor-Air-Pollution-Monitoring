/*
 * scd41.h -- Sensirion SCD41 CO2 / temperature / humidity sensor.
 * I2C address 0x62. Raw-I2C driver (no devicetree binding required).
 */
#ifndef SCD41_H
#define SCD41_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint16_t co2_ppm;    /* parts per million */
	float    temp_c;     /* degrees Celsius   */
	float    rh_percent; /* %RH               */
	bool     valid;
} scd41_sample_t;

/* Initialise the bus spec and start periodic measurement. */
int scd41_init(void);

/* True once at least 5 s have passed and new data is ready. */
bool scd41_data_ready(void);

/* Read one sample. Call only after scd41_data_ready() returns true. */
int scd41_read(scd41_sample_t *out);

#endif /* SCD41_H */
