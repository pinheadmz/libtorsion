#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#define EC_TEST
#include "ec.c"

static const wei_def_t *wei_curves[5] = {
  &curve_p224,
  &curve_p256,
  &curve_p384,
  &curve_p521,
  &curve_secp256k1
};

static const edwards_def_t *edwards_curves[1] = {
  &curve_ed25519
};

int RAND_status(void);
int RAND_poll();
int RAND_bytes(unsigned char *buf, int num);

static int
random_bytes(void *dst, size_t len) {
  memset(dst, 0x00, len);

  if (len > (size_t)INT_MAX)
    return 0;

  for (;;) {
    int status = RAND_status();

    assert(status >= 0);

    if (status != 0)
      break;

    if (RAND_poll() == 0)
      break;
  }

  return RAND_bytes(dst, (int)len) == 1;
}

static unsigned int
random_int(unsigned int mod) {
  unsigned int x;

  if (mod == 0)
    return 0;

  assert(random_bytes(&x, sizeof(x)));

  return x % mod;
}

static void
print_hex(const unsigned char *data, size_t len) {
  char str[512 + 1];
  size_t i;

  assert(len <= 256);

  for (i = 0; i < len; i++) {
    char hi = data[i] >> 4;
    char lo = data[i] & 0x0f;

    if (hi < 10)
      hi += '0';
    else
      hi += 'a' - 10;

    if (lo < 10)
      lo += '0';
    else
      lo += 'a' - 10;

    str[i * 2 + 0] = hi;
    str[i * 2 + 1] = lo;
  }

  str[i * 2] = '\0';

  printf("%s\n", str);
}

static void
test_sc(void) {
  wei_t curve;
  wei_t *ec = &curve;
  scalar_field_t *sc = &ec->sc;
  mp_limb_t r[MAX_SCALAR_LIMBS];
  mp_limb_t t[MAX_SCALAR_LIMBS * 4];
  unsigned char raw[MAX_SCALAR_SIZE];

  printf("Scalar sanity check.\n");

  wei_init(ec, &curve_p256);

  memcpy(raw, sc->raw, sc->size);

  mpn_zero(r, ARRAY_SIZE(r));
  mpn_zero(t, ARRAY_SIZE(t));

  mpn_copyi(t, sc->n, sc->limbs);

  sc_reduce(sc, r, t);

  assert(sc_is_zero(sc, r));

  raw[sc->size - 1] -= 1;
  assert(sc_import(sc, r, raw));

  raw[sc->size - 1] += 1;
  assert(!sc_import(sc, r, raw));

  raw[sc->size - 1] += 1;
  assert(!sc_import(sc, r, raw));
}

static void
test_fe(void) {
  wei_t curve;
  wei_t *ec = &curve;
  prime_field_t *fe = &ec->fe;
  fe_t t;
  unsigned char raw[MAX_FIELD_SIZE];

  printf("Field element sanity check.\n");

  wei_init(ec, &curve_p256);

  memcpy(raw, fe->raw, fe->size);

  raw[fe->size - 1] -= 1;
  assert(fe_import(fe, t, raw));

  raw[fe->size - 1] += 1;
  assert(!fe_import(fe, t, raw));

  raw[7] += 1;
  assert(!fe_import(fe, t, raw));
}


static void
test_wei_points_p256(void) {
  const unsigned char g_raw[33] = {
    0x03, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42,
    0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40,
    0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33,
    0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2,
    0x96
  };

  const unsigned char g2_raw[33] = {
    0x03, 0x7c, 0xf2, 0x7b, 0x18, 0x8d, 0x03, 0x4f,
    0x7e, 0x8a, 0x52, 0x38, 0x03, 0x04, 0xb5, 0x1a,
    0xc3, 0xc0, 0x89, 0x69, 0xe2, 0x77, 0xf2, 0x1b,
    0x35, 0xa6, 0x0b, 0x48, 0xfc, 0x47, 0x66, 0x99,
    0x78
  };

  const unsigned char g3_raw[33] = {
    0x02, 0x5e, 0xcb, 0xe4, 0xd1, 0xa6, 0x33, 0x0a,
    0x44, 0xc8, 0xf7, 0xef, 0x95, 0x1d, 0x4b, 0xf1,
    0x65, 0xe6, 0xc6, 0xb7, 0x21, 0xef, 0xad, 0xa9,
    0x85, 0xfb, 0x41, 0x66, 0x1b, 0xc6, 0xe7, 0xfd,
    0x6c
  };

  wei_t curve;
  wei_t *ec = &curve;
  wge_t g, p, q, r;
  jge_t jg, jp, jq, jr;
  unsigned char entropy[32];
  unsigned char p_raw[33];
  size_t p_size;

  printf("Testing Weierstrass group law (P256).\n");

  wei_init(ec, &curve_p256);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  wge_set(ec, &g, &ec->g);
  wge_to_jge(ec, &jg, &ec->g);

  assert(wge_import(ec, &p, g_raw, 33));

  wge_to_jge(ec, &jp, &p);
  wge_to_jge(ec, &jq, &ec->g);

  assert(wge_validate(ec, &p));
  assert(jge_validate(ec, &jp));
  assert(jge_validate(ec, &jq));
  assert(wge_equal(ec, &p, &ec->g));
  assert(jge_equal(ec, &jp, &jq));

  assert(wge_import(ec, &q, g2_raw, 33));
  assert(wge_import(ec, &r, g3_raw, 33));

  wge_to_jge(ec, &jq, &q);
  wge_to_jge(ec, &jr, &r);

  wge_dbl(ec, &p, &ec->g);

  assert(wge_equal(ec, &p, &q));

  wge_add(ec, &p, &p, &ec->g);

  assert(wge_equal(ec, &p, &r));

  jge_dbl(ec, &jp, &jg);

  assert(jge_equal(ec, &jp, &jq));

  jge_add(ec, &jp, &jp, &jg);

  assert(jge_equal(ec, &jp, &jr));

  jge_sub(ec, &jp, &jp, &jg);

  assert(jge_equal(ec, &jp, &jq));

  jge_mixed_add(ec, &jp, &jp, &g);

  assert(jge_equal(ec, &jp, &jr));

  jge_mixed_sub(ec, &jp, &jp, &g);

  assert(jge_equal(ec, &jp, &jq));

  assert(jge_validate(ec, &jg));
  assert(jge_validate(ec, &jp));
  assert(jge_validate(ec, &jq));
  assert(jge_validate(ec, &jr));

  assert(!jge_is_zero(ec, &jg));
  assert(!jge_is_zero(ec, &jp));
  assert(!jge_is_zero(ec, &jq));
  assert(!jge_is_zero(ec, &jr));

  jge_to_wge(ec, &p, &jp);

  assert(wge_equal(ec, &p, &q));

  assert(wge_export(ec, p_raw, &p_size, &p, 1));
  assert(p_size == 33);

  assert(memcmp(p_raw, g2_raw, 33) == 0);
}

