#include "user.h"

// -------------------------------------------------------
// Claymore Dungeon - a turn-based dungeon crawler
// Controls: w/a/s/d to move, f to fight, r to run, q to quit
// -------------------------------------------------------

#define MAP_W 16
#define MAP_H 8
#define MAX_ENEMIES 6
#define MAX_ITEMS 4

// --- tiny libc stubs ---
static int my_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s) {
    while (*s) putchar(*s++);
}

static void print_int(int n) {
    if (n < 0) { putchar('-'); n = -n; }
    if (n >= 10) print_int(n / 10);
    putchar('0' + n % 10);
}

static void newline(void) { putchar('\r'); putchar('\n'); }

static int my_rand_state = 12345;
static int my_rand(void) {
    my_rand_state ^= my_rand_state << 13;
    my_rand_state ^= my_rand_state >> 17;
    my_rand_state ^= my_rand_state << 5;
    int r = my_rand_state;
    if (r < 0) r = -r;
    return r;
}

// -------------------------------------------------------
// Map
// -------------------------------------------------------
// Tiles: ' '=floor, '#'=wall, '>'=exit
static char map[MAP_H][MAP_W + 1] = {
    "################",
    "#..............#",
    "#.###.#.####...#",
    "#.#...#....#...#",
    "#.#.###.##.#...#",
    "#...#......#...#",
    "#...########..>#",
    "################",
};

// -------------------------------------------------------
// Entities
// -------------------------------------------------------
struct Enemy {
    int x, y;
    int hp;
    int alive;
    char symbol;
    const char *name;
    int attack;
};

struct Item {
    int x, y;
    int collected;
    char symbol;
    const char *name;
    int heal;
};

struct Player {
    int x, y;
    int hp;
    int max_hp;
    int attack;
    int level;
    int xp;
    int xp_next;
    int floor;
};

static struct Player player;
static struct Enemy enemies[MAX_ENEMIES];
static struct Item items[MAX_ITEMS];

// -------------------------------------------------------
// Init
// -------------------------------------------------------
static void init_level(void) {
    // enemies
    struct { int x; int y; char sym; const char *name; int hp; int atk; } edata[MAX_ENEMIES] = {
        {3,  2, 'g', "Goblin",    8,  2},
        {7,  4, 'o', "Orc",      14,  4},
        {11, 2, 's', "Skeleton", 10,  3},
        {5,  5, 'r', "Rat",       5,  1},
        {13, 3, 'b', "Bat",       6,  2},
        {9,  5, 'T', "Troll",    20,  6},
    };
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].x      = edata[i].x;
        enemies[i].y      = edata[i].y;
        enemies[i].hp     = edata[i].hp + player.level * 2;
        enemies[i].alive  = 1;
        enemies[i].symbol = edata[i].sym;
        enemies[i].name   = edata[i].name;
        enemies[i].attack = edata[i].atk;
    }

    // items
    struct { int x; int y; char sym; const char *name; int heal; } idata[MAX_ITEMS] = {
        {2,  5, '!', "Health Potion", 8},
        {10, 1, '!', "Elixir",       15},
        {14, 5, '*', "Power Gem",     0},
        {6,  3, '!', "Tonic",         5},
    };
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].x         = idata[i].x;
        items[i].y         = idata[i].y;
        items[i].collected = 0;
        items[i].symbol    = idata[i].sym;
        items[i].name      = idata[i].name;
        items[i].heal      = idata[i].heal;
    }
}

static void init_game(void) {
    player.x       = 1;
    player.y       = 1;
    player.hp      = 20;
    player.max_hp  = 20;
    player.attack  = 5;
    player.level   = 1;
    player.xp      = 0;
    player.xp_next = 10;
    player.floor   = 1;
    init_level();
}

// -------------------------------------------------------
// Rendering
// -------------------------------------------------------
static void clear_screen(void) {
    // ANSI clear
    print("\033[2J\033[H");
}

static void draw_map(void) {
    for (int row = 0; row < MAP_H; row++) {
        for (int col = 0; col < MAP_W; col++) {
            // player
            if (col == player.x && row == player.y) {
                putchar('@');
                continue;
            }
            // enemies
            int drew = 0;
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (enemies[i].alive && enemies[i].x == col && enemies[i].y == row) {
                    putchar(enemies[i].symbol);
                    drew = 1;
                    break;
                }
            }
            if (drew) continue;
            // items
            for (int i = 0; i < MAX_ITEMS; i++) {
                if (!items[i].collected && items[i].x == col && items[i].y == row) {
                    putchar(items[i].symbol);
                    drew = 1;
                    break;
                }
            }
            if (drew) continue;
            putchar(map[row][col]);
        }
        newline();
    }
}

