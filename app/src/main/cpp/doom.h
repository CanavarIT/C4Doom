/*
 * C4Doom - a from-scratch Doom (1993) engine for Android in pure C.
 * Single shared header: types and prototypes for every module.
 */
#ifndef DOOM_H
#define DOOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define DOOM_PI 3.14159265358979323846

typedef uint8_t u8;
typedef uint32_t u32;

static inline int mini(int a, int b) { return a < b ? a : b; }
static inline int maxi(int a, int b) { return a > b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float absf(float a) { return a < 0 ? -a : a; }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
/* Java Math.round(float) */
static inline int jround(float x) { return (int) floorf(x + 0.5f); }

#define DEFVEC(name, T) typedef struct { T *d; int n, cap; } name
#define vec_push(v, item) do { \
        if ((v).n == (v).cap) { (v).cap = (v).cap ? (v).cap * 2 : 16; \
            (v).d = (__typeof__((v).d)) realloc((v).d, (size_t) (v).cap * sizeof(*(v).d)); } \
        (v).d[(v).n++] = (item); } while (0)
#define vec_clear(v) ((v).n = 0)
#define vec_free(v) do { free((v).d); (v).d = NULL; (v).n = (v).cap = 0; } while (0)

/* ------------------------------------------------------------------ string map (8 char lump names) */
typedef struct { char (*keys)[9]; void **vals; int cap, count; } StrMap;
void strmap_init(StrMap *m);
void strmap_free(StrMap *m);
void *strmap_get(const StrMap *m, const char *key, bool *found);
/* returns true when the entry was stored; with overwrite=false an existing key is kept */
bool strmap_put(StrMap *m, const char *key, void *val, bool overwrite);

/* ------------------------------------------------------------------ WAD */
typedef struct { char name[9]; int pos, size; } Lump;
typedef struct Wad {
    u8 *data; int len;
    Lump *lumps; int n;
    StrMap index;
    bool isIwad;
    char maps[256][9]; int nmaps;
} Wad;

static inline int rd_i32(const u8 *d) { return (int) ((u32) d[0] | (u32) d[1] << 8 | (u32) d[2] << 16 | (u32) d[3] << 24); }
static inline int rd_u16(const u8 *d) { return d[0] | d[1] << 8; }
static inline int rd_s16(const u8 *d) { return (int16_t) (d[0] | d[1] << 8); }
void cstr_up(char out[9], const u8 *d, int max);          /* copy up to max chars, upper-cased, zero terminated */
void str_upper(char out[9], const char *s);

bool wad_load(Wad *w, u8 *data, int len, char *err, int errsz);   /* takes ownership of data */
int wad_find(const Wad *w, const char *name);
static inline bool wad_has(const Wad *w, const char *name) { return wad_find(w, name) >= 0; }
int wad_find_after(const Wad *w, const char *name, int after);
const u8 *wad_lump(const Wad *w, int i, int *size);
const u8 *wad_lump_named(const Wad *w, const char *name, int *size);   /* NULL when missing */
bool wad_isDoom2(const Wad *w);
int wad_map_index(const Wad *w, const char *name);                    /* index in maps[] or -1 */

/* ------------------------------------------------------------------ graphics from the WAD */
typedef struct { int w, h, lo, to; u8 *pix; u8 *op; } Pic;   /* column major: pix[x*h+y]; op NULL = fully opaque */
typedef struct {
    Pic *pic[29][8];
    bool flip[29][8];
    bool rotates[29];
    bool present[29];
    int maxFrame;
} SpriteSet;

typedef struct Gfx {
    Wad *wad;
    const u8 *playpal;
    const u8 *colormap;
    u8 *colormapOwned;
    u32 lit[34][256];
    StrMap textures, flats, sprites, patchCache;
} Gfx;