static void
test_wei_points_p521(void) {
  wei_t curve;
  wei_t *ec = &curve;
  wge_t g, p, q, r;
  jge_t jg, jp, jq, jr;
  unsigned char entropy[66];
  unsigned char p_raw[67];
  size_t p_size;

  const unsigned char g_raw[67] = {
    0x02, 0x00, 0xc6, 0x85, 0x8e, 0x06, 0xb7, 0x04,
    0x04, 0xe9, 0xcd, 0x9e, 0x3e, 0xcb, 0x66, 0x23,
    0x95, 0xb4, 0x42, 0x9c, 0x64, 0x81, 0x39, 0x05,
    0x3f, 0xb5, 0x21, 0xf8, 0x28, 0xaf, 0x60, 0x6b,
    0x4d, 0x3d, 0xba, 0xa1, 0x4b, 0x5e, 0x77, 0xef,
    0xe7, 0x59, 0x28, 0xfe, 0x1d, 0xc1, 0x27, 0xa2,
    0xff, 0xa8, 0xde, 0x33, 0x48, 0xb3, 0xc1, 0x85,
    0x6a, 0x42, 0x9b, 0xf9, 0x7e, 0x7e, 0x31, 0xc2,
    0xe5, 0xbd, 0x66
  };

  const unsigned char g2_raw[67] = {
    0x02, 0x00, 0x43, 0x3c, 0x21, 0x90, 0x24, 0x27,
    0x7e, 0x7e, 0x68, 0x2f, 0xcb, 0x28, 0x81, 0x48,
    0xc2, 0x82, 0x74, 0x74, 0x03, 0x27, 0x9b, 0x1c,
    0xcc, 0x06, 0x35, 0x2c, 0x6e, 0x55, 0x05, 0xd7,
    0x69, 0xbe, 0x97, 0xb3, 0xb2, 0x04, 0xda, 0x6e,
    0xf5, 0x55, 0x07, 0xaa, 0x10, 0x4a, 0x3a, 0x35,
    0xc5, 0xaf, 0x41, 0xcf, 0x2f, 0xa3, 0x64, 0xd6,
    0x0f, 0xd9, 0x67, 0xf4, 0x3e, 0x39, 0x33, 0xba,
    0x6d, 0x78, 0x3d
  };

  const unsigned char g3_raw[67] = {
    0x03, 0x01, 0xa7, 0x3d, 0x35, 0x24, 0x43, 0xde,
    0x29, 0x19, 0x5d, 0xd9, 0x1d, 0x6a, 0x64, 0xb5,
    0x95, 0x94, 0x79, 0xb5, 0x2a, 0x6e, 0x5b, 0x12,
    0x3d, 0x9a, 0xb9, 0xe5, 0xad, 0x7a, 0x11, 0x2d,
    0x7a, 0x8d, 0xd1, 0xad, 0x3f, 0x16, 0x4a, 0x3a,
    0x48, 0x32, 0x05, 0x1d, 0xa6, 0xbd, 0x16, 0xb5,
    0x9f, 0xe2, 0x1b, 0xae, 0xb4, 0x90, 0x86, 0x2c,
    0x32, 0xea, 0x05, 0xa5, 0x91, 0x9d, 0x2e, 0xde,
    0x37, 0xad, 0x7d
  };

  printf("Testing Weierstrass group law (P521).\n");

  wei_init(ec, &curve_p521);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  wge_set(ec, &g, &ec->g);
  wge_to_jge(ec, &jg, &ec->g);

  assert(wge_import(ec, &p, g_raw, 67));

  wge_to_jge(ec, &jp, &p);
  wge_to_jge(ec, &jq, &ec->g);

  assert(wge_validate(ec, &p));
  assert(jge_validate(ec, &jp));
  assert(jge_validate(ec, &jq));
  assert(wge_equal(ec, &p, &ec->g));
  assert(jge_equal(ec, &jp, &jq));

  assert(wge_import(ec, &q, g2_raw, 67));
  assert(wge_import(ec, &r, g3_raw, 67));

  wge_to_jge(ec, &jq, &q);
  wge_to_jge(ec, &jr, &r);

  wge_dbl(ec, &p, &ec->g);

  assert(wge_equal(ec, &p, &q));

  wge_add(ec, &p, &p, &ec->g);

  assert(wge_equal(ec, &p, &r));

  jge_dbl(ec, &jp, &jg);

  assert(jge_equal(ec, &jp, &jq));

  jge_add(ec, &jp, &jp, &jg);

  assert(jge_equal(ec, &jp, &jr));

  jge_sub(ec, &jp, &jp, &jg);

  assert(jge_equal(ec, &jp, &jq));

  jge_mixed_add(ec, &jp, &jp, &g);

  assert(jge_equal(ec, &jp, &jr));

  jge_mixed_sub(ec, &jp, &jp, &g);

  assert(jge_equal(ec, &jp, &jq));

  assert(jge_validate(ec, &jg));
  assert(jge_validate(ec, &jp));
  assert(jge_validate(ec, &jq));
  assert(jge_validate(ec, &jr));

  assert(!jge_is_zero(ec, &jg));
  assert(!jge_is_zero(ec, &jp));
  assert(!jge_is_zero(ec, &jq));
  assert(!jge_is_zero(ec, &jr));

  jge_to_wge(ec, &p, &jp);

  assert(wge_equal(ec, &p, &q));

  assert(wge_export(ec, p_raw, &p_size, &p, 1));
  assert(p_size == 67);

  assert(memcmp(p_raw, g2_raw, 67) == 0);
}

