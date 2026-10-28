/* Game state and 35 Hz simulation: player, weapons, items, projectiles, level flow. */
#include "doom.h"

Game *GM;

const char *const W_NAME[9] = {"Fist", "Pistol", "Shotgun", "Chaingun", "Rocket Launcher", "Plasma Rifle", "BFG 9000", "Chainsaw", "Super Shotgun"};
static const char *const W_SPR[9] = {"PUNG", "PISG", "SHTG", "CHGG", "MISG", "PLSG", "BFGG", "SAWG", "SHT2"};
static const char *const W_FLASH[9] = {NULL, "PISF", "SHTF", "CHGF", "MISF", "PLSF", NULL, NULL, "SHT2"};
static const int W_AMMO[9] = {-1, 0, 1, 0, 2, 3, 3, -1, 1};
static const int W_USE[9] = {0, 1, 1, 1, 1, 1, 40, 0, 2};
/* attack steps: {frame, tics, flashFrame, fire} */
static const int W_ATKN[9] = {5, 4, 8, 2, 2, 2, 1, 2, 9};
static const int W_ATK[9][9][4] = {
    {{1, 4, -1, 0}, {2, 4, -1, 1}, {3, 5, -1, 0}, {2, 4, -1, 0}, {1, 5, -1, 0}},
    {{0, 4, -1, 0}, {1, 6, 0, 1}, {2, 4, -1, 0}, {1, 5, -1, 0}},
    {{0, 3, -1, 0}, {0, 7, 0, 1}, {1, 5, -1, 0}, {2, 5, -1, 0}, {3, 4, -1, 0}, {2, 5, -1, 0}, {1, 5, -1, 0}, {0, 3, -1, 0}},
    {{0, 4, 0, 1}, {1, 4, 1, 1}},
    {{1, 8, 0, 0}, {1, 12, 1, 1}},
    {{0, 3, 0, 1}, {1, 20, -1, 0}},
    {{0, 20, -1, 0}},
    {{0, 4, -1, 1}, {1, 4, -1, 1}},
    {{0, 3, -1, 0}, {0, 7, 8, 1}, {1, 7, 9, 0}, {2, 7, -1, 0}, {3, 7, -1, 0}, {4, 7, -1, 0}, {5, 7, -1, 0}, {6, 6, -1, 0}, {7, 6, -1, 0}}};

static u32 rngState = 2463534242u;
int game_rnd(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return (int) ((rngState >> 8) & 255);
}
int game_rnd_diff(void) { int a = game_rnd(); int b = game_rnd(); return a - b; }

Mobj *mobj_new(float x, float y, float z) {
    Mobj *m = (Mobj *) calloc(1, sizeof(Mobj));
    m->kind = MK_DECOR; m->radius = 16; m->height = 16; m->onFloor = true;
    m->x = x; m->y = y; m->z = z; m->ox = x; m->oy = y; m->oz = z;
    return m;
}

Game *game_new(Wad *wad, Gfx *gfx, Renderer *rend) {
    Game *g = (Game *) calloc(1, sizeof(Game));
    GM = g;
    g->wad = wad; g->gfx = gfx; g->rend = rend;
    g->skill = 3; g->aimAssist = true;
    g->health = 100; g->weapon = W_PISTOL; g->pending = -1;
    g->maxAmmo[0] = 200; g->maxAmmo[1] = 50; g->maxAmmo[2] = 50; g->maxAmmo[3] = 300;
    g->speedMul = g->takenMul = g->dealtMul = 1.0f;
    g->hurtTic = -1000; g->grinTic = -1000;
    g->psy = 128; g->psx = 1; g->wFrameOverride = -1; g->viewHeightDead = 41;
    rngState ^= (u32) plat_nanotime();
    if (rngState == 0) rngState = 2463534242u;
    return g;
}

void game_setRenderer(Renderer *r) {
    GM->rend = r;
    if (GM->L) {
        char sky[16];
        const char *m = GM->mapName;
        if (strncmp(m, "MAP", 3) == 0) { int n = game_mapNumber(m); snprintf(sky, sizeof sky, "%s", n < 12 ? "SKY1" : (n < 21 ? "SKY2" : "SKY3")); }
        else { snprintf(sky, sizeof sky, "SKY%c", m[1]); if (!gfx_texture(GM->gfx, sky)) snprintf(sky, sizeof sky, "SKY1"); }
        rend_setLevel(r, GM->L, gfx_texture(GM->gfx, sky));
    }
}

int game_mapNumber(const char *map) { return atoi(map + 3); }

/* ---- cheats (setup menu) */
void game_cheatWeapons(void) {
    for (int w = 0; w < 9; w++) if (gfx_sprite(GM->gfx, W_SPR[w])) GM->owned[w] = true;
    if (!GM->backpack) { GM->backpack = true; for (int i = 0; i < 4; i++) GM->maxAmmo[i] *= 2; }
    for (int i = 0; i < 4; i++) GM->ammo[i] = GM->maxAmmo[i];
    game_msg("All weapons and full ammo");
}
void game_cheatKeys(void) { GM->keys[0] = GM->keys[1] = GM->keys[2] = true; game_msg("All keys"); }
void game_cheatHealth(void) { GM->health = 100; GM->pl->health = 100; GM->armor = 200; GM->armorType = 2; game_msg("Full health and armor"); }
void game_cheatKillAll(void) {
    int n = 0;
    for (int i = 0, c = GM->mobjs.n; i < c; i++) {
        Mobj *m = GM->mobjs.d[i];
        if (m == GM->pl || !m->def || !m->shootable || m->removed) continue;
        game_damage(m, GM->pl, GM->pl, 10000); n++;
    }
    char buf[48];
    snprintf(buf, sizeof buf, "Monsters killed: %d", n);
    game_msg(buf);
}

void game_msg(const char *s) { snprintf(GM->message, sizeof GM->message, "%s", s); GM->msgTics = 105; }

