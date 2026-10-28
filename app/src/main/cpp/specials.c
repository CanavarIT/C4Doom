/* Line specials (doors, lifts, floors, ceilings, switches, teleports, exits), sector light effects and texture animation. */
#include "doom.h"

/* {trigger, repeat, action, param, key(0 none,1 blue,2 yellow,3 red), fast} */
static int TABLE[256][6];
static bool TABLE_SET[256];

static void sp(int n, int trig, int rep, int act, int param, int key, int fast) {
    TABLE[n][0] = trig; TABLE[n][1] = rep; TABLE[n][2] = act; TABLE[n][3] = param; TABLE[n][4] = key; TABLE[n][5] = fast;
    TABLE_SET[n] = true;
}

static bool table_ready;
static void table_init(void) {
    if (table_ready) return;
    table_ready = true;
    /* manual doors */
    sp(1, TR_DOOR, 1, A_DOOR_OWC, 0, 0, 0); sp(26, TR_DOOR, 1, A_DOOR_OWC, 0, 1, 0);
    sp(27, TR_DOOR, 1, A_DOOR_OWC, 0, 2, 0); sp(28, TR_DOOR, 1, A_DOOR_OWC, 0, 3, 0);
    sp(31, TR_DOOR, 0, A_DOOR_STAY, 0, 0, 0); sp(32, TR_DOOR, 0, A_DOOR_STAY, 0, 1, 0);
    sp(33, TR_DOOR, 0, A_DOOR_STAY, 0, 3, 0); sp(34, TR_DOOR, 0, A_DOOR_STAY, 0, 2, 0);
    sp(117, TR_DOOR, 1, A_DOOR_OWC, 0, 0, 1); sp(118, TR_DOOR, 0, A_DOOR_STAY, 0, 0, 1);
    /* tagged doors */
    sp(2, TR_WALK, 0, A_DOOR_STAY, 0, 0, 0); sp(3, TR_WALK, 0, A_DOOR_CLOSE, 0, 0, 0); sp(4, TR_WALK, 0, A_DOOR_OWC, 0, 0, 0);
    sp(16, TR_WALK, 0, A_DOOR_CLOSE30, 0, 0, 0); sp(108, TR_WALK, 0, A_DOOR_OWC, 0, 0, 1); sp(109, TR_WALK, 0, A_DOOR_STAY, 0, 0, 1);
    sp(110, TR_WALK, 0, A_DOOR_CLOSE, 0, 0, 1);
    sp(75, TR_WALK, 1, A_DOOR_CLOSE, 0, 0, 0); sp(76, TR_WALK, 1, A_DOOR_CLOSE30, 0, 0, 0); sp(86, TR_WALK, 1, A_DOOR_STAY, 0, 0, 0);
    sp(90, TR_WALK, 1, A_DOOR_OWC, 0, 0, 0); sp(105, TR_WALK, 1, A_DOOR_OWC, 0, 0, 1); sp(106, TR_WALK, 1, A_DOOR_STAY, 0, 0, 1);
    sp(107, TR_WALK, 1, A_DOOR_CLOSE, 0, 0, 1);
    sp(29, TR_SWITCH, 0, A_DOOR_OWC, 0, 0, 0); sp(50, TR_SWITCH, 0, A_DOOR_CLOSE, 0, 0, 0); sp(103, TR_SWITCH, 0, A_DOOR_STAY, 0, 0, 0);
    sp(111, TR_SWITCH, 0, A_DOOR_OWC, 0, 0, 1); sp(112, TR_SWITCH, 0, A_DOOR_STAY, 0, 0, 1); sp(113, TR_SWITCH, 0, A_DOOR_CLOSE, 0, 0, 1);
    sp(133, TR_SWITCH, 0, A_DOOR_STAY, 0, 1, 1); sp(135, TR_SWITCH, 0, A_DOOR_STAY, 0, 3, 1); sp(137, TR_SWITCH, 0, A_DOOR_STAY, 0, 2, 1);
    sp(42, TR_SWITCH, 1, A_DOOR_CLOSE, 0, 0, 0); sp(61, TR_SWITCH, 1, A_DOOR_STAY, 0, 0, 0); sp(63, TR_SWITCH, 1, A_DOOR_OWC, 0, 0, 0);
    sp(114, TR_SWITCH, 1, A_DOOR_OWC, 0, 0, 1); sp(115, TR_SWITCH, 1, A_DOOR_STAY, 0, 0, 1); sp(116, TR_SWITCH, 1, A_DOOR_CLOSE, 0, 0, 1);
    sp(99, TR_SWITCH, 1, A_DOOR_STAY, 0, 1, 1); sp(134, TR_SWITCH, 1, A_DOOR_STAY, 0, 3, 1); sp(136, TR_SWITCH, 1, A_DOOR_STAY, 0, 2, 1);
    sp(46, TR_GUN, 1, A_DOOR_STAY, 0, 0, 0);
    /* lifts */
    sp(10, TR_WALK, 0, A_LIFT, 0, 0, 0); sp(121, TR_WALK, 0, A_LIFT, 0, 0, 1); sp(88, TR_WALK, 1, A_LIFT, 0, 0, 0);
    sp(120, TR_WALK, 1, A_LIFT, 0, 0, 1); sp(21, TR_SWITCH, 0, A_LIFT, 0, 0, 0); sp(122, TR_SWITCH, 0, A_LIFT, 0, 0, 1);
    sp(62, TR_SWITCH, 1, A_LIFT, 0, 0, 0); sp(123, TR_SWITCH, 1, A_LIFT, 0, 0, 1);
    /* floors */
    sp(5, TR_WALK, 0, A_FLOOR, F_RAISE_CEIL, 0, 0); sp(19, TR_WALK, 0, A_FLOOR, F_LOWER_HIGHEST, 0, 0);
    sp(30, TR_WALK, 0, A_FLOOR, F_RAISE_TEX, 0, 0); sp(36, TR_WALK, 0, A_FLOOR, F_LOWER_8, 0, 1);
    sp(37, TR_WALK, 0, A_FLOOR, F_LOWER_LOWEST, 0, 0); sp(38, TR_WALK, 0, A_FLOOR, F_LOWER_LOWEST, 0, 0);
    sp(56, TR_WALK, 0, A_FLOOR, F_RAISE_CEIL, 0, 0); sp(58, TR_WALK, 0, A_FLOOR, F_RAISE_24, 0, 0);
    sp(59, TR_WALK, 0, A_FLOOR, F_RAISE_24, 0, 0); sp(119, TR_WALK, 0, A_FLOOR, F_RAISE_NEXT, 0, 0);
    sp(22, TR_WALK, 0, A_FLOOR, F_RAISE_NEXT, 0, 0); sp(47, TR_GUN, 0, A_FLOOR, F_RAISE_NEXT, 0, 0);
    sp(18, TR_SWITCH, 0, A_FLOOR, F_RAISE_NEXT, 0, 0); sp(20, TR_SWITCH, 0, A_FLOOR, F_RAISE_NEXT, 0, 0);
    sp(23, TR_SWITCH, 0, A_FLOOR, F_LOWER_LOWEST, 0, 0); sp(55, TR_SWITCH, 0, A_FLOOR, F_RAISE_CEIL, 0, 0);
    sp(71, TR_SWITCH, 0, A_FLOOR, F_LOWER_8, 0, 1); sp(101, TR_SWITCH, 0, A_FLOOR, F_RAISE_CEIL, 0, 0);
    sp(102, TR_SWITCH, 0, A_FLOOR, F_LOWER_HIGHEST, 0, 0); sp(14, TR_SWITCH, 0, A_FLOOR, F_RAISE_32, 0, 0);
    sp(15, TR_SWITCH, 0, A_FLOOR, F_RAISE_24, 0, 0);
    sp(64, TR_SWITCH, 1, A_FLOOR, F_RAISE_CEIL, 0, 0); sp(65, TR_SWITCH, 1, A_FLOOR, F_RAISE_CEIL, 0, 0);
    sp(66, TR_SWITCH, 1, A_FLOOR, F_RAISE_24, 0, 0); sp(67, TR_SWITCH, 1, A_FLOOR, F_RAISE_32, 0, 0);
    sp(68, TR_SWITCH, 1, A_FLOOR, F_RAISE_NEXT, 0, 0); sp(69, TR_SWITCH, 1, A_FLOOR, F_RAISE_NEXT, 0, 0);
    sp(70, TR_SWITCH, 1, A_FLOOR, F_LOWER_8, 0, 1); sp(45, TR_SWITCH, 1, A_FLOOR, F_LOWER_HIGHEST, 0, 0);
    sp(60, TR_SWITCH, 1, A_FLOOR, F_LOWER_LOWEST, 0, 0);
    sp(83, TR_WALK, 1, A_FLOOR, F_LOWER_HIGHEST, 0, 0); sp(84, TR_WALK, 1, A_FLOOR, F_LOWER_LOWEST, 0, 0);
    sp(82, TR_WALK, 1, A_FLOOR, F_LOWER_LOWEST, 0, 0); sp(91, TR_WALK, 1, A_FLOOR, F_RAISE_CEIL, 0, 0);
    sp(92, TR_WALK, 1, A_FLOOR, F_RAISE_24, 0, 0); sp(93, TR_WALK, 1, A_FLOOR, F_RAISE_24, 0, 0);
    sp(94, TR_WALK, 1, A_FLOOR, F_RAISE_CEIL, 0, 0); sp(95, TR_WALK, 1, A_FLOOR, F_RAISE_NEXT, 0, 0);
    sp(96, TR_WALK, 1, A_FLOOR, F_RAISE_TEX, 0, 0); sp(98, TR_WALK, 1, A_FLOOR, F_LOWER_8, 0, 1);
    sp(128, TR_WALK, 1, A_FLOOR, F_RAISE_NEXT, 0, 0); sp(129, TR_WALK, 1, A_FLOOR, F_RAISE_NEXT, 0, 1);
    sp(24, TR_GUN, 0, A_FLOOR, F_RAISE_CEIL, 0, 0);
    /* ceilings */
    sp(40, TR_WALK, 0, A_CEIL, C_RAISE_HIGH, 0, 0); sp(41, TR_SWITCH, 0, A_CEIL, C_LOWER_FLOOR, 0, 0);
    sp(43, TR_SWITCH, 1, A_CEIL, C_LOWER_FLOOR, 0, 0); sp(44, TR_WALK, 0, A_CEIL, C_LOWER_8, 0, 0);
    sp(72, TR_WALK, 1, A_CEIL, C_LOWER_8, 0, 0);
    /* exits / teleports / stairs / lights / scroll */
    sp(11, TR_SWITCH, 0, A_EXIT, 0, 0, 0); sp(52, TR_WALK, 0, A_EXIT, 0, 0, 0);
    sp(51, TR_SWITCH, 0, A_SECRET, 0, 0, 0); sp(124, TR_WALK, 0, A_SECRET, 0, 0, 0);
    sp(39, TR_WALK, 0, A_TELE, 0, 0, 0); sp(97, TR_WALK, 1, A_TELE, 0, 0, 0);
    sp(7, TR_SWITCH, 0, A_STAIRS, 0, 0, 0); sp(8, TR_WALK, 0, A_STAIRS, 0, 0, 0);
    sp(12, TR_WALK, 0, A_LIGHT, 0, 0, 0); sp(13, TR_WALK, 0, A_LIGHT, 1, 0, 0); sp(35, TR_WALK, 0, A_LIGHT, 2, 0, 0);
    sp(79, TR_WALK, 1, A_LIGHT, 2, 0, 0); sp(80, TR_WALK, 1, A_LIGHT, 0, 0, 0); sp(81, TR_WALK, 1, A_LIGHT, 1, 0, 0);
    sp(138, TR_SWITCH, 1, A_LIGHT, 1, 0, 0); sp(139, TR_SWITCH, 1, A_LIGHT, 2, 0, 0);
    sp(48, TR_WALK, 1, A_SCROLL, 0, 0, 0);
}