static void
test_wei_mul_g(void) {
  const unsigned char k_raw[32] = {
    0x38, 0xf8, 0x62, 0x0b, 0xa6, 0x0b, 0xed, 0x7c,
    0xf9, 0x0c, 0x7a, 0x99, 0xac, 0x35, 0xa4, 0x4e,
    0x39, 0x27, 0x59, 0x8e, 0x3c, 0x99, 0xbb, 0xc5,
    0xf5, 0x70, 0x75, 0x13, 0xc4, 0x0e, 0x2c, 0xe3
  };

  const unsigned char expect_raw[33] = {
    0x02, 0x1a, 0xb3, 0x49, 0x34, 0xb8, 0x11, 0xb5,
    0x5e, 0x2f, 0xa4, 0xf1, 0xcd, 0x57, 0xf1, 0x68,
    0x51, 0x3d, 0x04, 0xb9, 0x45, 0xb0, 0x43, 0xec,
    0xe9, 0x6b, 0x25, 0x53, 0x96, 0x72, 0xff, 0x52,
    0x03
  };

  wei_t curve;
  wei_t *ec = &curve;
  sc_t k;
  wge_t q, expect;
  unsigned char entropy[32];
  unsigned char q_raw[33];
  size_t q_size;
  scalar_field_t *sc = &ec->sc;

  printf("Testing mul_g (vector).\n");

  wei_init(ec, &curve_p256);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(sc_import(sc, k, k_raw));
  assert(wge_import(ec, &expect, expect_raw, 33));

  assert(wge_validate(ec, &expect));
  assert(wge_equal(ec, &expect, &expect));
  assert(!wge_equal(ec, &expect, &ec->g));

  wei_mul_g(ec, &q, k);

  assert(wge_equal(ec, &q, &expect));

  assert(wge_export(ec, q_raw, &q_size, &q, 1));
  assert(q_size == 33);

  assert(memcmp(q_raw, expect_raw, 33) == 0);

  wei_mul_g_var(ec, &q, k);

  assert(wge_equal(ec, &q, &expect));

  assert(wge_export(ec, q_raw, &q_size, &q, 1));
  assert(q_size == 33);

  assert(memcmp(q_raw, expect_raw, 33) == 0);
}

static void
test_wei_mul(void) {
  const unsigned char p_raw[33] = {
    0x03, 0x42, 0x67, 0xab, 0xc7, 0xde, 0x72, 0x0f,
    0x14, 0x5a, 0xbc, 0x94, 0xb9, 0x5b, 0x33, 0x50,
    0x7a, 0x37, 0x55, 0x55, 0x2b, 0xef, 0xaf, 0x57,
    0x61, 0x33, 0x7a, 0xd6, 0x7a, 0x28, 0xa9, 0x08,
    0xa1
  };

  const unsigned char k_raw[32] = {
    0xfd, 0x37, 0xfe, 0xab, 0xd9, 0xdd, 0x8d, 0xe5,
    0xfd, 0x04, 0x79, 0xf4, 0xd6, 0xea, 0xd4, 0xe6,
    0x02, 0xc7, 0x06, 0x0f, 0x43, 0x6e, 0x2b, 0xf1,
    0xc0, 0x72, 0xe9, 0x91, 0x80, 0xcb, 0x09, 0x18
  };

  const unsigned char expect_raw[33] = {
    0x02, 0x93, 0xa3, 0x55, 0xe4, 0x8f, 0x3b, 0x74,
    0xcc, 0x3b, 0xcb, 0xb4, 0x6c, 0xb2, 0x84, 0x3a,
    0xd5, 0x4e, 0xe5, 0xe0, 0x45, 0xe9, 0x17, 0x0b,
    0x00, 0x45, 0xbc, 0xc2, 0x86, 0x68, 0x8c, 0x4d,
    0x56
  };

  wei_t curve;
  wei_t *ec = &curve;
  sc_t k;
  wge_t p, q, expect;
  unsigned char entropy[32];
  unsigned char q_raw[33];
  size_t q_size;
  scalar_field_t *sc = &ec->sc;

  printf("Testing mul (vector).\n");

  wei_init(ec, &curve_p256);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(wge_import(ec, &p, p_raw, 33));
  assert(sc_import(sc, k, k_raw));
  assert(wge_import(ec, &expect, expect_raw, 33));

  assert(wge_validate(ec, &p));
  assert(wge_validate(ec, &expect));
  assert(wge_equal(ec, &expect, &expect));
  assert(!wge_equal(ec, &expect, &ec->g));

  wei_mul(ec, &q, &p, k);

  assert(wge_equal(ec, &q, &expect));

  assert(wge_export(ec, q_raw, &q_size, &q, 1));
  assert(q_size == 33);

  assert(memcmp(q_raw, expect_raw, 33) == 0);

  wei_mul_var(ec, &q, &p, k);

  assert(wge_equal(ec, &q, &expect));

  assert(wge_export(ec, q_raw, &q_size, &q, 1));
  assert(q_size == 33);

  assert(memcmp(q_raw, expect_raw, 33) == 0);
}

static void
test_wei_double_mul(void) {
  const unsigned char p_raw[33] = {
    0x02, 0x65, 0x26, 0x45, 0xad, 0x1a, 0x36, 0x8c,
    0xdc, 0xcf, 0x81, 0x90, 0x56, 0x3b, 0x2a, 0x12,
    0xba, 0x31, 0xea, 0x33, 0x78, 0xc2, 0x23, 0x66,
    0xff, 0xf8, 0x47, 0x92, 0x63, 0x8c, 0xb8, 0xc8,
    0x94
  };

  const unsigned char k1_raw[32] = {
    0x5f, 0xd3, 0x7e, 0x3c, 0x67, 0x9e, 0xc5, 0xd0,
    0x2b, 0xb6, 0x6a, 0xa8, 0x6e, 0x56, 0xd6, 0x40,
    0x65, 0xe9, 0x47, 0x74, 0x4e, 0x50, 0xee, 0xec,
    0x80, 0xcf, 0xcc, 0xce, 0x3b, 0xd2, 0xf2, 0x1a
  };

  const unsigned char k2_raw[32] = {
    0xfb, 0x15, 0x9a, 0x7d, 0x37, 0x4d, 0x24, 0xde,
    0xde, 0x0a, 0x55, 0xb2, 0x98, 0x26, 0xe3, 0x24,
    0xf6, 0xf1, 0xd7, 0x57, 0x36, 0x53, 0xd7, 0x8a,
    0x98, 0xed, 0xa2, 0x80, 0x6d, 0xbe, 0x37, 0x98
  };

  const unsigned char expect_raw[33] = {
    0x02, 0x96, 0xf1, 0xb9, 0xe3, 0xe7, 0x0b, 0xa1,
    0x2e, 0xaf, 0x40, 0x23, 0x05, 0x64, 0x5b, 0x0f,
    0x28, 0x1b, 0xec, 0x25, 0x4f, 0xf2, 0x31, 0x8f,
    0x96, 0x9c, 0x97, 0x96, 0x0c, 0x35, 0x0b, 0x2c,
    0x6d
  };

  wei_t curve;
  wei_t *ec = &curve;
  sc_t k1, k2;
  wge_t p, q, expect;
  unsigned char entropy[32];
  unsigned char q_raw[33];
  size_t q_size;
  scalar_field_t *sc = &ec->sc;

  printf("Testing double mul (vector).\n");

  wei_init(ec, &curve_p256);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(wge_import(ec, &p, p_raw, 33));
  assert(sc_import(sc, k1, k1_raw));
  assert(sc_import(sc, k2, k2_raw));
  assert(wge_import(ec, &expect, expect_raw, 33));

  assert(wge_validate(ec, &p));
  assert(wge_validate(ec, &expect));
  assert(wge_equal(ec, &expect, &expect));
  assert(!wge_equal(ec, &expect, &ec->g));

  wei_mul_double_var(ec, &q, k1, &p, k2);

  assert(wge_equal(ec, &q, &expect));

  assert(wge_export(ec, q_raw, &q_size, &q, 1));
  assert(q_size == 33);

  assert(memcmp(q_raw, expect_raw, 33) == 0);
}

