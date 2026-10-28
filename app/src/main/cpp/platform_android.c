/*
 * Android platform layer, pure C on top of NativeActivity:
 *   - window / software presentation (ANativeWindow)
 *   - touch + back key (AInputQueue)
 *   - WAD discovery (external dir, internal dir, APK assets)
 *   - sound output (OpenSL ES), music (MediaPlayer through JNI), haptics + immersive mode (JNI)
 */
#include "doom.h"
#include <jni.h>
#include <errno.h>
#include <dirent.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/window.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android_native_app_glue.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "C4Doom", __VA_ARGS__)

static struct android_app *App;
static ANativeWindow *Win;
static bool appPaused = true, loaded, loading;
static char status[1024];
static float density = 2.75f;
static int winW, winH;

/* ================================================================== helpers */
int64_t plat_nanotime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

const char *plat_data_dir(void) {
    static char dir[512];
    if (!dir[0]) {
        const char *p = App && App->activity ? App->activity->internalDataPath : NULL;
        snprintf(dir, sizeof dir, "%s", p ? p : "/data/local/tmp");
    }
    return dir;
}

static JNIEnv *env_get(void) {
    JavaVM *vm = App->activity->vm;
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void **) &env, JNI_VERSION_1_6) == JNI_OK) return env;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK) return NULL;
    return env;
}

static bool jni_ok(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return false; }
    return true;
}

/* Activity.getWindow().getDecorView() as a local reference (or NULL) */
static jobject decor_view(JNIEnv *env) {
    jobject act = App->activity->clazz;
    jclass ac = (*env)->GetObjectClass(env, act);
    jmethodID gw = (*env)->GetMethodID(env, ac, "getWindow", "()Landroid/view/Window;");
    jobject w = gw ? (*env)->CallObjectMethod(env, act, gw) : NULL;
    (*env)->DeleteLocalRef(env, ac);
    if (!jni_ok(env) || !w) return NULL;
    jclass wc = (*env)->GetObjectClass(env, w);
    jmethodID gd = (*env)->GetMethodID(env, wc, "getDecorView", "()Landroid/view/View;");
    jobject dv = gd ? (*env)->CallObjectMethod(env, w, gd) : NULL;
    (*env)->DeleteLocalRef(env, wc);
    (*env)->DeleteLocalRef(env, w);
    if (!jni_ok(env)) return NULL;
    return dv;
}

/* Immersive sticky full screen: hide status + navigation bars. */
static void hide_system_ui(void) {
    JNIEnv *env = env_get();
    if (!env) return;
    jobject dv = decor_view(env);
    if (!dv) return;
    jclass vc = (*env)->GetObjectClass(env, dv);
    jmethodID m = (*env)->GetMethodID(env, vc, "setSystemUiVisibility", "(I)V");
    if (m) (*env)->CallVoidMethod(env, dv, m, (jint) 0x1706);   /* IMMERSIVE_STICKY|FULLSCREEN|HIDE_NAVIGATION|LAYOUT_* */
    jni_ok(env);
    (*env)->DeleteLocalRef(env, vc);
    (*env)->DeleteLocalRef(env, dv);
}

void plat_haptic(void) {
    JNIEnv *env = env_get();
    if (!env) return;
    jobject dv = decor_view(env);
    if (!dv) return;
    jclass vc = (*env)->GetObjectClass(env, dv);
    jmethodID m = (*env)->GetMethodID(env, vc, "performHapticFeedback", "(II)Z");
    if (m) (*env)->CallBooleanMethod(env, dv, m, (jint) 0 /* LONG_PRESS */, (jint) 2 /* FLAG_IGNORE_GLOBAL_SETTING */);
    jni_ok(env);
    (*env)->DeleteLocalRef(env, vc);
    (*env)->DeleteLocalRef(env, dv);
}

void plat_quit(void) { ANativeActivity_finish(App->activity); }

