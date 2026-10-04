#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <dwmapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <math.h>
#include "LineacEngine.h"
#include "BindManager.h"
#include "WindowSelector.h"
#include "Console.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' "                 \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "             \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace G = Gdiplus;

// ---- geometry -------------------------------------------------------------
// One fixed-size window: header, a paged body, footer. Pages are switched with
// the dots in the footer (or the arrow keys), the gear opens a settings sheet.

#define WIN_W      700
#define HDR_H      38
#define FT_H       40
#define WIN_H      420
#define BODY_TOP   (HDR_H + 10)
#define BODY_BOT   (WIN_H - FT_H)
#define PAD_X      22          // body side padding
#define COL_GAP    32          // gap between columns; a divider sits in its middle
#define COL_L0     PAD_X       // used by the settings sheet
#define IND        24          // label indent past the row icon
#define ROW_H      32
#define BAR_H      20
#define TIMER_TICK 1
#define TIMER_ANIM 2
#define TICK_MS    33
#define ANIM_MS    16
#define WM_APP_EDITDONE (WM_APP + 1)

#define C_BG       RGB(0x0b,0x0c,0x0f)
#define C_EDGE     RGB(0x1a,0x1c,0x22)
#define C_TEXT     RGB(0xf2,0xf4,0xf8)
#define C_SOFT     RGB(0xc9,0xcd,0xd6)
#define C_MUTED    RGB(0x6b,0x70,0x80)
#define C_DIM      RGB(0x4a,0x4e,0x5a)
#define C_TRACK    RGB(0x1e,0x20,0x27)
#define C_TOG_OFF  RGB(0x24,0x26,0x2e)
#define C_DOT      RGB(0x2a,0x2d,0x35)
#define C_DOT_HOT  RGB(0x3a,0x3e,0x48)
#define C_BTN      RGB(0x1a,0x1c,0x22)
#define C_BTN_HOT  RGB(0x24,0x26,0x2e)
#define C_FIELD    RGB(0x16,0x18,0x1d)
#define C_ACCENT   RGB(0x1e,0x9b,0xff)

// ---- state ----------------------------------------------------------------

enum { U_NONE, U_MS, U_PCT, U_LIMIT };

// Every numeric setting: the slider, the stepper and the typed value all edit
// the same record, and Apply reads it.
struct Num { double v, mn, mx, step; int unit; };

static Num nL      = { 20.0,  0, 100, 1, U_NONE };
static Num nR      = { 20.0,  0, 100, 1, U_NONE };
static Num nHigh   = { 20.0,  0, 100, 1, U_NONE };
static Num nBps    = { 10.0,  0, 100, 1, U_NONE };
static Num nDur    = { 5.0,   0, 100, 1, U_MS   };
static Num nChance = { 100.0, 0, 100, 1, U_PCT  };
static Num nStr    = { 55.0,  0, 100, 1, U_PCT  };
static Num nLimit  = { 0.0,   0, 100, 1, U_LIMIT };

static bool g_allowAll = false;
static HWND g_targets[MAX_TARGETS];
static int  g_targetCount = 0;
static int  g_bindL = 0, g_bindR = 0, g_bindHighCps = 0, g_bindBlockPause = 0;
static int  g_patternSel = 0;
static int  g_modeSel    = 0;
static int  g_blockPauseModeSel = 0;
static bool g_blockHit    = false;
static bool g_highCps     = false;
static bool g_showConsole = false;

static int  g_page     = 0;
static int  g_pageDir  = 0;
static bool g_gearOpen = false;
static int* g_listen   = NULL;         // bind currently being captured
static DWORD g_flashUntil = 0;
static bool g_flashShown  = false;
static bool g_lastActive  = false;
static bool g_dwmRound = false;

struct Anim { double cur, target; };
static Anim g_aAllow   = { 0.0, 0.0 };
static Anim g_aBlock   = { 0.0, 0.0 };
static Anim g_aHigh    = { 0.0, 0.0 };
static Anim g_aConsole = { 0.0, 0.0 };
static Anim g_aGear    = { 0.0, 0.0 };
static Anim g_aPage    = { 1.0, 1.0 };
static Anim* const g_anims[] = { &g_aAllow, &g_aBlock, &g_aHigh, &g_aConsole, &g_aGear, &g_aPage };

static HWND    g_hwnd  = NULL;
static HWND    g_hEdit = NULL;          // inline editor for a typed value
static Num*    g_editNum = NULL;
static WNDPROC g_oldEditProc = NULL;
static HHOOK   g_rmbHook = NULL;
static HDC     g_measure = NULL;
static ULONG_PTR g_gdipToken = 0;

static HFONT g_fTitle, g_fPage, g_fLabel, g_fCtl, g_fSmall, g_fStat, g_fHead, g_fSection;
static HBRUSH g_fieldBrush;

static const wchar_t* PAGES[] = { L"Clicking", L"Pattern & BlockHit" };
#define PAGE_COUNT 2

static const wchar_t* PATTERN_ITEMS[] = { L"Legit", L"Blatant", L"Custom" };
static const wchar_t* MODE_ITEMS[]    = { L"Hold",  L"Toggle" };

// ---- small helpers --------------------------------------------------------

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }

static COLORREF Lerp(COLORREF a, COLORREF b, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t + 0.5);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t + 0.5);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t + 0.5);
    return RGB(r, g, bl);
}

