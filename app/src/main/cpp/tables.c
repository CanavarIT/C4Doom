/* Data tables: static things (decorations, pickups, barrels), monsters, projectiles. Key = DoomEd number. */
#include "doom.h"

/* ================================================================== things */
static ThingSpec SPECS[128];
static int nSpecs;

static ThingSpec *d(int ed, const char *spr, const char *frames, int tics, bool solid, float r, float h, bool fb) {
    ThingSpec *s = &SPECS[nSpecs++];
    memset(s, 0, sizeof(*s));
    s->ed = ed; s->spr = spr; s->frames = frames; s->tics = tics; s->solid = solid; s->radius = r; s->height = h; s->fullbright = fb;
    return s;
}
static ThingSpec *item(int ed, const char *spr, const char *frames, int pk, bool fb) {
    ThingSpec *s = d(ed, spr, frames, 6, false, 20, 16, fb);
    s->pickup = pk;
    return s;
}

const ThingSpec *things_spec(int ed) {
    for (int i = nSpecs - 1; i >= 0; i--) if (SPECS[i].ed == ed) return &SPECS[i];
    return NULL;
}
int things_spec_count(void) { return nSpecs; }
const ThingSpec *things_spec_at(int i) { return &SPECS[i]; }

static void things_init(void) {
    nSpecs = 0;
    ThingSpec *s;
    /* decorations (solid) */
    d(2028, "COLU", "A", 6, true, 16, 16, true);
    d(30, "COL1", "A", 6, true, 16, 16, false);
    d(31, "COL2", "A", 6, true, 16, 16, false);
    d(32, "COL3", "A", 6, true, 16, 16, false);
    d(33, "COL4", "A", 6, true, 16, 16, false);
    d(35, "CBRA", "A", 6, true, 16, 16, true);
    d(36, "COL5", "AB", 14, true, 16, 16, false);
    d(37, "COL6", "A", 6, true, 16, 16, false);
    d(41, "CEYE", "ABCB", 6, true, 16, 16, true);
    d(42, "FSKU", "ABC", 6, true, 16, 16, true);
    d(43, "TRE1", "A", 6, true, 16, 16, false);
    d(44, "TBLU", "ABCD", 4, true, 16, 16, true);
    d(45, "TGRN", "ABCD", 4, true, 16, 16, true);
    d(46, "TRED", "ABCD", 4, true, 16, 16, true);
    d(47, "SMIT", "A", 6, true, 16, 16, false);
    d(48, "ELEC", "A", 6, true, 16, 16, false);
    d(54, "TRE2", "A", 6, true, 32, 16, false);
    d(55, "SMBT", "ABCD", 4, true, 16, 16, true);
    d(56, "SMGT", "ABCD", 4, true, 16, 16, true);
    d(57, "SMRT", "ABCD", 4, true, 16, 16, true);
    d(70, "FCAN", "ABC", 4, true, 10, 42, true);
    d(85, "TLMP", "ABCD", 4, true, 16, 16, true);
    d(86, "TLP2", "ABCD", 4, true, 16, 16, true);
    d(25, "POL1", "A", 6, true, 16, 16, false);
    d(26, "POL6", "AB", 6, true, 16, 16, false);
    d(27, "POL4", "A", 6, true, 16, 16, false);
    d(28, "POL2", "A", 6, true, 16, 16, false);
    d(29, "POL3", "AB", 6, true, 16, 16, true);
    /* decorations (walk-through) */
    d(34, "CAND", "A", 6, false, 16, 16, true);
    d(10, "PLAY", "W", 6, false, 16, 16, false);
    d(12, "PLAY", "W", 6, false, 16, 16, false);
    d(15, "PLAY", "N", 6, false, 16, 16, false);
    d(18, "POSS", "L", 6, false, 16, 16, false);
    d(19, "SPOS", "L", 6, false, 16, 16, false);
    d(20, "TROO", "M", 6, false, 16, 16, false);
    d(21, "SARG", "N", 6, false, 16, 16, false);
    d(22, "HEAD", "L", 6, false, 16, 16, false);
    d(23, "SKUL", "K", 6, false, 16, 16, false);
    d(24, "POL5", "A", 6, false, 16, 16, false);
    d(79, "POB1", "A", 6, false, 16, 16, false);
    d(80, "POB2", "A", 6, false, 16, 16, false);
    d(81, "BRS1", "A", 6, false, 16, 16, false);
    /* hanging */
    s = d(49, "GOR1", "ABCB", 10, true, 16, 68, false); s->hang = true;
    s = d(50, "GOR2", "A", 6, true, 16, 84, false); s->hang = true;
    s = d(51, "GOR3", "A", 6, true, 16, 84, false); s->hang = true;
    s = d(52, "GOR4", "A", 6, true, 16, 68, false); s->hang = true;
    s = d(53, "GOR5", "A", 6, true, 16, 52, false); s->hang = true;
    s = d(59, "GOR2", "A", 6, false, 20, 84, false); s->hang = true;
    s = d(60, "GOR4", "A", 6, false, 20, 68, false); s->hang = true;
    s = d(61, "GOR3", "A", 6, false, 20, 52, false); s->hang = true;
    s = d(62, "GOR5", "A", 6, false, 20, 52, false); s->hang = true;
    s = d(63, "GOR1", "ABCB", 10, false, 20, 68, false); s->hang = true;
    s = d(73, "HDB1", "A", 6, true, 16, 88, false); s->hang = true;
    s = d(74, "HDB2", "A", 6, true, 16, 88, false); s->hang = true;
    s = d(75, "HDB3", "A", 6, true, 16, 64, false); s->hang = true;
    s = d(76, "HDB4", "A", 6, true, 16, 64, false); s->hang = true;
    s = d(77, "HDB5", "A", 6, true, 16, 64, false); s->hang = true;
    s = d(78, "HDB6", "A", 6, true, 16, 64, false); s->hang = true;
    /* barrel */
    s = d(2035, "BAR1", "AB", 6, true, 10, 42, false); s->barrel = true;

    /* ammo */
    item(2007, "CLIP", "A", P_CLIP, false);
    item(2048, "AMMO", "A", P_CLIPBOX, false);
    item(2008, "SHEL", "A", P_SHELL, false);
    item(2049, "SBOX", "A", P_SHELLBOX, false);
    item(2010, "ROCK", "A", P_ROCKET, false);
    item(2046, "BROK", "A", P_ROCKETBOX, false);
    item(2047, "CELL", "A", P_CELL, false);
    item(17, "CELP", "A", P_CELLPACK, false);
    item(8, "BPAK", "A", P_BACKPACK, false);
    /* health / armor / powerups */
    item(2011, "STIM", "A", P_STIM, false);
    item(2012, "MEDI", "A", P_MEDI, false);
    item(2014, "BON1", "ABCDCB", P_HBONUS, true);
    item(2015, "BON2", "ABCDCB", P_ABONUS, true);
    item(2018, "ARM1", "AB", P_ARMOR1, false);
    item(2019, "ARM2", "AB", P_ARMOR2, false);
    item(2013, "SOUL", "ABCDCB", P_SOUL, true);
    item(83, "MEGA", "ABCD", P_MEGA, true);
    item(2022, "PINV", "ABCD", P_INVULN, true);
    item(2023, "PSTR", "A", P_BERSERK, true);
    item(2024, "PINS", "ABCD", P_INVIS, true);
    item(2025, "SUIT", "A", P_SUIT, false);
    item(2026, "PMAP", "ABCDCB", P_MAP, true);
    item(2045, "PVIS", "AB", P_LIGHT, true);
    /* keys */
    item(5, "BKEY", "AB", P_KEY_BLUE, false);
    item(40, "BSKU", "AB", P_KEY_BLUE, false);
    item(6, "YKEY", "AB", P_KEY_YELLOW, false);
    item(39, "YSKU", "AB", P_KEY_YELLOW, false);
    item(13, "RKEY", "AB", P_KEY_RED, false);
    item(38, "RSKU", "AB", P_KEY_RED, false);
    /* weapons */
    item(2001, "SHOT", "A", P_W_SHOTGUN, false);
    item(2002, "MGUN", "A", P_W_CHAINGUN, false);
    item(2003, "LAUN", "A", P_W_ROCKET, false);
    item(2004, "PLAS", "A", P_W_PLASMA, false);
    item(2006, "BFUG", "A", P_W_BFG, false);
    item(2005, "CSAW", "A", P_W_SAW, false);
    item(82, "SGN2", "A", P_W_SSG, false);
}