/* ================================================================== music backend (MediaPlayer via JNI) */
void *mp_open(const char *path, bool loop) {
    JNIEnv *env = env_get();
    if (!env) return NULL;
    jclass cls = (*env)->FindClass(env, "android/media/MediaPlayer");
    if (!cls) { jni_ok(env); return NULL; }
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    jobject mp = ctor ? (*env)->NewObject(env, cls, ctor) : NULL;
    if (!jni_ok(env) || !mp) { (*env)->DeleteLocalRef(env, cls); return NULL; }
    jstring js = (*env)->NewStringUTF(env, path);
    bool ok = true;
    jmethodID m = (*env)->GetMethodID(env, cls, "setDataSource", "(Ljava/lang/String;)V");
    if (m) (*env)->CallVoidMethod(env, mp, m, js);
    ok = jni_ok(env) && m;
    if (ok) { m = (*env)->GetMethodID(env, cls, "setLooping", "(Z)V"); if (m) (*env)->CallVoidMethod(env, mp, m, (jboolean) loop); ok = jni_ok(env); }
    if (ok) { m = (*env)->GetMethodID(env, cls, "prepare", "()V"); if (m) (*env)->CallVoidMethod(env, mp, m); ok = jni_ok(env) && m; }
    (*env)->DeleteLocalRef(env, js);
    jobject g = NULL;
    if (ok) g = (*env)->NewGlobalRef(env, mp);
    else {
        m = (*env)->GetMethodID(env, cls, "release", "()V");
        if (m) (*env)->CallVoidMethod(env, mp, m);
        jni_ok(env);
    }
    (*env)->DeleteLocalRef(env, mp);
    (*env)->DeleteLocalRef(env, cls);
    return g;
}

void mp_setVolume(void *h, float v) {
    JNIEnv *env = env_get();
    if (!env || !h) return;
    jclass cls = (*env)->GetObjectClass(env, (jobject) h);
    jmethodID m = (*env)->GetMethodID(env, cls, "setVolume", "(FF)V");
    if (m) (*env)->CallVoidMethod(env, (jobject) h, m, (jfloat) v, (jfloat) v);
    jni_ok(env);
    (*env)->DeleteLocalRef(env, cls);
}

bool mp_isPlaying(void *h) {
    JNIEnv *env = env_get();
    if (!env || !h) return false;
    jclass cls = (*env)->GetObjectClass(env, (jobject) h);
    jmethodID m = (*env)->GetMethodID(env, cls, "isPlaying", "()Z");
    bool r = m ? (*env)->CallBooleanMethod(env, (jobject) h, m) : false;
    r = jni_ok(env) && r;
    (*env)->DeleteLocalRef(env, cls);
    return r;
}

static void mp_void(void *h, const char *name) {
    JNIEnv *env = env_get();
    if (!env || !h) return;
    jclass cls = (*env)->GetObjectClass(env, (jobject) h);
    jmethodID m = (*env)->GetMethodID(env, cls, name, "()V");
    if (m) (*env)->CallVoidMethod(env, (jobject) h, m);
    jni_ok(env);
    (*env)->DeleteLocalRef(env, cls);
}

void mp_start(void *h) { mp_void(h, "start"); }
void mp_pause(void *h) { mp_void(h, "pause"); }

void mp_release(void *h) {
    JNIEnv *env = env_get();
    if (!env || !h) return;
    mp_void(h, "stop");
    mp_void(h, "release");
    (*env)->DeleteGlobalRef(env, (jobject) h);
}

/* ================================================================== sound output (OpenSL ES) */
#define AFR 512
#define NBUF 3
static SLObjectItf slEngineObj, slMixObj, slPlayerObj;
static SLPlayItf slPlay;
static SLAndroidSimpleBufferQueueItf slQueue;
static short abuf[NBUF][AFR * 2];
static int acur;
static bool audioOn;

static void audio_cb(SLAndroidSimpleBufferQueueItf q, void *ctx) {
    (void) ctx;
    snd_mix(abuf[acur], AFR);
    (*q)->Enqueue(q, abuf[acur], sizeof abuf[acur]);
    acur = (acur + 1) % NBUF;
}

