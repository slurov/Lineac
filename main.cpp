#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>
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
#pragma comment(linker, "\"/manifestdependency:type='win32' "                 \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "             \
    "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#define WIN_W        400
#define TITLE_H      34
#define STATUS_H     24
#define PAD_X        14
#define CONTENT_TOP  44
#define GAP          6
#define HEAD_H       22
#define ROW_H        30
#define ITEM_H       28
#define ID_EDIT_L        201
#define ID_EDIT_R        202
#define ID_EDIT_BPS      203
#define ID_EDIT_HIGHCPS  204
#define ID_EDIT_DURATION 205
#define ID_EDIT_CHANCE   206
#define ID_EDIT_STRENGTH 207
#define ID_EDIT_LIMITED  208
#define TIMER_STATUS     1
#define TIMER_ANIM       2
#define ANIM_MS          16

#define C_BG            RGB(0x18,0x1b,0x23)
#define C_BAR           RGB(0x13,0x15,0x1c)
#define C_CARD          RGB(0x20,0x23,0x2e)
#define C_CARDBORDER    RGB(0x2b,0x2e,0x3a)
#define C_SEP           RGB(0x27,0x2a,0x35)
#define C_ACCENT        RGB(0x1a,0x7d,0xff)
#define C_ACCENT_HOVER  RGB(0x2e,0x8a,0xff)
#define C_LOGO_LINE     RGB(0xee,0xf1,0xf8)
#define C_LABEL         RGB(0xb8,0xbd,0xd0)
#define C_MUTED         RGB(0x45,0x4c,0x63)
#define C_TITLE         RGB(0x7a,0x80,0x99)
#define C_INPUT_TXT     RGB(0xd8,0xdc,0xea)
#define C_BORDER        RGB(0x3a,0x3d,0x49)
#define C_TOGGLE_OFF    RGB(0x31,0x36,0x4a)
#define C_BINDBG        RGB(0x29,0x2d,0x3e)
#define C_BINDBG_HOVER  RGB(0x32,0x37,0x49)
#define C_BIND_TXT      RGB(0x8a,0x93,0xad)
#define C_BADGE_TXT     RGB(0x5a,0x62,0x78)
#define C_WAIT_TXT      RGB(0xf5,0xa6,0x23)
#define C_WAIT_BG       RGB(0x2a,0x27,0x21)
#define C_WAIT_BORDER   RGB(0x6b,0x55,0x2e)
#define C_BOUND_BG      RGB(0x1b,0x28,0x3b)
#define C_BOUND_BORDER  RGB(0x2c,0x47,0x6b)
#define C_SEL_DISBG     RGB(0x1c,0x1f,0x28)
#define C_SEL_DISTXT    RGB(0x2f,0x34,0x48)
#define C_DOT_OFF       RGB(0x2f,0x34,0x48)
#define C_STAT_RUN      RGB(0x6e,0xa8,0xff)
#define C_DOT_MIN       RGB(0xf5,0xa6,0x23)
#define C_DOT_CLS       RGB(0xff,0x5f,0x57)
#define C_POPUP_TXT     RGB(0x7a,0x80,0x99)
#define C_POPUP_TXT1    RGB(0x9b,0xa3,0xb8)

enum { EL_NONE, EL_TOGGLE, EL_SEL, EL_BINDL, EL_BINDR,
       EL_PATTERN, EL_MODE, EL_APPLY, EL_GEAR, EL_MIN, EL_CLS,
       EL_BLOCKTOGGLE, EL_HIGHCPSTOGGLE, EL_BINDHIGHCPS, EL_BINDBLOCKPAUSE,
       EL_BLOCKPAUSEMODE };

enum BindState { BIND_NOTSET = 0, BIND_WAITING = 1, BIND_BOUND = 2 };

static bool g_allowAll      = false;
static HWND g_targets[MAX_TARGETS];
static int  g_targetCount   = 0;
static int  g_bindL = 0, g_bindR = 0;
static int  g_bindLState = BIND_NOTSET, g_bindRState = BIND_NOTSET;
static int  g_patternSel = 0;
static int  g_modeSel    = 0;
static bool g_blockHit   = false;
static int  g_bindBlockPause = 0;
static int  g_bindBlockPauseState = BIND_NOTSET;
static int  g_blockPauseModeSel = 0;
static bool g_highCps    = false;
static int  g_bindHighCps = 0;
static int  g_bindHighCpsState = BIND_NOTSET;
static bool g_customVisible = false;
static bool g_blatantVisible = false;
static bool g_limitedTip = false;
static int  g_winH = 638;
static int  g_openCombo  = 0;
static int  g_comboHot   = -1;
static bool g_gearOpen    = false;
static bool g_showConsole = false;
static bool g_editsHidden = false;

// Toggles and the gear backdrop ease toward their target instead of snapping.
// 0 = off/hidden, 1 = on/fully shown.
struct Anim { double cur, target; };
static Anim g_aAllow   = { 0.0, 0.0 };
static Anim g_aBlock   = { 0.0, 0.0 };
static Anim g_aHigh    = { 0.0, 0.0 };
static Anim g_aConsole = { 0.0, 0.0 };
static Anim g_aGear    = { 0.0, 0.0 };
static Anim* const g_anims[] = { &g_aAllow, &g_aBlock, &g_aHigh, &g_aConsole, &g_aGear };
static int  g_hot        = EL_NONE;
static bool g_lastActive = false;
static DWORD g_flashUntil = 0;

static HWND  g_hEditL = NULL, g_hEditR = NULL, g_hEditBPS = NULL, g_hEditHighCps = NULL;
static HWND  g_hEditDuration = NULL, g_hEditChance = NULL, g_hEditStrength = NULL;
static HWND  g_hEditLimited = NULL;
static WNDPROC g_oldEditProc = NULL;
static HHOOK g_rmbHook = NULL;

static HFONT g_fTitle, g_fLabel, g_fHead, g_fSmall, g_fInput, g_fApply, g_fArrow;
static HBRUSH g_inputBrush, g_disabledBrush;

static RECT rcTitlebar, rcMin, rcCls;
static RECT rcCardWindow, rcCardLMB, rcCardRMB, rcCardSettings, rcCardBlockHit, rcCardHighCps;
static RECT rcCardCustom;
static RECT rcToggle, rcSel, rcWinLbl;
static RECT rcInpL, rcBindL, rcBadgeL, rcInpR, rcBindR, rcBadgeR;
static RECT rcPattern, rcMode, rcBlockToggle, rcInpBPS, rcApply, rcStatusBar, rcGear;
static RECT rcBlockPauseBindBtn, rcBlockPauseBadge, rcBlockPauseMode;
static RECT rcHighCpsToggle, rcInpHighCps, rcBindHighCps, rcBadgeHighCps;
static RECT rcInpDuration, rcInpChance, rcInpStrength;
static RECT rcCardLimited, rcInpLimited;

static const wchar_t* PATTERN_ITEMS[]   = { L"Legit", L"Blatant", L"Custom" };
static const wchar_t* MODE_ITEMS[]      = { L"Hold",  L"Toggle"  };
static const wchar_t* PAUSEMODE_ITEMS[] = { L"Hold",  L"Toggle"  };
static int comboCount(int which) { return (which == EL_PATTERN) ? 3 : 2; }

static RECT comboRectOf(int id) {
    if (id == EL_PATTERN) return rcPattern;
    if (id == EL_MODE)    return rcMode;
    return rcBlockPauseMode;
}
static const wchar_t** comboItemsOf(int id) {
    if (id == EL_PATTERN) return PATTERN_ITEMS;
    if (id == EL_MODE)    return MODE_ITEMS;
    return PAUSEMODE_ITEMS;
}
static int* comboSelOf(int id) {
    if (id == EL_PATTERN) return &g_patternSel;
    if (id == EL_MODE)    return &g_modeSel;
    return &g_blockPauseModeSel;
}

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int  cv(int top, int h, int eh)     { return top + (h - eh) / 2; }

static void FillRound(HDC dc, RECT rc, int rad, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, rad * 2, rad * 2);
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(b);
}
static void FrameRound(HDC dc, RECT rc, int rad, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ op = SelectObject(dc, p);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, rad * 2, rad * 2);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(p);
}
static void FillCircle(HDC dc, int cx, int cy, int r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ ob = SelectObject(dc, b);
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(b);
}
static void Txt(HDC dc, const wchar_t* s, RECT rc, COLORREF c, HFONT f, UINT fmt) {
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &rc, fmt | DT_NOPREFIX);
    SelectObject(dc, of);
}

