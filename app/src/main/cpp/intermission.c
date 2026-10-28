/*
 * The original Doom end-of-level screen (wi_stuff.c): world map with animated background,
 * "LEVEL FINISHED" title, KILLS / ITEMS / SECRETS percentages counting up, TIME / PAR,
 * then the "ENTERING" screen with splats on finished levels and a blinking "you are here" skull.
 */
#include "doom.h"

Intermission WI;

#define TIC_NS (1e9 / 35.0)
enum { WST_STATS = 0, WST_NEXT = 1 };

/* level positions on the episode maps (wi_stuff.c lnodes) */
static const int LNODES[3][9][2] = {
    {{185, 164}, {148, 143}, {69, 122}, {209, 102}, {116, 89}, {166, 55}, {71, 56}, {135, 29}, {71, 24}},
    {{254, 25}, {97, 50}, {188, 64}, {128, 78}, {214, 92}, {133, 130}, {208, 136}, {148, 140}, {235, 158}},
    {{156, 168}, {48, 154}, {174, 95}, {265, 75}, {130, 48}, {279, 23}, {198, 48}, {140, 25}, {281, 136}}};
/* animated background of episode 1 */
static const int ANIM0[10][2] = {{224, 104}, {184, 160}, {112, 136}, {72, 112}, {88, 96}, {64, 48}, {192, 40}, {136, 16}, {80, 16}, {64, 24}};
static const int PAR1[3][9] = {{30, 75, 120, 90, 165, 180, 180, 30, 165}, {90, 90, 90, 120, 90, 360, 240, 30, 170}, {90, 45, 90, 150, 90, 90, 165, 30, 135}};
static const int PAR2[32] = {30, 90, 120, 120, 90, 150, 120, 120, 270, 90, 210, 150, 150, 150, 210, 150, 420, 150, 210, 150, 240, 150, 180, 150, 150, 300, 330, 420, 300, 180, 120, 30};

static int idx2(const char *m) { return atoi(m + 3) - 1; }

static void begin(void) {
    Game *g = GM;
    Intermission *w = &WI;
    w->doom2 = wad_isDoom2(G_wad);
    snprintf(w->lastName, sizeof w->lastName, "%s", g->mapName);
    snprintf(w->nextName, sizeof w->nextName, "%s", g->nextMap);
    w->hasNext = w->nextName[0] != 0;
    if (w->doom2) {
        w->epsd = 0;
        w->last = idx2(w->lastName); w->next = w->hasNext ? idx2(w->nextName) : w->last;
        w->par = w->last >= 0 && w->last < 32 ? PAR2[w->last] : 0;
    } else {
        w->epsd = maxi(0, mini(2, w->lastName[1] - '1'));
        w->last = w->lastName[3] - '1';
        w->next = w->hasNext ? w->nextName[3] - '1' : w->last;
        w->par = w->last >= 0 && w->last < 9 ? PAR1[w->epsd][w->last] : 0;
        if (g->secretExit) w->didSecret = true;
        if (w->last == 7) w->didSecret = false;
    }
    w->pctKills = g->totalKills == 0 ? 100 : g->kills * 100 / g->totalKills;
    w->pctItems = g->totalItems == 0 ? 100 : g->items * 100 / g->totalItems;
    w->pctSecret = g->totalSecrets == 0 ? 100 : g->secrets * 100 / g->totalSecrets;
    w->secs = g->levelTics / 35;
    w->state = WST_STATS; w->spState = 1; w->cntPause = 35; w->bcnt = 0; w->ticsDone = 0;
    w->cntKills = w->cntItems = w->cntSecret = w->cntTime = w->cntPar = 0;
    for (int i = 0; i < 10; i++) { w->aCtr[i] = -1; w->aNext[i] = 1 + game_rnd() % 11; }
    w->t0 = plat_nanotime();
    w->active = true;
}

static void finish(void) {
    WI.active = false;
    gv_advanceLevel();
}

