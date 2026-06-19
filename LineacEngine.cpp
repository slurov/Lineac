#include "LineacEngine.h"
#include "BindManager.h"
#include "WindowSelector.h"
#include <atomic>
#include <random>

static CRITICAL_SECTION g_cs;
static Settings         g_settings = {};

static std::atomic<bool> g_running(false);

static HANDLE g_thInput    = NULL;
static HANDLE g_thLeft     = NULL;
static HANDLE g_thRight    = NULL;
static HANDLE g_thHighCps  = NULL;
static HANDLE g_thBlockHit = NULL;

static std::atomic<bool> g_leftActive(false);
static std::atomic<bool> g_rightActive(false);
static std::atomic<bool> g_highCpsActive(false);

static std::atomic<bool> g_leftToggled(false);
static std::atomic<bool> g_rightToggled(false);
static std::atomic<bool> g_blockPausedToggle(false);

static std::atomic<bool> g_physicalRmb(false);

static Settings snapshot() {
    EnterCriticalSection(&g_cs);
    Settings s = g_settings;
    LeaveCriticalSection(&g_cs);
    return s;
}

static double calcDelay(double cps, int pattern, double chance, double strength) {

    static thread_local std::mt19937 rng(GetCurrentThreadId() ^ GetTickCount());

    double base = 1000.0 / cps;
    double ms;

    if (pattern == PATTERN_CUSTOM) {

        ms = base;
        std::uniform_real_distribution<double> roll(0.0, 100.0);
        if (roll(rng) <= chance) {
            double s = strength / 100.0;
            std::uniform_real_distribution<double> dev(-s, s);
            ms = base * (1.0 + dev(rng));
        }
        if (ms < 0.5)    ms = 0.5;
        if (ms > 1000.0) ms = 1000.0;
        return ms;
    }

    if (pattern == PATTERN_BLATANT) {

        std::uniform_real_distribution<double> j(0.98, 1.02);
        ms = base * j(rng);
    } else {

        std::uniform_real_distribution<double> j(0.90, 1.20);
        ms = base * j(rng);
    }

    if (ms < 1.0) ms = 1.0;
    return ms;
}

static const DWORD LIMITED_BURST_MS = 250;

static double effectiveCps(double cps, int pattern, double cpsLimit, DWORD elapsedMs) {
    if (pattern != PATTERN_BLATANT)   return cps;
    if (cpsLimit <= 0.0)              return cps;
    if (cps <= cpsLimit)              return cps;
    if (elapsedMs < LIMITED_BURST_MS) return cps;
    return cpsLimit;
}

static void preciseSleep(double ms) {
    if (ms <= 0.0) return;
    static LARGE_INTEGER qpf = { 0 };
    if (qpf.QuadPart == 0) QueryPerformanceFrequency(&qpf);
    LARGE_INTEGER start; QueryPerformanceCounter(&start);
    LONGLONG target = start.QuadPart + (LONGLONG)(ms * (double)qpf.QuadPart / 1000.0);
    if (ms >= 10.0) {
        DWORD coarse = (DWORD)(ms - 2.0);
        if (coarse > 0) Sleep(coarse);
    }
    LARGE_INTEGER now;
    do { QueryPerformanceCounter(&now); } while (now.QuadPart < target);
}

static void ae_ClickLeft(double durationMs) {
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &in, sizeof(INPUT));
    if (durationMs > 0.0) preciseSleep(durationMs);
    in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &in, sizeof(INPUT));
}

static void ae_ClickRight(double durationMs) {
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    SendInput(1, &in, sizeof(INPUT));
    if (durationMs > 0.0) preciseSleep(durationMs);
    in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(1, &in, sizeof(INPUT));
}

static DWORD WINAPI inputThread(LPVOID) {
    bool prevLeftDown = false, prevRightDown = false, prevPauseDown = false;

    while (g_running.load()) {
        Settings s = snapshot();

        bool lDown = (s.bindL != 0) && bm_IsDown(s.bindL);
        if (s.holdMode) {
            g_leftActive.store(lDown);
        } else {
            if (lDown && !prevLeftDown)
                g_leftToggled.store(!g_leftToggled.load());
            g_leftActive.store(g_leftToggled.load());
        }
        prevLeftDown = lDown;

        bool rDown = (s.bindR != 0) && bm_IsDown(s.bindR);
        if (s.holdMode) {
            g_rightActive.store(rDown);
        } else {
            if (rDown && !prevRightDown)
                g_rightToggled.store(!g_rightToggled.load());
            g_rightActive.store(g_rightToggled.load());
        }
        prevRightDown = rDown;

        g_highCpsActive.store(s.highCpsBind != 0 && bm_IsDown(s.highCpsBind));

        bool pDown = (s.blockHitPauseBind != 0) && bm_IsDown(s.blockHitPauseBind);
        if (!s.blockHitPauseHold && pDown && !prevPauseDown)
            g_blockPausedToggle.store(!g_blockPausedToggle.load());
        prevPauseDown = pDown;

        Sleep(1);
    }
    return 0;
}

static bool windowOk(const Settings& s) {
    if (s.allowAll) return true;
    return ws_ForegroundInList(s.targets, s.targetCount);
}

static bool blockHitPaused(const Settings& s) {
    if (s.blockHitPauseBind == 0) return false;
    return s.blockHitPauseHold ? bm_IsDown(s.blockHitPauseBind)
                               : g_blockPausedToggle.load();
}

