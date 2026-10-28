/*
 * The classic Doom menu (New Game / Options / Read This / Quit, skull cursor, thermometer sliders)
 * drawn from the WAD graphics, plus a "C4Doom Setup" page with the app's own settings.
 */
#include "doom.h"

static const char *const RES_LABEL[3] = {"LOW 180P", "MEDIUM 240P", "HIGH 360P"};
static const char *const SIZE_LABEL[3] = {"SMALL", "MEDIUM", "LARGE"};
static const char *const ALPHA_LABEL[3] = {"LOW", "MEDIUM", "HIGH"};
static const char *const BRIGHT_LABEL[4] = {"OFF", "LOW", "MEDIUM", "HIGH"};
static const char *const XHAIR_LABEL[3] = {"OFF", "DOT", "CROSS"};
static const char *const SPEED_LABEL[4] = {"SLOW", "NORMAL", "FAST", "TURBO"};
static const char *const TAKEN_LABEL[4] = {"NONE", "HALF", "NORMAL", "DOUBLE"};
static const char *const DEALT_LABEL[4] = {"NORMAL", "DOUBLE", "X4", "X10"};
static const char *const TEMPO_LABEL[4] = {"SLOW", "NORMAL", "FAST", "TURBO"};
static const char *const FOV_LABEL[4] = {"NARROW", "NORMAL", "WIDE", "ULTRA WIDE"};
static const char *const FILTER_LABEL[5] = {"OFF", "GRAYSCALE", "SEPIA", "NIGHT VISION", "INFERNO"};
static const char *const INFO_LABEL[4] = {"OFF", "KILLS", "KILLS AND TIME", "TIME"};

static bool isJV(int s) { return s == S_JV || s >= S_JVV; }

typedef struct { int id, slider; const char *patch; char text[64]; bool hasText; } It;

static struct {
    It items[16]; int nitems;
    float itY[17];
    int ax, ay;
    int scr, prev, helpPage;
    int sel[16];
    int episode;
    float sx, sy, ox, oy;
    bool drag;
    int downItem;                       /* -1 none, -3 back button */
    float downVx, downVy;
} M;

static const char *onOff(bool b) { return b ? "ON" : "OFF"; }

void menu_init(void) {
    memset(&M, 0, sizeof M);
    M.sx = M.sy = 1; M.episode = 1; M.downItem = -2;
}

/* ------------------------------------------------------------------ navigation */
void menu_open(void) { M.scr = S_MAIN; menu_build(); }

static void goTo(int s) { M.prev = M.scr; M.scr = s; menu_build(); gv_beep("DSPSTART"); }

static int episodeCount(void) {
    int n = 0;
    for (int e = 1; e <= 4; e++) { char nm[8]; snprintf(nm, sizeof nm, "E%dM1", e); if (wad_has(G_wad, nm)) n++; }
    return n;
}

static int firstEpisode(void) {
    for (int e = 1; e <= 4; e++) { char nm[8]; snprintf(nm, sizeof nm, "E%dM1", e); if (wad_has(G_wad, nm)) return e; }
    return 1;
}

void menu_back(void) {
    gv_beep("DSSWTCHN");
    switch (M.scr) {
        case S_MAIN: gv_closeMenu(); return;
        case S_SKILL: M.scr = episodeCount() > 1 && !wad_isDoom2(G_wad) ? S_EPI : S_MAIN; break;
        case S_SND: M.scr = S_OPT; break;
        case S_JVV: case S_JVC: case S_JVG: case S_JVX: case S_JVP: M.scr = S_JV; break;
        case S_QUIT: case S_END: M.scr = M.prev; break;
        default: M.scr = S_MAIN;
    }
    menu_build();
}

/* ------------------------------------------------------------------ layout */
static void add(int id, const char *patch, const char *text, int slider) {
    if (M.nitems >= 16) return;
    It *it = &M.items[M.nitems++];
    it->id = id; it->patch = patch; it->slider = slider; it->hasText = text != NULL;
    if (text) snprintf(it->text, sizeof it->text, "%s", text);
    else it->text[0] = 0;
}

