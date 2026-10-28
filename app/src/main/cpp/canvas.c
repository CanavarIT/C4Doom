/* Small software 2D canvas: fills, anti-aliased circles / lines / rounded rects, scaled patches, text. */
#include "doom.h"

Canvas CV;

void cv_init(int w, int h) {
    if (w == CV.w && h == CV.h && CV.px) { cv_resetClip(); return; }
    free(CV.px);
    CV.px = (u32 *) calloc((size_t) w * h, sizeof(u32));
    CV.w = w; CV.h = h;
    cv_resetClip();
}

void cv_setClip(int x0, int y0, int x1, int y1) {
    CV.cx0 = maxi(0, x0); CV.cy0 = maxi(0, y0); CV.cx1 = mini(CV.w, x1); CV.cy1 = mini(CV.h, y1);
}
void cv_resetClip(void) { CV.cx0 = 0; CV.cy0 = 0; CV.cx1 = CV.w; CV.cy1 = CV.h; }

void cv_clear(u32 argb) {
    for (int i = 0, n = CV.w * CV.h; i < n; i++) CV.px[i] = argb | 0xFF000000u;
}

static inline u32 blend(u32 d, u32 s, u32 a) {
    if (a >= 255) return 0xFF000000u | (s & 0xFFFFFFu);
    if (a == 0) return d;
    u32 ia = 255 - a;
    u32 rb = (d & 0x00FF00FFu) * ia + (s & 0x00FF00FFu) * a + 0x00800080u;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    u32 tg = ((d >> 8) & 0xFF) * ia + ((s >> 8) & 0xFF) * a + 128;
    u32 g = (((tg + (tg >> 8)) >> 8) & 0xFF) << 8;
    return 0xFF000000u | rb | g;
}

static inline void put(int x, int y, u32 argb, float cov) {
    if (x < CV.cx0 || x >= CV.cx1 || y < CV.cy0 || y >= CV.cy1) return;
    u32 a = (u32) ((float) (argb >> 24) * cov + 0.5f);
    CV.px[y * CV.w + x] = blend(CV.px[y * CV.w + x], argb, a);
}

void cv_rect(float x0, float y0, float x1, float y1, u32 argb) {
    int ix0 = maxi(CV.cx0, jround(x0)), iy0 = maxi(CV.cy0, jround(y0)), ix1 = mini(CV.cx1, jround(x1)), iy1 = mini(CV.cy1, jround(y1));
    u32 a = argb >> 24;
    for (int y = iy0; y < iy1; y++) {
        u32 *row = CV.px + y * CV.w;
        for (int x = ix0; x < ix1; x++) row[x] = blend(row[x], argb, a);
    }
}

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

void cv_circleFill(float cx, float cy, float r, u32 argb) {
    int x0 = (int) floorf(cx - r - 1), x1 = (int) ceilf(cx + r + 1), y0 = (int) floorf(cy - r - 1), y1 = (int) ceilf(cy + r + 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float cov = clamp01(r - sqrtf(dx * dx + dy * dy) + 0.5f);
            if (cov > 0) put(x, y, argb, cov);
        }
}

void cv_circleStroke(float cx, float cy, float r, float w, u32 argb) {
    float o = r + w * 0.5f;
    int x0 = (int) floorf(cx - o - 1), x1 = (int) ceilf(cx + o + 1), y0 = (int) floorf(cy - o - 1), y1 = (int) ceilf(cy + o + 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float cov = clamp01(w * 0.5f - fabsf(sqrtf(dx * dx + dy * dy) - r) + 0.5f);
            if (cov > 0) put(x, y, argb, cov);
        }
}

void cv_line(float x0, float y0, float x1, float y1, float w, u32 argb) {
    float hw = w * 0.5f;
    int bx0 = (int) floorf(minf(x0, x1) - hw - 1), bx1 = (int) ceilf(maxf(x0, x1) + hw + 1);
    int by0 = (int) floorf(minf(y0, y1) - hw - 1), by1 = (int) ceilf(maxf(y0, y1) + hw + 1);
    float vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
    for (int y = by0; y <= by1; y++)
        for (int x = bx0; x <= bx1; x++) {
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float t = len2 > 0 ? (px * vx + py * vy) / len2 : 0;
            t = clamp01(t);
            float dx = px - vx * t, dy = py - vy * t;
            float cov = clamp01(hw - sqrtf(dx * dx + dy * dy) + 0.5f);
            if (cov > 0) put(x, y, argb, cov);
        }
}

