/* Parsed map: geometry, BSP, sectors and a coarse line grid for collision queries. */
#include "doom.h"

#define CELL 128

static const u8 *map_lump(Wad *w, int mapIdx, const char *name, int *size) {
    /* map lumps follow the marker directly (within the next 12 entries) */
    int lim = mini(w->n, mapIdx + 13);
    for (int i = mapIdx + 1; i < lim; i++)
        if (strcmp(w->lumps[i].name, name) == 0) return wad_lump(w, i, size);
    if (size) *size = 0;
    return NULL;
}

static void build_grid(Level *L) {
    L->gx0 = (int) floor(L->minX / CELL) - 1; L->gy0 = (int) floor(L->minY / CELL) - 1;
    L->gw = (int) floor(L->maxX / CELL) - L->gx0 + 2; L->gh = (int) floor(L->maxY / CELL) - L->gy0 + 2;
    int cells = L->gw * L->gh;
    L->gridN = (int *) calloc((size_t) cells, sizeof(int));
    L->grid = (int **) calloc((size_t) cells, sizeof(int *));
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            for (int i = 0; i < cells; i++) { if (L->gridN[i]) L->grid[i] = (int *) malloc((size_t) L->gridN[i] * sizeof(int)); L->gridN[i] = 0; }
        }
        for (int li = 0; li < L->nlines; li++) {
            Line *l = &L->lines[li];
            int c0 = (int) floor(l->minx / CELL) - L->gx0, c1 = (int) floor(l->maxx / CELL) - L->gx0;
            int r0 = (int) floor(l->miny / CELL) - L->gy0, r1 = (int) floor(l->maxy / CELL) - L->gy0;
            for (int r = r0; r <= r1; r++)
                for (int c = c0; c <= c1; c++) {
                    if (c < 0 || r < 0 || c >= L->gw || r >= L->gh) continue;
                    int cell = r * L->gw + c;
                    if (pass == 1) L->grid[cell][L->gridN[cell]] = l->idx;
                    L->gridN[cell]++;
                }
        }
    }
}