void game_snd(const char *name, Mobj *at) {
    if (name) snd_play(name, at ? at->x : GM->pl->x, at ? at->y : GM->pl->y);
}
void game_sndAt(const char *name, float x, float y) { if (name) snd_play(name, x, y); }

void game_removeMobj(Mobj *m) {
    for (int i = GM->mobjs.n - 1; i >= 0; i--)
        if (GM->mobjs.d[i] == m) {
            memmove(&GM->mobjs.d[i], &GM->mobjs.d[i + 1], (size_t) (GM->mobjs.n - i - 1) * sizeof(Mobj *));
            GM->mobjs.n--;
            return;
        }
}

/* ================================================================== level setup */
static void resetPlayer(void) {
    Game *g = GM;
    g->health = 100; g->armor = 0; g->armorType = 0;
    memset(g->ammo, 0, sizeof g->ammo);
    memset(g->owned, 0, sizeof g->owned);
    g->maxAmmo[0] = 200; g->maxAmmo[1] = 50; g->maxAmmo[2] = 50; g->maxAmmo[3] = 300; g->backpack = false;
    g->owned[W_FIST] = true; g->owned[W_PISTOL] = true; g->ammo[0] = 50;
    g->weapon = W_PISTOL; g->pending = -1;
    g->berserk = false;
}

static void freeMobjs(void) {
    for (int i = 0; i < GM->mobjs.n; i++) free(GM->mobjs.d[i]);
    for (int i = 0; i < GM->spawnQueue.n; i++) free(GM->spawnQueue.d[i]);
    vec_clear(GM->mobjs); vec_clear(GM->spawnQueue);
    GM->pl = NULL;
}

bool game_startMap(const char *map, bool keepInventory, char *err, int errsz) {
    Game *g = GM;
    char tmp[128];
    if (!err) { err = tmp; errsz = sizeof tmp; }
    Level *nl = level_load(g->wad, g->gfx, map, err, errsz);
    if (!nl) return false;
    if (!keepInventory) resetPlayer();
    memset(g->keys, 0, sizeof g->keys);
    g->invuln = g->invis = g->suit = g->light = 0; g->hasMap = false;
    str_upper(g->mapName, map);
    freeMobjs();
    Level *old = g->L;
    g->L = nl;
    vec_clear(g->teleDests);
    level_free(old);
    g->kills = g->items = g->secrets = g->totalKills = g->totalItems = g->totalSecrets = g->levelTics = 0;
    g->tic = 0; g->state = ST_PLAY; g->wState = 0; g->psy = 128; g->wState = 2;   /* raise */
    g->damageFlash = g->bonusFlash = 0; g->message[0] = 0; g->msgTics = 0;
    g->hurtTic = -1000; g->hurtAmount = 0; g->grinTic = -1000; g->hurtFrom = false;
    g->nextMap[0] = 0; g->secretExit = false;
    int mask = g->skill <= 2 ? 1 : (g->skill == 3 ? 2 : 4);
    Thing *start = NULL;
    for (int i = 0; i < nl->nthings; i++) {
        Thing *t = &nl->things[i];
        if (t->type == 1) { start = t; continue; }
        if (t->type == 14) { vec_push(g->teleDests, t); continue; }
        if ((t->flags & 16) != 0) continue;
        if ((t->flags & mask) == 0) continue;
        game_spawnThing(t);
    }
    if (!start && nl->nthings > 0) start = &nl->things[0];
    float sx = start ? start->x : 0, sy = start ? start->y : 0;
    g->pl = mobj_new(sx, sy, level_sectorAt(nl, sx, sy)->floor);
    Mobj *pl = g->pl;
    pl->kind = MK_PLAYER; pl->radius = 16; pl->height = 56; pl->solid = true; pl->shootable = true; pl->health = g->health;
    pl->sector = level_sectorAt(nl, sx, sy); pl->floorz = pl->sector->floor; pl->ceilz = pl->sector->ceil;
    g->angle = start ? (float) (start->angle * (DOOM_PI / 180.0)) : 0;
    pl->angle = g->angle;
    g->eyeZ = g->eyeZPrev = pl->z + 41;
    vec_push(g->mobjs, pl);
    if (g->health <= 0) g->health = 100;
    if (!g->owned[g->weapon]) g->weapon = W_PISTOL;
    specials_init();               /* also drops the movers that pointed into the old level */
    game_setRenderer(g->rend);
    snd_listener(pl->x, pl->y, g->angle);
    return true;
}

Mobj *game_spawnThing(const Thing *t) {
    Game *g = GM;
    Sector *sec = level_sectorAt(g->L, t->x, t->y);
    MonsterDef *def = monsters_def(t->type);
    Mobj *m = mobj_new(t->x, t->y, sec->floor);
    m->type = t->type; m->sector = sec; m->angle = (float) (t->angle * (DOOM_PI / 180.0));
    m->floorz = sec->floor; m->ceilz = sec->ceil;
    if (def) {
        SpriteSet *ss = gfx_sprite(g->gfx, def->spr);
        if (!ss) { free(m); return NULL; }
        m->kind = MK_MONSTER; m->def = def; m->spr = ss;
        m->radius = def->radius; m->height = def->height; m->health = def->hp;
        m->solid = true; m->shootable = true; m->countKill = true; m->fuzz = def->fuzz;
        m->noGravity = def->fly; m->ambush = (t->flags & 8) != 0;
        g->totalKills++;
        ai_setSeq(m, SEQ_STAND, 0);
        m->tics = 1 + game_rnd() % 10;
        vec_push(g->mobjs, m);
        return m;
    }
    const ThingSpec *sp = things_spec(t->type);
    if (!sp) { free(m); return NULL; }
    SpriteSet *ss = gfx_sprite(g->gfx, sp->spr);
    if (!ss) { free(m); return NULL; }
    m->spec = sp; m->spr = ss; m->radius = sp->radius; m->height = sp->height; m->fullbright = sp->fullbright;
    m->frame = sp->frames[0] - 'A'; m->tics = sp->tics;
    m->solid = sp->solid;
    if (sp->hang) m->z = sec->ceil - sp->height;
    if (sp->barrel) {
        m->kind = MK_BARREL; m->shootable = true; m->health = 20;
    } else if (sp->pickup != 0) {
        m->kind = MK_PICKUP; m->pickupKind = sp->pickup; m->solid = false;
        if (sp->pickup < P_KEY_BLUE || sp->pickup > P_KEY_RED) m->countItem = true;
        if (m->countItem) g->totalItems++;
    }
    vec_push(g->mobjs, m);
    return m;
}