static void addf(int id, int slider, const char *fmt, const char *a) {
    char buf[64];
    snprintf(buf, sizeof buf, fmt, a);
    add(id, NULL, buf, slider);
}

static int rows(const It *it) { return it->slider > 0 ? 2 : 1; }

void menu_build(void) {
    GameView *v = &GV;
    M.nitems = 0;
    char buf[64];
    switch (M.scr) {
        case S_MAIN:
            M.ax = 97; M.ay = 64;
            add(1, "M_NGAME", NULL, 0); add(2, "M_OPTION", NULL, 0); add(6, "M_LOADG", NULL, 0); add(7, "M_SAVEG", NULL, 0);
            add(3, NULL, "C4DOOM SETUP", 0);
            add(4, "M_RDTHIS", NULL, 0); add(5, "M_QUITG", NULL, 0);
            break;
        case S_LOAD: case S_SAVE:
            M.ax = 44; M.ay = 46;
            if (M.scr == S_SAVE && !gv_canSave()) { add(0, NULL, "NO GAME TO SAVE", 0); break; }
            for (int i = 0; i < SAVE_SLOTS; i++) {
                char d[64];
                bool has = save_describe(i, d, sizeof d);
                snprintf(buf, sizeof buf, "%d %s", i + 1, has ? d : "EMPTY");
                add(100 + i, NULL, buf, 0);
            }
            break;
        case S_EPI:
            M.ax = 48; M.ay = 63;
            for (int e = 1; e <= 4; e++) {
                char nm[8]; snprintf(nm, sizeof nm, "E%dM1", e);
                if (wad_has(G_wad, nm)) {
                    static char pn[4][12];
                    snprintf(pn[e - 1], sizeof pn[e - 1], "M_EPI%d", e);
                    add(10 + e, pn[e - 1], NULL, 0);
                }
            }
            break;
        case S_SKILL:
            M.ax = 48; M.ay = 63;
            add(20, "M_JKILL", NULL, 0); add(21, "M_ROUGH", NULL, 0); add(22, "M_HURT", NULL, 0);
            add(23, "M_ULTRA", NULL, 0); add(24, "M_NMARE", NULL, 0);
            break;
        case S_OPT:
            M.ax = 60; M.ay = 37;
            add(30, "M_ENDGAM", NULL, 0); add(31, "M_MESSG", NULL, 0); add(32, "M_DETAIL", NULL, 0);
            add(33, "M_SCRNSZ", NULL, 9); add(34, "M_MSENS", NULL, 10); add(35, "M_SVOL", NULL, 0);
            break;
        case S_SND:
            M.ax = 80; M.ay = 64;
            add(40, "M_SFXVOL", NULL, 16);
            add(41, "M_MUSVOL", NULL, 16);
            break;
        case S_JV:
            M.ax = 44; M.ay = 34;
            add(58, NULL, "VIDEO >", 0);
            add(75, NULL, "PICTURE >", 0);
            add(59, NULL, "CONTROLS >", 0);
            add(60, NULL, "GAMEPLAY >", 0);
            add(61, NULL, "CHEATS >", 0);
            add(68, NULL, "RESET SETTINGS", 0);
            break;
        case S_JVV:
            M.ax = 44; M.ay = 34;
            addf(50, 0, "RESOLUTION: %s", RES_LABEL[v->resIndex]);
            addf(76, 0, "SCREEN: %s", GV_ASPECT_NAMES[v->aspectIdx]);
            addf(51, 0, "SMOOTHING: %s", onOff(v->smooth));
            addf(53, 0, "FPS COUNTER: %s", onOff(v->showFps));
            addf(64, 0, "HUD INFO: %s", INFO_LABEL[!v->showStats ? (v->showTime ? 3 : 0) : (v->showTime ? 2 : 1)]);
            addf(63, 0, "CROSSHAIR: %s", XHAIR_LABEL[v->crosshair]);
            break;
        case S_JVP:
            M.ax = 44; M.ay = 34;
            addf(62, 0, "BRIGHTNESS: %s", BRIGHT_LABEL[v->bright]);
            addf(69, 0, "FOV: %s", FOV_LABEL[v->fovIdx]);
            addf(70, 0, "FILTER: %s", FILTER_LABEL[v->filterIdx]);
            break;
        case S_JVC:
            M.ax = 44; M.ay = 34;
            addf(52, 0, "AIM ASSIST: %s", onOff(GM->aimAssist));
            addf(54, 0, "BAR SIZE: %s", SIZE_LABEL[v->hudSize]);
            addf(55, 0, "BUTTON SIZE: %s", SIZE_LABEL[v->ctlSize]);
            addf(56, 0, "BUTTON OPACITY: %s", ALPHA_LABEL[v->ctlAlpha]);
            addf(65, 0, "VIBRATION: %s", onOff(v->haptic));
            break;
        case S_JVG:
            M.ax = 44; M.ay = 34;
            snprintf(buf, sizeof buf, "NEXT MAP: %s >", GM->mapName); add(57, NULL, buf, 0);
            add(67, NULL, "< PREVIOUS MAP", 0);
            add(66, NULL, "RESTART LEVEL", 0);
            addf(71, 0, "MOVE SPEED: %s", SPEED_LABEL[v->speedIdx]);
            addf(72, 0, "DAMAGE TAKEN: %s", TAKEN_LABEL[v->takenIdx]);
            addf(73, 0, "YOUR DAMAGE: %s", DEALT_LABEL[v->dealtIdx]);
            addf(74, 0, "GAME SPEED: %s", TEMPO_LABEL[v->tempoIdx]);
            break;
        case S_JVX:
            M.ax = 44; M.ay = 34;
            addf(80, 0, "GOD MODE: %s", onOff(GM->god));
            addf(81, 0, "NOCLIP: %s", onOff(GM->noclip));
            addf(82, 0, "INFINITE AMMO: %s", onOff(GM->infAmmo));
            add(83, NULL, "ALL WEAPONS AND AMMO", 0);
            add(84, NULL, "ALL KEYS", 0);
            add(85, NULL, "FULL HEALTH AND ARMOR", 0);
            add(86, NULL, "KILL ALL MONSTERS", 0);
            addf(87, 0, "NOTARGET: %s", onOff(GM->noTarget));
            break;
        default: break;
    }
    float y = (float) M.ay;
    for (int i = 0; i < M.nitems; i++) { M.itY[i] = y; y += 16.0f * rows(&M.items[i]); }
    if (M.sel[M.scr] >= M.nitems) M.sel[M.scr] = 0;
}