Level *level_load(Wad *w, Gfx *gfx, const char *mapName, char *err, int errsz) {
    int m = wad_find(w, mapName);
    if (m < 0) { snprintf(err, (size_t) errsz, "No such map: %s", mapName); return NULL; }
    int vts, lds, sds, scs, sgs, sss, nds, ths;
    const u8 *vt = map_lump(w, m, "VERTEXES", &vts), *ld = map_lump(w, m, "LINEDEFS", &lds), *sd = map_lump(w, m, "SIDEDEFS", &sds),
             *sc = map_lump(w, m, "SECTORS", &scs), *sg = map_lump(w, m, "SEGS", &sgs), *ss = map_lump(w, m, "SSECTORS", &sss),
             *nd = map_lump(w, m, "NODES", &nds), *th = map_lump(w, m, "THINGS", &ths);
    if (!vt || !ld || !sd || !sc || !sg || !ss || !nd) {
        snprintf(err, (size_t) errsz, "Map %s is damaged or has no BSP nodes (a WAD with compiled nodes is required)", mapName);
        return NULL;
    }
    Level *L = (Level *) calloc(1, sizeof(Level));
    str_upper(L->name, mapName);
    L->validcount = 1;

    int nv = vts / 4;
    L->nv = nv;
    L->vx = (float *) calloc((size_t) (nv ? nv : 1), sizeof(float)); L->vy = (float *) calloc((size_t) (nv ? nv : 1), sizeof(float));
    L->minX = L->minY = 1e9f; L->maxX = L->maxY = -1e9f;
    for (int i = 0; i < nv; i++) {
        L->vx[i] = (float) rd_s16(vt + i * 4); L->vy[i] = (float) rd_s16(vt + i * 4 + 2);
        L->minX = minf(L->minX, L->vx[i]); L->maxX = maxf(L->maxX, L->vx[i]);
        L->minY = minf(L->minY, L->vy[i]); L->maxY = maxf(L->maxY, L->vy[i]);
    }

    int ns = scs / 26;
    L->nsectors = ns;
    L->sectors = (Sector *) calloc((size_t) (ns ? ns : 1), sizeof(Sector));
    for (int i = 0; i < ns; i++) {
        int o = i * 26;
        Sector *s = &L->sectors[i];
        s->idx = i;
        s->floor = s->origFloor = (float) rd_s16(sc + o);
        s->ceil = s->origCeil = (float) rd_s16(sc + o + 2);
        cstr_up(s->floorName, sc + o + 4, 8);
        cstr_up(s->ceilName, sc + o + 12, 8);
        s->floorPic = gfx_flat(gfx, s->floorName);
        s->ceilPic = gfx_flat(gfx, s->ceilName);
        s->skyCeil = strcmp(s->ceilName, "F_SKY1") == 0;
        s->light = rd_s16(sc + o + 20);
        s->special = rd_s16(sc + o + 22);
        s->tag = rd_s16(sc + o + 24);
    }

    int nsd = sds / 30;
    L->nsides = nsd;
    L->sides = (Side *) calloc((size_t) (nsd ? nsd : 1), sizeof(Side));
    for (int i = 0; i < nsd; i++) {
        int o = i * 30;
        Side *s = &L->sides[i];
        s->xoff = (float) rd_s16(sd + o); s->yoff = (float) rd_s16(sd + o + 2);
        cstr_up(s->upperName, sd + o + 4, 8);
        cstr_up(s->lowerName, sd + o + 12, 8);
        cstr_up(s->midName, sd + o + 20, 8);
        s->upper = gfx_texture(gfx, s->upperName);
        s->lower = gfx_texture(gfx, s->lowerName);
        s->mid = gfx_texture(gfx, s->midName);
        int sec = rd_u16(sd + o + 28);
        s->sector = sec < ns ? &L->sectors[sec] : &L->sectors[0];
    }

    int nl = lds / 14;
    L->nlines = nl;
    L->lines = (Line *) calloc((size_t) (nl ? nl : 1), sizeof(Line));
    for (int i = 0; i < nl; i++) {
        int o = i * 14;
        Line *l = &L->lines[i];
        l->idx = i;
        l->v1 = rd_u16(ld + o); l->v2 = rd_u16(ld + o + 2);
        l->flags = rd_u16(ld + o + 4); l->special = rd_u16(ld + o + 6); l->tag = rd_u16(ld + o + 8);
        int s0 = rd_u16(ld + o + 10), s1 = rd_u16(ld + o + 12);
        if (l->v1 >= nv || l->v2 >= nv) { l->v1 = 0; l->v2 = 0; }
        l->x1 = L->vx[l->v1]; l->y1 = L->vy[l->v1]; l->x2 = L->vx[l->v2]; l->y2 = L->vy[l->v2];
        l->dx = l->x2 - l->x1; l->dy = l->y2 - l->y1;
        l->len = (float) sqrt((double) (l->dx * l->dx + l->dy * l->dy));
        l->minx = minf(l->x1, l->x2); l->maxx = maxf(l->x1, l->x2);
        l->miny = minf(l->y1, l->y2); l->maxy = maxf(l->y1, l->y2);
        l->front = s0 != 0xFFFF && s0 < nsd ? &L->sides[s0] : NULL;
        l->back = s1 != 0xFFFF && s1 < nsd ? &L->sides[s1] : NULL;
        l->fs = l->front ? l->front->sector : NULL;
        l->bs = l->back ? l->back->sector : NULL;
        if (!l->back) l->flags &= ~ML_TWOSIDED;
    }
    /* per sector line lists (front sector first, then back sector when different) */
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            for (int i = 0; i < ns; i++) { Sector *s = &L->sectors[i]; s->lines = (Line **) malloc((size_t) (s->nlines ? s->nlines : 1) * sizeof(Line *)); s->nlines = 0; }
        }
        for (int i = 0; i < nl; i++) {
            Line *l = &L->lines[i];
            if (l->fs) { if (pass == 1) l->fs->lines[l->fs->nlines] = l; l->fs->nlines++; }
            if (l->bs && l->bs != l->fs) { if (pass == 1) l->bs->lines[l->bs->nlines] = l; l->bs->nlines++; }
        }
    }
    for (int i = 0; i < ns; i++) {
        Sector *s = &L->sectors[i];
        float sx = 0, sy = 0; int c = 0;
        for (int k = 0; k < s->nlines; k++) { Line *l = s->lines[k]; sx += l->x1 + l->x2; sy += l->y1 + l->y2; c += 2; }
        if (c > 0) { s->cx = sx / c; s->cy = sy / c; }
    }

    int nsg = sgs / 12;
    L->nsegs = nsg;
    L->segs = (Seg *) calloc((size_t) (nsg ? nsg : 1), sizeof(Seg));
    for (int i = 0; i < nsg; i++) {
        int o = i * 12;
        Seg *s = &L->segs[i];
        int a = rd_u16(sg + o), b = rd_u16(sg + o + 2), li = rd_u16(sg + o + 6);
        s->dir = rd_s16(sg + o + 8) != 0;
        s->offset = (float) rd_s16(sg + o + 10);
        if (a >= nv) a = 0;
        if (b >= nv) b = 0;
        if (li >= nl) li = 0;
        s->x1 = L->vx[a]; s->y1 = L->vy[a]; s->x2 = L->vx[b]; s->y2 = L->vy[b];
        s->len = (float) hypot((double) (s->x2 - s->x1), (double) (s->y2 - s->y1));
        s->line = &L->lines[li];
        s->side = s->dir ? s->line->back : s->line->front;
        if (!s->side) s->side = s->line->front ? s->line->front : s->line->back;
        s->front = s->dir ? s->line->bs : s->line->fs;
        s->back = (s->line->flags & ML_TWOSIDED) ? (s->dir ? s->line->fs : s->line->bs) : NULL;
        if (!s->front) s->front = &L->sectors[0];
    }

    int nss = sss / 4;
    L->nss = nss;
    L->ssNum = (int *) calloc((size_t) (nss ? nss : 1), sizeof(int));
    L->ssFirst = (int *) calloc((size_t) (nss ? nss : 1), sizeof(int));
    L->ssSector = (Sector **) calloc((size_t) (nss ? nss : 1), sizeof(Sector *));
    for (int i = 0; i < nss; i++) {
        L->ssNum[i] = rd_u16(ss + i * 4);
        L->ssFirst[i] = rd_u16(ss + i * 4 + 2);
        int f = L->ssFirst[i];
        L->ssSector[i] = f < nsg ? L->segs[f].front : &L->sectors[0];
    }

    int nn = nds / 28;
    L->nn = nn;
    L->nx = (float *) calloc((size_t) (nn ? nn : 1), sizeof(float)); L->ny = (float *) calloc((size_t) (nn ? nn : 1), sizeof(float));
    L->ndx = (float *) calloc((size_t) (nn ? nn : 1), sizeof(float)); L->ndy = (float *) calloc((size_t) (nn ? nn : 1), sizeof(float));
    L->nbb = (float (*)[8]) calloc((size_t) (nn ? nn : 1), sizeof(float[8]));
    L->nchild = (int *) calloc((size_t) (nn ? nn * 2 : 2), sizeof(int));
    for (int i = 0; i < nn; i++) {
        int o = i * 28;
        L->nx[i] = (float) rd_s16(nd + o); L->ny[i] = (float) rd_s16(nd + o + 2);
        L->ndx[i] = (float) rd_s16(nd + o + 4); L->ndy[i] = (float) rd_s16(nd + o + 6);
        for (int k = 0; k < 8; k++) L->nbb[i][k] = (float) rd_s16(nd + o + 8 + k * 2);
        L->nchild[i * 2] = rd_u16(nd + o + 24);
        L->nchild[i * 2 + 1] = rd_u16(nd + o + 26);
    }

    int nt = th ? ths / 10 : 0;
    L->nthings = nt;
    L->things = (Thing *) calloc((size_t) (nt ? nt : 1), sizeof(Thing));
    for (int i = 0; i < nt; i++) {
        int o = i * 10;
        Thing *t = &L->things[i];
        t->x = (float) rd_s16(th + o); t->y = (float) rd_s16(th + o + 2);
        t->angle = rd_s16(th + o + 4); t->type = rd_u16(th + o + 6); t->flags = rd_u16(th + o + 8);
    }
    build_grid(L);
    return L;
}

