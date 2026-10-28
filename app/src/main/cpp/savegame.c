/*
 * Save / load of a game in progress.
 *
 * A save stores the whole state of the level on top of the map itself: every sector, side and line,
 * the moving doors / lifts / floors, the light effects, all things in the world (monsters, items,
 * missiles, effects) and the player. Loading builds the same map again and then puts that state back.
 */
#include "doom.h"
#include <time.h>

#define MAGIC 0x56533443u      /* "C4SV" */
#define VERSION 1

typedef struct { u8 *d; int n, cap; } Buf;
typedef struct { const u8 *d; int n, p; bool err; } Rd;

static void bput(Buf *b, const void *src, int len) {
    if (b->n + len > b->cap) { b->cap = (b->n + len) * 2 + 4096; b->d = (u8 *) realloc(b->d, (size_t) b->cap); }
    memcpy(b->d + b->n, src, (size_t) len); b->n += len;
}
static void wI(Buf *b, int v) { bput(b, &v, 4); }
static void wF(Buf *b, float v) { bput(b, &v, 4); }
static void wB(Buf *b, bool v) { u8 c = v ? 1 : 0; bput(b, &c, 1); }
static void wS(Buf *b, const char *s) { int n = s ? (int) strlen(s) : -1; wI(b, n); if (n > 0) bput(b, s, n); }

static void rget(Rd *r, void *dst, int len) {
    if (r->p + len > r->n) { r->err = true; memset(dst, 0, (size_t) len); return; }
    memcpy(dst, r->d + r->p, (size_t) len); r->p += len;
}
static int rI(Rd *r) { int v; rget(r, &v, 4); return v; }
static float rF(Rd *r) { float v; rget(r, &v, 4); return v; }
static bool rB(Rd *r) { u8 c; rget(r, &c, 1); return c != 0; }
/* returns false for a null string */
static bool rS(Rd *r, char *out, int max) {
    int n = rI(r);
    if (n < 0) { out[0] = 0; return false; }
    if (n >= max || r->p + n > r->n) { r->err = true; out[0] = 0; return false; }
    memcpy(out, r->d + r->p, (size_t) n); out[n] = 0; r->p += n;
    return true;
}

static void savePath(int slot, char *out, int sz) { snprintf(out, (size_t) sz, "%s/c4doom_save%d.sav", plat_data_dir(), slot); }

static u8 *readFile(const char *path, int *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    u8 *d = (u8 *) malloc((size_t) n);
    if (fread(d, 1, (size_t) n, f) != (size_t) n) { fclose(f); free(d); return NULL; }
    fclose(f);
    *len = (int) n;
    return d;
}

/* Text shown in the menu, or false when the slot is empty or unreadable. */
bool save_describe(int slot, char *out, int outsz) {
    char path[512];
    savePath(slot, path, sizeof path);
    int len;
    u8 *d = readFile(path, &len);
    if (!d) return false;
    Rd r = {d, len, 0, false};
    bool ok = false;
    if ((u32) rI(&r) == MAGIC && rI(&r) == VERSION) {
        char desc[64];
        if (rS(&r, desc, sizeof desc) && !r.err) { snprintf(out, (size_t) outsz, "%s", desc); ok = true; }
    }
    free(d);
    return ok;
}

/* ------------------------------------------------------------------ names <-> pointers */
static const char *spriteName(Gfx *g, SpriteSet *ss) {
    for (int i = 0; i < g->sprites.cap; i++) if (g->sprites.keys[i][0] && g->sprites.vals[i] == ss) return g->sprites.keys[i];
    return NULL;
}

/* effect frame strings are literals in the code; keep one persistent copy of each distinct string */
static const char *intern(const char *s) {
    static char *pool[32];
    static int n;
    for (int i = 0; i < n; i++) if (strcmp(pool[i], s) == 0) return pool[i];
    if (n >= 32) return pool[0];
    pool[n] = strdup(s);
    return pool[n++];
}

