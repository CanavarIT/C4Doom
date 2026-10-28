/*
 * Software renderer: BSP front-to-back walls with per-column floor/ceiling spans, per-pixel depth buffer for
 * sprites and masked textures. Renders into an ARGB buffer.
 */
#include "doom.h"

int R_filter = 0;
int R_boost = 0;
bool R_weaponFuzz = false;

#define NEAR_Z 1.0f
#define VSTRETCH 1.2f

void rend_setZoom(Renderer *r, float z) { r->zoom = z; r->focal = 0.8f * r->H * z; r->focalY = r->focal * VSTRETCH; }

Renderer *rend_new(Gfx *g, int w, int h) {
    Renderer *r = (Renderer *) calloc(1, sizeof(Renderer));
    r->gfx = g; r->W = w; r->H = h; r->zoom = 1.0f;
    r->fb = (u32 *) calloc((size_t) w * h, sizeof(u32));
    r->depth = (float *) malloc((size_t) w * h * sizeof(float));
    r->fdepth = (float *) malloc((size_t) w * h * sizeof(float));
    r->colZ = (float *) malloc((size_t) w * sizeof(float));
    r->cx = w / 2.0f; r->cy = h / 2.0f;
    r->focal = 0.8f * h;
    r->focalY = r->focal * VSTRETCH;
    r->top = (int *) calloc((size_t) w, sizeof(int)); r->bot = (int *) calloc((size_t) w, sizeof(int));
    r->closed = (bool *) calloc((size_t) w, sizeof(bool));
    r->dirX = (float *) calloc((size_t) w, sizeof(float)); r->dirY = (float *) calloc((size_t) w, sizeof(float));
    r->skyU = (float *) calloc((size_t) w, sizeof(float));
    r->absDy = (float *) calloc((size_t) h, sizeof(float)); r->invDy = (float *) calloc((size_t) h, sizeof(float));
    for (int y = 0; y < h; y++) {
        float d = absf(y + 0.5f - r->cy);
        if (d < 0.01f) d = 0.01f;
        r->absDy[y] = d; r->invDy[y] = 1.0f / d;
    }
    r->fixedLevel = -1;
    r->alpha = 1.0f;
    return r;
}

void rend_free(Renderer *r) {
    if (!r) return;
    free(r->fb); free(r->depth); free(r->fdepth); free(r->colZ); free(r->top); free(r->bot); free(r->closed);
    free(r->dirX); free(r->dirY); free(r->skyU); free(r->absDy); free(r->invDy);
    vec_free(r->masked); vec_free(r->drawables);
    free(r);
}

void rend_setLevel(Renderer *r, Level *l, Pic *skyPic) { r->L = l; r->sky = skyPic; }

static int shadeLevel(int lightnum, float z) {
    if (lightnum < 0) lightnum = 0; else if (lightnum > 15) lightnum = 15;
    int j = (int) (2560.0f / z);
    if (j > 47) j = 47;
    int lvl = (15 - lightnum) * 4 - (j >> 1) - R_boost;
    return lvl < 0 ? 0 : (lvl > 31 ? 31 : lvl);
}

/* Original R_DrawPSprite: weapon lit with the brightest scalelight slot (j = 47). */
static int weaponShade(int lightnum) {
    if (lightnum < 0) lightnum = 0; else if (lightnum > 15) lightnum = 15;
    int lvl = (15 - lightnum) * 4 - 23;
    return lvl < 0 ? 0 : (lvl > 31 ? 31 : lvl);
}

static float ix(Renderer *r, Mobj *m) { return m->ox + (m->x - m->ox) * r->alpha; }
static float iy(Renderer *r, Mobj *m) { return m->oy + (m->y - m->oy) * r->alpha; }
static float izz(Renderer *r, Mobj *m) { return m->oz + (m->z - m->oz) * r->alpha; }

void rend_addSeg(Renderer *r, Seg *s);
static void bsp(Renderer *r, int node);
static void drawSpritesAndMasked(Renderer *r, MobjVec *mobjs, Mobj *self);
static void drawWeapon(Renderer *r, SpriteSet *wpn, int wf, SpriteSet *flash, int ff, float psx, float psy, int light);