Mobj *game_spawnEffect(const char *spr, const char *frames, int tics, float x, float y, float z, bool bright) {
    SpriteSet *ss = gfx_sprite(GM->gfx, spr);
    if (!ss) return NULL;
    Mobj *e = mobj_new(x, y, z);
    e->kind = MK_EFFECT; e->spr = ss; e->fullbright = bright; e->missileName = frames; e->step = 0;
    e->frame = frames[0] - 'A'; e->tics = tics; e->animTics = tics; e->noGravity = true;
    e->sector = level_sectorAt(GM->L, x, y);
    vec_push(GM->spawnQueue, e);
    return e;
}

/* ================================================================== input */
void game_addTurn(float rad) { GM->turnAccum += rad; }

/* Called every render frame to apply smooth turning. */
void game_applyTurn(void) {
    float t = GM->turnAccum; GM->turnAccum = 0;
    if (t != 0 && GM->state == ST_PLAY) { GM->angle += t; GM->pl->angle = GM->angle; }
}

void game_selectWeapon(int w) { if (w >= 0 && w < 9 && GM->owned[w] && w != GM->weapon) GM->pending = w; }

void game_cycleWeapon(int dir) {
    Game *g = GM;
    int w = g->pending >= 0 ? g->pending : g->weapon;
    for (int i = 0; i < 9; i++) {
        w = (w + dir + 9) % 9;
        if (g->owned[w] && w != W_BFG && (W_AMMO[w] < 0 || g->ammo[W_AMMO[w]] >= W_USE[w])) { g->pending = w; return; }
    }
}

/* ================================================================== tick */
static void playerThink(void);
static void deadThink(void);
static void weaponThink(void);
static void missileThink(Mobj *m);
static void barrelThink(Mobj *m);
static void effectThink(Mobj *m);
static void decorThink(Mobj *m);

void game_tick(void) {
    Game *g = GM;
    if (g->state == ST_EXIT) return;
    g->tic++;
    for (int i = 0; i < g->mobjs.n; i++) { Mobj *m = g->mobjs.d[i]; m->ox = m->x; m->oy = m->y; m->oz = m->z; }
    g->eyeZPrev = g->eyeZ;
    if (g->state == ST_PLAY) { g->levelTics++; playerThink(); } else deadThink();
    specials_tick();
    for (int i = 0; i < g->mobjs.n; i++) {
        Mobj *m = g->mobjs.d[i];
        if (m == g->pl || m->removed) continue;
        switch (m->kind) {
            case MK_MONSTER: ai_think(m); break;
            case MK_MISSILE: missileThink(m); break;
            case MK_EFFECT: effectThink(m); break;
            case MK_BARREL: barrelThink(m); break;
            default: decorThink(m);
        }
        if (m->kind != MK_MONSTER) game_applyMomentum(m);
    }
    for (int i = 0; i < g->spawnQueue.n; i++) vec_push(g->mobjs, g->spawnQueue.d[i]);
    vec_clear(g->spawnQueue);
    /* sweep removed objects: forget references to them first, then free */
    bool any = false;
    for (int i = 0; i < g->mobjs.n; i++) if (g->mobjs.d[i]->removed) { any = true; break; }
    if (any) {
        for (int i = 0; i < g->mobjs.n; i++) {
            Mobj *m = g->mobjs.d[i];
            if (m->target && m->target->removed) m->target = NULL;
            if (m->owner && m->owner->removed) m->owner = NULL;
        }
        int w = 0;
        for (int i = 0; i < g->mobjs.n; i++) {
            Mobj *m = g->mobjs.d[i];
            if (m->removed) free(m); else g->mobjs.d[w++] = m;
        }
        g->mobjs.n = w;
    }
    if (g->damageFlash > 0) g->damageFlash--;
    if (g->bonusFlash > 0) g->bonusFlash--;
    if (g->msgTics > 0) g->msgTics--;
    snd_listener(g->pl->x, g->pl->y, g->angle);
}

static void decorThink(Mobj *m) {
    if (!m->spec || strlen(m->spec->frames) < 2) return;
    if (--m->tics <= 0) {
        size_t n = strlen(m->spec->frames);
        m->animTics = (int) ((m->animTics + 1) % (int) n);
        m->frame = m->spec->frames[m->animTics] - 'A';
        m->tics = m->spec->tics;
    }
}

static void barrelThink(Mobj *m) {
    if (m->exploding) {
        if (--m->tics <= 0) {
            m->step++;
            if (m->step >= 5) { m->removed = true; return; }
            m->frame = m->step;
            m->tics = 5;
            if (m->step == 2) game_radiusDamage(m, m->owner, 128, 128);
        }
        return;
    }
    decorThink(m);
}

static void effectThink(Mobj *m) {
    if (--m->tics <= 0) {
        m->step++;
        if (!m->missileName || m->step >= (int) strlen(m->missileName)) { m->removed = true; return; }
        m->frame = m->missileName[m->step] - 'A';
        m->tics = m->animTics;
    }
    m->z += m->mz;
}