void plat_audio_start(void) {
    if (audioOn) return;
    SLEngineItf engine;
    if (slCreateEngine(&slEngineObj, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS) return;
    (*slEngineObj)->Realize(slEngineObj, SL_BOOLEAN_FALSE);
    (*slEngineObj)->GetInterface(slEngineObj, SL_IID_ENGINE, &engine);
    (*engine)->CreateOutputMix(engine, &slMixObj, 0, NULL, NULL);
    (*slMixObj)->Realize(slMixObj, SL_BOOLEAN_FALSE);
    SLDataLocator_AndroidSimpleBufferQueue lq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, NBUF};
    SLDataFormat_PCM fmt = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_22_05, SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT, SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource src = {&lq, &fmt};
    SLDataLocator_OutputMix lo = {SL_DATALOCATOR_OUTPUTMIX, slMixObj};
    SLDataSink snk = {&lo, NULL};
    const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    if ((*engine)->CreateAudioPlayer(engine, &slPlayerObj, &src, &snk, 1, ids, req) != SL_RESULT_SUCCESS) goto fail;
    (*slPlayerObj)->Realize(slPlayerObj, SL_BOOLEAN_FALSE);
    (*slPlayerObj)->GetInterface(slPlayerObj, SL_IID_PLAY, &slPlay);
    (*slPlayerObj)->GetInterface(slPlayerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &slQueue);
    (*slQueue)->RegisterCallback(slQueue, audio_cb, NULL);
    snd_setRunning(true);
    acur = 0;
    for (int i = 0; i < NBUF; i++) audio_cb(slQueue, NULL);
    (*slPlay)->SetPlayState(slPlay, SL_PLAYSTATE_PLAYING);
    audioOn = true;
    return;
fail:
    if (slPlayerObj) (*slPlayerObj)->Destroy(slPlayerObj);
    if (slMixObj) (*slMixObj)->Destroy(slMixObj);
    if (slEngineObj) (*slEngineObj)->Destroy(slEngineObj);
    slPlayerObj = slMixObj = slEngineObj = NULL;
}

void plat_audio_stop(void) {
    snd_setRunning(false);
    if (!audioOn) return;
    audioOn = false;
    if (slPlay) (*slPlay)->SetPlayState(slPlay, SL_PLAYSTATE_STOPPED);
    if (slPlayerObj) (*slPlayerObj)->Destroy(slPlayerObj);
    if (slMixObj) (*slMixObj)->Destroy(slMixObj);
    if (slEngineObj) (*slEngineObj)->Destroy(slEngineObj);
    slPlayerObj = slMixObj = slEngineObj = NULL; slPlay = NULL; slQueue = NULL;
    snd_clear();
}

/* ================================================================== WAD discovery */
static const char *const NAMES[] = {"doom1.wad", "doom.wad", "doom2.wad", "freedoom1.wad", "freedoom2.wad", "doomu.wad", "plutonia.wad", "tnt.wad"};
#define NNAMES ((int) (sizeof NAMES / sizeof NAMES[0]))

static u8 *read_file(const char *path, int *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    u8 *d = (u8 *) malloc((size_t) n);
    if (!d || fread(d, 1, (size_t) n, f) != (size_t) n) { fclose(f); free(d); return NULL; }
    fclose(f);
    *len = (int) n;
    return d;
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *) a, *(char *const *) b); }

static bool ends_wad(const char *n) { size_t l = strlen(n); return l >= 4 && strcasecmp(n + l - 4, ".wad") == 0; }

static u8 *from_dir(const char *dir, int *len) {
    if (!dir || !dir[0]) return NULL;
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char *names[256];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < 256) if (e->d_name[0] != '.') names[n++] = strdup(e->d_name);
    closedir(d);
    u8 *res = NULL;
    char path[1024];
    for (int k = 0; k < NNAMES && !res; k++)
        for (int i = 0; i < n && !res; i++)
            if (strcasecmp(names[i], NAMES[k]) == 0) { snprintf(path, sizeof path, "%s/%s", dir, names[i]); res = read_file(path, len); }
    if (!res) {
        qsort(names, (size_t) n, sizeof(char *), cmp_str);
        for (int i = 0; i < n && !res; i++) if (ends_wad(names[i])) { snprintf(path, sizeof path, "%s/%s", dir, names[i]); res = read_file(path, len); }
    }
    for (int i = 0; i < n; i++) free(names[i]);
    return res;
}

static u8 *read_asset(AAssetManager *am, const char *name, int *len) {
    AAsset *a = AAssetManager_open(am, name, AASSET_MODE_STREAMING);
    if (!a) return NULL;
    off_t n = AAsset_getLength(a);
    u8 *d = n > 0 ? (u8 *) malloc((size_t) n) : NULL;
    off_t got = 0;
    while (d && got < n) {
        int r = AAsset_read(a, d + got, (size_t) (n - got));
        if (r <= 0) break;
        got += r;
    }
    AAsset_close(a);
    if (!d || got != n) { free(d); return NULL; }
    *len = (int) n;
    return d;
}