/* =================================================================== frame */
void rend_render(Renderer *r, float px, float py, float eyeZ, float angle, MobjVec *mobjs, Mobj *self,
                 SpriteSet *wpn, int wpnFrame, SpriteSet *flash, int flashFrame, float wsx, float wsy,
                 int playerLight, int fixedMap) {
    int W = r->W, H = r->H;
    r->vx = px; r->vy = py; r->vz = eyeZ; r->ang = angle;
    r->cosA = (float) cos((double) angle); r->sinA = (float) sin((double) angle);
    for (int i = 0; i < W * H; i++) r->fb[i] = 0xFF000000u;
    for (int i = 0; i < W * H; i++) { r->depth[i] = 3.402823466e+38f; r->fdepth[i] = 3.402823466e+38f; }
    for (int x = 0; x < W; x++) { r->colZ[x] = 3.402823466e+38f; r->top[x] = 0; r->bot[x] = H - 1; r->closed[x] = false; }
    r->closedCount = 0;
    vec_clear(r->masked); vec_clear(r->drawables);
    for (int x = 0; x < W; x++) {
        float xn = (x + 0.5f - r->cx) / r->focal;
        r->dirX[x] = r->cosA + r->sinA * xn;
        r->dirY[x] = r->sinA - r->cosA * xn;
        r->skyU[x] = (float) (((double) angle - atan((double) xn)) / (DOOM_PI / 2));   /* fraction of sky texture width (covers 90 deg) */
    }
    r->fixedLevel = fixedMap;
    Level *L = r->L;
    if (L && L->nss > 0) {
        if (L->nn == 0) {
            /* subsector(0) */
            int n = L->ssNum[0], f = L->ssFirst[0];
            for (int i = 0; i < n && f + i < L->nsegs; i++) rend_addSeg(r, &L->segs[f + i]);
        } else bsp(r, L->nn - 1);
    }
    drawSpritesAndMasked(r, mobjs, self);
    drawWeapon(r, wpn, wpnFrame, flash, flashFrame, wsx, wsy, playerLight);
    r->frameCount++;
}

/* =================================================================== BSP */
static bool checkBox(Renderer *r, int node, int side) {
    const float *b = r->L->nbb[node];
    int o = side * 4;
    float bt = b[o], bb = b[o + 1], bl = b[o + 2], br = b[o + 3];
    if (r->vx >= bl && r->vx <= br && r->vy >= bb && r->vy <= bt) return true;
    float minSx = 1e9f, maxSx = -1e9f;
    int behind = 0;
    for (int i = 0; i < 4; i++) {
        float x = (i & 1) == 0 ? bl : br, y = (i & 2) == 0 ? bb : bt;
        float dx = x - r->vx, dy = y - r->vy;
        float z = dx * r->cosA + dy * r->sinA;
        if (z < NEAR_Z) { behind++; continue; }
        float xr = dx * r->sinA - dy * r->cosA;
        float sx = r->cx + xr * r->focal / z;
        if (sx < minSx) minSx = sx;
        if (sx > maxSx) maxSx = sx;
    }
    if (behind == 4) return false;
    if (behind > 0) return true;
    int x0 = maxi(0, (int) floor((double) minSx)), x1 = mini(r->W - 1, (int) ceil((double) maxSx));
    if (x0 > x1) return false;
    for (int x = x0; x <= x1; x++) if (!r->closed[x]) return true;
    return false;
}

static void subsector(Renderer *r, int ss) {
    Level *L = r->L;
    if (ss >= L->nss) return;
    int n = L->ssNum[ss], f = L->ssFirst[ss];
    for (int i = 0; i < n && f + i < L->nsegs; i++) rend_addSeg(r, &L->segs[f + i]);
}

static void bsp(Renderer *r, int node) {
    if (r->closedCount >= r->W) return;
    if ((node & 0x8000) != 0) { subsector(r, node & 0x7FFF); return; }
    int side = level_pointOnSide(r->L, r->vx, r->vy, node);
    bsp(r, r->L->nchild[node * 2 + side]);
    if (r->closedCount < r->W && checkBox(r, node, side ^ 1)) bsp(r, r->L->nchild[node * 2 + (side ^ 1)]);
}

