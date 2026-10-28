/* Game loop, screen layout, touch controls, status bar, automap and the glue between menu, game and platform. */
#include "doom.h"

GameView GV;
Wad *G_wad;
Gfx *G_gfx;

static const int RES[3] = {180, 240, 360};
const float GV_ASPECT_VAL[7] = {0.0f, 4.0f / 3.0f, 5.0f / 4.0f, 16.0f / 10.0f, 16.0f / 9.0f, 21.0f / 9.0f, 1.0f};
const char *const GV_ASPECT_NAMES[7] = {"FULL", "4:3", "5:4", "16:10", "16:9", "21:9", "1:1"};
static const float SPEED_VAL[4] = {0.6f, 1.0f, 1.4f, 1.9f}, TAKEN_VAL[4] = {0.0f, 0.5f, 1.0f, 2.0f}, DEALT_VAL[4] = {1.0f, 2.0f, 4.0f, 10.0f};
static const float TEMPO_VAL[4] = {0.7f, 1.0f, 1.4f, 2.0f}, FOV_VAL[4] = {1.25f, 1.0f, 0.8f, 0.65f};
const int GV_BOOST[4] = {0, 3, 6, 10};
static const float HUD_FRAC[3] = {0.12f, 0.15f, 0.18f}, CTL_SCALE[3] = {0.8f, 1.0f, 1.25f}, CTL_ALPHA[3] = {0.4f, 0.7f, 1.0f};

static int64_t lastT;
static double acc;

#define D (GV.d)

void gv_init(float density) {
    memset(&GV, 0, sizeof GV);
    GV.d = density;
    GV.ticNs = 1e9 / 35.0;
    GV.titleMode = true;
    GV.resIndex = 1; GV.smooth = true; GV.sens = 1.0f; GV.haptic = true;
    GV.speedIdx = 1; GV.takenIdx = 2; GV.tempoIdx = 1; GV.fovIdx = 1;
    GV.lastHealth = 100; GV.messagesOn = true; GV.screenBlocks = 10; GV.sfxVol = 15; GV.musVol = 9;
    GV.hudSize = 1; GV.ctlSize = 1; GV.ctlAlpha = 2;
    GV.rebuildRenderer = true;
    GV.stickPtr = GV.lookPtr = GV.firePtr = GV.usePtr = -1;
    GV.fpsTime = 0;
}

void gv_setWad(Wad *w, Gfx *g) {
    G_wad = w; G_gfx = g;
    snd_init(w);
    music_init(w);
    tables_init();
    Renderer *tmp = rend_new(g, 320, 200);
    Game *game = game_new(w, g, tmp);
    game_startMap(w->maps[0], false, NULL, 0);      /* preloaded; the title screen is shown until "New Game" */
    hud_init();
    menu_init();
    prefs_load();
    GV.resIndex = prefs_getInt("res", 1); GV.smooth = prefs_getBool("smooth", true);
    GV.sens = prefs_getFloat("sens", 1.0f); game->skill = prefs_getInt("skill", 3);
    GV.messagesOn = prefs_getBool("msg", true); GV.lowDetail = prefs_getBool("detail", false);
    GV.screenBlocks = clampi(prefs_getInt("blocks", 10), 3, 11);
    GV.sfxVol = clampi(prefs_getInt("sfx", 15), 0, 15);
    GV.hudSize = clampi(prefs_getInt("hud", 1), 0, 2);
    GV.ctlSize = clampi(prefs_getInt("ctl", 1), 0, 2);
    GV.ctlAlpha = clampi(prefs_getInt("alpha", 2), 0, 2);
    GV.musVol = clampi(prefs_getInt("mus", 9), 0, 15);
    SND_volume = GV.sfxVol / 15.0f;
    music_setVolume(GV.musVol / 15.0f);
    game->aimAssist = prefs_getBool("assist", true);
    GV.showFps = prefs_getBool("fps", false);
    GV.showStats = prefs_getBool("stats", false); GV.showTime = prefs_getBool("time", false); GV.haptic = prefs_getBool("haptic", true);
    GV.aspectIdx = clampi(prefs_getInt("aspect", 0), 0, 6);
    GV.speedIdx = clampi(prefs_getInt("speed", 1), 0, 3); GV.takenIdx = clampi(prefs_getInt("taken", 2), 0, 3);
    GV.dealtIdx = clampi(prefs_getInt("dealt", 0), 0, 3); GV.tempoIdx = clampi(prefs_getInt("tempo", 1), 0, 3);
    GV.fovIdx = clampi(prefs_getInt("fov", 1), 0, 3); GV.filterIdx = clampi(prefs_getInt("filter", 0), 0, 4);
    GV.bright = clampi(prefs_getInt("bright", 0), 0, 3); GV.crosshair = clampi(prefs_getInt("xhair", 0), 0, 2);
    R_boost = GV_BOOST[GV.bright];
    menu_build();
    GV.musKey[0] = 0;
    GV.rebuildRenderer = true;
    lastT = 0; acc = 0;
}

