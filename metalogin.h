// metaloginh.h - MetaLogin Avatar Manager
// 1-Basic: Sample-3 Challenge

#ifndef METALOGIN_H
#define METALOGIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#define MAX_LENGTH  16
#define NUM_BLACK_SUN_MEMBERS 4
extern const char *black_sun_member_usernames[];
extern const char *black_sun_member_access_codes[];

// ============================================================================
// STRUCTURE DEFINITIONS
// ============================================================================
typedef struct avatar {
    void      *inventory;
    char      *access_code;
    char      *username;
    char      expansion_slot[MAX_LENGTH];
} avatar;

typedef struct start_loc {
    char      *location_name;
    uint8_t   coordinates[8];
} start_loc;

typedef struct item {
    long       inventory_obj;
    char      thingname[MAX_LENGTH];
    struct item  *next;
    int       internal_use_only;
} item;

typedef struct session {
    avatar    *current_avatar;
    char      *session_id;
    void      *inventory;
    int       is_active;
    int       is_blacksun_member;
    int       is_port;
    start_loc *start_loc;
} session;

// ============================================================================
// GLOBALS
// ============================================================================

extern session g_session;

void set_avatar(char *, char *);
void clear_avatar(void);
void print_card(void);

#endif