static void draw_hud(void) {
    print("Floor:"); print_int(player.floor);
    print("  HP:"); print_int(player.hp); putchar('/'); print_int(player.max_hp);
    print("  ATK:"); print_int(player.attack);
    print("  LVL:"); print_int(player.level);
    print("  XP:"); print_int(player.xp); putchar('/'); print_int(player.xp_next);
    newline();
    print("[w/a/s/d]move  [f]fight  [r]run  [q]quit");
    newline();
}

static const char *last_msg = "";

static void draw(void) {
    clear_screen();
    print("=== CLAYMORE DUNGEON  Floor ");
    print_int(player.floor);
    print(" ===");
    newline();
    draw_map();
    draw_hud();
    if (my_strlen(last_msg) > 0) {
        print(last_msg);
        newline();
    }
}

// -------------------------------------------------------
// Combat
// -------------------------------------------------------
static int enemy_at(int x, int y) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (enemies[i].alive && enemies[i].x == x && enemies[i].y == y)
            return i;
    return -1;
}

static void fight_enemy(int idx) {
    struct Enemy *e = &enemies[idx];

    // player hits enemy
    int pdmg = player.attack + my_rand() % 4;
    e->hp -= pdmg;

    if (e->hp <= 0) {
        e->alive = 0;
        int gained_xp = 5 + player.floor * 2;
        player.xp += gained_xp;
        last_msg = "You slew the enemy!";

        // level up
        if (player.xp >= player.xp_next) {
            player.level++;
            player.xp -= player.xp_next;
            player.xp_next = player.xp_next + 10;
            player.attack += 2;
            player.max_hp += 5;
            player.hp = player.max_hp;
            last_msg = "*** LEVEL UP! ***";
        }
        return;
    }

    // enemy hits back
    int edmg = e->attack + my_rand() % 3;
    player.hp -= edmg;
    last_msg = "You fought the enemy!";
}

// -------------------------------------------------------
// Movement & interaction
// -------------------------------------------------------
static int is_wall(int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 1;
    return map[y][x] == '#';
}

static void try_move(int dx, int dy) {
    int nx = player.x + dx;
    int ny = player.y + dy;

    if (is_wall(nx, ny)) {
        last_msg = "Blocked!";
        return;
    }

    int eidx = enemy_at(nx, ny);
    if (eidx >= 0) {
        fight_enemy(eidx);
        return;
    }

    player.x = nx;
    player.y = ny;
    last_msg = "";

    // pick up items
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!items[i].collected && items[i].x == nx && items[i].y == ny) {
            items[i].collected = 1;
            if (items[i].heal > 0) {
                player.hp += items[i].heal;
                if (player.hp > player.max_hp) player.hp = player.max_hp;
                last_msg = "Picked up a potion!";
            } else {
                player.attack += 3;
                last_msg = "Power Gem! ATK increased!";
            }
        }
    }
}

static void fight_adjacent(void) {
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};
    for (int d = 0; d < 4; d++) {
        int idx = enemy_at(player.x + dx[d], player.y + dy[d]);
        if (idx >= 0) {
            fight_enemy(idx);
            return;
        }
    }
    last_msg = "No enemy adjacent.";
}

// -------------------------------------------------------
// Next floor
// -------------------------------------------------------
static void next_floor(void) {
    player.floor++;
    player.x = 1;
    player.y = 1;
    player.hp += 5;
    if (player.hp > player.max_hp) player.hp = player.max_hp;
    init_level();
    last_msg = "You descend deeper...";
}

// -------------------------------------------------------
// Main loop
// -------------------------------------------------------
void main(void) {
    init_game();
    last_msg = "Find the exit '>' to descend. Walk into enemies to fight!";

    while (1) {
        draw();

        if (player.hp <= 0) {
            print("*** YOU DIED ***");
            newline();
            print("Press q to quit.");
            newline();
            char c = getchar();
            putchar(c);
            newline();
            exit();
        }

        // check on exit tile
        if (map[player.y][player.x] == '>') {
            next_floor();
            continue;
        }

        char ch = getchar();
        putchar(ch);
        newline();

        switch (ch) {
            case 'w': try_move(0, -1); break;
            case 's': try_move(0,  1); break;
            case 'a': try_move(-1, 0); break;
            case 'd': try_move( 1, 0); break;
            case 'f': fight_adjacent(); break;
            case 'r':
                // run — move back toward start
                last_msg = "You run away!";
                player.x = 1;
                player.y = 1;
                break;
            case 'q':
                print("Thanks for playing Claymore Dungeon!");
                newline();
                exit();
                break;
            default:
                last_msg = "Unknown command.";
                break;
        }
    }
}