void game_applyMomentum(Mobj *m) {
    if (m->kind == MK_MISSILE || m->kind == MK_EFFECT) return;
    if (m->mx != 0 || m->my != 0) {
        float steps = maxf(1, (float) ceil((double) (maxf(absf(m->mx), absf(m->my)) / 12.0f)));
        for (int i = 0; i < steps; i++) phys_slideMove(m, m->mx / steps, m->my / steps);
        m->mx *= 0.90625f; m->my *= 0.90625f;
        if (absf(m->mx) < 0.06f && absf(m->my) < 0.06f) { m->mx = 0; m->my = 0; }
    }
}

/* ================================================================== player */
void game_refreshFloor(Mobj *m) {
    if (GM->noclip && m == GM->pl) {
        m->sector = level_sectorAt(GM->L, m->x, m->y);
        m->floorz = m->sector->floor; m->ceilz = m->sector->ceil;
        return;
    }
    phys_checkPosition(m, m->x, m->y, true);
    m->floorz = PH.tmFloor; m->ceilz = PH.tmCeil;
    m->sector = level_sectorAt(GM->L, m->x, m->y);
}

void game_gravity(Mobj *m) {
    if (m->noGravity) return;
    if (m->z > m->floorz) { m->mz -= 1.0f; }
    m->z += m->mz;
    if (m->z <= m->floorz) {
        if (m->mz < -8 && m == GM->pl) game_snd("DSOOF", GM->pl);
        m->z = m->floorz; m->mz = 0;
    }
    if (m->z + m->height > m->ceilz) { m->z = m->ceilz - m->height; if (m->mz > 0) m->mz = 0; }
    if (m->z < m->floorz) m->z = m->floorz;     /* never sink below the floor when squeezed */
}

static void hurtPlayer(int n) { game_damage(GM->pl, NULL, NULL, n); }

static void kill(void) {
    GM->state = ST_DEAD; GM->pl->shootable = false; GM->health = 0; GM->pl->health = 0;
    game_snd("DSPLDETH", GM->pl);
    GM->wState = 1;
}

static void useLine(void) {
    Game *g = GM;
    float c = (float) cos((double) g->angle), s = (float) sin((double) g->angle);
    Line *best = specials_findUseLine(g->pl->x, g->pl->y, c, s, 64.0f);
    if (best) { if (!specials_useLine(best, g->pl)) game_snd("DSNOWAY", g->pl); }
    else game_snd("DSNOWAY", g->pl);
}

static bool touchPickup(int k);

static void touchPickups(void) {
    Game *g = GM;
    Mobj *pl = g->pl;
    for (int i = 0, n = g->mobjs.n; i < n; i++) {
        Mobj *o = g->mobjs.d[i];
        if (o->kind != MK_PICKUP || o->removed) continue;
        if (absf(o->x - pl->x) > pl->radius + o->radius || absf(o->y - pl->y) > pl->radius + o->radius) continue;
        if (pl->z > o->z + o->height || pl->z + pl->height < o->z) continue;
        if (touchPickup(o->pickupKind)) {
            o->removed = true;
            if (o->countItem) g->items++;
            g->bonusFlash = 6;
        }
    }
}

static void playerThink(void) {
    Game *g = GM;
    Mobj *pl = g->pl;
    game_refreshFloor(pl);
    float fwd = g->inMove * 50.0f * g->speedMul, side = g->inStrafe * 40.0f * g->speedMul;
    if (pl->z <= pl->floorz + 0.01f) {
        float c = (float) cos((double) g->angle), s = (float) sin((double) g->angle);
        pl->mx += (c * fwd + s * side) / 32.0f;
        pl->my += (s * fwd - c * side) / 32.0f;
    }
    float spd2 = pl->mx * pl->mx + pl->my * pl->my;
    g->bob = minf(16.0f, spd2 / 4.0f);
    float fr = 0.90625f;
    float steps = maxf(1, (float) ceil((double) (maxf(absf(pl->mx), absf(pl->my)) / 12.0f)));
    if (g->noclip) { pl->x += pl->mx; pl->y += pl->my; }
    else for (int i = 0; i < steps; i++) phys_slideMove(pl, pl->mx / steps, pl->my / steps);
    if (pl->z <= pl->floorz + 0.01f) { pl->mx *= fr; pl->my *= fr; }
    if (absf(pl->mx) < 0.06f && absf(pl->my) < 0.06f && fwd == 0 && side == 0) { pl->mx = 0; pl->my = 0; }
    game_gravity(pl);
    pl->angle = g->angle;
    float target = pl->z + 41 + 0.5f * g->bob * (float) sin((double) (g->tic * 0.314f));
    if (target > g->eyeZ) g->eyeZ += minf(target - g->eyeZ, 6.0f); else g->eyeZ += (target - g->eyeZ) * 0.5f;

    /* use */
    if (g->inUse) { if (!g->useHeld) { g->useHeld = true; useLine(); } } else g->useHeld = false;

    /* sector effects */
    Sector *s = pl->sector;
    if (s->special == 9) { s->special = 0; g->secrets++; game_msg("A secret is revealed!"); }
    if (pl->z <= pl->floorz + 0.5f && (g->tic & 31) == 0) {
        switch (s->special) {
            case 5: if (g->suit <= 0) hurtPlayer(10); break;
            case 7: if (g->suit <= 0) hurtPlayer(5); break;
            case 16: case 4: if (g->suit <= 0 || game_rnd() < 5) hurtPlayer(s->special == 4 ? 10 : 20); break;
            case 11: hurtPlayer(20); break;
            default: break;
        }
    }
    if (s->special == 11 && g->health <= 10) game_exitLevel(false);
    specials_checkCross(pl, g->lastPx, g->lastPy); g->lastPx = pl->x; g->lastPy = pl->y;
    touchPickups();
    if (g->invuln > 0) g->invuln--;
    if (g->invis > 0) g->invis--;
    if (g->suit > 0) g->suit--;
    if (g->light > 0) g->light--;
    weaponThink();
    if (g->health <= 0 && g->state == ST_PLAY) kill();
}