const int *specials_lookup(int special) {
    table_init();
    if (special < 0 || special >= 256 || !TABLE_SET[special]) return NULL;
    return TABLE[special];
}

/* ------------------------------------------------------------------ animation tables */
typedef struct { const char *const *names; int n; } AnimGroup;
#define GRP(...) { (const char *const[]){__VA_ARGS__}, (int) (sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *)) }
static const AnimGroup FLAT_ANIMS[] = {
    GRP("NUKAGE1", "NUKAGE2", "NUKAGE3"), GRP("FWATER1", "FWATER2", "FWATER3", "FWATER4"), GRP("LAVA1", "LAVA2", "LAVA3", "LAVA4"),
    GRP("BLOOD1", "BLOOD2", "BLOOD3"), GRP("RROCK05", "RROCK06", "RROCK07", "RROCK08"), GRP("SLIME01", "SLIME02", "SLIME03", "SLIME04"),
    GRP("SLIME05", "SLIME06", "SLIME07", "SLIME08"), GRP("SLIME09", "SLIME10", "SLIME11", "SLIME12")};
static const AnimGroup TEX_ANIMS[] = {
    GRP("BLODGR1", "BLODGR2", "BLODGR3", "BLODGR4"), GRP("BLODRIP1", "BLODRIP2", "BLODRIP3", "BLODRIP4"), GRP("FIREBLU1", "FIREBLU2"),
    GRP("FIRELAV3", "FIRELAVA"), GRP("FIREMAG1", "FIREMAG2", "FIREMAG3"), GRP("FIREWALA", "FIREWALB", "FIREWALL"),
    GRP("GSTFONT1", "GSTFONT2", "GSTFONT3"), GRP("ROCKRED1", "ROCKRED2", "ROCKRED3"), GRP("SLADRIP1", "SLADRIP2", "SLADRIP3"),
    GRP("BFALL1", "BFALL2", "BFALL3", "BFALL4"), GRP("SFALL1", "SFALL2", "SFALL3", "SFALL4"), GRP("WFALL1", "WFALL2", "WFALL3", "WFALL4"),
    GRP("DBRAIN1", "DBRAIN2", "DBRAIN3", "DBRAIN4")};
