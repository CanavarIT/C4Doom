/*
 * Doom music. The WAD stores songs in the MUS format; they are converted to a standard MIDI file
 * and played by the platform's built-in MIDI synthesizer (MediaPlayer on Android). All work happens on a
 * single background thread so the game never stalls while a song is being prepared.
 */
#include "doom.h"
#include <pthread.h>

/* ------------------------------------------------------------------ MUS -> MIDI */
typedef struct { u8 *d; int n, cap; } OBuf;
static void ow(OBuf *o, int b) {
    if (o->n == o->cap) { o->cap = o->cap ? o->cap * 2 : 4096; o->d = (u8 *) realloc(o->d, (size_t) o->cap); }
    o->d[o->n++] = (u8) b;
}
static void vlq(OBuf *o, int v) {
    unsigned buf = (unsigned) v & 0x7F;
    unsigned u = (unsigned) v;
    while ((u >>= 7) > 0) { buf <<= 8; buf |= 0x80 | (u & 0x7F); }
    while (1) {
        ow(o, (int) (buf & 0xFF));
        if (buf & 0x80) buf >>= 8; else break;
    }
}

static const int CTRL[10] = {-1, 0, 1, 7, 10, 11, 91, 93, 64, 67};   /* MUS controller -> MIDI controller (0 = program change) */

/* Converts a Doom MUS lump into a format-0 Standard MIDI File (140 ticks per second). */
bool mus_to_midi(const u8 *mus, int len, u8 **outp, int *outLen) {
    if (!mus || len < 16 || mus[0] != 'M' || mus[1] != 'U' || mus[2] != 'S' || mus[3] != 0x1A) return false;
    int scoreLen = mus[4] | mus[5] << 8;
    int pos = mus[6] | mus[7] << 8;
    int end = mini(len, pos + scoreLen);
    OBuf tr = {NULL, 0, 0};
    int vel[16];
    for (int i = 0; i < 16; i++) vel[i] = 127;
    ow(&tr, 0); ow(&tr, 0xFF); ow(&tr, 0x51); ow(&tr, 3); ow(&tr, 0x0F); ow(&tr, 0x42); ow(&tr, 0x40);
    int delta = 0;
    bool done = false;
    while (!done && pos < end) {
        int ev = mus[pos++];
        bool last = (ev & 0x80) != 0;
        int type = (ev >> 4) & 7, ch = ev & 15;
        int mch = ch == 15 ? 9 : (ch >= 9 ? ch + 1 : ch);
        switch (type) {
            case 0: {
                if (pos >= end) { done = true; break; }
                int note = mus[pos++] & 127;
                vlq(&tr, delta); delta = 0;
                ow(&tr, 0x80 | mch); ow(&tr, note); ow(&tr, 0);
                break;
            }
            case 1: {
                if (pos >= end) { done = true; break; }
                int b = mus[pos++];
                int note = b & 127;
                if ((b & 128) != 0 && pos < end) vel[ch] = mus[pos++] & 127;
                vlq(&tr, delta); delta = 0;
                ow(&tr, 0x90 | mch); ow(&tr, note); ow(&tr, vel[ch]);
                break;
            }
            case 2: {
                if (pos >= end) { done = true; break; }
                int bend = mus[pos++] * 64;
                vlq(&tr, delta); delta = 0;
                ow(&tr, 0xE0 | mch); ow(&tr, bend & 127); ow(&tr, (bend >> 7) & 127);
                break;
            }
            case 3: {
                if (pos >= end) { done = true; break; }
                int c = mus[pos++] & 127, cc = -1;
                if (c == 10) cc = 120; else if (c == 11) cc = 123; else if (c == 12) cc = 126; else if (c == 13) cc = 127; else if (c == 14) cc = 121;
                if (cc >= 0) { vlq(&tr, delta); delta = 0; ow(&tr, 0xB0 | mch); ow(&tr, cc); ow(&tr, 0); }
                break;
            }
            case 4: {
                if (pos + 1 >= end) { done = true; break; }
                int c = mus[pos++] & 127, v = mus[pos++] & 127;
                if (c == 0) { vlq(&tr, delta); delta = 0; ow(&tr, 0xC0 | mch); ow(&tr, v); }
                else if (c < 10) { vlq(&tr, delta); delta = 0; ow(&tr, 0xB0 | mch); ow(&tr, CTRL[c]); ow(&tr, v); }
                break;
            }
            case 6: done = true; break;
            default: break;
        }
        if (done) break;
        if (last) {
            int d = 0, b;
            do {
                if (pos >= end) break;
                b = mus[pos++];
                d = d * 128 + (b & 127);
            } while ((b & 128) != 0);
            delta += d;
        }
    }
    vlq(&tr, delta);
    ow(&tr, 0xFF); ow(&tr, 0x2F); ow(&tr, 0);

    OBuf out = {NULL, 0, 0};
    static const u8 head[14] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 140};
    for (int i = 0; i < 14; i++) ow(&out, head[i]);
    ow(&out, 'M'); ow(&out, 'T'); ow(&out, 'r'); ow(&out, 'k');
    ow(&out, (tr.n >> 24) & 255); ow(&out, (tr.n >> 16) & 255); ow(&out, (tr.n >> 8) & 255); ow(&out, tr.n & 255);
    for (int i = 0; i < tr.n; i++) ow(&out, tr.d[i]);
    free(tr.d);
    *outp = out.d; *outLen = out.n;
    return true;
}

