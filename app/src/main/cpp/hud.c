/*
 * Draws the original Doom (1993) status bar, face, fonts and menu graphics straight from the WAD.
 * All coordinates are "virtual" Doom coordinates (320x200); hud_setT maps them to the screen.
 */
#include "doom.h"

Hud HUD;

void hud_init(void) {
    memset(&HUD, 0, sizeof HUD);
    HUD.sx = HUD.sy = 1;
    snprintf(HUD.face, sizeof HUD.face, "STFST01");
    HUD.lastTic = -1; HUD.idleLook = 1; HUD.hurtSeen = -1000; HUD.grinSeen = -1000;
}

void hud_setT(float sx, float sy, float ox, float oy) { HUD.sx = sx; HUD.sy = sy; HUD.ox = ox; HUD.oy = oy; }

bool hud_has(const char *name) { return gfx_patch(G_gfx, name) != NULL; }

void hud_patch(const char *name, float x, float y) { hud_patchK(name, x, y, 1.0f); }

/* Draw a patch at virtual (x,y), honouring its offsets like V_DrawPatch, scaled by k. */
void hud_patchK(const char *name, float x, float y, float k) {
    Pic *pc = gfx_patch(G_gfx, name);
    if (!pc) return;
    float l = HUD.ox + (x - pc->lo * k) * HUD.sx, t = HUD.oy + (y - pc->to * k) * HUD.sy;
    cv_drawPic(pc, G_gfx->lit[0], jround(l), jround(t), jround(l + pc->w * k * HUD.sx), jround(t + pc->h * k * HUD.sy), 0);
}

/* ------------------------------------------------------------------ font (STCFN) */
static Pic *glyph(char ch, char *nameOut) {
    if (ch >= 'a' && ch <= 'z') ch = (char) (ch - 32);
    if ((u8) ch < 33 || (u8) ch > 95) return NULL;
    snprintf(nameOut, 12, "STCFN%03d", (int) ch);
    return gfx_patch(G_gfx, nameOut);
}

/* Width in virtual units at scale 1. */
float hud_textW(const char *s) {
    float w = 0;
    char nm[12];
    for (; *s; s++) { Pic *g = glyph(*s, nm); w += g ? (float) g->w : 4; }
    return w;
}

void hud_text(const char *s, float x, float y, float k) {
    char nm[12];
    for (; *s; s++) {
        Pic *g = glyph(*s, nm);
        if (!g) { x += 4 * k; continue; }
        hud_patchK(nm, x, y, k);
        x += g->w * k;
    }
}

/* ------------------------------------------------------------------ numbers */
/* Right-aligned number made of prefix0..prefix9 patches, like STlib_drawNum. */
void hud_num(const char *pre, int v, float right, float y) {
    char nm[24];
    snprintf(nm, sizeof nm, "%s0", pre);
    Pic *z = gfx_patch(G_gfx, nm);
    if (!z) return;
    bool neg = v < 0;
    char s[16];
    snprintf(s, sizeof s, "%d", v < 0 ? -v : v);
    float x = right;
    for (int i = (int) strlen(s) - 1; i >= 0; i--) {
        x -= z->w;
        snprintf(nm, sizeof nm, "%s%c", pre, s[i]);
        hud_patch(nm, x, y);
    }
    if (neg) hud_patch("STTMINUS", x - 8, y);
}

/* ------------------------------------------------------------------ face logic */
static int painLevel(int health) {
    int h = maxi(0, mini(100, health));
    return ((100 - h) * 5) / 101;
}

