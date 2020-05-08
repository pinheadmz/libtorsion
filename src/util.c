/*!
 * util.c - utils for libtorsion
 * Copyright (c) 2020, Christopher Jeffrey (MIT License).
 * https://github.com/bcoin-org/libtorsion
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
/* For SecureZeroMemory (actually defined in winbase.h). */
#include <windows.h>
#endif

#include <torsion/util.h>
#include "internal.h"

/*
 * Callbacks
 */

static void
default_die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
  abort();
}

static void *
default_malloc(size_t size) {
  return malloc(size);
}

static void *
default_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}

static void
default_free(void *ptr) {
  free(ptr);
}

static torsion_die_f *die_cb = default_die;
static torsion_malloc_f *malloc_cb = default_malloc;
static torsion_realloc_f *realloc_cb = default_realloc;
static torsion_free_f *free_cb = default_free;

/*
 * Error Handling
 */

void
torsion_set_die_function(torsion_die_f *die_fn) {
  die_cb = die_fn;
}

void
torsion_get_die_function(torsion_die_f **die_fn) {
  *die_fn = die_cb;
}

void
torsion_die(const char *msg) {
  die_cb(msg);
}

/*
 * Allocation (avoids impl-defined behavior)
 */

void
torsion_set_memory_functions(torsion_malloc_f *malloc_fn,
                             torsion_realloc_f *realloc_fn,
                             torsion_free_f *free_fn) {
  if (malloc_fn)
    malloc_cb = malloc_fn;

  if (realloc_fn)
    realloc_cb = realloc_fn;

  if (free_fn)
    free_cb = free_fn;
}

void
torsion_get_memory_functions(torsion_malloc_f **malloc_fn,
                             torsion_realloc_f **realloc_fn,
                             torsion_free_f **free_fn) {
  if (malloc_fn)
    *malloc_fn = malloc_cb;

  if (realloc_fn)
    *realloc_fn = realloc_cb;

  if (free_fn)
    *free_fn = free_cb;
}

void *
torsion_malloc(size_t size) {
  if (size == 0)
    return NULL;

  return malloc_cb(size);
}

void *
torsion_calloc(size_t nmemb, size_t size) {
  void *ptr;

  size *= nmemb;

  if (size == 0)
    return NULL;

  ptr = malloc_cb(size);

  if (LIKELY(ptr != NULL))
    memset(ptr, 0, size);

  return ptr;
}

void *
torsion_realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    if (size == 0)
      return NULL;

    return malloc_cb(size);
  }

  if (size == 0) {
    free_cb(ptr);
    return NULL;
  }

  return realloc_cb(ptr, size);
}

void
torsion_free(void *ptr) {
  if (ptr != NULL)
    free_cb(ptr);
}

void *
torsion_xmalloc(size_t size) {
  void *ptr = torsion_malloc(size);

  if (UNLIKELY(ptr == NULL && size != 0))
    die_cb("torsion_xmalloc: allocation failure.");

  return ptr;
}

void *
torsion_xcalloc(size_t nmemb, size_t size) {
  void *ptr = torsion_calloc(nmemb, size);

  if (UNLIKELY(ptr == NULL && nmemb != 0 && size != 0))
    die_cb("torsion_xcalloc: allocation failure.");

  return ptr;
}

void *
torsion_xrealloc(void *ptr, size_t size) {
  ptr = torsion_realloc(ptr, size);

  if (UNLIKELY(ptr == NULL && size != 0))
    die_cb("torsion_xrealloc: allocation failure.");

  return ptr;
}

/*
 * Memzero
 */

void
cleanse(void *ptr, size_t len) {
#if defined(_WIN32)
  /* https://github.com/jedisct1/libsodium/blob/3b26a5c/src/libsodium/sodium/utils.c#L112 */
  if (len > 0)
    SecureZeroMemory(ptr, len);
#elif defined(__GNUC__) || defined(__clang__)
  /* https://github.com/torvalds/linux/blob/37d4e84/include/linux/string.h#L233 */
  /* https://github.com/torvalds/linux/blob/37d4e84/include/linux/compiler-gcc.h#L21 */
  /* https://github.com/bminor/glibc/blob/master/string/explicit_bzero.c */
  if (len > 0) {
    memset(ptr, 0, len);
    __asm__ __volatile__("" : : "r" (ptr) : "memory");
  }
#else
  /* http://www.daemonology.net/blog/2014-09-04-how-to-zero-a-buffer.html */
  static void *(*const volatile memset_ptr)(void *, int, size_t) = memset;
  if (len > 0)
    memset_ptr(ptr, 0, len);
#endif
}