static void deadThink(void) {
    Game *g = GM;
    game_refreshFloor(g->pl); game_gravity(g->pl); game_applyMomentum(g->pl);
    g->viewHeightDead = maxi(6, g->viewHeightDead - 1);
    g->eyeZ = g->pl->z + g->viewHeightDead;
    if (g->psy < 128) g->psy += 6;
}

static void nextMapName(const char *m, bool secret, char out[9]) {
    out[0] = 0;
    if (strncmp(m, "MAP", 3) == 0) {
        int n = game_mapNumber(m);
        int nx = n + 1;
        if (secret) { if (n == 15) nx = 31; else if (n == 31) nx = 32; }
        else if (n == 31 || n == 32) nx = 16;
        else if (n == 30) return;
        snprintf(out, 9, "MAP%02d", nx);
        return;
    }
    int e = m[1] - '0', mp = m[3] - '0';
    if (secret) { snprintf(out, 9, "E%dM9", e); return; }
    if (mp == 9) { int ret = e == 1 ? 4 : (e == 2 ? 6 : (e == 3 ? 7 : 3)); snprintf(out, 9, "E%dM%d", e, ret); return; }
    if (mp == 8) return;
    snprintf(out, 9, "E%dM%d", e, mp + 1);
}

void game_exitLevel(bool secret) {
    if (GM->state == ST_EXIT) return;
    GM->secretExit = secret;
    nextMapName(GM->mapName, secret, GM->nextMap);
    GM->state = ST_EXIT;
    game_snd("DSSWTCHX", GM->pl);
}

/* ================================================================== pickups */
static bool giveAmmo(int t, int n) {
    Game *g = GM;
    if (g->ammo[t] >= g->maxAmmo[t]) return false;
    g->ammo[t] = mini(g->maxAmmo[t], g->ammo[t] + n);
    return true;
}

static bool giveWeapon(int w, int ammoAdd) {
    Game *g = GM;
    bool had = g->owned[w];
    bool gotAmmo = W_AMMO[w] < 0 ? false : giveAmmo(W_AMMO[w], ammoAdd);
    if (had && !gotAmmo) return false;
    g->owned[w] = true;
    if (!had) { g->pending = w; g->grinTic = g->tic; }
    game_snd("DSWPNUP", g->pl);
    return true;
}

static bool giveArmor(int type, int pts) {
    Game *g = GM;
    if (g->armor >= pts && type > 0 && g->armor >= pts) return false;
    g->armorType = type; g->armor = pts; return true;
}

static void heal(int n) { GM->health = mini(100, GM->health + n); GM->pl->health = GM->health; }

static bool touchPickup(int k) {
    Game *g = GM;
    const char *sound = "DSITEMUP";
    switch (k) {
        case P_CLIP: if (!giveAmmo(0, 10)) return false; game_msg("Picked up a clip."); break;
        case P_CLIPBOX: if (!giveAmmo(0, 50)) return false; game_msg("Picked up a box of bullets."); break;
        case P_SHELL: if (!giveAmmo(1, 4)) return false; game_msg("Picked up 4 shotgun shells."); break;
        case P_SHELLBOX: if (!giveAmmo(1, 20)) return false; game_msg("Picked up a box of shotgun shells."); break;
        case P_ROCKET: if (!giveAmmo(2, 1)) return false; game_msg("Picked up a rocket."); break;
        case P_ROCKETBOX: if (!giveAmmo(2, 5)) return false; game_msg("Picked up a box of rockets."); break;
        case P_CELL: if (!giveAmmo(3, 20)) return false; game_msg("Picked up an energy cell."); break;
        case P_CELLPACK: if (!giveAmmo(3, 100)) return false; game_msg("Picked up an energy cell pack."); break;
        case P_BACKPACK:
            if (!g->backpack) { g->backpack = true; for (int i = 0; i < 4; i++) g->maxAmmo[i] *= 2; }
            for (int i = 0; i < 4; i++) giveAmmo(i, i == 0 ? 10 : (i == 1 ? 4 : (i == 2 ? 1 : 20)));
            game_msg("Picked up a backpack full of ammo!"); break;
        case P_STIM: if (g->health >= 100) return false; heal(10); game_msg("Picked up a stimpack."); break;
        case P_MEDI: if (g->health >= 100) return false; heal(25); game_msg("Picked up a medikit."); break;
        case P_HBONUS: g->health = mini(200, g->health + 1); g->pl->health = g->health; game_msg("Picked up a health bonus."); break;
        case P_ABONUS: g->armor = mini(200, g->armor + 1); if (g->armorType == 0) g->armorType = 1; game_msg("Picked up an armor bonus."); break;
        case P_ARMOR1: if (!giveArmor(1, 100)) return false; game_msg("Picked up the armor."); break;
        case P_ARMOR2: if (!giveArmor(2, 200)) return false; game_msg("Picked up the MegaArmor!"); break;
        case P_SOUL: g->health = mini(200, g->health + 100); g->pl->health = g->health; game_msg("Supercharge!"); sound = "DSGETPOW"; break;
        case P_MEGA: g->health = 200; g->pl->health = 200; g->armor = 200; g->armorType = 2; game_msg("MegaSphere!"); sound = "DSGETPOW"; break;
        case P_INVULN: g->invuln = 30 * 35; game_msg("Invulnerability!"); sound = "DSGETPOW"; break;
        case P_BERSERK: g->berserk = true; heal(100); if (g->weapon != W_FIST) g->pending = W_FIST; game_msg("Berserk!"); sound = "DSGETPOW"; break;
        case P_INVIS: g->invis = 60 * 35; game_msg("Partial Invisibility"); sound = "DSGETPOW"; break;
        case P_SUIT: g->suit = 60 * 35; game_msg("Radiation Shielding Suit"); sound = "DSGETPOW"; break;
        case P_MAP: g->hasMap = true; game_msg("Computer Area Map"); sound = "DSGETPOW"; break;
        case P_LIGHT: g->light = 120 * 35; game_msg("Light Amplification Visor"); sound = "DSGETPOW"; break;
        case P_KEY_BLUE: if (g->keys[0]) return false; g->keys[0] = true; game_msg("Picked up a blue keycard."); break;
        case P_KEY_YELLOW: if (g->keys[1]) return false; g->keys[1] = true; game_msg("Picked up a yellow keycard."); break;
        case P_KEY_RED: if (g->keys[2]) return false; g->keys[2] = true; game_msg("Picked up a red keycard."); break;
        case P_W_SHOTGUN: if (!giveWeapon(W_SHOTGUN, 8)) return false; game_msg("You got the shotgun!"); sound = NULL; break;
        case P_W_CHAINGUN: if (!giveWeapon(W_CHAINGUN, 20)) return false; game_msg("You got the chaingun!"); sound = NULL; break;
        case P_W_ROCKET: if (!giveWeapon(W_ROCKET, 2)) return false; game_msg("You got the rocket launcher!"); sound = NULL; break;
        case P_W_PLASMA: if (!giveWeapon(W_PLASMA, 40)) return false; game_msg("You got the plasma gun!"); sound = NULL; break;
        case P_W_SAW: if (g->owned[W_SAW]) return false; g->owned[W_SAW] = true; g->pending = W_SAW; g->grinTic = g->tic; game_msg("A chainsaw! Find some meat!"); sound = "DSWPNUP"; break;
        case P_W_SSG: if (!giveWeapon(W_SSG, 8)) return false; game_msg("You got the super shotgun!"); sound = NULL; break;
        default: return false;
    }
    if (sound) game_snd(sound, g->pl);
    return true;
}

