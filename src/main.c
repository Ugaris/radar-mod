/*
 * Ugaris Radar & Nameplates
 *
 * Combat awareness: readable health bars over the characters around you,
 * level badges colored by how dangerous they are relative to you, a list
 * of players currently in view, and an alert (chat line + ping) the moment
 * another player enters your view.
 *
 * Commands:
 *   #radar          - Show status and help
 *   #radar bars     - Toggle health/shield bars over characters
 *   #radar full     - Toggle hiding bars of characters at full health
 *   #radar levels   - Toggle level badges
 *   #radar list     - Toggle the players-in-view list
 *   #radar alert    - Toggle player-entered-view alerts
 *   #radar sound    - Toggle the alert ping
 *
 * Settings persist in <client config dir>/radar_mod.cfg.
 *
 * Player detection uses the client's own rule (base player sprites).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#define _GNU_SOURCE 1
#include <dlfcn.h>
#endif

#include "amod/amod.h"

#define RADAR_VERSION "1.0.0"

#define COL_INFO  "\260c2"
#define COL_ALERT "\260c3"

#define C_BG     IRGB(2, 2, 4)
#define C_GOLD   IRGB(31, 26, 8)
#define C_GOLD2  IRGB(24, 18, 4)
#define C_YELLOW IRGB(31, 28, 2)
#define C_GREEN  IRGB(8, 26, 10)
#define C_RED    IRGB(28, 8, 6)

/* There is no is-a-player flag anywhere in the wire protocol: monsters and
 * players share the same character data, and even classic monsters (bears
 * are sprite 12) live in the low sprite range. The best available test is
 * the base-sprite rule; it treats some classic monsters as players, which
 * is why the list and the alerts ship disabled by default - enable them
 * where they shine (arenas, PvP areas, anywhere with few NPCs). A reliable
 * filter needs one is-player bit from the server. */
#define PLAYER_SPRITE_MAX 120

static int is_player_sprite(unsigned int csprite)
{
    return csprite < PLAYER_SPRITE_MAX;
}

/* ---------------------------------------------------------------- state */

static int s_ingame;
static unsigned int s_ticks;
static unsigned int s_quiet_until;   /* no alerts right after (area) login */
static unsigned int s_last_ping;
static int s_ping;                   /* sound handle, 0 = unavailable */

static int s_bars = 1;
static int s_hide_full = 1;
static int s_levels = 1;
static int s_list = 0;   /* see the is-a-player note above */
static int s_alert = 0;
static int s_sound = 1;

static unsigned int s_last_seen[MAXCHARS];
static unsigned int s_first_sight[MAXCHARS]; /* pending alert: waiting for the name */

struct seen_char {
    unsigned int cn;
    int wx, wy;
    int dist;
    int level;
    int is_player;
    unsigned char clan, pk;
};

/* ---------------------------------------------------------------- utils */

static int own_cn(void)
{
    map_index_t mn = mapmn(DIST, DIST);

    if (mn >= (map_index_t)MAXMN) return 0;
    return (int)map[mn].cn;
}

static int own_level(void)
{
    return exp2level((int)experience);
}

static void world_to_screen(int wx, int wy, int *sx, int *sy)
{
    int dx = wx - (int)originx;
    int dy = wy - (int)originy;
    int cx = (dotx(DOT_MTL) + dotx(DOT_MBR)) / 2;
    int cy = (doty(DOT_MTL) + doty(DOT_MBR)) / 2;

    *sx = cx + (dx - dy) * (FDX / 2);
    *sy = cy + (dx + dy) * (FDY / 2);
}

static unsigned short level_color(int lvl)
{
    int d = lvl - own_level();

    if (d >= 15) return C_RED;
    if (d >= 6) return IRGB(31, 20, 4);
    if (d >= -5) return IRGB(12, 26, 12);
    return graycolor;
}

static const char *pk_sign(unsigned char pk)
{
    switch (pk) {
    case 5: return " **";
    case 4: return " *";
    case 3: return " ++";
    case 2: return " +";
    case 1: return " -";
    default: return "";
    }
}