/* =================================================================== column drawers */
/* Draw wall texture rows y0..y1; texMidRel = world offset such that texRow = texMidRel + (y+.5-cy)*z/focalY. */
static void drawWallColumn(Renderer *r, int x, int y0, int y1, const Pic *tex, float u, float texMidRel, float z, const u32 *lt) {
    int W = r->W;
    int w = tex->w, h = tex->h;
    int c = (int) floor((double) u);
    c %= w; if (c < 0) c += w;
    int base = c * h;
    float stepY = z / r->focalY;
    float rf = texMidRel + (y0 + 0.5f - r->cy) * stepY;
    rf = (float) fmod((double) rf, (double) h); if (rf < 0) rf += h;
    int r16 = (int) (rf * 65536.0f);
    int st = (int) (stepY * 65536.0f);
    const u8 *pix = tex->pix;
    int idx = y0 * W + x;
    for (int y = y0; y <= y1; y++) {
        int rr = r16 >> 16;
        if (rr >= h) rr %= h;
        r->fb[idx] = lt[pix[base + rr]];
        r->depth[idx] = z;
        idx += W;
        r16 += st;
    }
}

/* Never leave a hole: a plane whose flat is missing is painted dark grey. */
static void fillPlain(Renderer *r, int x, int y0, int y1, u32 color) {
    int idx = y0 * r->W + x;
    for (int y = y0; y <= y1; y++) { r->fb[idx] = color; idx += r->W; }
}

static void fillColumn(Renderer *r, int x, int y0, int y1, float z, u32 color) {
    int idx = y0 * r->W + x;
    for (int y = y0; y <= y1; y++) { r->fb[idx] = color; r->depth[idx] = z; idx += r->W; }
}

static void drawFlatColumn(Renderer *r, int x, int y0, int y1, const u8 *flat, float hd, int lightnum) {
    int W = r->W;
    float hdF = hd * r->focalY;
    float k = 2560.0f / hdF;
    float dx = r->dirX[x], dy = r->dirY[x];
    int idx = y0 * W + x;
    int start = (15 - maxi(0, mini(15, lightnum))) * 4;
    for (int y = y0; y <= y1; y++) {
        float z = hdF * r->invDy[y];
        float wx = r->vx + z * dx, wy = r->vy + z * dy;
        int tx = ((int) (wx + 65536.0f)) & 63;
        int ty = ((int) (65536.0f - wy)) & 63;
        int lvl;
        if (r->fixedLevel >= 0) lvl = r->fixedLevel;
        else {
            int j = (int) (k * r->absDy[y]);
            if (j > 47) j = 47;
            lvl = start - (j >> 1) - R_boost;
            if (lvl < 0) lvl = 0; else if (lvl > 31) lvl = 31;
        }
        r->fb[idx] = r->gfx->lit[lvl][flat[(ty << 6) | tx]];
        r->fdepth[idx] = z;
        idx += W;
    }
}

static void drawSkyColumn(Renderer *r, int x, int y0, int y1) {
    int W = r->W;
    Pic *sky = r->sky;
    if (!sky) {
        int idx = y0 * W + x;
        for (int y = y0; y <= y1; y++) { r->fb[idx] = 0xFF2040A0u; idx += W; }
        return;
    }
    int w = sky->w, h = sky->h;
    float uf = r->skyU[x] * w;
    int c = (int) floor((double) uf);
    c %= w; if (c < 0) c += w;
    int base = c * h;
    float rowPerPx = (w / (float) (DOOM_PI / 2)) / r->focal / VSTRETCH;
    const u32 *lt = r->gfx->lit[r->fixedLevel >= 0 ? r->fixedLevel : 0];
    int idx = y0 * W + x;
    for (int y = y0; y <= y1; y++) {
        int rr = (int) (100.0f + (y + 0.5f - r->cy) * rowPerPx);
        if (rr < 0) rr = 0; else if (rr >= h) rr = h - 1;
        r->fb[idx] = lt[sky->pix[base + rr]];
        idx += W;
    }
}

