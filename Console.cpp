#include "Console.h"
#include "LineacEngine.h"
#include <windowsx.h>

#define CON_W        238
#define HEADER_H     30
#define ROW_H        21
#define SEP_PAD      7
#define DIAG_GAP     14      // room the separator takes between the two groups
#define BOTTOM_PAD   6
#define PAD_L        14
#define TIMER_TICK   1
#define TICK_MS      150

#define K_BG      RGB(0x18,0x1b,0x23)
#define K_BAR     RGB(0x13,0x15,0x1c)
#define K_BORDER  RGB(0x2b,0x2e,0x3a)
#define K_SEP     RGB(0x27,0x2a,0x35)
#define K_ACCENT  RGB(0x1a,0x7d,0xff)
#define K_HEAD    RGB(0x45,0x4c,0x63)
#define K_LABEL   RGB(0x8a,0x93,0xad)
#define K_VALUE   RGB(0xd8,0xdc,0xea)
#define K_OFF     RGB(0x2f,0x34,0x48)
#define K_CLOSE   RGB(0xff,0x5f,0x57)

static HWND    g_wnd     = NULL;
static HWND    g_owner   = NULL;
static HFONT   g_fHead   = NULL;
static HFONT   g_fRow    = NULL;
static bool    g_visible = false;
static bool    g_closeHot = false;
static bool    g_dragging = false;
static POINT   g_dragFrom = { 0, 0 };
static int     g_posX = -1, g_posY = -1;
static int     g_conH = 0;

static RECT rcClose;

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }

static void Txt(HDC dc, const wchar_t* s, RECT rc, COLORREF c, HFONT f, UINT fmt) {
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &rc, fmt | DT_NOPREFIX);
    SelectObject(dc, of);
}

// Same two-decimal formatting the main window uses; wsprintfW has no float support.
static void fmt2(double v, wchar_t* out) {
    if (v < 0.0)    v = 0.0;
    if (v > 9999.0) v = 9999.0;
    int whole = (int)v;
    int frac  = (int)((v - whole) * 100.0 + 0.5);
    if (frac >= 100) { whole++; frac -= 100; }
    wsprintfW(out, L"%d.%02d", whole, frac);
}

static int rowTop(int i)  { return HEADER_H + i * ROW_H; }        // the four cps rows
static int sepY()         { return rowTop(4) + SEP_PAD; }
static int diagTop(int i) { return rowTop(4) + DIAG_GAP + i * ROW_H; }
static int consoleH()     { return diagTop(3) + BOTTOM_PAD; }