/* ------------------------------------------------------------------ settings */
void gv_savePrefs(void) {
    Game *g = GM;
    prefs_setInt("res", GV.resIndex); prefs_setBool("smooth", GV.smooth); prefs_setFloat("sens", GV.sens);
    prefs_setInt("skill", g->skill); prefs_setBool("assist", g->aimAssist); prefs_setBool("fps", GV.showFps);
    prefs_setBool("msg", GV.messagesOn); prefs_setBool("detail", GV.lowDetail); prefs_setInt("blocks", GV.screenBlocks);
    prefs_setInt("sfx", GV.sfxVol); prefs_setInt("mus", GV.musVol);
    prefs_setInt("hud", GV.hudSize); prefs_setInt("ctl", GV.ctlSize); prefs_setInt("alpha", GV.ctlAlpha);
    prefs_setBool("stats", GV.showStats); prefs_setBool("time", GV.showTime);
    prefs_setInt("aspect", GV.aspectIdx); prefs_setInt("speed", GV.speedIdx); prefs_setInt("taken", GV.takenIdx);
    prefs_setInt("dealt", GV.dealtIdx); prefs_setInt("tempo", GV.tempoIdx); prefs_setInt("fov", GV.fovIdx);
    prefs_setInt("filter", GV.filterIdx); prefs_setBool("haptic", GV.haptic); prefs_setInt("bright", GV.bright);
    prefs_setInt("xhair", GV.crosshair);
    prefs_flush();
}

void gv_resetPrefs(void) {
    GV.resIndex = 1; GV.smooth = true; GM->aimAssist = true; GV.showFps = false; GV.showStats = false; GV.haptic = true;
    GV.hudSize = 1; GV.ctlSize = 1; GV.ctlAlpha = 2; GV.bright = 0; GV.crosshair = 0; R_boost = 0; GV.showTime = false;
    GV.aspectIdx = 0; GV.speedIdx = 1; GV.takenIdx = 2; GV.dealtIdx = 0; GV.tempoIdx = 1; GV.fovIdx = 1; GV.filterIdx = 0;
    GV.messagesOn = true; GV.screenBlocks = 10; GV.sens = 1.0f; GV.musVol = 9; music_setVolume(GV.musVol / 15.0f);
    GV.rebuildRenderer = true; gv_relayout(); gv_layoutButtons(); gv_savePrefs();
}

/* Pushes the setup-menu choices into the game and the renderer (cheap, runs every frame). */
static void applyOptions(void) {
    Game *g = GM;
    g->speedMul = SPEED_VAL[GV.speedIdx]; g->takenMul = TAKEN_VAL[GV.takenIdx]; g->dealtMul = DEALT_VAL[GV.dealtIdx];
    GV.ticNs = 1e9 / (35.0 * TEMPO_VAL[GV.tempoIdx]);
    R_filter = GV.filterIdx;
    if (GV.rend && GV.rend->zoom != FOV_VAL[GV.fovIdx]) rend_setZoom(GV.rend, FOV_VAL[GV.fovIdx]);
}