/* ---------------------------------------------------------------- config */

static void save_config(void)
{
    const char *dir = client_config_dir();
    char path[512];
    FILE *f;

    snprintf(path, sizeof(path), "%sradar_mod.cfg", dir && *dir ? dir : "");
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "bars=%d\nfull=%d\nlevels=%d\nlist=%d\nalert=%d\nsound=%d\n",
            s_bars, s_hide_full, s_levels, s_list, s_alert, s_sound);
    fclose(f);
}

static void load_config(void)
{
    const char *dir = client_config_dir();
    char path[512], line[64];
    FILE *f;

    snprintf(path, sizeof(path), "%sradar_mod.cfg", dir && *dir ? dir : "");
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int v = atoi(strchr(line, '=') ? strchr(line, '=') + 1 : "0");
        if (!strncmp(line, "bars=", 5)) s_bars = v;
        else if (!strncmp(line, "full=", 5)) s_hide_full = v;
        else if (!strncmp(line, "levels=", 7)) s_levels = v;
        else if (!strncmp(line, "list=", 5)) s_list = v;
        else if (!strncmp(line, "alert=", 6)) s_alert = v;
        else if (!strncmp(line, "sound=", 6)) s_sound = v;
    }
    fclose(f);
}

/* ----------------------------------------------------------------- scan */

static int scan_chars(struct seen_char *out, int max)
{
    unsigned int x, y;
    int n = 0;
    int self = own_cn();

    for (y = 0; y < MAPDY && n < max; y++) {
        for (x = 0; x < MAPDX && n < max; x++) {
            map_index_t mn = mapmn(x, y);
            unsigned int cn;

            if (mn >= (map_index_t)MAXMN) continue;
            if (!map[mn].csprite || !map[mn].cn) continue;
            if (!(map[mn].flags & CMF_VISIBLE) || !map[mn].rlight) continue;
            cn = map[mn].cn;
            if ((int)cn == self || cn >= MAXCHARS) continue;

            {
                struct seen_char *c = &out[n++];
                int dx, dy2;

                c->cn = cn;
                c->wx = (int)originx - (int)DIST + (int)x;
                c->wy = (int)originy - (int)DIST + (int)y;
                dx = abs(c->wx - (int)originx);
                dy2 = abs(c->wy - (int)originy);
                c->dist = dx > dy2 ? dx : dy2;
                c->is_player = is_player_sprite(map[mn].csprite);
                c->level = player[cn].level;
                c->clan = player[cn].clan;
                c->pk = player[cn].pk_status;
            }
        }
    }
    return n;
}

/* ----------------------------------------------------------------- bars */

static void draw_char_overlay(const struct seen_char *c)
{
    map_index_t mn;
    int sx, sy, hp_pct, sh_pct;
    int bx0, bx1, by;

    /* re-find the tile for the percentages */
    {
        int lx = c->wx - ((int)originx - (int)DIST);
        int ly = c->wy - ((int)originy - (int)DIST);
        mn = mapmn((unsigned int)lx, (unsigned int)ly);
        if (mn >= (map_index_t)MAXMN) return;
    }
    hp_pct = map[mn].health;
    sh_pct = map[mn].shield;

    if (s_hide_full && hp_pct >= 100 && !sh_pct) {
        if (!s_levels) return;
    }

    world_to_screen(c->wx, c->wy, &sx, &sy);
    sx += map[mn].xadd;
    sy += map[mn].yadd;

    /* above the native name/level stack */
    by = sy - 58;
    bx0 = sx - 14;
    bx1 = sx + 14;

    if (s_bars && !(s_hide_full && hp_pct >= 100 && !sh_pct)) {
        unsigned short col = hp_pct > 66 ? IRGB(6, 26, 8)
                           : hp_pct > 33 ? C_YELLOW
                                         : IRGB(30, 6, 4);
        int fill = (bx1 - bx0 - 2) * (hp_pct > 100 ? 100 : hp_pct) / 100;

        render_rect_alpha(bx0, by, bx1, by + 5, blackcolor, 190);
        render_rect_alpha(bx0 + 1, by + 1, bx0 + 1 + fill, by + 4, col, 235);
        if (sh_pct > 0) {
            int sfill = (bx1 - bx0 - 2) * (sh_pct > 100 ? 100 : sh_pct) / 100;
            render_rect_alpha(bx0 + 1, by + 6, bx0 + 1 + sfill, by + 8,
                              IRGB(10, 16, 30), 235);
        }
    }

    if (s_levels && c->level > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", c->level);
        render_text(bx1 + 4, by - 3, level_color(c->level),
                    RENDER_TEXT_SMALL | RENDER_TEXT_FRAMED, buf);
    }
}