/* ------------------------------------------------------------------ actions */
/* Save / load screens: tap a slot. */
static void slotAction(int slot) {
    if (M.scr == S_SAVE) {
        bool ok = gv_canSave() && save_save(slot);
        gv_closeMenu();
        game_msg(ok ? "GAME SAVED" : "SAVE FAILED");
    } else {
        char d[64];
        if (!save_describe(slot, d, sizeof d)) return;
        bool ok = save_load(slot);
        gv_closeMenu();
        game_msg(ok ? "GAME LOADED" : "LOAD FAILED");
    }
}

static void activate(int id) {
    GameView *v = &GV;
    gv_beep("DSPISTOL");
    if (id >= 100 && id < 100 + SAVE_SLOTS) { slotAction(id - 100); return; }
    switch (id) {
        case 0: menu_back(); break;
        case 6: goTo(S_LOAD); break;
        case 7: goTo(S_SAVE); break;
        case 1:
            if (wad_isDoom2(G_wad) || episodeCount() <= 1) { M.episode = firstEpisode(); M.sel[S_SKILL] = 2; goTo(S_SKILL); }
            else goTo(S_EPI);
            break;
        case 2: goTo(S_OPT); break;
        case 3: goTo(S_JV); break;
        case 4: M.helpPage = 0; if (!hud_has("HELP1") && !hud_has("CREDIT")) break; goTo(S_HELP); break;
        case 5: goTo(S_QUIT); break;
        case 11: case 12: case 13: case 14: M.episode = id - 10; M.sel[S_SKILL] = 2; goTo(S_SKILL); break;
        case 20: case 21: case 22: case 23: case 24: gv_startNewGame(M.episode, id - 20); break;
        case 30: if (!v->titleMode) goTo(S_END); break;
        case 31: v->messagesOn = !v->messagesOn; gv_savePrefs(); break;
        case 32: v->lowDetail = !v->lowDetail; v->rebuildRenderer = true; gv_savePrefs(); break;
        case 35: goTo(S_SND); break;
        case 50: v->resIndex = (v->resIndex + 1) % 3; v->rebuildRenderer = true; gv_savePrefs(); menu_build(); break;
        case 51: v->smooth = !v->smooth; gv_savePrefs(); menu_build(); break;
        case 52: GM->aimAssist = !GM->aimAssist; gv_savePrefs(); menu_build(); break;
        case 53: v->showFps = !v->showFps; gv_savePrefs(); menu_build(); break;
        case 54: v->hudSize = (v->hudSize + 1) % 3; gv_relayout(); gv_savePrefs(); menu_build(); break;
        case 55: v->ctlSize = (v->ctlSize + 1) % 3; gv_layoutButtons(); gv_savePrefs(); menu_build(); break;
        case 56: v->ctlAlpha = (v->ctlAlpha + 1) % 3; gv_savePrefs(); menu_build(); break;
        case 57: gv_warpNext(); break;
        case 58: goTo(S_JVV); break;
        case 59: goTo(S_JVC); break;
        case 60: goTo(S_JVG); break;
        case 61: goTo(S_JVX); break;
        case 62: v->bright = (v->bright + 1) % 4; R_boost = GV_BOOST[v->bright]; gv_savePrefs(); menu_build(); break;
        case 63: v->crosshair = (v->crosshair + 1) % 3; gv_savePrefs(); menu_build(); break;
        case 64: {
            int cur = !v->showStats ? (v->showTime ? 3 : 0) : (v->showTime ? 2 : 1), nx = (cur + 1) % 4;
            v->showStats = nx == 1 || nx == 2; v->showTime = nx == 2 || nx == 3; gv_savePrefs(); menu_build(); break;
        }
        case 65: v->haptic = !v->haptic; gv_savePrefs(); menu_build(); break;
        case 66: if (!v->titleMode) { gv_restartLevel(); gv_closeMenu(); } break;
        case 67: gv_warpPrev(); break;
        case 68: gv_resetPrefs(); menu_build(); break;
        case 75: goTo(S_JVP); break;
        case 76: v->aspectIdx = (v->aspectIdx + 1) % 7; gv_relayout(); gv_savePrefs(); menu_build(); break;
        case 69: v->fovIdx = (v->fovIdx + 1) % 4; gv_savePrefs(); menu_build(); break;
        case 70: v->filterIdx = (v->filterIdx + 1) % 5; gv_savePrefs(); menu_build(); break;
        case 71: v->speedIdx = (v->speedIdx + 1) % 4; gv_savePrefs(); menu_build(); break;
        case 72: v->takenIdx = (v->takenIdx + 1) % 4; gv_savePrefs(); menu_build(); break;
        case 73: v->dealtIdx = (v->dealtIdx + 1) % 4; gv_savePrefs(); menu_build(); break;
        case 74: v->tempoIdx = (v->tempoIdx + 1) % 4; gv_savePrefs(); menu_build(); break;
        case 87: GM->noTarget = !GM->noTarget; menu_build(); break;
        case 80: GM->god = !GM->god; menu_build(); break;
        case 81: GM->noclip = !GM->noclip; menu_build(); break;
        case 82: GM->infAmmo = !GM->infAmmo; menu_build(); break;
        case 83: if (!v->titleMode) { game_cheatWeapons(); gv_closeMenu(); } break;
        case 84: if (!v->titleMode) { game_cheatKeys(); gv_closeMenu(); } break;
        case 85: if (!v->titleMode) { game_cheatHealth(); gv_closeMenu(); } break;
        case 86: if (!v->titleMode) { game_cheatKillAll(); gv_closeMenu(); } break;
        default: break;
    }
}