static G::Color GC(COLORREF c, BYTE a = 255) {
    return G::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

// GDI+ for every shape (it antialiases), GDI for text (it does ClearType).
struct Gfx {
    G::Graphics g;
    explicit Gfx(HDC dc) : g(dc) {
        g.SetSmoothingMode(G::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(G::PixelOffsetModeHalf);
    }
};

static void RoundPath(G::GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}
static void FillRound(HDC dc, RECT rc, float r, COLORREF c, BYTE a = 255) {
    Gfx x(dc);
    G::GraphicsPath p;
    RoundPath(p, (float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top), r);
    G::SolidBrush b(GC(c, a));
    x.g.FillPath(&b, &p);
}
static void FrameRound(HDC dc, RECT rc, float r, COLORREF c) {
    Gfx x(dc);
    G::GraphicsPath p;
    RoundPath(p, rc.left + 0.5f, rc.top + 0.5f, (float)(rc.right - rc.left) - 1, (float)(rc.bottom - rc.top) - 1, r);
    G::Pen pen(GC(c), 1.0f);
    x.g.DrawPath(&pen, &p);
}
static void FillCircle(HDC dc, float cx, float cy, float r, COLORREF c, BYTE a = 255) {
    Gfx x(dc);
    G::SolidBrush b(GC(c, a));
    x.g.FillEllipse(&b, cx - r, cy - r, r * 2, r * 2);
}
static void FillBox(HDC dc, RECT rc, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &rc, b);
    DeleteObject(b);
}

static void Txt(HDC dc, const wchar_t* s, RECT rc, COLORREF c, HFONT f, UINT fmt) {
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &rc, fmt | DT_NOPREFIX);
    SelectObject(dc, of);
}
static int TextW(HFONT f, const wchar_t* s) {
    HGDIOBJ of = SelectObject(g_measure, f);
    SIZE sz; GetTextExtentPoint32W(g_measure, s, lstrlenW(s), &sz);
    SelectObject(g_measure, of);
    return sz.cx;
}
static int TextH(HFONT f, const wchar_t* s, int w) {
    RECT rc = R(0, 0, w, 0);
    HGDIOBJ of = SelectObject(g_measure, f);
    DrawTextW(g_measure, s, -1, &rc, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(g_measure, of);
    return rc.bottom;
}

// wsprintfW has no float support, so numbers go through integer hundredths.
static void FmtDec(double v, wchar_t* out, int maxDec) {
    if (v < 0) v = 0;
    if (maxDec == 1) {
        int t = (int)(v * 10.0 + 0.5);
        if (t % 10 == 0) wsprintfW(out, L"%d", t / 10);
        else             wsprintfW(out, L"%d.%d", t / 10, t % 10);
        return;
    }
    int h = (int)(v * 100.0 + 0.5);
    if (h % 100 == 0)     wsprintfW(out, L"%d", h / 100);
    else if (h % 10 == 0) wsprintfW(out, L"%d.%d", h / 100, (h % 100) / 10);
    else                  wsprintfW(out, L"%d.%02d", h / 100, h % 100);
}
static void FmtNum(const Num* n, wchar_t* out) {
    if (n->unit == U_LIMIT && n->v <= 0.0) { lstrcpyW(out, L"off"); return; }
    FmtDec(n->v, out, 2);
    if (n->unit == U_MS)  lstrcatW(out, L" ms");
    if (n->unit == U_PCT) lstrcatW(out, L"%");
}
static void SetNum(Num* n, double v) {
    if (v < n->mn) v = n->mn;
    if (v > n->mx) v = n->mx;
    n->v = floor(v * 100.0 + 0.5) / 100.0;
}

// ---- icons ----------------------------------------------------------------
// Lucide outlines, kept as their SVG path data and parsed once into GDI+
// paths, so they render exactly like the prototype at any size.

enum { IC_POWER, IC_CROSSHAIR, IC_LINK, IC_REPEAT, IC_ACTIVITY, IC_ZAP, IC_CLOCK,
       IC_TARGET, IC_APPWIN, IC_TERMINAL, IC_GEAR, IC_SHIELD, IC_PAUSE, IC_TIMER,
       IC_WAVES, IC_MOVE, IC_CAP, IC_LAYERS, IC_MINUS, IC_X, IC_COUNT };

static const char* ICON_SVG[IC_COUNT] = {
    "M12 2v10M18.4 6.6a9 9 0 1 1-12.77.04",
    "M2 12a10 10 0 1 0 20 0a10 10 0 1 0-20 0M22 12h-4M6 12H2M12 6V2M12 22v-4",
    "M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"
    "M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71",
    "M17 2l4 4-4 4M3 11v-1a4 4 0 0 1 4-4h14M7 22l-4-4 4-4M21 13v1a4 4 0 0 1-4 4H3",
    "M22 12h-4l-3 9L9 3l-3 9H2",
    "M13 2 3 14h9l-1 8 10-12h-9l1-8z",
    "M2 12a10 10 0 1 0 20 0a10 10 0 1 0-20 0M12 6v6l4 2",
    "M2 12a10 10 0 1 0 20 0a10 10 0 1 0-20 0M6 12a6 6 0 1 0 12 0a6 6 0 1 0-12 0"
    "M10 12a2 2 0 1 0 4 0a2 2 0 1 0-4 0",
    "M4 4h16a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2zM10 4v4M2 8h20M6 4v4",
    "M4 17l6-6-6-6M12 19h8",
    "M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73"
    "l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73"
    "l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44"
    "a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39"
    "a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73"
    "l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z"
    "M9 12a3 3 0 1 0 6 0a3 3 0 1 0-6 0",
    "M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z",
    "M15 4h2a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1h-2a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1z"
    "M7 4h2a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1H7a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1z",
    "M10 2h4M12 14l3-3M4 14a8 8 0 1 0 16 0a8 8 0 1 0-16 0",
    "M2 6c.6.5 1.2 1 2.5 1C7 7 7 5 9.5 5c2.6 0 2.4 2 5 2 2.5 0 2.5-2 5-2 1.3 0 1.9.5 2.5 1"
    "M2 12c.6.5 1.2 1 2.5 1 2.5 0 2.5-2 5-2 2.6 0 2.4 2 5 2 2.5 0 2.5-2 5-2 1.3 0 1.9.5 2.5 1"
    "M2 18c.6.5 1.2 1 2.5 1 2.5 0 2.5-2 5-2 2.6 0 2.4 2 5 2 2.5 0 2.5-2 5-2 1.3 0 1.9.5 2.5 1",
    "M18 8l4 4-4 4M2 12h20M6 8l-4 4 4 4",
    "M5 3h14M18 13l-6-6-6 6M12 7v14",
    "M12 2l10 5-10 5L2 7zM2 17l10 5 10-5M2 12l10 5 10-5",
    "M5 12h14",
    "M18 6 6 18M6 6l12 12",
};
static G::GraphicsPath* g_icons[IC_COUNT];

static void ArcTo(G::GraphicsPath& p, float x1, float y1, float rx, float ry,
                  bool large, bool sweep, float x2, float y2) {
    const double PI = 3.14159265358979;
    rx = fabsf(rx); ry = fabsf(ry);
    if (rx == 0 || ry == 0) { p.AddLine(x1, y1, x2, y2); return; }
    double xp = (x1 - x2) / 2.0, yp = (y1 - y2) / 2.0;
    double lam = xp * xp / (rx * rx) + yp * yp / (ry * ry);
    if (lam > 1) { rx *= (float)sqrt(lam); ry *= (float)sqrt(lam); }
    double num = (double)rx * rx * ry * ry - (double)rx * rx * yp * yp - (double)ry * ry * xp * xp;
    double den = (double)rx * rx * yp * yp + (double)ry * ry * xp * xp;
    double co = den > 0 ? sqrt(num / den > 0 ? num / den : 0) : 0;
    if (large == sweep) co = -co;
    double cxp = co * rx * yp / ry, cyp = -co * ry * xp / rx;
    double cx = cxp + (x1 + x2) / 2.0, cy = cyp + (y1 + y2) / 2.0;
    double t1 = atan2((yp - cyp) / ry, (xp - cxp) / rx);
    double t2 = atan2((-yp - cyp) / ry, (-xp - cxp) / rx);
    double dt = t2 - t1;
    if (!sweep && dt > 0) dt -= 2 * PI;
    if (sweep && dt < 0)  dt += 2 * PI;
    p.AddArc((float)(cx - rx), (float)(cy - ry), rx * 2, ry * 2,
             (float)(t1 * 180 / PI), (float)(dt * 180 / PI));
}

// Enough of the SVG path grammar for Lucide: M L H V C S A Z, both cases,
// implicit repeats and packed numbers like "1.5.5" or "2-1.73".
static void ParsePath(G::GraphicsPath& p, const char* s) {
    float cx = 0, cy = 0, sx = 0, sy = 0, lcx = 0, lcy = 0;
    char cmd = 0, prev = 0;
    auto skip = [&]() { while (*s == ' ' || *s == ',') ++s; };
    auto num  = [&](float& out) -> bool {
        skip(); char* e; out = strtof(s, &e);
        if (e == s) return false;
        s = e; return true;
    };
    for (;;) {
        skip();
        if (!*s) break;
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) cmd = *s++;
        else if (!cmd) break;
        bool rel = (cmd >= 'a');
        char up = rel ? (char)(cmd - 32) : cmd;
        float a[7];
        switch (up) {
        case 'M':
            if (!num(a[0]) || !num(a[1])) return;
            if (rel) { a[0] += cx; a[1] += cy; }
            p.StartFigure();
            cx = sx = a[0]; cy = sy = a[1];
            cmd = rel ? 'l' : 'L';
            break;
        case 'L':
            if (!num(a[0]) || !num(a[1])) return;
            if (rel) { a[0] += cx; a[1] += cy; }
            p.AddLine(cx, cy, a[0], a[1]); cx = a[0]; cy = a[1];
            break;
        case 'H':
            if (!num(a[0])) return;
            if (rel) a[0] += cx;
            p.AddLine(cx, cy, a[0], cy); cx = a[0];
            break;
        case 'V':
            if (!num(a[0])) return;
            if (rel) a[0] += cy;
            p.AddLine(cx, cy, cx, a[0]); cy = a[0];
            break;
        case 'C':
            for (int i = 0; i < 6; ++i) if (!num(a[i])) return;
            if (rel) for (int i = 0; i < 6; i += 2) { a[i] += cx; a[i + 1] += cy; }
            p.AddBezier(cx, cy, a[0], a[1], a[2], a[3], a[4], a[5]);
            lcx = a[2]; lcy = a[3]; cx = a[4]; cy = a[5];
            break;
        case 'S': {
            for (int i = 0; i < 4; ++i) if (!num(a[i])) return;
            if (rel) for (int i = 0; i < 4; i += 2) { a[i] += cx; a[i + 1] += cy; }
            float c1x = cx, c1y = cy;
            if (prev == 'C' || prev == 'S') { c1x = 2 * cx - lcx; c1y = 2 * cy - lcy; }
            p.AddBezier(cx, cy, c1x, c1y, a[0], a[1], a[2], a[3]);
            lcx = a[0]; lcy = a[1]; cx = a[2]; cy = a[3];
            break;
        }
        case 'A':
            for (int i = 0; i < 7; ++i) if (!num(a[i])) return;
            if (rel) { a[5] += cx; a[6] += cy; }
            ArcTo(p, cx, cy, a[0], a[1], a[3] != 0, a[4] != 0, a[5], a[6]);
            cx = a[5]; cy = a[6];
            break;
        case 'Z':
            p.CloseFigure(); cx = sx; cy = sy;
            break;
        default:
            return;
        }
        prev = up;
    }
}