Gfx *gfx_new(Wad *w, char *err, int errsz);
Pic *gfx_decode_patch(const u8 *d, int len);
Pic *gfx_patch(Gfx *g, const char *name);
Pic *gfx_texture(Gfx *g, const char *name);
const u8 *gfx_flat(Gfx *g, const char *name);
SpriteSet *gfx_sprite(Gfx *g, const char *prefix);

/* ------------------------------------------------------------------ level */
enum { ML_BLOCKING = 1, ML_BLOCKMONSTERS = 2, ML_TWOSIDED = 4, ML_DONTPEGTOP = 8, ML_DONTPEGBOTTOM = 16, ML_SECRET = 32, ML_SOUNDBLOCK = 64 };

struct Sector; struct Line; struct Mover;
typedef struct Side {
    float xoff, yoff;
    Pic *upper, *lower, *mid;
    char upperName[9], lowerName[9], midName[9];
    struct Sector *sector;
} Side;
typedef struct Line {
    int idx, v1, v2, flags, special, tag;
    float x1, y1, x2, y2, dx, dy, len;
    Side *front, *back;
    struct Sector *fs, *bs;
    int validcount;
    bool used, mapped;
    float minx, maxx, miny, maxy;
} Line;
typedef struct Sector {
    float floor, ceil;
    const u8 *floorPic, *ceilPic;
    char floorName[9], ceilName[9];
    bool skyCeil;
    int light, special, tag, idx;
    Line **lines; int nlines;
    struct Mover *mover;
    float origFloor, origCeil;
    bool secretFound;
    float cx, cy;
} Sector;
typedef struct Seg {
    float x1, y1, x2, y2, len, offset;
    Line *line; Side *side; Sector *front, *back;
    bool dir;
} Seg;
typedef struct Thing { float x, y; int angle, type, flags; } Thing;
typedef struct Mover {
    Sector *sector;
    int kind;                 /* 0 door, 1 lift, 2 floor, 3 ceiling */
    float target, speed;
    int state;                /* 0 moving to target, 1 waiting, 2 returning, 3 closing */
    int wait, waitMax;
    float home;
    bool useCeil, once, reopen, reopenWait;
    Line *triggerLine;
} Mover;

typedef struct Level {
    char name[9];
    int nv; float *vx, *vy;
    Line *lines; int nlines;
    Side *sides; int nsides;
    Sector *sectors; int nsectors;
    Seg *segs; int nsegs;
    int nss; int *ssNum, *ssFirst; Sector **ssSector;
    int nn; float *nx, *ny, *ndx, *ndy; float (*nbb)[8]; int *nchild;
    Thing *things; int nthings;
    float minX, minY, maxX, maxY;
    int gx0, gy0, gw, gh; int **grid; int *gridN;
    int validcount;
} Level;

Level *level_load(Wad *w, Gfx *gfx, const char *mapName, char *err, int errsz);
void level_free(Level *L);
int level_pointOnSide(const Level *L, float x, float y, int node);
Sector *level_sectorAt(const Level *L, float x, float y);
typedef bool (*LineVisitor)(Line *l, void *ctx);
bool level_forLinesInBox(Level *L, float x0, float y0, float x1, float y1, LineVisitor v, void *ctx);

/* ------------------------------------------------------------------ data tables */
typedef struct {
    int ed; const char *spr, *frames; int tics;
    bool solid, fullbright, hang, barrel;
    float radius, height; int pickup;
} ThingSpec;
enum { P_CLIP = 1, P_CLIPBOX, P_SHELL, P_SHELLBOX, P_ROCKET, P_ROCKETBOX, P_CELL, P_CELLPACK, P_BACKPACK, P_STIM, P_MEDI,
       P_HBONUS, P_ABONUS, P_ARMOR1, P_ARMOR2, P_SOUL, P_MEGA, P_INVULN, P_BERSERK, P_INVIS, P_SUIT, P_MAP, P_LIGHT,
       P_KEY_BLUE, P_KEY_YELLOW, P_KEY_RED, P_W_SHOTGUN = 30, P_W_CHAINGUN, P_W_ROCKET, P_W_PLASMA, P_W_BFG, P_W_SAW, P_W_SSG };

