/* Palette, colormaps, patches, composite wall textures, flats and sprite frames decoded from a WAD. */
#include "doom.h"

static int nearest(const u8 *pal, int r, int g, int b) {
    int best = 0, bd = 0x7fffffff;
    for (int i = 0; i < 256; i++) {
        int dr = pal[i * 3] - r, dg = pal[i * 3 + 1] - g, db = pal[i * 3 + 2] - b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* ---------------------------------------------------------------- patches */
Pic *gfx_decode_patch(const u8 *d, int len) {
    if (!d || len < 8) return NULL;
    int w = rd_u16(d), h = rd_u16(d + 2);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || len < 8 + w * 4) return NULL;
    Pic *p = (Pic *) calloc(1, sizeof(Pic));
    p->w = w; p->h = h; p->lo = rd_s16(d + 4); p->to = rd_s16(d + 6);
    p->pix = (u8 *) calloc((size_t) w * h, 1);
    p->op = (u8 *) calloc((size_t) w * h, 1);
    bool tall = h > 254;
    for (int x = 0; x < w; x++) {
        int o = rd_i32(d + 8 + x * 4);
        if (o < 0 || o >= len) continue;
        int last = -1;
        while (o < len) {
            int td = d[o];
            if (td == 255) break;
            if (o + 1 >= len) break;
            int n = d[o + 1];
            if (tall && td <= last) td += last;
            last = td;
            int src = o + 3;
            for (int i = 0; i < n; i++) {
                int y = td + i;
                if (y >= 0 && y < h && src + i < len) {
                    p->pix[x * h + y] = d[src + i];
                    p->op[x * h + y] = 1;
                }
            }
            o += n + 4;
        }
    }
    return p;
}

Pic *gfx_patch(Gfx *g, const char *name) {
    bool found;
    Pic *p = (Pic *) strmap_get(&g->patchCache, name, &found);
    if (found) return p;
    int i = wad_find(g->wad, name);
    if (i >= 0) { int sz; const u8 *d = wad_lump(g->wad, i, &sz); p = gfx_decode_patch(d, sz); }
    else p = NULL;
    strmap_put(&g->patchCache, name, p, true);
    return p;
}

/* ---------------------------------------------------------------- flats */
static void load_flats(Gfx *g) {
    Wad *wad = g->wad;
    int s = wad_find(wad, "F_START"), e = wad_find(wad, "F_END");
    if (s < 0) s = wad_find(wad, "FF_START");
    if (e < 0) e = wad_find(wad, "FF_END");
    if (s < 0 || e < 0) return;
    for (int i = s + 1; i < e; i++) {
        const char *n = wad->lumps[i].name;
        size_t L = strlen(n);
        if ((L >= 6 && strcmp(n + L - 6, "_START") == 0) || (L >= 4 && strcmp(n + L - 4, "_END") == 0)) continue;
        int sz = wad->lumps[i].size;
        if (sz >= 4096) strmap_put(&g->flats, n, (void *) wad_lump(wad, i, NULL), true);
        else if (sz == 64 * 64) strmap_put(&g->flats, n, (void *) wad_lump(wad, i, NULL), true);
    }
}

/* ---------------------------------------------------------------- textures */
static void load_textures(Gfx *g) {
    Wad *wad = g->wad;
    int pnlen;
    const u8 *pn = wad_lump_named(wad, "PNAMES", &pnlen);
    if (!pn) return;
    int np = rd_i32(pn);
    if (np < 0) return;
    char (*pnames)[9] = (char (*)[9]) calloc((size_t) (np ? np : 1), 9);
    for (int i = 0; i < np && 4 + i * 8 + 8 <= pnlen; i++) cstr_up(pnames[i], pn + 4 + i * 8, 8);
    static const char *const lists[2] = {"TEXTURE1", "TEXTURE2"};
    for (int li = 0; li < 2; li++) {
        int tl;
        const u8 *t = wad_lump_named(wad, lists[li], &tl);
        if (!t || tl < 4) continue;
        int n = rd_i32(t);
        for (int i = 0; i < n; i++) {
            if (4 + i * 4 + 4 > tl) break;
            int o = rd_i32(t + 4 + i * 4);
            if (o < 0 || o + 22 > tl) continue;
            char name[9];
            cstr_up(name, t + o, 8);
            int w = rd_s16(t + o + 12), h = rd_s16(t + o + 14), pc = rd_s16(t + o + 20);
            if (w <= 0 || h <= 0 || w > 2048 || h > 2048) continue;
            Pic *tx = (Pic *) calloc(1, sizeof(Pic));
            tx->w = w; tx->h = h;
            tx->pix = (u8 *) calloc((size_t) w * h, 1);
            u8 *cov = (u8 *) calloc((size_t) w * h, 1);
            for (int k = 0; k < pc; k++) {
                int po = o + 22 + k * 10;
                if (po + 10 > tl) break;
                int ox = rd_s16(t + po), oy = rd_s16(t + po + 2), pi = rd_s16(t + po + 4);
                if (pi < 0 || pi >= np || pnames[pi][0] == 0) continue;
                Pic *p = gfx_patch(g, pnames[pi]);
                if (!p) continue;
                for (int x = 0; x < p->w; x++) {
                    int tx0 = ox + x;
                    if (tx0 < 0 || tx0 >= w) continue;
                    for (int y = 0; y < p->h; y++) {
                        int ty = oy + y;
                        if (ty < 0 || ty >= h) continue;
                        if (p->op[x * p->h + y]) {
                            tx->pix[tx0 * h + ty] = p->pix[x * p->h + y];
                            cov[tx0 * h + ty] = 1;
                        }
                    }
                }
            }
            bool holes = false;
            for (int k = 0; k < w * h; k++) if (cov[k] == 0) { holes = true; break; }
            if (holes) tx->op = cov; else free(cov);
            if (!strmap_put(&g->textures, name, tx, false)) {     /* like vanilla: the first texture with a given name wins */
                free(tx->pix); free(tx->op); free(tx);
            }
        }
    }
    free(pnames);
}

Pic *gfx_texture(Gfx *g, const char *name) {
    if (!name || !name[0] || name[0] == '-') return NULL;
    return (Pic *) strmap_get(&g->textures, name, NULL);
}

const u8 *gfx_flat(Gfx *g, const char *name) { return (const u8 *) strmap_get(&g->flats, name, NULL); }
SpriteSet *gfx_sprite(Gfx *g, const char *prefix) { return (SpriteSet *) strmap_get(&g->sprites, prefix, NULL); }

/* ---------------------------------------------------------------- sprites */
static void load_sprites(Gfx *g) {
    Wad *wad = g->wad;
    int s = wad_find(wad, "S_START"), e = wad_find(wad, "S_END");
    if (s < 0) s = wad_find(wad, "SS_START");
    if (e < 0) e = wad_find(wad, "SS_END");
    if (s < 0 || e < 0) return;
    for (int i = s + 1; i < e; i++) {
        const char *n = wad->lumps[i].name;
        int L = (int) strlen(n);
        if (L < 6 || wad->lumps[i].size < 8) continue;
        char pre[9];
        memcpy(pre, n, 4); pre[4] = 0;
        Pic *pic = NULL;
        SpriteSet *ss = gfx_sprite(g, pre);
        if (!ss) { ss = (SpriteSet *) calloc(1, sizeof(SpriteSet)); ss->maxFrame = -1; strmap_put(&g->sprites, pre, ss, true); }
        int f1 = n[4] - 'A', r1 = n[5] - '0';
        for (int pass = 0; pass < 2; pass++) {
            int f = pass == 0 ? f1 : (L >= 8 ? n[6] - 'A' : -1);
            int r = pass == 0 ? r1 : (L >= 8 ? n[7] - '0' : -1);
            if (f < 0 || f >= 29 || r < 0 || r > 8) continue;
            if (!pic) { int sz; const u8 *d = wad_lump(wad, i, &sz); pic = gfx_decode_patch(d, sz); }
            if (!pic) continue;
            ss->present[f] = true;
            if (f > ss->maxFrame) ss->maxFrame = f;
            if (r == 0) {
                for (int k = 0; k < 8; k++) { ss->pic[f][k] = pic; ss->flip[f][k] = pass == 1; }
                ss->rotates[f] = false;
            } else {
                ss->pic[f][r - 1] = pic; ss->flip[f][r - 1] = pass == 1;
                ss->rotates[f] = true;
            }
        }
    }
}

Gfx *gfx_new(Wad *w, char *err, int errsz) {
    int pl, cl;
    const u8 *playpal = wad_lump_named(w, "PLAYPAL", &pl);
    const u8 *cm = wad_lump_named(w, "COLORMAP", &cl);
    if (!playpal || pl < 768) { snprintf(err, (size_t) errsz, "PLAYPAL not found - this is not a Doom IWAD"); return NULL; }
    Gfx *g = (Gfx *) calloc(1, sizeof(Gfx));
    g->wad = w; g->playpal = playpal;
    strmap_init(&g->textures); strmap_init(&g->flats); strmap_init(&g->sprites); strmap_init(&g->patchCache);
    if (!cm || cl < 34 * 256) {
        /* Fallback: generate colormap from palette */
        u8 *c = (u8 *) calloc(34 * 256, 1);
        for (int l = 0; l < 32; l++)
            for (int i = 0; i < 256; i++)
                c[l * 256 + i] = (u8) nearest(playpal, playpal[i * 3] * (32 - l) / 32, playpal[i * 3 + 1] * (32 - l) / 32,
                                              playpal[i * 3 + 2] * (32 - l) / 32);
        for (int i = 0; i < 256; i++) { c[32 * 256 + i] = (u8) i; c[33 * 256 + i] = (u8) i; }
        g->colormapOwned = c; cm = c;
    }
    g->colormap = cm;
    for (int l = 0; l < 34; l++)
        for (int i = 0; i < 256; i++) {
            int c = cm[l * 256 + i];
            g->lit[l][i] = 0xFF000000u | (u32) playpal[c * 3] << 16 | (u32) playpal[c * 3 + 1] << 8 | (u32) playpal[c * 3 + 2];
        }
    load_flats(g);
    load_textures(g);
    load_sprites(g);
    return g;
}
