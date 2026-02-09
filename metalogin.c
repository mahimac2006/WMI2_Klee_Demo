// metalogin.c - MetaLogin Avatar Manager
// 1-Basic: Sample-3 Challenge
#include "metalogin.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================
session g_session = {0};
typedef void (*func_t)(void);
func_t *render_functions;

// ============================================================================
// INVENTORY MANAGEMENT
// ============================================================================
static item *item_create(const char *name, long inventory_obj) {
    item *it = (item*)malloc(sizeof(*it));
    if (!it) {
        printf("[ERROR] Memory allocation failed\n");
        exit(0);
    }
    memset(it->thingname, 0, MAX_LENGTH);
    strncpy(it->thingname, name, MAX_LENGTH-1);
    it->inventory_obj = inventory_obj;
    it->next = NULL;
    return it;
}

static void item_free(item *it) {
    if (!it) return;
    free(it);
}

int items_add(item **headp, const char *name, long inventory_obj) {
    if (!headp) return -EINVAL;
    item *it = item_create(name, inventory_obj);
    if (!it) return -ENOMEM;
    it->next = *headp;
    *headp = it;
    return 0;
}

int items_remove_by_obj(item **headp, long inventory_obj) {
    if (!headp) return -EINVAL;
    item *prev = NULL, *cur = *headp;
    while (cur) {
        if (cur->inventory_obj == inventory_obj) {
            if (prev) prev->next = cur->next;
            else *headp = cur->next;
            item_free(cur);
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

void items_clear_all(item **headp) {
    if (!headp) return;
    item *cur = *headp;
    while (cur) {
        item *next = cur->next;
        item_free(cur);
        cur = next;
    }
    *headp = NULL;
}

/* ---------- inventory wrappers ---------- */
static item **inventory_headp(void) {
    return g_session.is_active ? (item **)&g_session.current_avatar->inventory : (item **)&g_session.inventory;
}

int inventory_add(const char *name, long inventory_obj) {
    item **headp = inventory_headp();
    if (!headp) return -EINVAL;
    return items_add(headp, name, inventory_obj);
}

int inventory_remove_by_obj(long inventory_obj) {
    item **headp = inventory_headp();
    if (!headp) return -EINVAL;
    return items_remove_by_obj(headp, inventory_obj);
}

/* ---------- top level inventory calls ---------- */
void add_item_from_user(void) {
    char name[16];
    long  num;

    printf("Enter item name (max 15 chars): ");
    if (!fgets(name, sizeof(name), stdin))
        return;
    name[strcspn(name, "\n")] = '\0';

    printf("Enter item number: ");
    if (scanf("%ld", &num) != 1) {
        printf("[ERROR] Invalid number.\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    if (inventory_add(name, num) == 0)
        printf("[SYSTEM] Item added.\n");
    else
        printf("[ERROR] Failed to add item.\n");
}

void remove_item_from_user(void) {
    int num;

    printf("Enter item number to remove: ");
    if (scanf("%d", &num) != 1) {
        printf("Invalid number.\n");
        while (getchar() != '\n');
        return;
    }
    while (getchar() != '\n');

    int result = inventory_remove_by_obj(num);
    if (result > 0)
        printf("[SYSTEM] Item removed.\n");
    else if (result == 0)
        printf("[ERROR] Item not found.\n");
    else
        printf("[ERROR] Error removing item.\n");
}

void inventory_clear_all(void) {
    if (!g_session.current_avatar)
        return;
    if (g_session.current_avatar->inventory)
        items_clear_all(g_session.current_avatar->inventory);
}

void view_inventory(void) {
    item *cur = NULL;
    if (g_session.is_active)
        cur = g_session.current_avatar->inventory;
    else
        cur = g_session.inventory;
    if (!cur) {
        printf("[SYSTEM] Inventory is empty.\n");
        return;
    }
    printf("=== Current Inventory ===\n");
    while (cur) {
        printf("Name: %-15s | ID: %ld\n",
               cur->thingname ? cur->thingname : "(null)",
               cur->inventory_obj);
        cur = cur->next;
    }
}

// ============================================================================
// START LOCATION MANAGEMENT
// ============================================================================
static void be_store_u32(uint8_t out[4], uint32_t v) {
    out[0] = (uint8_t)((v >> 24) & 0xFF);
    out[1] = (uint8_t)((v >> 16) & 0xFF);
    out[2] = (uint8_t)((v >>  8) & 0xFF);
    out[3] = (uint8_t)( v        & 0xFF);
}

static int parse_u8_strict(const char *s, uint8_t *out_val) {
    if (!s || !*s) return 0;
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0) return 0;
    if (*end != '\0') return 0;
    if (v < 0 || v > 255) return 0;
    *out_val = (uint8_t)v;
    return 1;
}

void set_start_location(void) {
    char user_input[MAX_LENGTH];
    if (g_session.start_loc != NULL) {
        printf("[ERROR] Start Location already set - clear it first\n");
        return;
    }

    printf("Enter Start Location Name: ");
    if (fgets(user_input, sizeof(user_input), stdin)) {
        user_input[strcspn(user_input, "\n")] = '\0';
    }
    size_t len = strnlen(user_input, MAX_LENGTH-1);
    if (!len){
        printf("[ERROR] You didn't enter anything\n");
        return;
    }
    g_session.start_loc = malloc(sizeof(start_loc));
    if (!g_session.start_loc) {
        printf("[ERROR] Memory allocation failed\n");
        return;
    }
    g_session.start_loc->location_name = malloc(len + 1);
    if (!g_session.start_loc->location_name) {
        printf("[ERROR] Memory allocation failed\n");
        free(g_session.start_loc);
        g_session.start_loc = NULL;
        return;
    }
    memcpy(g_session.start_loc->location_name, user_input, len + 1);

    // If user_input is an integer 0..255, place on the Street ring
    uint8_t port_idx;
    if (parse_u8_strict(user_input, &port_idx)) {
        const double C = 65536.0;
        const double R = C / (2.0 * M_PI);
        const double theta = (2.0 * M_PI) * ((double)port_idx / 256.0);
        const double xd = R * cos(theta);
        const double yd = R * sin(theta);
        const double SCALE = 65536.0;
        int32_t fx = (int32_t)llround(xd * SCALE);
        int32_t fy = (int32_t)llround(yd * SCALE);
        uint32_t ux = (uint32_t)((uint32_t)fx + 0x80000000u);
        uint32_t uy = (uint32_t)((uint32_t)fy + 0x80000000u);
        be_store_u32(&g_session.start_loc->coordinates[0], ux);
        be_store_u32(&g_session.start_loc->coordinates[4], uy);
        g_session.is_port = 1;
    } else {
        g_session.is_port = 0;
        memset(g_session.start_loc->coordinates, 0, sizeof(g_session.start_loc->coordinates));
    }
}

void clear_start_location(void) {
    if (!g_session.start_loc) {
        printf("[ERROR] No start_loc to clear\n");
        return;
    }
    g_session.is_port = 0;
    free(g_session.start_loc);
    g_session.start_loc = NULL;
}

// ============================================================================
// AVATAR MANAGEMENT
// ============================================================================
int verify_black_sun_member(const char *username, const char *access_code) {
    for (int i = 0; i < NUM_BLACK_SUN_MEMBERS; i++) {
        if (strcmp(username, black_sun_member_usernames[i]) == 0) {
            if (strcmp(access_code, black_sun_member_access_codes[i]) == 0) {
                return 1;
            } else {
                return -1;
            }
        }
    }
    return 0;
}

void free_avatar_and_components(avatar *av) {
    if (!av) return;
    if (av->access_code)
        free(av->access_code);
    if (av->username)
        free(av->username);
    inventory_clear_all();
    av->inventory = NULL;
    free(av);
}

void set_avatar(char *username, char *access_code) {
    if (g_session.is_active == 0x1337) {
        g_session.is_blacksun_member = 0;
        clear_avatar();
    }

    avatar *av = malloc(sizeof(avatar));
    if (!av) {
        printf("[ERROR] Memory allocation failed\n");
        exit(0);
    }

    if (username) {
        av->username = malloc(MAX_LENGTH);
        if (!av->username) {
            free_avatar_and_components(av);
            printf("[ERROR] Memory allocation failed\n");
            exit(0);
        }
        memset(av->username, 0, MAX_LENGTH);
        strncpy(av->username, username, MAX_LENGTH-1);
    }

    if (access_code) {
        av->access_code = malloc(MAX_LENGTH);
        if (!av->access_code) {
            free_avatar_and_components(av);
            printf("[ERROR] Memory allocation failed\n");
            exit(0);
        }
        memset(av->access_code, 0, MAX_LENGTH);
        strncpy(av->access_code, access_code, MAX_LENGTH-1);
    }

    int status = verify_black_sun_member(username, access_code);
    if (status == -1) {
        printf("[ACCESS DENIED] Incorrect access code for Black Sun member\n");
        free_avatar_and_components(av);
        return;
    } else if (status == 1) {
        printf("[ACCESS GRANTED] Black Sun member verified\n");
        g_session.is_blacksun_member = 0x1337;
    } else {
        printf("[SYSTEM] Welcome User - %s\n", username);
    }

    if (g_session.inventory)
        av->inventory = g_session.inventory;
    else
        av->inventory = NULL;

    g_session.current_avatar = av;
    g_session.is_active = 0x1337;

    printf("[SYSTEM] Avatar '%s' loaded successfully\n", username);
}

void clear_avatar(void) {
    if (g_session.is_active == 0) {
        printf("[SYSTEM] No avatar currently set\n");
        return;
    }
    avatar *av = g_session.current_avatar;
    if (!av) {
        printf("[SYSTEM] No avatar currently set\n");
        return;
    }
    printf("\n[SYSTEM] Removing avatar '%s'\n", av->username);
    free_avatar_and_components(av);
    clear_start_location();
}

// ============================================================================
// RENDER CALLBACKS
// ============================================================================
void render_ascii(void) {
    avatar *av = g_session.current_avatar;
    if (!av) {
        printf("[RENDER] No avatar loaded\n");
        return;
    }
    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║     METAVERSE AVATAR (Standard)           ║\n");
    printf("╠═══════════════════════════════════════════╣\n");
    printf("║ Username: ");
    if (av->username) {
        for (int i = 0; i < 16 && av->username[i] != '\0'; i++) {
            unsigned char c = av->username[i];
            if (c >= 0x20 && c <= 0x7E) putchar(c);
        }
    } else {
        printf("NULL");
    }
    printf("%*s║\n", (int)(32 - strnlen(av->username, MAX_LENGTH-1)), "");
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");
}

void print16_hex(const char *buf) {
    const unsigned char *b = (const unsigned char *)buf;
    printf("║ ");
    for (int i = 0; i < 16; i++) {
        printf("%02X", b[i]);
        if (i == 7) printf(" | ");
        else if (i != 15) printf(" ");
    }
    printf(" ║ \n");
}

void render_hex(void) {
    avatar *av = g_session.current_avatar;
    if (!av) {
        printf("[RENDER] No avatar loaded\n");
        return;
    }
    printf("\n");
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║     METAVERSE AVATAR (Black Sun)                  ║\n");
    printf("╠═══════════════════════════════════════════════════╣\n");
    printf("║ LEET USER (can read hex):                         ║\n");
    printf("╠═══════════════════════════════════════════════════╣\n");
    if (av->username) {
        print16_hex(av->username);
        printf("╠═══════════════════════════════════════════════════╣\n");
        if (g_session.start_loc) {
            start_loc *sl = g_session.start_loc;
            if (sl) {
                printf("║ Here's where you're headed:                       ║\n");
                if (g_session.is_port) {
                    printf("║   Port: %-41s ║\n", sl->location_name);
                    uint32_t ux = (sl->coordinates[0] << 24) | (sl->coordinates[1] << 16) |
                                (sl->coordinates[2] << 8)  |  sl->coordinates[3];
                    uint32_t uy = (sl->coordinates[4] << 24) | (sl->coordinates[5] << 16) |
                                (sl->coordinates[6] << 8)  |  sl->coordinates[7];
                    printf("║   Coordinates -> X=0x%08X Y=0x%08X        ║\n", ux, uy);
                } else {
                    printf("║   Name: %-41s ║\n", sl->location_name);
                }
            }
        }
    } else {
        printf("║ NULL                                     ║\n");
    }
    printf("╚═══════════════════════════════════════════════════╝\n");
    printf("\n");
}

void clear_screen(void) {
    printf("\033[2J\033[H");
}

void wait_enter(const char *msg) {
    printf("%s", msg);
    while (getchar() != '\n');
}

static bool hex_nibble(char c, uint8_t *out) {
    if ('0' <= c && c <= '9') { *out = (uint8_t)(c - '0'); return true; }
    c = (char)tolower((unsigned char)c);
    if ('a' <= c && c <= 'f') { *out = (uint8_t)(10 + c - 'a'); return true; }
    return false;
}

static const char *skip_0x(const char *s) {
    return (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? s + 2 : s;
}

void show_menu(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("MENU:\n");
    printf("  1. Set Avatar\n");
    printf("  2. Clear Avatar\n");
    printf("  3. Set Start Location\n");
    printf("  4. Clear Start Location\n");
    printf("  5. Add Item to Inventory\n");
    printf("  6. Remove Item from Inventory\n");
    printf("  7. Show Inventory\n");
    printf("  8. Clear Inventory\n");
    printf("  9. Test Render\n");
    printf("  0. Exit\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Choice: ");
}

void test_render(void) {
    if (g_session.is_blacksun_member) {
        render_functions[0]();
    } else {
        render_functions[1]();
    }
}

void print_banner(void) {
    clear_screen();
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("   METALOGIN v0.7.3 - Avatar Authentication System\n");
    printf("   Black Sun Terminal #4471\n");
    printf("   *Legacy system - use at own risk*\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}

void init_system(void) {
    print_banner();
    printf("[BOOT] Initializing MetaLogin system...\n");
    g_session.current_avatar = NULL;
    g_session.session_id = strdup("SESSION-4471");
    g_session.is_active = 0;
    g_session.is_blacksun_member = 0;

    render_functions = calloc(4, sizeof(func_t));
    if (render_functions == (func_t *)NULL) {
        printf("[ERROR] system out of resources");
        exit(137);
    }
    render_functions[0]=test_render;
    render_functions[1]=print_card;
    printf("[LIBRARIAN] Maybe this will help(%p)\n", print_card);
    render_functions[0]=render_hex;
    render_functions[1]=render_ascii;
}

#ifndef KLEE_DRIVER_BUILD
int main(void) {
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    init_system();
    int choice;
    while (1) {
        show_menu();
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("[ERROR] Invalid input\n");
            continue;
        }
        while (getchar() != '\n');
        switch (choice) {
            case 1: {
                char username[16] = {0};
                char access_code[16] = {0};
                printf("Enter username: ");
                if (fgets(username, sizeof(username), stdin))
                    username[strcspn(username, "\n")] = 0;
                printf("Enter access code: ");
                if (fgets(access_code, sizeof(access_code), stdin))
                    access_code[strcspn(access_code, "\n")] = 0;
                set_avatar(username, access_code);
                break;
            }
            case 2: clear_avatar(); break;
            case 3: set_start_location(); break;
            case 4: clear_start_location(); break;
            case 5: add_item_from_user(); break;
            case 6: remove_item_from_user(); break;
            case 7: view_inventory(); break;
            case 8: inventory_clear_all(); break;
            case 9: test_render(); break;
            case 0: printf("[SYSTEM] Shutting down...\n"); return 0;
            default: printf("[ERROR] Invalid choice\n");
        }
    }
    return 0;
}
#endif