static void PaintConsole(HDC dc, const RECT& cr) {
    HBRUSH bg = CreateSolidBrush(K_BG);
    FillRect(dc, &cr, bg);
    DeleteObject(bg);

    RECT hd = R(0, 0, CON_W, HEADER_H);
    HBRUSH hb = CreateSolidBrush(K_BAR);
    FillRect(dc, &hd, hb);
    DeleteObject(hb);

    RECT ht = R(PAD_L, 0, CON_W - 34, HEADER_H);
    Txt(dc, L"LINEAC CONSOLE", ht, K_HEAD, g_fHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // close cross
    {
        COLORREF cc = g_closeHot ? K_CLOSE : K_OFF;
        HPEN p = CreatePen(PS_SOLID, 1, cc);
        HGDIOBJ op = SelectObject(dc, p);
        int cx = (rcClose.left + rcClose.right) / 2;
        int cy = (rcClose.top + rcClose.bottom) / 2;
        MoveToEx(dc, cx - 4, cy - 4, NULL); LineTo(dc, cx + 5, cy + 5);
        MoveToEx(dc, cx + 4, cy - 4, NULL); LineTo(dc, cx - 5, cy + 5);
        SelectObject(dc, op); DeleteObject(p);
    }

    EngineStats st = {};
    ae_GetStats(&st);

    const wchar_t* names[4] = { L"LMB", L"RMB", L"HighCPS", L"BlockHit" };
    double vals[4] = { st.cpsLeft, st.cpsRight, st.cpsHighCps, st.cpsBlockHit };

    HPEN sp = CreatePen(PS_SOLID, 1, K_SEP);
    HGDIOBJ osp = SelectObject(dc, sp);

    for (int i = 0; i < 4; ++i) {
        int t = rowTop(i);
        RECT lbl = R(PAD_L, t, CON_W / 2, t + ROW_H);
        Txt(dc, names[i], lbl, K_LABEL, g_fRow, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        wchar_t num[24], line[40];
        fmt2(vals[i], num);
        lstrcpynW(line, num, 24);
        lstrcatW(line, L" cps");
        RECT val = R(CON_W / 2, t, CON_W - PAD_L, t + ROW_H);
        Txt(dc, line, val, vals[i] > 0.0 ? K_ACCENT : K_VALUE, g_fRow,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    MoveToEx(dc, PAD_L, sepY(), NULL); LineTo(dc, CON_W - PAD_L, sepY());

    // Diagnostics. If "Timer res" stays at 1.00 ms while the main window is
    // minimised, Windows is honouring our resolution request and the rate above
    // is the real thing rather than a throttled one.
    wchar_t tr[24];
    fmt2(st.timerResMs, tr);
    lstrcatW(tr, L" ms");
    if (st.timerResMs <= 0.0) lstrcpynW(tr, L"n/a", 24);

    const wchar_t* thr = L"on";
    if (st.throttling == THROTTLE_OFF_PROCESS)     thr = L"off";
    else if (st.throttling == THROTTLE_OFF_SYSTEM) thr = L"off (system)";

    const wchar_t* dlbl[3] = { L"Timer res", L"Priority", L"Throttling" };
    const wchar_t* dval[3] = { tr, st.highPriority ? L"HIGH" : L"normal", thr };
    bool dgood[3] = { st.timerResMs > 0.0 && st.timerResMs <= 2.0,
                      st.highPriority,
                      st.throttling != THROTTLE_ACTIVE };

    for (int i = 0; i < 3; ++i) {
        int t = diagTop(i);
        RECT lbl = R(PAD_L, t, CON_W / 2, t + ROW_H);
        Txt(dc, dlbl[i], lbl, K_LABEL, g_fRow, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT val = R(CON_W / 2, t, CON_W - PAD_L, t + ROW_H);
        Txt(dc, dval[i], val, dgood[i] ? K_VALUE : K_CLOSE, g_fRow,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(dc, osp); DeleteObject(sp);

    // 1px frame drawn last so the rounded region keeps a crisp edge
    HPEN fp = CreatePen(PS_SOLID, 1, K_BORDER);
    HGDIOBJ ofp = SelectObject(dc, fp);
    HGDIOBJ obr = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, 0, 0, CON_W, g_conH, 20, 20);
    SelectObject(dc, obr); SelectObject(dc, ofp); DeleteObject(fp);
}

static LRESULT CALLBACK conProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;              // never steal focus from the game

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(mem, bmp);
        PaintConsole(mem, cr);
        BitBlt(hdc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_TIMER:
        if (wp == TIMER_TICK) InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_MOUSEMOVE: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (g_dragging) {
            POINT cur; GetCursorPos(&cur);
            g_posX = cur.x - g_dragFrom.x;
            g_posY = cur.y - g_dragFrom.y;
            SetWindowPos(hwnd, NULL, g_posX, g_posY, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        bool hot = (PtInRect(&rcClose, p) != FALSE);
        if (hot != g_closeHot) { g_closeHot = hot; InvalidateRect(hwnd, NULL, FALSE); }
        if (hot) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_closeHot) { g_closeHot = false; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;

    case WM_LBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (PtInRect(&rcClose, p)) {
            con_Hide();
            if (g_owner) PostMessageW(g_owner, WM_LINEAC_CONSOLE_CLOSED, 0, 0);
            return 0;
        }
        // WS_EX_NOACTIVATE makes the usual HTCAPTION trick unreliable, so the
        // header is dragged by hand.
        if (p.y < HEADER_H) {
            g_dragging = true;
            g_dragFrom = p;
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_dragging) { g_dragging = false; ReleaseCapture(); }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_TICK);
        g_wnd = NULL;
        g_visible = false;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ensureCreated() {
    if (g_wnd) return;

    HINSTANCE hInst = GetModuleHandleW(NULL);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = conProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = L"LineacConsoleWnd";
        RegisterClassExW(&wc);
        registered = true;
    }

    if (!g_fHead) {
        g_fHead = CreateFontW(-10, 0,0,0, FW_BOLD,   0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
        g_fRow  = CreateFontW(-12, 0,0,0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0, L"Segoe UI");
    }

    g_conH   = consoleH();
    rcClose  = R(CON_W - 30, (HEADER_H - 16) / 2, CON_W - 14, (HEADER_H - 16) / 2 + 16);

    if (g_posX < 0) {
        g_posX = GetSystemMetrics(SM_CXSCREEN) - CON_W - 28;
        g_posY = 28;
    }

    g_wnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"LineacConsoleWnd", L"LineAC Console",
        WS_POPUP,
        g_posX, g_posY, CON_W, g_conH,
        NULL, NULL, hInst, NULL);

    if (!g_wnd) return;

    SetWindowRgn(g_wnd, CreateRoundRectRgn(0, 0, CON_W + 1, g_conH + 1, 12, 12), TRUE);
    SetTimer(g_wnd, TIMER_TICK, TICK_MS, NULL);
}

void con_Show(HWND owner) {
    g_owner = owner;
    ensureCreated();
    if (!g_wnd) return;
    ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_wnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    g_visible = true;
}

void con_Hide() {
    if (!g_wnd) { g_visible = false; return; }
    if (g_dragging) { g_dragging = false; ReleaseCapture(); }
    ShowWindow(g_wnd, SW_HIDE);
    g_visible = false;
}

bool con_IsVisible() {
    return g_visible && g_wnd != NULL;
}

void con_Shutdown() {
    if (g_wnd) { DestroyWindow(g_wnd); g_wnd = NULL; }
    g_visible = false;
    if (g_fHead) { DeleteObject(g_fHead); g_fHead = NULL; }
    if (g_fRow)  { DeleteObject(g_fRow);  g_fRow  = NULL; }
}