/* ----------------------------------------------------------------- list */

static void draw_list(const struct seen_char *chars, int n)
{
    int players = 0, shown = 0, i;
    int x1 = dotx(DOT_MBR) - 10;
    int y = doty(DOT_MTL) + 26;
    int w = 0, rows;
    char buf[128];
    unsigned char own_clan = 0;

    if (own_cn() > 0 && own_cn() < MAXCHARS) own_clan = player[own_cn()].clan;

    for (i = 0; i < n; i++)
        if (chars[i].is_player) players++;
    if (!players) return;

    rows = players > 10 ? 10 : players;

    /* measure widest row */
    for (i = 0; i < n && shown < rows; i++) {
        int tw;
        if (!chars[i].is_player) continue;
        snprintf(buf, sizeof(buf), "%s%s  %d  %d",
                 player[chars[i].cn].name[0] ? player[chars[i].cn].name : "Someone",
                 pk_sign(chars[i].pk), chars[i].level, chars[i].dist);
        tw = render_text_length(RENDER_TEXT_SMALL, buf);
        if (tw > w) w = tw;
        shown++;
    }
    w += 20;
    if (w < 120) w = 120;

    render_rounded_rect_filled_alpha(x1 - w, y - 4, x1, y + 14 + rows * 13 + (players > rows ? 13 : 0), 6, C_BG, 205);
    render_rounded_rect_alpha(x1 - w, y - 4, x1, y + 14 + rows * 13 + (players > rows ? 13 : 0), 6, C_GOLD2, 120);
    snprintf(buf, sizeof(buf), "players in view: %d", players);
    render_text(x1 - w + 8, y, C_GOLD, RENDER_TEXT_SMALL, buf);
    y += 14;

    for (i = 0, shown = 0; i < n && shown < rows; i++) {
        unsigned short col = whitecolor;
        if (!chars[i].is_player) continue;
        if (own_clan && chars[i].clan == own_clan) col = IRGB(12, 28, 14);
        else if (chars[i].pk >= 4) col = IRGB(31, 12, 8);
        snprintf(buf, sizeof(buf), "%s%s  %d  %d",
                 player[chars[i].cn].name[0] ? player[chars[i].cn].name : "Someone",
                 pk_sign(chars[i].pk), chars[i].level, chars[i].dist);
        render_text(x1 - w + 8, y, col, RENDER_TEXT_SMALL, buf);
        y += 13;
        shown++;
    }
    if (players > rows) {
        snprintf(buf, sizeof(buf), "+%d more", players - rows);
        render_text(x1 - w + 8, y, graycolor, RENDER_TEXT_SMALL, buf);
    }
}

/* ---------------------------------------------------------------- alerts */