/* ================================================================== monsters */
static MonsterDef DEFS[16];
static int nDefs;
static MissileDef MISSILES[8];
static int nMissiles;
static Seq SEQS[64];
static int nSeqs;

/* all frames get the same tic count */
static Seq *seq(const char *frames, int tics, int events) {
    Seq *q = &SEQS[nSeqs++];
    q->frames = frames; q->len = (int) strlen(frames); q->events = events;
    for (int i = 0; i < q->len; i++) q->tics[i] = tics;
    return q;
}
static Seq *seqA(const char *frames, const int *tics, int events) {
    Seq *q = &SEQS[nSeqs++];
    q->frames = frames; q->len = (int) strlen(frames); q->events = events;
    for (int i = 0; i < q->len; i++) q->tics[i] = tics[i];
    return q;
}
#define SEQ_ARR(frames, events, ...) seqA(frames, (const int[]){__VA_ARGS__}, events)

static MonsterDef *mk(int type, const char *spr, const char *name, int hp, float r, float h, float speed, int pain) {
    MonsterDef *m = &DEFS[nDefs++];
    memset(m, 0, sizeof(*m));
    m->type = type; strncpy(m->spr, spr, 4); m->name = name; m->hp = hp; m->radius = r; m->height = h; m->speed = speed; m->painChance = pain;
    m->mass = 100; m->pellets = 1; m->dmgMul = 3; m->dmgDie = 5;
    return m;
}