static void
test_ecdsa_vector_p224(void) {
  const unsigned char priv[28] = {
    0x03, 0x18, 0x4c, 0xae, 0x2f, 0x68, 0x48, 0x28,
    0xfb, 0xe6, 0x84, 0x68, 0x5e, 0xbe, 0xad, 0xe4,
    0x2e, 0x81, 0x62, 0x1a, 0xc3, 0xe9, 0xde, 0xf7,
    0xb6, 0x74, 0xd2, 0x4c
  };

  const unsigned char pub[29] = {
    0x03, 0xcf, 0xef, 0x22, 0x9d, 0x70, 0x3e, 0x5c,
    0x45, 0x39, 0x47, 0x3d, 0x85, 0x4e, 0x15, 0x66,
    0x8a, 0x1f, 0x8a, 0x5e, 0x95, 0xe6, 0xc5, 0x24,
    0x4b, 0x13, 0x4c, 0x09, 0xdd
  };

  const unsigned char msg[32] = {
    0x0d, 0x76, 0x8c, 0xad, 0x89, 0x13, 0x06, 0xbe,
    0x8a, 0xb9, 0x7d, 0x1d, 0x92, 0x12, 0x2d, 0xf4,
    0x98, 0xa2, 0x25, 0xf6, 0xcb, 0x98, 0x6a, 0xe8,
    0x48, 0xd8, 0x4d, 0x10, 0xb0, 0x15, 0x0b, 0xec
  };

  const unsigned char sig[56] = {
    0x11, 0x94, 0x55, 0x81, 0x0e, 0xfe, 0x95, 0xfe,
    0x37, 0x98, 0x56, 0x8c, 0xf1, 0xb3, 0x53, 0xcd,
    0x61, 0x99, 0xec, 0xc2, 0xd0, 0x40, 0xb0, 0xbf,
    0x73, 0xd3, 0x21, 0x39, 0x43, 0x53, 0x2e, 0x35,
    0x12, 0x1e, 0xe0, 0xed, 0x17, 0x68, 0x77, 0x9f,
    0x85, 0xe0, 0xcb, 0x01, 0x1a, 0x6c, 0x32, 0x02,
    0x99, 0xaf, 0xe4, 0xf5, 0x15, 0x81, 0xcc, 0xfa
  };

  wei_t curve;
  wei_t *ec = &curve;
  unsigned int param = 0;
  unsigned char rec[29];
  unsigned char entropy[28];
  unsigned char sig0[56];
  unsigned int param0;
  size_t rec_len;

  printf("Testing P224 (vector).\n");

  wei_init(ec, &curve_p224);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(ecdsa_sign(ec, sig0, &param0, msg, 32, priv));
  assert(memcmp(sig0, sig, 56) == 0);
  assert(param0 == param);
  assert(ecdsa_pubkey_create(ec, rec, &rec_len, priv, 1));
  assert(rec_len == 29);
  assert(memcmp(rec, pub, 29) == 0);
  assert(ecdsa_verify(ec, msg, 32, sig, pub, 29));
  assert(ecdsa_recover(ec, rec, &rec_len, msg, 32, sig, param, 1));
  assert(rec_len == 29);
  assert(memcmp(rec, pub, 29) == 0);
}

static void
test_ecdsa_vector_p256(void) {
  const unsigned char priv[32] = {
    0x43, 0xf7, 0x29, 0xcc, 0x1d, 0x94, 0x94, 0xfe,
    0xb2, 0x8c, 0x1e, 0x1d, 0x36, 0xdb, 0xcd, 0xdf,
    0xdc, 0xd7, 0x17, 0x98, 0x8d, 0x51, 0xda, 0x88,
    0x8f, 0xea, 0xbc, 0x9e, 0x55, 0xe1, 0x71, 0xb8
  };

  const unsigned char pub[33] = {
    0x03, 0x80, 0x2b, 0x0d, 0xc2, 0x63, 0xd9, 0x1b,
    0xc5, 0x83, 0x1b, 0x9e, 0xfc, 0xc2, 0xb5, 0x0e,
    0x5b, 0xb5, 0xd9, 0x02, 0xbd, 0x67, 0xa4, 0x04,
    0xf7, 0xb7, 0x52, 0xdb, 0x3e, 0xed, 0xeb, 0x39,
    0xbf
  };

  const unsigned char msg[32] = {
    0x51, 0x89, 0x05, 0x98, 0xbf, 0xf4, 0xa6, 0x46,
    0x86, 0x35, 0xe8, 0xd1, 0x90, 0x3e, 0xdc, 0x7e,
    0x9b, 0xf4, 0xeb, 0xa7, 0x56, 0xe9, 0x7f, 0x3c,
    0xa0, 0x1a, 0x2c, 0xa9, 0x36, 0x54, 0x04, 0xae
  };

  const unsigned char sig[64] = {
    0xf5, 0xb0, 0x85, 0x60, 0xd4, 0xc6, 0x7b, 0x9d,
    0xa2, 0xe5, 0xda, 0x53, 0x22, 0x10, 0x1c, 0x96,
    0x44, 0x38, 0x6d, 0x7e, 0xc8, 0xd6, 0x8f, 0xc6,
    0x4a, 0xb5, 0xfe, 0xc6, 0x54, 0x66, 0xf9, 0x5e,
    0x33, 0x3e, 0x7d, 0x9c, 0x7c, 0xf2, 0x63, 0x5e,
    0x72, 0x49, 0x7d, 0xcf, 0xff, 0xcb, 0x38, 0x96,
    0xa2, 0x56, 0x10, 0x20, 0xee, 0x56, 0x42, 0x99,
    0x45, 0x11, 0x43, 0x75, 0x00, 0x0d, 0x96, 0xc5
  };

  wei_t curve;
  wei_t *ec = &curve;
  unsigned int param = 1;
  unsigned char rec[33];
  unsigned char entropy[32];
  unsigned char sig0[64];
  unsigned int param0;
  size_t rec_len;

  printf("Testing P256 (vector).\n");

  wei_init(ec, &curve_p256);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(ecdsa_sign(ec, sig0, &param0, msg, 32, priv));
  assert(memcmp(sig0, sig, 64) == 0);
  assert(param0 == param);
  assert(ecdsa_pubkey_create(ec, rec, &rec_len, priv, 1));
  assert(rec_len == 33);
  assert(memcmp(rec, pub, 33) == 0);
  assert(ecdsa_verify(ec, msg, 32, sig, pub, 33));
  assert(ecdsa_recover(ec, rec, &rec_len, msg, 32, sig, param, 1));
  assert(rec_len == 33);
  assert(memcmp(rec, pub, 33) == 0);
}