static float rrDist(float px, float py, float x0, float y0, float x1, float y1, float rad) {
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = (x1 - x0) * 0.5f - rad, hy = (y1 - y0) * 0.5f - rad;
    float qx = fabsf(px - cx) - hx, qy = fabsf(py - cy) - hy;
    float ox = maxf(qx, 0), oy = maxf(qy, 0);
    return sqrtf(ox * ox + oy * oy) + minf(maxf(qx, qy), 0) - rad;
}

void cv_rrectFill(float x0, float y0, float x1, float y1, float rad, u32 argb) {
    for (int y = (int) floorf(y0) - 1; y <= (int) ceilf(y1) + 1; y++)
        for (int x = (int) floorf(x0) - 1; x <= (int) ceilf(x1) + 1; x++) {
            float cov = clamp01(0.5f - rrDist(x + 0.5f, y + 0.5f, x0, y0, x1, y1, rad));
            if (cov > 0) put(x, y, argb, cov);
        }
}

void cv_rrectStroke(float x0, float y0, float x1, float y1, float rad, float w, u32 argb) {
    for (int y = (int) floorf(y0 - w) - 1; y <= (int) ceilf(y1 + w) + 1; y++)
        for (int x = (int) floorf(x0 - w) - 1; x <= (int) ceilf(x1 + w) + 1; x++) {
            float cov = clamp01(w * 0.5f - fabsf(rrDist(x + 0.5f, y + 0.5f, x0, y0, x1, y1, rad)) + 0.5f);
            if (cov > 0) put(x, y, argb, cov);
        }
}

void cv_triFill(float x0, float y0, float x1, float y1, float x2, float y2, u32 argb) {
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (area == 0) return;
    float sgn = area > 0 ? 1.0f : -1.0f;
    float px[3] = {x0, x1, x2}, py[3] = {y0, y1, y2};
    int bx0 = (int) floorf(minf(x0, minf(x1, x2))) - 1, bx1 = (int) ceilf(maxf(x0, maxf(x1, x2))) + 1;
    int by0 = (int) floorf(minf(y0, minf(y1, y2))) - 1, by1 = (int) ceilf(maxf(y0, maxf(y1, y2))) + 1;
    for (int y = by0; y <= by1; y++)
        for (int x = bx0; x <= bx1; x++) {
            float best = 1e9f;
            for (int i = 0; i < 3; i++) {
                int j = (i + 1) % 3;
                float ex = px[j] - px[i], ey = py[j] - py[i];
                float el = sqrtf(ex * ex + ey * ey);
                float d = sgn * (ex * (y + 0.5f - py[i]) - ey * (x + 0.5f - px[i])) / (el > 0 ? el : 1);
                if (d < best) best = d;
            }
            float cov = clamp01(best + 0.5f);
            if (cov > 0) put(x, y, argb, cov);
        }
}