/* ------------------------------------------------------------------ save */
static void writeState(Buf *b) {
    Game *g = GM;
    Level *L = g->L;
    int sec = g->levelTics / 35;
    char date[32];
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(date, sizeof date, "%d.%m %H:%M", &tmv);
    char desc[64];
    snprintf(desc, sizeof desc, "%s %d:%02d %s", g->mapName, sec / 60, sec % 60, date);
    wI(b, (int) MAGIC); wI(b, VERSION);
    wS(b, desc); wS(b, g->mapName); wI(b, g->skill);

    wI(b, L->nsectors);
    for (int i = 0; i < L->nsectors; i++) {
        Sector *s = &L->sectors[i];
        wF(b, s->floor); wF(b, s->ceil); wS(b, s->floorName); wS(b, s->ceilName); wB(b, s->skyCeil);
        wI(b, s->light); wI(b, s->special); wI(b, s->tag); wB(b, s->secretFound);
    }
    wI(b, L->nsides);
    for (int i = 0; i < L->nsides; i++) {
        Side *s = &L->sides[i];
        wF(b, s->xoff); wF(b, s->yoff); wS(b, s->upperName); wS(b, s->lowerName); wS(b, s->midName);
    }
    wI(b, L->nlines);
    for (int i = 0; i < L->nlines; i++) {
        Line *l = &L->lines[i];
        wI(b, l->flags); wI(b, l->special); wI(b, l->tag); wB(b, l->used); wB(b, l->mapped);
    }

    Specials *sp = &g->sp;
    wI(b, sp->movers.n);
    for (int i = 0; i < sp->movers.n; i++) {
        Mover *m = sp->movers.d[i];
        wI(b, m->kind); wF(b, m->target); wF(b, m->speed); wI(b, m->state); wI(b, m->wait); wI(b, m->waitMax); wF(b, m->home);
        wB(b, m->useCeil); wB(b, m->once); wB(b, m->reopen); wB(b, m->reopenWait);
        wI(b, m->sector ? (int) (m->sector - L->sectors) : -1);
        wI(b, m->triggerLine ? (int) (m->triggerLine - L->lines) : -1);
    }
    wI(b, sp->lights.n);
    for (int i = 0; i < sp->lights.n; i++) {
        LightFx *f = &sp->lights.d[i];
        wI(b, (int) (f->s - L->sectors));
        wI(b, f->type); wI(b, f->min); wI(b, f->max); wI(b, f->count); wI(b, f->dark); wI(b, f->bright); wI(b, f->dir);
    }
    wI(b, sp->switchBack.n);
    for (int i = 0; i < sp->switchBack.n; i++) {
        wI(b, (int) (sp->switchBack.d[i].line - L->lines));
        wI(b, sp->switchBack.d[i].tics);
    }

    /* things: live list plus anything still queued */
    int total = g->mobjs.n + g->spawnQueue.n;
    Mobj **list = (Mobj **) malloc((size_t) (total ? total : 1) * sizeof(Mobj *));
    int n = 0;
    for (int i = 0; i < g->mobjs.n; i++) list[n++] = g->mobjs.d[i];
    for (int i = 0; i < g->spawnQueue.n; i++) {
        bool dup = false;
        for (int k = 0; k < g->mobjs.n; k++) if (g->mobjs.d[k] == g->spawnQueue.d[i]) { dup = true; break; }
        if (!dup) list[n++] = g->spawnQueue.d[i];
    }
    int pi = 0;
    for (int i = 0; i < n; i++) if (list[i] == g->pl) pi = i;
    wI(b, pi);
    wI(b, n);
    for (int i = 0; i < n; i++) {
        Mobj *m = list[i];
        wI(b, m->kind); wI(b, m->type);
        wF(b, m->x); wF(b, m->y); wF(b, m->z); wF(b, m->mx); wF(b, m->my); wF(b, m->mz);
        wF(b, m->ox); wF(b, m->oy); wF(b, m->oz); wF(b, m->angle); wF(b, m->radius); wF(b, m->height);
        wF(b, m->floorz); wF(b, m->ceilz);
        wI(b, m->frame);
        wB(b, m->fullbright); wB(b, m->fuzz); wB(b, m->solid); wB(b, m->shootable); wB(b, m->noGravity); wB(b, m->corpse);
        wB(b, m->removed); wB(b, m->countKill); wB(b, m->countItem); wB(b, m->floatMonster); wB(b, m->ambush); wB(b, m->onFloor);
        wI(b, m->health); wI(b, m->tics); wI(b, m->seq); wI(b, m->step);
        wI(b, m->reaction); wI(b, m->moveCount); wI(b, m->moveDir); wI(b, m->threshold); wI(b, m->painTics); wI(b, m->attackTics);
        wB(b, m->seesTarget); wI(b, m->pickupKind); wI(b, m->damage);
        wS(b, m->missileName);
        wB(b, m->charging); wB(b, m->justAttacked); wB(b, m->justHit); wB(b, m->exploding);
        wI(b, m->chargeTics); wI(b, m->animTics); wI(b, m->life);
        wI(b, m->sector ? (int) (m->sector - L->sectors) : -1);
        const char *sn = m->spr ? spriteName(g->gfx, m->spr) : NULL;
        wB(b, sn != NULL); if (sn) wS(b, sn);
        wI(b, m->def ? m->def->type : -1);
        wI(b, m->spec ? m->spec->ed : -1);
        wB(b, m->miss != NULL); if (m->miss) wS(b, m->miss->name);
        int ti = -1, oi = -1;
        for (int k = 0; k < n; k++) { if (list[k] == m->target) ti = k; if (list[k] == m->owner) oi = k; }
        if (!m->target) ti = -1;
        if (!m->owner) oi = -1;
        wI(b, ti); wI(b, oi);
    }
    free(list);

    wI(b, g->tic); wI(b, g->state); wF(b, g->angle);
    wI(b, g->health); wI(b, g->armor); wI(b, g->armorType);
    for (int i = 0; i < 4; i++) wI(b, g->ammo[i]);
    for (int i = 0; i < 4; i++) wI(b, g->maxAmmo[i]);
    for (int i = 0; i < 9; i++) wB(b, g->owned[i]);
    for (int i = 0; i < 3; i++) wB(b, g->keys[i]);
    wI(b, g->weapon); wI(b, g->pending);
    wI(b, g->invuln); wI(b, g->invis); wI(b, g->suit); wI(b, g->light);
    wB(b, g->berserk); wB(b, g->hasMap); wB(b, g->backpack);
    wI(b, g->kills); wI(b, g->items); wI(b, g->secrets); wI(b, g->totalKills); wI(b, g->totalItems); wI(b, g->totalSecrets); wI(b, g->levelTics);
    wI(b, g->damageFlash); wI(b, g->bonusFlash);
    wI(b, g->hurtTic); wI(b, g->hurtAmount); wI(b, g->grinTic); wB(b, g->hurtFrom); wF(b, g->hurtAngle);
    wS(b, g->message); wI(b, g->msgTics); wS(b, g->nextMap); wB(b, g->secretExit);
    wI(b, g->wState); wI(b, g->wStep); wI(b, g->wTics); wI(b, g->refire);
    wF(b, g->psy); wF(b, g->psx); wI(b, g->wFrameOverride); wF(b, g->bob); wF(b, g->eyeZ); wF(b, g->eyeZPrev);
    wI(b, g->viewHeightDead); wF(b, g->lastPx); wF(b, g->lastPy);
}

