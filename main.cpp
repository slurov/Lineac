#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <stdlib.h>
#include "LineacEngine.h"
#include "BindManager.h"
#include "WindowSelector.h"

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
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
#define C_DOT_MAX       RGB(0x27,0xc9,0x3f)
#define C_DOT_CLS       RGB(0xff,0x5f,0x57)
#define C_POPUP_TXT     RGB(0x7a,0x80,0x99)
#define C_POPUP_TXT1    RGB(0x9b,0xa3,0xb8)

enum { EL_NONE, EL_TOGGLE, EL_SEL, EL_BINDL, EL_BINDR,
       EL_PATTERN, EL_MODE, EL_APPLY, EL_INFO, EL_MIN, EL_MAX, EL_CLS,
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
static bool g_infoOpen   = false;
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

static RECT rcTitlebar, rcMin, rcMax, rcCls;
static RECT rcCardWindow, rcCardLMB, rcCardRMB, rcCardSettings, rcCardBlockHit, rcCardHighCps;
static RECT rcCardCustom;
static RECT rcToggle, rcSel, rcWinLbl;
static RECT rcInpL, rcBindL, rcBadgeL, rcInpR, rcBindR, rcBadgeR;
static RECT rcPattern, rcMode, rcBlockToggle, rcInpBPS, rcApply, rcStatusBar, rcInfo;
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
    int maxCx = clsCx - 20;
    int minCx = maxCx - 20;
    rcMin = R(minCx - 7, cy - 7, minCx + 7, cy + 7);
    rcMax = R(maxCx - 7, cy - 7, maxCx + 7, cy + 7);
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
    rcInfo = R(WIN_W - PAD_X - 16, rcStatusBar.top + (STATUS_H - 16) / 2,
               WIN_W - PAD_X, rcStatusBar.top + (STATUS_H - 16) / 2 + 16);
}