static void
test_ecdsa_vector_p384(void) {
  const unsigned char priv[48] = {
    0x91, 0x4f, 0xea, 0xd3, 0x24, 0xc1, 0x96, 0xe2,
    0x13, 0x21, 0x3b, 0x2b, 0x95, 0xb3, 0x96, 0x80,
    0x46, 0x8e, 0xe9, 0xb1, 0x0d, 0x56, 0x33, 0x5f,
    0x47, 0x04, 0xe6, 0xf7, 0xdf, 0x2a, 0x54, 0xca,
    0x18, 0xe1, 0xde, 0x2e, 0xcf, 0xa8, 0x92, 0x4c,
    0x61, 0xb5, 0x61, 0x4f, 0x41, 0x09, 0x63, 0xfa
  };

  const unsigned char pub[49] = {
    0x02, 0x15, 0xd6, 0x0b, 0xab, 0xdb, 0xea, 0x58,
    0xe1, 0x9a, 0x84, 0xbf, 0x5e, 0x3a, 0x6b, 0xbf,
    0xb4, 0x62, 0x6a, 0xd9, 0x1b, 0xb5, 0xd3, 0x92,
    0x4b, 0xc6, 0x38, 0x6e, 0xb7, 0x10, 0x66, 0x7b,
    0x0f, 0xfb, 0x68, 0x3e, 0x00, 0x45, 0x63, 0xe5,
    0x38, 0x15, 0x8d, 0x0d, 0x58, 0xbf, 0xb1, 0x20,
    0x97
  };

  const unsigned char msg[48] = {
    0x44, 0xf0, 0x46, 0xcf, 0x41, 0x81, 0xd9, 0x01,
    0xff, 0xd3, 0x9c, 0xce, 0x82, 0xff, 0x05, 0xc7,
    0xfd, 0x7b, 0xf9, 0x83, 0x35, 0x58, 0xb7, 0x68,
    0x46, 0xc5, 0x54, 0xa6, 0x73, 0x29, 0xf4, 0x0e,
    0x65, 0x93, 0xe2, 0xd9, 0x1c, 0xc8, 0x07, 0x71,
    0x49, 0x8f, 0x77, 0x17, 0x3a, 0xcb, 0xf5, 0xf6
  };

  const unsigned char sig[96] = {
    0x56, 0x2f, 0x6a, 0x5d, 0xbc, 0x58, 0xa9, 0xd5,
    0xa0, 0xe3, 0xe0, 0x10, 0xff, 0x8e, 0x84, 0xf6,
    0xe8, 0xd7, 0x0c, 0x63, 0x3e, 0x90, 0x49, 0x8e,
    0x32, 0xd2, 0xce, 0x6e, 0x66, 0x9a, 0x05, 0x03,
    0xcd, 0x11, 0xf9, 0xde, 0x8d, 0x8c, 0x04, 0x88,
    0xca, 0xdc, 0x9c, 0x36, 0xdd, 0x30, 0x15, 0xc5,
    0x6b, 0xd9, 0xed, 0xe8, 0x36, 0xa8, 0xc7, 0xf5,
    0xbf, 0x03, 0xef, 0xc0, 0xcd, 0xc4, 0x53, 0x02,
    0x28, 0x82, 0xb9, 0x16, 0x30, 0x6e, 0xb2, 0x61,
    0xe1, 0xdd, 0x54, 0x7a, 0xd5, 0x3a, 0x34, 0x08,
    0x1e, 0xa6, 0x78, 0xd5, 0x18, 0x4f, 0xb7, 0x95,
    0x09, 0xf0, 0x31, 0x57, 0xd1, 0xac, 0x49, 0x06
  };

  wei_t curve;
  wei_t *ec = &curve;
  unsigned int param = 1;
  unsigned char rec[49];
  unsigned char entropy[48];
  unsigned char sig0[96];
  unsigned int param0;
  size_t rec_len;

  printf("Testing P384 (vector).\n");

  wei_init(ec, &curve_p384);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(ecdsa_sign(ec, sig0, &param0, msg, 48, priv));
  assert(memcmp(sig0, sig, 96) == 0);
  assert(param0 == param);
  assert(ecdsa_pubkey_create(ec, rec, &rec_len, priv, 1));
  assert(rec_len == 49);
  assert(memcmp(rec, pub, 49) == 0);
  assert(ecdsa_verify(ec, msg, 48, sig, pub, 49));
  assert(ecdsa_recover(ec, rec, &rec_len, msg, 48, sig, param, 1));
  assert(rec_len == 49);
  assert(memcmp(rec, pub, 49) == 0);
}