static u8 *find_wad(int *len) {
    const char *dirs[2] = {App->activity->externalDataPath, App->activity->internalDataPath};
    for (int i = 0; i < 2; i++) { u8 *d = from_dir(dirs[i], len); if (d) return d; }
    AAssetManager *am = App->activity->assetManager;
    AAssetDir *ad = AAssetManager_openDir(am, "");
    if (!ad) return NULL;
    char *names[256];
    int n = 0;
    const char *nm;
    while ((nm = AAssetDir_getNextFileName(ad)) && n < 256) names[n++] = strdup(nm);
    AAssetDir_close(ad);
    u8 *res = NULL;
    for (int k = 0; k < NNAMES && !res; k++)
        for (int i = 0; i < n && !res; i++) if (strcasecmp(names[i], NAMES[k]) == 0) res = read_asset(am, names[i], len);
    for (int i = 0; i < n && !res; i++) if (ends_wad(names[i])) res = read_asset(am, names[i], len);
    for (int i = 0; i < n; i++) free(names[i]);
    return res;
}

/* ================================================================== status screen (before the WAD is loaded) */
static void present(void);

static void draw_status(void) {
    if (CV.w <= 0) return;
    cv_resetClip();
    cv_clear(0xFF000000u);
    float sc = fmaxf(2.0f, floorf(CV.w / 340.0f));
    float x0 = 24 * density, y = 24 * density;
    int perLine = (int) ((CV.w - 2 * x0) / (6 * sc));
    if (perLine < 8) perLine = 8;
    const char *p = status;
    while (*p) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int) (nl - p) : (int) strlen(p);
        int off = 0;
        do {
            int chunk = len - off > perLine ? perLine : len - off;
            char line[256];
            if (chunk > 255) chunk = 255;
            memcpy(line, p + off, (size_t) chunk); line[chunk] = 0;
            cv_miniText(line, x0, y, sc, 0xFFFFFFFFu);
            y += 10 * sc;
            off += chunk;
        } while (off < len);
        p += len + (nl ? 1 : 0);
        if (!nl) break;
    }
    present();
}

/* ================================================================== window / present */
static void present(void) {
    if (!Win || CV.w <= 0) return;
    ANativeWindow_Buffer b;
    if (ANativeWindow_lock(Win, &b, NULL) != 0) return;
    if (b.width == CV.w && b.height == CV.h && b.bits) {
        for (int y = 0; y < CV.h; y++) {
            const u32 *s = CV.px + y * CV.w;
            u32 *d = (u32 *) b.bits + y * b.stride;
            for (int x = 0; x < CV.w; x++) {
                u32 c = s[x];
                d[x] = (c & 0xFF00FF00u) | ((c >> 16) & 0xFFu) | ((c & 0xFFu) << 16);     /* ARGB -> RGBA bytes */
            }
        }
    }
    ANativeWindow_unlockAndPost(Win);
}

static void update_window_size(void) {
    if (!Win) return;
    int w = ANativeWindow_getWidth(Win), h = ANativeWindow_getHeight(Win);
    if (w <= 0 || h <= 0) return;
    if (w != winW || h != winH) {
        winW = w; winH = h;
        ANativeWindow_setBuffersGeometry(Win, 0, 0, WINDOW_FORMAT_RGBA_8888);
        if (loaded) gv_surfaceChanged(w, h); else cv_init(w, h);
    }
}

static void read_density(void) {
    int dpi = App->config ? AConfiguration_getDensity(App->config) : 0;
    if (dpi > 0 && dpi < 2000) density = dpi / 160.0f;
}

/* ================================================================== loading */
static void load_game(void) {
    loading = true;
    snprintf(status, sizeof status, "LOADING WAD...");
    draw_status();
    int len = 0;
    char err[256] = "";
    u8 *data = find_wad(&len);
    if (!data) {
        const char *ext = App->activity->externalDataPath;
        snprintf(status, sizeof status,
                 "WAD NOT FOUND.\n\nPUT DOOM1.WAD (OR DOOM.WAD / DOOM2.WAD) HERE:\n\n"
                 "  APP/SRC/MAIN/ASSETS/DOOM1.WAD  (BEFORE BUILDING)\n\nOR ON THE PHONE, IN THE FOLDER:\n\n%s\n\nTHEN TAP THE SCREEN.",
                 ext ? ext : "ANDROID/DATA/COM.C.C4DOOM/FILES");
        draw_status(); loading = false; return;
    }
    static Wad wad;
    if (!wad_load(&wad, data, len, err, sizeof err)) {
        snprintf(status, sizeof status, "ERROR: %s\n\nTAP THE SCREEN TO TRY AGAIN.", err);
        free(data); draw_status(); loading = false; return;
    }
    if (wad.nmaps == 0) {
        snprintf(status, sizeof status, "ERROR: THIS WAD CONTAINS NO MAPS (DOOM1.WAD / DOOM.WAD / DOOM2.WAD REQUIRED)\n\nTAP THE SCREEN TO TRY AGAIN.");
        draw_status(); loading = false; return;
    }
    Gfx *gfx = gfx_new(&wad, err, sizeof err);
    if (!gfx) {
        snprintf(status, sizeof status, "ERROR: %s\n\nTAP THE SCREEN TO TRY AGAIN.", err);
        draw_status(); loading = false; return;
    }
    gv_init(density);
    gv_surfaceChanged(winW, winH);
    gv_setWad(&wad, gfx);
    GV.paused = appPaused;
    if (!appPaused) plat_audio_start();
    loaded = true;
    loading = false;
    LOG("WAD loaded: %d lumps, %d maps", wad.n, wad.nmaps);
}