/* Player tapped the screen. */
void wi_press(void) {
    Intermission *w = &WI;
    if (!w->active) return;
    if (w->state == WST_STATS) {
        if (w->spState != 10) {
            w->cntKills = w->pctKills; w->cntItems = w->pctItems; w->cntSecret = w->pctSecret; w->cntTime = w->secs; w->cntPar = w->par;
            w->spState = 10; gv_beep("DSSGCOCK");
        } else if (!w->doom2 && w->hasNext) {
            w->state = WST_NEXT; w->cnt = 4 * 35; gv_beep("DSSGCOCK");
        } else finish();
    } else finish();
}

/* ------------------------------------------------------------------ simulation (35 Hz like the original) */
static void tick(void) {
    Intermission *w = &WI;
    w->bcnt++;
    if (!w->doom2 && w->epsd == 0)
        for (int i = 0; i < 10; i++)
            if (w->bcnt == w->aNext[i]) { w->aCtr[i] = (w->aCtr[i] + 1) % 3; w->aNext[i] = w->bcnt + 11; }
    if (w->state == WST_NEXT) {
        if (--w->cnt <= 0) { finish(); return; }
        w->pointerOn = (w->cnt & 31) < 20;
        return;
    }
    if (w->spState == 2) {
        w->cntKills += 2; if ((w->bcnt & 3) == 0) gv_beep("DSPISTOL");
        if (w->cntKills >= w->pctKills) { w->cntKills = w->pctKills; gv_beep("DSBAREXP"); w->spState++; }
    } else if (w->spState == 4) {
        w->cntItems += 2; if ((w->bcnt & 3) == 0) gv_beep("DSPISTOL");
        if (w->cntItems >= w->pctItems) { w->cntItems = w->pctItems; gv_beep("DSBAREXP"); w->spState++; }
    } else if (w->spState == 6) {
        w->cntSecret += 2; if ((w->bcnt & 3) == 0) gv_beep("DSPISTOL");
        if (w->cntSecret >= w->pctSecret) { w->cntSecret = w->pctSecret; gv_beep("DSBAREXP"); w->spState++; }
    } else if (w->spState == 8) {
        if ((w->bcnt & 3) == 0) gv_beep("DSPISTOL");
        w->cntTime += 3; w->cntPar += 3;
        if (w->cntPar >= w->par) {
            w->cntPar = w->par;
            if (w->cntTime >= w->secs) { w->cntTime = w->secs; gv_beep("DSBAREXP"); w->spState++; }
        }
    } else if (w->spState != 10 && (w->spState & 1) != 0) {
        if (--w->cntPause <= 0) { w->spState++; w->cntPause = 35; }
    }
}

/* ------------------------------------------------------------------ drawing */
static float nw(const char *n) { Pic *p = gfx_patch(G_gfx, n); return p ? (float) p->w : 0; }
static float nh(const char *n) { Pic *p = gfx_patch(G_gfx, n); return p ? (float) p->h : 0; }

static float drawNum(float x, float y, int n, int digits) {
    float w = nw("WINUM0");
    char nm[16];
    if (digits < 0) {
        if (n == 0) digits = 1;
        else { digits = 0; for (int t = n; t > 0; t /= 10) digits++; }
    }
    while (digits-- > 0) {
        x -= w;
        snprintf(nm, sizeof nm, "WINUM%d", n % 10);
        hud_patch(nm, x, y);
        n /= 10;
    }
    return x;
}

static void drawPercent(float x, float y, int p) {
    hud_patch("WIPCNT", x, y);
    drawNum(x, y, p, -1);
}

static void drawTime(float x, float y, int t) {
    if (t > 61 * 59) { hud_patch("WISUCKS", x - nw("WISUCKS"), y); return; }
    int div = 1;
    do {
        int n = (t / div) % 60;
        x = drawNum(x, y, n, 2) - nw("WICOLON");
        div *= 60;
        if (div == 60 || t / div > 0) hud_patch("WICOLON", x, y);
    } while (t / div > 0);
}

static void lvName(int i, char *out, int sz) {
    if (WI.doom2) snprintf(out, (size_t) sz, "CWILV%02d", i);
    else snprintf(out, (size_t) sz, "WILV%d%d", WI.epsd, i);
}