/* ================================================================== weapons */
static SpriteSet *wSprite(void) { return gfx_sprite(GM->gfx, W_SPR[GM->weapon]); }
static bool hasAmmo(int w) { return W_AMMO[w] < 0 || GM->ammo[W_AMMO[w]] >= W_USE[w]; }

static void bestWeapon(void) {
    static const int pref[8] = {W_PLASMA, W_SSG, W_CHAINGUN, W_SHOTGUN, W_PISTOL, W_ROCKET, W_SAW, W_FIST};
    for (int i = 0; i < 8; i++) {
        int w = pref[i];
        if (GM->owned[w] && hasAmmo(w) && gfx_sprite(GM->gfx, W_SPR[w])) { GM->pending = w; return; }
    }
    GM->pending = W_FIST;
}

void game_shoot(Mobj *src, float a, float range, float slope, int dmg) {
    float z0 = src->z + (src->kind == MK_PLAYER ? 41.0f : src->height * 0.75f);
    Hit h = phys_trace(src, src->x, src->y, z0, a, range, slope);
    if (h.mobj) {
        Mobj *t = h.mobj;
        if (t->kind == MK_BARREL || t == GM->pl) game_spawnEffect("PUFF", "ABCD", 4, h.x, h.y, h.z, true);
        else game_spawnEffect("BLUD", "CBA", 8, h.x, h.y, h.z, false);
        game_damage(t, src, src, dmg);
    } else if (h.line) {
        game_spawnEffect("PUFF", "ABCD", 4, h.x, h.y, h.z, true);
        specials_shootLine(h.line, src);
    }
}

static void bullet(bool accurate, int pellets, float spread) {
    Game *g = GM;
    phys_aim(g->pl, g->angle, 2048.0f, g->aimAssist);
    float slope = PH.aimSlope, baseAng = PH.aimAngle;
    for (int i = 0; i < pellets; i++) {
        float a = baseAng;
        if (!accurate) a += (float) game_rnd_diff() * spread;
        float sl = slope + (pellets > 1 ? (float) game_rnd_diff() * 0.0005f : 0.0f);
        int dmg = 5 * (game_rnd() % 3 + 1);
        game_shoot(g->pl, a, 2048.0f, sl, dmg);
    }
}

void game_noiseAlert(void) { specials_soundAlert(GM->pl); }

static void fireWeapon(void) {
    Game *g = GM;
    int w = g->weapon;
    if (W_AMMO[w] >= 0 && !g->infAmmo) g->ammo[W_AMMO[w]] -= W_USE[w];
    game_noiseAlert();
    switch (w) {
        case W_FIST: case W_SAW: {
            float range = w == W_FIST ? 64.0f : 65.0f;
            float a = g->angle + (float) game_rnd_diff() * 0.000383f;
            phys_aim(g->pl, a, range, g->aimAssist);
            int dmg = (game_rnd() % 10 + 1) * 2;
            if (w == W_FIST && g->berserk) dmg *= 10;
            Hit h = phys_trace(g->pl, g->pl->x, g->pl->y, g->pl->z + 41, PH.aimAngle, range, PH.aimSlope);
            if (h.mobj) {
                game_snd(w == W_FIST ? "DSPUNCH" : "DSSAWHIT", g->pl);
                game_spawnEffect("BLUD", "CBA", 8, h.x, h.y, h.z, false);
                game_damage(h.mobj, g->pl, g->pl, dmg);
                float da = (float) atan2((double) (h.mobj->y - g->pl->y), (double) (h.mobj->x - g->pl->x));
                float diff = phys_normAngle(da - g->angle);
                g->angle += diff * 0.4f; g->pl->angle = g->angle;
            } else game_snd(w == W_SAW ? "DSSAWFUL" : NULL, g->pl);
            break;
        }
        case W_PISTOL: game_snd("DSPISTOL", g->pl); bullet(g->refire == 0, 1, 0.000383f); break;
        case W_CHAINGUN: game_snd("DSPISTOL", g->pl); bullet(g->refire == 0, 1, 0.000383f); break;
        case W_SHOTGUN: game_snd("DSSHOTGN", g->pl); bullet(false, 7, 0.000383f); break;
        case W_SSG: game_snd("DSDSHTGN", g->pl); bullet(false, 20, 0.000767f); break;
        case W_ROCKET:
            phys_aim(g->pl, g->angle, 2048.0f, g->aimAssist);
            game_spawnMissile(g->pl, monsters_missile("ROCKET"), PH.aimAngle, PH.aimSlope, 32.0f);
            break;
        case W_PLASMA:
            phys_aim(g->pl, g->angle, 2048.0f, g->aimAssist);
            game_spawnMissile(g->pl, monsters_missile("PLASMA"), PH.aimAngle, PH.aimSlope, 32.0f);
            break;
        default: break;
    }
}