static void
test_ecdsa_vector_p521(void) {
  const unsigned char priv[66] = {
    0x00, 0x31, 0x70, 0x3d, 0x94, 0x34, 0xb1, 0x2a,
    0xfc, 0x32, 0xb5, 0x51, 0x23, 0x39, 0xa2, 0xc7,
    0x85, 0xb6, 0xb6, 0xff, 0x22, 0xf4, 0xa1, 0xdd,
    0x04, 0xe8, 0xe0, 0xc2, 0xfc, 0x62, 0x8d, 0x9d,
    0x9b, 0x41, 0xfc, 0x7c, 0x28, 0xf4, 0xfb, 0x42,
    0x25, 0xf1, 0x32, 0xbd, 0x6f, 0x92, 0xdc, 0xb6,
    0xc0, 0x56, 0x43, 0xc3, 0xd4, 0x9c, 0x06, 0xb2,
    0xd2, 0x6d, 0x15, 0xbe, 0x0b, 0xe0, 0x6a, 0x15,
    0x77, 0x78
  };

  const unsigned char pub[67] = {
    0x03, 0x00, 0x07, 0x30, 0x29, 0x49, 0xb5, 0xe2,
    0x96, 0x2f, 0xf2, 0x11, 0xcf, 0x47, 0x23, 0x49,
    0x2a, 0x34, 0xce, 0xd7, 0x1a, 0x1b, 0xc0, 0xed,
    0x34, 0x21, 0x51, 0xdc, 0xf1, 0xb8, 0xe1, 0xa1,
    0x9c, 0x6b, 0x66, 0xf2, 0xcd, 0x54, 0xbe, 0x40,
    0x62, 0x42, 0xb2, 0x54, 0x50, 0x22, 0xf3, 0x41,
    0x84, 0x4c, 0x33, 0x84, 0x7a, 0xb0, 0x38, 0x7e,
    0xa2, 0x3a, 0x00, 0x5d, 0x41, 0xa0, 0xe5, 0x6a,
    0x93, 0x7a, 0x50
  };

  const unsigned char msg[64] = {
    0x5d, 0xed, 0xf5, 0x8d, 0xe6, 0x01, 0x5e, 0x54,
    0x2a, 0xd1, 0x80, 0x6e, 0x47, 0x69, 0x2d, 0x86,
    0x48, 0xaf, 0x84, 0x31, 0x10, 0x58, 0x37, 0x4d,
    0x46, 0xd9, 0x12, 0xa1, 0xe5, 0xa3, 0x20, 0x62,
    0x0b, 0xe4, 0xea, 0xc8, 0x8c, 0xcc, 0x52, 0xa7,
    0xaa, 0x17, 0xd4, 0x65, 0x37, 0x54, 0xa4, 0xe3,
    0xb4, 0x92, 0x2e, 0xe9, 0x28, 0xb8, 0xfb, 0x7e,
    0x2f, 0x55, 0xd4, 0xd5, 0x15, 0x86, 0xae, 0xc6
  };

  const unsigned char sig[132] = {
    0x01, 0x48, 0x8e, 0xb1, 0x8e, 0x71, 0x7e, 0xce,
    0x21, 0x5a, 0xb9, 0x02, 0x61, 0xb7, 0xaa, 0x5a,
    0x1c, 0x04, 0x2e, 0x5c, 0x0b, 0x02, 0x24, 0x9e,
    0x91, 0xaf, 0x87, 0x10, 0x4e, 0x14, 0xc9, 0x67,
    0xb8, 0xf0, 0x5c, 0x70, 0xf0, 0x00, 0xd8, 0xe1,
    0xdc, 0xe4, 0xf2, 0x35, 0x14, 0xd9, 0x4a, 0xef,
    0xfb, 0x2a, 0xc8, 0x27, 0x5e, 0x03, 0x6e, 0x55,
    0x6b, 0xf8, 0xfe, 0xe9, 0x4b, 0xb5, 0xcf, 0x39,
    0xb8, 0xd8, 0x00, 0x94, 0xf0, 0x01, 0x26, 0xb6,
    0x12, 0x9e, 0xb1, 0xca, 0x58, 0x19, 0xd6, 0x0f,
    0xcb, 0x34, 0x7b, 0x44, 0x02, 0xbe, 0x21, 0x0e,
    0x6e, 0x52, 0x71, 0xbe, 0xd6, 0x13, 0xb6, 0x51,
    0x98, 0xb0, 0x79, 0x83, 0x73, 0x0f, 0xe5, 0x4c,
    0x17, 0x6d, 0xd2, 0x1e, 0x23, 0x98, 0xb5, 0xd1,
    0x66, 0xc1, 0x40, 0x71, 0xa4, 0x42, 0x50, 0x87,
    0xdc, 0xa9, 0xb5, 0xe2, 0x0e, 0x8d, 0xd7, 0x3d,
    0x3a, 0xe1, 0xe2, 0x17
  };

  wei_t curve;
  wei_t *ec = &curve;
  unsigned int param = 0;
  unsigned char rec[67];
  unsigned char entropy[66];
  unsigned char sig0[132];
  unsigned int param0;
  size_t rec_len;

  printf("Testing P521 (vector).\n");

  wei_init(ec, &curve_p521);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(ecdsa_sign(ec, sig0, &param0, msg, 64, priv));
  assert(memcmp(sig0, sig, 132) == 0);
  assert(param0 == param);
  assert(ecdsa_pubkey_create(ec, rec, &rec_len, priv, 1));
  assert(rec_len == 67);
  assert(memcmp(rec, pub, 67) == 0);
  assert(ecdsa_verify(ec, msg, 64, sig, pub, 67));
  assert(ecdsa_recover(ec, rec, &rec_len, msg, 64, sig, param, 1));
  assert(rec_len == 67);
  assert(memcmp(rec, pub, 67) == 0);
}

static void
test_ecdsa_vector_secp256k1(void) {
  const unsigned char priv[32] = {
    0xcc, 0x52, 0x4c, 0x2f, 0xe6, 0x2c, 0xc8, 0xb8,
    0x20, 0xbc, 0x83, 0x08, 0x90, 0xbe, 0xdd, 0x62,
    0x3d, 0x3a, 0x83, 0x6d, 0xce, 0x22, 0x51, 0x70,
    0x23, 0xbc, 0xda, 0x4f, 0x1c, 0x5c, 0x75, 0x6e
  };

  const unsigned char pub[33] = {
    0x02, 0x03, 0xca, 0xd7, 0xf3, 0x01, 0xac, 0xf0,
    0xbb, 0x10, 0x2b, 0xc7, 0xe6, 0x80, 0xdc, 0xb0,
    0x74, 0x00, 0x3f, 0xfd, 0xa0, 0xa6, 0xbe, 0x69,
    0x6a, 0xd0, 0xcf, 0x12, 0x9b, 0x87, 0x57, 0x6c,
    0xd0
  };

  const unsigned char msg[32] = {
    0xfa, 0x09, 0xee, 0x3d, 0x85, 0xc4, 0x93, 0x8e,
    0x09, 0x8f, 0xbb, 0xf6, 0xa4, 0xf7, 0x61, 0xa0,
    0x53, 0x7e, 0x46, 0x5f, 0x61, 0x0b, 0x78, 0x73,
    0xfb, 0x26, 0x43, 0x06, 0xc3, 0x7b, 0x33, 0x6c
  };

  const unsigned char sig[64] = {
    0x83, 0xec, 0xd1, 0xab, 0x7c, 0x38, 0x8d, 0xc9,
    0xf0, 0x95, 0x7a, 0xe3, 0x9e, 0x9c, 0x40, 0xdf,
    0x99, 0xf8, 0x30, 0x30, 0x04, 0x25, 0xea, 0xd6,
    0x65, 0x9f, 0x1a, 0xcd, 0xed, 0xbe, 0xc9, 0xe6,
    0x17, 0x78, 0x97, 0x4e, 0x16, 0x8d, 0xa0, 0xcd,
    0x64, 0xd0, 0xf8, 0x96, 0x31, 0x48, 0xec, 0xbc,
    0x7f, 0xa7, 0x32, 0x5c, 0x5a, 0x8f, 0x1b, 0x9b,
    0x3a, 0xa0, 0xea, 0xcf, 0x74, 0x56, 0x8c, 0x1a
  };

  printf("Testing SECP256K1 (vector).\n");

  wei_t curve;
  wei_t *ec = &curve;
  unsigned int param = 0;
  unsigned char rec[33];
  unsigned char entropy[32];
  unsigned char sig0[64];
  unsigned int param0;
  size_t rec_len;

  wei_init(ec, &curve_secp256k1);

  random_bytes(entropy, sizeof(entropy));

  wei_randomize(ec, entropy);

  assert(ecdsa_sign(ec, sig0, &param0, msg, 32, priv));
  assert(memcmp(sig0, sig, 64) == 0);
  assert(param0 == param);
  assert(ecdsa_pubkey_create(ec, rec, &rec_len, priv, 1));
  assert(rec_len == 33);
  assert(memcmp(rec, pub, 33) == 0);
  assert(ecdsa_verify(ec, msg, 32, sig, pub, 33));
  assert(ecdsa_recover(ec, rec, &rec_len, msg, 32, sig, param, 1));
  assert(rec_len == 33);
  assert(memcmp(rec, pub, 33) == 0);
}