static void DrawIcon(HDC dc, int icon, float x, float y, float size, COLORREF c) {
    Gfx gx(dc);
    gx.g.TranslateTransform(x, y);
    gx.g.ScaleTransform(size / 24.0f, size / 24.0f);
    G::Pen pen(GC(c), 1.7f);
    pen.SetLineCap(G::LineCapRound, G::LineCapRound, G::DashCapRound);
    pen.SetLineJoin(G::LineJoinRound);
    gx.g.DrawPath(&pen, g_icons[icon]);
}

// The LineAC mark: an "L" stroke ending in a click node with a faint ripple.
static void DrawMark(HDC dc, float x, float y, float size) {
    Gfx gx(dc);
    gx.g.TranslateTransform(x, y);
    gx.g.ScaleTransform(size / 96.0f, size / 96.0f);
    G::Pen ring(GC(C_ACCENT, 90), 4.0f);
    gx.g.DrawEllipse(&ring, 47.0f, 43.0f, 30.0f, 30.0f);
    G::Pen line(GC(C_ACCENT), 11.0f);
    line.SetLineCap(G::LineCapRound, G::LineCapRound, G::DashCapRound);
    line.SetLineJoin(G::LineJoinRound);
    G::PointF pts[3] = { G::PointF(30, 22), G::PointF(30, 58), G::PointF(62, 58) };
    gx.g.DrawLines(&line, pts, 3);
    G::SolidBrush node(GC(C_ACCENT));
    gx.g.FillEllipse(&node, 54.0f, 50.0f, 16.0f, 16.0f);
}

// ---- page model -----------------------------------------------------------
// Each page is a list of items. One build pass lays them out; painting and
// hit-testing both read the same rects, so they can never disagree.
enum { K_HEAD, K_TOGGLE, K_SLIDER, K_BIND, K_SEG, K_LINK, K_NOTE, K_CHIPS };
enum { TG_ALLOW, TG_HIGH, TG_BLOCK, TG_CONSOLE };
enum { PT_NONE, PT_TOGGLE, PT_MINUS, PT_PLUS, PT_VALUE, PT_BAR, PT_BIND, PT_LINK, PT_SEG0 };

struct Item {
    int kind, icon;
    const wchar_t* label;
    RECT rc;
    bool hasToggle; int toggle; // K_TOGGLE, or a K_HEAD with a switch on its right
    Num* num;                   // K_SLIDER
    int* sel; const wchar_t** opts; int nopts;   // K_SEG
    int* vk;                    // K_BIND
    wchar_t text[400];          // K_NOTE / K_LINK
};

#define MAX_ITEMS 32
static Item g_items[MAX_ITEMS];
static int  g_nItems = 0;
static Item g_sheet[MAX_ITEMS];
static int  g_nSheet = 0;

// Vertical dividers between the columns of the current page.
static int g_div[2];
static int g_nDiv = 0;

#define MAX_CHIPS 12
static wchar_t g_chipText[MAX_CHIPS][96];
static RECT    g_chipRc[MAX_CHIPS];
static int     g_nChips = 0;
static wchar_t g_chipMore[64];
static RECT    g_chipMoreRc;

static bool* toggleFlag(int t) {
    switch (t) {
    case TG_ALLOW: return &g_allowAll;
    case TG_HIGH:  return &g_highCps;
    case TG_BLOCK: return &g_blockHit;
    default:       return &g_showConsole;
    }
}
static Anim* toggleAnim(int t) {
    switch (t) {
    case TG_ALLOW: return &g_aAllow;
    case TG_HIGH:  return &g_aHigh;
    case TG_BLOCK: return &g_aBlock;
    default:       return &g_aConsole;
    }
}

static Item* Push(Item* list, int& n, int kind, int icon, const wchar_t* label, int x0, int x1, int& y, int h) {
    Item& it = list[n++];
    ZeroMemory(&it, sizeof(it));
    it.kind = kind; it.icon = icon; it.label = label;
    it.rc = R(x0, y, x1, y + h);
    y += h;
    return &it;
}

// The builders below place items into the current list (page or sheet), in the
// current column [g_x0, g_x1].
static Item* g_list  = g_items;
static int*  g_count = &g_nItems;
static int g_x0 = PAD_X, g_x1 = WIN_W - PAD_X;
static void Col(int x0, int x1) { g_x0 = x0; g_x1 = x1; }

#define HEAD_H 40

static void AddHead(int& y, int icon, const wchar_t* l, int toggle = -1) {
    Item* it = Push(g_list, *g_count, K_HEAD, icon, l, g_x0, g_x1, y, HEAD_H);
    if (toggle >= 0) { it->hasToggle = true; it->toggle = toggle; }
}
static void AddToggle(int& y, int icon, const wchar_t* l, int t) {
    Item* it = Push(g_list, *g_count, K_TOGGLE, icon, l, g_x0, g_x1, y, ROW_H);
    it->hasToggle = true; it->toggle = t;
}
static void AddSlider(int& y, int icon, const wchar_t* l, Num* n) {
    Push(g_list, *g_count, K_SLIDER, icon, l, g_x0, g_x1, y, ROW_H + BAR_H)->num = n;
    y += 4;
}
static void AddBind(int& y, int icon, const wchar_t* l, int* vk) {
    Push(g_list, *g_count, K_BIND, icon, l, g_x0, g_x1, y, ROW_H)->vk = vk;
}
static void AddSeg(int& y, int icon, const wchar_t* l, const wchar_t** opts, int n, int* sel) {
    Item* it = Push(g_list, *g_count, K_SEG, icon, l, g_x0, g_x1, y, ROW_H);
    it->opts = opts; it->nopts = n; it->sel = sel;
}
static void AddNote(int& y, const wchar_t* text, int indent) {
    int w = g_x1 - g_x0 - indent;
    int h = TextH(g_fSmall, text, w) + 10;
    Item* it = Push(g_list, *g_count, K_NOTE, 0, NULL, g_x0 + indent, g_x1, y, h);
    lstrcpynW(it->text, text, 400);
}

