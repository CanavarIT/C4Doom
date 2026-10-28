/* Collision, line-of-sight and hitscan helpers working on Game state. */
#include "doom.h"

PhysState PH;

/* Overlap of an axis aligned box with a line (touching counts). */
static bool boxTouchesLine(float x0, float y0, float x1, float y1, const Line *l) {
    if (l->maxx < x0 || l->minx > x1 || l->maxy < y0 || l->miny > y1) return false;
    float s0 = phys_side(l, x0, y0), s1 = phys_side(l, x1, y0), s2 = phys_side(l, x1, y1), s3 = phys_side(l, x0, y1);
    bool allPos = s0 > 0 && s1 > 0 && s2 > 0 && s3 > 0;
    bool allNeg = s0 < 0 && s1 < 0 && s2 < 0 && s3 < 0;
    return !(allPos || allNeg);
}

float phys_side(const Line *l, float x, float y) { return l->dx * (y - l->y1) - l->dy * (x - l->x1); }

typedef struct { float x, y, r; bool monster; bool ok; } CheckCtx;

static bool check_visit(Line *l, void *vc) {
    CheckCtx *c = (CheckCtx *) vc;
    float x = c->x, y = c->y, r = c->r;
    if (!boxTouchesLine(x - r, y - r, x + r, y + r, l)) return true;
    if (!l->back || (l->flags & ML_BLOCKING) != 0 || (c->monster && (l->flags & ML_BLOCKMONSTERS) != 0)) {
        PH.tmBlockLine = l; c->ok = false; return true;
    }
    float top = minf(l->fs->ceil, l->bs->ceil), bot = maxf(l->fs->floor, l->bs->floor);
    float low = minf(l->fs->floor, l->bs->floor);
    if (top < PH.tmCeil) { PH.tmCeil = top; PH.tmCeilLine = l; }
    if (bot > PH.tmFloor) { PH.tmFloor = bot; PH.tmFloorLine = l; }
    if (low < PH.tmDrop) PH.tmDrop = low;
    return true;
}

/* Fills PH.tm* fields for mobj m placed at (x,y). Returns false if blocked by a line or solid thing. */
bool phys_checkPosition(Mobj *m, float x, float y, bool ignoreThings) {
    Level *L = GM->L;
    float r = m->radius;
    Sector *s = level_sectorAt(L, x, y);
    PH.tmFloor = s->floor; PH.tmDrop = s->floor; PH.tmCeil = s->ceil;
    PH.tmBlockLine = NULL; PH.tmBlockMobj = NULL; PH.tmFloorLine = NULL; PH.tmCeilLine = NULL;
    CheckCtx c = {x, y, r, m->kind == MK_MONSTER, true};

    if (!ignoreThings && (m->solid || m->kind == MK_MISSILE)) {
        for (int i = 0, n = GM->mobjs.n; i < n; i++) {
            Mobj *o = GM->mobjs.d[i];
            if (o == m || o->removed || !o->solid) continue;
            if (m->kind == MK_MISSILE && !o->shootable) continue;
            float rr = r + o->radius;
            if (absf(o->x - x) >= rr || absf(o->y - y) >= rr) continue;
            if (m->z >= o->z + o->height || m->z + m->height <= o->z) continue;
            if (m->kind == MK_MISSILE && (o == m->owner)) continue;
            PH.tmBlockMobj = o;
            c.ok = false;
            break;
        }
    }
    level_forLinesInBox(L, x - r, y - r, x + r, y + r, check_visit, &c);
    return c.ok;
}

bool phys_tryMove(Mobj *m, float nx, float ny) {
    if (!phys_checkPosition(m, nx, ny, false)) return false;
    bool fly = m->noGravity;
    if (PH.tmCeil - PH.tmFloor < m->height) { PH.tmBlockLine = PH.tmCeilLine ? PH.tmCeilLine : PH.tmFloorLine; return false; }
    if (!fly) {
        if (PH.tmCeil - m->z < m->height) { PH.tmBlockLine = PH.tmCeilLine; return false; }
        if (PH.tmFloor - m->z > 24.0f) { PH.tmBlockLine = PH.tmFloorLine; return false; }
        if (m->kind == MK_MONSTER && PH.tmFloor - PH.tmDrop > 24.0f) return false;
    } else {
        if (PH.tmCeil - m->z < m->height) { PH.tmBlockLine = PH.tmCeilLine; return false; }
    }
    m->x = nx; m->y = ny;
    m->floorz = PH.tmFloor; m->ceilz = PH.tmCeil;
    m->sector = level_sectorAt(GM->L, nx, ny);
    return true;
}