static void
test_edwards_points_ed25519(void) {
  edwards_t curve;
  edwards_t *ec = &curve;
  ege_t g, p, q, r;
  xge_t jg, jp, jq, jr;
  unsigned char entropy[32];
  unsigned char p_raw[32];

  const unsigned char g_raw[32] = {
    0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
  };

  const unsigned char g2_raw[32] = {
    0xc9, 0xa3, 0xf8, 0x6a, 0xae, 0x46, 0x5f, 0x0e,
    0x56, 0x51, 0x38, 0x64, 0x51, 0x0f, 0x39, 0x97,
    0x56, 0x1f, 0xa2, 0xc9, 0xe8, 0x5e, 0xa2, 0x1d,
    0xc2, 0x29, 0x23, 0x09, 0xf3, 0xcd, 0x60, 0x22
  };

  const unsigned char g3_raw[32] = {
    0xd4, 0xb4, 0xf5, 0x78, 0x48, 0x68, 0xc3, 0x02,
    0x04, 0x03, 0x24, 0x67, 0x17, 0xec, 0x16, 0x9f,
    0xf7, 0x9e, 0x26, 0x60, 0x8e, 0xa1, 0x26, 0xa1,
    0xab, 0x69, 0xee, 0x77, 0xd1, 0xb1, 0x67, 0x12
  };

  printf("Testing Edwards group law (ED25519).\n");

  edwards_init(ec, &curve_ed25519);

  random_bytes(entropy, sizeof(entropy));

  edwards_randomize(ec, entropy);

  ege_set(ec, &g, &ec->g);
  ege_to_xge(ec, &jg, &ec->g);

  assert(ege_import(ec, &p, g_raw));

  ege_to_xge(ec, &jp, &p);
  ege_to_xge(ec, &jq, &ec->g);

  assert(ege_validate(ec, &p));
  assert(xge_validate(ec, &jp));
  assert(xge_validate(ec, &jq));
  assert(ege_equal(ec, &p, &ec->g));
  assert(xge_equal(ec, &jp, &jq));

  assert(ege_import(ec, &q, g2_raw));
  assert(ege_import(ec, &r, g3_raw));

  ege_to_xge(ec, &jq, &q);
  ege_to_xge(ec, &jr, &r);

  ege_dbl(ec, &p, &ec->g);

  assert(ege_equal(ec, &p, &q));

  ege_add(ec, &p, &p, &ec->g);

  assert(ege_equal(ec, &p, &r));

  xge_dbl(ec, &jp, &jg);

  assert(xge_equal(ec, &jp, &jq));

  xge_add(ec, &jp, &jp, &jg);

  assert(xge_equal(ec, &jp, &jr));

  xge_sub(ec, &jp, &jp, &jg);

  assert(xge_equal(ec, &jp, &jq));

  xge_add(ec, &jp, &jp, &jg);

  assert(xge_equal(ec, &jp, &jr));

  xge_sub(ec, &jp, &jp, &jg);

  assert(xge_equal(ec, &jp, &jq));

  assert(xge_validate(ec, &jg));
  assert(xge_validate(ec, &jp));
  assert(xge_validate(ec, &jq));
  assert(xge_validate(ec, &jr));

  assert(!xge_is_zero(ec, &jg));
  assert(!xge_is_zero(ec, &jp));
  assert(!xge_is_zero(ec, &jq));
  assert(!xge_is_zero(ec, &jr));

  xge_to_ege(ec, &p, &jp);

  assert(ege_equal(ec, &p, &q));

  ege_export(ec, p_raw, &p);
  assert(memcmp(p_raw, g2_raw, 32) == 0);
}

static void
test_eddsa_vector_ed25519(void) {
  const unsigned char priv[32] = {
    0xd7, 0x4c, 0x01, 0x53, 0xc5, 0xcd, 0xf4, 0x8b,
    0x7b, 0x3e, 0x60, 0x2c, 0x2e, 0x4b, 0x36, 0xaf,
    0x2b, 0xe6, 0x62, 0xe6, 0xd7, 0x83, 0x84, 0x5f,
    0xc4, 0x96, 0x0f, 0x16, 0x25, 0x0d, 0x23, 0xbe
  };

  const unsigned char pub[33] = {
    0x75, 0x0d, 0xcf, 0x38, 0xc4, 0x57, 0x9c, 0x65,
    0xea, 0x16, 0x16, 0x0c, 0x51, 0xc6, 0x42, 0x2d,
    0x72, 0x76, 0x3e, 0x69, 0x7f, 0xd8, 0x6d, 0x09,
    0x5e, 0x91, 0x73, 0x3b, 0x1a, 0xab, 0x4b, 0x7e
  };

  const unsigned char msg[32] = {
    0x9d, 0x89, 0xd6, 0xbd, 0x57, 0x83, 0x61, 0xa9,
    0x9f, 0x01, 0x8b, 0x23, 0x48, 0xed, 0x97, 0xf1,
    0xdd, 0x06, 0xd1, 0x79, 0xe7, 0xe1, 0xa2, 0xba,
    0xee, 0x59, 0x56, 0x0a, 0xbe, 0x54, 0xaf, 0x06
  };

  const unsigned char sig[64] = {
    0xe2, 0x33, 0xf6, 0x44, 0x0e, 0x5a, 0x88, 0xc8,
    0xdc, 0x20, 0x6b, 0xfb, 0x5e, 0xe2, 0x41, 0x97,
    0x29, 0x2b, 0x89, 0x39, 0x6b, 0x26, 0x39, 0x0a,
    0x42, 0x57, 0x06, 0x70, 0x01, 0x57, 0x5a, 0x06,
    0x61, 0x95, 0x5a, 0x70, 0xd9, 0x14, 0x4f, 0x92,
    0x9e, 0xfd, 0x0f, 0xf5, 0x20, 0x12, 0xa8, 0x74,
    0x89, 0xe1, 0x05, 0x95, 0x45, 0x09, 0x76, 0x2d,
    0x82, 0xb2, 0x69, 0xec, 0x82, 0x52, 0x7b, 0x08
  };

  printf("Testing EdDSA (vector).\n");

  edwards_t curve;
  edwards_t *ec = &curve;
  unsigned char rec[32];
  unsigned char entropy[32];
  unsigned char sig0[64];

  edwards_init(ec, &curve_ed25519);

  random_bytes(entropy, sizeof(entropy));

  edwards_randomize(ec, entropy);

  eddsa_sign(ec, sig0, msg, 32, priv, -1, NULL, 0);

  assert(memcmp(sig0, sig, 64) == 0);

  eddsa_pubkey_create(ec, rec, priv);

  assert(memcmp(rec, pub, 32) == 0);

  assert(eddsa_verify(ec, msg, 32, sig, pub, -1, NULL, 0));
}