static int sliderValue(int id) {
    switch (id) {
        case 33: return GV.screenBlocks - 3;
        case 34: return maxi(0, mini(9, jround((GV.sens - 0.4f) / 0.2f)));
        case 40: return GV.sfxVol;
        case 41: return GV.musVol;
        default: return 0;
    }
}

static void setSlider(int id, int dot) {
    switch (id) {
        case 33: GV.screenBlocks = dot + 3; gv_relayout(); break;
        case 34: GV.sens = 0.4f + 0.2f * dot; break;
        case 40: GV.sfxVol = dot; SND_volume = dot / 15.0f; break;
        case 41: GV.musVol = dot; music_setVolume(dot / 15.0f); break;
        default: return;
    }
    gv_savePrefs();
}

static void sliderFromTouch(float vx) {
    if (M.sel[M.scr] >= M.nitems) return;
    It *it = &M.items[M.sel[M.scr]];
    if (it->slider <= 0) return;
    int dot = (int) floorf((vx - (M.ax + 8)) / 8.0f);
    dot = maxi(0, mini(it->slider - 1, dot));
    if (dot != sliderValue(it->id)) { setSlider(it->id, dot); gv_beep("DSSTNMOV"); }
}

/* ------------------------------------------------------------------ drawing */
static void transform(int sw, int sh) {
    M.sy = sh / 200.0f; M.sx = sh / 240.0f;               /* 320x200 shown at 4:3 like the original */
    M.ox = (sw - 320 * M.sx) / 2.0f; M.oy = 0;
    hud_setT(M.sx, M.sy, M.ox, M.oy);
}