/* =================================================================== walls */
void rend_addSeg(Renderer *r, Seg *s) {
    int W = r->W;
    if (r->closedCount >= W) return;
    float cross = (s->x2 - s->x1) * (r->vy - s->y1) - (s->y2 - s->y1) * (r->vx - s->x1);
    if (cross >= 0) return;                       /* back-facing */
    s->line->mapped = true;

    float ax = s->x1 - r->vx, ay = s->y1 - r->vy, bx = s->x2 - r->vx, by = s->y2 - r->vy;
    float za = ax * r->cosA + ay * r->sinA, xa = ax * r->sinA - ay * r->cosA;
    float zb = bx * r->cosA + by * r->sinA, xb = bx * r->sinA - by * r->cosA;
    if (za < NEAR_Z && zb < NEAR_Z) return;
    float u1 = 0, u2 = s->len;
    if (za < NEAR_Z) {
        float t = (NEAR_Z - za) / (zb - za);
        xa += t * (xb - xa); u1 = t * s->len; za = NEAR_Z;
    } else if (zb < NEAR_Z) {
        float t = (NEAR_Z - za) / (zb - za);
        xb = xa + t * (xb - xa); u2 = t * s->len; zb = NEAR_Z;
    }
    float sx1 = r->cx + xa * r->focal / za, sx2 = r->cx + xb * r->focal / zb;
    if (sx1 >= sx2) return;
    int x1 = maxi(0, (int) ceil((double) (sx1 - 0.5f)));
    int x2 = mini(W - 1, (int) ceil((double) (sx2 - 0.5f)) - 1);
    if (x1 > x2) return;
    /* quick reject if all columns closed */
    bool any = false;
    for (int x = x1; x <= x2; x++) if (!r->closed[x]) { any = true; break; }
    if (!any) return;

    Sector *fs = s->front, *bs = s->back;
    Side *side = s->side;
    float uOff = s->offset + side->xoff;
    float iz1 = 1.0f / za, iz2 = 1.0f / zb;
    float uz1 = (u1 + uOff) * iz1, uz2 = (u2 + uOff) * iz2;

    int lightnum = fs->light >> 4;
    if (s->y1 == s->y2) lightnum--; else if (s->x1 == s->x2) lightnum++;

    float worldTop = fs->ceil, worldBot = fs->floor;
    bool solid = bs == NULL;
    float backTop = 0, backBot = 0;
    bool drawUpper = false, drawLower = false, isClosed = false;
    Pic *midTex = NULL, *upTex = NULL, *loTex = NULL;
    float vtopMid = 0, vtopUp = 0, vtopLo = 0;
    bool skyBoth = false;
    if (solid) {
        midTex = side->mid;
        if ((s->line->flags & ML_DONTPEGBOTTOM) != 0)
            vtopMid = fs->floor + (midTex ? midTex->h : 0);
        else vtopMid = fs->ceil;
    } else {
        backTop = bs->ceil; backBot = bs->floor;
        if (fs->skyCeil && bs->skyCeil) { worldTop = backTop; skyBoth = true; }
        isClosed = bs->ceil <= fs->floor || bs->floor >= fs->ceil;
        drawUpper = backTop < worldTop && !skyBoth;
        drawLower = backBot > worldBot;
        if (drawUpper) {
            upTex = side->upper;
            vtopUp = (s->line->flags & ML_DONTPEGTOP) != 0 ? worldTop : backTop + (upTex ? upTex->h : 0);
        }
        if (drawLower) {
            loTex = side->lower;
            vtopLo = (s->line->flags & ML_DONTPEGBOTTOM) != 0 ? worldTop : backBot;
        }
        if (side->mid) {
            Masked m;
            m.seg = s; m.x1 = x1; m.x2 = x2; m.sx1 = sx1; m.sx2 = sx2; m.iz1 = iz1; m.iz2 = iz2;
            m.uz1 = uz1; m.uz2 = uz2; m.lightnum = lightnum; m.fs = fs; m.bs = bs;
            m.key = 2.0f / (iz1 + iz2);
            vec_push(r->masked, m);
        }
    }

    bool drawCeil = fs->ceil > r->vz || fs->skyCeil;
    bool drawFloor = fs->floor < r->vz;
    float ceilHd = fs->ceil - r->vz, floorHd = r->vz - fs->floor;
    float iscale = 1.0f / (sx2 - sx1);
    bool ceilSky = fs->skyCeil;
    const u8 *ceilPic = fs->ceilPic, *floorPic = fs->floorPic;
    int planeLight = fs->light >> 4;

    for (int x = x1; x <= x2; x++) {
        if (r->closed[x]) continue;
        float xc = x + 0.5f;
        float t = (xc - sx1) * iscale;
        float iz = iz1 + (iz2 - iz1) * t;
        float z = 1.0f / iz;
        float scaleY = r->focalY * iz;
        float ytF = r->cy - (worldTop - r->vz) * scaleY;
        float ybF = r->cy - (worldBot - r->vz) * scaleY;
        int yt = (int) ceil((double) (ytF - 0.5f));                 /* first wall row */
        int yb = (int) ceil((double) (ybF - 0.5f)) - 1;             /* last wall row */
        int tp = r->top[x], bt = r->bot[x];

        /* ceiling */
        if (drawCeil) {
            int ce = mini(yt - 1, bt);
            if (ce >= tp) {
                if (ceilSky) drawSkyColumn(r, x, tp, ce);
                else if (ceilPic && ceilHd > 0.01f) drawFlatColumn(r, x, tp, ce, ceilPic, ceilHd, planeLight);
                else if (!ceilPic) fillPlain(r, x, tp, ce, 0xFF202020u);
            }
        }
        /* floor */
        if (drawFloor) {
            int fst = maxi(yb + 1, tp);
            if (fst <= bt && floorPic && floorHd > 0.01f) drawFlatColumn(r, x, fst, bt, floorPic, floorHd, planeLight);
            else if (fst <= bt && !floorPic) fillPlain(r, x, fst, bt, 0xFF202020u);
        }

        int lvl = r->fixedLevel >= 0 ? r->fixedLevel : shadeLevel(lightnum, z);
        const u32 *lt = r->gfx->lit[lvl];
        float uCol = (uz1 + (uz2 - uz1) * t) * z;

        if (solid) {
            int y0 = maxi(yt, tp), y1 = mini(yb, bt);
            if (y0 <= y1 && midTex)
                drawWallColumn(r, x, y0, y1, midTex, uCol, vtopMid - r->vz + side->yoff, z, lt);
            else if (y0 <= y1) fillColumn(r, x, y0, y1, z, 0xFF000000u);
            r->top[x] = r->H; r->bot[x] = -1;
            r->closed[x] = true; r->closedCount++; r->colZ[x] = z;
        } else {
            int newTop, newBot;
            if (drawUpper) {
                float ytU = r->cy - (backTop - r->vz) * scaleY;
                int mid = (int) ceil((double) (ytU - 0.5f));        /* first row below upper wall */
                int y0 = maxi(yt, tp), y1 = mini(mid - 1, bt);
                if (y0 <= y1) {
                    if (upTex) drawWallColumn(r, x, y0, y1, upTex, uCol, vtopUp - r->vz + side->yoff, z, lt);
                    else fillColumn(r, x, y0, y1, z, 0xFF000000u);
                }
                newTop = maxi(tp, maxi(mid, yt));
            } else {
                newTop = maxi(tp, yt);
            }
            if (drawLower) {
                float ybL = r->cy - (backBot - r->vz) * scaleY;
                int mid = (int) ceil((double) (ybL - 0.5f));        /* first row of lower wall */
                int y0 = maxi(mid, tp), y1 = mini(yb, bt);
                if (y0 <= y1) {
                    if (loTex) drawWallColumn(r, x, y0, y1, loTex, uCol, vtopLo - r->vz + side->yoff, z, lt);
                    else fillColumn(r, x, y0, y1, z, 0xFF000000u);
                }
                newBot = mini(bt, mini(mid - 1, yb));
            } else {
                newBot = mini(bt, yb);
            }
            if (isClosed) {
                r->top[x] = r->H; r->bot[x] = -1; r->closed[x] = true; r->closedCount++; r->colZ[x] = z;
            } else {
                r->top[x] = newTop; r->bot[x] = newBot;
                if (newTop > newBot) { r->closed[x] = true; r->closedCount++; r->colZ[x] = z; }
            }
        }
    }
}