static void Layout() {
    rcTitlebar = R(0, 0, WIN_W, TITLE_H);
    int cy = TITLE_H / 2;
    int clsCx = WIN_W - PAD_X - 7;
    int minCx = clsCx - 20;
    rcMin = R(minCx - 7, cy - 7, minCx + 7, cy + 7);
    rcCls = R(clsCx - 7, cy - 7, clsCx + 7, cy + 7);

    int x = PAD_X, w = WIN_W - 2 * PAD_X, y = CONTENT_TOP;
    rcCardWindow   = R(x, y, x + w, y + 88); y += 88 + GAP;
    rcCardLMB      = R(x, y, x + w, y + 58); y += 58 + GAP;
    rcCardRMB      = R(x, y, x + w, y + 58); y += 58 + GAP;
    rcCardHighCps  = R(x, y, x + w, y + 88); y += 88 + GAP;
    rcCardSettings = R(x, y, x + w, y + 88); y += 88 + GAP;
    if (g_customVisible) { rcCardCustom = R(x, y, x + w, y + 112); y += 112 + GAP; }
    else                 { rcCardCustom = R(0, 0, 0, 0); }
    if (g_blatantVisible) { rcCardLimited = R(x, y, x + w, y + 58); y += 58 + GAP; }
    else                  { rcCardLimited = R(0, 0, 0, 0); }
    rcCardBlockHit = R(x, y, x + w, y + 142);

    int iL = x + 14, iR = x + w - 14;

    int r1 = rcCardWindow.top + HEAD_H;
    rcToggle = R(iR - 38, cv(r1, ROW_H, 21), iR, cv(r1, ROW_H, 21) + 21);
    int r2 = r1 + ROW_H;
    rcSel    = R(iL, cv(r2, ROW_H, 25), iL + 100, cv(r2, ROW_H, 25) + 25);
    rcWinLbl = R(rcSel.right + 10, r2, iR, r2 + ROW_H);

    int lr = rcCardLMB.top + HEAD_H;
    rcBadgeL = R(iR - 66, cv(lr, ROW_H, 22), iR, cv(lr, ROW_H, 22) + 22);
    rcBindL  = R(rcBadgeL.left - 8 - 64, cv(lr, ROW_H, 25), rcBadgeL.left - 8, cv(lr, ROW_H, 25) + 25);
    rcInpL   = R(rcBindL.left - 8 - 66, cv(lr, ROW_H, 27), rcBindL.left - 8, cv(lr, ROW_H, 27) + 27);

    int rr = rcCardRMB.top + HEAD_H;
    rcBadgeR = R(iR - 66, cv(rr, ROW_H, 22), iR, cv(rr, ROW_H, 22) + 22);
    rcBindR  = R(rcBadgeR.left - 8 - 64, cv(rr, ROW_H, 25), rcBadgeR.left - 8, cv(rr, ROW_H, 25) + 25);
    rcInpR   = R(rcBindR.left - 8 - 66, cv(rr, ROW_H, 27), rcBindR.left - 8, cv(rr, ROW_H, 27) + 27);

    int pr = rcCardSettings.top + HEAD_H;
    rcPattern = R(iR - 150, cv(pr, ROW_H, 27), iR, cv(pr, ROW_H, 27) + 27);
    int mr = pr + ROW_H;
    rcMode    = R(iR - 150, cv(mr, ROW_H, 27), iR, cv(mr, ROW_H, 27) + 27);

    if (g_customVisible) {
        int cu = rcCardCustom.top + HEAD_H;
        rcInpDuration = R(iR - 66, cv(cu, ROW_H, 27), iR, cv(cu, ROW_H, 27) + 27);
        int cu2 = cu + ROW_H;
        rcInpChance   = R(iR - 66, cv(cu2, ROW_H, 27), iR, cv(cu2, ROW_H, 27) + 27);
        int cu3 = cu2 + ROW_H;
        rcInpStrength = R(iR - 66, cv(cu3, ROW_H, 27), iR, cv(cu3, ROW_H, 27) + 27);
    }

    if (g_blatantVisible) {
        int lu = rcCardLimited.top + HEAD_H;
        rcInpLimited = R(iR - 66, cv(lu, ROW_H, 27), iR, cv(lu, ROW_H, 27) + 27);
    }

    int b1 = rcCardBlockHit.top + HEAD_H;
    rcBlockToggle = R(iR - 38, cv(b1, ROW_H, 21), iR, cv(b1, ROW_H, 21) + 21);
    int b2 = b1 + ROW_H;
    rcInpBPS = R(iR - 66, cv(b2, ROW_H, 27), iR, cv(b2, ROW_H, 27) + 27);
    int b3 = b2 + ROW_H;
    rcBlockPauseBadge   = R(iR - 66, cv(b3, ROW_H, 22), iR, cv(b3, ROW_H, 22) + 22);
    rcBlockPauseBindBtn = R(rcBlockPauseBadge.left - 8 - 64, cv(b3, ROW_H, 25), rcBlockPauseBadge.left - 8, cv(b3, ROW_H, 25) + 25);
    int b4 = b3 + ROW_H;
    rcBlockPauseMode = R(iR - 150, cv(b4, ROW_H, 27), iR, cv(b4, ROW_H, 27) + 27);

    int hc1 = rcCardHighCps.top + HEAD_H;
    rcHighCpsToggle = R(iR - 38, cv(hc1, ROW_H, 21), iR, cv(hc1, ROW_H, 21) + 21);
    int hc2 = hc1 + ROW_H;
    rcBadgeHighCps = R(iR - 66, cv(hc2, ROW_H, 22), iR, cv(hc2, ROW_H, 22) + 22);
    rcBindHighCps  = R(rcBadgeHighCps.left - 8 - 64, cv(hc2, ROW_H, 25), rcBadgeHighCps.left - 8, cv(hc2, ROW_H, 25) + 25);
    rcInpHighCps   = R(rcBindHighCps.left - 8 - 66, cv(hc2, ROW_H, 27), rcBindHighCps.left - 8, cv(hc2, ROW_H, 27) + 27);

    int bottom = rcCardBlockHit.bottom;
    rcApply = R((WIN_W - 100) / 2, bottom + 32, (WIN_W + 100) / 2, bottom + 32 + 32);
    rcStatusBar = R(0, rcApply.bottom + 8, WIN_W, rcApply.bottom + 8 + STATUS_H);
    g_winH = rcStatusBar.bottom;
    rcGear = R(WIN_W - PAD_X - 16, rcStatusBar.top + (STATUS_H - 16) / 2,
               WIN_W - PAD_X, rcStatusBar.top + (STATUS_H - 16) / 2 + 16);
}

static RECT comboItem(const RECT& cmb, int i) {
    int top = cmb.bottom + 4 + i * ITEM_H;
    return R(cmb.left, top, cmb.right, top + ITEM_H);
}

// Gear panel: a SETTINGS block on top, the credits underneath. Its height is
// derived from GEAR_ROWS, so adding the planned Polling Rate row is a one-line
// change rather than a re-measure.
#define GP_W       236
#define GP_PAD     8
#define GP_HEAD_H  13
#define GP_ROW_H   24
#define GP_INFO_H  18
#define GP_SEP_GAP 7
#define GEAR_ROWS  1

static int gearPopupH() {
    return GP_PAD + GP_HEAD_H + 4 + GEAR_ROWS * GP_ROW_H
         + GP_SEP_GAP + 1 + GP_SEP_GAP + 3 * GP_INFO_H + GP_PAD;
}
static RECT gearPopupRect() {
    int rgt = WIN_W - PAD_X;
    int bot = rcStatusBar.top - 8;
    return R(rgt - GP_W, bot - gearPopupH(), rgt, bot);
}
static int gearRowTop(int i) {
    return gearPopupRect().top + GP_PAD + GP_HEAD_H + 4 + i * GP_ROW_H;
}
static RECT gearConsoleToggle() {
    RECT pp = gearPopupRect();
    int t = gearRowTop(0) + (GP_ROW_H - 21) / 2;
    return R(pp.right - 13 - 38, t, pp.right - 13, t + 21);
}

static double readCps(HWND edit) {
    wchar_t buf[32] = {};
    GetWindowTextW(edit, buf, 32);
    wchar_t* end = NULL;
    double v = wcstod(buf, &end);
    if (end == buf) v = 0.0;
    if (v < 0.0) v = 0.0;
    if (v > 100.0) v = 100.0;
    return v;
}
static void fmt2(double v, wchar_t* out) {
    if (v < 0) v = 0;
    int whole = (int)v, frac = (int)((v - whole) * 100.0 + 0.5);
    if (frac >= 100) { whole++; frac -= 100; }
    wsprintfW(out, L"%d.%02d", whole, frac);
}