/* Picks the song for the current situation (title, intermission or the level) like Doom's S_Start / S_ChangeMusic. */
static void updateMusic(void) {
    char key[40];
    if (GV.titleMode) snprintf(key, sizeof key, "T");
    else if (GM->state == ST_EXIT) snprintf(key, sizeof key, "I");
    else snprintf(key, sizeof key, "L%s", GM->mapName);
    if (strcmp(key, GV.musKey) == 0) return;
    snprintf(GV.musKey, sizeof GV.musKey, "%s", key);
    bool d2 = wad_isDoom2(G_wad);
    char lump[16];
    bool loop = true;
    if (GV.titleMode) { snprintf(lump, sizeof lump, "%s", d2 ? "D_DM2TTL" : "D_INTRO"); loop = d2; }
    else if (GM->state == ST_EXIT) snprintf(lump, sizeof lump, "%s", d2 ? "D_DM2INT" : "D_INTER");
    else if (d2) {
        static const char *const d2m[32] = {"D_RUNNIN", "D_STALKS", "D_COUNTD", "D_BETWEE", "D_DOOM", "D_THE_DA", "D_SHAWN", "D_DDTBLU", "D_IN_CIT", "D_DEAD",
            "D_STLKS2", "D_THEDA2", "D_DOOM2", "D_DDTBL2", "D_RUNNI2", "D_DEAD2", "D_STLKS3", "D_ROMERO", "D_SHAWN2", "D_MESSAG",
            "D_COUNT2", "D_DDTBL3", "D_AMPIE", "D_THEDA3", "D_ADRIAN", "D_MESSG2", "D_ROMER2", "D_TENSE", "D_SHAWN3", "D_OPENIN",
            "D_EVIL", "D_ULTIMA"};
        int n = strncmp(GM->mapName, "MAP", 3) == 0 ? game_mapNumber(GM->mapName) - 1 : 0;
        snprintf(lump, sizeof lump, "%s", d2m[clampi(n, 0, 31)]);
    } else snprintf(lump, sizeof lump, "D_%s", GM->mapName);
    if (!wad_has(G_wad, lump)) return;
    music_request(lump, loop);
}

/* ------------------------------------------------------------------ layout */
void gv_layoutButtons(void) {
    GameView *v = &GV;
    float k = CTL_SCALE[v->ctlSize], d = v->d;
    v->bFireR = 54 * d * k; v->bFireX = v->sw - 96 * d * k; v->bFireY = v->sh - 96 * d * k;
    v->bUseR = 34 * d * k; v->bUseX = v->sw - 200 * d * k; v->bUseY = v->sh - 62 * d * k;
    v->bSmallR = 24 * d * k;
    v->bNextX = v->sw - 52 * d * k; v->bNextY = v->sh - 210 * d * k;
    v->bPrevX = v->sw - 140 * d * k; v->bPrevY = v->sh - 190 * d * k;
    v->bMapX = v->sw - 44 * d; v->bMapY = 40 * d;
    v->bMenuX = v->sw - 104 * d; v->bMenuY = 40 * d;
}

/* Recompute the 3D view rectangle and the status bar size (Doom's "Screen Size" + our bar scale). */
void gv_relayout(void) {
    GameView *v = &GV;
    if (v->sw <= 0 || v->sh <= 0) return;
    float barH = v->sh * HUD_FRAC[v->hudSize];
    v->barS = minf(barH / 38.4f, v->sw / 320.0f);
    barH = 38.4f * v->barS;
    v->barX0 = (v->sw - 320 * v->barS) / 2.0f;
    v->barTop = jround(v->sh - barH);
    if (v->screenBlocks >= 11) { v->vX = 0; v->vY = 0; v->vW = v->sw; v->vH = v->sh; }
    else {
        v->vW = jround(v->sw * v->screenBlocks / 10.0f); v->vH = jround(v->barTop * v->screenBlocks / 10.0f);
        v->vX = (v->sw - v->vW) / 2; v->vY = (v->barTop - v->vH) / 2;
    }
    float asp = GV_ASPECT_VAL[v->aspectIdx];                 /* fit the picture to the chosen screen shape, centred */
    if (asp > 0.0f && v->vW > 0 && v->vH > 0) {
        int nw = v->vW, nh = v->vH;
        if ((float) v->vW / v->vH > asp) nw = jround(v->vH * asp); else nh = jround(v->vW / asp);
        v->vX += (v->vW - nw) / 2; v->vY += (v->vH - nh) / 2; v->vW = nw; v->vH = nh;
    }
    v->rebuildRenderer = true;
}

void gv_surfaceChanged(int w, int h) {
    GV.sw = w; GV.sh = h;
    cv_init(w, h);
    gv_layoutButtons(); gv_relayout();
}

