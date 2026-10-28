/* Developer test harness (not part of the Android build): runs the engine on a PC and dumps frames as PPM. */
#include "../app/src/main/cpp/doom.h"
#include <time.h>
#include <unistd.h>

static int64_t fake = 1000000000LL;
int64_t plat_nanotime(void) { return fake; }
const char *plat_data_dir(void) { return "/tmp/c4doom"; }
void plat_haptic(void) {}
void plat_quit(void) {}
void plat_audio_start(void) { snd_setRunning(true); }
void plat_audio_stop(void) { snd_setRunning(false); }
void *mp_open(const char *p, bool l) { (void) p; (void) l; return NULL; }
void mp_setVolume(void *h, float v) { (void) h; (void) v; }
bool mp_isPlaying(void *h) { (void) h; return false; }
void mp_start(void *h) { (void) h; }
void mp_pause(void *h) { (void) h; }
void mp_release(void *h) { (void) h; }

static void dump(const char *name) {
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", CV.w, CV.h);
    for (int i = 0; i < CV.w * CV.h; i++) { u32 c = CV.px[i]; fputc((c >> 16) & 255, f); fputc((c >> 8) & 255, f); fputc(c & 255, f); }
    fclose(f);
}

static void frames(int n) { for (int i = 0; i < n; i++) { fake += 28571428LL; gv_frame(); } }

int main(int argc, char **argv) {
    const char *wadpath = argc > 1 ? argv[1] : "doom1.wad";
    const char *out = argc > 2 ? argv[2] : "/tmp/c4doom";
    system("mkdir -p /tmp/c4doom");
    FILE *f = fopen(wadpath, "rb");
    if (!f) { fprintf(stderr, "no wad\n"); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *data = malloc(n); fread(data, 1, n, f); fclose(f);
    static Wad wad; char err[200];
    if (!wad_load(&wad, data, (int) n, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 1; }
    Gfx *g = gfx_new(&wad, err, sizeof err);
    if (!g) { fprintf(stderr, "%s\n", err); return 1; }
    printf("maps=%d first=%s\n", wad.nmaps, wad.maps[0]);
    gv_init(2.5f);
    gv_surfaceChanged(1280, 576);
    gv_setWad(&wad, g);
    gv_onResume();
    char p[256];
    if (getenv("STRESS_ONLY")) { gv_startNewGame(1, 2); frames(2); goto stress; }
    frames(3); snprintf(p, sizeof p, "%s/title.ppm", out); dump(p);
    gv_openMenu(); frames(2); snprintf(p, sizeof p, "%s/menu.ppm", out); dump(p);
    gv_closeMenu();
    gv_startNewGame(1, 2);
    frames(20); snprintf(p, sizeof p, "%s/e1m1_start.ppm", out); dump(p);
    GM->inMove = 1.0f;
    for (int k = 0; k < 6; k++) { frames(60); GM->inFire = (k & 1); }
    GM->inFire = false;
    snprintf(p, sizeof p, "%s/e1m1_walk.ppm", out); dump(p);
    GM->inMove = 0; GM->inFire = true; frames(4); snprintf(p, sizeof p, "%s/fire.ppm", out); dump(p); GM->inFire = false;
    GV.mapOpen = true; frames(2); snprintf(p, sizeof p, "%s/map.ppm", out); dump(p); GV.mapOpen = false;
    printf("health=%d kills=%d/%d tic=%d\n", GM->health, GM->kills, GM->totalKills, GM->tic);
    /* save / load round trip */
    printf("save=%d\n", save_save(0));
    char d[64]; printf("describe=%d '%s'\n", save_describe(0, d, sizeof d), d);
    printf("load=%d\n", save_load(0));
    frames(10);
    /* all maps load */
    for (int i = 0; i < wad.nmaps; i++) { game_startMap(wad.maps[i], false, err, sizeof err); frames(30); printf("%s ok mobjs=%d\n", wad.maps[i], GM->mobjs.n); }
stress:
    /* stress: random play on every map */
    srand(12345);
    for (int i = 0; i < wad.nmaps; i++) {
        game_startMap(wad.maps[i], false, err, sizeof err);
        GM->god = (i % 2 == 0);
        for (int f = 0; f < 4000; f++) {
            if ((f % 40) == 0) { GM->inMove = (rand() % 3 - 1) * 1.0f; GM->inStrafe = (rand() % 3 - 1) * 1.0f; GM->inFire = rand() % 2; GM->inUse = rand() % 4 == 0; game_addTurn((rand() % 100 - 50) * 0.01f); }
            if (GM->state == ST_DEAD) { game_startMap(wad.maps[i], false, err, sizeof err); }
            if (GM->state == ST_EXIT) game_startMap(wad.maps[i], false, err, sizeof err);
            if ((f % 500) == 250) { game_cheatWeapons(); game_selectWeapon(1 + rand() % 8); }
            game_tick(); fake += 28571428LL;
            if (f % 200 == 0) { game_render(0.5f); }
        }
        printf("stress %s ok kills=%d/%d items=%d/%d mobjs=%d tic=%d\n", wad.maps[i], GM->kills, GM->totalKills, GM->items, GM->totalItems, GM->mobjs.n, GM->tic);
    }
    if (getenv("STRESS_ONLY")) return 0;
    /* intermission */
    game_startMap("E1M1", false, NULL, 0);
    game_exitLevel(false); frames(120); snprintf(p, sizeof p, "%s/intermission.ppm", out); dump(p);
    frames(200); snprintf(p, sizeof p, "%s/intermission2.ppm", out); dump(p);
    return 0;
}