static DWORD WINAPI leftClickThread(LPVOID) {
    bool prevFire = false; DWORD burstStart = 0;
    while (g_running.load()) {
        Settings s = snapshot();

        bool active = s.holdMode ? (s.bindL != 0 && bm_IsDown(s.bindL))
                                 : g_leftActive.load();
        bool fire = s.leftEnabled && s.leftCPS > 0.0 && active && windowOk(s);
        if (fire && !prevFire) burstStart = GetTickCount();
        prevFire = fire;
        if (fire) {
            double dur = (s.pattern == PATTERN_CUSTOM) ? s.customDuration : 0.0;
            double eff = effectiveCps(s.leftCPS, s.pattern, s.limitedCps, GetTickCount() - burstStart);
            ae_ClickLeft(dur);
            preciseSleep(calcDelay(eff, s.pattern, s.customChance, s.customStrength));
        } else {
            Sleep(1);
        }
    }
    return 0;
}

static DWORD WINAPI rightClickThread(LPVOID) {
    bool prevFire = false; DWORD burstStart = 0;
    while (g_running.load()) {
        Settings s = snapshot();
        bool fire = s.rightEnabled && s.rightCPS > 0.0 &&
                    g_rightActive.load() && windowOk(s);
        if (fire && !prevFire) burstStart = GetTickCount();
        prevFire = fire;
        if (fire) {
            double dur = (s.pattern == PATTERN_CUSTOM) ? s.customDuration : 0.0;
            double eff = effectiveCps(s.rightCPS, s.pattern, s.limitedCps, GetTickCount() - burstStart);
            ae_ClickRight(dur);
            preciseSleep(calcDelay(eff, s.pattern, s.customChance, s.customStrength));
        } else {
            Sleep(1);
        }
    }
    return 0;
}

static DWORD WINAPI highCpsThread(LPVOID) {
    bool prevFire = false; DWORD burstStart = 0;
    while (g_running.load()) {
        Settings s = snapshot();
        bool active = (s.highCpsBind != 0 && bm_IsDown(s.highCpsBind));
        bool fire = s.highCpsEnabled && s.highCpsCPS > 0.0 && active && windowOk(s);
        if (fire && !prevFire) burstStart = GetTickCount();
        prevFire = fire;
        if (fire) {
            double dur = (s.pattern == PATTERN_CUSTOM) ? s.customDuration : 0.0;
            double eff = effectiveCps(s.highCpsCPS, s.pattern, s.limitedCps, GetTickCount() - burstStart);
            ae_ClickLeft(dur);
            preciseSleep(calcDelay(eff, s.pattern, s.customChance, s.customStrength));
        } else {
            Sleep(1);
        }
    }
    return 0;
}

static DWORD WINAPI blockHitThread(LPVOID) {
    bool prevFire = false; DWORD burstStart = 0;
    while (g_running.load()) {
        Settings s = snapshot();

        bool fire = s.blockHitEnabled && s.blockHitBPS > 0.0 &&
                    g_physicalRmb.load() && windowOk(s) && !blockHitPaused(s);
        if (fire && !prevFire) burstStart = GetTickCount();
        prevFire = fire;
        if (fire) {
            double dur = (s.pattern == PATTERN_CUSTOM) ? s.customDuration : 0.0;
            double eff = effectiveCps(s.blockHitBPS, s.pattern, s.limitedCps, GetTickCount() - burstStart);
            ae_ClickRight(dur);
            preciseSleep(calcDelay(eff, s.pattern, s.customChance, s.customStrength));
        } else {
            Sleep(1);
        }
    }
    return 0;
}

void ae_Start() {
    InitializeCriticalSection(&g_cs);
    g_running.store(true);
    g_thInput    = CreateThread(NULL, 0, inputThread,      NULL, 0, NULL);
    g_thLeft     = CreateThread(NULL, 0, leftClickThread,  NULL, 0, NULL);
    g_thRight    = CreateThread(NULL, 0, rightClickThread, NULL, 0, NULL);
    g_thHighCps  = CreateThread(NULL, 0, highCpsThread,    NULL, 0, NULL);
    g_thBlockHit = CreateThread(NULL, 0, blockHitThread,   NULL, 0, NULL);
}

void ae_Stop() {
    g_running.store(false);

    HANDLE handles[5] = { g_thInput, g_thLeft, g_thRight, g_thHighCps, g_thBlockHit };
    int n = 0;
    HANDLE valid[5];
    for (int i = 0; i < 5; ++i)
        if (handles[i]) valid[n++] = handles[i];
    if (n > 0)
        WaitForMultipleObjects(n, valid, TRUE, 2000);

    for (int i = 0; i < 5; ++i)
        if (handles[i]) { CloseHandle(handles[i]); }
    g_thInput = g_thLeft = g_thRight = g_thHighCps = g_thBlockHit = NULL;

    DeleteCriticalSection(&g_cs);
}

bool ae_IsActiveNow() {
    Settings s = snapshot();
    bool l  = s.leftEnabled && s.leftCPS > 0.0 && g_leftActive.load() && windowOk(s);
    bool hc = s.highCpsEnabled && s.highCpsCPS > 0.0 && g_highCpsActive.load() && windowOk(s);
    bool bh = s.blockHitEnabled && s.blockHitBPS > 0.0 &&
              g_physicalRmb.load() && windowOk(s) && !blockHitPaused(s);
    bool r  = bh || (s.rightEnabled && s.rightCPS > 0.0 && g_rightActive.load() && windowOk(s));
    return l || hc || r;
}

void ae_SetRmbPhysical(bool down) {
    g_physicalRmb.store(down);
}

void ae_Apply(const Settings& s) {
    EnterCriticalSection(&g_cs);
    g_settings = s;
    LeaveCriticalSection(&g_cs);

    g_leftToggled.store(false);
    g_rightToggled.store(false);
    g_leftActive.store(false);
    g_rightActive.store(false);
    g_highCpsActive.store(false);
    g_blockPausedToggle.store(false);
}