enum { SEQ_STAND = 0, SEQ_WALK, SEQ_ATTACK, SEQ_PAIN, SEQ_DIE };
enum { ATK_NONE = 0, ATK_HITSCAN, ATK_MISSILE, ATK_CHARGE, ATK_MELEE };
typedef struct { const char *frames; int len; int tics[8]; int events; } Seq;
typedef struct {
    int type; char spr[5]; const char *name;
    int hp, painChance, mass;
    float radius, height, speed;
    bool fly, fuzz;
    Seq *stand, *walk, *attack, *pain, *die;
    int atk;
    int pellets, dmgMul, dmgDie;
    int meleeMul, meleeDie;
    const char *missile; int missDmgMul, missDmgDie;
    const char *sight, *act, *painSnd, *deathSnd, *atkSnd, *meleeSnd;
    int drop;
    bool seeAll;
} MonsterDef;
typedef struct {
    const char *name, *spr, *fly, *boom; int flyTics, boomTics;
    float speed, radius, height; int dmgMul, dmgDie;
    bool splash; const char *launchSnd, *hitSnd;
    bool fullbright;
} MissileDef;

void tables_init(void);
const ThingSpec *things_spec(int ed);
int things_spec_count(void);
const ThingSpec *things_spec_at(int i);
MonsterDef *monsters_def(int type);
MissileDef *monsters_missile(const char *name);

/* ------------------------------------------------------------------ world objects */
enum { MK_DECOR = 0, MK_MONSTER, MK_PICKUP, MK_MISSILE, MK_PLAYER, MK_EFFECT, MK_BARREL };
typedef struct Mobj {
    int kind, type;
    float x, y, z, mx, my, mz;
    float ox, oy, oz;
    float angle;
    float radius, height;
    float floorz, ceilz;
    Sector *sector;
    SpriteSet *spr;
    int frame;
    bool fullbright, fuzz;
    bool solid, shootable, noGravity, corpse, removed, countKill, countItem, floatMonster, ambush;
    bool onFloor;
    int health;
    MonsterDef *def;
    int tics, seq, step;
    struct Mobj *target, *owner;
    int reaction, moveCount, moveDir, threshold, painTics, attackTics;
    bool seesTarget;
    int pickupKind, damage;
    const char *missileName;
    MissileDef *miss;
    bool charging, justAttacked, justHit, exploding;
    int chargeTics;
    const ThingSpec *spec;
    int animTics, life;
} Mobj;
Mobj *mobj_new(float x, float y, float z);
DEFVEC(MobjVec, Mobj *);

/* ------------------------------------------------------------------ renderer */
typedef struct { Seg *seg; int x1, x2; float sx1, sx2, iz1, iz2, uz1, uz2, key; int lightnum; Sector *fs, *bs; } Masked;
typedef struct { float z; Mobj *m; Masked *s; int order; } Drawable;
DEFVEC(MaskedVec, Masked);
DEFVEC(DrawVec, Drawable);

typedef struct Renderer {
    int W, H;
    u32 *fb;
    float *depth, *fdepth, *colZ;
    float cx, cy, focal, focalY, zoom;
    Gfx *gfx;
    Level *L;
    Pic *sky;
    float vx, vy, vz, ang, cosA, sinA;
    int *top, *bot; bool *closed; int closedCount;
    float *dirX, *dirY, *skyU, *absDy, *invDy;
    MaskedVec masked; DrawVec drawables;
    int frameCount;
    float alpha;
    int fixedLevel;
} Renderer;

