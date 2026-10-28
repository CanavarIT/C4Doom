/* Monster behaviour: look, chase, attack, pain, death. */
#include "doom.h"

#define NODIR 8
static const int DIAGS[4] = {3, 1, 5, 7};

static Seq *seq_of(MonsterDef *d, int s) {
    switch (s) {
        case SEQ_STAND: return d->stand;
        case SEQ_WALK: return d->walk;
        case SEQ_ATTACK: return d->attack;
        case SEQ_PAIN: return d->pain;
        default: return d->die;
    }
}

static void look(Mobj *m);
static void chase(Mobj *m);
static void attack(Mobj *m);
static void faceTarget(Mobj *m);
static void newChaseDir(Mobj *m);
static void wake(Mobj *m);
static void die(Mobj *t, Mobj *source);
static void charge(Mobj *m);

static void enterStep(Mobj *m, Seq *q, bool fire) {
    if (m->step >= q->len) m->step = q->len - 1;
    int f = q->frames[m->step] - 'A';
    m->frame = f;
    m->tics = q->tics[m->step];
    m->fullbright = false;
    if (!fire) return;
    switch (m->seq) {
        case SEQ_STAND: look(m); break;
        case SEQ_WALK: chase(m); break;
        case SEQ_ATTACK:
            if (((q->events >> m->step) & 1) != 0) attack(m);
            else faceTarget(m);                           /* keep facing the player until the shot */
            break;
        default: break;
    }
}

void ai_setSeq(Mobj *m, int s, int step) {
    Seq *q = seq_of(m->def, s);
    if (!q) q = m->def->stand;
    m->seq = s; m->step = step;
    if (s == SEQ_ATTACK) faceTarget(m);      /* A_FaceTarget on the first attack frame */
    enterStep(m, q, false);
}

void ai_think(Mobj *m) {
    /* physics */
    if (m->shootable || m->corpse) {
        game_refreshFloor(m);
        if (!m->noGravity) game_gravity(m);
        else if (m->corpse) { m->z -= 2; if (m->z < m->floorz) m->z = m->floorz; }
    }
    if (m->charging) charge(m);
    else game_applyMomentum(m);
    if (m->painTics > 0) m->painTics--;
    /* animation */
    if (m->tics < 0) return;
    if (--m->tics > 0) return;
    Seq *q = seq_of(m->def, m->seq);
    if (!q) q = m->def->stand;
    m->step++;
    if (m->step >= q->len) {
        switch (m->seq) {
            case SEQ_STAND: case SEQ_WALK: m->step = 0; break;
            case SEQ_ATTACK:
                m->justAttacked = true;
                m->seq = m->target ? SEQ_WALK : SEQ_STAND; m->step = 0;
                q = seq_of(m->def, m->seq); break;
            case SEQ_PAIN:
                m->seq = m->target ? SEQ_WALK : SEQ_STAND; m->step = 0;
                q = seq_of(m->def, m->seq); break;
            default: m->step = q->len - 1; m->tics = -1; return;
        }
    }
    enterStep(m, q, true);
}

/* ------------------------------------------------------------------ look */
static void look(Mobj *m) {
    if (GM->noTarget) return;
    if (m->target) { wake(m); return; }
    Mobj *p = GM->pl;
    if (GM->state != ST_PLAY || !p->shootable) return;
    float dx = p->x - m->x, dy = p->y - m->y;
    float dist = (float) hypot((double) dx, (double) dy);
    if (dist > 2600.0f) return;
    if (dist > 64.0f) {
        float a = phys_normAngle((float) atan2((double) dy, (double) dx) - m->angle);
        if (absf(a) > DOOM_PI / 2) return;
    }
    if (!phys_canSee(m, p)) return;
    m->target = p;
    wake(m);
}

static void wake(Mobj *m) {
    game_snd(m->def->sight, m);
    m->reaction = 8;
    ai_setSeq(m, SEQ_WALK, 0);
}

void ai_alert(Mobj *m, Mobj *who) {
    if (m->target || !m->shootable || m->seq != SEQ_STAND) return;
    if (m->ambush && !phys_canSee(m, who)) return;
    m->target = who; wake(m);
}

