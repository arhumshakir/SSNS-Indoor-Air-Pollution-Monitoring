/*
 * ema.h -- Exponential Moving Average low-pass filter.
 * Implements proposal eq. (1):  y[n] = a*x[n] + (1-a)*y[n-1]
 */
#ifndef EMA_H
#define EMA_H

#include <stdbool.h>

typedef struct {
	float alpha;
	float y;
	bool  initialised;
} ema_t;

static inline void ema_init(ema_t *f, float alpha)
{
	f->alpha = alpha;
	f->y = 0.0f;
	f->initialised = false;
}

static inline float ema_update(ema_t *f, float x)
{
	if (!f->initialised) {
		f->y = x;            /* seed with first sample */
		f->initialised = true;
	} else {
		f->y = f->alpha * x + (1.0f - f->alpha) * f->y;
	}
	return f->y;
}

#endif /* EMA_H */
