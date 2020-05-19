/*!
 * rng.h - rng for libtorsion
 * Copyright (c) 2020, Christopher Jeffrey (MIT License).
 * https://github.com/bcoin-org/libtorsion
 */

#ifndef _TORSION_RNG_H
#define _TORSION_RNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include "chacha20.h"

/*
 * Symbol Aliases
 */

#define rng_init torsion_rng_init
#define rng_generate torsion_rng_generate
#define rng_random torsion_rng_random
#define rng_uniform torsion_rng_uniform

/*
 * Structs
 */

typedef struct _rng_s {
  chacha20_t chacha;
  uint32_t pool[16];
  size_t pos;
  int rdrand;
  int rdseed;
} rng_t;

/*
 * Entropy
 */

int
torsion_getentropy(void *dst, size_t size);

/*
 * RNG
 */

int
rng_init(rng_t *rng);

void
rng_generate(rng_t *rng, void *dst, size_t size);

uint32_t
rng_random(rng_t *rng);

uint32_t
rng_uniform(rng_t *rng, uint32_t max);

#ifdef __cplusplus
}
#endif

#endif /* _TORSION_RNG_H */