static void buildRenderer(void) {
    GameView *v = &GV;
    v->rebuildRenderer = false;
    if (v->vW <= 0 || v->vH <= 0) { gv_relayout(); v->rebuildRenderer = false; }
    int h = maxi(64, jround(RES[v->resIndex] * (float) v->vH / v->sh));
    int w = maxi(64, jround(h * (float) v->vW / v->vH));
    if (v->lowDetail) w = maxi(32, w / 2);           /* Doom's "Graphic Detail: LOW" = chunky pixels */
    w &= ~1;
    Renderer *old = GM->rend;
    Renderer *r = rend_new(G_gfx, w, h);
    v->rend = r;
    game_setRenderer(r);
    rend_free(old);
}

/* ------------------------------------------------------------------ simple state changes */
void gv_beep(const char *lump) { snd_play(lump, SND_lx, SND_ly); }
void gv_closeMenu(void) { GV.menuOpen = false; }
bool gv_canSave(void) { return !GV.titleMode && GM->state == ST_PLAY; }
void gv_releaseAll(void) {
    GV.stickPtr = GV.lookPtr = GV.firePtr = GV.usePtr = -1;
    GM->inMove = GM->inStrafe = 0; GM->inFire = false; GM->inUse = false;
}
void gv_openMenu(void) { gv_releaseAll(); menu_open(); GV.menuOpen = true; }

void gv_startNewGame(int episode, int skill) {
    GM->skill = skill + 1;
    char m[16];
    if (wad_isDoom2(G_wad)) snprintf(m, sizeof m, "MAP01"); else snprintf(m, sizeof m, "E%dM1", episode);
    if (wad_find(G_wad, m) < 0) snprintf(m, sizeof m, "%s", G_wad->maps[0]);
    game_startMap(m, false, NULL, 0);
    gv_savePrefs();
    GV.mapOpen = false; GV.titleMode = false; GV.menuOpen = false;
}

void gv_endGame(void) { GV.titleMode = true; GV.menuOpen = false; GV.mapOpen = false; gv_releaseAll(); }
void gv_quitApp(void) { plat_quit(); }

void gv_warpPrev(void) {
    if (G_wad->nmaps == 0) return;
    int i = wad_map_index(G_wad, GM->mapName), n = G_wad->nmaps;
    game_startMap(G_wad->maps[(i - 1 + n) % n], false, NULL, 0);
    if (GV.titleMode) GV.titleMode = false;
    menu_build();
}

void gv_warpNext(void) {
    if (G_wad->nmaps == 0) return;
    int i = wad_map_index(G_wad, GM->mapName), n = G_wad->nmaps;
    game_startMap(G_wad->maps[(i + 1) % n], false, NULL, 0);
    if (GV.titleMode) GV.titleMode = false;
    menu_build();
}

void gv_restartLevel(void) { char m[9]; snprintf(m, sizeof m, "%s", GM->mapName); game_startMap(m, false, NULL, 0); }

void gv_advanceLevel(void) {
    char n[9];
    snprintf(n, sizeof n, "%s", GM->nextMap);
    if (n[0] == 0) game_startMap(G_wad->nmaps == 0 ? GM->mapName : G_wad->maps[0], false, NULL, 0);
    else if (wad_find(G_wad, n) < 0) game_startMap(G_wad->maps[0], false, NULL, 0);
    else game_startMap(n, true, NULL, 0);
}

/* Hardware back button. Returns false when the app should close (title screen). */
bool gv_onBack(void) {
    if (GV.menuOpen) { menu_back(); return true; }
    if (GV.titleMode) return false;
    if (GV.mapOpen) { GV.mapOpen = false; return true; }
    gv_openMenu();
    return true;
}

void gv_onPause(void) {
    GV.paused = true;
    GM->inFire = false; GM->inUse = false; GM->inMove = 0; GM->inStrafe = 0;
    plat_audio_stop(); music_pause();
    gv_savePrefs();
}
void gv_onResume(void) { GV.paused = false; plat_audio_start(); music_resume(); lastT = 0; }

/* ------------------------------------------------------------------ drawing helpers */
static u32 argb(int a, int r, int g, int b) { return (u32) a << 24 | (u32) r << 16 | (u32) g << 8 | (u32) b; }