/* =================================================================== sprites + masked textures */
static void drawSprite(Renderer *r, Mobj *m, float z) {
    int W = r->W, H = r->H;
    SpriteSet *ss = m->spr;
    int f = m->frame;
    if (f < 0 || f >= 29 || !ss->present[f]) return;
    float dx = ix(r, m) - r->vx, dy = iy(r, m) - r->vy;
    float xr = dx * r->sinA - dy * r->cosA;
    int rot = 0;
    if (ss->rotates[f]) {
        double a = atan2((double) dy, (double) dx);
        rot = ((int) floor((a - (double) m->angle) / (DOOM_PI / 4) + 4.5)) & 7;
    }
    Pic *p = ss->pic[f][rot];
    if (!p) return;
    bool flip = ss->flip[f][rot];
    float sc = r->focal / z, scY = r->focalY / z;
    float sx = r->cx + xr * sc;
    float left = sx - (flip ? (p->w - p->lo) : p->lo) * sc;
    float right = left + p->w * sc;
    int x0 = maxi(0, (int) ceil((double) (left - 0.5f))), x1 = mini(W - 1, (int) ceil((double) (right - 0.5f)) - 1);
    if (x0 > x1) return;
    float topS = r->cy - (izz(r, m) + p->to - r->vz) * scY;
    float botS = topS + p->h * scY;
    int y0 = maxi(0, (int) ceil((double) (topS - 0.5f))), y1 = mini(H - 1, (int) ceil((double) (botS - 0.5f)) - 1);
    if (y0 > y1) return;
    int lvl;
    if (r->fixedLevel >= 0) lvl = r->fixedLevel;
    else if (m->fullbright) lvl = 0;
    else {
        int light = m->sector ? m->sector->light : 160;
        lvl = shadeLevel(light >> 4, z);
    }
    const u32 *lt = r->gfx->lit[lvl];
    float invSc = 1.0f / sc, invScY = 1.0f / scY;
    float zt = z - 0.5f;
    bool fuzz = m->fuzz;
    int ph = p->h;
    float feetF = r->cy - (izz(r, m) - r->vz) * scY;               /* screen row of the sprite's ground contact */
    int feetRow = (int) ceil((double) (feetF - 0.5f));
    /* Rows of the sprite that sink below its origin are only drawn over floor that is about as far as the sprite
     * itself. Without this test they poked out below walls and ledges that hide the rest of the sprite. */
    float sinkPx = maxf(0, p->h - p->to) * scY;
    float dyFeet = feetF - r->cy;
    float minRatio = dyFeet > 1.0f ? 0.93f * dyFeet / (dyFeet + sinkPx + 1.0f) : 0.0f;
    for (int x = x0; x <= x1; x++) {
        if (zt >= r->colZ[x]) continue;                       /* behind a wall that closes this column completely */
        int tc = (int) ((x + 0.5f - left) * invSc);
        if (tc < 0) tc = 0; else if (tc >= p->w) tc = p->w - 1;
        if (flip) tc = p->w - 1 - tc;
        int base = tc * ph;
        float v = (y0 + 0.5f - topS) * invScY;
        int idx = y0 * W + x;
        for (int y = y0; y <= y1; y++) {
            int rr = (int) v;
            if (rr >= ph) rr = ph - 1;
            v += invScY;
            if (p->op[base + rr] != 0 && zt < r->depth[idx] && (y >= feetRow ? r->fdepth[idx] >= minRatio * z : zt < r->fdepth[idx])) {
                if (fuzz) {
                    int sidx = idx + (((x + y + r->frameCount) & 1) == 0 ? W : -W);
                    if (sidx < 0 || sidx >= W * H) sidx = idx;
                    u32 c = r->fb[sidx];
                    r->fb[idx] = 0xFF000000u | (((c >> 1) & 0x7F7F7Fu) + ((c >> 3) & 0x1F1F1Fu));
                } else r->fb[idx] = lt[p->pix[base + rr]];
            }
            idx += W;
        }
    }
}