static void weaponThink(void) {
    Game *g = GM;
    if (g->state != ST_PLAY) return;
    switch (g->wState) {
        case 0: /* ready */
            if (g->pending == g->weapon) g->pending = -1;
            if (g->pending >= 0 && g->pending != g->weapon) { g->wState = 1; break; }
            if (g->inFire) {
                if (hasAmmo(g->weapon) && g->weapon != W_BFG) {
                    g->wState = 3; g->wStep = 0; g->wTics = W_ATK[g->weapon][0][1]; g->refire = 0;
                    if (W_ATK[g->weapon][0][3] != 0) fireWeapon();
                } else if (g->weapon != W_BFG) bestWeapon();
            }
            if (g->weapon == W_SAW) g->wFrameOverride = 2 + ((g->tic >> 2) & 1); else g->wFrameOverride = -1;
            g->psx = 1 + g->bob * (float) cos(g->tic * DOOM_PI / 32);
            g->psy = 32 + g->bob * absf((float) sin(g->tic * DOOM_PI / 32));
            break;
        case 1: /* lowering */
            g->psy += 6;
            if (g->psy >= 128) {
                g->psy = 128;
                if (g->pending >= 0) { g->weapon = g->pending; g->pending = -1; }
                if (!g->owned[g->weapon]) g->weapon = W_PISTOL;
                g->wState = 2;
            }
            break;
        case 2: /* raising */
            g->psy -= 6;
            if (g->pending >= 0 && g->pending != g->weapon) { g->wState = 1; break; }
            if (g->psy <= 32) { g->psy = 32; g->wState = 0; }
            break;
        case 3: /* attacking */
            if (--g->wTics <= 0) {
                const int (*st)[4] = W_ATK[g->weapon];
                int len = W_ATKN[g->weapon];
                g->wStep++;
                /* original A_ReFire sits at the start of the plasma rifle's long 20 tic state: while the button is held the gun
                 * goes straight back to firing, the long pause is only taken after the button is released */
                if (g->weapon == W_PLASMA && g->wStep == 1 && g->inFire && hasAmmo(g->weapon)) {
                    g->wStep = 0; g->refire++; g->wTics = st[0][1];
                    fireWeapon();
                    break;
                }
                if (g->wStep >= len) {
                    if (g->inFire && hasAmmo(g->weapon)) { g->wStep = 0; g->refire++; }
                    else { g->wState = 0; g->refire = 0; g->wFrameOverride = -1; if (!hasAmmo(g->weapon)) bestWeapon(); break; }
                }
                g->wTics = st[g->wStep][1];
                if (st[g->wStep][3] != 0 && hasAmmo(g->weapon)) fireWeapon();
                else if (st[g->wStep][3] != 0) { g->wStep = len - 1; }
            }
            g->psx = 1; g->psy = 32;
            break;
    }
}

Mobj *game_spawnMissile(Mobj *src, MissileDef *md, float a, float slope, float zoff) {
    SpriteSet *ss = gfx_sprite(GM->gfx, md->spr);
    if (!ss) return NULL;
    float c = (float) cos((double) a), s = (float) sin((double) a);
    float off = src->kind == MK_PLAYER ? 4.0f : src->radius * 0.5f;
    Mobj *m = mobj_new(src->x + c * off, src->y + s * off, src->z + zoff);
    m->kind = MK_MISSILE; m->miss = md; m->spr = ss; m->fullbright = md->fullbright; m->owner = src;
    m->radius = md->radius; m->height = md->height; m->noGravity = true; m->angle = a;
    m->mx = c * md->speed; m->my = s * md->speed;
    float h2 = (float) sqrt((double) (1 + slope * slope));
    m->mx /= h2; m->my /= h2; m->mz = slope * md->speed / h2;
    m->frame = md->fly[0] - 'A'; m->tics = md->flyTics; m->step = 0;
    m->sector = level_sectorAt(GM->L, m->x, m->y);
    m->floorz = m->sector->floor; m->ceilz = m->sector->ceil;
    vec_push(GM->spawnQueue, m);
    game_snd(md->launchSnd, src);
    return m;
}

static void explode(Mobj *m) {
    m->exploding = true; m->mx = m->my = m->mz = 0; m->step = 0;
    m->frame = m->miss->boom[0] - 'A'; m->tics = m->miss->boomTics;
    game_snd(m->miss->hitSnd, m);
    if (m->miss->splash) game_radiusDamage(m, m->owner, 128, 128);
}