/* Move with wall sliding. Returns true if any movement happened. */
bool phys_slideMove(Mobj *m, float dx, float dy) {
    if (phys_tryMove(m, m->x + dx, m->y + dy)) return true;
    Line *bl = PH.tmBlockLine;
    if (bl && bl->len > 0) {
        float ux = bl->dx / bl->len, uy = bl->dy / bl->len;
        float dot = dx * ux + dy * uy;
        if (phys_tryMove(m, m->x + ux * dot, m->y + uy * dot)) {
            float md = m->mx * ux + m->my * uy;
            m->mx = ux * md; m->my = uy * md;
            return true;
        }
    }
    bool moved = false;
    if (phys_tryMove(m, m->x + dx, m->y)) { moved = true; m->my *= 0.3f; }
    else if (phys_tryMove(m, m->x, m->y + dy)) { moved = true; m->mx *= 0.3f; }
    else { m->mx = 0; m->my = 0; }
    return moved;
}

/* ------------------------------------------------------------------ line of sight */
typedef struct { float dx, dy, ax, ay, az, dz; bool blocked; } LosCtx;

static bool los_visit(Line *l, void *vc) {
    LosCtx *c = (LosCtx *) vc;
    float den = c->dx * l->dy - c->dy * l->dx;
    if (absf(den) < 1e-6f) return true;
    float t = ((l->x1 - c->ax) * l->dy - (l->y1 - c->ay) * l->dx) / den;
    float u = ((l->x1 - c->ax) * c->dy - (l->y1 - c->ay) * c->dx) / den;
    if (t <= 0.0f || t >= 1.0f || u < 0.0f || u > 1.0f) return true;
    if (!l->back) { c->blocked = true; return false; }
    float top = minf(l->fs->ceil, l->bs->ceil), bot = maxf(l->fs->floor, l->bs->floor);
    if (top <= bot) { c->blocked = true; return false; }
    float z = c->az + c->dz * t;
    if (z < bot || z > top) { c->blocked = true; return false; }
    return true;
}

/* Does a ray from (x1,y1,z1) to (x2,y2,z2) pass through all two-sided openings and no solid wall? */
bool phys_los(float x1, float y1, float z1, float x2, float y2, float z2) {
    LosCtx c = {x2 - x1, y2 - y1, x1, y1, z1, z2 - z1, false};
    level_forLinesInBox(GM->L, minf(x1, x2) - 1, minf(y1, y2) - 1, maxf(x1, x2) + 1, maxf(y1, y2) + 1, los_visit, &c);
    return !c.blocked;
}

bool phys_canSee(Mobj *a, Mobj *b) {
    return phys_los(a->x, a->y, a->z + a->height * 0.75f, b->x, b->y, b->z + b->height * 0.75f);
}

/* ------------------------------------------------------------------ hitscan */
typedef struct { float dx, dy, fx, fy, fz, sl, rng; float bestWall; Line *wallLine; } TraceCtx;

static bool trace_visit(Line *l, void *vc) {
    TraceCtx *c = (TraceCtx *) vc;
    float den = c->dx * l->dy - c->dy * l->dx;
    if (absf(den) < 1e-6f) return true;
    float t = ((l->x1 - c->fx) * l->dy - (l->y1 - c->fy) * l->dx) / den;
    float u = ((l->x1 - c->fx) * c->dy - (l->y1 - c->fy) * c->dx) / den;
    if (t <= 0.0f || t >= 1.0f || u < 0.0f || u > 1.0f) return true;
    float dist = t * c->rng;
    if (dist >= c->bestWall) return true;
    if (l->back) {
        float top = minf(l->fs->ceil, l->bs->ceil), bot = maxf(l->fs->floor, l->bs->floor);
        float z = c->fz + c->sl * dist;
        if (top > bot && z >= bot && z <= top) return true;
    }
    c->bestWall = dist; c->wallLine = l;
    return true;
}

