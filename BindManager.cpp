#include "BindManager.h"

static HHOOK g_kbHook     = NULL;
static HHOOK g_msHook     = NULL;
static int   g_capturedVk = 0;
static bool  g_capturing  = false;

static LRESULT CALLBACK kbProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_capturing &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)lParam;
        if (k->vkCode == VK_ESCAPE) g_capturedVk = 0;
        else                        g_capturedVk = (int)k->vkCode;
        g_capturing = false;
    }
    return CallNextHookEx(g_kbHook, code, wParam, lParam);
}

static LRESULT CALLBACK msProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_capturing) {
        int vk = 0;
        switch (wParam) {
            case WM_LBUTTONDOWN: vk = VK_LBUTTON; break;
            case WM_RBUTTONDOWN: vk = VK_RBUTTON; break;
            case WM_MBUTTONDOWN: vk = VK_MBUTTON; break;
            case WM_XBUTTONDOWN: {
                MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)lParam;

                int btn = HIWORD(m->mouseData);
                vk = (btn == XBUTTON1) ? VK_XBUTTON1 : VK_XBUTTON2;
                break;
            }
        }
        if (vk != 0) {
            g_capturedVk = vk;
            g_capturing  = false;
            return 1;
        }
    }
    return CallNextHookEx(g_msHook, code, wParam, lParam);
}

int bm_CaptureBind(HWND owner) {
    (void)owner;
    g_capturedVk = 0;
    g_capturing  = true;

    HINSTANCE hInst = GetModuleHandleW(NULL);
    g_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, kbProc, hInst, 0);
    g_msHook = SetWindowsHookExW(WH_MOUSE_LL,    msProc, hInst, 0);

    if (!g_kbHook || !g_msHook) {
        if (g_kbHook) { UnhookWindowsHookEx(g_kbHook); g_kbHook = NULL; }
        if (g_msHook) { UnhookWindowsHookEx(g_msHook); g_msHook = NULL; }
        g_capturing = false;
        return 0;
    }

    MSG msg;
    while (g_capturing) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
    }

    UnhookWindowsHookEx(g_kbHook); g_kbHook = NULL;
    UnhookWindowsHookEx(g_msHook); g_msHook = NULL;
    return g_capturedVk;
}

bool bm_IsDown(int vk) {
    if (vk == 0) return false;

    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void bm_DescribeKey(int vk, wchar_t* out, int cch) {
    if (vk == 0) { lstrcpynW(out, L"<none>", cch); return; }

    switch (vk) {
        case VK_LBUTTON:  lstrcpynW(out, L"Mouse L",  cch); return;
        case VK_RBUTTON:  lstrcpynW(out, L"Mouse R",  cch); return;
        case VK_MBUTTON:  lstrcpynW(out, L"Mouse M",  cch); return;
        case VK_XBUTTON1: lstrcpynW(out, L"Mouse X1", cch); return;
        case VK_XBUTTON2: lstrcpynW(out, L"Mouse X2", cch); return;
        case VK_SPACE:    lstrcpynW(out, L"Space",    cch); return;
        case VK_RETURN:   lstrcpynW(out, L"Enter",    cch); return;
    }

    UINT sc = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
    LONG lparam = (LONG)(sc << 16);

    switch (vk) {
        case VK_LEFT: case VK_RIGHT: case VK_UP:    case VK_DOWN:
        case VK_PRIOR: case VK_NEXT: case VK_HOME:  case VK_END:
        case VK_INSERT: case VK_DELETE:
            lparam |= (1L << 24); break;
    }

    wchar_t name[64] = {};
    if (GetKeyNameTextW(lparam, name, 64) > 0)
        lstrcpynW(out, name, cch);
    else
        wsprintfW(out, L"VK 0x%02X", vk);
}