/* Scales an ARGB image into the destination rectangle; bilinear when smooth. */
void cv_blitView(const u32 *src, int sw, int sh, int dx, int dy, int dw, int dh, bool smooth) {
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    int ox0 = maxi(dx, CV.cx0), ox1 = mini(dx + dw, CV.cx1), oy0 = maxi(dy, CV.cy0), oy1 = mini(dy + dh, CV.cy1);
    if (ox0 >= ox1 || oy0 >= oy1) return;
    int n = ox1 - ox0;
    int *xi0 = (int *) malloc((size_t) n * sizeof(int) * 2);
    int *xi1 = xi0 + n;
    u8 *xw = (u8 *) malloc((size_t) n);
    for (int i = 0; i < n; i++) {
        float fx = ((ox0 + i - dx) + 0.5f) * sw / dw;
        if (smooth) {
            fx -= 0.5f;
            int a = (int) floorf(fx);
            float w = fx - a;
            xi0[i] = clampi(a, 0, sw - 1); xi1[i] = clampi(a + 1, 0, sw - 1); xw[i] = (u8) (w * 256 > 255 ? 255 : w * 256);
        } else { xi0[i] = xi1[i] = clampi((int) fx, 0, sw - 1); xw[i] = 0; }
    }
    u32 *tmp = (u32 *) malloc((size_t) sw * sizeof(u32));
    for (int y = oy0; y < oy1; y++) {
        float fy = ((y - dy) + 0.5f) * sh / dh;
        u32 *out = CV.px + y * CV.w + ox0;
        if (!smooth) {
            const u32 *row = src + clampi((int) fy, 0, sh - 1) * sw;
            for (int i = 0; i < n; i++) out[i] = row[xi0[i]];
            continue;
        }
        fy -= 0.5f;
        int a = (int) floorf(fy);
        float wy = fy - a;
        u32 w8 = (u32) (wy * 256 > 255 ? 255 : wy * 256);
        const u32 *r0 = src + clampi(a, 0, sh - 1) * sw, *r1 = src + clampi(a + 1, 0, sh - 1) * sw;
        for (int i = 0; i < sw; i++) {
            u32 p = r0[i], q = r1[i];
            u32 rb = (((p & 0x00FF00FFu) * (256 - w8) + (q & 0x00FF00FFu) * w8) >> 8) & 0x00FF00FFu;
            u32 g = (((p & 0x0000FF00u) * (256 - w8) + (q & 0x0000FF00u) * w8) >> 8) & 0x0000FF00u;
            tmp[i] = 0xFF000000u | rb | g;
        }
        for (int i = 0; i < n; i++) {
            u32 p = tmp[xi0[i]], q = tmp[xi1[i]], w = xw[i];
            u32 rb = (((p & 0x00FF00FFu) * (256 - w) + (q & 0x00FF00FFu) * w) >> 8) & 0x00FF00FFu;
            u32 g = (((p & 0x0000FF00u) * (256 - w) + (q & 0x0000FF00u) * w) >> 8) & 0x0000FF00u;
            out[i] = 0xFF000000u | rb | g;
        }
    }
    free(tmp); free(xi0); free(xw);
}

/* Nearest-neighbour patch draw into the rectangle [x0,x1) x [y0,y1). tint != 0 uses the patch as an alpha mask. */
void cv_drawPic(const Pic *p, const u32 *lit0, int x0, int y0, int x1, int y1, u32 tint) {
    int dw = x1 - x0, dh = y1 - y0;
    if (dw <= 0 || dh <= 0) return;
    int sx0 = maxi(x0, CV.cx0), sx1 = mini(x1, CV.cx1), sy0 = maxi(y0, CV.cy0), sy1 = mini(y1, CV.cy1);
    if (sx0 >= sx1 || sy0 >= sy1) return;
    int n = sx1 - sx0;
    int *xm = (int *) malloc((size_t) n * sizeof(int));
    for (int i = 0; i < n; i++) xm[i] = mini(p->w - 1, (int) (((2LL * (sx0 + i - x0) + 1) * p->w) / (2LL * dw)));
    u32 ta = tint >> 24;
    for (int y = sy0; y < sy1; y++) {
        int ty = mini(p->h - 1, (int) (((2LL * (y - y0) + 1) * p->h) / (2LL * dh)));
        u32 *row = CV.px + y * CV.w;
        for (int i = 0; i < n; i++) {
            int k = xm[i] * p->h + ty;
            if (!p->op[k]) continue;
            if (tint) {
                u32 c = lit0 ? lit0[p->pix[k]] : 0xFFFFFFFFu;     /* keep the font's own shading, recoloured with the tint */
                int l = (int) ((((c >> 16) & 255) * 77 + ((c >> 8) & 255) * 151 + (c & 255) * 28) >> 8);
                u32 b = lit0 ? (u32) clampi(l * 4 + 70, 0, 255) : 255u;
                u32 t = ((((tint >> 16) & 255) * b / 255) << 16) | ((((tint >> 8) & 255) * b / 255) << 8) | ((tint & 255) * b / 255);
                row[sx0 + i] = blend(row[sx0 + i], t, ta);
            }
            else row[sx0 + i] = lit0[p->pix[k]];
        }
    }
    free(xm);
}

