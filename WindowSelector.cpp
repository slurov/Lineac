#include "WindowSelector.h"
#include "LineacEngine.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#define SEL_W        440
#define SEL_H        474
#define IDC_LIST     1001

#define SC_BG    RGB(0x18,0x1b,0x23)
#define SC_CARD  RGB(0x20,0x23,0x2e)
#define SC_TEXT  RGB(0xb8,0xbd,0xd0)
#define SC_MUTED RGB(0x7a,0x80,0x99)

static HWND        g_dlg     = NULL;
static HWND        g_list    = NULL;
static HWND        g_owner   = NULL;
static int         g_done    = 0;
static const HWND* g_pre     = NULL;
static int         g_preCount = 0;
static HWND*       g_out      = NULL;
static int         g_maxOut   = 0;
static int         g_outCount = 0;
static int         g_rowIndex = 0;
static HBRUSH      g_bgBrush  = NULL;

static bool inPreselect(HWND h) {
    for (int i = 0; i < g_preCount; ++i) if (g_pre[i] == h) return true;
    return false;
}

static BOOL CALLBACK enumProc(HWND h, LPARAM) {
    if (!IsWindowVisible(h)) return TRUE;
    if (GetWindow(h, GW_OWNER) != NULL) return TRUE;
    if (GetWindowLongW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    if (h == g_owner) return TRUE;
    wchar_t title[256];
    if (GetWindowTextW(h, title, 256) == 0) return TRUE;

    LVITEMW it = {};
    it.mask   = LVIF_TEXT | LVIF_PARAM;
    it.iItem  = g_rowIndex;
    it.pszText = title;
    it.lParam = (LPARAM)h;
    int idx = (int)SendMessageW(g_list, LVM_INSERTITEMW, 0, (LPARAM)&it);
    if (idx >= 0 && inPreselect(h))
        ListView_SetCheckState(g_list, idx, TRUE);
    g_rowIndex++;
    return TRUE;
}

static void collectChecked() {
    g_outCount = 0;
    int cnt = ListView_GetItemCount(g_list);
    for (int i = 0; i < cnt && g_outCount < g_maxOut; ++i) {
        if (ListView_GetCheckState(g_list, i)) {
            LVITEMW it = {};
            it.mask = LVIF_PARAM;
            it.iItem = i;
            ListView_GetItem(g_list, &it);
            g_out[g_outCount++] = (HWND)it.lParam;
        }
    }
}

static LRESULT CALLBACK selProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_bgBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, SC_BG);
        SetTextColor(dc, SC_MUTED);
        return (LRESULT)g_bgBrush;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK)     { collectChecked(); g_done = 1; }
        else if (LOWORD(wp) == IDCANCEL) { g_done = 2; }
        return 0;
    case WM_CLOSE:
        g_done = 2;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int ws_SelectWindows(HWND owner, const HWND* preselect, int preCount,
                     HWND* outList, int maxOut) {
    g_owner = owner; g_pre = preselect; g_preCount = preCount;
    g_out = outList; g_maxOut = maxOut; g_outCount = 0; g_done = 0; g_rowIndex = 0;

    HINSTANCE hInst = GetModuleHandleW(NULL);
    g_bgBrush = CreateSolidBrush(SC_BG);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc   = selProc;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = L"LineacWinSelDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT wr = { 0, 0, SEL_W, SEL_H };
    AdjustWindowRectEx(&wr, style, FALSE, 0);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    RECT orc = { 0,0,0,0 };
    if (owner) GetWindowRect(owner, &orc);
    int px = orc.left + ((orc.right - orc.left) - ww) / 2;
    int py = orc.top + ((orc.bottom - orc.top) - wh) / 2;
    if (!owner) { px = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2; py = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2; }

    g_dlg = CreateWindowExW(0, L"LineacWinSelDlg", L"Select windows", style,
                            px, py, ww, wh, owner, NULL, hInst, NULL);

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND info = CreateWindowExW(0, L"STATIC",
        L"Tick the windows the clicker may work in. None ticked = all windows.",
        WS_CHILD | WS_VISIBLE, 12, 10, SEL_W - 24, 18, g_dlg, NULL, hInst, NULL);
    SendMessageW(info, WM_SETFONT, (WPARAM)font, TRUE);

    g_list = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
        12, 34, SEL_W - 24, SEL_H - 34 - 46, g_dlg, (HMENU)IDC_LIST, hInst, NULL);
    ListView_SetExtendedListViewStyle(g_list,
        LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    ListView_SetBkColor(g_list, SC_CARD);
    ListView_SetTextBkColor(g_list, SC_CARD);
    ListView_SetTextColor(g_list, SC_TEXT);

    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"Window title";
    col.cx = SEL_W - 24 - 20;
    SendMessageW(g_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

    EnumWindows(enumProc, 0);

    HWND ok = CreateWindowExW(0, L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        SEL_W - 12 - 90 - 8 - 90, SEL_H - 38, 90, 28, g_dlg, (HMENU)IDOK, hInst, NULL);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        SEL_W - 12 - 90, SEL_H - 38, 90, 28, g_dlg, (HMENU)IDCANCEL, hInst, NULL);
    SendMessageW(ok, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(cancel, WM_SETFONT, (WPARAM)font, TRUE);

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(g_dlg, SW_SHOW);
    SetForegroundWindow(g_dlg);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(g_dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_done) break;
    }

    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(g_dlg); g_dlg = NULL; g_list = NULL;
    DeleteObject(g_bgBrush); g_bgBrush = NULL;

    return (g_done == 1) ? g_outCount : -1;
}

bool ws_ForegroundInList(const HWND* list, int count) {
    if (count <= 0) return true;
    HWND fg = GetForegroundWindow();
    if (fg == NULL) return false;
    fg = GetAncestor(fg, GA_ROOT);
    for (int i = 0; i < count; ++i)
        if (list[i] == fg) return true;
    return false;
}

void ws_GetTitle(HWND hwnd, wchar_t* out, int cch) {
    if (!hwnd || !IsWindow(hwnd)) { lstrcpynW(out, L"<none>", cch); return; }
    if (GetWindowTextW(hwnd, out, cch) == 0)
        lstrcpynW(out, L"<no title>", cch);
}
