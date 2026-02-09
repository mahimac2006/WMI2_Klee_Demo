/*
 * WMI-2 (Leak via Type Confusion) — KLEE driver
 *
 * Model: After set_avatar → clear_avatar, g_session.current_avatar is a stale
 * pointer. We call set_start_location() which allocates; the allocator may
 * reuse the freed avatar chunk. We then read the "username" field (offset 16)
 * from the stale pointer. If that memory was overwritten with a heap pointer
 * (e.g. from a new allocation), we have an information leak: a pointer value
 * is observable as if it were user data.
 *
 * Assertion: The 8-byte value at (current_avatar + offsetof(avatar, username))
 * must NOT be a valid heap address. If it is, we report a leak (assert fails).
 *
 */
#include <stdint.h>
#include <stddef.h>

#ifdef __KLEE__
  #include "klee/klee.h"
#else
  #define klee_make_symbolic(a,b,c) ((void)0)
  #define klee_assume(x) ((void)0)
  #define klee_assert(x) ((void)0)
#endif

#include "metalogin.h"

#define HEAP_START  0x77fe00000000ULL
#define HEAP_END    0x78000000000ULL

static int is_in_heap_range(uintptr_t value) {
  return (value >= HEAP_START && value <= HEAP_END);
}

void init_system(void);
void set_avatar(char *username, char *access_code);
void clear_avatar(void);
void set_start_location(void);

extern session g_session;

int main(void) {
  init_system();

  char username[MAX_LENGTH];
  char access_code[MAX_LENGTH];

#ifdef __KLEE__
  klee_make_symbolic(username, sizeof(username), "username");
  klee_make_symbolic(access_code, sizeof(access_code), "access_code");
  username[sizeof(username) - 1] = 0;
  access_code[sizeof(access_code) - 1] = 0;
  klee_assume((unsigned char)username[0] != 0);
#endif

  /* WMI-2 path: create avatar, free it (stale ref), then allocate to encourage reuse */
  set_avatar(username, access_code);
  clear_avatar();
  set_start_location();   /* uses fgets stub that returns "127" → allocates start_loc + location_name */

  /* Read the "username" field from the stale avatar. If the chunk was reused,
   * this may now hold a heap pointer (type confusion / leak). */
  if (g_session.current_avatar) {
    uintptr_t leaked = *(uintptr_t *)((char *)g_session.current_avatar + offsetof(avatar, username));
#ifdef __KLEE__
    /* Fail when we observe a heap pointer in "user" data → information leak */
    klee_assert(!is_in_heap_range(leaked));
#endif
  }

  return 0;
}