static void thermo(float x, float y, int width, int dot) {
    float xx = x;
    hud_patch("M_THERML", xx, y);
    xx += 8;
    for (int i = 0; i < width; i++) { hud_patch("M_THERMM", xx, y); xx += 8; }
    hud_patch("M_THERMR", xx, y);
    hud_patch("M_THERMO", x + 8 + dot * 8, y);
}

static void title(const char *t) { hud_text(t, 160 - hud_textW(t), 8, 2.0f); }

static void drawHelp(void) {
    const char *n = M.helpPage == 0 ? "HELP1" : "HELP2";
    if (!hud_has(n)) n = hud_has("CREDIT") ? "CREDIT" : "HELP1";
    hud_patch(n, 0, 0);
}

static const float YES[4] = {50, 112, 150, 138}, NO[4] = {170, 112, 270, 138};

static void button(const float *r, const char *s) {
    cv_rrectFill(M.ox + r[0] * M.sx, r[1] * M.sy, M.ox + r[2] * M.sx, r[3] * M.sy, 5 * M.sx, 0xFF2A2A2Au);
    cv_rrectStroke(M.ox + r[0] * M.sx, r[1] * M.sy, M.ox + r[2] * M.sx, r[3] * M.sy, 5 * M.sx, M.sx, 0xFF999999u);
    hud_text(s, (r[0] + r[2]) / 2.0f - hud_textW(s), r[1] + 6, 2.0f);
}