static void check_alerts(const struct seen_char *chars, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        unsigned int cn = chars[i].cn;
        int is_new;

        if (!chars[i].is_player) continue;
        is_new = (s_last_seen[cn] + 240 < s_ticks);

        if (is_new && s_ticks >= s_quiet_until) {
            /* The name arrives from the server a moment after the character
             * does; hold the alert briefly so it can use the real name. */
            if (!s_first_sight[cn]) s_first_sight[cn] = s_ticks;
            if (player[cn].name[0] || s_ticks > s_first_sight[cn] + 24) {
                if (s_alert) {
                    addline(COL_ALERT "Radar: %s (level %d) entered view, %d away",
                            player[cn].name[0] ? player[cn].name : "Someone",
                            player[cn].level, chars[i].dist);
                    if (s_sound && s_ping && s_ticks > s_last_ping + 48) {
                        sound_play(s_ping, 0.45f);
                        s_last_ping = s_ticks;
                    }
                }
                s_first_sight[cn] = 0;
                s_last_seen[cn] = s_ticks;
            }
            /* else: keep pending until the name arrives (or the grace ends) */
        } else {
            s_first_sight[cn] = 0;
            s_last_seen[cn] = s_ticks;
        }
    }
}

/* ------------------------------------------------------------- mod hooks */

DLL_EXPORT char *amod_version(void)
{
    return "Radar & Nameplates " RADAR_VERSION;
}

DLL_EXPORT void amod_init(void)
{
}

DLL_EXPORT void amod_exit(void)
{
}

DLL_EXPORT void amod_gamestart(void)
{
    s_ingame = 1;
    s_ticks = 0;
    s_quiet_until = 3 * 24;
    memset(s_last_seen, 0, sizeof(s_last_seen));
    memset(s_first_sight, 0, sizeof(s_first_sight));
    load_config();
    s_ping = sound_load("029_magic.wav");
}

DLL_EXPORT void amod_areachange(void)
{
    /* everyone "enters view" on a transfer; stay quiet for a moment */
    s_quiet_until = s_ticks + 2 * 24;
}

DLL_EXPORT void amod_tick(void)
{
    struct seen_char chars[128];
    int n;

    if (!s_ingame) return;
    s_ticks++;

    if (s_ticks == 24)
        addline("Radar & Nameplates %s loaded. Type #radar for options.", RADAR_VERSION);

    n = scan_chars(chars, 128);
    check_alerts(chars, n);
}

DLL_EXPORT void amod_frame(void)
{
    struct seen_char chars[128];
    int n, i;

    if (!s_ingame) return;
    if (!s_bars && !s_levels && !s_list) return;

    n = scan_chars(chars, 128);
    if (s_bars || s_levels) {
        for (i = 0; i < n; i++) draw_char_overlay(&chars[i]);
    }
    if (s_list) draw_list(chars, n);
}

static int toggle(int *setting, const char *name)
{
    *setting = !*setting;
    addline(COL_INFO "Radar: %s %s", name, *setting ? "on" : "off");
    save_config();
    return 1;
}

DLL_EXPORT int amod_client_cmd(const char *buf)
{
    if (strncmp(buf, "#radar", 6)) return 0;
    buf += 6;
    while (*buf == ' ') buf++;

    if (!*buf || !strcmp(buf, "help")) {
        addline(COL_INFO "Radar & Nameplates %s - bars:%s full:%s levels:%s list:%s alert:%s sound:%s",
                RADAR_VERSION, s_bars ? "on" : "off", s_hide_full ? "on" : "off",
                s_levels ? "on" : "off", s_list ? "on" : "off",
                s_alert ? "on" : "off", s_sound ? "on" : "off");
        addline("#radar bars/full/levels/list/alert/sound toggles a feature");
        return 1;
    }
    if (!strcmp(buf, "bars")) return toggle(&s_bars, "health bars");
    if (!strcmp(buf, "full")) return toggle(&s_hide_full, "hide full-health bars");
    if (!strcmp(buf, "levels")) return toggle(&s_levels, "level badges");
    if (!strcmp(buf, "list")) return toggle(&s_list, "player list");
    if (!strcmp(buf, "alert")) return toggle(&s_alert, "enter-view alerts");
    if (!strcmp(buf, "sound")) return toggle(&s_sound, "alert ping");
    addline(COL_INFO "Unknown #radar option. Try #radar help");
    return 1;
}