/* Fills everything except the rectangle with a solid colour. */
void cv_fillOutside(int ex0, int ey0, int ex1, int ey1, u32 argb) {
    for (int y = 0; y < CV.h; y++) {
        u32 *row = CV.px + y * CV.w;
        if (y >= ey0 && y < ey1 && ex0 < ex1) {
            for (int x = 0; x < mini(ex0, CV.w); x++) row[x] = argb;
            for (int x = maxi(ex1, 0); x < CV.w; x++) row[x] = argb;
        } else for (int x = 0; x < CV.w; x++) row[x] = argb;
    }
}

/* Tiles the 64x64 flat (scaled) over the screen, except the rectangle. */
void cv_tileFlat(const u8 *flat, const u32 *lit0, float scx, float scy, int ex0, int ey0, int ex1, int ey1) {
    int *xm = (int *) malloc((size_t) CV.w * sizeof(int));
    for (int x = 0; x < CV.w; x++) xm[x] = ((int) floorf(x / scx)) & 63;
    for (int y = 0; y < CV.h; y++) {
        const u8 *fr = flat + (((int) floorf(y / scy)) & 63) * 64;
        u32 *row = CV.px + y * CV.w;
        if (y >= ey0 && y < ey1 && ex0 < ex1) {
            for (int x = 0; x < mini(ex0, CV.w); x++) row[x] = lit0[fr[xm[x]]];
            for (int x = maxi(ex1, 0); x < CV.w; x++) row[x] = lit0[fr[xm[x]]];
        } else for (int x = 0; x < CV.w; x++) row[x] = lit0[fr[xm[x]]];
    }
    free(xm);
}

/* ------------------------------------------------------------------ Doom font text for overlays */
static Pic *glyphPic(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = (char) (ch - 32);
    if ((u8) ch < 33 || (u8) ch > 95) return NULL;
    char nm[12];
    snprintf(nm, sizeof nm, "STCFN%03d", (int) ch);
    return gfx_patch(G_gfx, nm);
}

void cv_text(const char *s, float x, float y, float size, u32 argb, int align) {
    Pic *hp = glyphPic('H');
    float capH = hp ? (float) hp->h : 7.0f;
    float k = size * 0.72f / capH;
    float w = 0;
    for (const char *c = s; *c; c++) { Pic *g = glyphPic(*c); w += g ? g->w * k : 4 * k; }
    if (align == AL_CENTER) x -= w * 0.5f; else if (align == AL_RIGHT) x -= w;
    for (int pass = 0; pass < 2; pass++) {
        float xx = x + (pass == 0 ? size * 0.07f : 0), yy = y + (pass == 0 ? size * 0.07f : 0);
        u32 col = pass == 0 ? 0xCC000000u : argb;
        for (const char *c = s; *c; c++) {
            Pic *g = glyphPic(*c);
            if (!g) { xx += 4 * k; continue; }
            /* glyph top: STCFN patches have topoffset = 7 for caps; align the glyph bottom to the baseline */
            float top = yy - g->h * k, left = xx;
            cv_drawPic(g, G_gfx->lit[0], jround(left), jround(top), jround(left + g->w * k), jround(yy), col);
            xx += g->w * k;
        }
    }
}

/* ------------------------------------------------------------------ built-in 5x7 font (used before a WAD is loaded) */
static const u8 FONT5[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00}, {0x41, 0x22, 0x14, 0x08, 0x00}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x40, 0x40, 0x40, 0x40, 0x40}};

static int fontIndex(char c) {
    if (c >= 'a' && c <= 'z') c = (char) (c - 32);
    switch (c) {
        case ' ': return 0; case '!': return 1; case '-': return 2; case '.': return 3; case '/': return 4;
        case ':': return 15; case '<': return 16; case '>': return 17; case '?': return 18;
        case '(': return 45; case ')': return 46; case '_': return 47;
        default: break;
    }
    if (c >= '0' && c <= '9') return 5 + (c - '0');
    if (c >= 'A' && c <= 'Z') return 19 + (c - 'A');
    return 18;
}

void cv_miniText(const char *s, float x, float y, float scale, u32 argb) {
    for (const char *c = s; *c; c++) {
        if (*c == '\n') continue;
        const u8 *g = FONT5[fontIndex(*c)];
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if ((g[col] >> row) & 1)
                    cv_rect(x + col * scale, y + row * scale, x + (col + 1) * scale, y + (row + 1) * scale, argb);
        x += 6 * scale;
    }
}
