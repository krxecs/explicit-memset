/*
 * SPDX-License-Identifier: 0BSD
 */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <ay/explicit-memset.h>
#include <munit.h>

/*#ifndef __GLIBC__*/
/*
 * The my_memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
void *my_memmem(const void *haystack, size_t hlen, const void *needle,
                size_t nlen) {
  int needle_first;
  const void *p = haystack;
  size_t plen = hlen;

  if (!nlen) {
    return NULL;
  }

  needle_first = *(unsigned char *)needle;

  while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1))) {
    if (!memcmp(p, needle, nlen)) {
      return (void *)p;
    }

    p++;
    plen = hlen - (p - haystack);
  }

  return NULL;
}
/* #endif */

struct count_of_secrets {
  int without_memset;
  int with_memset;
};

static struct count_of_secrets secrets_count = {0};

/* 128 bits of random data. */
static const unsigned char secret[16] = {
    0xa0, 0x6c, 0x0c, 0x81, 0xba, 0xd8, 0x5b, 0x0c,
    0xb0, 0xd6, 0xd4, 0xe3, 0xeb, 0x52, 0x5f, 0x96,
};

enum { SECRETCOUNT = 64, SECRETBYTES = SECRETCOUNT * sizeof(secret) };

static char altstack[SIGSTKSZ + SECRETBYTES];

static void setup_stack(void) {
  const stack_t sigstk = {
      .ss_sp = altstack,
      .ss_size = sizeof(altstack),
  };

  munit_assert_int(sigaltstack(&sigstk, NULL), ==, 0);
}

static void assert_on_stack(void) {
  stack_t cursigstk;
  munit_assert_int(sigaltstack(NULL, &cursigstk), ==, 0);
  munit_assert_int(SS_ONSTACK, ==,
                   cursigstk.ss_flags & (SS_DISABLE | SS_ONSTACK));
}

static void call_on_stack(void (*fn)(int)) {
  /*
   * This is a bit more complicated than strictly necessary, but
   * it ensures we don't have any flaky test failures due to
   * inherited signal masks/actions/etc.
   *
   * On systems where SA_ONSTACK is not supported, this could
   * alternatively be implemented using makecontext() or
   * pthread_attr_setstack().
   */

  const struct sigaction sigact = {
      .sa_handler = fn,
      .sa_flags = SA_ONSTACK,
  };
  struct sigaction oldsigact;
  sigset_t sigset0, oldsigset;

  /* First, block all signals. */
  munit_assert_int(sigemptyset(&sigset0), ==, 0);
  munit_assert_int(sigfillset(&sigset0), ==, 0);
  munit_assert_int(sigprocmask(SIG_BLOCK, &sigset0, &oldsigset), ==, 0);

  /* Next setup the signal handler for SIGUSR1. */
  munit_assert_int(sigaction(SIGUSR1, &sigact, &oldsigact), ==, 0);

  /* Raise SIGUSR1 and momentarily unblock it to run the handler. */
  munit_assert_int(raise(SIGUSR1), ==, 0);
  munit_assert_int(sigdelset(&sigset0, SIGUSR1), ==, 0);
  munit_assert_int(sigsuspend(&sigset0), ==, -1);
  munit_assert_int(errno, ==, EINTR);

  /* Restore the original signal action, stack, and mask. */
  munit_assert_int(sigaction(SIGUSR1, &oldsigact, NULL), ==, 0);
  munit_assert_int(sigprocmask(SIG_SETMASK, &oldsigset, NULL), ==, 0);
}

static void populate_secret(char *buf, size_t len) {
  int i, fds[2];
  munit_assert_int(pipe(fds), ==, 0);

  for (i = 0; i < SECRETCOUNT; i++)
    munit_assert_int(sizeof(secret), ==, write(fds[1], secret, sizeof(secret)));

  munit_assert_int(close(fds[1]), ==, 0);

  munit_assert_int(read(fds[0], buf, len), ==, len);
  munit_assert_int(close(fds[0]), ==, 0);
}

static int count_secrets(const char *buf) {
  int res = 0;
  size_t i;
  for (i = 0; i < SECRETCOUNT; i++) {
    if (memcmp(buf + i * sizeof(secret), secret, sizeof(secret)) == 0) {
      res += 1;
    }
  }
  return (res);
}

static char *test_without_memset(void) {
  char buf[SECRETBYTES];
  assert_on_stack();
  populate_secret(buf, sizeof(buf));
  char *res = my_memmem(altstack, sizeof(altstack), buf, sizeof(buf));

  munit_assert_not_null(res);
  return (res);
}

static char *test_with_memset(void) {
  char buf[SECRETBYTES];
  assert_on_stack();
  populate_secret(buf, sizeof(buf));
  char *res = my_memmem(altstack, sizeof(altstack), buf, sizeof(buf));

  munit_assert_not_null(res);

  ay_explicit_memset(buf, 0, sizeof(buf));
  return (res);
}

static void do_test_without_memset(int signo) {
  (void)signo;
  char *buf = test_without_memset();
  secrets_count.without_memset = count_secrets(buf);
}

static void do_test_with_memset(int signo) {
  (void)signo;
  char *buf = test_with_memset();
  secrets_count.with_memset = count_secrets(buf);
}

static MunitResult without_memset_test(const MunitParameter params[],
                                       void *user_data_or_fixture) {
  (void)params;
  (void)user_data_or_fixture;
  /*
   * First, test that if we *don't* call ay_explicit_memset, that we
   * *are* able to find at least one instance of the secret data still
   * on the stack.  This sanity checks that call_on_stack() and
   * populate_secret() work as intended.
   */

  memset(altstack, 0, sizeof(altstack));
  call_on_stack(do_test_without_memset);

  munit_assert_int(secrets_count.without_memset, >=, 1);
  return MUNIT_OK;
}

static MunitResult with_memset_test(const MunitParameter params[],
                                    void *user_data_or_fixture) {
  (void)params;
  (void)user_data_or_fixture;
  /*
   * Now test with a call to ay_explicit_memset() and check that we
   * *don't* find any instances of the secret data.
   */
  memset(altstack, 0, sizeof(altstack));
  call_on_stack(do_test_with_memset);

  munit_assert_int(secrets_count.with_memset, ==, 0);
  return MUNIT_OK;
}

static MunitTest tests[] = {
    {(char *)"/without-memset", without_memset_test, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/with-memset", with_memset_test, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    (char *)"/explicit-memset", /* name */
    tests,                      /* tests */
    NULL,                       /* suites */
    1,                          /* iterations */
    MUNIT_SUITE_OPTION_NONE,    /* options */
};

int main(int argc, char *const argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  setup_stack();

  /*
   * Solaris and OS X clobber the signal stack after returning to the
   * normal stack, so we need to inspect altstack while we're still
   * running on it.  Unfortunately, this means we risk clobbering the
   * buffer ourselves.
   *
   * To minimize this risk, test_with{,out}_memset() are responsible for
   * locating the offset of their buf variable within altstack, and
   * and returning that address.  Then we can simply memcmp() repeatedly
   * to count how many instances of secret we found.
   */

  return munit_suite_main(&suite, NULL, argc, argv);
}