bool save_save(int slot) {
    char path[512], tmp[540];
    savePath(slot, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    Buf b = {NULL, 0, 0};
    writeState(&b);
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(b.d); return false; }
    bool ok = fwrite(b.d, 1, (size_t) b.n, f) == (size_t) b.n;
    ok = (fclose(f) == 0) && ok;
    free(b.d);
    if (!ok) { remove(tmp); return false; }
    remove(path);
    if (rename(tmp, path) != 0) { remove(tmp); return false; }
    return true;
}

/* ------------------------------------------------------------------ load */
static void readState(Rd *r) {
    Game *g = GM;
    Level *L = g->L;
    Gfx *gfx = g->gfx;
    char nm[64];

    int n = rI(r);
    for (int i = 0; i < n && !r->err; i++) {
        Sector tmp;
        memset(&tmp, 0, sizeof tmp);
        tmp.floor = rF(r); tmp.ceil = rF(r);
        rS(r, nm, sizeof nm); snprintf(tmp.floorName, 9, "%s", nm);
        rS(r, nm, sizeof nm); snprintf(tmp.ceilName, 9, "%s", nm);
        tmp.skyCeil = rB(r); tmp.light = rI(r); tmp.special = rI(r); tmp.tag = rI(r); tmp.secretFound = rB(r);
        if (i < L->nsectors) {
            Sector *s = &L->sectors[i];
            s->floor = tmp.floor; s->ceil = tmp.ceil;
            memcpy(s->floorName, tmp.floorName, 9); memcpy(s->ceilName, tmp.ceilName, 9);
            s->skyCeil = tmp.skyCeil; s->light = tmp.light; s->special = tmp.special; s->tag = tmp.tag; s->secretFound = tmp.secretFound;
            s->floorPic = gfx_flat(gfx, s->floorName);
            s->ceilPic = gfx_flat(gfx, s->ceilName);
            s->mover = NULL;
        }
    }
    n = rI(r);
    for (int i = 0; i < n && !r->err; i++) {
        float xo = rF(r), yo = rF(r);
        char u[9], lo[9], mi[9];
        rS(r, nm, sizeof nm); snprintf(u, 9, "%s", nm);
        rS(r, nm, sizeof nm); snprintf(lo, 9, "%s", nm);
        rS(r, nm, sizeof nm); snprintf(mi, 9, "%s", nm);
        if (i < L->nsides) {
            Side *s = &L->sides[i];
            s->xoff = xo; s->yoff = yo;
            memcpy(s->upperName, u, 9); memcpy(s->lowerName, lo, 9); memcpy(s->midName, mi, 9);
            s->upper = gfx_texture(gfx, u); s->lower = gfx_texture(gfx, lo); s->mid = gfx_texture(gfx, mi);
        }
    }
    n = rI(r);
    for (int i = 0; i < n && !r->err; i++) {
        int fl = rI(r), spc = rI(r), tag = rI(r);
        bool used = rB(r), mapped = rB(r);
        if (i < L->nlines) { Line *l = &L->lines[i]; l->flags = fl; l->special = spc; l->tag = tag; l->used = used; l->mapped = mapped; }
    }

    /* rebuild animation lists / scrollers from the restored names, then put the live effects back */
    Specials *sp = &g->sp;
    specials_init();
    n = rI(r);
    for (int i = 0; i < n && !r->err; i++) {
        Mover *m = (Mover *) calloc(1, sizeof(Mover));
        m->kind = rI(r); m->target = rF(r); m->speed = rF(r); m->state = rI(r); m->wait = rI(r); m->waitMax = rI(r); m->home = rF(r);
        m->useCeil = rB(r); m->once = rB(r); m->reopen = rB(r); m->reopenWait = rB(r);
        int si = rI(r), li = rI(r);
        if (si < 0 || si >= L->nsectors) { free(m); continue; }
        m->sector = &L->sectors[si];
        m->triggerLine = li >= 0 && li < L->nlines ? &L->lines[li] : NULL;
        m->sector->mover = m;
        vec_push(sp->movers, m);
    }
    vec_clear(sp->lights);
    n = rI(r);
    for (int i = 0; i < n && !r->err; i++) {
        LightFx f;
        memset(&f, 0, sizeof f);
        int si = rI(r);
        f.type = rI(r); f.min = rI(r); f.max = rI(r); f.count = rI(r); f.dark = rI(r); f.bright = rI(r); f.dir = rI(r);
        if (si >= 0 && si < L->nsectors) { f.s = &L->sectors[si]; vec_push(sp->lights, f); }
    }
    vec_clear(sp->switchBack);
    n = rI(r);
    for (int i = 0; i < n && !r->err; i++) {
        int li = rI(r), tics = rI(r);
        if (li >= 0 && li < L->nlines) { SwitchBack sb = {&L->lines[li], tics}; vec_push(sp->switchBack, sb); }
    }

    /* things */
    int plIndex = rI(r);
    n = rI(r);
    if (r->err || n < 0 || n > 200000) { r->err = true; return; }
    Mobj **arr = (Mobj **) calloc((size_t) (n ? n : 1), sizeof(Mobj *));
    int *tgt = (int *) calloc((size_t) (n ? n : 1), sizeof(int)), *own = (int *) calloc((size_t) (n ? n : 1), sizeof(int));
    for (int i = 0; i < n; i++) {
        Mobj *m = mobj_new(0, 0, 0);
        m->kind = rI(r); m->type = rI(r);
        m->x = rF(r); m->y = rF(r); m->z = rF(r); m->mx = rF(r); m->my = rF(r); m->mz = rF(r);
        m->ox = rF(r); m->oy = rF(r); m->oz = rF(r); m->angle = rF(r); m->radius = rF(r); m->height = rF(r);
        m->floorz = rF(r); m->ceilz = rF(r);
        m->frame = rI(r);
        m->fullbright = rB(r); m->fuzz = rB(r); m->solid = rB(r); m->shootable = rB(r); m->noGravity = rB(r); m->corpse = rB(r);
        m->removed = rB(r); m->countKill = rB(r); m->countItem = rB(r); m->floatMonster = rB(r); m->ambush = rB(r); m->onFloor = rB(r);
        m->health = rI(r); m->tics = rI(r); m->seq = rI(r); m->step = rI(r);
        m->reaction = rI(r); m->moveCount = rI(r); m->moveDir = rI(r); m->threshold = rI(r); m->painTics = rI(r); m->attackTics = rI(r);
        m->seesTarget = rB(r); m->pickupKind = rI(r); m->damage = rI(r);
        m->missileName = rS(r, nm, sizeof nm) ? intern(nm) : NULL;
        m->charging = rB(r); m->justAttacked = rB(r); m->justHit = rB(r); m->exploding = rB(r);
        m->chargeTics = rI(r); m->animTics = rI(r); m->life = rI(r);
        int si = rI(r);
        m->sector = si >= 0 && si < L->nsectors ? &L->sectors[si] : level_sectorAt(L, m->x, m->y);
        if (rB(r)) { rS(r, nm, sizeof nm); m->spr = gfx_sprite(gfx, nm); }
        int dt = rI(r);
        m->def = dt >= 0 ? monsters_def(dt) : NULL;
        int sk = rI(r);
        m->spec = sk >= 0 ? things_spec(sk) : NULL;
        if (rB(r)) { rS(r, nm, sizeof nm); m->miss = monsters_missile(nm); }
        tgt[i] = rI(r); own[i] = rI(r);
        arr[i] = m;
    }
    for (int i = 0; i < g->mobjs.n; i++) free(g->mobjs.d[i]);
    for (int i = 0; i < g->spawnQueue.n; i++) free(g->spawnQueue.d[i]);
    vec_clear(g->mobjs); vec_clear(g->spawnQueue);
    for (int i = 0; i < n; i++) {
        arr[i]->target = tgt[i] >= 0 && tgt[i] < n ? arr[tgt[i]] : NULL;
        arr[i]->owner = own[i] >= 0 && own[i] < n ? arr[own[i]] : NULL;
        vec_push(g->mobjs, arr[i]);
    }
    g->pl = plIndex >= 0 && plIndex < n ? arr[plIndex] : (n > 0 ? arr[0] : NULL);
    free(arr); free(tgt); free(own);
    if (!g->pl) { r->err = true; return; }

    g->tic = rI(r); g->state = rI(r); g->angle = rF(r);
    g->health = rI(r); g->armor = rI(r); g->armorType = rI(r);
    for (int i = 0; i < 4; i++) g->ammo[i] = rI(r);
    for (int i = 0; i < 4; i++) g->maxAmmo[i] = rI(r);
    for (int i = 0; i < 9; i++) g->owned[i] = rB(r);
    for (int i = 0; i < 3; i++) g->keys[i] = rB(r);
    g->weapon = rI(r); g->pending = rI(r);
    g->invuln = rI(r); g->invis = rI(r); g->suit = rI(r); g->light = rI(r);
    g->berserk = rB(r); g->hasMap = rB(r); g->backpack = rB(r);
    g->kills = rI(r); g->items = rI(r); g->secrets = rI(r); g->totalKills = rI(r); g->totalItems = rI(r); g->totalSecrets = rI(r); g->levelTics = rI(r);
    g->damageFlash = rI(r); g->bonusFlash = rI(r);
    g->hurtTic = rI(r); g->hurtAmount = rI(r); g->grinTic = rI(r); g->hurtFrom = rB(r); g->hurtAngle = rF(r);
    rS(r, g->message, sizeof g->message); g->msgTics = rI(r); rS(r, g->nextMap, sizeof g->nextMap); g->secretExit = rB(r);
    g->wState = rI(r); g->wStep = rI(r); g->wTics = rI(r); g->refire = rI(r);
    g->psy = rF(r); g->psx = rF(r); g->wFrameOverride = rI(r); g->bob = rF(r); g->eyeZ = rF(r); g->eyeZPrev = rF(r);
    g->viewHeightDead = rI(r); g->lastPx = rF(r); g->lastPy = rF(r);
    g->weapon = clampi(g->weapon, 0, 8);
    snd_listener(g->pl->x, g->pl->y, g->angle);
}

bool save_load(int slot) {
    char path[512];
    savePath(slot, path, sizeof path);
    int len;
    u8 *d = readFile(path, &len);
    if (!d) return false;
    Rd r = {d, len, 0, false};
    bool ok = false;
    if ((u32) rI(&r) == MAGIC && rI(&r) == VERSION) {
        char desc[64], map[16];
        rS(&r, desc, sizeof desc);
        rS(&r, map, sizeof map);
        int skill = rI(&r);
        if (!r.err) {
            GV.titleMode = false;
            GM->skill = skill;
            if (game_startMap(map, false, NULL, 0)) {
                readState(&r);
                ok = !r.err;
            }
            GV.musKey[0] = 0;
        }
    }
    free(d);
    return ok;
}