void level_free(Level *L) {
    if (!L) return;
    for (int i = 0; i < L->nsectors; i++) free(L->sectors[i].lines);
    if (L->grid) { for (int i = 0; i < L->gw * L->gh; i++) free(L->grid[i]); }
    free(L->grid); free(L->gridN);
    free(L->vx); free(L->vy); free(L->lines); free(L->sides); free(L->sectors); free(L->segs);
    free(L->ssNum); free(L->ssFirst); free(L->ssSector);
    free(L->nx); free(L->ny); free(L->ndx); free(L->ndy); free(L->nbb); free(L->nchild); free(L->things);
    free(L);
}

/* ------------------------------------------------------------------ BSP queries */
int level_pointOnSide(const Level *L, float x, float y, int node) {
    float dx = x - L->nx[node], dy = y - L->ny[node];
    return (L->ndx[node] * dy - L->ndy[node] * dx) < 0 ? 0 : 1;
}

Sector *level_sectorAt(const Level *L, float x, float y) {
    if (L->nn == 0) return L->nss > 0 ? L->ssSector[0] : &L->sectors[0];
    int c = L->nn - 1;
    int guard = 0;
    while ((c & 0x8000) == 0 && guard++ < 4096) c = L->nchild[c * 2 + level_pointOnSide(L, x, y, c)];
    int s = c & 0x7FFF;
    return s < L->nss ? L->ssSector[s] : &L->sectors[0];
}

/* Calls visitor for every line whose cells overlap the box (each line once). */
bool level_forLinesInBox(Level *L, float x0, float y0, float x1, float y1, LineVisitor v, void *ctx) {
    L->validcount++;
    int c0 = (int) floor(x0 / CELL) - L->gx0, c1 = (int) floor(x1 / CELL) - L->gx0;
    int r0 = (int) floor(y0 / CELL) - L->gy0, r1 = (int) floor(y1 / CELL) - L->gy0;
    c0 = maxi(c0, 0); r0 = maxi(r0, 0); c1 = mini(c1, L->gw - 1); r1 = mini(r1, L->gh - 1);
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
            int cell = r * L->gw + c;
            const int *ids = L->grid[cell];
            int n = L->gridN[cell];
            for (int k = 0; k < n; k++) {
                Line *l = &L->lines[ids[k]];
                if (l->validcount == L->validcount) continue;
                l->validcount = L->validcount;
                if (!v(l, ctx)) return false;
            }
        }
    return true;
}