/* Casts a ray, returning nearest thing or wall hit. */
Hit phys_trace(Mobj *shooter, float x0, float y0, float z0, float angle, float range, float slope) {
    Level *L = GM->L;
    float c = (float) cos((double) angle), s = (float) sin((double) angle);
    float ex = x0 + c * range, ey = y0 + s * range;
    TraceCtx tc = {ex - x0, ey - y0, x0, y0, z0, slope, range, range + 1, NULL};
    level_forLinesInBox(L, minf(x0, ex) - 1, minf(y0, ey) - 1, maxf(x0, ex) + 1, maxf(y0, ey) + 1, trace_visit, &tc);
    Hit h;
    memset(&h, 0, sizeof(h));
    float best = tc.bestWall;
    Mobj *hm = NULL;
    for (int i = 0, n = GM->mobjs.n; i < n; i++) {
        Mobj *o = GM->mobjs.d[i];
        if (o == shooter || o->removed || !o->shootable) continue;
        float ox = o->x - x0, oy = o->y - y0;
        float along = ox * c + oy * s;
        if (along <= 0 || along >= best) continue;
        float perp = absf(-ox * s + oy * c);
        if (perp > o->radius) continue;
        float z = z0 + slope * along;
        if (z < o->z || z > o->z + o->height) continue;
        best = along; hm = o;
    }
    if (hm) {
        h.mobj = hm; h.dist = best;
    } else if (tc.wallLine) {
        h.line = tc.wallLine; h.dist = tc.bestWall;
        Line *l = tc.wallLine;
        if (l->back && l->bs && l->fs && (l->fs->skyCeil && l->bs->skyCeil)) h.hitSky = true;
    } else {
        h.dist = range; h.hitSky = true;
        h.x = x0 + c * range; h.y = y0 + s * range; h.z = z0 + slope * range;
        return h;
    }
    float back = hm ? 0.0f : 4.0f;
    h.x = x0 + c * (h.dist - back); h.y = y0 + s * (h.dist - back); h.z = z0 + slope * h.dist;
    return h;
}

/* Vertical autoaim. Returns target (or NULL) and sets PH.aimSlope / PH.aimAngle. */
Mobj *phys_aim(Mobj *shooter, float angle, float range, bool assist) {
    float eye = shooter->z + (shooter->kind == MK_PLAYER ? 41.0f : shooter->height * 0.75f);
    static const float offs[3] = {0.0f, 0.098f, -0.098f};
    Mobj *best = NULL; float bestD = 1e9f;
    for (int k = 0; k < 3; k++) {
        float a = angle + offs[k];
        float ca = (float) cos((double) a), sa = (float) sin((double) a);
        for (int i = 0, n = GM->mobjs.n; i < n; i++) {
            Mobj *o = GM->mobjs.d[i];
            if (o == shooter || o->removed || !o->shootable) continue;
            float ox = o->x - shooter->x, oy = o->y - shooter->y;
            float along = ox * ca + oy * sa;
            if (along <= 0 || along > range) continue;
            float perp = absf(-ox * sa + oy * ca);
            if (perp > o->radius) continue;
            if (along < bestD && phys_los(shooter->x, shooter->y, eye, o->x, o->y, o->z + o->height * 0.5f)) {
                bestD = along; best = o;
            }
        }
        if (best) break;
    }
    if (best) {
        float d = (float) hypot((double) (best->x - shooter->x), (double) (best->y - shooter->y));
        PH.aimSlope = d > 1.0f ? (best->z + best->height * 0.5f - eye) / d : 0.0f;
        PH.aimAngle = assist ? (float) atan2((double) (best->y - shooter->y), (double) (best->x - shooter->x)) : angle;
        float diff = absf(phys_normAngle(PH.aimAngle - angle));
        if (assist && diff > 0.11f) PH.aimAngle = angle;
        if (!assist) PH.aimAngle = angle;
    } else { PH.aimSlope = 0.0f; PH.aimAngle = angle; }
    return best;
}

float phys_normAngle(float a) {
    double t = DOOM_PI * 2;
    a = (float) fmod((double) a, t);
    if (a > DOOM_PI) a = (float) ((double) a - t);
    if (a < -DOOM_PI) a = (float) ((double) a + t);
    return a;
}
