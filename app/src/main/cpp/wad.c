/* Reads IWAD/PWAD files fully into memory; lump lookup is case-insensitive, last lump wins. */
#include "doom.h"

/* ---------------------------------------------------------------- string helpers */
void str_upper(char out[9], const char *s) {
    int i = 0;
    for (; s[i] && i < 8; i++) {
        char c = s[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char) (c - 32) : c;
    }
    out[i] = 0;
}

void cstr_up(char out[9], const u8 *d, int max) {
    int e = 0;
    if (max > 8) max = 8;
    while (e < max && d[e] != 0) { char c = (char) d[e]; out[e] = (c >= 'a' && c <= 'z') ? (char) (c - 32) : c; e++; }
    out[e] = 0;
}

/* ---------------------------------------------------------------- string map */
static u32 hash8(const char *k) {
    u32 h = 2166136261u;
    for (int i = 0; i < 8 && k[i]; i++) { h ^= (u8) k[i]; h *= 16777619u; }
    return h;
}

void strmap_init(StrMap *m) { m->keys = NULL; m->vals = NULL; m->cap = 0; m->count = 0; }

void strmap_free(StrMap *m) { free(m->keys); free(m->vals); strmap_init(m); }

static bool norm_key(char out[9], const char *key) {
    int n = 0;
    while (key[n]) { if (n >= 8) return false; n++; }
    if (n == 0) return false;
    str_upper(out, key);
    return true;
}

static void strmap_grow(StrMap *m) {
    int nc = m->cap ? m->cap * 2 : 64;
    char (*nk)[9] = (char (*)[9]) calloc((size_t) nc, 9);
    void **nv = (void **) calloc((size_t) nc, sizeof(void *));
    for (int i = 0; i < m->cap; i++) {
        if (!m->keys[i][0]) continue;
        u32 h = hash8(m->keys[i]) & (u32) (nc - 1);
        while (nk[h][0]) h = (h + 1) & (u32) (nc - 1);
        memcpy(nk[h], m->keys[i], 9); nv[h] = m->vals[i];
    }
    free(m->keys); free(m->vals);
    m->keys = nk; m->vals = nv; m->cap = nc;
}

void *strmap_get(const StrMap *m, const char *key, bool *found) {
    char k[9];
    if (found) *found = false;
    if (m->cap == 0 || !norm_key(k, key)) return NULL;
    u32 h = hash8(k) & (u32) (m->cap - 1);
    while (m->keys[h][0]) {
        if (strcmp(m->keys[h], k) == 0) { if (found) *found = true; return m->vals[h]; }
        h = (h + 1) & (u32) (m->cap - 1);
    }
    return NULL;
}

bool strmap_put(StrMap *m, const char *key, void *val, bool overwrite) {
    char k[9];
    if (!norm_key(k, key)) return false;
    if (m->count * 2 >= m->cap) strmap_grow(m);
    u32 h = hash8(k) & (u32) (m->cap - 1);
    while (m->keys[h][0]) {
        if (strcmp(m->keys[h], k) == 0) {
            if (!overwrite) return false;
            m->vals[h] = val;
            return true;
        }
        h = (h + 1) & (u32) (m->cap - 1);
    }
    memcpy(m->keys[h], k, 9); m->vals[h] = val; m->count++;
    return true;
}

/* ---------------------------------------------------------------- WAD */
static bool is_digit(char c) { return c >= '0' && c <= '9'; }

bool wad_load(Wad *w, u8 *d, int len, char *err, int errsz) {
    memset(w, 0, sizeof(*w));
    w->data = d; w->len = len;
    if (len < 12) { snprintf(err, (size_t) errsz, "File is too small to be a WAD"); return false; }
    if (memcmp(d, "IWAD", 4) != 0 && memcmp(d, "PWAD", 4) != 0) {
        snprintf(err, (size_t) errsz, "Not a WAD file (header: %.4s)", (const char *) d); return false;
    }
    w->isIwad = memcmp(d, "IWAD", 4) == 0;
    int n = rd_i32(d + 4), dir = rd_i32(d + 8);
    if (n < 0 || dir < 0 || (long long) dir + (long long) n * 16 > len) {
        snprintf(err, (size_t) errsz, "Corrupt WAD (directory is outside the file)"); return false;
    }
    w->n = n;
    w->lumps = (Lump *) calloc((size_t) (n ? n : 1), sizeof(Lump));
    strmap_init(&w->index);
    for (int i = 0; i < n; i++) {
        int o = dir + i * 16;
        int p = rd_i32(d + o), s = rd_i32(d + o + 4);
        cstr_up(w->lumps[i].name, d + o + 8, 8);
        if (p < 0 || s < 0 || (long long) p + s > len) { p = 0; s = 0; }
        w->lumps[i].pos = p; w->lumps[i].size = s;
        strmap_put(&w->index, w->lumps[i].name, (void *) (intptr_t) (i + 1), true);   /* last lump wins */
    }
    w->nmaps = 0;
    for (int i = 0; i + 1 < n; i++) {
        const char *nm = w->lumps[i].name;
        size_t L = strlen(nm);
        bool m = (L == 4 && nm[0] == 'E' && is_digit(nm[1]) && nm[2] == 'M' && is_digit(nm[3]))
                 || (L == 5 && nm[0] == 'M' && nm[1] == 'A' && nm[2] == 'P' && is_digit(nm[3]) && is_digit(nm[4]));
        if (m && strcmp(w->lumps[i + 1].name, "THINGS") == 0 && w->nmaps < 256)
            memcpy(w->maps[w->nmaps++], nm, 9);
    }
    return true;
}

int wad_find(const Wad *w, const char *name) {
    bool f;
    void *v = strmap_get(&w->index, name, &f);
    return f ? (int) (intptr_t) v - 1 : -1;
}

int wad_find_after(const Wad *w, const char *name, int after) {
    char k[9];
    str_upper(k, name);
    for (int i = after + 1; i < w->n; i++) if (strcmp(w->lumps[i].name, k) == 0) return i;
    return -1;
}

const u8 *wad_lump(const Wad *w, int i, int *size) {
    if (size) *size = w->lumps[i].size;
    return w->data + w->lumps[i].pos;
}

const u8 *wad_lump_named(const Wad *w, const char *name, int *size) {
    int i = wad_find(w, name);
    if (i < 0) { if (size) *size = 0; return NULL; }
    return wad_lump(w, i, size);
}

bool wad_isDoom2(const Wad *w) { return wad_has(w, "MAP01") && !wad_has(w, "E1M1"); }

int wad_map_index(const Wad *w, const char *name) {
    for (int i = 0; i < w->nmaps; i++) if (strcmp(w->maps[i], name) == 0) return i;
    return -1;
}