static void
test_ecdsa_random(void) {
  size_t i;

  printf("Randomized ECDSA testing...\n");

  for (i = 0; i < ARRAY_SIZE(wei_curves); i++) {
    const wei_def_t *def = wei_curves[i];
    wei_t ec;
    prime_field_t *fe = &ec.fe;
    scalar_field_t *sc = &ec.sc;
    unsigned char entropy[MAX_SCALAR_SIZE];
    unsigned char priv[MAX_SCALAR_SIZE];
    unsigned char msg[MAX_SCALAR_SIZE];
    unsigned char sig[MAX_SCALAR_SIZE * 2];
    unsigned char pub[MAX_FIELD_SIZE + 1];
    unsigned char rec[MAX_FIELD_SIZE + 1];
    size_t pub_len, rec_len;
    unsigned int param;
    size_t i;

    printf("  - %s\n", def->id);

    wei_init(&ec, def);

    random_bytes(entropy, sizeof(entropy));
    random_bytes(priv, sizeof(priv));
    random_bytes(msg, sizeof(msg));

    priv[0] = 0;

    wei_randomize(&ec, entropy);

    assert(ecdsa_sign(&ec, sig, &param, msg, sc->size, priv));
    assert(ecdsa_pubkey_create(&ec, pub, &pub_len, priv, 1));
    assert(pub_len == fe->size + 1);
    assert(ecdsa_verify(&ec, msg, sc->size, sig, pub, fe->size + 1));
    assert(ecdsa_recover(&ec, rec, &rec_len, msg, sc->size, sig, param, 1));
    assert(rec_len == fe->size + 1);
    assert(memcmp(pub, rec, fe->size + 1) == 0);

    i = random_int(sc->size);

    msg[i] ^= 1;
    assert(!ecdsa_verify(&ec, msg, sc->size, sig, pub, fe->size + 1));
    msg[i] ^= 1;

    pub[i] ^= 1;
    assert(!ecdsa_verify(&ec, msg, sc->size, sig, pub, fe->size + 1));
    pub[i] ^= 1;

    sig[i] ^= 1;
    assert(!ecdsa_verify(&ec, msg, sc->size, sig, pub, fe->size + 1));
    sig[i] ^= 1;

    sig[sc->size + i] ^= 1;
    assert(!ecdsa_verify(&ec, msg, sc->size, sig, pub, fe->size + 1));
    sig[sc->size + i] ^= 1;

    assert(ecdsa_verify(&ec, msg, sc->size, sig, pub, fe->size + 1));
  }
}

static void
test_eddsa_random(void) {
  size_t i;

  printf("Randomized EdDSA testing...\n");

  for (i = 0; i < ARRAY_SIZE(edwards_curves); i++) {
    const edwards_def_t *def = edwards_curves[i];
    edwards_t ec;
    prime_field_t *fe = &ec.fe;
    scalar_field_t *sc = &ec.sc;
    unsigned char entropy[MAX_SCALAR_SIZE];
    unsigned char priv[MAX_FIELD_SIZE];
    unsigned char msg[MAX_SCALAR_SIZE];
    unsigned char sig[MAX_FIELD_SIZE * 2];
    unsigned char pub[MAX_FIELD_SIZE];
    size_t i;

    printf("  - %s\n", def->id);

    edwards_init(&ec, def);

    random_bytes(entropy, sizeof(entropy));
    random_bytes(priv, sizeof(priv));
    random_bytes(msg, sizeof(msg));

    edwards_randomize(&ec, entropy);

    eddsa_sign(&ec, sig, msg, sc->size, priv, -1, NULL, 0);
    eddsa_pubkey_create(&ec, pub, priv);

    assert(eddsa_verify(&ec, msg, sc->size, sig, pub, -1, NULL, 0));

    i = random_int(sc->size);

    msg[i] ^= 1;
    assert(!eddsa_verify(&ec, msg, sc->size, sig, pub, -1, NULL, 0));
    msg[i] ^= 1;

    pub[i] ^= 1;
    assert(!eddsa_verify(&ec, msg, sc->size, sig, pub, -1, NULL, 0));
    pub[i] ^= 1;

    sig[i] ^= 1;
    assert(!eddsa_verify(&ec, msg, sc->size, sig, pub, -1, NULL, 0));
    sig[i] ^= 1;

    sig[fe->size + i] ^= 1;
    assert(!eddsa_verify(&ec, msg, sc->size, sig, pub, -1, NULL, 0));
    sig[fe->size + i] ^= 1;

    assert(eddsa_verify(&ec, msg, sc->size, sig, pub, -1, NULL, 0));
  }
}

int
main(void) {
  test_sc();
  test_fe();
  test_wei_points_p256();
  test_wei_points_p521();
  test_wei_mul_g();
  test_wei_mul();
  test_wei_double_mul();
  test_ecdsa_vector_p224();
  test_ecdsa_vector_p256();
  test_ecdsa_vector_p384();
  test_ecdsa_vector_p521();
  test_ecdsa_vector_secp256k1();
  test_edwards_points_ed25519();
  test_eddsa_vector_ed25519();
  test_ecdsa_random();
  test_eddsa_random();
  return 0;
}