extern int R_filter, R_boost;
extern bool R_weaponFuzz;
Renderer *rend_new(Gfx *g, int w, int h);
void rend_free(Renderer *r);
void rend_setLevel(Renderer *r, Level *l, Pic *sky);
void rend_setZoom(Renderer *r, float z);
void rend_render(Renderer *r, float px, float py, float eyeZ, float angle, MobjVec *mobjs, Mobj *self,
                 SpriteSet *wpn, int wpnFrame, SpriteSet *flash, int flashFrame, float wsx, float wsy,
                 int playerLight, int fixedMap);

/* ------------------------------------------------------------------ physics / ai / specials */
typedef struct {
    float tmFloor, tmCeil, tmDrop;
    Line *tmBlockLine; Mobj *tmBlockMobj;
    Line *tmFloorLine, *tmCeilLine;
    float aimSlope, aimAngle;
} PhysState;
extern PhysState PH;
typedef struct { Mobj *mobj; float x, y, z, dist; Line *line; bool hitSky; } Hit;

float phys_side(const Line *l, float x, float y);
bool phys_checkPosition(Mobj *m, float x, float y, bool ignoreThings);
bool phys_tryMove(Mobj *m, float nx, float ny);
bool phys_slideMove(Mobj *m, float dx, float dy);
bool phys_los(float x1, float y1, float z1, float x2, float y2, float z2);
bool phys_canSee(Mobj *a, Mobj *b);
Hit phys_trace(Mobj *shooter, float x0, float y0, float z0, float angle, float range, float slope);
Mobj *phys_aim(Mobj *shooter, float angle, float range, bool assist);
float phys_normAngle(float a);

void ai_setSeq(Mobj *m, int s, int step);
void ai_think(Mobj *m);
void ai_alert(Mobj *m, Mobj *who);
void ai_hurt(Mobj *t, Mobj *inflictor, Mobj *source, int amount);

enum { TR_WALK = 0, TR_SWITCH = 1, TR_GUN = 2, TR_DOOR = 3 };
enum { A_DOOR_OWC = 1, A_DOOR_STAY = 2, A_DOOR_CLOSE = 3, A_LIFT = 4, A_FLOOR = 5, A_CEIL = 6, A_EXIT = 7, A_SECRET = 8,
       A_TELE = 9, A_LIGHT = 10, A_STAIRS = 11, A_SCROLL = 12, A_DOOR_CLOSE30 = 13 };
enum { F_RAISE_CEIL = 1, F_RAISE_NEXT = 2, F_LOWER_LOWEST = 3, F_LOWER_HIGHEST = 4, F_LOWER_8 = 5, F_RAISE_24 = 6, F_RAISE_TEX = 7, F_RAISE_32 = 8 };
enum { C_RAISE_HIGH = 1, C_LOWER_FLOOR = 2, C_LOWER_8 = 3 };

typedef struct { Sector *s; int type, min, max, count, dark, bright, dir; } LightFx;
typedef struct { Line *line; int tics; } SwitchBack;
typedef struct { Sector *s; bool floor; int grp, idx; } FlatUser;
typedef struct { Side *sd; int which, grp, idx; } TexUser;
DEFVEC(MoverVec, Mover *);
DEFVEC(LightVec, LightFx);
DEFVEC(SwitchVec, SwitchBack);
DEFVEC(FlatUserVec, FlatUser);
DEFVEC(TexUserVec, TexUser);
DEFVEC(LineVec, Line *);
DEFVEC(ThingVec, Thing *);
typedef struct Specials {
    MoverVec movers; LineVec scrollers; SwitchVec switchBack; LightVec lights;
    FlatUserVec flatUsers; TexUserVec texUsers;
    Line *curLine;
} Specials;

const int *specials_lookup(int special);   /* {trigger, repeat, action, param, key, fast} or NULL */
void specials_init(void);
void specials_tick(void);
bool specials_startDoor(Sector *s, int action, bool fast);
bool specials_startFloor(Sector *s, int type, bool fast);
bool specials_useLine(Line *l, Mobj *who);
void specials_shootLine(Line *l, Mobj *who);
void specials_checkCross(Mobj *m, float ox, float oy);
Line *specials_findUseLine(float x, float y, float c, float s, float range);
void specials_soundAlert(Mobj *who);
void specials_free(void);

