// Chrome-style dino runner for PS3 (PSL1GHT + tiny3d)
// Controls: X = start / jump / restart
// NOTE: untested. Build with the PS3 toolchain (ps3dev / PSL1GHT + tiny3d).

#include <stdlib.h>
#include <tiny3d.h>
#include <io/pad.h>

#define SCR_W       848
#define SCR_H       512
#define GROUND_Y    400
#define DINO_X      80
#define DINO_W      40
#define DINO_H      50
#define GRAVITY     1.2f
#define JUMP_V      (-20.0f)
#define OBS_W       20
#define MAX_OBS     4

typedef enum { MENU, PLAY, OVER } State;

static State state = MENU;
static float dinoY, dinoVy;
static float speed;
static int score;
static float obsX[MAX_OBS];
static int   obsH[MAX_OBS];
static int   obsOn[MAX_OBS];
static unsigned int seed = 12345;

static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (unsigned int)n);
}

static void drawRect(float x, float y, float w, float h, u32 col)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, 0);         tiny3d_VertexColor(col);
    tiny3d_VertexPos(x + w, y, 0);     tiny3d_VertexColor(col);
    tiny3d_VertexPos(x + w, y + h, 0); tiny3d_VertexColor(col);
    tiny3d_VertexPos(x, y + h, 0);     tiny3d_VertexColor(col);
    tiny3d_End();
}

static void resetGame(void)
{
    dinoY = GROUND_Y;
    dinoVy = 0;
    speed = 6.0f;
    score = 0;
    for (int i = 0; i < MAX_OBS; i++) obsOn[i] = 0;
}

static void spawnObstacle(void)
{
    for (int i = 0; i < MAX_OBS; i++) {
        if (!obsOn[i]) {
            obsOn[i] = 1;
            obsX[i] = SCR_W + rnd(200);
            obsH[i] = 30 + rnd(30);
            return;
        }
    }
}

static int collides(int i)
{
    float dTop = dinoY - DINO_H, dBot = dinoY;
    float oTop = GROUND_Y - obsH[i], oBot = GROUND_Y;
    float dL = DINO_X, dR = DINO_X + DINO_W;
    float oL = obsX[i], oR = obsX[i] + OBS_W;
    return dL < oR && dR > oL && dTop < oBot && dBot > oTop;
}

static void update(int jumpPressed)
{
    if (state == MENU) {
        if (jumpPressed) { resetGame(); state = PLAY; }
        return;
    }
    if (state == OVER) {
        if (jumpPressed) { resetGame(); state = PLAY; }
        return;
    }

    // PLAY
    if (jumpPressed && dinoY >= GROUND_Y) {
        dinoVy = JUMP_V;
    }
    dinoVy += GRAVITY;
    dinoY += dinoVy;
    if (dinoY >= GROUND_Y) { dinoY = GROUND_Y; dinoVy = 0; }

    speed += 0.002f;
    score++;

    int lastX = -1000;
    int active = 0;
    for (int i = 0; i < MAX_OBS; i++) {
        if (!obsOn[i]) continue;
        active++;
        obsX[i] -= speed;
        if (obsX[i] + OBS_W < 0) { obsOn[i] = 0; continue; }
        if (obsX[i] > lastX) lastX = (int)obsX[i];
        if (collides(i)) { state = OVER; return; }
    }
    if (active == 0 || lastX < SCR_W - 300 - rnd(150)) {
        spawnObstacle();
    }
}

static void draw(void)
{
    const u32 BLACK = 0xff535353;
    const u32 GREY  = 0xff8a8a8a;
    const u32 RED   = 0xffe53935;
    const u32 GREEN = 0xff4caf50;

    // ground line
    drawRect(0, GROUND_Y, SCR_W, 2, BLACK);

    if (state == MENU) {
        drawRect(SCR_W / 2 - 150, 140, 300, 60, BLACK);   // title block
        drawRect(DINO_X, GROUND_Y - DINO_H, DINO_W, DINO_H, GREY);
        drawRect(SCR_W / 2 - 80, 300, 160, 50, GREEN);    // "press X"
        return;
    }

    // dino
    drawRect(DINO_X, dinoY - DINO_H, DINO_W, DINO_H, BLACK);
    drawRect(DINO_X + 20, dinoY - DINO_H - 10, 20, 12, BLACK); // head

    // cacti
    for (int i = 0; i < MAX_OBS; i++) {
        if (obsOn[i])
            drawRect(obsX[i], GROUND_Y - obsH[i], OBS_W, obsH[i], BLACK);
    }

    // score: one small square per 50 points
    int squares = score / 50;
    if (squares > 40) squares = 40;
    for (int i = 0; i < squares; i++)
        drawRect(SCR_W - 30 - i * 10, 20, 6, 6, GREY);

    if (state == OVER)
        drawRect(SCR_W / 2 - 60, 200, 120, 40, RED);      // "game over" marker
}

int main(void)
{
    padInfo pInfo;
    padData pData;
    int prevCross = 0;

    tiny3d_Init(1024 * 1024);
    ioPadInit(7);
    resetGame();

    while (1) {
        ioPadGetInfo(&pInfo);
        ioPadGetData(0, &pData);

        int cross = pData.BTN_CROSS ? 1 : 0;
        int jumpPressed = cross && !prevCross;   // edge: one press = one jump
        prevCross = cross;

        update(jumpPressed);

        tiny3d_Clear(0xffffffff, TINY3D_CLEAR_ALL);
        tiny3d_Project2D();
        draw();
        tiny3d_Flip();
    }
    return 0;
}