static void drawMasked(Renderer *r, Masked *m) {
    int W = r->W, H = r->H;
    Seg *s = m->seg;
    Pic *tex = s->side->mid;
    if (!tex) return;
    float openTop = minf(m->fs->ceil, m->bs->ceil), openBot = maxf(m->fs->floor, m->bs->floor);
    float vtop = (s->line->flags & ML_DONTPEGBOTTOM) != 0 ? openBot + tex->h : openTop;
    float texTopZ = vtop + s->side->yoff;
    float texBotZ = texTopZ - tex->h;
    float zTop = minf(texTopZ, openTop), zBot = maxf(texBotZ, openBot);
    if (zTop <= zBot) return;
    float iscale = 1.0f / (m->sx2 - m->sx1);
    for (int x = m->x1; x <= m->x2; x++) {
        float xc = x + 0.5f;
        float t = (xc - m->sx1) * iscale;
        float iz = m->iz1 + (m->iz2 - m->iz1) * t;
        float z = 1.0f / iz;
        float u = (m->uz1 + (m->uz2 - m->uz1) * t) * z;
        int c = (int) floor((double) u);
        c %= tex->w; if (c < 0) c += tex->w;
        float scY = r->focalY * iz;
        int y0 = maxi(0, (int) ceil((double) (r->cy - (zTop - r->vz) * scY - 0.5f)));
        int y1 = mini(H - 1, (int) ceil((double) (r->cy - (zBot - r->vz) * scY - 0.5f)) - 1);
        if (y0 > y1) continue;
        if (z - 0.5f >= r->colZ[x]) continue;                /* hidden behind a closing wall */
        int lvl = r->fixedLevel >= 0 ? r->fixedLevel : shadeLevel(m->lightnum, z);
        const u32 *lt = r->gfx->lit[lvl];
        float stepY = z / r->focalY;
        float rowF = (texTopZ - (r->vz - (y0 + 0.5f - r->cy) * stepY));
        int base = c * tex->h;
        int idx = y0 * W + x;
        float zt = z - 0.5f;
        for (int y = y0; y <= y1; y++) {
            int rr = (int) floor((double) rowF);
            rowF += stepY;
            if (rr >= 0 && rr < tex->h && (!tex->op || tex->op[base + rr] != 0) && zt < r->depth[idx] && zt < r->fdepth[idx])
                r->fb[idx] = lt[tex->pix[base + rr]];
            idx += W;
        }
    }
}