static void DrawLogoMark(HDC dc, int ox, int oy, int S) {
    double s = S / 96.0;
    POINT a = { ox + (int)(30 * s + 0.5), oy + (int)(22 * s + 0.5) };
    POINT b = { ox + (int)(30 * s + 0.5), oy + (int)(58 * s + 0.5) };
    POINT c = { ox + (int)(62 * s + 0.5), oy + (int)(58 * s + 0.5) };
    int strokeW = (int)(9 * s + 0.5);  if (strokeW < 2) strokeW = 2;
    int nodeR   = (int)(7.5 * s + 0.5); if (nodeR < 2) nodeR = 2;
    int ringR   = (int)(15 * s + 0.5);

    HPEN rp = CreatePen(PS_SOLID, 1, RGB(0x22,0x3a,0x5c));
    HGDIOBJ orp = SelectObject(dc, rp);
    HGDIOBJ orb = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, c.x - ringR, c.y - ringR, c.x + ringR, c.y + ringR);
    SelectObject(dc, orb); SelectObject(dc, orp); DeleteObject(rp);

    LOGBRUSH lb = { BS_SOLID, C_ACCENT, 0 };
    HPEN lp = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
                           strokeW, &lb, 0, NULL);
    HGDIOBJ olp = SelectObject(dc, lp);
    POINT pts[3] = { a, b, c };
    Polyline(dc, pts, 3);
    SelectObject(dc, olp); DeleteObject(lp);

    FillCircle(dc, c.x, c.y, nodeR, C_ACCENT);
}

// Thin-line cog, drawn to match the status bar's existing icon weight.
static const double GEAR_DIR[8][2] = {
    {  1.0000,  0.0000 }, {  0.7071,  0.7071 }, {  0.0000,  1.0000 }, { -0.7071,  0.7071 },
    { -1.0000,  0.0000 }, { -0.7071, -0.7071 }, {  0.0000, -1.0000 }, {  0.7071, -0.7071 }
};

static void DrawGear(HDC dc, int cx, int cy, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ op = SelectObject(dc, p);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - 5, cy - 5, cx + 5, cy + 5);
    Ellipse(dc, cx - 2, cy - 2, cx + 2, cy + 2);
    for (int i = 0; i < 8; ++i) {
        int x0 = cx + (int)(GEAR_DIR[i][0] * 4.0 + 0.5), y0 = cy + (int)(GEAR_DIR[i][1] * 4.0 + 0.5);
        int x1 = cx + (int)(GEAR_DIR[i][0] * 7.0 + 0.5), y1 = cy + (int)(GEAR_DIR[i][1] * 7.0 + 0.5);
        MoveToEx(dc, x0, y0, NULL); LineTo(dc, x1, y1);
    }
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(p);
}


// Exponential ease-out: fast at first, settles in ~10 frames at 16 ms each.
// Returns true while the value is still moving.
static bool AnimStep(Anim& a) {
    double d = a.target - a.cur;
    if (fabs(d) < 0.004) { a.cur = a.target; return false; }
    a.cur += d * 0.26;
    return true;
}
static COLORREF Lerp(COLORREF a, COLORREF b, double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    int r = (int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t + 0.5);
    int g = (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t + 0.5);
    int bl = (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t + 0.5);
    return RGB(r, g, bl);
}

// One drawer for every switch in the UI: the track colour and the thumb both
// follow the same 0..1 progress, so they slide instead of jumping.
static void DrawToggle(HDC dc, const RECT& rc, double t) {
    int h = rc.bottom - rc.top;
    FillRound(dc, rc, h / 2, Lerp(C_TOGGLE_OFF, C_ACCENT, t));
    int x0 = rc.left + 3;
    int x1 = rc.right - 3 - 15;
    int x  = x0 + (int)((x1 - x0) * t + 0.5);
    FillCircle(dc, x + 7, rc.top + 3 + 7, 7, RGB(255, 255, 255));
}

// ---- backdrop blur -------------------------------------------------------
// GDI's StretchBlt does not interpolate when it scales up, so the old
// shrink-and-enlarge trick produced visible blocks. This is a real separable
// box blur over the pixels instead; three passes read as a gaussian.

static HBITMAP MakeDib(HDC ref, int w, int h, BYTE** bits) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;            // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* p = NULL;
    HBITMAP b = CreateDIBSection(ref, &bi, DIB_RGB_COLORS, &p, NULL, 0);
    *bits = (BYTE*)p;
    return b;
}

// One pass along an arbitrary axis, so the same code does rows and columns.
// `step` walks the axis, `line` jumps to the next row/column, both in bytes.
static void BlurAxis(const BYTE* src, BYTE* dst, int n, int lines, int step, int line, int r) {
    int span = r * 2 + 1;
    for (int l = 0; l < lines; ++l) {
        const BYTE* s = src + (size_t)l * line;
        BYTE*       d = dst + (size_t)l * line;
        int sb = 0, sg = 0, sr = 0;
        for (int i = -r; i <= r; ++i) {
            int k = i < 0 ? 0 : (i >= n ? n - 1 : i);
            const BYTE* p = s + (size_t)k * step;
            sb += p[0]; sg += p[1]; sr += p[2];
        }
        for (int i = 0; i < n; ++i) {
            BYTE* o = d + (size_t)i * step;
            o[0] = (BYTE)(sb / span);
            o[1] = (BYTE)(sg / span);
            o[2] = (BYTE)(sr / span);
            o[3] = 255;
            int io = i - r;     if (io < 0)  io = 0;
            int in = i + r + 1; if (in >= n) in = n - 1;
            const BYTE* pa = s + (size_t)in * step;
            const BYTE* pr = s + (size_t)io * step;
            sb += pa[0] - pr[0];
            sg += pa[1] - pr[1];
            sr += pa[2] - pr[2];
        }
    }
}

// The blurred backdrop is built once per opening and reused for every frame of
// the fade, so the animation stays cheap.
static HDC     g_blurDc   = NULL;
static HBITMAP g_blurBmp  = NULL;
static BYTE*   g_blurBits = NULL;
static BYTE*   g_blurTmp  = NULL;
static int     g_blurW = 0, g_blurH = 0;
static bool    g_blurValid = false;

static void FreeBlur() {
    if (g_blurDc)  { DeleteDC(g_blurDc);      g_blurDc = NULL; }
    if (g_blurBmp) { DeleteObject(g_blurBmp); g_blurBmp = NULL; }
    if (g_blurTmp) { free(g_blurTmp);         g_blurTmp = NULL; }
    g_blurBits = NULL;
    g_blurW = g_blurH = 0;
    g_blurValid = false;
}

static const int  BLUR_RADIUS = 3;
static const int  BLUR_PASSES = 3;
static const int  BLUR_DIM    = 200;   // out of 255: a gentle darkening

static void BuildBlur(HDC ref, HDC src, int w, int h) {
    if (g_blurDc && (g_blurW != w || g_blurH != h)) FreeBlur();
    if (!g_blurDc) {
        g_blurDc  = CreateCompatibleDC(ref);
        g_blurBmp = MakeDib(ref, w, h, &g_blurBits);
        g_blurTmp = (BYTE*)malloc((size_t)w * h * 4);
        if (!g_blurDc || !g_blurBmp || !g_blurBits || !g_blurTmp) { FreeBlur(); return; }
        SelectObject(g_blurDc, g_blurBmp);
        g_blurW = w; g_blurH = h;
    }

    BitBlt(g_blurDc, 0, 0, w, h, src, 0, 0, SRCCOPY);
    GdiFlush();                                    // the DIB bits are stale until this

    for (int p = 0; p < BLUR_PASSES; ++p) {
        BlurAxis(g_blurBits, g_blurTmp,  w, h, 4,     w * 4, BLUR_RADIUS);
        BlurAxis(g_blurTmp,  g_blurBits, h, w, w * 4, 4,     BLUR_RADIUS);
    }

    size_t n = (size_t)w * h * 4;
    for (size_t i = 0; i < n; i += 4) {
        g_blurBits[i]     = (BYTE)(g_blurBits[i]     * BLUR_DIM / 255);
        g_blurBits[i + 1] = (BYTE)(g_blurBits[i + 1] * BLUR_DIM / 255);
        g_blurBits[i + 2] = (BYTE)(g_blurBits[i + 2] * BLUR_DIM / 255);
    }
    g_blurValid = true;
}