#define NFLAT ((int) (sizeof(FLAT_ANIMS) / sizeof(FLAT_ANIMS[0])))
#define NTEX ((int) (sizeof(TEX_ANIMS) / sizeof(TEX_ANIMS[0])))

#define S (&GM->sp)

void specials_free(void) {
    for (int i = 0; i < S->movers.n; i++) free(S->movers.d[i]);
    vec_free(S->movers); vec_free(S->scrollers); vec_free(S->lights); vec_free(S->switchBack);
    vec_free(S->flatUsers); vec_free(S->texUsers);
}

/* ================================================================== init */
void specials_init(void) {
    table_init();
    for (int i = 0; i < S->movers.n; i++) free(S->movers.d[i]);
    vec_clear(S->movers); vec_clear(S->scrollers); vec_clear(S->lights); vec_clear(S->switchBack);
    vec_clear(S->flatUsers); vec_clear(S->texUsers);
    Level *L = GM->L;
    for (int si = 0; si < L->nsectors; si++) {
        Sector *s = &L->sectors[si];
        if (s->special == 9) GM->totalSecrets++;
        int min = s->light;
        for (int k = 0; k < s->nlines; k++) {
            Line *l = s->lines[k];
            Sector *o = l->fs == s ? l->bs : l->fs;
            if (o && o->light < min) min = o->light;
        }
        bool has = false;
        LightFx f;
        memset(&f, 0, sizeof(f));
        switch (s->special) {
            case 1: case 2: case 3: case 4: case 8: case 12: case 13: case 17:
                has = true;
                f.s = s; f.type = s->special; f.max = s->light; f.min = min;
                if (s->special == 17) f.min = mini(s->light, min + 16);
                f.count = 1 + game_rnd() % 8; f.dark = 1; f.bright = 1; f.dir = -1;
                break;
            default: break;
        }
        if (has) vec_push(S->lights, f);
        for (int g = 0; g < NFLAT; g++)
            for (int i = 0; i < FLAT_ANIMS[g].n; i++) {
                if (strcmp(s->floorName, FLAT_ANIMS[g].names[i]) == 0) { FlatUser u = {s, true, g, i}; vec_push(S->flatUsers, u); }
                if (strcmp(s->ceilName, FLAT_ANIMS[g].names[i]) == 0) { FlatUser u = {s, false, g, i}; vec_push(S->flatUsers, u); }
            }
    }
    for (int si = 0; si < L->nsides; si++) {
        Side *sd = &L->sides[si];
        for (int g = 0; g < NTEX; g++)
            for (int i = 0; i < TEX_ANIMS[g].n; i++) {
                if (strcmp(sd->upperName, TEX_ANIMS[g].names[i]) == 0) { TexUser u = {sd, 0, g, i}; vec_push(S->texUsers, u); }
                if (strcmp(sd->lowerName, TEX_ANIMS[g].names[i]) == 0) { TexUser u = {sd, 1, g, i}; vec_push(S->texUsers, u); }
                if (strcmp(sd->midName, TEX_ANIMS[g].names[i]) == 0) { TexUser u = {sd, 2, g, i}; vec_push(S->texUsers, u); }
            }
    }
    for (int li = 0; li < L->nlines; li++) {
        Line *l = &L->lines[li];
        if (l->special == 48 && l->front) vec_push(S->scrollers, l);
    }
}