/* ------------------------------------------------------------------ chase */
static float approach(float cur, float want, float step) {
    float d = phys_normAngle(want - cur);
    if (absf(d) <= step) return want;
    return cur + (d > 0 ? step : -step);
}

static void faceTarget(Mobj *m) {
    if (!m->target) return;
    float a = (float) atan2((double) (m->target->y - m->y), (double) (m->target->x - m->x));
    if (m->target->fuzz || (m->target == GM->pl && GM->invis > 0)) a += (float) game_rnd_diff() * 0.003068f;
    m->angle = a;
}

static bool meleeRange(Mobj *m) {
    if (!m->target) return false;
    float d = (float) hypot((double) (m->target->x - m->x), (double) (m->target->y - m->y));
    if (d >= 44.0f + m->target->radius) return false;
    return phys_canSee(m, m->target);
}

static bool missileRange(Mobj *m) {
    if (!phys_canSee(m, m->target)) return false;
    if (m->justHit) { m->justHit = false; return true; }
    if (m->reaction > 0) return false;
    float dist = (float) hypot((double) (m->target->x - m->x), (double) (m->target->y - m->y)) - 64.0f;
    if (m->def->meleeMul == 0) dist -= 128.0f;
    if (m->def->atk == ATK_CHARGE) dist -= 100.0f;
    if (dist > 200.0f) dist = 200.0f;
    if (dist < 0) dist = 0;
    return (float) game_rnd() >= dist;
}

static bool move(Mobj *m) {
    if (m->moveDir >= 8) return false;
    float a = m->moveDir * (float) (DOOM_PI / 4);
    float sp = m->def->speed;
    float nx = m->x + (float) cos((double) a) * sp, ny = m->y + (float) sin((double) a) * sp;
    if (!phys_tryMove(m, nx, ny)) {
        Line *bl = PH.tmBlockLine;
        if (bl && bl->special != 0) {
            if (specials_useLine(bl, m)) return true;
        }
        return false;
    }
    if (m->noGravity && m->target) {
        float dz = m->target->z - m->z;
        if (dz > 4) m->z += 4; else if (dz < -4) m->z -= 4;
        if (m->z < m->floorz) m->z = m->floorz;
        if (m->z + m->height > m->ceilz) m->z = m->ceilz - m->height;
    }
    return true;
}

static void chase(Mobj *m) {
    if (GM->noTarget && m->target == GM->pl) { m->target = NULL; ai_setSeq(m, SEQ_STAND, 0); return; }
    if (m->reaction > 0) m->reaction--;
    if (m->threshold > 0) { if (!m->target || !m->target->shootable) m->threshold = 0; else m->threshold--; }
    if (m->moveDir < 8) m->angle = approach(m->angle, m->moveDir * (float) (DOOM_PI / 4), (float) (DOOM_PI / 4));
    if (!m->target || !m->target->shootable) {
        m->target = NULL;
        if (GM->state == ST_PLAY && GM->pl->shootable && phys_canSee(m, GM->pl)) { m->target = GM->pl; }
        else { ai_setSeq(m, SEQ_STAND, 0); return; }
    }
    if (m->justAttacked) { m->justAttacked = false; newChaseDir(m); return; }
    MonsterDef *d = m->def;
    if (d->meleeMul > 0 && d->atk != ATK_CHARGE && meleeRange(m)) {
        game_snd(d->meleeSnd, m);
        ai_setSeq(m, SEQ_ATTACK, 0);
        return;
    }
    if (d->atk != ATK_NONE && d->atk != ATK_MELEE && m->moveCount <= 0 && missileRange(m)) {
        ai_setSeq(m, SEQ_ATTACK, 0);
        game_snd(d->atkSnd, m);
        return;
    }
    if (--m->moveCount < 0 || !move(m)) newChaseDir(m);
    if (game_rnd() < 3) game_snd(d->act, m);
}

static bool tryDir(Mobj *m, int dir) {
    if (dir == NODIR) return false;
    int old = m->moveDir;
    m->moveDir = dir;
    if (!move(m)) { m->moveDir = old; return false; }
    m->moveCount = game_rnd() & 15;
    return true;
}