static int cmpDrawable(const void *a, const void *b) {
    const Drawable *x = (const Drawable *) a, *y = (const Drawable *) b;
    if (y->z < x->z) return -1;
    if (y->z > x->z) return 1;
    return x->order - y->order;              /* stable, like List.sort */
}

static void drawSpritesAndMasked(Renderer *r, MobjVec *mobjs, Mobj *self) {
    int W = r->W;
    for (int i = 0; i < mobjs->n; i++) {
        Mobj *m = mobjs->d[i];
        if (m == self || m->removed || !m->spr) continue;
        float dx = ix(r, m) - r->vx, dy = iy(r, m) - r->vy;
        float z = dx * r->cosA + dy * r->sinA;
        if (z < 4.0f || z > 8000.0f) continue;
        float xr = dx * r->sinA - dy * r->cosA;
        if (absf(xr) > z * (W / r->focal) + 128.0f) continue;
        Drawable d = {z, m, NULL, r->drawables.n};
        vec_push(r->drawables, d);
    }
    for (int i = 0; i < r->masked.n; i++) {
        Drawable d = {r->masked.d[i].key, NULL, &r->masked.d[i], r->drawables.n};
        vec_push(r->drawables, d);
    }
    if (r->drawables.n > 1) qsort(r->drawables.d, (size_t) r->drawables.n, sizeof(Drawable), cmpDrawable);
    for (int i = 0; i < r->drawables.n; i++) {
        Drawable *d = &r->drawables.d[i];
        if (d->m) drawSprite(r, d->m, d->z); else drawMasked(r, d->s);
    }
}