/* Advance the face once per game tic. */
static void update(void) {
    Game *g = GM;
    Hud *h = &HUD;
    if (g->tic < h->lastTic) { h->hurtSeen = h->grinSeen = -1000; h->faceCount = 0; h->fireTics = 0; }
    if (g->tic == h->lastTic) return;
    h->lastTic = g->tic;
    int pain = painLevel(g->health);
    if (g->state == ST_DEAD || g->health <= 0) { snprintf(h->face, sizeof h->face, "STFDEAD0"); return; }
    if (g->invuln > 0 || g->god) { snprintf(h->face, sizeof h->face, "STFGOD0"); return; }
    if (g->hurtTic > h->hurtSeen) {
        h->hurtSeen = g->hurtTic;
        h->faceCount = 35;
        if (g->hurtAmount > 20) snprintf(h->face, sizeof h->face, "STFOUCH%d", pain);
        else if (g->hurtFrom) {
            if (absf(g->hurtAngle) < DOOM_PI / 4) snprintf(h->face, sizeof h->face, "STFKILL%d", pain);
            else snprintf(h->face, sizeof h->face, "%s%d0", g->hurtAngle > 0 ? "STFTL" : "STFTR", pain);
        } else snprintf(h->face, sizeof h->face, "STFOUCH%d", pain);
    } else if (g->grinTic > h->grinSeen) {
        h->grinSeen = g->grinTic;
        if (h->faceCount <= 0) { h->faceCount = 70; snprintf(h->face, sizeof h->face, "STFEVL%d", pain); }
    }
    if (g->inFire) h->fireTics++; else h->fireTics = 0;
    if (h->faceCount > 0) { h->faceCount--; return; }
    if (h->fireTics > 70) { snprintf(h->face, sizeof h->face, "STFKILL%d", pain); return; }
    if (--h->idleCount <= 0) { h->idleLook = game_rnd() % 3; h->idleCount = 17; }
    snprintf(h->face, sizeof h->face, "STFST%d%d", pain, h->idleLook);
}

/* ------------------------------------------------------------------ status bar */
/* Draws the 320x32 status bar; virtual y = 168 is the top of the bar. */
void hud_drawBar(void) {
    Game *g = GM;
    char nm[24];
    update();
    hud_patch("STBAR", 0, 168);
    hud_patch("STARMS", 104, 168);
    int am = game_currentAmmo();
    if (am >= 0) hud_num("STTNUM", am, 44, 171);
    hud_num("STTNUM", g->health, 90, 171);
    hud_patch("STTPRCNT", 90, 171);
    hud_num("STTNUM", g->armor, 221, 171);
    hud_patch("STTPRCNT", 221, 171);
    bool arms[6] = {g->owned[W_PISTOL], g->owned[W_SHOTGUN] || g->owned[W_SSG], g->owned[W_CHAINGUN],
                    g->owned[W_ROCKET], g->owned[W_PLASMA], g->owned[W_BFG]};
    for (int i = 0; i < 6; i++) {
        snprintf(nm, sizeof nm, "%s%d", arms[i] ? "STYSNUM" : "STGNUM", i + 2);
        hud_patch(nm, 111.0f + (i % 3) * 12, 172.0f + (i / 3) * 10);
    }
    hud_patch(HUD.face, 143, 168);
    for (int i = 0; i < 3; i++) if (g->keys[i]) { snprintf(nm, sizeof nm, "STKEYS%d", i); hud_patch(nm, 239, 171.0f + 10 * i); }
    for (int i = 0; i < 4; i++) {
        hud_num("STYSNUM", g->ammo[i], 288, 173.0f + 6 * i);
        hud_num("STYSNUM", g->maxAmmo[i], 314, 173.0f + 6 * i);
    }
}

/* Compact overlay for full-screen mode (no status bar): health, armor, ammo with the big Doom digits. */
void hud_drawMinimal(float sw, float sh, float s, float margin) {
    Game *g = GM;
    char nm[24];
    update();
    float k = s, kY = s * 1.2f;
    float by = sh - margin - 16 * kY;
    hud_setT(k, kY, margin, by);
    hud_num("STTNUM", g->health, 42, 0);
    hud_patch("STTPRCNT", 42, 0);
    hud_setT(k, kY, margin + 62 * k, by);
    hud_num("STTNUM", g->armor, 42, 0);
    hud_patch("STTPRCNT", 42, 0);
    int am = game_currentAmmo();
    if (am >= 0) {
        hud_setT(k, kY, sw - margin - 44 * k, by);
        hud_num("STTNUM", am, 44, 0);
    }
    hud_setT(k, kY, margin, by - 14 * kY);
    for (int i = 0; i < 3; i++) if (g->keys[i]) { snprintf(nm, sizeof nm, "STKEYS%d", i); hud_patch(nm, i * 9.0f, 0); }
}