static void newChaseDir(Mobj *m) {
    if (!m->target) return;
    int old = m->moveDir;
    int turn = old == NODIR ? NODIR : (old + 4) & 7;
    float dx = m->target->x - m->x, dy = m->target->y - m->y;
    int d1 = dx > 10 ? 0 : (dx < -10 ? 4 : NODIR);
    int d2 = dy < -10 ? 6 : (dy > 10 ? 2 : NODIR);
    if (d1 != NODIR && d2 != NODIR) {
        int dd = DIAGS[((dy < 0) ? 2 : 0) + (dx > 0 ? 1 : 0)];
        if (dd != turn && tryDir(m, dd)) return;
    }
    if (game_rnd() > 200 || absf(dy) > absf(dx)) { int t = d1; d1 = d2; d2 = t; }
    if (d1 == turn) d1 = NODIR;
    if (d2 == turn) d2 = NODIR;
    if (d1 != NODIR && tryDir(m, d1)) return;
    if (d2 != NODIR && tryDir(m, d2)) return;
    if (old != NODIR && tryDir(m, old)) return;
    if ((game_rnd() & 1) != 0) {
        for (int d = 0; d < 8; d++) if (d != turn && tryDir(m, d)) return;
    } else {
        for (int d = 7; d >= 0; d--) if (d != turn && tryDir(m, d)) return;
    }
    if (turn != NODIR && tryDir(m, turn)) return;
    m->moveDir = NODIR;
}

/* ------------------------------------------------------------------ attacks */
static void attack(Mobj *m) {
    MonsterDef *d = m->def;
    if (!m->target) return;
    faceTarget(m);
    switch (d->atk) {
        case ATK_HITSCAN: {
            for (int i = 0; i < d->pellets; i++) {
                float a = m->angle + (float) game_rnd_diff() * 0.001534f;
                float dist = (float) hypot((double) (m->target->x - m->x), (double) (m->target->y - m->y));
                float z0 = m->z + m->height * 0.75f;
                float slope = (m->target->z + m->target->height * 0.5f - z0) / maxf(1.0f, dist);
                game_shoot(m, a, 2048.0f, slope, ((game_rnd() % 5) + 1) * 3);
            }
            game_snd(d->atkSnd, m);
            break;
        }
        case ATK_MISSILE:
            if (d->meleeMul > 0 && meleeRange(m)) {
                game_snd(d->meleeSnd, m);
                game_damage(m->target, m, m, d->meleeMul * (game_rnd() % d->meleeDie + 1));
            } else {
                MissileDef *md = monsters_missile(d->missile);
                float dist = (float) hypot((double) (m->target->x - m->x), (double) (m->target->y - m->y));
                float z0 = m->z + 32.0f;
                float slope = (m->target->z + m->target->height * 0.5f - z0) / maxf(1.0f, dist);
                game_spawnMissile(m, md, m->angle, slope, 32.0f);
            }
            break;
        case ATK_MELEE:
            if (meleeRange(m)) {
                game_snd(d->meleeSnd, m);
                game_damage(m->target, m, m, d->meleeMul * (game_rnd() % d->meleeDie + 1));
            }
            break;
        case ATK_CHARGE: {
            game_snd(d->atkSnd, m);
            float a = (float) atan2((double) (m->target->y - m->y), (double) (m->target->x - m->x));
            m->angle = a;
            m->mx = (float) cos((double) a) * 20.0f; m->my = (float) sin((double) a) * 20.0f;
            float dist = (float) hypot((double) (m->target->x - m->x), (double) (m->target->y - m->y));
            float dt = maxf(1.0f, dist / 20.0f);
            m->mz = (m->target->z + m->target->height * 0.5f - (m->z + m->height * 0.5f)) / dt;
            m->charging = true; m->chargeTics = 40; m->fullbright = true;
            break;
        }
        default: break;
    }
}

static void stopCharge(Mobj *m) {
    m->charging = false; m->mx = m->my = m->mz = 0; m->fullbright = false;
    ai_setSeq(m, SEQ_WALK, 0);
}

static void charge(Mobj *m) {
    m->fullbright = true;
    float nx = m->x + m->mx, ny = m->y + m->my;
    if (--m->chargeTics <= 0) { stopCharge(m); return; }
    if (!phys_tryMove(m, nx, ny)) {
        Mobj *t = PH.tmBlockMobj;
        if (t && t->shootable && t != m) game_damage(t, m, m, m->def->meleeMul * (game_rnd() % m->def->meleeDie + 1));
        stopCharge(m);
        return;
    }
    m->z += m->mz;
    if (m->z < m->floorz) m->z = m->floorz;
    if (m->z + m->height > m->ceilz) m->z = m->ceilz - m->height;
}

