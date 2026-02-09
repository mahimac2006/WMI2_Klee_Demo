#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[stub_wmi2.sh] Writing stubs_wmi2.c (libc + io stubs for WMI-2)"

cat > "$HERE/stubs_wmi2.c" <<'EOF'
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __KLEE__
  #include "klee/klee.h"
#endif

int printf(const char *fmt, ...) { (void)fmt; return 0; }
int puts(const char *s) { (void)s; return 0; }
int putchar(int c) { (void)c; return c; }

void *stdin;

int getchar(void) { return -1; }

/* WMI-2: set_start_location() needs real input so it allocates and can reuse
   freed avatar memory. Return a fixed string so set_start_location gets "127"
   and performs malloc(start_loc) + malloc(location_name). */
char *fgets(char *s, int size, void *stream) {
  (void)stream;
  if (!s || size < 5) return NULL;
  const char *in = "127\n";
  int i = 0;
  for (; i < size - 1 && in[i]; i++) s[i] = in[i];
  s[i] = '\0';
  return s;
}

int __isoc99_scanf(const char *fmt, ...) { (void)fmt; return 0; }

size_t strlen(const char *s) {
  if (!s) return 0;
  size_t n = 0;
  while (s[n]) n++;
  return n;
}

size_t strnlen(const char *s, size_t maxlen) {
  if (!s) return 0;
  size_t i = 0;
  for (; i < maxlen && s[i]; i++) {}
  return i;
}

size_t strcspn(const char *s, const char *reject) {
  if (!s || !reject) return 0;
  for (size_t i = 0; s[i]; i++) {
    for (size_t j = 0; reject[j]; j++) {
      if (s[i] == reject[j]) return i;
    }
  }
  return strlen(s);
}

int strcmp(const char *a, const char *b) {
  if (a == b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  while (*a && (*a == *b)) { a++; b++; }
  return (unsigned char)*a - (unsigned char)*b;
}

char *strncpy(char *dst, const char *src, size_t n) {
  if (!dst || !src) return dst;
  size_t i = 0;
  for (; i < n && src[i]; i++) dst[i] = src[i];
  for (; i < n; i++) dst[i] = 0;
  return dst;
}

char *strdup(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);
  if (!p) return NULL;
  for (size_t i = 0; i < n; i++) p[i] = s[i];
  return p;
}

long strtol(const char *nptr, char **endptr, int base) {
  (void)base;
  if (!nptr) { if (endptr) *endptr = (char*)nptr; return 0; }
  long v = 0;
  const char *p = nptr;
  while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
  if (endptr) *endptr = (char*)p;
  return v;
}

void *memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < n; i++) d[i] = s[i];
  return dst;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  unsigned char u = (unsigned char)c;
  for (size_t i = 0; i < n; i++) p[i] = u;
  return s;
}

double sin(double x) { (void)x; return 0.0; }
double cos(double x) { (void)x; return 0.0; }
long long llround(double x) { (void)x; return 0; }

void print_card(void) {}

void exit(int code) {
  (void)code;
#ifdef __KLEE__
  klee_silent_exit(0);
#else
  abort();
#endif
}
EOF

echo "[stub_wmi2.sh] Done."