static void doomTextC(const char *s, float cx, float y, float k) {
    float t = GV.sh / 200.0f * k / 2.0f;
    hud_setT(t, t * 1.2f, cx - hud_textW(s) * t / 2.0f, y);
    hud_text(s, 0, 0, 1.0f);
}

/* Tiles Doom's FLOOR7_2 flat behind the framed view and around the status bar. */
static void drawBorder(void) {
    const u8 *f = gfx_flat(G_gfx, "FLOOR7_2");
    GameView *v = &GV;
    if (f) cv_tileFlat(f, G_gfx->lit[0], maxf(1.0f, v->barS), maxf(1.2f, v->barS * 1.2f), v->vX, v->vY, v->vX + v->vW, v->vY + v->vH);
    else cv_fillOutside(v->vX, v->vY, v->vX + v->vW, v->vY + v->vH, 0xFF3A3A3Au);
}

static void drawTitle(void) {
    GameView *v = &GV;
    float sy = v->sh / 200.0f, sx = v->sh / 240.0f;
    hud_setT(sx, sy, (v->sw - 320 * sx) / 2.0f, 0);
    if (hud_has("TITLEPIC")) hud_patch("TITLEPIC", 0, 0);
    if (!v->menuOpen) {
        if (((plat_nanotime() / 600000000LL) & 1) == 0) {
            const char *s = "TAP TO START";
            hud_text(s, 160 - hud_textW(s) * 0.75f, 182, 1.5f);
        }
    } else cv_rect(0, 0, (float) v->sw, (float) v->sh, 0xA0000000u);
}

static void text(const char *s, float x, float y, float size, u32 color, int al) { cv_text(s, x, y, size, color, al); }

static void drawCrosshair(void) {
    GameView *v = &GV;
    float x = v->vX + v->vW / 2.0f, y = v->vY + v->vH / 2.0f, r = 3.5f * D;
    if (v->crosshair == 1) { cv_circleFill(x, y, r * 0.55f, 0xCC00FF00u); return; }
    float w = maxf(1.5f, D * 1.2f);
    cv_line(x - r * 2.2f, y, x - r * 0.8f, y, w, 0xCC00FF00u); cv_line(x + r * 0.8f, y, x + r * 2.2f, y, w, 0xCC00FF00u);
    cv_line(x, y - r * 2.2f, x, y - r * 0.8f, w, 0xCC00FF00u); cv_line(x, y + r * 0.8f, x, y + r * 2.2f, w, 0xCC00FF00u);
}

static void drawHud(void) {
    GameView *v = &GV;
    Game *g = GM;
    if (v->screenBlocks <= 10) {
        /* original status bar, centred; the sides continue with the border flat */
        hud_setT(v->barS, v->barS * 1.2f, v->barX0, v->barTop - 168 * v->barS * 1.2f);
        hud_drawBar();
    } else hud_drawMinimal((float) v->sw, (float) v->sh, maxf(1.5f, v->sh * 0.0045f), 10 * D);
    if (v->messagesOn && g->msgTics > 0 && g->message[0]) {
        float k = maxf(2.0f, v->sh * 0.0042f);
        hud_setT(k, k * 1.2f, v->vX + 6 * D, v->vY + 6 * D);
        hud_text(g->message, 0, 0, 1.0f);
    }
}

static u32 al(u32 c) { return ((u32) jround((float) (c >> 24) * CTL_ALPHA[GV.ctlAlpha]) << 24) | (c & 0xFFFFFFu); }

/* label "<" / ">" are drawn as triangles, everything else with the Doom font */
static void circle(float x, float y, float r, bool active, const char *label, float ts) {
    cv_circleFill(x, y, r, al(active ? 0x66FFFFFFu : 0x33FFFFFFu));
    cv_circleStroke(x, y, r, 2 * D, al(0x88FFFFFFu));
    if (strcmp(label, "<") == 0) { float s = ts * 0.4f; cv_triFill(x - s, y, x + s * 0.8f, y - s, x + s * 0.8f, y + s, 0xFFFFFFFFu); }
    else if (strcmp(label, ">") == 0) { float s = ts * 0.4f; cv_triFill(x + s, y, x - s * 0.8f, y - s, x - s * 0.8f, y + s, 0xFFFFFFFFu); }
    else text(label, x, y + ts * 0.35f, ts, 0xFFFFFFFFu, AL_CENTER);
}