/* ================================================================== movers */
static float lowestNeighborCeil(Sector *s);
static void toggleSwitch(Line *l);
static bool moverTick(Mover *m);
static void animate(int phase);
static void lightTick(LightFx *f);

/* True when the mobj's body touches the sector (centre inside, or closer to one of its lines than its radius). */
static bool overlaps(Mobj *m, Sector *s) {
    if (m->sector == s) return true;
    float r = m->radius;
    for (int i = 0; i < s->nlines; i++) {
        Line *l = s->lines[i];
        if (m->x < l->minx - r || m->x > l->maxx + r || m->y < l->miny - r || m->y > l->maxy + r) continue;
        float ex = l->x2 - l->x1, ey = l->y2 - l->y1;
        float len2 = ex * ex + ey * ey;
        float t = len2 <= 0 ? 0 : ((m->x - l->x1) * ex + (m->y - l->y1) * ey) / len2;
        if (t < 0) t = 0; else if (t > 1) t = 1;
        float px = l->x1 + ex * t - m->x, py = l->y1 + ey * t - m->y;
        if (px * px + py * py < r * r) return true;
    }
    return false;
}

static bool blockedBy(Sector *s) {
    float gap = s->ceil - s->floor;
    for (int i = 0, n = GM->mobjs.n; i < n; i++) {
        Mobj *m = GM->mobjs.d[i];
        if (m->removed || !m->solid || !m->shootable) continue;
        if (m->height <= gap) continue;
        if (overlaps(m, s)) return true;
    }
    return false;
}

/* Things standing on a sector's floor follow it up and down (P_ThingHeightClip). */
static void settleThings(Sector *s, float oldFloor) {
    float nf = s->floor;
    if (nf == oldFloor) return;
    for (int i = 0, n = GM->mobjs.n; i < n; i++) {
        Mobj *m = GM->mobjs.d[i];
        if (m->removed || m->noGravity || m == GM->pl) continue;
        if (m->kind == MK_MISSILE || m->kind == MK_EFFECT || m->kind == MK_PLAYER) continue;
        bool onIt = absf(m->z - oldFloor) < 0.5f || m->z < nf;
        if (!onIt) continue;
        Sector *ms = m->sector ? m->sector : level_sectorAt(GM->L, m->x, m->y);
        if (ms != s && level_sectorAt(GM->L, m->x, m->y) != s) continue;
        m->z = nf; m->oz = nf; m->floorz = nf;
    }
}

/* Returns true when mover finished. */
static bool moverTick(Mover *m) {
    Sector *s = m->sector;
    switch (m->kind) {
        case 0: { /* door */
            if (m->state == 0) { /* opening */
                s->ceil += m->speed;
                if (s->ceil >= m->target) { s->ceil = m->target; if (m->once) return true; m->state = 1; m->wait = m->waitMax; }
            } else if (m->state == 1) {
                if (--m->wait <= 0) {
                    if (m->reopenWait) {           /* "close for 30 seconds, then open" finished waiting */
                        m->reopenWait = false; m->state = 0; m->once = true;
                        m->target = lowestNeighborCeil(s) - 4;
                        game_sndAt(m->speed > 4 ? "DSBDOPN" : "DSDOROPN", s->cx, s->cy);
                    } else { m->state = 2; game_sndAt(m->speed > 4 ? "DSBDCLS" : "DSDORCLS", s->cx, s->cy); }
                }
            } else {
                float prev = s->ceil;
                s->ceil -= m->speed;
                if (blockedBy(s)) { s->ceil = prev; m->state = 0; m->reopen = false; game_sndAt(m->speed > 4 ? "DSBDOPN" : "DSDOROPN", s->cx, s->cy); break; }
                if (s->ceil <= m->home) {
                    s->ceil = m->home;
                    if (m->reopen) { m->reopen = false; m->reopenWait = true; m->state = 1; m->wait = 30 * 35; return false; }
                    return true;
                }
            }
            return false;
        }
        case 1: { /* lift */
            if (m->state == 0) {
                float of = s->floor;
                s->floor -= m->speed;
                if (s->floor <= m->target) { s->floor = m->target; m->state = 1; m->wait = m->waitMax; game_sndAt("DSPSTOP", s->cx, s->cy); }
                settleThings(s, of);
            } else if (m->state == 1) {
                if (--m->wait <= 0) { m->state = 2; game_sndAt("DSPSTART", s->cx, s->cy); }
            } else {
                float prev = s->floor;
                s->floor += m->speed;
                if (blockedBy(s)) { s->floor = prev; m->state = 0; break; }
                if (s->floor >= m->home) s->floor = m->home;
                settleThings(s, prev);
                if (s->floor >= m->home) { game_sndAt("DSPSTOP", s->cx, s->cy); return true; }
            }
            return false;
        }
        case 2: { /* floor */
            float prev = s->floor;
            if (m->target > s->floor) { s->floor = minf(m->target, s->floor + m->speed); if (blockedBy(s)) { s->floor = prev; return false; } }
            else s->floor = maxf(m->target, s->floor - m->speed);
            settleThings(s, prev);
            if (s->floor == m->target) { game_sndAt("DSPSTOP", s->cx, s->cy); return true; }
            return false;
        }
        default: { /* ceiling */
            float prev = s->ceil;
            if (m->target > s->ceil) s->ceil = minf(m->target, s->ceil + m->speed);
            else { s->ceil = maxf(m->target, s->ceil - m->speed); if (blockedBy(s)) { s->ceil = prev; return false; } }
            if (s->ceil == m->target) return true;
            return false;
        }
    }
    return false;
}

