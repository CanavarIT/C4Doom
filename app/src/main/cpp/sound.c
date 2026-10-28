/* Small software mixer: Doom DS* sound lumps -> one stereo stream with distance attenuation and panning. */
#include "doom.h"
#include <pthread.h>

#define MAXV 12

typedef struct { short *d; int n; } SndData;

static Wad *wad;
static StrMap cache;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static const short *vData[MAXV];
static int vLen[MAXV], vPos[MAXV];
static float vL[MAXV], vR[MAXV];
static volatile bool running;
float SND_volume = 1.0f, SND_lx, SND_ly;
static float la;

void snd_init(Wad *w) { wad = w; strmap_init(&cache); }
void snd_setRunning(bool r) { running = r; }

static SndData *load(const char *name) {
    bool found;
    SndData *sd = (SndData *) strmap_get(&cache, name, &found);
    if (found) return sd;
    sd = NULL;
    int len;
    const u8 *d = wad_lump_named(wad, name, &len);
    if (d && len > 8 + 32 && rd_u16(d) == 3) {
        int rate = rd_u16(d + 2), n = rd_i32(d + 4);
        if (n > len - 8) n = len - 8;
        int start = 8 + 16, end = 8 + n - 16;
        if (end > start && rate > 0) {
            int l = end - start;
            int outLen = (int) ((long long) l * SND_RATE / rate);
            sd = (SndData *) malloc(sizeof *sd);
            sd->n = outLen;
            sd->d = (short *) malloc((size_t) (outLen ? outLen : 1) * sizeof(short));
            for (int i = 0; i < outLen; i++) {
                float p = (float) i * rate / SND_RATE;
                int i0 = (int) p;
                float f = p - i0;
                int a = d[start + mini(i0, l - 1)] - 128;
                int b = d[start + mini(i0 + 1, l - 1)] - 128;
                sd->d[i] = (short) (int) ((a + (b - a) * f) * 200.0f);
            }
        }
    }
    strmap_put(&cache, name, sd, true);
    return sd;
}

void snd_listener(float x, float y, float a) { SND_lx = x; SND_ly = y; la = a; }

void snd_play(const char *lump, float x, float y) {
    if (!running) return;
    pthread_mutex_lock(&lock);
    SndData *s = load(lump);
    if (!s) { pthread_mutex_unlock(&lock); return; }
    float dx = x - SND_lx, dy = y - SND_ly;
    float dist = (float) sqrt((double) (dx * dx + dy * dy));
    float vol = (dist < 200.0f ? 1.0f : maxf(0.0f, 1.0f - (dist - 200.0f) / 1100.0f)) * SND_volume;
    if (vol < 0.03f) { pthread_mutex_unlock(&lock); return; }
    float pan = 0.0f;
    if (dist > 8.0f) pan = (float) sin(atan2((double) dy, (double) dx) - la);   /* +1 = left */
    float l = vol * (0.72f + 0.28f * pan), r = vol * (0.72f - 0.28f * pan);
    int slot = -1;
    for (int i = 0; i < MAXV; i++) if (!vData[i]) { slot = i; break; }
    if (slot < 0) {
        int oldest = 0, best = -1;
        for (int i = 0; i < MAXV; i++) if (vPos[i] > best) { best = vPos[i]; oldest = i; }
        slot = oldest;
    }
    vData[slot] = s->d; vLen[slot] = s->n; vPos[slot] = 0; vL[slot] = l; vR[slot] = r;
    pthread_mutex_unlock(&lock);
}

void snd_clear(void) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAXV; i++) vData[i] = NULL;
    pthread_mutex_unlock(&lock);
}

/* Mixes `frames` stereo frames into out (interleaved L,R). Called from the audio thread. */
void snd_mix(short *out, int frames) {
    int acc[1024 * 2];
    if (frames > 1024) frames = 1024;
    memset(acc, 0, (size_t) frames * 2 * sizeof(int));
    pthread_mutex_lock(&lock);
    for (int v = 0; v < MAXV; v++) {
        const short *d = vData[v];
        if (!d) continue;
        int p = vPos[v], n = mini(frames, vLen[v] - p);
        float l = vL[v], r = vR[v];
        for (int i = 0; i < n; i++) {
            int smp = d[p + i];
            acc[i * 2] += (int) (smp * l);
            acc[i * 2 + 1] += (int) (smp * r);
        }
        vPos[v] = p + n;
        if (vPos[v] >= vLen[v]) vData[v] = NULL;
    }
    pthread_mutex_unlock(&lock);
    for (int i = 0; i < frames * 2; i++) {
        int x = acc[i];
        out[i] = (short) (x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
    }
}