/* ------------------------------------------------------------------ game */
enum { ST_PLAY = 0, ST_DEAD = 1, ST_EXIT = 2 };
enum { W_FIST = 0, W_PISTOL, W_SHOTGUN, W_CHAINGUN, W_ROCKET, W_PLASMA, W_BFG, W_SAW, W_SSG };
extern const char *const W_NAME[9];

typedef struct Game {
    Wad *wad; Gfx *gfx; Renderer *rend; Level *L;
    char mapName[9];
    int skill; bool aimAssist;
    MobjVec mobjs, spawnQueue;
    Mobj *pl;
    Specials sp;
    int tic, state;
    float angle, turnAccum;
    float inMove, inStrafe; bool inFire, inUse, useHeld;
    int health, armor, armorType;
    int ammo[4], maxAmmo[4];
    bool owned[9], keys[3];
    int weapon, pending;
    int invuln, invis, suit, light;
    bool god, noclip, infAmmo, noTarget;
    float speedMul, takenMul, dealtMul;
    bool berserk, hasMap, backpack;
    int kills, items, secrets, totalKills, totalItems, totalSecrets, levelTics;
    int damageFlash, bonusFlash;
    int hurtTic, hurtAmount, grinTic;
    bool hurtFrom; float hurtAngle;
    char message[128]; int msgTics;
    char nextMap[9];           /* empty = no next map */
    bool secretExit;
    int wState, wStep, wTics, refire;
    float psy, psx;
    int wFrameOverride;
    float bob, eyeZ, eyeZPrev;
    int viewHeightDead;
    ThingVec teleDests;
    float lastPx, lastPy;
} Game;
extern Game *GM;

Game *game_new(Wad *wad, Gfx *gfx, Renderer *rend);
int game_rnd(void);
int game_rnd_diff(void);                   /* rnd() - rnd() */
void game_setRenderer(Renderer *r);
bool game_startMap(const char *map, bool keepInventory, char *err, int errsz);
void game_msg(const char *s);
void game_snd(const char *name, Mobj *at);
void game_sndAt(const char *name, float x, float y);
void game_addTurn(float rad);
void game_applyTurn(void);
void game_selectWeapon(int w);
void game_cycleWeapon(int dir);
void game_tick(void);
void game_render(float alpha);
int game_currentAmmo(void);
void game_exitLevel(bool secret);
void game_refreshFloor(Mobj *m);
void game_gravity(Mobj *m);
void game_applyMomentum(Mobj *m);
void game_shoot(Mobj *src, float a, float range, float slope, int dmg);
Mobj *game_spawnThing(const Thing *t);
Mobj *game_spawnEffect(const char *spr, const char *frames, int tics, float x, float y, float z, bool bright);
Mobj *game_spawnMissile(Mobj *src, MissileDef *md, float a, float slope, float zoff);
void game_radiusDamage(Mobj *center, Mobj *source, int dmg, float radius);
void game_damage(Mobj *t, Mobj *inflictor, Mobj *source, int amount);
void game_noiseAlert(void);
void game_cheatWeapons(void);
void game_cheatKeys(void);
void game_cheatHealth(void);
void game_cheatKillAll(void);
void game_removeMobj(Mobj *m);             /* remove from the live list (not freed) */
int game_mapNumber(const char *map);       /* Integer.parseInt(map.substring(3)) */