static void missileThink(Mobj *m) {
    Game *g = GM;
    if (m->exploding) {
        if (--m->tics <= 0) {
            m->step++;
            if (m->step >= (int) strlen(m->miss->boom)) { m->removed = true; return; }
            m->frame = m->miss->boom[m->step] - 'A'; m->tics = m->miss->boomTics;
        }
        return;
    }
    if (--m->tics <= 0) {
        m->step = (m->step + 1) % (int) strlen(m->miss->fly);
        m->frame = m->miss->fly[m->step] - 'A'; m->tics = m->miss->flyTics;
    }
    int parts = (int) ceil((double) (m->miss->speed / 8.0f));
    for (int i = 0; i < parts; i++) {
        float nx = m->x + m->mx / parts, ny = m->y + m->my / parts, nz = m->z + m->mz / parts;
        m->z = nz;
        if (!phys_checkPosition(m, nx, ny, false)) {
            if (PH.tmBlockMobj && PH.tmBlockMobj != m->owner) {
                Mobj *t = PH.tmBlockMobj;
                int dmg = m->miss->dmgMul * (game_rnd() % m->miss->dmgDie + 1);
                if (t->shootable) game_damage(t, m, m->owner, dmg);
                explode(m);
            } else if (PH.tmBlockLine) {
                Line *l = PH.tmBlockLine;
                if (!l->back && l->fs && l->fs->skyCeil && m->z + m->height > l->fs->ceil - 1) { m->removed = true; return; }
                explode(m);
            } else explode(m);
            return;
        }
        if (m->z + m->height >= PH.tmCeil) {
            Sector *s = level_sectorAt(g->L, nx, ny);
            if (s->skyCeil) { m->removed = true; return; }
            explode(m); return;
        }
        if (m->z <= PH.tmFloor) { explode(m); return; }
        m->x = nx; m->y = ny; m->sector = level_sectorAt(g->L, nx, ny);
    }
}

void game_radiusDamage(Mobj *center, Mobj *source, int dmg, float radius) {
    for (int i = 0, n = GM->mobjs.n; i < n; i++) {
        Mobj *o = GM->mobjs.d[i];
        if (o->removed || !o->shootable) continue;
        float d = maxf(absf(o->x - center->x), absf(o->y - center->y)) - o->radius;
        if (d < 0) d = 0;
        if (d >= radius) continue;
        if (!phys_los(center->x, center->y, center->z + 8, o->x, o->y, o->z + o->height * 0.5f)) continue;
        game_damage(o, center, source, (int) (dmg - d));
    }
}

/* ================================================================== damage */
void game_damage(Mobj *t, Mobj *inflictor, Mobj *source, int amount) {
    Game *g = GM;
    Mobj *pl = g->pl;
    if (!t->shootable || amount <= 0) return;
    if (t == pl) { if (g->takenMul <= 0.0f) return; amount = maxi(1, jround(amount * g->takenMul)); }
    else if (inflictor == pl || source == pl) amount = maxi(1, jround(amount * g->dealtMul));
    if (inflictor && inflictor != t && (t->kind == MK_MONSTER || t->kind == MK_PLAYER)) {
        float a = (float) atan2((double) (t->y - inflictor->y), (double) (t->x - inflictor->x));
        int mass = t->def ? t->def->mass : 100;
        float th = amount * 12.5f / mass;
        if (th > 30) th = 30;
        t->mx += (float) cos((double) a) * th; t->my += (float) sin((double) a) * th;
    }
    if (t == pl) {
        if (g->invuln > 0 || g->god) return;
        if (g->armorType > 0) {
            int saved = g->armorType == 1 ? amount / 3 : amount / 2;
            if (g->armor <= saved) saved = g->armor;
            g->armor -= saved; amount -= saved;
            if (g->armor <= 0) g->armorType = 0;
        }
        g->health -= amount; pl->health = g->health;
        g->hurtTic = g->tic; g->hurtAmount = amount;
        g->hurtFrom = source && source != pl;
        if (g->hurtFrom) {
            double da = atan2((double) (source->y - pl->y), (double) (source->x - pl->x)) - g->angle;
            while (da > DOOM_PI) da -= 2 * DOOM_PI;
            while (da < -DOOM_PI) da += 2 * DOOM_PI;
            g->hurtAngle = (float) da;
        }
        g->damageFlash = mini(40, g->damageFlash + amount / 2 + 3);
        if (g->health > 0) game_snd("DSPLPAIN", pl);
        if (g->health <= 0) g->health = 0;
        return;
    }
    t->health -= amount;
    if (t->kind == MK_BARREL) {
        if (t->health <= 0 && !t->exploding) {
            SpriteSet *ss = gfx_sprite(g->gfx, "BEXP");
            t->exploding = true; t->owner = source; t->shootable = false; t->solid = false;
            t->fullbright = true;
            if (ss) { t->spr = ss; t->step = 0; t->frame = 0; t->tics = 5; }
            else { game_radiusDamage(t, source, 128, 128); t->removed = true; }
            game_snd("DSBAREXP", t);
        }
        return;
    }
    ai_hurt(t, inflictor, source, amount);
}

/* ================================================================== render */
void game_render(float alpha) {
    Game *g = GM;
    if (!g->pl) return;
    game_applyTurn();
    Mobj *pl = g->pl;
    float px = pl->ox + (pl->x - pl->ox) * alpha, py = pl->oy + (pl->y - pl->oy) * alpha;
    float ez = g->eyeZPrev + (g->eyeZ - g->eyeZPrev) * alpha;
    g->rend->alpha = alpha;
    SpriteSet *ws = (g->state == ST_PLAY || g->state == ST_DEAD) ? wSprite() : NULL;
    int wf = 0, ff = -1;
    SpriteSet *fs = NULL;
    if (ws) {
        wf = g->wFrameOverride >= 0 ? g->wFrameOverride : 0;
        if (g->wState == 3) {
            const int *st = W_ATK[g->weapon][mini(g->wStep, W_ATKN[g->weapon] - 1)];
            wf = st[0];
            if (st[2] >= 0 && W_FLASH[g->weapon]) { fs = gfx_sprite(g->gfx, W_FLASH[g->weapon]); ff = st[2]; }
        }
    }
    int fixed = g->invuln > 0 ? 32 : (g->light > 0 ? 1 : -1);
    R_weaponFuzz = g->invis > 4 * 32 || (g->invis & 8) != 0;
    rend_render(g->rend, px, py, ez, g->angle, &g->mobjs, pl, ws, wf, fs, ff, g->psx, g->psy, pl->sector ? pl->sector->light : 160, fixed);
}

int game_currentAmmo(void) { return W_AMMO[GM->weapon] >= 0 ? GM->ammo[W_AMMO[GM->weapon]] : -1; }