/* =================================================================== weapon (psprite) */
/* Whole-screen colour grading from the setup menu. */
static void applyFilter(Renderer *r) {
    int mode = R_filter;
    for (int i = 0, n = r->W * r->H; i < n; i++) {
        u32 c = r->fb[i];
        int rr = (c >> 16) & 255, gg = (c >> 8) & 255, bb = c & 255;
        int l = (rr * 77 + gg * 151 + bb * 28) >> 8;
        int or_, og, ob;
        switch (mode) {
            case 1: or_ = og = ob = l; break;
            case 2: or_ = mini(255, l * 9 / 8); og = l * 7 / 8; ob = l * 5 / 8; break;
            case 3: { int g2 = mini(255, l * 3 / 2 + 12); or_ = l / 6; og = g2; ob = l / 5; break; }
            default: or_ = mini(255, l * 5 / 4); og = l / 4; ob = l / 5; break;
        }
        r->fb[i] = 0xFF000000u | ((u32) or_ << 16) | ((u32) og << 8) | (u32) ob;
    }
}

static void drawPsprite(Renderer *r, SpriteSet *ss, int f, float psx, float psy, float s, bool bright, int light) {
    int W = r->W, H = r->H;
    if (f < 0 || f >= 29 || !ss->present[f]) return;
    Pic *p = ss->pic[f][0];
    if (!p) return;
    float sh = s, sv = s * VSTRETCH;
    float left = r->cx + (psx - 160.0f - p->lo) * sh;
    float topS = r->cy - (100.5f - (psy - p->to)) * sv;
    float right = left + p->w * sh, botS = topS + p->h * sv;
    int x0 = maxi(0, (int) ceil((double) (left - 0.5f))), x1 = mini(W - 1, (int) ceil((double) (right - 0.5f)) - 1);
    int y0 = maxi(0, (int) ceil((double) (topS - 0.5f))), y1 = mini(H - 1, (int) ceil((double) (botS - 0.5f)) - 1);
    if (x0 > x1 || y0 > y1) return;
    int lvl = r->fixedLevel >= 0 ? r->fixedLevel : (bright ? 0 : mini(weaponShade(light >> 4), 24));
    const u32 *lt = r->gfx->lit[lvl];
    for (int x = x0; x <= x1; x++) {
        int tc = (int) ((x + 0.5f - left) / sh);
        if (tc < 0) tc = 0; else if (tc >= p->w) tc = p->w - 1;
        int base = tc * p->h;
        int idx = y0 * W + x;
        for (int y = y0; y <= y1; y++) {
            int rr = (int) ((y + 0.5f - topS) / sv);
            if (rr >= p->h) rr = p->h - 1;
            if (p->op[base + rr] != 0) {
                if (R_weaponFuzz) {
                    int sidx = idx + (((x + y + r->frameCount) & 1) == 0 ? W : -W);
                    if (sidx < 0 || sidx >= W * H) sidx = idx;
                    u32 c = r->fb[sidx];
                    r->fb[idx] = 0xFF000000u | (((c >> 1) & 0x7F7F7Fu) + ((c >> 3) & 0x1F1F1Fu));
                } else r->fb[idx] = lt[p->pix[base + rr]];
            }
            idx += W;
        }
    }
}

static void drawWeapon(Renderer *r, SpriteSet *wpn, int wf, SpriteSet *flash, int ff, float psx, float psy, int light) {
    float s = r->H / 240.0f;
    if (wpn) drawPsprite(r, wpn, wf, psx, psy, s, false, light);
    if (flash) drawPsprite(r, flash, ff, psx, psy, s, true, light);
    if (R_filter != 0) applyFilter(r);
}