static void BuildChips(int& y) {
    int x0 = g_x0 + IND, x1 = g_x1;
    Item* it = Push(g_list, *g_count, K_CHIPS, 0, NULL, x0, x1, y, 0);
    g_nChips = 0; g_chipMore[0] = 0;
    int x = x0, cy = y + 4, rows = 1, shown = 0;
    for (int i = 0; i < g_targetCount && g_nChips < MAX_CHIPS; ++i) {
        if (!IsWindow(g_targets[i])) continue;
        wchar_t t[96]; ws_GetTitle(g_targets[i], t, 96);
        int w = TextW(g_fSmall, t) + 18;
        if (w > x1 - x0) w = x1 - x0;
        if (x + w > x1 && x > x0) {
            if (rows == 5) break;
            rows++; x = x0; cy += 28;
        }
        lstrcpynW(g_chipText[g_nChips], t, 96);
        g_chipRc[g_nChips] = R(x, cy, x + w, cy + 22);
        g_nChips++; shown++;
        x += w + 6;
    }
    int rest = g_targetCount - shown;
    if (rest > 0) {
        wsprintfW(g_chipMore, L"+%d more", rest);
        int w = TextW(g_fSmall, g_chipMore) + 4;
        if (x + w > x1) { x = x0; cy += 28; }
        g_chipMoreRc = R(x, cy, x + w, cy + 22);
    }
    if (g_targetCount == 0) {
        lstrcpyW(g_chipMore, L"None selected \x2014 clicks go to every window.");
        g_chipMoreRc = R(x0, cy, x1, cy + 22);
    }
    int h = (cy + 22 + 6) - y;
    it->rc.bottom = y + h;
    y += h;
}

static const wchar_t* PatternNote() {
    switch (g_patternSel) {
    case PATTERN_LEGIT:
        return L"Legit spaces clicks irregularly so the rhythm isn't machine-perfect.";
    case PATTERN_BLATANT:
        return L"Blatant keeps jitter minimal: the fastest, most consistent pattern. The "
               L"limit bursts on activation, then holds the cap (0 = off). Use it only on "
               L"servers without anticheat checks to avoid bans and logs.";
    default:
        return L"Custom sets how long each click is held, and how often and how far the "
               L"delay is allowed to drift.";
    }
}

static int g_sheetDiv = 0;      // x of the divider between the sheet's columns
static int g_creditsY = 0;      // where the credits start in the sheet

static void Rebuild() {
    g_nItems = 0;
    g_nDiv = 0;
    g_list = g_items; g_count = &g_nItems;
    int inner = WIN_W - 2 * PAD_X;
    int w  = (inner - COL_GAP) / 2;
    int xl = PAD_X, xr = PAD_X + w + COL_GAP;
    g_div[0] = xr - COL_GAP / 2; g_nDiv = 1;

    switch (g_page) {
    case 0: {
        // Left and Right Click stacked in the left half, HighCPS on the right.
        int y = BODY_TOP;
        Col(xl, xl + w);
        AddHead(y, IC_CROSSHAIR, L"Left Click");
        AddSlider(y, IC_ZAP, L"CPS", &nL);
        AddBind(y, IC_LINK, L"Bind", &g_bindL);
        y += 16;
        AddHead(y, IC_CROSSHAIR, L"Right Click");
        AddSlider(y, IC_ZAP, L"CPS", &nR);
        AddBind(y, IC_LINK, L"Bind", &g_bindR);

        int ry = BODY_TOP;
        Col(xr, xr + w);
        AddHead(ry, IC_LAYERS, L"HighCPS", TG_HIGH);
        AddSlider(ry, IC_ZAP, L"CPS", &nHigh);
        AddBind(ry, IC_LINK, L"Bind", &g_bindHighCps);
        AddNote(ry, L"A second left-click channel on its own timer; its rate adds to "
                    L"Left Click.", IND);
        ry += 6;
        AddNote(ry, L"Left and Right Click are active once they have a bind. Mode and "
                    L"pattern are shared by every channel \x2014 set them on the "
                    L"Pattern & BlockHit tab.", IND);
        break;
    }
    case 1: {
        // Pattern on the left, BlockHit in its own column on the right.
        int y = BODY_TOP;
        Col(xl, xl + w);
        AddHead(y, IC_ACTIVITY, L"Pattern");
        AddSeg(y, IC_REPEAT, L"Mode", MODE_ITEMS, 2, &g_modeSel);
        AddSeg(y, IC_WAVES, L"Pattern", PATTERN_ITEMS, 3, &g_patternSel);
        y += 4;
        if (g_patternSel == PATTERN_BLATANT)
            AddSlider(y, IC_CAP, L"Max CPS limit", &nLimit);
        if (g_patternSel == PATTERN_CUSTOM) {
            AddSlider(y, IC_TIMER, L"Click duration", &nDur);
            AddSlider(y, IC_WAVES, L"Difference chance", &nChance);
            AddSlider(y, IC_MOVE, L"Difference strength", &nStr);
        }
        wchar_t note[400];
        lstrcpyW(note, g_modeSel == 0 ? L"Hold clicks while the bind is down. "
                                      : L"Toggle: one press starts, the next stops. ");
        lstrcatW(note, PatternNote());
        AddNote(y, note, IND);

        int ry = BODY_TOP;
        Col(xr, xr + w);
        AddHead(ry, IC_SHIELD, L"BlockHit", TG_BLOCK);
        AddSlider(ry, IC_ZAP, L"BPS", &nBps);
        AddBind(ry, IC_PAUSE, L"Pause bind", &g_bindBlockPause);
        AddSeg(ry, IC_REPEAT, L"Pause mode", MODE_ITEMS, 2, &g_blockPauseModeSel);
        AddNote(ry, L"Fires RMB while you physically hold the right mouse button.", IND);
        break;
    }
    }

    // The settings sheet: general settings and credits on the left, window
    // targeting on the right.
    g_nSheet = 0;
    g_list = g_sheet; g_count = &g_nSheet;
    g_sheetDiv = g_div[0];

    int sy = HDR_H + 36;
    Col(xl, xl + w);
    AddHead(sy, IC_GEAR, L"General");
    AddToggle(sy, IC_TERMINAL, L"Show Console", TG_CONSOLE);
    g_creditsY = sy + 14;

    sy = HDR_H + 36;
    Col(xr, xr + w);
    AddHead(sy, IC_TARGET, L"Targeting");
    AddToggle(sy, IC_APPWIN, L"Allow in all programs", TG_ALLOW);
    if (g_allowAll) {
        AddNote(sy, L"Clicks go to whichever window has focus.", IND);
    } else {
        Item* it = Push(g_list, *g_count, K_LINK, IC_APPWIN, L"Windows", g_x0, g_x1, sy, ROW_H);
        lstrcpyW(it->text, L"Choose\x2026");
        BuildChips(sy);
    }

    g_list = g_items; g_count = &g_nItems;
}


// ---- sub-rects ------------------------------------------------------------

static int midY(const Item& it) { return it.rc.top + (it.kind == K_HEAD ? 18 : ROW_H / 2); }