static void drawControls(void) {
    GameView *v = &GV;
    circle(v->bFireX, v->bFireY, v->bFireR, v->firePtr >= 0, "FIRE", 14 * D);
    circle(v->bUseX, v->bUseY, v->bUseR, v->usePtr >= 0, "USE", 13 * D);
    circle(v->bPrevX, v->bPrevY, v->bSmallR, false, "<", 16 * D);
    circle(v->bNextX, v->bNextY, v->bSmallR, false, ">", 16 * D);
    circle(v->bMapX, v->bMapY, v->bSmallR, v->mapOpen, "MAP", 10 * D);
    circle(v->bMenuX, v->bMenuY, v->bSmallR, false, "MENU", 9 * D);
    float k = CTL_SCALE[v->ctlSize];
    float cx = v->stickPtr >= 0 ? v->stickCx : 120 * D * k, cy = v->stickPtr >= 0 ? v->stickCy : v->sh - 110 * D * k;
    cv_circleFill(cx, cy, 64 * D * k, al(0x22FFFFFFu));
    cv_circleStroke(cx, cy, 64 * D * k, 2 * D, al(0x66FFFFFFu));
    cv_circleFill(cx + (v->stickPtr >= 0 ? v->stickX : 0), cy + (v->stickPtr >= 0 ? v->stickY : 0), 26 * D * k, al(0x77FFFFFFu));
}

static void drawAutomap(void) {
    GameView *v = &GV;
    Level *L = GM->L;
    Game *g = GM;
    if (!L) return;
    cv_rect(0, 0, (float) v->sw, (float) v->sh, 0xB0000000u);
    float scale = minf(v->sw / (L->maxX - L->minX + 200.0f), v->sh / (L->maxY - L->minY + 200.0f));
    float ox = v->sw / 2.0f - (L->minX + L->maxX) / 2.0f * scale, oy = v->sh / 2.0f + (L->minY + L->maxY) / 2.0f * scale;
    float lw = maxf(1.0f, D);
    for (int i = 0; i < L->nlines; i++) {
        Line *l = &L->lines[i];
        u32 col;
        if (!l->mapped && !g->hasMap) continue;
        if ((l->flags & ML_SECRET) != 0 && !g->hasMap) col = 0xFFC00000u;
        else if (!l->back) col = 0xFFE02020u;
        else if (l->fs->floor != l->bs->floor) col = 0xFFC8A030u;
        else if (l->fs->ceil != l->bs->ceil) col = 0xFFA0A0A0u;
        else { if (!g->hasMap) continue; col = 0xFF555555u; }
        cv_line(ox + l->x1 * scale, oy - l->y1 * scale, ox + l->x2 * scale, oy - l->y2 * scale, lw, col);
    }
    float px = ox + g->pl->x * scale, py = oy - g->pl->y * scale;
    float a = g->angle;
    float len = 14 * D;
    float ex = px + (float) cos((double) a) * len, ey = py - (float) sin((double) a) * len;
    cv_line(px, py, ex, ey, 2 * D, 0xFF40FF40u);
    cv_line(ex, ey, px + (float) cos(a + 2.6) * len * 0.6f, py - (float) sin(a + 2.6) * len * 0.6f, 2 * D, 0xFF40FF40u);
    cv_line(ex, ey, px + (float) cos(a - 2.6) * len * 0.6f, py - (float) sin(a - 2.6) * len * 0.6f, 2 * D, 0xFF40FF40u);
    text(g->mapName, 12 * D, v->sh - 12 * D, 16 * D, 0xFFFFFFFFu, AL_LEFT);
    for (int i = 0; i < g->mobjs.n; i++) {
        Mobj *m = g->mobjs.d[i];
        if (m->kind == MK_MONSTER && m->shootable && g->hasMap) cv_circleFill(ox + m->x * scale, oy - m->y * scale, 2.5f * D, 0xFFFF8000u);
    }
}