/* ================================================================== events */
static void on_cmd(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            Win = app->window;
            winW = winH = 0;
            read_density();
            update_window_size();
            hide_system_ui();
            if (!loaded && !loading) load_game();
            break;
        case APP_CMD_TERM_WINDOW:
            Win = NULL;
            winW = winH = 0;
            break;
        case APP_CMD_WINDOW_RESIZED: case APP_CMD_CONFIG_CHANGED: case APP_CMD_CONTENT_RECT_CHANGED:
            read_density();
            update_window_size();
            break;
        case APP_CMD_GAINED_FOCUS:
            hide_system_ui();
            break;
        case APP_CMD_PAUSE:
            appPaused = true;
            if (loaded) gv_onPause();
            break;
        case APP_CMD_RESUME:
            appPaused = false;
            if (loaded) gv_onResume();
            hide_system_ui();
            break;
        default: break;
    }
}

static int32_t on_input(struct android_app *app, AInputEvent *ev) {
    (void) app;
    int type = AInputEvent_getType(ev);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t a = AMotionEvent_getAction(ev);
        int act = a & AMOTION_EVENT_ACTION_MASK;
        int idx = (a & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        if (!loaded) {
            if (act == AMOTION_EVENT_ACTION_UP && !loading) load_game();      /* tap = retry */
            return 1;
        }
        int ids[16];
        float xs[16], ys[16];
        int n = (int) AMotionEvent_getPointerCount(ev);
        if (n > 16) n = 16;
        for (int i = 0; i < n; i++) {
            ids[i] = AMotionEvent_getPointerId(ev, (size_t) i);
            xs[i] = AMotionEvent_getX(ev, (size_t) i);
            ys[i] = AMotionEvent_getY(ev, (size_t) i);
        }
        if (idx >= n) idx = 0;
        int ta;
        switch (act) {
            case AMOTION_EVENT_ACTION_DOWN: ta = TA_DOWN; break;
            case AMOTION_EVENT_ACTION_UP: ta = TA_UP; break;
            case AMOTION_EVENT_ACTION_MOVE: ta = TA_MOVE; break;
            case AMOTION_EVENT_ACTION_CANCEL: ta = TA_CANCEL; break;
            case AMOTION_EVENT_ACTION_POINTER_DOWN: ta = TA_POINTER_DOWN; break;
            case AMOTION_EVENT_ACTION_POINTER_UP: ta = TA_POINTER_UP; break;
            default: return 1;
        }
        gv_touch(ta, idx, n, ids, xs, ys);
        return 1;
    }
    if (type == AINPUT_EVENT_TYPE_KEY && AKeyEvent_getKeyCode(ev) == AKEYCODE_BACK) {
        if (!loaded) return 0;
        bool canHandle = GV.menuOpen || !GV.titleMode;       /* on the title screen the system closes the app */
        if (!canHandle) return 0;
        if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) gv_onBack();
        return 1;
    }
    return 0;
}

/* ================================================================== main */
void android_main(struct android_app *app) {
    App = app;
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    env_get();
    for (;;) {
        int events;
        struct android_poll_source *source;
        while (ALooper_pollOnce((Win && !appPaused) ? 0 : -1, NULL, &events, (void **) &source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) {
                if (loaded) { gv_savePrefs(); plat_audio_stop(); }
                return;
            }
        }
        if (!Win || appPaused) continue;
        if (!loaded) { usleep(30000); continue; }
        int64_t t0 = plat_nanotime();
        update_window_size();
        gv_frame();
        present();
        int64_t used = plat_nanotime() - t0;
        if (used < 16000000LL) usleep((useconds_t) ((16000000LL - used) / 1000));
    }
}