static RECT comboItem(const RECT& cmb, int i) {
    int top = cmb.bottom + 4 + i * ITEM_H;
    return R(cmb.left, top, cmb.right, top + ITEM_H);
}
static RECT infoPopupRect() {
    int rgt = WIN_W - PAD_X, w = 212, h = 70;
    int bot = rcStatusBar.top - 8;
    return R(rgt - w, bot - h, rgt, bot);
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
    FillCircle(dc, (rcMax.left + rcMax.right) / 2, (rcMax.top + rcMax.bottom) / 2, 7, C_DOT_MAX);
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

    FillRound(dc, rcToggle, (rcToggle.bottom - rcToggle.top) / 2, g_allowAll ? C_ACCENT : C_TOGGLE_OFF);
    int thumbX = g_allowAll ? rcToggle.right - 3 - 15 : rcToggle.left + 3;
    FillCircle(dc, thumbX + 7, rcToggle.top + 3 + 7, 7, RGB(255,255,255));

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
    DrawButton(dc, rcBindL, L"Set bind", g_hot == EL_BINDL ? C_BINDBG_HOVER : C_BINDBG, C_BORDER, C_BIND_TXT, g_fSmall);
    DrawBadge(dc, rcBadgeL, g_bindLState, g_bindL);

    RECT lblR = R(rcCardRMB.left + 14, rcCardRMB.top + HEAD_H, rcInpR.left - 8, rcCardRMB.top + HEAD_H + ROW_H);
    Txt(dc, L"CPS", lblR, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpR, 5, C_BORDER);
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
        for (int i = 0; i < 3; ++i) {
            RECT lbl = R(rcCardCustom.left + 14, crc[i].top - (ROW_H - 27) / 2,
                         crc[i].left - 8, crc[i].top - (ROW_H - 27) / 2 + ROW_H);
            Txt(dc, clbl[i], lbl, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            FrameRound(dc, crc[i], 5, C_BORDER);
        }
    }

    if (g_blatantVisible) {
        RECT lblLim = R(rcCardLimited.left + 14, rcInpLimited.top - (ROW_H - 27) / 2,
                        rcInpLimited.left - 8, rcInpLimited.top - (ROW_H - 27) / 2 + ROW_H);
        Txt(dc, L"Max CPS limit", lblLim, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        FrameRound(dc, rcInpLimited, 5, C_BORDER);
    }

    RECT lblBH = R(rcCardBlockHit.left + 14, rcCardBlockHit.top + HEAD_H,
                   rcBlockToggle.left - 8, rcCardBlockHit.top + HEAD_H + ROW_H);
    Txt(dc, L"Enable BlockHit", lblBH, C_LABEL, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FillRound(dc, rcBlockToggle, (rcBlockToggle.bottom - rcBlockToggle.top) / 2,
              g_blockHit ? C_ACCENT : C_TOGGLE_OFF);
    int bthumbX = g_blockHit ? rcBlockToggle.right - 3 - 15 : rcBlockToggle.left + 3;
    FillCircle(dc, bthumbX + 7, rcBlockToggle.top + 3 + 7, 7, RGB(255,255,255));
    RECT lblBPS = R(rcCardBlockHit.left + 14, rcInpBPS.top - (ROW_H - 27) / 2,
                    rcInpBPS.left - 8, rcInpBPS.top - (ROW_H - 27) / 2 + ROW_H);
    Txt(dc, L"BPS (hold RMB)", lblBPS, g_blockHit ? C_LABEL : C_SEL_DISTXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpBPS, 5, g_blockHit ? C_BORDER : C_CARDBORDER);

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
    FillRound(dc, rcHighCpsToggle, (rcHighCpsToggle.bottom - rcHighCpsToggle.top) / 2,
              g_highCps ? C_ACCENT : C_TOGGLE_OFF);
    int hthumbX = g_highCps ? rcHighCpsToggle.right - 3 - 15 : rcHighCpsToggle.left + 3;
    FillCircle(dc, hthumbX + 7, rcHighCpsToggle.top + 3 + 7, 7, RGB(255,255,255));
    RECT lblHCcps = R(rcCardHighCps.left + 14, rcInpHighCps.top - (ROW_H - 27) / 2,
                      rcInpHighCps.left - 8, rcInpHighCps.top - (ROW_H - 27) / 2 + ROW_H);
    Txt(dc, L"CPS", lblHCcps, g_highCps ? C_LABEL : C_SEL_DISTXT, g_fLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    FrameRound(dc, rcInpHighCps, 5, g_highCps ? C_BORDER : C_CARDBORDER);
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
    bool running = ae_IsActiveNow();
    const wchar_t* st; COLORREF stc; bool dotOn;
    if (GetTickCount() < g_flashUntil) { st = L"Status: Settings applied"; stc = C_STAT_RUN; dotOn = true; }
    else if (running)                  { st = L"Status: Running";          stc = C_STAT_RUN; dotOn = true; }
    else                               { st = L"Status: Stopped";          stc = C_MUTED;    dotOn = false; }
    FillCircle(dc, PAD_X + 3, rcStatusBar.top + STATUS_H / 2, 3, dotOn ? C_ACCENT : C_DOT_OFF);
    RECT stt = R(PAD_X + 11, rcStatusBar.top, WIN_W - 40, rcStatusBar.bottom);
    Txt(dc, st, stt, stc, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int icx = (rcInfo.left + rcInfo.right) / 2, icy = (rcInfo.top + rcInfo.bottom) / 2;
    COLORREF icol = (g_hot == EL_INFO || g_infoOpen) ? C_ACCENT : C_DOT_OFF;
    HPEN ip = CreatePen(PS_SOLID, 1, icol); HGDIOBJ oip = SelectObject(dc, ip);
    HGDIOBJ oib = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, icx - 6, icy - 6, icx + 6, icy + 6);
    SelectObject(dc, oib); SelectObject(dc, oip); DeleteObject(ip);
    FillCircle(dc, icx, icy - 2, 1, icol);
    HPEN ip2 = CreatePen(PS_SOLID, 1, icol); HGDIOBJ oip2 = SelectObject(dc, ip2);
    MoveToEx(dc, icx, icy, NULL); LineTo(dc, icx, icy + 4);
    SelectObject(dc, oip2); DeleteObject(ip2);

    if (g_infoOpen) {
        RECT pp = infoPopupRect();
        FillRound(dc, pp, 6, C_CARD);
        FrameRound(dc, pp, 6, C_CARDBORDER);
        RECT l1 = R(pp.left + 13, pp.top + 9, pp.right - 13, pp.top + 26);
        RECT l2 = R(pp.left + 13, pp.top + 28, pp.right - 13, pp.top + 45);
        RECT l3 = R(pp.left + 13, pp.top + 46, pp.right - 13, pp.top + 63);
        Txt(dc, L"This autoclicker was made by @sznc", l1, C_POPUP_TXT1, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        Txt(dc, L"YT — @sznc",       l2, C_POPUP_TXT, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        Txt(dc, L"TikTok — @szncccc", l3, C_POPUP_TXT, g_fSmall, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

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
    if (PtInRect(&rcMax, p)) return EL_MAX;
    if (PtInRect(&rcInfo, p)) return EL_INFO;
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
        g_hEditL = CreateWindowExW(0, L"EDIT", L"10.00", es,
            rcInpL.left + 1, rcInpL.top + 1, (rcInpL.right - rcInpL.left) - 2, (rcInpL.bottom - rcInpL.top) - 2,
            hwnd, (HMENU)ID_EDIT_L, GetModuleHandleW(NULL), NULL);
        g_hEditR = CreateWindowExW(0, L"EDIT", L"10.00", es,
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
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(mem, bmp);
        PaintAll(mem, cr);
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
               ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
               mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wp == TIMER_STATUS) {
            bool now = ae_IsActiveNow();
            if (now != g_lastActive || GetTickCount() < g_flashUntil + 200) {
                g_lastActive = now;
                InvalidateRect(hwnd, &rcStatusBar, FALSE);
            }

            bool tip = false;
            if (g_blatantVisible && !g_openCombo && !g_infoOpen) {
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

        if (g_infoOpen) { g_infoOpen = false; InvalidateRect(hwnd, NULL, FALSE); }

        if (PtInRect(&rcCls, p)) { DestroyWindow(hwnd); return 0; }
        if (PtInRect(&rcMin, p)) { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
        if (PtInRect(&rcMax, p)) { return 0; }
        if (PtInRect(&rcInfo, p)) { g_infoOpen = !g_infoOpen; InvalidateRect(hwnd, NULL, FALSE); return 0; }

        if (PtInRect(&rcToggle, p)) {
            g_allowAll = !g_allowAll;
            SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (PtInRect(&rcBlockToggle, p)) {
            g_blockHit = !g_blockHit;
            EnableWindow(g_hEditBPS, g_blockHit);
            SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (PtInRect(&rcHighCpsToggle, p)) {
            g_highCps = !g_highCps;
            EnableWindow(g_hEditHighCps, g_highCps);
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
            if (g_infoOpen)  { g_infoOpen = false; InvalidateRect(hwnd, NULL, FALSE); return 0; }
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_rmbHook) { UnhookWindowsHookEx(g_rmbHook); g_rmbHook = NULL; }
        KillTimer(hwnd, TIMER_STATUS);
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
