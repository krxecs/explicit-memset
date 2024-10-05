/*
 * SPDX-License-Identifier: 0BSD
 *
 * License Text:
 * BSD Zero Clause License (SPDX: 0BSD)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef AY_EXPLICIT_MEMSET_H
#define AY_EXPLICIT_MEMSET_H

/* To use this single-file library, create a file explicit_memset.c with the
 * following content: (or just copy src/ay/explicit-memset.c)
 *
 * #define AY_EXPLICIT_MEMSET_IMPLEMENTATION
 * #include "<location of header>"
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stddef.h>

/**
 * Writes len bytes of value c (converted to an unsigned char) to the string
 * str. It is guaranteed not to be optimized away by the compiler even if str is
 * no longer used and is about to be freed or go out of scope.
 *
 * @param str Pointer to the block of memory to fill
 * @param c Value to be set. The value is passed as an int, but the function
 * fills the block of memory using the unsigned char conversion of this value
 * @param n Number of bytes to be set to the value
 *
 * @returns str is returned
 */
void *ay_explicit_memset(void *str, int c, size_t n);

#ifdef AY_EXPLICIT_MEMSET_IMPLEMENTATION

#ifndef __STDC_WANT_LIB_EXT1__
/* memset_s() function in C11 Annex K (bound-checking interface) */
#define __STDC_WANT_LIB_EXT1__ 1
#endif

#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <sys/param.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif /* _WIN32 */

#ifdef __GLIBC__
#if __GLIBC_PREREQ(2, 25)
#define HAVE_EXPLICIT_BZERO 1
#endif
#endif

#if !defined(HAVE_EXPLICIT_BZERO) &&                                           \
        (defined(__FreeBSD__) && __FreeBSD_version >= 1100037) ||              \
    (defined(__OpenBSD__) && OpenBSD >= 201405)
/* explicit_bzero was added in:
 * - glibc 2.25
 * - FreeBSD 11.0 (__FreeBSD_version == 1100037). From:
 *   https://docs.freebsd.org/en/books/porters-handbook/versions/#versions-11
 * - OpenBSD 5.5 (OpenBSD >= 201405)
 */
#define HAVE_EXPLICIT_BZERO 1
#endif

/* explicit_memset was added in:
 * - NetBSD 7.0 (__NetBSD_Version__ >= 702000000).
 */
#if defined(__NetBSD__) && __NetBSD_Version__ >= 702000000
#define HAVE_EXPLICIT_MEMSET 1
#endif

#if defined(__GNUC__) && (defined(__ELF__) || defined(__APPLE_CC__))
#define HAVE_WEAK_LINKING_SUPPORT 1
__attribute__((weak)) void weak_sym_to_avoid_optimization(void *str, size_t n);
#endif

void *ay_explicit_memset(void *str, int c, size_t n) {
#if defined(HAVE_EXPLICIT_MEMSET)
  return explicit_memset(str, c, n);
#elif defined(__STDC_LIB_EXT1__)
  /* memset_s() function in C11 Annex K (bounds-checking interface) */
  (void)memset_s(str, (rsize_t)n, c, (rsize_t)n);
  return str;
#else

  if (c == 0) {
#if defined(_WIN32)
    return SecureZeroMemory(str, n);
#elif defined(HAVE_EXPLICIT_BZERO)
    explicit_bzero(str, n);
    return str;
#endif
  }

#if defined(__GNUC__)
  /* Use weak linking if available to prevent compiler from optimizing away
   * memset calls. (Approach used by libsodium[1]).
   *
   * [1]:
   * https://github.com/jedisct1/libsodium/blob/master/src/libsodium/sodium/utils.c#L106
   */
  void *result_of_memset = memset(str, c, n);

#ifdef HAVE_WEAK_LINKING_SUPPORT
  if (weak_sym_to_avoid_optimization) {
    weak_sym_to_avoid_optimization(str, n);
  }
#endif

  /* Use a ASM memory barrier to force GCC to not optimize memset away. (Used by
   * Glibc) */
  __asm__ __volatile__("" : : "r"(str) : "memory");

  return result_of_memset;
#else
  /* Use a volatile pointer to memory buffer as used by Libsodium for securely
   * clearing memory. */
  volatile unsigned char *vstr = (volatile unsigned char *)str;
  for (size_t i = 0; i < n; ++i) {
    vstr[i] = (unsigned char)c;
  }

  return str;
#endif /* defined(__GNUC__) */

#endif
}

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* AY_EXPLICIT_MEMSET_H */
