/*
 * Dino Run - a tiny Chrome-dino style runner for PS3 (PSL1GHT)
 *
 * Controls:
 *   X (cross)  - jump / start
 *   START      - start / restart
 *
 * Everything is drawn with RSX scissor + clear, so no shaders are needed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include <rsx/rsx.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>

#include "rsxutil.h"

/* Logical game resolution, scaled up to the real display */
#define LW          640
#define LH          360
#define GROUND_Y    290
#define DINO_X      60
#define DINO_H      30
#define MAX_OBS     4
#define GRAVITY     0.7f
#define JUMP_VEL    (-12.5f)

#define COL_BLACK   0x00000000
#define COL_BG      0x00F7F7F7
#define COL_INK     0x00535353
#define COL_CACTUS  0x001E7B34

enum { ST_TITLE, ST_PLAY, ST_OVER };

typedef struct {
    int x, y, w, h;
    int active;
} Obstacle;

typedef struct {
    char ch;
    const char *rows[7];   /* 5 pixels wide, 7 pixels tall */
} Glyph;

static const Glyph font[] = {
    { '0', { " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### " } },
    { '1', { "  #  ", " ##  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " } },
    { '2', { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" } },
    { '3', { "#### ", "    #", "    #", " ### ", "    #", "    #", "#### " } },
    { '4', { "   # ", "  ## ", " # # ", "#  # ", "#####", "   # ", "   # " } },
    { '5', { "#####", "#    ", "#### ", "    #", "    #", "#   #", " ### " } },
    { '6', { "  ## ", " #   ", "#    ", "#### ", "#   #", "#   #", " ### " } },
    { '7', { "#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   " } },
    { '8', { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " } },
    { '9', { " ### ", "#   #", "#   #", " ####", "    #", "   # ", " ##  " } },
    { 'A', { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" } },
    { 'D', { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " } },
    { 'E', { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" } },
    { 'G', { " ### ", "#   #", "#    ", "# ###", "#   #", "#   #", " ####" } },
    { 'H', { "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" } },
    { 'I', { " ### ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", " ### " } },
    { 'M', { "#   #", "## ##", "# # #", "# # #", "#   #", "#   #", "#   #" } },
    { 'N', { "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #" } },
    { 'O', { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " } },
    { 'P', { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " } },
    { 'R', { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" } },
    { 'S', { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " } },
    { 'T', { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " } },
    { 'U', { "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " } },
    { 'V', { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " } },
    { ' ', { "     ", "     ", "     ", "     ", "     ", "     ", "     " } },
};

static int scale = 1;
static int ox = 0, oy = 0;

static int quit_flag = 0;
static int state = ST_TITLE;

static float dino_y = GROUND_Y - DINO_H;
static float dino_vy = 0.0f;
static int on_ground = 1;

static Obstacle obs[MAX_OBS];
static int spawn_timer = 60;
static float speed = 5.0f;

static int frame = 0;          /* advances only while playing */
static int ticks = 0;          /* advances every drawn frame (for blinking) */
static int score = 0;
static int hi_score = 0;

static int prev_cross = 0, prev_start = 0;
static int jump_edge = 0, start_edge = 0;

/* ---------- system ---------- */

static void sysutil_exit_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        quit_flag = 1;
}

/* ---------- drawing primitives ---------- */

static void fill_rect(int x, int y, int w, int h, u32 color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LW) w = LW - x;
    if (y + h > LH) h = LH - y;
    if (w <= 0 || h <= 0)
        return;

    rsxSetScissor(context, ox + x * scale, oy + y * scale, w * scale, h * scale);
    rsxSetClearColor(context, color);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
}

static void clear_display(u32 color)
{
    rsxSetScissor(context, 0, 0, display_width, display_height);
    rsxSetClearColor(context, color);
    rsxClearSurface(context, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);
}

static const char *const *find_glyph(char c)
{
    size_t i;
    for (i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
        if (font[i].ch == c)
            return font[i].rows;
    }
    return NULL;
}

static int text_width(const char *s, int px)
{
    int n = (int)strlen(s);
    if (n == 0)
        return 0;
    return n * 6 * px - px;
}

static void draw_text(int x, int y, const char *s, int px, u32 color)
{
    for (; *s; s++) {
        const char *const *g = find_glyph(*s);
        if (g) {
            int r, c;
            for (r = 0; r < 7; r++)
                for (c = 0; c < 5; c++)
                    if (g[r][c] == '#')
                        fill_rect(x + c * px, y + r * px, px, px, color);
        }
        x += 6 * px;
    }
}

static void draw_text_centered(int y, const char *s, int px, u32 color)
{
    draw_text((LW - text_width(s, px)) / 2, y, s, px, color);
}

/* ---------- sprites ---------- */

static void draw_dino(int x, int y, int anim, int airborne)
{
    fill_rect(x,      y + 10, 20, 14, COL_INK);   /* body */
    fill_rect(x + 10, y,      14, 12, COL_INK);   /* head */
    fill_rect(x + 19, y + 3,  3,  3,  COL_BG);    /* eye  */
    fill_rect(x - 5,  y + 12, 6,  5,  COL_INK);   /* tail */

    /* legs alternate while running, both tucked while airborne */
    if (airborne || ((anim / 6) % 2 == 0))
        fill_rect(x + 4,  y + 24, 5, 6, COL_INK);
    if (airborne || ((anim / 6) % 2 == 1))
        fill_rect(x + 12, y + 24, 5, 6, COL_INK);
}

static void draw_cactus(const Obstacle *o)
{
    int x = o->x, y = o->y, w = o->w, h = o->h;

    fill_rect(x, y, w, h, COL_CACTUS);                             /* trunk */
    fill_rect(x - 5, y + h / 2 - 4, 5, 4, COL_CACTUS);             /* left arm  */
    fill_rect(x - 5, y + h / 2 - 12, 4, 12, COL_CACTUS);
    fill_rect(x + w, y + h / 3 - 4, 5, 4, COL_CACTUS);             /* right arm */
    fill_rect(x + w + 1, y + h / 3 - 12, 4, 12, COL_CACTUS);
}

static void draw_hud(void)
{
    char buf[32];

    sprintf(buf, "HI %05d", hi_score);
    draw_text(LW - 230, 10, buf, 2, COL_INK);

    sprintf(buf, "%05d", score);
    draw_text(LW - 110, 10, buf, 2, COL_INK);
}

/* ---------- game logic ---------- */

static void reset_game(void)
{
    int i;

    state = ST_PLAY;
    dino_y = GROUND_Y - DINO_H;
    dino_vy = 0.0f;
    on_ground = 1;

    for (i = 0; i < MAX_OBS; i++)
        obs[i].active = 0;

    spawn_timer = 60;
    speed = 5.0f;
    frame = 0;
    score = 0;
}

static void spawn_obstacle(void)
{
    int i;

    for (i = 0; i < MAX_OBS; i++)
        if (!obs[i].active)
            break;
    if (i == MAX_OBS)
        return;

    obs[i].w = 12 + rand() % 10;
    obs[i].h = 26 + rand() % 16;
    obs[i].x = LW + rand() % 40;
    obs[i].y = GROUND_Y - obs[i].h;
    obs[i].active = 1;
}

static void update_playing(void)
{
    int i;
    int dl, dr, dt, db;

    frame++;
    score = frame / 6;

    speed += 0.0015f;
    if (speed > 13.0f)
        speed = 13.0f;

    /* jump */
    if (jump_edge && on_ground) {
        dino_vy = JUMP_VEL;
        on_ground = 0;
    }

    /* gravity */
    dino_vy += GRAVITY;
    dino_y += dino_vy;
    if (dino_y >= GROUND_Y - DINO_H) {
        dino_y = GROUND_Y - DINO_H;
        dino_vy = 0.0f;
        on_ground = 1;
    }

    /* spawn new cactus */
    if (--spawn_timer <= 0) {
        spawn_obstacle();
        spawn_timer = 40 + rand() % 30;
    }

    /* scroll cacti, drop the ones that left the screen */
    for (i = 0; i < MAX_OBS; i++) {
        if (!obs[i].active)
            continue;
        obs[i].x -= (int)speed;
        if (obs[i].x + obs[i].w < 0)
            obs[i].active = 0;
    }

    /* collision (dino hitbox vs cactus hitbox) */
    dl = DINO_X + 4;
    dr = DINO_X + 20;
    dt = (int)dino_y + 4;
    db = (int)dino_y + DINO_H - 2;

    for (i = 0; i < MAX_OBS; i++) {
        int ol, orr, ot, ob;
        if (!obs[i].active)
            continue;

        ol  = obs[i].x + 3;
        orr = obs[i].x + obs[i].w - 3;
        ot  = obs[i].y + 2;
        ob  = obs[i].y + obs[i].h;

        if (dl < orr && dr > ol && dt < ob && db > ot) {
            state = ST_OVER;
            if (score > hi_score)
                hi_score = score;
            return;
        }
    }
}

static void update(void)
{
    if (state == ST_PLAY)
        update_playing();
    else if (state == ST_TITLE && (start_edge || jump_edge))
        reset_game();
    else if (state == ST_OVER && start_edge)
        reset_game();
}

/* ---------- input ---------- */

static void read_input(void)
{
    padInfo info;
    padData pad;
    int cross = 0, start = 0;

    ioPadGetInfo(&info);
    if (info.status[0] && ioPadGetData(0, &pad) == 0) {
        cross = pad.BTN_CROSS;
        start = pad.BTN_START;
    }

    jump_edge  = cross && !prev_cross;
    start_edge = start && !prev_start;
    prev_cross = cross;
    prev_start = start;
}

/* ---------- rendering ---------- */

static void render(void)
{
    int i;

    ticks++;

    clear_display(COL_BLACK);
    fill_rect(0, 0, LW, LH, COL_BG);
    fill_rect(0, GROUND_Y, LW, 2, COL_INK);

    if (state == ST_TITLE) {
        draw_text_centered(90, "DINO RUN", 6, COL_INK);
        draw_dino(DINO_X, GROUND_Y - DINO_H, 0, 0);

        /* menu: one button, START (select with X or START) */
        fill_rect(220, 200, 200, 52, COL_INK);
        draw_text_centered(212, "START", 5, COL_BG);
        return;
    }

    for (i = 0; i < MAX_OBS; i++)
        if (obs[i].active)
            draw_cactus(&obs[i]);

    draw_dino(DINO_X, (int)dino_y, frame, !on_ground);
    draw_hud();

    if (state == ST_OVER) {
        draw_text_centered(120, "GAME OVER", 5, COL_INK);
        if ((ticks / 30) % 2 == 0)
            draw_text_centered(200, "PRESS START", 3, COL_INK);
    }
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    void *host_addr;

    (void)argc;
    (void)argv;

    host_addr = memalign(1024 * 1024, HOST_SIZE);
    init_screen(host_addr, HOST_SIZE);

    /* scale the 640x360 logical screen to the real resolution, centered */
    scale = (int)display_width / LW;
    if (scale < 1)
        scale = 1;
    ox = ((int)display_width - LW * scale) / 2;
    oy = ((int)display_height - LH * scale) / 2;

    ioPadInit(7);
    sysUtilRegisterCallback(0, sysutil_exit_callback, NULL);
    srand((unsigned)time(NULL));

    while (!quit_flag) {
        sysUtilCheckCallback();
        read_input();
        update();

        waitflip();
        set_render_target(curr_fb);
        render();
        flip();
    }

    return 0;
}
