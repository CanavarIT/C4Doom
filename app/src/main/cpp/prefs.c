/* Tiny key=value settings file (replacement for SharedPreferences). */
#include "doom.h"

#define MAXP 96
static struct { char k[24]; char v[32]; } P[MAXP];
static int np;

static void path(char *out, int sz) { snprintf(out, (size_t) sz, "%s/c4doom.cfg", plat_data_dir()); }

void prefs_load(void) {
    char p[512];
    path(p, sizeof p);
    FILE *f = fopen(p, "r");
    np = 0;
    if (!f) return;
    char line[96];
    while (fgets(line, sizeof line, f) && np < MAXP) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *v = eq + 1;
        size_t n = strlen(v);
        while (n > 0 && (v[n - 1] == '\n' || v[n - 1] == '\r')) v[--n] = 0;
        snprintf(P[np].k, sizeof P[np].k, "%s", line);
        snprintf(P[np].v, sizeof P[np].v, "%s", v);
        np++;
    }
    fclose(f);
}

static const char *get(const char *k) {
    for (int i = 0; i < np; i++) if (strcmp(P[i].k, k) == 0) return P[i].v;
    return NULL;
}
static void put(const char *k, const char *v) {
    for (int i = 0; i < np; i++) if (strcmp(P[i].k, k) == 0) { snprintf(P[i].v, sizeof P[i].v, "%s", v); return; }
    if (np >= MAXP) return;
    snprintf(P[np].k, sizeof P[np].k, "%s", k);
    snprintf(P[np].v, sizeof P[np].v, "%s", v);
    np++;
}

int prefs_getInt(const char *k, int def) { const char *v = get(k); return v ? atoi(v) : def; }
float prefs_getFloat(const char *k, float def) { const char *v = get(k); return v ? (float) atof(v) : def; }
bool prefs_getBool(const char *k, bool def) { const char *v = get(k); return v ? v[0] == '1' : def; }
void prefs_setInt(const char *k, int v) { char b[16]; snprintf(b, sizeof b, "%d", v); put(k, b); }
void prefs_setFloat(const char *k, float v) { char b[32]; snprintf(b, sizeof b, "%.4f", (double) v); put(k, b); }
void prefs_setBool(const char *k, bool v) { put(k, v ? "1" : "0"); }

void prefs_flush(void) {
    char p[512], t[540];
    path(p, sizeof p);
    snprintf(t, sizeof t, "%s.tmp", p);
    FILE *f = fopen(t, "w");
    if (!f) return;
    for (int i = 0; i < np; i++) fprintf(f, "%s=%s\n", P[i].k, P[i].v);
    if (fclose(f) == 0) { remove(p); rename(t, p); }
}
