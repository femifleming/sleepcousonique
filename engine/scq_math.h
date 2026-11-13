/* Small, fast math for the engine: no libm, so the browser and the SP-1 agree to the bit
 * (as far as float rounding allows) and the Cortex-M4F never calls a slow library. */
#ifndef SCQ_MATH_H
#define SCQ_MATH_H

#include <stdint.h>

#define SCQ_PI 3.14159265f

static inline float scq_abs(float x) { return x < 0.f ? -x : x; }
static inline float scq_min(float a, float b) { return a < b ? a : b; }
static inline float scq_max(float a, float b) { return a > b ? a : b; }
static inline float scq_clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

static inline float scq_floor(float x)
{
	int32_t i = (int32_t)x;
	return (float)(i - (x < (float)i));
}

/* sin(2*pi*p) for any p. Parabola plus one correction step: error about 0.001. */
static inline float scq_sin2pi(float p)
{
	p -= scq_floor(p);
	float x = p - 0.5f;                      /* sin(2pi p) = -sin(2pi x) */
	float y = 8.f * x - 16.f * x * scq_abs(x);
	y = 0.225f * (y * scq_abs(y) - y) + y;
	return -y;
}

/* 2^x */
static inline float scq_exp2(float x)
{
	if (x < -60.f) return 0.f;
	if (x > 60.f) x = 60.f;
	float fl = scq_floor(x);
	float f = x - fl;
	float p = 1.f + f * (0.6960656f + f * (0.2244943f + f * 0.0794402f));
	union { float f; int32_t i; } u;
	u.f = p;
	u.i += ((int32_t)fl) << 23;
	return u.f;
}

/* log2(x) for x > 0 */
static inline float scq_log2(float x)
{
	union { float f; int32_t i; } u;
	if (x <= 0.f) return -60.f;
	u.f = x;
	float e = (float)(((u.i >> 23) & 255) - 128);
	u.i = (u.i & ~(255 << 23)) | (127 << 23);   /* mantissa in [1,2) */
	float m = u.f;
	return e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f + 1.f;
}

/* soft clip, exact at 0 and smooth to +-1 */
static inline float scq_tanh(float x)
{
	if (x > 3.f) return 1.f;
	if (x < -3.f) return -1.f;
	float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}

/* a gentle soft clip with no division: cubic, flat beyond +-1.5 (the output stage) */
static inline float scq_softclip(float x)
{
	x = scq_clamp(x, -1.5f, 1.5f);
	return x - 0.148148f * x * x * x;
}

/* tan(x) for 0 <= x < 1.4 (filter warping) */
static inline float scq_tan(float x)
{
	float x2 = x * x;
	return x * (1.f + x2 * (0.3333333f + x2 * (0.1333333f + x2 * (0.0539683f + x2 * 0.0218695f))));
}

/* xorshift: random in [0,1) and [-1,1) */
static inline uint32_t scq_rng(uint32_t *s)
{
	uint32_t x = *s;
	x ^= x << 13; x ^= x >> 17; x ^= x << 5;
	return *s = x;
}
static inline float scq_rand01(uint32_t *s) { return (float)(scq_rng(s) >> 8) * (1.f / 16777216.f); }
static inline float scq_noise(uint32_t *s) { return scq_rand01(s) * 2.f - 1.f; }

/* one-pole coefficient reaching ~63% in `seconds` */
static inline float scq_coef_time(float seconds, float rate)
{
	if (seconds <= 0.f) return 1.f;
	float n = seconds * rate;
	return 1.f - scq_exp2(-1.442695f / n);
}

/* one-pole lowpass coefficient for a cutoff in Hz */
static inline float scq_coef_hz(float hz, float rate)
{
	float w = 2.f * SCQ_PI * hz / rate;
	if (w > 1.5f) w = 1.5f;
	return w / (1.f + w);
}

#endif