static void drawOverlays(void) {
    GameView *v = &GV;
    Game *g = GM;
    u32 tint = 0;
    if (g->damageFlash > 0) tint = argb(mini(150, g->damageFlash * 5), 255, 0, 0);
    else if (g->bonusFlash > 0) tint = argb(g->bonusFlash * 10, 255, 220, 80);
    else if (g->suit > 0 && (g->suit > 140 || (g->suit & 8) != 0)) tint = argb(45, 0, 200, 0);
    if (g->state == ST_DEAD) tint = argb(120, 120, 0, 0);
    if (tint != 0) {
        cv_setClip(v->vX, v->vY, v->vX + v->vW, v->vY + v->vH);
        cv_rect(0, 0, (float) v->sw, (float) v->sh, tint);
        cv_resetClip();
    }
    if (v->mapOpen) drawAutomap();
    drawHud();
    if (g->state == ST_PLAY && !v->menuOpen) drawControls();
    if (g->state == ST_DEAD && !v->menuOpen) {
        doomTextC("YOU DIED", v->sw / 2.0f, v->sh * 0.34f, 6.0f);
        doomTextC("TAP TO RESTART THE LEVEL", v->sw / 2.0f, v->sh * 0.34f + 56 * D, 2.0f);
    }
    if (g->state == ST_EXIT) wi_draw(v->sw, v->sh); else WI.active = false;
    if (v->menuOpen) {
        cv_rect(0, 0, (float) v->sw, (float) v->sh, 0x66000000u);
        menu_draw(v->sw, v->sh);
    }
    if (g->state == ST_PLAY && !v->menuOpen && !v->mapOpen && v->crosshair > 0) drawCrosshair();
    if (v->haptic) {
        if (g->state == ST_PLAY && g->health < v->lastHealth && !g->god && g->invuln <= 0) plat_haptic();
    }
    v->lastHealth = g->health;
    if ((v->showStats || v->showTime) && g->state == ST_PLAY && !v->menuOpen) {
        char st[96] = "";
        if (v->showStats) snprintf(st, sizeof st, "K %d/%d   I %d/%d   S %d/%d", g->kills, g->totalKills, g->items, g->totalItems, g->secrets, g->totalSecrets);
        if (v->showTime) {
            int sec = g->levelTics / 35;
            size_t n = strlen(st);
            snprintf(st + n, sizeof st - n, "%s%d:%02d", n ? "   " : "", sec / 60, sec % 60);
        }
        text(st, 8 * D, (v->showFps ? 38 : 20) * D, 13 * D, 0xFFFFFFFFu, AL_LEFT);
    }
    if (v->showFps) {
        char b[48];
        snprintf(b, sizeof b, "%d fps  %dx%d", v->fps, v->rend ? v->rend->W : 0, v->rend ? v->rend->H : 0);
        text(b, 8 * D, 20 * D, 13 * D, 0xFFFFFF00u, AL_LEFT);
    }
}

/* Composes the whole frame into the canvas. */
void gv_draw(void) {
    GameView *v = &GV;
    cv_resetClip();
    if (v->titleMode) {
        cv_clear(0xFF000000u);
        drawTitle();
        if (v->menuOpen) menu_draw(v->sw, v->sh);
        return;
    }
    Renderer *r = v->rend;
    if (!r) { cv_clear(0xFF000000u); return; }
    if (v->screenBlocks < 11) drawBorder();
    else if (v->aspectIdx != 0) cv_fillOutside(v->vX, v->vY, v->vX + v->vW, v->vY + v->vH, 0xFF000000u);
    cv_blitView(r->fb, r->W, r->H, v->vX, v->vY, v->vW, v->vH, v->smooth);
    drawOverlays();
}

/* ------------------------------------------------------------------ main loop (one iteration) */
void gv_frame(void) {
    GameView *v = &GV;
    if (v->paused || !GM || v->sw <= 0) return;
    if (v->rebuildRenderer) buildRenderer();
    applyOptions();
    updateMusic();
    int64_t now = plat_nanotime();
    if (lastT == 0) lastT = now;
    acc += (double) (now - lastT); lastT = now;
    if (acc > 250e6) acc = 250e6;
    bool freeze = v->menuOpen || v->titleMode || GM->state == ST_EXIT;
    if (!freeze) {
        while (acc >= v->ticNs) { game_tick(); acc -= v->ticNs; }
    } else acc = 0;
    if (!v->titleMode) game_render(freeze ? 1.0f : (float) (acc / v->ticNs));
    gv_draw();
    v->fpsCount++;
    if (now - v->fpsTime > 1000000000LL) { v->fps = v->fpsCount; v->fpsCount = 0; v->fpsTime = now; }
}