static void drawLevelTitle(int i, const char *fallback, float y, const char *sub) {
    char n[16];
    float h;
    lvName(i, n, sizeof n);
    if (hud_has(n)) { hud_patch(n, (320 - nw(n)) / 2.0f, y); h = nh(n); }
    else { hud_text(fallback, 160 - hud_textW(fallback) * 0.75f, y + 2, 1.5f); h = 16; }
    y += 5 * h / 4.0f;
    if (hud_has(sub)) hud_patch(sub, (320 - nw(sub)) / 2.0f, y);
}

static void background(void) {
    char bg[16], n[24];
    if (WI.doom2) snprintf(bg, sizeof bg, "INTERPIC"); else snprintf(bg, sizeof bg, "WIMAP%d", WI.epsd);
    if (hud_has(bg)) hud_patch(bg, 0, 0);
    if (!WI.doom2 && WI.epsd == 0)
        for (int i = 0; i < 10; i++) {
            if (WI.aCtr[i] < 0) continue;
            snprintf(n, sizeof n, "WIA0%d%02d%02d", WI.epsd, i, WI.aCtr[i]);
            if (hud_has(n)) hud_patch(n, (float) ANIM0[i][0], (float) ANIM0[i][1]);
        }
}

static void drawStats(void) {
    Intermission *w = &WI;
    char fb[32];
    snprintf(fb, sizeof fb, "%s FINISHED", w->lastName);
    drawLevelTitle(w->last, fb, 2, "WIF");
    float lh = 3 * nh("WINUM0") / 2.0f;
    hud_patch("WIOSTK", 50, 50);
    hud_patch("WIOSTI", 50, 50 + lh);
    hud_patch("WIOSTS", 50, 50 + 2 * lh);
    if (w->spState >= 2) drawPercent(270, 50, w->cntKills);
    if (w->spState >= 4) drawPercent(270, 50 + lh, w->cntItems);
    if (w->spState >= 6) drawPercent(270, 50 + 2 * lh, w->cntSecret);
    hud_patch("WITIME", 16, 168);
    if (w->spState >= 8) drawTime(144, 168, w->cntTime);
    if (w->doom2 || w->epsd < 3) {
        hud_patch("WIPAR", 176, 168);
        if (w->spState >= 8) drawTime(304, 168, w->cntPar);
    }
}

static void drawNext(void) {
    Intermission *w = &WI;
    char n[16];
    if (!w->doom2) {
        const int (*ln)[2] = LNODES[w->epsd];
        int lastSplat = w->last == 8 ? w->next - 1 : w->last;
        for (int i = 0; i <= lastSplat && i < 9; i++) hud_patch("WISPLAT", (float) ln[i][0], (float) ln[i][1]);
        if (w->didSecret) hud_patch("WISPLAT", (float) ln[8][0], (float) ln[8][1]);
        if (w->pointerOn && w->next >= 0 && w->next < 9) hud_patch("WIURH0", (float) ln[w->next][0], (float) ln[w->next][1]);
    }
    if (hud_has("WIENTER")) hud_patch("WIENTER", (320 - nw("WIENTER")) / 2.0f, 2);
    lvName(w->next, n, sizeof n);
    float y = 2 + 5 * nh(n) / 4.0f;
    if (hud_has(n)) hud_patch(n, (320 - nw(n)) / 2.0f, y);
    else hud_text(w->nextName, 160 - hud_textW(w->nextName) * 0.75f, y, 1.5f);
}

void wi_draw(int sw, int sh) {
    Intermission *w = &WI;
    if (!w->active) begin();
    int target = (int) ((plat_nanotime() - w->t0) / TIC_NS);
    int steps = 0;
    while (w->ticsDone < target && steps < 8 && w->active) { w->ticsDone++; steps++; tick(); }
    if (!w->active) return;
    if (w->ticsDone < target) w->ticsDone = target;

    w->sy = sh / 200.0f; w->sx = sh / 240.0f;
    w->ox = (sw - 320 * w->sx) / 2.0f;
    hud_setT(w->sx, w->sy, w->ox, 0);
    cv_clear(0xFF000000u);

    background();
    if (w->state == WST_STATS) drawStats();
    else drawNext();

    hud_setT(1, 1, 0, 0);
}