static MissileDef *mm(const char *name, const char *spr, const char *fly, const char *boom, float speed, int mul, int die) {
    MissileDef *m = &MISSILES[nMissiles++];
    memset(m, 0, sizeof(*m));
    m->name = name; m->spr = spr; m->fly = fly; m->boom = boom; m->speed = speed; m->dmgMul = mul; m->dmgDie = die;
    m->flyTics = 4; m->boomTics = 6; m->radius = 6; m->height = 8; m->fullbright = true;
    return m;
}

static void monsters_init(void) {
    nDefs = 0; nMissiles = 0; nSeqs = 0;
    MonsterDef *d;
    /* ---- Zombieman */
    d = mk(3004, "POSS", "Zombieman", 20, 20, 56, 8, 200);
    d->stand = seq("AB", 10, 0); d->walk = seq("AABBCCDD", 4, 0);
    d->attack = SEQ_ARR("EFE", 2, 10, 8, 8);
    d->pain = seq("GG", 3, 0); d->die = SEQ_ARR("HIJKL", 0, 5, 5, 5, 5, -1);
    d->atk = ATK_HITSCAN; d->pellets = 1; d->atkSnd = "DSPISTOL"; d->sight = "DSPOSIT1"; d->act = "DSPOSACT";
    d->painSnd = "DSPOPAIN"; d->deathSnd = "DSPODTH1"; d->drop = 2007;
    /* ---- Shotgun guy */
    d = mk(9, "SPOS", "Sergeant", 30, 20, 56, 8, 170);
    d->stand = seq("AB", 10, 0); d->walk = seq("AABBCCDD", 3, 0);
    d->attack = SEQ_ARR("EFE", 2, 10, 10, 10);
    d->pain = seq("GG", 3, 0); d->die = SEQ_ARR("HIJKL", 0, 5, 5, 5, 5, -1);
    d->atk = ATK_HITSCAN; d->pellets = 3; d->atkSnd = "DSSHOTGN"; d->sight = "DSPOSIT2"; d->act = "DSPOSACT";
    d->painSnd = "DSPOPAIN"; d->deathSnd = "DSPODTH2"; d->drop = 2001;
    /* ---- Chaingunner */
    d = mk(65, "CPOS", "Chaingunner", 70, 20, 56, 8, 170);
    d->stand = seq("AB", 10, 0); d->walk = seq("AABBCCDD", 3, 0);
    d->attack = SEQ_ARR("EFEFEF", 2 | 8 | 32, 10, 4, 4, 4, 4, 4);
    d->pain = seq("GG", 3, 0); d->die = SEQ_ARR("HIJKLMN", 0, 5, 5, 5, 5, 5, 5, -1);
    d->atk = ATK_HITSCAN; d->pellets = 1; d->atkSnd = "DSSHOTGN"; d->sight = "DSPOSIT3"; d->act = "DSPOSACT";
    d->painSnd = "DSPOPAIN"; d->deathSnd = "DSPODTH3"; d->drop = 2002;
    /* ---- Imp */
    d = mk(3001, "TROO", "Imp", 60, 20, 56, 8, 200);
    d->stand = seq("AB", 10, 0); d->walk = seq("AABBCCDD", 3, 0);
    d->attack = SEQ_ARR("EFG", 4, 8, 8, 6);
    d->pain = seq("HH", 2, 0); d->die = SEQ_ARR("IJKLM", 0, 8, 8, 6, 6, -1);
    d->atk = ATK_MISSILE; d->missile = "IMPBALL"; d->meleeMul = 3; d->meleeDie = 8; d->meleeSnd = "DSCLAW";
    d->sight = "DSBGSIT1"; d->act = "DSBGACT"; d->painSnd = "DSPOPAIN"; d->deathSnd = "DSBGDTH1"; d->atkSnd = "DSFIRSHT";
    /* ---- Demon & Spectre */
    d = mk(3002, "SARG", "Demon", 150, 30, 56, 10, 180);
    d->stand = seq("AB", 10, 0); d->walk = seq("AABBCCDD", 2, 0);
    d->attack = seq("EFG", 8, 4);
    d->pain = seq("HH", 2, 0); d->die = SEQ_ARR("IJKLMN", 0, 8, 8, 4, 4, 4, -1);
    d->atk = ATK_MELEE; d->meleeMul = 4; d->meleeDie = 10; d->meleeSnd = "DSSGTATK";
    d->sight = "DSSGTSIT"; d->act = "DSDMACT"; d->painSnd = "DSDMPAIN"; d->deathSnd = "DSSGTDTH";
    MonsterDef *sp = mk(58, "SARG", "Spectre", 150, 30, 56, 10, 180);
    sp->stand = d->stand; sp->walk = d->walk; sp->attack = d->attack; sp->pain = d->pain; sp->die = d->die;
    sp->atk = d->atk; sp->meleeMul = 4; sp->meleeDie = 10; sp->meleeSnd = d->meleeSnd; sp->fuzz = true;
    sp->sight = d->sight; sp->act = d->act; sp->painSnd = d->painSnd; sp->deathSnd = d->deathSnd;
    /* ---- Lost soul */
    d = mk(3006, "SKUL", "Lost Soul", 100, 16, 56, 8, 256);
    d->stand = seq("AB", 10, 0); d->walk = seq("AB", 6, 0);
    d->attack = SEQ_ARR("CD", 2, 10, 4);
    d->pain = seq("EF", 3, 0); d->die = SEQ_ARR("GHIJK", 0, 6, 6, 6, 6, -1);
    d->fly = true; d->atk = ATK_CHARGE; d->meleeMul = 3; d->meleeDie = 8; d->mass = 50; d->fuzz = false;
    d->sight = "DSSKLATK"; d->act = "DSDMACT"; d->painSnd = "DSDMPAIN"; d->deathSnd = "DSFIRXPL"; d->atkSnd = "DSSKLATK";
    /* ---- Cacodemon */
    d = mk(3005, "HEAD", "Cacodemon", 400, 31, 56, 8, 128);
    d->stand = seq("A", 10, 0); d->walk = seq("A", 3, 0);
    d->attack = seq("BCD", 5, 4);
    d->pain = seq("EF", 3, 0); d->die = SEQ_ARR("GHIJKL", 0, 8, 8, 8, 8, 8, -1);
    d->fly = true; d->mass = 400; d->atk = ATK_MISSILE; d->missile = "CACOBALL"; d->meleeMul = 10; d->meleeDie = 6; d->meleeSnd = "DSCLAW";
    d->sight = "DSCACSIT"; d->act = "DSDMACT"; d->painSnd = "DSDMPAIN"; d->deathSnd = "DSCACDTH"; d->atkSnd = "DSFIRSHT";
    /* ---- Baron / Hell knight */
    d = mk(3003, "BOSS", "Baron of Hell", 1000, 24, 64, 8, 50);
    d->stand = seq("AB", 10, 0); d->walk = seq("AABBCCDD", 3, 0);
    d->attack = seq("EFG", 8, 4);
    d->pain = seq("HH", 2, 0); d->die = SEQ_ARR("IJKLMNO", 0, 8, 8, 8, 8, 8, 8, -1);
    d->mass = 1000; d->atk = ATK_MISSILE; d->missile = "BARONBALL"; d->meleeMul = 10; d->meleeDie = 8; d->meleeSnd = "DSCLAW";
    d->sight = "DSBRSSIT"; d->act = "DSDMACT"; d->painSnd = "DSDMPAIN"; d->deathSnd = "DSBRSDTH"; d->atkSnd = "DSFIRSHT";
    MonsterDef *hk = mk(69, "BOS2", "Hell Knight", 500, 24, 64, 8, 50);
    hk->stand = d->stand; hk->walk = d->walk; hk->attack = d->attack; hk->pain = d->pain; hk->die = d->die;
    hk->mass = 1000; hk->atk = ATK_MISSILE; hk->missile = "BARONBALL"; hk->meleeMul = 10; hk->meleeDie = 8; hk->meleeSnd = "DSCLAW";
    hk->sight = "DSKNTSIT"; hk->act = "DSDMACT"; hk->painSnd = "DSDMPAIN"; hk->deathSnd = "DSKNTDTH"; hk->atkSnd = "DSFIRSHT";

    /* ---- Missiles */
    MissileDef *m;
    m = mm("IMPBALL", "BAL1", "AB", "CDE", 10, 3, 8); m->launchSnd = "DSFIRSHT"; m->hitSnd = "DSFIRXPL";
    m = mm("CACOBALL", "BAL2", "AB", "CDE", 10, 5, 8); m->launchSnd = "DSFIRSHT"; m->hitSnd = "DSFIRXPL";
    m = mm("BARONBALL", "BAL7", "AB", "CDE", 15, 8, 8); m->launchSnd = "DSFIRSHT"; m->hitSnd = "DSFIRXPL";
    m = mm("ROCKET", "MISL", "A", "BCD", 20, 20, 8); m->splash = true; m->launchSnd = "DSRLAUNC"; m->hitSnd = "DSBAREXP";
    m->boomTics = 6; m->radius = 11; m->height = 8;
    m = mm("PLASMA", "PLSS", "AB", "PLSE", 25, 5, 8); m->launchSnd = "DSPLASMA"; m->hitSnd = "DSFIRXPL"; m->boomTics = 4;
    m->boom = "ABCDE"; m->flyTics = 6;
}

MonsterDef *monsters_def(int type) {
    for (int i = 0; i < nDefs; i++) if (DEFS[i].type == type) return &DEFS[i];
    return NULL;
}
MissileDef *monsters_missile(const char *name) {
    for (int i = 0; i < nMissiles; i++) if (strcmp(MISSILES[i].name, name) == 0) return &MISSILES[i];
    return NULL;
}

void tables_init(void) {
    things_init();
    monsters_init();
}