/* ------------------------------------------------------------------ touch */
static bool hit(float x, float y, float cx, float cy, float r) { float dx = x - cx, dy = y - cy; return dx * dx + dy * dy <= r * r; }

static void down(int id, float x, float y) {
    GameView *v = &GV;
    Game *g = GM;
    if (hit(x, y, v->bFireX, v->bFireY, v->bFireR * 1.2f)) { v->firePtr = id; v->lastFireX = x; g->inFire = true; return; }
    if (hit(x, y, v->bUseX, v->bUseY, v->bUseR * 1.3f)) { v->usePtr = id; g->inUse = true; return; }
    if (hit(x, y, v->bNextX, v->bNextY, v->bSmallR * 1.4f)) { game_cycleWeapon(1); return; }
    if (hit(x, y, v->bPrevX, v->bPrevY, v->bSmallR * 1.4f)) { game_cycleWeapon(-1); return; }
    if (hit(x, y, v->bMapX, v->bMapY, v->bSmallR * 1.4f)) { v->mapOpen = !v->mapOpen; return; }
    if (hit(x, y, v->bMenuX, v->bMenuY, v->bSmallR * 1.4f)) { gv_openMenu(); return; }
    if (x < v->sw * 0.45f && v->stickPtr < 0) { v->stickPtr = id; v->stickCx = x; v->stickCy = y; v->stickX = v->stickY = 0; return; }
    if (v->lookPtr < 0) { v->lookPtr = id; v->lastLookX = x; }
}

static void move(int id, float x, float y) {
    GameView *v = &GV;
    Game *g = GM;
    if (id == v->stickPtr) {
        float R = 60 * D * CTL_SCALE[v->ctlSize];
        float dx = x - v->stickCx, dy = y - v->stickCy;
        float len = (float) sqrt((double) (dx * dx + dy * dy));
        if (len > R) { dx = dx / len * R; dy = dy / len * R; len = R; }
        v->stickX = dx; v->stickY = dy;
        float mag = len / R;
        if (mag < 0.12f) { g->inMove = 0; g->inStrafe = 0; }
        else {
            float k = (mag - 0.12f) / 0.88f;
            float nx = dx / (len == 0 ? 1 : len), ny = dy / (len == 0 ? 1 : len);
            g->inMove = -ny * minf(1.0f, k * 1.25f);
            g->inStrafe = nx * minf(1.0f, k * 1.25f);
        }
    } else if (id == v->lookPtr) {
        game_addTurn(-(x - v->lastLookX) * 0.0042f * v->sens); v->lastLookX = x;
    } else if (id == v->firePtr) {
        game_addTurn(-(x - v->lastFireX) * 0.0042f * v->sens); v->lastFireX = x;
    }
}

static void up(int id) {
    GameView *v = &GV;
    Game *g = GM;
    if (id == v->stickPtr) { v->stickPtr = -1; g->inMove = 0; g->inStrafe = 0; }
    if (id == v->lookPtr) v->lookPtr = -1;
    if (id == v->firePtr) { v->firePtr = -1; g->inFire = false; }
    if (id == v->usePtr) { v->usePtr = -1; g->inUse = false; }
}

void gv_touch(int act, int idx, int count, const int *ids, const float *xs, const float *ys) {
    GameView *v = &GV;
    if (!GM) return;
    if (v->menuOpen) { if (count > 0) menu_touch(act, xs[0], ys[0]); return; }
    if (v->titleMode) { if (act == TA_UP) gv_openMenu(); return; }
    if (GM->state == ST_EXIT) { if (act == TA_UP) wi_press(); return; }
    if (GM->state == ST_DEAD) { if (act == TA_UP) gv_restartLevel(); return; }
    switch (act) {
        case TA_DOWN: case TA_POINTER_DOWN: down(ids[idx], xs[idx], ys[idx]); break;
        case TA_MOVE: for (int i = 0; i < count; i++) move(ids[i], xs[i], ys[i]); break;
        case TA_UP: case TA_POINTER_UP: up(ids[idx]); break;
        case TA_CANCEL: for (int i = 0; i < count; i++) up(ids[i]); break;
        default: break;
    }
}