static RECT toggleRc(const Item& it) { int c = midY(it); return R(it.rc.right - 30, c - 8, it.rc.right, c + 8); }
static RECT plusRc(const Item& it)   { int c = midY(it); return R(it.rc.right - 16, c - 10, it.rc.right, c + 10); }
static RECT valueRc(const Item& it)  { RECT p = plusRc(it); return R(p.left - 4 - 48, p.top, p.left - 4, p.bottom); }
static RECT minusRc(const Item& it)  { RECT v = valueRc(it); return R(v.left - 4 - 16, v.top, v.left - 4, v.bottom); }
static RECT barRc(const Item& it)    { return R(it.rc.left + IND, it.rc.top + ROW_H, it.rc.right, it.rc.bottom); }
static RECT trackRc(const Item& it)  { RECT b = barRc(it); return R(b.left, b.top + 5, b.right, b.top + 11); }

static void BindText(const Item& it, wchar_t* out) {
    if (g_listen == it.vk)  lstrcpyW(out, L"press a key\x2026");
    else if (*it.vk == 0)   lstrcpyW(out, L"none");
    else                    bm_DescribeKey(*it.vk, out, 48);
}
static RECT bindRc(const Item& it) {
    wchar_t t[48]; BindText(it, t);
    int w = TextW(g_fCtl, t); if (w < 40) w = 40;
    int c = midY(it);
    return R(it.rc.right - w, c - 10, it.rc.right, c + 10);
}
static RECT linkRc(const Item& it) {
    int w = TextW(g_fCtl, it.text), c = midY(it);
    return R(it.rc.right - w, c - 10, it.rc.right, c + 10);
}
static RECT segRc(const Item& it, int i) {
    int x = it.rc.right, c = midY(it);
    for (int k = it.nopts - 1; k >= 0; --k) {
        int w = TextW(g_fCtl, it.opts[k]);
        if (k == i) return R(x - w, c - 10, x, c + 10);
        x -= w + 12;
    }
    return R(0, 0, 0, 0);
}

// Header and footer controls.
static RECT rcClose() { return R(WIN_W - 10 - 22, 8, WIN_W - 10, 30); }
static RECT rcMin()   { RECT c = rcClose(); return R(c.left - 2 - 22, 8, c.left - 2, 30); }
static RECT rcGear()  { return R(12, BODY_BOT + 7, 34, BODY_BOT + 29); }
static RECT rcApply() { return R(WIN_W - 12 - 92, BODY_BOT + 5, WIN_W - 12, BODY_BOT + 31); }
static RECT rcDot(int i) {
    int total = PAGE_COUNT * 6 + (PAGE_COUNT - 1) * 8;
    int x = (WIN_W - total) / 2 + i * 14, cy = BODY_BOT + FT_H / 2;
    return R(x, cy - 3, x + 6, cy + 3);
}
static RECT rcSheetX() { return R(WIN_W - 10 - 22, HDR_H + 6, WIN_W - 10, HDR_H + 28); }

// ---- hit testing ----------------------------------------------------------
// A hot code is item*16+part for page items, 400+item*16+part for the sheet,
// and one of the H_ values for the fixed chrome.

enum { H_NONE = 0, H_CLOSE = 1000, H_MIN, H_GEAR, H_APPLY, H_SHEETX, H_DOT0 = 1100 };

static bool In(RECT r, POINT p, int pad = 0) { InflateRect(&r, pad, pad); return PtInRect(&r, p) != FALSE; }

static int HitItem(const Item& it, POINT p) {
    switch (it.kind) {
    case K_HEAD:
    case K_TOGGLE: return (it.hasToggle && In(toggleRc(it), p, 4)) ? PT_TOGGLE : PT_NONE;
    case K_SLIDER:
        if (In(minusRc(it), p)) return PT_MINUS;
        if (In(plusRc(it), p))  return PT_PLUS;
        if (In(valueRc(it), p)) return PT_VALUE;
        if (In(barRc(it), p))   return PT_BAR;
        return PT_NONE;
    case K_BIND: return In(bindRc(it), p, 2) ? PT_BIND : PT_NONE;
    case K_LINK: return In(linkRc(it), p, 2) ? PT_LINK : PT_NONE;
    case K_SEG:
        for (int i = 0; i < it.nopts; ++i) if (In(segRc(it, i), p, 3)) return PT_SEG0 + i;
        return PT_NONE;
    }
    return PT_NONE;
}

static int HotTest(POINT p) {
    if (In(rcClose(), p)) return H_CLOSE;
    if (In(rcMin(), p))   return H_MIN;
    if (g_gearOpen) {
        if (In(rcSheetX(), p)) return H_SHEETX;
        for (int i = 0; i < g_nSheet; ++i) {
            int part = HitItem(g_sheet[i], p);
            if (part) return 400 + i * 16 + part;
        }
        return H_NONE;
    }
    if (In(rcGear(), p))  return H_GEAR;
    if (In(rcApply(), p)) return H_APPLY;
    for (int i = 0; i < PAGE_COUNT; ++i) if (In(rcDot(i), p, 4)) return H_DOT0 + i;
    for (int i = 0; i < g_nItems; ++i) {
        int part = HitItem(g_items[i], p);
        if (part) return i * 16 + part;
    }
    return H_NONE;
}

static int g_hot = H_NONE;
static bool HotIs(int base, int idx, int part) { return g_hot == base + idx * 16 + part; }

// ---- painting -------------------------------------------------------------

static void DrawToggle(HDC dc, RECT rc, double t) {
    FillRound(dc, rc, 8, Lerp(C_TOG_OFF, C_ACCENT, t));
    float x = rc.left + 2 + (float)(14 * t);
    FillCircle(dc, x + 6, rc.top + 8.0f, 6, Lerp(C_SOFT, RGB(255, 255, 255), t));
}