static void drawConfirm(void) {
    float x0 = M.ox + 10 * M.sx, y0 = 50 * M.sy, x1 = M.ox + 310 * M.sx, y1 = 150 * M.sy;
    cv_rrectFill(x0, y0, x1, y1, 6 * M.sx, 0xE0000000u);
    cv_rrectStroke(x0, y0, x1, y1, 6 * M.sx, 2 * M.sx, 0xFFB02020u);
    const char *ls[2];
    if (M.scr == S_QUIT) { ls[0] = "ARE YOU SURE YOU WANT TO QUIT"; ls[1] = "THIS GREAT GAME?"; }
    else { ls[0] = "ARE YOU SURE YOU WANT TO END"; ls[1] = "THE GAME?"; }
    for (int i = 0; i < 2; i++) hud_text(ls[i], 160 - hud_textW(ls[i]) * 0.65f, 62.0f + i * 14, 1.3f);
    button(YES, "YES"); button(NO, "NO");
}

void menu_draw(int sw, int sh) {
    transform(sw, sh);
    if (M.scr == S_HELP) { drawHelp(); return; }
    if (M.scr == S_QUIT || M.scr == S_END) { drawConfirm(); return; }
    switch (M.scr) {
        case S_MAIN: hud_patch("M_DOOM", 94, 2); break;
        case S_LOAD: hud_patch("M_LOADG", 72, 28); break;
        case S_SAVE: hud_patch("M_SAVEG", 72, 28); break;
        case S_EPI: hud_patch("M_NEWG", 96, 14); hud_patch("M_EPISOD", 54, 38); break;
        case S_SKILL: hud_patch("M_NEWG", 96, 14); hud_patch("M_SKILL", 54, 38); break;
        case S_OPT: hud_patch("M_OPTTTL", 108, 15); break;
        case S_SND: hud_patch("M_SVOL", 60, 38); break;
        case S_JV: title("C4DOOM SETUP"); break;
        case S_JVV: title("VIDEO"); break;
        case S_JVC: title("CONTROLS"); break;
        case S_JVG: title("GAMEPLAY"); break;
        case S_JVX: title("CHEATS"); break;
        case S_JVP: title("PICTURE"); break;
        default: break;
    }
    for (int i = 0; i < M.nitems; i++) {
        It *it = &M.items[i];
        float y = M.itY[i];
        if (it->patch) hud_patch(it->patch, (float) M.ax, y);
        else hud_text(it->text, (float) M.ax, y + (isJV(M.scr) ? 3 : 0), isJV(M.scr) ? 1.5f : 2.0f);
        if (it->id == 31) hud_patch(GV.messagesOn ? "M_MSGON" : "M_MSGOFF", M.ax + 120.0f, y);
        if (it->id == 32) hud_patch(GV.lowDetail ? "M_GDLOW" : "M_GDHIGH", M.ax + 175.0f, y);
        if (it->slider > 0) thermo((float) M.ax, y + 16, it->slider, sliderValue(it->id));
    }
    if (M.nitems > 0) {
        bool anim = ((plat_nanotime() / 228000000LL) & 1) == 0;
        int s = mini(M.sel[M.scr], M.nitems - 1);
        hud_patch(anim ? "M_SKULL1" : "M_SKULL2", M.ax - 32.0f, M.itY[s] - 5);
    }
    /* soft back button */
    const char *label = M.scr == S_MAIN ? (GV.titleMode ? "< CLOSE" : "< RESUME") : "< BACK";
    hud_text(label, 6, 184, 1.6f);
}

/* ------------------------------------------------------------------ touch */
static bool in(const float *r, float x, float y) { return x >= r[0] && x <= r[2] && y >= r[1] && y <= r[3]; }
static bool backHit(float vx, float vy) { return vx >= 0 && vx <= 80 && vy >= 178; }

/* Row whose vertical band contains vy; above/below the list gives the first/last row (the skull just follows the finger). */
static int rowAt(float vy) {
    if (M.nitems == 0) return -1;
    for (int i = 0; i < M.nitems; i++)
        if (vy < M.itY[i] + 16 * rows(&M.items[i])) return i;
    return M.nitems - 1;
}