/* ------------------------------------------------------------------ 2D canvas */
typedef struct { u32 *px; int w, h; int cx0, cy0, cx1, cy1; } Canvas;
extern Canvas CV;
enum { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };
void cv_init(int w, int h);
void cv_clear(u32 argb);
void cv_setClip(int x0, int y0, int x1, int y1);
void cv_resetClip(void);
void cv_rect(float x0, float y0, float x1, float y1, u32 argb);
void cv_circleFill(float cx, float cy, float r, u32 argb);
void cv_circleStroke(float cx, float cy, float r, float w, u32 argb);
void cv_line(float x0, float y0, float x1, float y1, float w, u32 argb);
void cv_rrectFill(float x0, float y0, float x1, float y1, float rad, u32 argb);
void cv_rrectStroke(float x0, float y0, float x1, float y1, float rad, float w, u32 argb);
void cv_triFill(float x0, float y0, float x1, float y1, float x2, float y2, u32 argb);
void cv_blitView(const u32 *src, int sw, int sh, int dx, int dy, int dw, int dh, bool smooth);
void cv_drawPic(const Pic *p, const u32 *lit0, int x0, int y0, int x1, int y1, u32 tint);
void cv_tileFlat(const u8 *flat, const u32 *lit0, float scx, float scy, int ex0, int ey0, int ex1, int ey1);
void cv_fillOutside(int ex0, int ey0, int ex1, int ey1, u32 argb);
void cv_text(const char *s, float x, float y, float size, u32 argb, int align);       /* Doom font, y = baseline */
void cv_miniText(const char *s, float x, float y, float scale, u32 argb);             /* built-in 5x7 font (no WAD needed) */

/* ------------------------------------------------------------------ HUD */
typedef struct Hud {
    float sx, sy, ox, oy;
    char face[16];
    int lastTic, faceCount, idleCount, idleLook, fireTics, hurtSeen, grinSeen;
} Hud;
extern Hud HUD;
void hud_init(void);
void hud_setT(float sx, float sy, float ox, float oy);
bool hud_has(const char *name);
void hud_patch(const char *name, float x, float y);
void hud_patchK(const char *name, float x, float y, float k);
float hud_textW(const char *s);
void hud_text(const char *s, float x, float y, float k);
void hud_num(const char *pre, int v, float right, float y);
void hud_drawBar(void);
void hud_drawMinimal(float sw, float sh, float s, float margin);

/* ------------------------------------------------------------------ prefs / platform */
void prefs_load(void);
int prefs_getInt(const char *key, int def);
float prefs_getFloat(const char *key, float def);
bool prefs_getBool(const char *key, bool def);
void prefs_setInt(const char *key, int v);
void prefs_setFloat(const char *key, float v);
void prefs_setBool(const char *key, bool v);
void prefs_flush(void);

const char *plat_data_dir(void);              /* writable directory for saves / prefs / temp files */
void plat_haptic(void);
void plat_quit(void);
int64_t plat_nanotime(void);
void plat_audio_start(void);
void plat_audio_stop(void);
/* music backend (MediaPlayer on Android): handle based */
void *mp_open(const char *path, bool loop);
void mp_setVolume(void *h, float v);
bool mp_isPlaying(void *h);
void mp_start(void *h);
void mp_pause(void *h);
void mp_release(void *h);

/* ------------------------------------------------------------------ sound + music */
void snd_init(Wad *w);
void snd_play(const char *lump, float x, float y);
void snd_listener(float x, float y, float a);
void snd_mix(short *out, int frames);          /* stereo interleaved */
void snd_clear(void);
void snd_setRunning(bool running);
extern float SND_volume, SND_lx, SND_ly;
#define SND_RATE 22050

bool mus_to_midi(const u8 *mus, int len, u8 **out, int *outLen);
void music_init(Wad *w);
void music_request(const char *lump, bool loop);
void music_setVolume(float v);
void music_pause(void);
void music_resume(void);

/* ------------------------------------------------------------------ save games */
#define SAVE_SLOTS 6
bool save_describe(int slot, char *out, int outsz);
bool save_save(int slot);
bool save_load(int slot);