/* ------------------------------------------------------------------ player (worker thread) */
enum { CMD_START = 1, CMD_VOLUME, CMD_PAUSE, CMD_RESUME };
typedef struct { int cmd; char lump[9]; bool loop; } Cmd;

static Wad *wad;
static pthread_t thread;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static Cmd queue[64];
static int qh, qt;
static bool started;

/* state shared with the game thread */
static char wanted[9];
static bool wantedLoop;
static volatile float volume = 0.7f;
static volatile bool paused;
/* worker only */
static void *mp;

static void post(int cmd, const char *lump, bool loop) {
    pthread_mutex_lock(&mtx);
    if (((qt + 1) & 63) != qh) {
        Cmd *c = &queue[qt];
        c->cmd = cmd; c->loop = loop;
        snprintf(c->lump, sizeof c->lump, "%s", lump ? lump : "");
        qt = (qt + 1) & 63;
        pthread_cond_signal(&cv);
    }
    pthread_mutex_unlock(&mtx);
}

static void releaseMp(void) {
    if (mp) mp_release(mp);
    mp = NULL;
}

static void startSong(const char *lump, bool loop) {
    releaseMp();
    if (!wad_has(wad, lump)) return;
    int len;
    const u8 *d = wad_lump_named(wad, lump, &len);
    u8 *midi = NULL; int mlen = 0;
    if (!d || !mus_to_midi(d, len, &midi, &mlen)) return;
    char path[600];
    snprintf(path, sizeof path, "%s/jvmus_%s.mid", plat_data_dir(), lump);
    FILE *f = fopen(path, "wb");
    if (!f) { free(midi); return; }
    fwrite(midi, 1, (size_t) mlen, f);
    fclose(f);
    free(midi);
    void *p = mp_open(path, loop);
    if (!p) return;
    mp_setVolume(p, volume);
    mp = p;
    if (volume > 0.0f && !paused) mp_start(p);
}

static void applyVolume(void) {
    if (volume <= 0.0f || paused) {
        if (mp && mp_isPlaying(mp)) mp_pause(mp);
        return;
    }
    if (!mp) {
        if (wanted[0]) { char l[9]; bool lp; pthread_mutex_lock(&mtx); memcpy(l, wanted, 9); lp = wantedLoop; pthread_mutex_unlock(&mtx); startSong(l, lp); }
        return;
    }
    mp_setVolume(mp, volume);
    if (!mp_isPlaying(mp)) mp_start(mp);
}

static void *worker(void *arg) {
    (void) arg;
    for (;;) {
        pthread_mutex_lock(&mtx);
        while (qh == qt) pthread_cond_wait(&cv, &mtx);
        Cmd c = queue[qh];
        qh = (qh + 1) & 63;
        pthread_mutex_unlock(&mtx);
        switch (c.cmd) {
            case CMD_START: startSong(c.lump, c.loop); break;
            case CMD_VOLUME: case CMD_RESUME: applyVolume(); break;
            case CMD_PAUSE: if (mp && mp_isPlaying(mp)) mp_pause(mp); break;
            default: break;
        }
    }
    return NULL;
}

void music_init(Wad *w) {
    wad = w;
    wanted[0] = 0;
    if (!started) { started = true; pthread_create(&thread, NULL, worker, NULL); pthread_detach(thread); }
}

/* Plays the given music lump; does nothing when it is already the current song. */
void music_request(const char *lump, bool loop) {
    if (!lump) return;
    char up[9];
    str_upper(up, lump);
    pthread_mutex_lock(&mtx);
    bool same = strcmp(up, wanted) == 0;
    if (!same) { memcpy(wanted, up, 9); wantedLoop = loop; }
    pthread_mutex_unlock(&mtx);
    if (!same) post(CMD_START, up, loop);
}

void music_setVolume(float v) { volume = clampf(v, 0.0f, 1.0f); post(CMD_VOLUME, NULL, false); }
void music_pause(void) { paused = true; post(CMD_PAUSE, NULL, false); }
void music_resume(void) { paused = false; post(CMD_RESUME, NULL, false); }