/* Right edge (virtual units) of everything drawn for this item, so only the real text/picture is "clickable". */
static float textRight(const It *it) {
    float r = (float) M.ax;
    if (it->patch) {
        Pic *p = gfx_patch(G_gfx, it->patch);
        if (p) r = M.ax - p->lo + p->w;
    } else if (it->hasText) {
        r = M.ax + hud_textW(it->text) * (isJV(M.scr) ? 1.5f : 2.0f);
    }
    if (it->id == 31) { Pic *p = gfx_patch(G_gfx, GV.messagesOn ? "M_MSGON" : "M_MSGOFF"); if (p) r = maxf(r, M.ax + 120 - p->lo + p->w); }
    if (it->id == 32) { Pic *p = gfx_patch(G_gfx, GV.lowDetail ? "M_GDLOW" : "M_GDHIGH"); if (p) r = maxf(r, M.ax + 175 - p->lo + p->w); }
    if (it->slider > 0) r = maxf(r, (float) (M.ax + 8 * (it->slider + 2)));
    return r;
}

/* True when (vx,vy) is on the text / picture of item i (not on the empty space beside it). */
static bool onText(int i, float vx, float vy) {
    if (i < 0 || i >= M.nitems) return false;
    const It *it = &M.items[i];
    if (vy < M.itY[i] || vy >= M.itY[i] + 16 * rows(it)) return false;
    return vx >= M.ax - 6 && vx <= textRight(it) + 6;
}

/* Slider is grabbed only on its thermometer row. */
static bool onThermo(int i, float vx, float vy) {
    if (i < 0 || i >= M.nitems) return false;
    const It *it = &M.items[i];
    return it->slider > 0 && vy >= M.itY[i] + 16 && vy < M.itY[i] + 32 && vx >= M.ax - 6 && vx <= M.ax + 8 * (it->slider + 2) + 6;
}

static void moveSel(int r) {
    if (r != M.sel[M.scr]) { M.sel[M.scr] = r; gv_beep("DSPSTOP"); }
}

bool menu_touch(int a, float x, float y) {
    float vx = (x - M.ox) / M.sx, vy = (y - M.oy) / M.sy;
    if (M.scr == S_HELP) {
        if (a == TA_UP) {
            if (M.helpPage == 0 && hud_has("HELP2")) M.helpPage = 1; else { M.scr = S_MAIN; menu_build(); }
        }
        return true;
    }
    if (M.scr == S_QUIT || M.scr == S_END) {
        if (a == TA_UP) {
            if (in(YES, vx, vy)) { if (M.scr == S_QUIT) gv_quitApp(); else gv_endGame(); }
            else if (in(NO, vx, vy)) menu_back();
        }
        return true;
    }
    if (a == TA_DOWN) {
        M.downVx = vx; M.downVy = vy; M.drag = false;
        if (backHit(vx, vy)) { M.downItem = -3; return true; }
        M.downItem = -1;
        int r = rowAt(vy);
        if (r < 0) return true;
        if (onThermo(r, vx, vy)) {                        /* grab slider */
            moveSel(r); M.drag = true; sliderFromTouch(vx);
        } else if (onText(r, vx, vy)) {                   /* on the text: will open / toggle on release */
            moveSel(r); M.downItem = r;
        } else {                                          /* empty space: only move the skull */
            moveSel(r);
        }
    } else if (a == TA_MOVE) {
        if (M.drag) { sliderFromTouch(vx); return true; }
        if (M.downItem == -3) return true;
        /* finger moved: it is a swipe / skull move, never a press */
        if (fabsf(vy - M.downVy) > 4 || fabsf(vx - M.downVx) > 8) M.downItem = -1;
        if (M.downItem == -1) { int r = rowAt(vy); if (r >= 0) moveSel(r); }
    } else if (a == TA_UP) {
        if (M.downItem == -3 && backHit(vx, vy)) menu_back();
        else if (M.downItem >= 0 && onText(M.downItem, vx, vy) && M.items[M.downItem].slider == 0) activate(M.items[M.downItem].id);
        M.drag = false; M.downItem = -2;
    } else if (a == TA_CANCEL) { M.drag = false; M.downItem = -2; }
    return true;
}