static void DrawRowHead(HDC dc, const Item& it) {
    int c = midY(it);
    DrawIcon(dc, it.icon, (float)it.rc.left, c - 7.0f, 14, C_MUTED);
    RECT l = R(it.rc.left + IND, it.rc.top, it.rc.right, it.rc.top + ROW_H);
    Txt(dc, it.label, l, C_TEXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static void PaintItem(HDC dc, const Item& it, int base, int idx) {
    switch (it.kind) {
    case K_HEAD: {
        int c = midY(it);
        DrawIcon(dc, it.icon, (float)it.rc.left, c - 8.0f, 16, C_ACCENT);
        RECT l = R(it.rc.left + IND, c - 12, it.rc.right, c + 12);
        Txt(dc, it.label, l, C_TEXT, g_fSection, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (it.hasToggle) DrawToggle(dc, toggleRc(it), toggleAnim(it.toggle)->cur);
        FillBox(dc, R(it.rc.left, it.rc.bottom - 6, it.rc.right, it.rc.bottom - 5), C_EDGE);
        break;
    }
    case K_TOGGLE:
        DrawRowHead(dc, it);
        DrawToggle(dc, toggleRc(it), toggleAnim(it.toggle)->cur);
        break;

    case K_SLIDER: {
        DrawRowHead(dc, it);
        RECT m = minusRc(it), p = plusRc(it), v = valueRc(it);
        Txt(dc, L"\x2013", m, HotIs(base, idx, PT_MINUS) ? C_TEXT : C_MUTED, g_fCtl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        Txt(dc, L"+",      p, HotIs(base, idx, PT_PLUS)  ? C_TEXT : C_MUTED, g_fCtl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (g_editNum != it.num) {
            wchar_t s[24]; FmtNum(it.num, s);
            Txt(dc, s, v, C_TEXT, g_fCtl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        RECT tr = trackRc(it);
        FillRound(dc, tr, 3, C_TRACK);
        double f = (it.num->v - it.num->mn) / (it.num->mx - it.num->mn);
        if (f < 0) f = 0;
        if (f > 1) f = 1;
        int fx = tr.left + (int)((tr.right - tr.left) * f + 0.5);
        if (fx - tr.left >= 2) FillRound(dc, R(tr.left, tr.top, fx, tr.bottom), 3, C_ACCENT);
        // Crosshair thumb, as in the reference design.
        {
            Gfx gx(dc);
            G::Pen pen(GC(RGB(255, 255, 255)), 1.5f);
            float cy = (tr.top + tr.bottom) / 2.0f;
            gx.g.DrawLine(&pen, (float)fx, cy - 7.5f, (float)fx, cy + 7.5f);
            gx.g.DrawLine(&pen, fx - 7.5f, cy, fx + 7.5f, cy);
        }
        break;
    }

    case K_BIND: {
        DrawRowHead(dc, it);
        wchar_t t[48]; BindText(it, t);
        COLORREF c = C_MUTED;
        if (g_listen == it.vk) {
            double ph = 0.5 + 0.5 * cos(GetTickCount() % 1000 / 1000.0 * 6.2831853);
            c = Lerp(C_ACCENT, C_BG, 0.6 * (1.0 - ph));
        } else if (HotIs(base, idx, PT_BIND)) c = C_SOFT;
        else if (*it.vk) c = C_SOFT;
        Txt(dc, t, bindRc(it), c, g_fCtl, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;
    }

    case K_LINK:
        DrawRowHead(dc, it);
        Txt(dc, it.text, linkRc(it), HotIs(base, idx, PT_LINK) ? C_TEXT : C_SOFT, g_fCtl,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        break;

    case K_SEG:
        DrawRowHead(dc, it);
        for (int i = 0; i < it.nopts; ++i) {
            RECT r = segRc(it, i);
            bool on = (*it.sel == i);
            COLORREF c = on ? C_TEXT : (HotIs(base, idx, PT_SEG0 + i) ? C_SOFT : C_MUTED);
            Txt(dc, it.opts[i], r, c, g_fCtl, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (on) FillBox(dc, R(r.left, r.bottom, r.right, r.bottom + 2), C_ACCENT);
        }
        break;

    case K_NOTE: {
        RECT r = it.rc; r.top += 2;
        Txt(dc, it.text, r, C_MUTED, g_fSmall, DT_LEFT | DT_TOP | DT_WORDBREAK);
        break;
    }

    case K_CHIPS:
        for (int i = 0; i < g_nChips; ++i) {
            FrameRound(dc, g_chipRc[i], 5, C_TRACK);
            RECT t = g_chipRc[i]; t.left += 8; t.right -= 8;
            Txt(dc, g_chipText[i], t, C_SOFT, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        if (g_chipMore[0])
            Txt(dc, g_chipMore, g_chipMoreRc, C_MUTED, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        break;

    }
}

static void PaintBody(HDC dc) {
    FillBox(dc, R(0, HDR_H, WIN_W, BODY_BOT), C_BG);
    int divBot = BODY_BOT - 12;
    for (int i = 0; i < g_nDiv; ++i)
        FillBox(dc, R(g_div[i], BODY_TOP + 4, g_div[i] + 1, divBot), C_EDGE);
    for (int i = 0; i < g_nItems; ++i) PaintItem(dc, g_items[i], 0, i);
}

static void PaintHeader(HDC dc) {
    FillBox(dc, R(0, 0, WIN_W, HDR_H), C_BG);
    DrawMark(dc, 13, 11, 16);
    RECT t = R(36, 0, WIN_W, HDR_H);
    Txt(dc, L"LineAC", t, C_TEXT, g_fTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    int x = 36 + TextW(g_fTitle, L"LineAC") + 8;

    wchar_t pg[40];
    wsprintfW(pg, L"\x00B7  %s", g_gearOpen ? L"Settings" : PAGES[g_page]);
    RECT pr = R(x, 1, WIN_W, HDR_H);
    Txt(dc, pg, pr, C_MUTED, g_fPage, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    x += TextW(g_fPage, pg) + 10;

    // Pulses while any channel is actually clicking.
    if (ae_IsActiveNow()) {
        double ph = (GetTickCount() % 1000) / 1000.0;
        float cy = HDR_H / 2.0f + 0.5f;
        FillCircle(dc, (float)x, cy, 3.0f + 4.0f * (float)ph, C_ACCENT, (BYTE)(110 * (1.0 - ph)));
        FillCircle(dc, (float)x, cy, 3.0f, C_ACCENT);
    }

    RECT m = rcMin(), c = rcClose();
    DrawIcon(dc, IC_MINUS, m.left + 4.5f, m.top + 4.5f, 13, g_hot == H_MIN ? C_TEXT : C_MUTED);
    DrawIcon(dc, IC_X, c.left + 4.5f, c.top + 4.5f, 13, g_hot == H_CLOSE ? C_TEXT : C_MUTED);
}

static void PaintFooter(HDC dc) {
    FillBox(dc, R(0, BODY_BOT, WIN_W, WIN_H), C_BG);
    RECT g = rcGear();
    DrawIcon(dc, IC_GEAR, g.left + 4.0f, g.top + 4.0f, 14, g_hot == H_GEAR ? C_TEXT : C_MUTED);

    for (int i = 0; i < PAGE_COUNT; ++i) {
        RECT d = rcDot(i);
        COLORREF c = (i == g_page) ? C_ACCENT : (g_hot == H_DOT0 + i ? C_DOT_HOT : C_DOT);
        FillCircle(dc, d.left + 3.0f, d.top + 3.0f, 3.0f, c);
    }

    RECT a = rcApply();
    bool done = GetTickCount() < g_flashUntil;
    FillRound(dc, a, 5, done ? C_ACCENT : (g_hot == H_APPLY ? C_BTN_HOT : C_BTN));
    Txt(dc, done ? L"Applied \x2713" : L"Apply", a, done ? RGB(255, 255, 255) : C_TEXT, g_fCtl,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Painted at its open position; PaintAll slides it into place.
static void PaintSheet(HDC dc) {
    FillBox(dc, R(0, HDR_H, WIN_W, WIN_H), C_BG);
    FillBox(dc, R(0, HDR_H, WIN_W, HDR_H + 1), C_EDGE);

    RECT hd = R(COL_L0, HDR_H + 6, WIN_W, HDR_H + 28);
    Txt(dc, L"SETTINGS", hd, C_MUTED, g_fHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT x = rcSheetX();
    DrawIcon(dc, IC_X, x.left + 4.5f, x.top + 4.5f, 13, g_hot == H_SHEETX ? C_TEXT : C_MUTED);

    for (int i = 0; i < g_nSheet; ++i) PaintItem(dc, g_sheet[i], 400, i);

    FillBox(dc, R(g_sheetDiv, HDR_H + 40, g_sheetDiv + 1, BODY_BOT - 12), C_EDGE);

    int y = g_creditsY;
    FillBox(dc, R(COL_L0, y, g_sheetDiv - COL_GAP / 2, y + 1), RGB(0x15, 0x17, 0x1c));
    y += 12;
    RECT l1 = R(COL_L0, y, WIN_W, y + 18);
    RECT l2 = R(COL_L0, y + 18, WIN_W, y + 36);
    RECT l3 = R(COL_L0, y + 36, WIN_W, y + 54);
    Txt(dc, L"This autoclicker was made by @slurov", l1, C_SOFT, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    Txt(dc, L"YT \x2014 @slurov",     l2, C_MUTED, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    Txt(dc, L"TikTok \x2014 @slurov", l3, C_MUTED, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT v = R(COL_L0, WIN_H - FT_H, WIN_W, WIN_H);
    Txt(dc, L"v3.0.0", v, C_DIM, g_fPage, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static HDC NewLayer(HDC ref, HBITMAP* bmp) {
    HDC dc = CreateCompatibleDC(ref);
    *bmp = CreateCompatibleBitmap(ref, WIN_W, WIN_H);
    SelectObject(dc, *bmp);
    return dc;
}

static void PaintAll(HDC ref, HDC dc) {
    PaintHeader(dc);

    double pt = g_aPage.cur;
    if (pt < 0.999) {
        // New page slides in from the side it came from and fades up.
        HBITMAP lb; HDC layer = NewLayer(ref, &lb);
        PaintBody(layer);
        FillBox(dc, R(0, HDR_H, WIN_W, BODY_BOT), C_BG);
        int dx = (int)((1.0 - pt) * 14.0 * g_pageDir);
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(pt * 255.0), 0 };
        AlphaBlend(dc, dx, HDR_H, WIN_W, BODY_BOT - HDR_H, layer, 0, HDR_H, WIN_W, BODY_BOT - HDR_H, bf);
        DeleteDC(layer); DeleteObject(lb);
    } else {
        PaintBody(dc);
    }
    PaintFooter(dc);

    double st = g_aGear.cur;
    if (st > 0.004) {
        HBITMAP lb; HDC layer = NewLayer(ref, &lb);
        PaintSheet(layer);
        int full = WIN_H - HDR_H;
        int off = (int)((1.0 - st) * full + 0.5);
        BitBlt(dc, 0, HDR_H + off, WIN_W, full - off, layer, 0, HDR_H, SRCCOPY);
        DeleteDC(layer); DeleteObject(lb);
    }

    if (!g_dwmRound) FrameRound(dc, R(0, 0, WIN_W, WIN_H), 8, C_EDGE);
}

// ---- behaviour ------------------------------------------------------------

static bool AnimStep(Anim& a) {
    double d = a.target - a.cur;
    if (fabs(d) < 0.004) { a.cur = a.target; return false; }
    a.cur += d * 0.26;
    return true;
}
static void StartAnim(HWND hwnd) { SetTimer(hwnd, TIMER_ANIM, ANIM_MS, NULL); }

static void GoPage(HWND hwnd, int p) {
    if (p < 0 || p >= PAGE_COUNT || p == g_page) return;
    g_pageDir = (p > g_page) ? 1 : -1;
    g_page = p;
    g_aPage.cur = 0.0; g_aPage.target = 1.0;
    Rebuild();
    StartAnim(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void OpenSheet(HWND hwnd, bool open) {
    g_gearOpen = open;
    g_aGear.target = open ? 1.0 : 0.0;
    StartAnim(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void FlipToggle(HWND hwnd, int t) {
    bool* f = toggleFlag(t);
    *f = !*f;
    toggleAnim(t)->target = *f ? 1.0 : 0.0;
    if (t == TG_CONSOLE) { if (*f) con_Show(hwnd); else con_Hide(); }
    if (t == TG_ALLOW) Rebuild();
    StartAnim(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void DoApply(HWND hwnd) {
    Settings s = {};
    s.allowAll     = g_allowAll;
    s.targetCount  = g_targetCount;
    for (int i = 0; i < g_targetCount; ++i) s.targets[i] = g_targets[i];
    s.leftEnabled  = (g_bindL != 0);
    s.rightEnabled = (g_bindR != 0);
    s.leftCPS      = nL.v;
    s.rightCPS     = nR.v;
    s.pattern      = g_patternSel;
    s.holdMode     = (g_modeSel == 0);
    s.bindL        = g_bindL;
    s.bindR        = g_bindR;
    s.blockHitEnabled   = g_blockHit;
    s.blockHitBPS       = nBps.v;
    s.blockHitPauseBind = g_bindBlockPause;
    s.blockHitPauseHold = (g_blockPauseModeSel == 0);
    s.highCpsEnabled = g_highCps;
    s.highCpsCPS     = nHigh.v;
    s.highCpsBind    = g_bindHighCps;
    s.customDuration = nDur.v;
    s.customChance   = nChance.v;
    s.customStrength = nStr.v;
    s.limitedCps     = nLimit.v;
    ae_Apply(s);

    g_flashUntil = GetTickCount() + 1400;
    g_flashShown = true;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void DoBind(HWND hwnd, int* vk) {
    g_listen = vk;
    InvalidateRect(hwnd, NULL, FALSE);
    UpdateWindow(hwnd);
    int got = bm_CaptureBind(hwnd);     // pumps messages until a key or button
    if (got) *vk = got;
    g_listen = NULL;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void SliderFromX(const Item& it, int x) {
    RECT tr = trackRc(it);
    double f = (double)(x - tr.left) / (tr.right - tr.left);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    Num* n = it.num;
    double v = n->mn + f * (n->mx - n->mn);
    SetNum(n, floor(v / n->step + 0.5) * n->step);
}

// Double-clicking a value swaps in an edit box, so exact numbers like 12.5
// can still be typed, as with the old input fields.
static void BeginEdit(const Item& it) {
    g_editNum = it.num;
    RECT v = valueRc(it);
    MoveWindow(g_hEdit, v.left, v.top + 1, v.right - v.left, v.bottom - v.top - 2, TRUE);
    wchar_t s[24]; FmtDec(it.num->v, s, 2);
    SetWindowTextW(g_hEdit, s);
    ShowWindow(g_hEdit, SW_SHOW);
    SetFocus(g_hEdit);
    SendMessageW(g_hEdit, EM_SETSEL, 0, -1);
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void EndEdit(bool commit) {
    if (!g_editNum) return;
    Num* n = g_editNum;
    g_editNum = NULL;
    if (commit) {
        wchar_t buf[32] = {};
        GetWindowTextW(g_hEdit, buf, 32);
        wchar_t* end = NULL;
        double v = wcstod(buf, &end);
        if (end != buf) SetNum(n, v);
    }
    ShowWindow(g_hEdit, SW_HIDE);
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR) {
        wchar_t c = (wchar_t)w;
        if (c == 13 || c == 27) return 0;
        if (c == 8) {}
        else if (c == L'.') {
            wchar_t buf[32]; GetWindowTextW(h, buf, 32);
            if (wcschr(buf, L'.')) return 0;
        } else if (c < L'0' || c > L'9') return 0;
    }
    if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE)) {
        PostMessageW(GetParent(h), WM_APP_EDITDONE, w == VK_RETURN, 0);
        return 0;
    }
    if (m == WM_KILLFOCUS) PostMessageW(GetParent(h), WM_APP_EDITDONE, 1, 0);
    return CallWindowProcW(g_oldEditProc, h, m, w, l);
}

static LRESULT CALLBACK RmbHookProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)lParam;
        if (!(m->flags & LLMHF_INJECTED)) {
            if (wParam == WM_RBUTTONDOWN)    ae_SetRmbPhysical(true);
            else if (wParam == WM_RBUTTONUP) ae_SetRmbPhysical(false);
        }
    }
    return CallNextHookEx(g_rmbHook, code, wParam, lParam);
}

static Num* g_drag = NULL;
static int  g_dragItem = -1;

static void OnClick(HWND hwnd, POINT p, bool dbl) {
    if (g_editNum) EndEdit(true);
    SetFocus(hwnd);

    int h = HotTest(p);
    if (h == H_CLOSE) { DestroyWindow(hwnd); return; }
    if (h == H_MIN)   { ShowWindow(hwnd, SW_MINIMIZE); return; }
    if (h == H_GEAR)  { OpenSheet(hwnd, true); return; }
    if (h == H_SHEETX){ OpenSheet(hwnd, false); return; }
    if (h == H_APPLY) { DoApply(hwnd); return; }
    if (h >= H_DOT0 && h < H_DOT0 + PAGE_COUNT) { GoPage(hwnd, h - H_DOT0); return; }

    if (h == H_NONE) {
        if (p.y < HDR_H) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        return;
    }

    bool sheet = (h >= 400);
    int code = sheet ? h - 400 : h;
    int idx = code / 16, part = code % 16;
    Item& it = sheet ? g_sheet[idx] : g_items[idx];
    switch (part) {
    case PT_TOGGLE: FlipToggle(hwnd, it.toggle); break;
    case PT_MINUS:  SetNum(it.num, it.num->v - it.num->step); break;
    case PT_PLUS:   SetNum(it.num, it.num->v + it.num->step); break;
    case PT_VALUE:  if (dbl) BeginEdit(it); break;
    case PT_BAR:
        g_drag = it.num; g_dragItem = idx;
        SetCapture(hwnd);
        SliderFromX(it, p.x);
        break;
    case PT_BIND:   DoBind(hwnd, it.vk); return;
    case PT_LINK: {
        HWND picked[MAX_TARGETS];
        int n = ws_SelectWindows(hwnd, g_targets, g_targetCount, picked, MAX_TARGETS);
        if (n >= 0) {
            g_targetCount = n;
            for (int i = 0; i < n; ++i) g_targets[i] = picked[i];
        }
        Rebuild();
        break;
    }
    default:
        if (part >= PT_SEG0) {
            int v = part - PT_SEG0;
            if (*it.sel != v) { *it.sel = v; Rebuild(); }
        }
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ApplyCorners(HWND hwnd) {
    DWORD round = 2;                                    // DWMWCP_ROUND
    g_dwmRound = SUCCEEDED(DwmSetWindowAttribute(hwnd, 33, &round, sizeof(round)));
    if (g_dwmRound) {
        COLORREF edge = C_EDGE;
        DwmSetWindowAttribute(hwnd, 34, &edge, sizeof(edge));   // DWMWA_BORDER_COLOR
    } else {
        SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, WIN_W + 1, WIN_H + 1, 16, 16), TRUE);
    }
}

static HFONT Font(int px, int weight) {
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                       CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;
        g_fTitle = Font(14, FW_BOLD);
        g_fPage  = Font(11, FW_SEMIBOLD);
        g_fLabel = Font(13, FW_SEMIBOLD);
        g_fCtl   = Font(12, FW_SEMIBOLD);
        g_fSmall = Font(11, FW_NORMAL);
        g_fStat  = Font(24, FW_BOLD);
        g_fHead  = Font(11, FW_BOLD);
        g_fSection = Font(15, FW_BOLD);
        g_fieldBrush = CreateSolidBrush(C_FIELD);
        g_measure = CreateCompatibleDC(NULL);

        for (int i = 0; i < IC_COUNT; ++i) {
            g_icons[i] = new G::GraphicsPath();
            ParsePath(*g_icons[i], ICON_SVG[i]);
        }

        g_hEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_CENTER | ES_AUTOHSCROLL,
                                  0, 0, 10, 10, hwnd, NULL, GetModuleHandleW(NULL), NULL);
        SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)g_fCtl, TRUE);
        SendMessageW(g_hEdit, EM_LIMITTEXT, 6, 0);
        g_oldEditProc = (WNDPROC)SetWindowLongPtrW(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditProc);

        Rebuild();
        ApplyCorners(hwnd);
        SetTimer(hwnd, TIMER_TICK, TICK_MS, NULL);
        g_rmbHook = SetWindowsHookExW(WH_MOUSE_LL, RmbHookProc, GetModuleHandleW(NULL), 0);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, C_FIELD);
        SetTextColor(dc, C_TEXT);
        return (LRESULT)g_fieldBrush;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        HBITMAP bmp; HDC mem = NewLayer(hdc, &bmp);
        PaintAll(hdc, mem);
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
               ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
               mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        DeleteDC(mem); DeleteObject(bmp);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wp == TIMER_ANIM) {
            bool busy = false;
            for (int i = 0; i < (int)(sizeof(g_anims) / sizeof(g_anims[0])); ++i)
                if (AnimStep(*g_anims[i])) busy = true;
            if (!busy) KillTimer(hwnd, TIMER_ANIM);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wp == TIMER_TICK) {
            bool active = ae_IsActiveNow();
            if (active || active != g_lastActive) {
                RECT hr = R(0, 0, WIN_W, HDR_H);
                InvalidateRect(hwnd, &hr, FALSE);
            }
            g_lastActive = active;

            if (g_flashShown && GetTickCount() >= g_flashUntil) {
                g_flashShown = false;
                RECT fr = R(0, BODY_BOT, WIN_W, WIN_H);
                InvalidateRect(hwnd, &fr, FALSE);
            }
            if (g_listen) InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_MOUSEMOVE: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (g_drag && g_dragItem >= 0) {
            double before = g_drag->v;
            SliderFromX(g_items[g_dragItem], p.x);
            if (g_drag->v != before) InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        int h = HotTest(p);
        if (h != g_hot) {
            g_hot = h;
            InvalidateRect(hwnd, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_hot != H_NONE) { g_hot = H_NONE; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        OnClick(hwnd, p, msg == WM_LBUTTONDBLCLK);
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_drag) { g_drag = NULL; g_dragItem = -1; ReleaseCapture(); }
        return 0;

    case WM_CAPTURECHANGED:
        g_drag = NULL; g_dragItem = -1;
        return 0;

    case WM_MOUSEWHEEL: {
        if (g_gearOpen) return 0;
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &p);
        int dir = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
        for (int i = 0; i < g_nItems; ++i) {
            if (g_items[i].kind == K_SLIDER && PtInRect(&g_items[i].rc, p)) {
                SetNum(g_items[i].num, g_items[i].num->v + dir * g_items[i].num->step);
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE && g_gearOpen) { OpenSheet(hwnd, false); return 0; }
        if (!g_gearOpen && wp == VK_LEFT)  { GoPage(hwnd, g_page - 1); return 0; }
        if (!g_gearOpen && wp == VK_RIGHT) { GoPage(hwnd, g_page + 1); return 0; }
        return 0;

    case WM_APP_EDITDONE:
        EndEdit(wp != 0);
        return 0;

    case WM_LINEAC_CONSOLE_CLOSED:
        g_showConsole = false;
        g_aConsole.target = 0.0;
        StartAnim(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        con_Shutdown();
        if (g_rmbHook) { UnhookWindowsHookEx(g_rmbHook); g_rmbHook = NULL; }
        KillTimer(hwnd, TIMER_TICK);
        KillTimer(hwnd, TIMER_ANIM);
        for (int i = 0; i < IC_COUNT; ++i) { delete g_icons[i]; g_icons[i] = NULL; }
        DeleteObject(g_fTitle); DeleteObject(g_fPage); DeleteObject(g_fLabel);
        DeleteObject(g_fCtl);   DeleteObject(g_fSmall); DeleteObject(g_fStat);
        DeleteObject(g_fHead);  DeleteObject(g_fSection); DeleteObject(g_fieldBrush);
        DeleteDC(g_measure);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\LineacAutoClicker_SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Another instance is already running.",
                    L"LineAC", MB_OK | MB_ICONINFORMATION);
        CloseHandle(mutex);
        return 0;
    }

    timeBeginPeriod(1);

    G::GdiplusStartupInput gsi;
    G::GdiplusStartup(&g_gdipToken, &gsi, NULL);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"LineacAutoClickerWnd";
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"LineAC",
        WS_POPUP | WS_CLIPCHILDREN | WS_MINIMIZEBOX, (sw - WIN_W) / 2, (sh - WIN_H) / 2, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);

    ae_Start();
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ae_Stop();
    G::GdiplusShutdown(g_gdipToken);
    timeEndPeriod(1);
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return 0;
}
