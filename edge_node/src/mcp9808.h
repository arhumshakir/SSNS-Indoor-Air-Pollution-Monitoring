/*
 * mcp9808.h -- Microchip MCP9808 high-accuracy temperature sensor.
 * Default I2C address 0x18 (A0..A2 low).
 */
#ifndef MCP9808_H
#define MCP9808_H

#include <stdbool.h>

typedef struct {
	float temp_c;
	bool  valid;
} mcp9808_sample_t;

int mcp9808_init(void);
int mcp9808_read(mcp9808_sample_t *out);

#endif /* MCP9808_H */