/* ------------------------------------------------------------------ menu */
enum { S_MAIN = 0, S_EPI, S_SKILL, S_OPT, S_SND, S_JV, S_HELP, S_QUIT, S_END, S_JVV, S_JVC, S_JVG, S_JVX, S_JVP, S_LOAD, S_SAVE };
enum { TA_DOWN = 0, TA_UP = 1, TA_MOVE = 2, TA_CANCEL = 3, TA_POINTER_DOWN = 5, TA_POINTER_UP = 6 };
void menu_init(void);
void menu_open(void);
void menu_back(void);
void menu_build(void);
void menu_draw(int sw, int sh);
bool menu_touch(int action, float x, float y);

/* ------------------------------------------------------------------ intermission */
typedef struct Intermission {
    bool active;
    int64_t t0;
    int ticsDone, bcnt, state, spState, cntPause, cnt;
    int cntKills, cntItems, cntSecret, cntTime, cntPar;
    int pctKills, pctItems, pctSecret, secs, par;
    int epsd, last, next;
    bool doom2, pointerOn, didSecret, hasNext;
    char lastName[9], nextName[9];
    int aCtr[10], aNext[10];
    float sx, sy, ox;
} Intermission;
extern Intermission WI;
void wi_press(void);
void wi_draw(int sw, int sh);

/* ------------------------------------------------------------------ game view (loop, input, overlays) */
typedef struct GameView {
    double ticNs;
    bool menuOpen, titleMode;
    bool mapOpen;
    int resIndex; bool smooth; float sens;
    bool showFps, showStats, showTime, haptic;
    int aspectIdx;
    int speedIdx, takenIdx, dealtIdx, tempoIdx, fovIdx, filterIdx;
    int bright, crosshair;
    int lastHealth;
    bool messagesOn, lowDetail;
    int screenBlocks;
    int sfxVol, musVol;
    char musKey[40];
    int hudSize, ctlSize, ctlAlpha;
    int vX, vY, vW, vH, barTop;
    float barS, barX0;
    int sw, sh;
    float d;                       /* density */
    Renderer *rend;
    bool rebuildRenderer;
    int64_t fpsTime; int fpsCount, fps;
    int stickPtr, lookPtr, firePtr, usePtr;
    float stickCx, stickCy, stickX, stickY, lastLookX, lastFireX;
    float bFireX, bFireY, bFireR, bUseX, bUseY, bUseR, bNextX, bNextY, bPrevX, bPrevY, bSmallR, bMapX, bMapY, bMenuX, bMenuY;
    bool paused;
    u32 *flatBorder;               /* FLOOR7_2 converted to ARGB, or NULL */
} GameView;
extern GameView GV;
extern const float GV_ASPECT_VAL[7];
extern const int GV_BOOST[4];
extern const char *const GV_ASPECT_NAMES[7];

void gv_init(float density);
void gv_surfaceChanged(int w, int h);
void gv_frame(void);                         /* one iteration of the main loop (no sleeping) */
void gv_onPause(void);
void gv_onResume(void);
bool gv_onBack(void);
void gv_touch(int action, int actionIndex, int pointerCount, const int *ids, const float *xs, const float *ys);
void gv_savePrefs(void);
void gv_resetPrefs(void);
void gv_relayout(void);
void gv_layoutButtons(void);
void gv_beep(const char *lump);
void gv_closeMenu(void);
bool gv_canSave(void);
void gv_openMenu(void);
void gv_startNewGame(int episode, int skill);
void gv_endGame(void);
void gv_warpPrev(void);
void gv_warpNext(void);
void gv_restartLevel(void);
void gv_advanceLevel(void);
void gv_quitApp(void);
void gv_releaseAll(void);
void gv_draw(void);                          /* composes the whole screen into CV */
void gv_setWad(Wad *w, Gfx *g);

extern Wad *G_wad;
extern Gfx *G_gfx;

#endif