static void lightTick(LightFx *f) {
    Sector *s = f->s;
    switch (f->type) {
        case 17:
            if (--f->count <= 0) {
                int amt = (game_rnd() & 3) * 16;
                s->light = maxi(f->max - amt, f->min);
                f->count = 4;
            }
            break;
        case 1:
            if (--f->count <= 0) {
                if (s->light == f->max) { s->light = f->min; f->count = (game_rnd() & 7) + 1; }
                else { s->light = f->max; f->count = (game_rnd() & 63) + 1; }
            }
            break;
        case 2: case 4: case 13:
        case 3: case 12: {
            int bright = (f->type == 3 || f->type == 12) ? 5 : 5, dark = (f->type == 3 || f->type == 12) ? 35 : 15;
            if (--f->count <= 0) {
                if (s->light == f->max) { s->light = f->min; f->count = dark; }
                else { s->light = f->max; f->count = bright; }
            }
            break;
        }
        case 8:
            s->light += f->dir * 8;
            if (s->light <= f->min) { s->light = f->min; f->dir = 1; }
            else if (s->light >= f->max) { s->light = f->max; f->dir = -1; }
            break;
        default: break;
    }
}

static void animate(int phase) {
    for (int i = 0; i < S->flatUsers.n; i++) {
        FlatUser *u = &S->flatUsers.d[i];
        const AnimGroup *g = &FLAT_ANIMS[u->grp];
        const u8 *pic = gfx_flat(GM->gfx, g->names[(u->idx + phase) % g->n]);
        if (!pic) continue;
        if (u->floor) u->s->floorPic = pic; else u->s->ceilPic = pic;
    }
    for (int i = 0; i < S->texUsers.n; i++) {
        TexUser *u = &S->texUsers.d[i];
        const AnimGroup *g = &TEX_ANIMS[u->grp];
        Pic *p = gfx_texture(GM->gfx, g->names[(u->idx + phase) % g->n]);
        if (!p) continue;
        switch (u->which) { case 0: u->sd->upper = p; break; case 1: u->sd->lower = p; break; default: u->sd->mid = p; }
    }
}

/* ================================================================== tick */
void specials_tick(void) {
    for (int i = S->movers.n - 1; i >= 0; i--) {
        Mover *m = S->movers.d[i];
        if (moverTick(m)) {
            m->sector->mover = NULL;
            free(m);
            memmove(&S->movers.d[i], &S->movers.d[i + 1], (size_t) (S->movers.n - i - 1) * sizeof(Mover *));
            S->movers.n--;
        }
    }
    for (int i = 0; i < S->lights.n; i++) lightTick(&S->lights.d[i]);
    for (int i = 0; i < S->scrollers.n; i++) S->scrollers.d[i]->front->xoff += 1;
    for (int i = S->switchBack.n - 1; i >= 0; i--) {
        SwitchBack *o = &S->switchBack.d[i];
        int t = o->tics - 1;
        if (t <= 0) {
            toggleSwitch(o->line); game_snd("DSSWTCHN", NULL);
            memmove(&S->switchBack.d[i], &S->switchBack.d[i + 1], (size_t) (S->switchBack.n - i - 1) * sizeof(SwitchBack));
            S->switchBack.n--;
        } else o->tics = t;
    }
    if ((GM->tic & 7) == 0) animate(GM->tic >> 3);
}

/* ------------------------------------------------------------------ neighbours */
static Sector *other(Line *l, Sector *s) { return l->fs == s ? l->bs : l->fs; }

static float lowestNeighborCeil(Sector *s) {
    float v = 1e9f;
    for (int i = 0; i < s->nlines; i++) { Sector *o = other(s->lines[i], s); if (o && o != s && o->ceil < v) v = o->ceil; }
    return v == 1e9f ? s->ceil : v;
}
static float highestNeighborCeil(Sector *s) {
    float v = -1e9f;
    for (int i = 0; i < s->nlines; i++) { Sector *o = other(s->lines[i], s); if (o && o != s && o->ceil > v) v = o->ceil; }
    return v == -1e9f ? s->ceil : v;
}
static float lowestNeighborFloor(Sector *s) {
    float v = s->floor;
    for (int i = 0; i < s->nlines; i++) { Sector *o = other(s->lines[i], s); if (o && o != s && o->floor < v) v = o->floor; }
    return v;
}
static float highestNeighborFloor(Sector *s) {
    float v = -1e9f;
    for (int i = 0; i < s->nlines; i++) { Sector *o = other(s->lines[i], s); if (o && o != s && o->floor > v) v = o->floor; }
    return v == -1e9f ? s->floor : v;
}
static float nextHigherFloor(Sector *s) {
    float v = 1e9f;
    for (int i = 0; i < s->nlines; i++) { Sector *o = other(s->lines[i], s); if (o && o != s && o->floor > s->floor && o->floor < v) v = o->floor; }
    return v == 1e9f ? s->floor : v;
}