// Lay `layer` over `dst` at the given opacity.
static void BlendLayer(HDC dst, HDC layer, int w, int h, double amount) {
    BYTE a = (BYTE)(amount * 255.0 + 0.5);
    if (a == 0) return;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, a, 0 };
    AlphaBlend(dst, 0, 0, w, h, layer, 0, 0, w, h, bf);
}

static void DrawButton(HDC dc, RECT rc, const wchar_t* text, COLORREF bg,
                       COLORREF border, COLORREF txt, HFONT f) {
    FillRound(dc, rc, 5, bg);
    if (border != bg) FrameRound(dc, rc, 5, border);
    Txt(dc, text, rc, txt, f, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawBadge(HDC dc, RECT rc, int state, int vk) {
    COLORREF bg, bd, tx; const wchar_t* s; wchar_t key[48];
    if (state == BIND_WAITING)      { bg = C_WAIT_BG;  bd = C_WAIT_BORDER;  tx = C_WAIT_TXT;  s = L"Press key…"; }
    else if (state == BIND_BOUND)   { bg = C_BOUND_BG; bd = C_BOUND_BORDER; tx = C_ACCENT;    bm_DescribeKey(vk, key, 48); s = key; }
    else                            { bg = C_BINDBG;   bd = C_BORDER;       tx = C_BADGE_TXT; s = L"Not set"; }
    FillRound(dc, rc, 4, bg);
    FrameRound(dc, rc, 4, bd);
    Txt(dc, s, rc, tx, g_fSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawCombo(HDC dc, RECT rc, const wchar_t* text, bool hot, bool enabled) {
    FillRound(dc, rc, 5, enabled ? C_BAR : C_SEL_DISBG);
    FrameRound(dc, rc, 5, enabled ? (hot ? C_ACCENT : C_BORDER) : C_CARDBORDER);
    RECT t = rc; t.left += 9; t.right -= 26;
    Txt(dc, text, t, enabled ? C_INPUT_TXT : C_SEL_DISTXT, g_fInput, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT a = R(rc.right - 22, rc.top, rc.right - 6, rc.bottom);
    Txt(dc, L"▾", a, enabled ? C_MUTED : C_SEL_DISTXT, g_fArrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// While the gear panel is up the child EDIT controls are hidden, because Windows
// would paint them sharply over our blurred backdrop. Their values are drawn
// here instead, matching the controls' own right-aligned layout.
static void DrawEditGhost(HDC dc, HWND edit, const RECT& rc, bool enabled) {
    if (!g_editsHidden || !edit) return;

    // The real control fills its box; without this the card colour showed
    // through and every field read as grey until the controls came back.
    RECT in = rc;
    InflateRect(&in, -1, -1);
    HBRUSH b = CreateSolidBrush(enabled ? C_BAR : C_SEL_DISBG);
    FillRect(dc, &in, b);
    DeleteObject(b);

    wchar_t buf[32] = {};
    GetWindowTextW(edit, buf, 32);
    RECT t = rc;
    t.left += 7;
    t.right -= 9;
    Txt(dc, buf, t, enabled ? C_INPUT_TXT : C_SEL_DISTXT, g_fInput,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void PaintAll(HDC dc, RECT cr) {

    HBRUSH bg = CreateSolidBrush(C_BG); FillRect(dc, &cr, bg); DeleteObject(bg);

    HBRUSH bar = CreateSolidBrush(C_BAR); FillRect(dc, &rcTitlebar, bar); DeleteObject(bar);
    DrawLogoMark(dc, PAD_X, (TITLE_H - 22) / 2, 22);
    {

        HGDIOBJ of = SelectObject(dc, g_fTitle);
        SetBkMode(dc, TRANSPARENT);
        SIZE szL; GetTextExtentPoint32W(dc, L"Line", 4, &szL);
        int tx = PAD_X + 22 + 8, ty = (TITLE_H - szL.cy) / 2;
        SetTextColor(dc, C_LOGO_LINE); TextOutW(dc, tx, ty, L"Line", 4);
        SetTextColor(dc, C_ACCENT);    TextOutW(dc, tx + szL.cx, ty, L"AC", 2);
        SelectObject(dc, of);
    }
    FillCircle(dc, (rcMin.left + rcMin.right) / 2, (rcMin.top + rcMin.bottom) / 2, 7, C_DOT_MIN);
    FillCircle(dc, (rcCls.left + rcCls.right) / 2, (rcCls.top + rcCls.bottom) / 2, 7, C_DOT_CLS);

    RECT cards[6] = { rcCardWindow, rcCardLMB, rcCardRMB, rcCardHighCps, rcCardSettings, rcCardBlockHit };
    const wchar_t* heads[6] = { L"WINDOW", L"LEFT MOUSE BUTTON", L"RIGHT MOUSE BUTTON", L"HIGHCPS BUTTON", L"SETTINGS", L"BLOCKHIT" };
    for (int i = 0; i < 6; ++i) {
        FillRound(dc, cards[i], 7, C_CARD);
        FrameRound(dc, cards[i], 7, C_CARDBORDER);
        RECT h = R(cards[i].left + 14, cards[i].top + 6, cards[i].right - 14, cards[i].top + 6 + 13);
        Txt(dc, heads[i], h, C_MUTED, g_fHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_customVisible) {
        FillRound(dc, rcCardCustom, 7, C_CARD);
        FrameRound(dc, rcCardCustom, 7, C_CARDBORDER);
        RECT h = R(rcCardCustom.left + 14, rcCardCustom.top + 6, rcCardCustom.right - 14, rcCardCustom.top + 6 + 13);
        Txt(dc, L"CUSTOM PATTERN", h, C_MUTED, g_fHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_blatantVisible) {
        FillRound(dc, rcCardLimited, 7, C_CARD);
        FrameRound(dc, rcCardLimited, 7, C_CARDBORDER);
        RECT h = R(rcCardLimited.left + 14, rcCardLimited.top + 6, rcCardLimited.right - 14, rcCardLimited.top + 6 + 13);
        Txt(dc, L"LIMITED CPS", h, C_MUTED, g_fHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    HPEN sep = CreatePen(PS_SOLID, 1, C_SEP);
    HGDIOBJ osep = SelectObject(dc, sep);

    int wsep = rcCardWindow.top + HEAD_H + ROW_H;
    MoveToEx(dc, rcCardWindow.left + 14, wsep, NULL); LineTo(dc, rcCardWindow.right - 14, wsep);

    int ssep = rcCardSettings.top + HEAD_H + ROW_H;
    MoveToEx(dc, rcCardSettings.left + 14, ssep, NULL); LineTo(dc, rcCardSettings.right - 14, ssep);

    int bsep = rcCardBlockHit.top + HEAD_H + ROW_H;
    MoveToEx(dc, rcCardBlockHit.left + 14, bsep, NULL); LineTo(dc, rcCardBlockHit.right - 14, bsep);
    int bsep2 = rcCardBlockHit.top + HEAD_H + 2 * ROW_H;
    MoveToEx(dc, rcCardBlockHit.left + 14, bsep2, NULL); LineTo(dc, rcCardBlockHit.right - 14, bsep2);
    int bsep3 = rcCardBlockHit.top + HEAD_H + 3 * ROW_H;
    MoveToEx(dc, rcCardBlockHit.left + 14, bsep3, NULL); LineTo(dc, rcCardBlockHit.right - 14, bsep3);

    int hsep = rcCardHighCps.top + HEAD_H + ROW_H;
    MoveToEx(dc, rcCardHighCps.left + 14, hsep, NULL); LineTo(dc, rcCardHighCps.right - 14, hsep);

    if (g_customVisible) {
        int cs1 = rcCardCustom.top + HEAD_H + ROW_H;
        int cs2 = rcCardCustom.top + HEAD_H + 2 * ROW_H;
        MoveToEx(dc, rcCardCustom.left + 14, cs1, NULL); LineTo(dc, rcCardCustom.right - 14, cs1);
        MoveToEx(dc, rcCardCustom.left + 14, cs2, NULL); LineTo(dc, rcCardCustom.right - 14, cs2);
    }
    SelectObject(dc, osep); DeleteObject(sep);

    RECT lblAllow = R(rcCardWindow.left + 14, rcCardWindow.top + HEAD_H,
                      rcToggle.left - 8, rcCardWindow.top + HEAD_H + ROW_H);
    Txt(dc, L"Allow in all programs", lblAllow, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawToggle(dc, rcToggle, g_aAllow.cur);

    bool selOn = !g_allowAll;
    DrawButton(dc, rcSel, L"Select window",
               selOn ? (g_hot == EL_SEL ? C_BINDBG_HOVER : C_BINDBG) : C_SEL_DISBG,
               selOn ? C_BORDER : C_CARDBORDER,
               selOn ? C_LABEL : C_SEL_DISTXT, g_fInput);

    wchar_t wl[160];
    if (g_allowAll) wl[0] = 0;
    else if (g_targetCount == 0) lstrcpynW(wl, L"All windows (none selected)", 160);
    else if (g_targetCount == 1 && IsWindow(g_targets[0])) ws_GetTitle(g_targets[0], wl, 160);
    else wsprintfW(wl, L"%d windows selected", g_targetCount);
    Txt(dc, wl, rcWinLbl, C_MUTED, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT lblL = R(rcCardLMB.left + 14, rcCardLMB.top + HEAD_H, rcInpL.left - 8, rcCardLMB.top + HEAD_H + ROW_H);
    Txt(dc, L"CPS", lblL, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpL, 5, C_BORDER);
    DrawEditGhost(dc, g_hEditL, rcInpL, true);
    DrawButton(dc, rcBindL, L"Set bind", g_hot == EL_BINDL ? C_BINDBG_HOVER : C_BINDBG, C_BORDER, C_BIND_TXT, g_fSmall);
    DrawBadge(dc, rcBadgeL, g_bindLState, g_bindL);

    RECT lblR = R(rcCardRMB.left + 14, rcCardRMB.top + HEAD_H, rcInpR.left - 8, rcCardRMB.top + HEAD_H + ROW_H);
    Txt(dc, L"CPS", lblR, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpR, 5, C_BORDER);
    DrawEditGhost(dc, g_hEditR, rcInpR, true);
    DrawButton(dc, rcBindR, L"Set bind", g_hot == EL_BINDR ? C_BINDBG_HOVER : C_BINDBG, C_BORDER, C_BIND_TXT, g_fSmall);
    DrawBadge(dc, rcBadgeR, g_bindRState, g_bindR);

    RECT lblP = R(rcCardSettings.left + 14, rcCardSettings.top + HEAD_H, rcPattern.left - 8, rcCardSettings.top + HEAD_H + ROW_H);
    Txt(dc, L"Click pattern", lblP, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawCombo(dc, rcPattern, PATTERN_ITEMS[g_patternSel], g_openCombo == EL_PATTERN, true);
    RECT lblM = R(rcCardSettings.left + 14, rcMode.top - (ROW_H - 27) / 2, rcMode.left - 8, rcMode.top - (ROW_H - 27) / 2 + ROW_H);
    Txt(dc, L"Mode", lblM, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawCombo(dc, rcMode, MODE_ITEMS[g_modeSel], g_openCombo == EL_MODE, true);

    if (g_customVisible) {
        const wchar_t* clbl[3] = { L"Click Duration (ms)", L"Difference Chance (%)", L"Difference Strength (%)" };
        RECT crc[3] = { rcInpDuration, rcInpChance, rcInpStrength };
        HWND cedit[3] = { g_hEditDuration, g_hEditChance, g_hEditStrength };
        for (int i = 0; i < 3; ++i) {
            RECT lbl = R(rcCardCustom.left + 14, crc[i].top - (ROW_H - 27) / 2,
                         crc[i].left - 8, crc[i].top - (ROW_H - 27) / 2 + ROW_H);
            Txt(dc, clbl[i], lbl, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            FrameRound(dc, crc[i], 5, C_BORDER);
            DrawEditGhost(dc, cedit[i], crc[i], true);
        }
    }

    if (g_blatantVisible) {
        RECT lblLim = R(rcCardLimited.left + 14, rcInpLimited.top - (ROW_H - 27) / 2,
                        rcInpLimited.left - 8, rcInpLimited.top - (ROW_H - 27) / 2 + ROW_H);
        Txt(dc, L"Max CPS limit", lblLim, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        FrameRound(dc, rcInpLimited, 5, C_BORDER);
        DrawEditGhost(dc, g_hEditLimited, rcInpLimited, true);
    }

    RECT lblBH = R(rcCardBlockHit.left + 14, rcCardBlockHit.top + HEAD_H,
                   rcBlockToggle.left - 8, rcCardBlockHit.top + HEAD_H + ROW_H);
    Txt(dc, L"Enable BlockHit", lblBH, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggle(dc, rcBlockToggle, g_aBlock.cur);
    RECT lblBPS = R(rcCardBlockHit.left + 14, rcInpBPS.top - (ROW_H - 27) / 2,
                    rcInpBPS.left - 8, rcInpBPS.top - (ROW_H - 27) / 2 + ROW_H);
    Txt(dc, L"BPS (hold RMB)", lblBPS, g_blockHit ? C_LABEL : C_SEL_DISTXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpBPS, 5, g_blockHit ? C_BORDER : C_CARDBORDER);
    DrawEditGhost(dc, g_hEditBPS, rcInpBPS, g_blockHit);

    RECT lblBP = R(rcCardBlockHit.left + 14, rcBlockPauseBindBtn.top - (ROW_H - 25) / 2,
                   rcBlockPauseBindBtn.left - 8, rcBlockPauseBindBtn.top - (ROW_H - 25) / 2 + ROW_H);
    Txt(dc, L"Pause bind", lblBP, g_blockHit ? C_LABEL : C_SEL_DISTXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (g_blockHit) {
        DrawButton(dc, rcBlockPauseBindBtn, L"Set bind",
                   g_hot == EL_BINDBLOCKPAUSE ? C_BINDBG_HOVER : C_BINDBG, C_BORDER, C_BIND_TXT, g_fSmall);
        DrawBadge(dc, rcBlockPauseBadge, g_bindBlockPauseState, g_bindBlockPause);
    } else {
        DrawButton(dc, rcBlockPauseBindBtn, L"Set bind", C_SEL_DISBG, C_CARDBORDER, C_SEL_DISTXT, g_fSmall);
        wchar_t key[48]; const wchar_t* bs = L"Not set";
        if (g_bindBlockPause) { bm_DescribeKey(g_bindBlockPause, key, 48); bs = key; }
        FillRound(dc, rcBlockPauseBadge, 4, C_SEL_DISBG);
        FrameRound(dc, rcBlockPauseBadge, 4, C_CARDBORDER);
        Txt(dc, bs, rcBlockPauseBadge, C_SEL_DISTXT, g_fSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    RECT lblPM = R(rcCardBlockHit.left + 14, rcBlockPauseMode.top - (ROW_H - 27) / 2,
                   rcBlockPauseMode.left - 8, rcBlockPauseMode.top - (ROW_H - 27) / 2 + ROW_H);
    Txt(dc, L"Pause mode", lblPM, g_blockHit ? C_LABEL : C_SEL_DISTXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawCombo(dc, rcBlockPauseMode, PAUSEMODE_ITEMS[g_blockPauseModeSel], g_openCombo == EL_BLOCKPAUSEMODE, g_blockHit);

    RECT lblHC = R(rcCardHighCps.left + 14, rcCardHighCps.top + HEAD_H,
                   rcHighCpsToggle.left - 8, rcCardHighCps.top + HEAD_H + ROW_H);
    Txt(dc, L"Enable HighCPS", lblHC, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggle(dc, rcHighCpsToggle, g_aHigh.cur);
    RECT lblHCcps = R(rcCardHighCps.left + 14, rcInpHighCps.top - (ROW_H - 27) / 2,
                      rcInpHighCps.left - 8, rcInpHighCps.top - (ROW_H - 27) / 2 + ROW_H);
    Txt(dc, L"CPS", lblHCcps, g_highCps ? C_LABEL : C_SEL_DISTXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpHighCps, 5, g_highCps ? C_BORDER : C_CARDBORDER);
    DrawEditGhost(dc, g_hEditHighCps, rcInpHighCps, g_highCps);
    if (g_highCps) {
        DrawButton(dc, rcBindHighCps, L"Set bind",
                   g_hot == EL_BINDHIGHCPS ? C_BINDBG_HOVER : C_BINDBG, C_BORDER, C_BIND_TXT, g_fSmall);
        DrawBadge(dc, rcBadgeHighCps, g_bindHighCpsState, g_bindHighCps);
    } else {

        DrawButton(dc, rcBindHighCps, L"Set bind", C_SEL_DISBG, C_CARDBORDER, C_SEL_DISTXT, g_fSmall);
        wchar_t key[48]; const wchar_t* bs = L"Not set";
        if (g_bindHighCps) { bm_DescribeKey(g_bindHighCps, key, 48); bs = key; }
        FillRound(dc, rcBadgeHighCps, 4, C_SEL_DISBG);
        FrameRound(dc, rcBadgeHighCps, 4, C_CARDBORDER);
        Txt(dc, bs, rcBadgeHighCps, C_SEL_DISTXT, g_fSmall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    DrawButton(dc, rcApply, L"Apply", g_hot == EL_APPLY ? C_ACCENT_HOVER : C_ACCENT, C_ACCENT, RGB(255,255,255), g_fApply);

    HBRUSH sb = CreateSolidBrush(C_BAR); FillRect(dc, &rcStatusBar, sb); DeleteObject(sb);
    // The nick sits where the status text used to; the dot and the nick colour
    // still carry running / applied state, so no feedback is lost.
    bool running = ae_IsActiveNow();
    bool dotOn   = running || GetTickCount() < g_flashUntil;
    FillCircle(dc, PAD_X + 3, rcStatusBar.top + STATUS_H / 2, 3, dotOn ? C_ACCENT : C_DOT_OFF);
    RECT stt = R(PAD_X + 11, rcStatusBar.top, WIN_W - 40, rcStatusBar.bottom);
    Txt(dc, L"@slurov", stt, dotOn ? C_STAT_RUN : C_MUTED, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawGear(dc, (rcGear.left + rcGear.right) / 2, (rcGear.top + rcGear.bottom) / 2,
             (g_hot == EL_GEAR || g_gearOpen) ? C_ACCENT : C_DOT_OFF);

    if (g_limitedTip && g_blatantVisible) {
        const wchar_t* tip =
            L"Advice: Turn on this module to get cps targeted cps super fast "
            L"if the server doesn't have ac checks to avoid bans and logs";
        const int pad = 9;
        int boxW = WIN_W - 2 * PAD_X;

        RECT calc = R(0, 0, boxW - 2 * pad, 0);
        HGDIOBJ of = SelectObject(dc, g_fSmall);
        DrawTextW(dc, tip, -1, &calc, DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
        SelectObject(dc, of);
        int boxH = (calc.bottom - calc.top) + 2 * pad;
        RECT tp = R(PAD_X, rcCardLimited.bottom + 4, PAD_X + boxW, rcCardLimited.bottom + 4 + boxH);
        FillRound(dc, tp, 6, C_BAR);
        FrameRound(dc, tp, 6, C_ACCENT);
        RECT ti = R(tp.left + pad, tp.top + pad, tp.right - pad, tp.bottom - pad);
        Txt(dc, tip, ti, C_POPUP_TXT1, g_fSmall, DT_LEFT | DT_TOP | DT_WORDBREAK);
    }

    if (g_openCombo) {
        RECT base = comboRectOf(g_openCombo);
        const wchar_t** items = comboItemsOf(g_openCombo);
        int curSel = *comboSelOf(g_openCombo);
        int n = comboCount(g_openCombo);
        RECT panel = R(base.left, base.bottom + 4, base.right, base.bottom + 4 + n * ITEM_H);
        FillRound(dc, panel, 5, C_BAR);
        FrameRound(dc, panel, 5, C_BORDER);
        for (int i = 0; i < n; ++i) {
            RECT it = comboItem(base, i);
            if (i == g_comboHot) {
                RECT hl = it; InflateRect(&hl, -3, 0);
                FillRound(dc, hl, 4, RGB(0x23,0x29,0x3a));
            }
            RECT tt2 = it; tt2.left += 9;
            Txt(dc, items[i], tt2, (i == curSel) ? C_ACCENT : C_INPUT_TXT, g_fInput,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// Drawn after the backdrop has been blurred, so it stays sharp on top of it.
static void PaintGearPanel(HDC dc) {
    RECT pp = gearPopupRect();
    FillRound(dc, pp, 6, C_CARD);
    FrameRound(dc, pp, 6, C_CARDBORDER);

    RECT hd = R(pp.left + 13, pp.top + GP_PAD, pp.right - 13, pp.top + GP_PAD + GP_HEAD_H);
    Txt(dc, L"SETTINGS", hd, C_MUTED, g_fHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT tg  = gearConsoleToggle();
    RECT lbl = R(pp.left + 13, gearRowTop(0), tg.left - 8, gearRowTop(0) + GP_ROW_H);
    Txt(dc, L"Show Console", lbl, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggle(dc, tg, g_aConsole.cur);

    int gsy = gearRowTop(GEAR_ROWS) + GP_SEP_GAP;
    HPEN gp = CreatePen(PS_SOLID, 1, C_SEP);
    HGDIOBJ ogp = SelectObject(dc, gp);
    MoveToEx(dc, pp.left + 13, gsy, NULL); LineTo(dc, pp.right - 13, gsy);
    SelectObject(dc, ogp); DeleteObject(gp);

    int iy = gsy + 1 + GP_SEP_GAP;
    RECT l1 = R(pp.left + 13, iy,                 pp.right - 13, iy + GP_INFO_H);
    RECT l2 = R(pp.left + 13, iy + GP_INFO_H,     pp.right - 13, iy + 2 * GP_INFO_H);
    RECT l3 = R(pp.left + 13, iy + 2 * GP_INFO_H, pp.right - 13, iy + 3 * GP_INFO_H);
    Txt(dc, L"This autoclicker was made by @slurov", l1, C_POPUP_TXT1, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    Txt(dc, L"YT — @slurov",     l2, C_POPUP_TXT, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    Txt(dc, L"TikTok — @slurov", l3, C_POPUP_TXT, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR) {
        wchar_t c = (wchar_t)w;
        if (c == 8) {}
        else if (c == L'.') {
            wchar_t buf[32]; GetWindowTextW(h, buf, 32);
            if (wcschr(buf, L'.')) return 0;
        } else if (c < L'0' || c > L'9') return 0;
    }
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

static void DoBindCh(HWND hwnd, int* bind, int* state, const RECT* badge) {
    *state = BIND_WAITING;
    InvalidateRect(hwnd, badge, FALSE);
    UpdateWindow(hwnd);
    int vk = bm_CaptureBind(hwnd);
    if (vk) { *bind = vk; *state = BIND_BOUND; }
    else    { *state = *bind ? BIND_BOUND : BIND_NOTSET; }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void DoApply(HWND hwnd) {
    Settings s = {};
    s.allowAll     = g_allowAll;
    s.targetCount  = g_targetCount;
    for (int i = 0; i < g_targetCount; ++i) s.targets[i] = g_targets[i];
    s.leftEnabled  = (g_bindL != 0);
    s.rightEnabled = (g_bindR != 0);
    s.leftCPS      = readCps(g_hEditL);
    s.rightCPS     = readCps(g_hEditR);
    s.pattern      = g_patternSel;
    s.holdMode     = (g_modeSel == 0);
    s.bindL        = g_bindL;
    s.bindR        = g_bindR;
    s.blockHitEnabled = g_blockHit;
    s.blockHitBPS     = readCps(g_hEditBPS);
    s.blockHitPauseBind = g_bindBlockPause;
    s.blockHitPauseHold = (g_blockPauseModeSel == 0);
    s.highCpsEnabled  = g_highCps;
    s.highCpsCPS      = readCps(g_hEditHighCps);
    s.highCpsBind     = g_bindHighCps;
    s.customDuration  = readCps(g_hEditDuration);
    s.customChance    = readCps(g_hEditChance);
    s.customStrength  = readCps(g_hEditStrength);
    s.limitedCps      = readCps(g_hEditLimited);
    ae_Apply(s);

    wchar_t lc[16], rc[16], bc[16], hc[16];
    fmt2(s.leftCPS, lc);     SetWindowTextW(g_hEditL, lc);
    fmt2(s.rightCPS, rc);    SetWindowTextW(g_hEditR, rc);
    fmt2(s.blockHitBPS, bc); SetWindowTextW(g_hEditBPS, bc);
    fmt2(s.highCpsCPS, hc);  SetWindowTextW(g_hEditHighCps, hc);
    if (g_customVisible) {
        wchar_t cd[16], cc[16], cs[16];
        fmt2(s.customDuration, cd); SetWindowTextW(g_hEditDuration, cd);
        fmt2(s.customChance, cc);   SetWindowTextW(g_hEditChance, cc);
        fmt2(s.customStrength, cs); SetWindowTextW(g_hEditStrength, cs);
    }
    if (g_blatantVisible) {
        wchar_t li[16]; fmt2(s.limitedCps, li); SetWindowTextW(g_hEditLimited, li);
    }

    g_flashUntil = GetTickCount() + 1400;
    InvalidateRect(hwnd, NULL, FALSE);
}

static int HotTest(POINT p) {
    if (PtInRect(&rcCls, p)) return EL_CLS;
    if (PtInRect(&rcMin, p)) return EL_MIN;
    if (PtInRect(&rcGear, p)) return EL_GEAR;
    if (PtInRect(&rcToggle, p)) return EL_TOGGLE;
    if (PtInRect(&rcBlockToggle, p)) return EL_BLOCKTOGGLE;
    if (PtInRect(&rcHighCpsToggle, p)) return EL_HIGHCPSTOGGLE;
    if (!g_allowAll && PtInRect(&rcSel, p)) return EL_SEL;
    if (PtInRect(&rcBindL, p)) return EL_BINDL;
    if (PtInRect(&rcBindR, p)) return EL_BINDR;
    if (g_highCps && PtInRect(&rcBindHighCps, p)) return EL_BINDHIGHCPS;
    if (g_blockHit && PtInRect(&rcBlockPauseBindBtn, p)) return EL_BINDBLOCKPAUSE;
    if (PtInRect(&rcPattern, p)) return EL_PATTERN;
    if (PtInRect(&rcMode, p)) return EL_MODE;
    if (PtInRect(&rcApply, p)) return EL_APPLY;
    return EL_NONE;
}

static void MoveEdit(HWND e, const RECT& rc) {
    MoveWindow(e, rc.left + 1, rc.top + 1, (rc.right - rc.left) - 2, (rc.bottom - rc.top) - 2, TRUE);
}


// Visibility mirrors what UpdatePatternCards set up; only the gear panel hides
// them wholesale so the backdrop can be blurred without sharp controls on top.
static void ShowEdits(bool show) {
    ShowWindow(g_hEditL,        show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditR,        show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditBPS,      show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditHighCps,  show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditDuration, (show && g_customVisible)  ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditChance,   (show && g_customVisible)  ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditStrength, (show && g_customVisible)  ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditLimited,  (show && g_blatantVisible) ? SW_SHOW : SW_HIDE);
}

// WM_TIMER stops the frame timer again once every animation has settled.
static void StartAnim(HWND hwnd) {
    SetTimer(hwnd, TIMER_ANIM, ANIM_MS, NULL);
}

static void OpenGear(HWND hwnd, bool open) {
    g_gearOpen = open;
    g_aGear.target = open ? 1.0 : 0.0;
    if (open) {
        g_blurValid = false;                       // backdrop changed since last time
        if (!g_editsHidden) { g_editsHidden = true; ShowEdits(false); }
    }
    StartAnim(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void UpdatePatternCards(HWND hwnd) {
    bool nowCustom  = (g_patternSel == PATTERN_CUSTOM);
    bool nowBlatant = (g_patternSel == PATTERN_BLATANT);
    if (nowCustom == g_customVisible && nowBlatant == g_blatantVisible) return;
    g_customVisible  = nowCustom;
    g_blatantVisible = nowBlatant;
    if (!nowBlatant) g_limitedTip = false;

    Layout();

    MoveEdit(g_hEditL, rcInpL);
    MoveEdit(g_hEditR, rcInpR);
    MoveEdit(g_hEditHighCps, rcInpHighCps);
    MoveEdit(g_hEditBPS, rcInpBPS);
    if (nowCustom) {
        MoveEdit(g_hEditDuration, rcInpDuration);
        MoveEdit(g_hEditChance,   rcInpChance);
        MoveEdit(g_hEditStrength, rcInpStrength);
    }
    if (nowBlatant) MoveEdit(g_hEditLimited, rcInpLimited);

    ShowWindow(g_hEditDuration, nowCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditChance,   nowCustom ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hEditStrength, nowCustom ? SW_SHOW : SW_HIDE);
    EnableWindow(g_hEditDuration, nowCustom);
    EnableWindow(g_hEditChance,   nowCustom);
    EnableWindow(g_hEditStrength, nowCustom);
    ShowWindow(g_hEditLimited, nowBlatant ? SW_SHOW : SW_HIDE);
    EnableWindow(g_hEditLimited, nowBlatant);

    SetWindowPos(hwnd, NULL, 0, 0, WIN_W, g_winH, SWP_NOMOVE | SWP_NOZORDER);
    SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, WIN_W + 1, g_winH + 1, 16, 16), TRUE);
    InvalidateRect(hwnd, NULL, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        Layout();
        g_fTitle = CreateFontW(-12, 0,0,0, FW_MEDIUM,    0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fLabel = CreateFontW(-13, 0,0,0, FW_NORMAL,    0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fHead  = CreateFontW(-10, 0,0,0, FW_BOLD,      0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fSmall = CreateFontW(-11, 0,0,0, FW_NORMAL,    0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fInput = CreateFontW(-12, 0,0,0, FW_NORMAL,    0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fApply = CreateFontW(-13, 0,0,0, FW_SEMIBOLD,  0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fArrow = CreateFontW(-9,  0,0,0, FW_NORMAL,    0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_inputBrush = CreateSolidBrush(C_BAR);
        g_disabledBrush = CreateSolidBrush(C_SEL_DISBG);

        DWORD es = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_RIGHT;
        g_hEditL = CreateWindowExW(0, L"EDIT", L"20.00", es,
            rcInpL.left + 1, rcInpL.top + 1, (rcInpL.right - rcInpL.left) - 2, (rcInpL.bottom - rcInpL.top) - 2,
            hwnd, (HMENU)ID_EDIT_L, GetModuleHandleW(NULL), NULL);
        g_hEditR = CreateWindowExW(0, L"EDIT", L"20.00", es,
            rcInpR.left + 1, rcInpR.top + 1, (rcInpR.right - rcInpR.left) - 2, (rcInpR.bottom - rcInpR.top) - 2,
            hwnd, (HMENU)ID_EDIT_R, GetModuleHandleW(NULL), NULL);
        g_hEditBPS = CreateWindowExW(0, L"EDIT", L"10.00", es,
            rcInpBPS.left + 1, rcInpBPS.top + 1, (rcInpBPS.right - rcInpBPS.left) - 2, (rcInpBPS.bottom - rcInpBPS.top) - 2,
            hwnd, (HMENU)ID_EDIT_BPS, GetModuleHandleW(NULL), NULL);
        g_hEditHighCps = CreateWindowExW(0, L"EDIT", L"20.00", es,
            rcInpHighCps.left + 1, rcInpHighCps.top + 1, (rcInpHighCps.right - rcInpHighCps.left) - 2, (rcInpHighCps.bottom - rcInpHighCps.top) - 2,
            hwnd, (HMENU)ID_EDIT_HIGHCPS, GetModuleHandleW(NULL), NULL);

        g_hEditDuration = CreateWindowExW(0, L"EDIT", L"5.00", es, 0, 0, 10, 10,
            hwnd, (HMENU)ID_EDIT_DURATION, GetModuleHandleW(NULL), NULL);
        g_hEditChance   = CreateWindowExW(0, L"EDIT", L"100.00", es, 0, 0, 10, 10,
            hwnd, (HMENU)ID_EDIT_CHANCE, GetModuleHandleW(NULL), NULL);
        g_hEditStrength = CreateWindowExW(0, L"EDIT", L"55.00", es, 0, 0, 10, 10,
            hwnd, (HMENU)ID_EDIT_STRENGTH, GetModuleHandleW(NULL), NULL);

        g_hEditLimited = CreateWindowExW(0, L"EDIT", L"0.00", es, 0, 0, 10, 10,
            hwnd, (HMENU)ID_EDIT_LIMITED, GetModuleHandleW(NULL), NULL);
        HWND edits[8] = { g_hEditL, g_hEditR, g_hEditBPS, g_hEditHighCps,
                          g_hEditDuration, g_hEditChance, g_hEditStrength, g_hEditLimited };
        for (int i = 0; i < 8; ++i) {
            SendMessageW(edits[i], WM_SETFONT, (WPARAM)g_fInput, TRUE);
            SendMessageW(edits[i], EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(6, 8));
            SendMessageW(edits[i], EM_LIMITTEXT, 6, 0);
            WNDPROC prev = (WNDPROC)SetWindowLongPtrW(edits[i], GWLP_WNDPROC, (LONG_PTR)EditProc);
            if (i == 0) g_oldEditProc = prev;
        }

        EnableWindow(g_hEditBPS, FALSE);
        EnableWindow(g_hEditHighCps, FALSE);
        ShowWindow(g_hEditDuration, SW_HIDE); EnableWindow(g_hEditDuration, FALSE);
        ShowWindow(g_hEditChance,   SW_HIDE); EnableWindow(g_hEditChance,   FALSE);
        ShowWindow(g_hEditStrength, SW_HIDE); EnableWindow(g_hEditStrength, FALSE);
        ShowWindow(g_hEditLimited,  SW_HIDE); EnableWindow(g_hEditLimited,  FALSE);

        SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, WIN_W + 1, g_winH + 1, 16, 16), TRUE);
        SetTimer(hwnd, TIMER_STATUS, 100, NULL);
        g_rmbHook = SetWindowsHookExW(WH_MOUSE_LL, RmbHookProc, GetModuleHandleW(NULL), 0);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, C_BAR);
        SetTextColor(dc, C_INPUT_TXT);
        return (LRESULT)g_inputBrush;
    }

    case WM_CTLCOLORSTATIC: {

        HDC dc = (HDC)wp;
        SetBkColor(dc, C_SEL_DISBG);
        SetTextColor(dc, C_SEL_DISTXT);
        return (LRESULT)g_disabledBrush;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        int w = cr.right, h = cr.bottom;

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ ob = SelectObject(mem, bmp);
        PaintAll(mem, cr);

        // Gear panel open: a blurred, gently dimmed copy of the backdrop fades
        // in underneath it. The blur itself is built once per opening.
        double ga = g_aGear.cur;
        if (ga > 0.004) {
            if (!g_blurValid) BuildBlur(hdc, mem, w, h);
            if (g_blurValid) BlendLayer(mem, g_blurDc, w, h, ga);

            HDC layer = CreateCompatibleDC(hdc);
            HBITMAP lb = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ olb = SelectObject(layer, lb);
            BitBlt(layer, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            PaintGearPanel(layer);
            BlendLayer(mem, layer, w, h, ga);
            SelectObject(layer, olb); DeleteObject(lb); DeleteDC(layer);
        }

        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
               ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
               mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }


    case WM_TIMER:
        if (wp == TIMER_ANIM) {
            bool busy = false;
            for (int i = 0; i < (int)(sizeof(g_anims) / sizeof(g_anims[0])); ++i)
                if (AnimStep(*g_anims[i])) busy = true;
            if (!busy) {
                KillTimer(hwnd, TIMER_ANIM);
                if (g_aGear.cur == 0.0 && g_editsHidden) { g_editsHidden = false; ShowEdits(true); }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wp == TIMER_STATUS) {
            bool now = ae_IsActiveNow();
            if (now != g_lastActive || GetTickCount() < g_flashUntil + 200) {
                g_lastActive = now;
                InvalidateRect(hwnd, &rcStatusBar, FALSE);
            }

            bool tip = false;
            if (g_blatantVisible && !g_openCombo && !g_gearOpen) {
                POINT cp; GetCursorPos(&cp); ScreenToClient(hwnd, &cp);
                tip = (PtInRect(&rcCardLimited, cp) != FALSE);
            }
            if (tip != g_limitedTip) { g_limitedTip = tip; InvalidateRect(hwnd, NULL, FALSE); }
        }
        return 0;

    case WM_MOUSEMOVE: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (g_openCombo) {
            RECT base = comboRectOf(g_openCombo);
            int n = comboCount(g_openCombo), hot = -1;
            for (int i = 0; i < n; ++i) { RECT it = comboItem(base, i); if (PtInRect(&it, p)) hot = i; }
            if (hot != g_comboHot) { g_comboHot = hot; InvalidateRect(hwnd, NULL, FALSE); }
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
        if (g_hot != EL_NONE) { g_hot = EL_NONE; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;

    case WM_LBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };

        if (g_openCombo) {
            int which = g_openCombo;
            RECT base = comboRectOf(which);
            int n = comboCount(which);
            for (int i = 0; i < n; ++i) {
                RECT it = comboItem(base, i);
                if (PtInRect(&it, p)) { *comboSelOf(which) = i; break; }
            }
            g_openCombo = 0; g_comboHot = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            if (which == EL_PATTERN) UpdatePatternCards(hwnd);
            return 0;
        }

        if (PtInRect(&rcCls, p)) { DestroyWindow(hwnd); return 0; }
        if (PtInRect(&rcMin, p)) { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
        if (PtInRect(&rcGear, p)) { OpenGear(hwnd, !g_gearOpen); return 0; }

        if (g_gearOpen) {
            RECT tg = gearConsoleToggle();
            if (PtInRect(&tg, p)) {
                g_showConsole = !g_showConsole;
                g_aConsole.target = g_showConsole ? 1.0 : 0.0;
                if (g_showConsole) con_Show(hwnd); else con_Hide();
                StartAnim(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;                       // the panel stays open while you flip switches
            }
            RECT pp = gearPopupRect();
            bool insidePanel = (PtInRect(&pp, p) != FALSE);
            OpenGear(hwnd, false);
            if (insidePanel) return 0;          // never let a click fall through to the card beneath
        }

        if (PtInRect(&rcToggle, p)) {
            g_allowAll = !g_allowAll;
            g_aAllow.target = g_allowAll ? 1.0 : 0.0;
            StartAnim(hwnd);
            SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (PtInRect(&rcBlockToggle, p)) {
            g_blockHit = !g_blockHit;
            g_aBlock.target = g_blockHit ? 1.0 : 0.0;
            EnableWindow(g_hEditBPS, g_blockHit);
            StartAnim(hwnd);
            SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (PtInRect(&rcHighCpsToggle, p)) {
            g_highCps = !g_highCps;
            g_aHigh.target = g_highCps ? 1.0 : 0.0;
            EnableWindow(g_hEditHighCps, g_highCps);
            StartAnim(hwnd);
            SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (!g_allowAll && PtInRect(&rcSel, p)) {
            HWND picked[MAX_TARGETS];
            int n = ws_SelectWindows(hwnd, g_targets, g_targetCount, picked, MAX_TARGETS);
            if (n >= 0) {
                g_targetCount = n;
                for (int i = 0; i < n; ++i) g_targets[i] = picked[i];
            }
            InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (PtInRect(&rcBindL, p)) { DoBindCh(hwnd, &g_bindL, &g_bindLState, &rcBadgeL); return 0; }
        if (PtInRect(&rcBindR, p)) { DoBindCh(hwnd, &g_bindR, &g_bindRState, &rcBadgeR); return 0; }
        if (g_highCps && PtInRect(&rcBindHighCps, p)) { DoBindCh(hwnd, &g_bindHighCps, &g_bindHighCpsState, &rcBadgeHighCps); return 0; }
        if (g_blockHit && PtInRect(&rcBlockPauseBindBtn, p)) { DoBindCh(hwnd, &g_bindBlockPause, &g_bindBlockPauseState, &rcBlockPauseBadge); return 0; }
        if (PtInRect(&rcPattern, p)) { g_openCombo = EL_PATTERN; g_comboHot = -1; InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (PtInRect(&rcMode, p))    { g_openCombo = EL_MODE;    g_comboHot = -1; InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (g_blockHit && PtInRect(&rcBlockPauseMode, p)) { g_openCombo = EL_BLOCKPAUSEMODE; g_comboHot = -1; InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (PtInRect(&rcApply, p)) { DoApply(hwnd); return 0; }

        if (PtInRect(&rcTitlebar, p)) {
            ReleaseCapture();
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        SetFocus(hwnd);
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (g_openCombo) { g_openCombo = 0; g_comboHot = -1; InvalidateRect(hwnd, NULL, FALSE); return 0; }
            if (g_gearOpen)  { OpenGear(hwnd, false); return 0; }
        }
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
        FreeBlur();
        if (g_rmbHook) { UnhookWindowsHookEx(g_rmbHook); g_rmbHook = NULL; }
        KillTimer(hwnd, TIMER_STATUS);
        KillTimer(hwnd, TIMER_ANIM);
        DeleteObject(g_fTitle); DeleteObject(g_fLabel); DeleteObject(g_fHead);
        DeleteObject(g_fSmall); DeleteObject(g_fInput); DeleteObject(g_fApply);
        DeleteObject(g_fArrow); DeleteObject(g_inputBrush); DeleteObject(g_disabledBrush);
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

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"LineacAutoClickerWnd";
    RegisterClassExW(&wc);

    Layout();

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"LineAC",
        WS_POPUP | WS_CLIPCHILDREN, (sw - WIN_W) / 2, (sh - g_winH) / 2, WIN_W, g_winH,
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
    timeEndPeriod(1);
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return 0;
}