/* ------------------------------------------------------------------ damage reaction */
void ai_hurt(Mobj *t, Mobj *inflictor, Mobj *source, int amount) {
    (void) inflictor; (void) amount;
    if (!t->def) { if (t->health <= 0) t->removed = true; return; }
    if (t->health <= 0) { die(t, source); return; }
    if (source && source != t && source->def != t->def && t->threshold <= 0 && source->shootable) {
        t->target = source; t->threshold = 100;
        if (t->seq == SEQ_STAND) wake(t);
    } else if (!t->target && source && source != t) {
        t->target = source; wake(t);
    }
    if (game_rnd() < t->def->painChance && !t->charging && t->def->pain) {
        t->justHit = true;
        game_snd(t->def->painSnd, t);
        ai_setSeq(t, SEQ_PAIN, 0);
    }
}

/* Original A_BossDeath: when the level's boss type is wiped out, tag 666 / 667 sectors open the way (or the episode ends). */
static void bossDeath(Mobj *t) {
    if (GM->health <= 0) return;
    int need;
    bool d2 = wad_isDoom2(GM->wad);
    const char *m = GM->mapName;
    if (d2) {
        if (strcmp(m, "MAP07") != 0) return;
        if (t->type != 67 && t->type != 68) return;      /* Mancubus / Arachnotron */
        need = t->type;
    } else {
        if (strlen(m) != 4) return;
        int e = m[1] - '0', mm = m[3] - '0';
        if (e == 1 && mm == 8) need = 3003;            /* Baron of Hell */
        else if (e == 2 && mm == 8) need = 16;         /* Cyberdemon */
        else if (e == 3 && mm == 8) need = 7;          /* Spiderdemon */
        else if (e == 4 && mm == 6) need = 16;
        else if (e == 4 && mm == 8) need = 7;
        else return;
        if (t->type != need) return;
    }
    for (int i = 0, n = GM->mobjs.n; i < n; i++) {
        Mobj *o = GM->mobjs.d[i];
        if (o != t && o->type == need && o->def && !o->corpse && !o->removed && o->health > 0) return;   /* another one is still alive */
    }
    Specials *sp = &GM->sp;
    sp->curLine = NULL;
    Level *L = GM->L;
    if (d2) {
        for (int i = 0; i < L->nsectors; i++) {
            Sector *s = &L->sectors[i];
            if (need == 67 && s->tag == 666) specials_startFloor(s, F_LOWER_LOWEST, false);
            if (need == 68 && s->tag == 667) specials_startFloor(s, F_RAISE_TEX, false);
        }
        return;
    }
    char e = m[1];
    if (e == '2' || e == '3') { game_exitLevel(false); return; }          /* E2M8 / E3M8: end of the episode */
    for (int i = 0; i < L->nsectors; i++) {
        Sector *s = &L->sectors[i];
        if (s->tag != 666) continue;
        if (strcmp(m, "E4M6") == 0) specials_startDoor(s, A_DOOR_STAY, true);
        else specials_startFloor(s, F_LOWER_LOWEST, false);
    }
}

static void die(Mobj *t, Mobj *source) {
    (void) source;
    if (t->corpse) return;
    t->corpse = true; t->shootable = false; t->solid = false; t->charging = false; t->fullbright = false;
    t->fuzz = false;
    if (t->def->fly) t->noGravity = false;
    if (t->countKill) GM->kills++;
    bossDeath(t);
    game_snd(t->def->deathSnd, t);
    ai_setSeq(t, SEQ_DIE, 0);
    if (t->def->drop != 0) {
        Thing th;
        memset(&th, 0, sizeof(th));
        th.x = t->x; th.y = t->y; th.type = t->def->drop; th.flags = 7;
        Mobj *item = game_spawnThing(&th);
        if (item) {
            game_removeMobj(item);
            if (item->countItem) GM->totalItems--;
            item->countItem = false;
            vec_push(GM->spawnQueue, item);
            item->z = t->z;
        }
    }
}