static Mover *newMover(Sector *s, int kind) {
    Mover *m = (Mover *) calloc(1, sizeof(Mover));
    m->sector = s; m->kind = kind;
    s->mover = m; vec_push(S->movers, m);
    return m;
}

bool specials_startDoor(Sector *s, int action, bool fast) {
    if (s->mover) return false;
    Mover *m = newMover(s, 0);
    m->speed = fast ? 8 : 2; m->waitMax = 150; m->home = s->floor;
    m->target = lowestNeighborCeil(s) - 4;
    if (action == A_DOOR_CLOSE30) { m->state = 2; m->reopen = true; game_sndAt(fast ? "DSBDCLS" : "DSDORCLS", s->cx, s->cy); }
    else if (action == A_DOOR_CLOSE) { m->state = 2; game_sndAt(fast ? "DSBDCLS" : "DSDORCLS", s->cx, s->cy); }
    else {
        m->state = 0; m->once = action == A_DOOR_STAY;
        game_sndAt(fast ? "DSBDOPN" : "DSDOROPN", s->cx, s->cy);
    }
    return true;
}

static bool startLift(Sector *s, bool fast) {
    if (s->mover) return false;
    Mover *m = newMover(s, 1);
    m->speed = fast ? 8 : 4; m->waitMax = 105; m->home = s->floor; m->state = 0;
    m->target = lowestNeighborFloor(s);
    game_sndAt("DSPSTART", s->cx, s->cy);
    return true;
}

/* Line specials whose floor move also changes the sector's flat / type (damage).
 * 1 = flat only, 2 = flat + type copied, 3 = flat copied, type cleared ("no more damage"). */
static int floorChangeMode(int special) {
    switch (special) {
        case 14: case 15: case 66: case 67: return 1;
        case 59: case 93: return 2;
        case 20: case 22: case 47: case 68: case 95: return 3;
        default: return 0;
    }
}

static void applyFloorChange(Sector *s) {
    if (!S->curLine || !S->curLine->fs) return;
    int mode = floorChangeMode(S->curLine->special);
    if (mode == 0) return;
    Sector *m = S->curLine->fs;
    s->floorPic = m->floorPic; memcpy(s->floorName, m->floorName, 9);
    if (mode == 2) s->special = m->special;
    else if (mode == 3) s->special = 0;
    /* stop the flat animation from restoring the old flat */
    int w = 0;
    for (int i = 0; i < S->flatUsers.n; i++) {
        FlatUser u = S->flatUsers.d[i];
        if (u.s == s && u.floor) continue;
        S->flatUsers.d[w++] = u;
    }
    S->flatUsers.n = w;
}

bool specials_startFloor(Sector *s, int type, bool fast) {
    if (s->mover) return false;
    float t = s->floor;
    switch (type) {
        case F_RAISE_CEIL: t = minf(lowestNeighborCeil(s), s->ceil); break;
        case F_RAISE_NEXT: t = nextHigherFloor(s); break;
        case F_LOWER_LOWEST: t = lowestNeighborFloor(s); break;
        case F_LOWER_HIGHEST: t = highestNeighborFloor(s); break;
        case F_LOWER_8: t = highestNeighborFloor(s) + 8; if (t > s->floor) t = s->floor; break;
        case F_RAISE_24: t = s->floor + 24; break;
        case F_RAISE_32: t = s->floor + 32; break;
        case F_RAISE_TEX: {
            float h = 1e9f;
            for (int i = 0; i < s->nlines; i++) {
                Line *l = s->lines[i];
                Side *sides[2] = {l->front, l->back};
                for (int k = 0; k < 2; k++)
                    if (sides[k] && sides[k]->lower && sides[k]->lower->h < h) h = (float) sides[k]->lower->h;
            }
            t = s->floor + (h == 1e9f ? 64 : h);
            break;
        }
        default: break;
    }
    if (t == s->floor) return false;
    Mover *m = newMover(s, 2);
    m->target = t; m->speed = fast ? 4 : 1;
    applyFloorChange(s);
    game_sndAt("DSSTNMOV", s->cx, s->cy);
    return true;
}

static bool startCeil(Sector *s, int type) {
    if (s->mover) return false;
    float t = s->ceil;
    switch (type) {
        case C_RAISE_HIGH: t = highestNeighborCeil(s); break;
        case C_LOWER_FLOOR: t = s->floor; break;
        case C_LOWER_8: t = s->floor + 8; break;
        default: break;
    }
    if (t == s->ceil) return false;
    Mover *m = newMover(s, 3);
    m->target = t; m->speed = 1;
    game_sndAt("DSSTNMOV", s->cx, s->cy);
    return true;
}

static bool buildStairs(Sector *s) {
    bool any = false;
    float h = s->floor + 8;
    Sector *cur = s;
    for (int guard = 0; guard < 64 && cur; guard++) {
        if (!cur->mover) {
            Mover *m = newMover(cur, 2);
            m->target = h; m->speed = 0.5f; any = true;
        }
        Sector *next = NULL;
        for (int i = 0; i < cur->nlines; i++) {
            Line *l = cur->lines[i];
            if (l->fs != cur || !l->bs || l->bs->mover) continue;
            if (strcmp(l->bs->floorName, cur->floorName) != 0) continue;
            next = l->bs; break;
        }
        cur = next; h += 8;
    }
    return any;
}

static void setLight(Sector *s, int mode) {
    int v;
    if (mode == 1) v = 255;
    else if (mode == 2) v = 35;
    else {
        v = 0;
        for (int i = 0; i < s->nlines; i++) { Sector *o = other(s->lines[i], s); if (o && o->light > v) v = o->light; }
    }
    s->light = v;
    for (int i = 0; i < S->lights.n; i++) if (S->lights.d[i].s == s) S->lights.d[i].max = v;
}

/* ================================================================== triggering */
static bool teleport(Line *l, Mobj *who) {
    if (who != GM->pl) return false;
    for (int i = 0; i < GM->teleDests.n; i++) {
        Thing *t = GM->teleDests.d[i];
        Sector *s = level_sectorAt(GM->L, t->x, t->y);
        if (s->tag != l->tag) continue;
        game_sndAt("DSTELEPT", who->x, who->y);
        who->x = who->ox = t->x; who->y = who->oy = t->y;
        who->z = who->oz = s->floor; who->mx = who->my = 0;
        who->sector = s; who->floorz = s->floor; who->ceilz = s->ceil;
        GM->angle = (float) (t->angle * (DOOM_PI / 180.0)); who->angle = GM->angle;
        GM->eyeZ = GM->eyeZPrev = who->z + 41;
        GM->lastPx = who->x; GM->lastPy = who->y;
        game_sndAt("DSTELEPT", t->x, t->y);
        return true;
    }
    return false;
}

static bool activate(Line *l, const int *spr, Mobj *who) {
    bool ok = false;
    int act = spr[2], prm = spr[3];
    bool fast = spr[5] != 0;
    S->curLine = l;
    switch (act) {
        case A_EXIT: if (who == GM->pl) { game_exitLevel(false); ok = true; } break;
        case A_SECRET: if (who == GM->pl) { game_exitLevel(true); ok = true; } break;
        case A_SCROLL: break;
        case A_TELE: ok = teleport(l, who); break;
        default:
            if (l->tag == 0 && spr[0] != TR_DOOR) return false;
            for (int i = 0; i < GM->L->nsectors; i++) {
                Sector *s = &GM->L->sectors[i];
                if (spr[0] == TR_DOOR) continue;
                if (s->tag != l->tag) continue;
                switch (act) {
                    case A_DOOR_OWC: case A_DOOR_STAY: case A_DOOR_CLOSE: case A_DOOR_CLOSE30: ok |= specials_startDoor(s, act, fast); break;
                    case A_LIFT: ok |= startLift(s, fast); break;
                    case A_FLOOR: ok |= specials_startFloor(s, prm, fast); break;
                    case A_CEIL: ok |= startCeil(s, prm); break;
                    case A_STAIRS: ok |= buildStairs(s); break;
                    case A_LIGHT: setLight(s, prm); ok = true; break;
                    default: break;
                }
            }
    }
    return ok;
}

static bool isNameSwitch(const char *n) { return strncmp(n, "SW1", 3) == 0 || strncmp(n, "SW2", 3) == 0; }

static void flipName(char n[9]) {
    if (strncmp(n, "SW1", 3) == 0) n[2] = '2';
    else if (strncmp(n, "SW2", 3) == 0) n[2] = '1';
}

static void toggleSwitch(Line *l) {
    if (!l->front) return;
    Side *s = l->front;
    flipName(s->upperName); flipName(s->lowerName); flipName(s->midName);
    s->upper = gfx_texture(GM->gfx, s->upperName); s->lower = gfx_texture(GM->gfx, s->lowerName); s->mid = gfx_texture(GM->gfx, s->midName);
}

static bool hasSwitchTex(Line *l) {
    return l->front && (isNameSwitch(l->front->upperName) || isNameSwitch(l->front->lowerName) || isNameSwitch(l->front->midName));
}

static const char *const KEYNAME[3] = {"blue", "yellow", "red"};

/* Player/monster presses a line. Returns true if the line did something (or is a special). */
bool specials_useLine(Line *l, Mobj *who) {
    const int *spr = specials_lookup(l->special);
    if (!spr) return false;
    if (spr[0] != TR_DOOR && spr[0] != TR_SWITCH) return false;
    bool player = who == GM->pl;
    if (!player && !(spr[0] == TR_DOOR && spr[4] == 0 && spr[2] == A_DOOR_OWC)) return false;
    if (player && phys_side(l, who->x, who->y) >= 0) return false;      /* must face the front side */
    if (spr[4] != 0 && player) {
        if (!GM->keys[spr[4] - 1]) {
            char buf[96];
            snprintf(buf, sizeof(buf), spr[0] == TR_DOOR ? "You need a %s card to open this door" : "You need a %s key to activate this object",
                     KEYNAME[spr[4] - 1]);
            game_msg(buf); game_snd("DSOOF", NULL); return true;
        }
    }
    if (spr[0] == TR_DOOR) {
        if (!l->bs) return false;
        Sector *s = l->bs;
        if (s->mover && s->mover->kind == 0) {
            Mover *m = s->mover;
            if (spr[2] == A_DOOR_OWC) {
                if (m->state == 2) { m->state = 0; game_sndAt(m->speed > 4 ? "DSBDOPN" : "DSDOROPN", s->cx, s->cy); }
                else if (player) { m->state = 2; m->wait = 0; game_sndAt(m->speed > 4 ? "DSBDCLS" : "DSDORCLS", s->cx, s->cy); }
            }
            return true;
        }
        if (!specials_startDoor(s, spr[2], spr[5] != 0)) return true;
        if (spr[1] == 0) l->special = 0;
        return true;
    }
    /* switch */
    bool ok = activate(l, spr, who);
    if (ok || spr[2] == A_EXIT || spr[2] == A_SECRET) {
        game_snd("DSSWTCHN", who);
        if (hasSwitchTex(l)) {
            toggleSwitch(l);
            if (spr[1] != 0) { SwitchBack sb = {l, 35}; vec_push(S->switchBack, sb); }
        }
        if (spr[1] == 0) l->special = 0;
    }
    return true;
}

void specials_shootLine(Line *l, Mobj *who) {
    const int *spr = specials_lookup(l->special);
    if (!spr || spr[0] != TR_GUN || who != GM->pl) return;
    if (activate(l, spr, who) && spr[1] == 0) l->special = 0;
}

typedef struct { Mobj *m; float fx, fy, nx, ny; } CrossCtx;

static bool cross_visit(Line *l, void *vc) {
    CrossCtx *c = (CrossCtx *) vc;
    if (l->special == 0) return true;
    const int *spr = specials_lookup(l->special);
    if (!spr || spr[0] != TR_WALK) return true;
    float s0 = phys_side(l, c->fx, c->fy), s1 = phys_side(l, c->nx, c->ny);
    if ((s0 < 0) == (s1 < 0)) return true;
    if (spr[2] == A_TELE && !(s0 < 0 && s1 >= 0)) return true;
    /* intersection within the line extents */
    float dx = c->nx - c->fx, dy = c->ny - c->fy;
    float den = dx * l->dy - dy * l->dx;
    if (absf(den) < 1e-6f) return true;
    float u = ((l->x1 - c->fx) * dy - (l->y1 - c->fy) * dx) / den;
    if (u < 0.0f || u > 1.0f) return true;
    if (activate(l, spr, c->m) && spr[1] == 0) l->special = 0;
    return true;
}

void specials_checkCross(Mobj *m, float ox, float oy) {
    float nx = m->x, ny = m->y;
    if (ox == nx && oy == ny) return;
    CrossCtx c = {m, ox, oy, nx, ny};
    level_forLinesInBox(GM->L, minf(ox, nx) - 1, minf(oy, ny) - 1, maxf(ox, nx) + 1, maxf(oy, ny) + 1, cross_visit, &c);
}

typedef struct { float dx, dy, fx, fy; Line *best; float bt; float wt; } UseCtx;

static bool use_visit(Line *l, void *vc) {
    UseCtx *c = (UseCtx *) vc;
    float den = c->dx * l->dy - c->dy * l->dx;
    if (absf(den) < 1e-6f) return true;
    float t = ((l->x1 - c->fx) * l->dy - (l->y1 - c->fy) * l->dx) / den;
    float u = ((l->x1 - c->fx) * c->dy - (l->y1 - c->fy) * c->dx) / den;
    if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) return true;
    if (l->special != 0 && specials_lookup(l->special)) { if (t < c->bt) { c->bt = t; c->best = l; } }
    else if (!l->back || minf(l->fs->ceil, l->bs->ceil) <= maxf(l->fs->floor, l->bs->floor)) { if (t < c->wt) c->wt = t; }
    return true;
}

/* Use-ray: first special line in front of the player, or NULL if a wall blocks first. */
Line *specials_findUseLine(float x, float y, float c, float s, float range) {
    float ex = x + c * range, ey = y + s * range;
    UseCtx u = {ex - x, ey - y, x, y, NULL, 2.0f, 2.0f};
    level_forLinesInBox(GM->L, minf(x, ex) - 1, minf(y, ey) - 1, maxf(x, ex) + 1, maxf(y, ey) + 1, use_visit, &u);
    if (u.best && u.bt <= u.wt) return u.best;
    return NULL;
}

/* ================================================================== sound */
static void flood(Sector *s, int block, int *best) {
    if (best[s->idx] <= block) return;
    best[s->idx] = block;
    for (int i = 0; i < s->nlines; i++) {
        Line *l = s->lines[i];
        if (!l->back) continue;
        if (minf(l->fs->ceil, l->bs->ceil) <= maxf(l->fs->floor, l->bs->floor)) continue;
        Sector *o = l->fs == s ? l->bs : l->fs;
        if ((l->flags & ML_SOUNDBLOCK) != 0) { if (block == 0) flood(o, 1, best); }
        else flood(o, block, best);
    }
}

void specials_soundAlert(Mobj *who) {
    int n = GM->L->nsectors;
    int *best = (int *) malloc((size_t) (n ? n : 1) * sizeof(int));
    for (int i = 0; i < n; i++) best[i] = 99;
    if (who->sector) flood(who->sector, 0, best);
    for (int i = 0, k = GM->mobjs.n; i < k; i++) {
        Mobj *m = GM->mobjs.d[i];
        if (m->kind != MK_MONSTER || !m->sector || m->removed) continue;
        if (best[m->sector->idx] < 99) ai_alert(m, who);
    }
    free(best);
}
