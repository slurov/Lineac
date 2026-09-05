#include "LineacEngine.h"
#include "BindManager.h"
#include "WindowSelector.h"
#include <atomic>
#include <random>

// ---------------------------------------------------------------- SDK shims
// Declared by hand so the project still builds against an older Windows SDK;
// the APIs themselves are resolved at runtime and simply skipped when absent.

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#define AE_PROCESS_POWER_THROTTLING          4   // ProcessPowerThrottling
#define AE_THROTTLING_VERSION                1
#define AE_THROTTLING_EXECUTION_SPEED        0x1
#define AE_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4

struct AePowerThrottlingState {
    ULONG Version;
    ULONG ControlMask;
    ULONG StateMask;
};

typedef BOOL (WINAPI *PFN_SetProcessInformation)(HANDLE, int, LPVOID, DWORD);
typedef LONG (NTAPI  *PFN_NtQueryTimerResolution)(PULONG, PULONG, PULONG);

// ---------------------------------------------------------------- channels

enum { CH_LEFT = 0, CH_RIGHT, CH_HIGHCPS, CH_BLOCKHIT, CH_COUNT };

struct Channel {
    HANDLE thread;
    HANDLE wake;                                  // auto-reset: pulsed when the channel switches on
    std::atomic<bool>               active;
    std::atomic<unsigned long long> clicks;
};

static Channel g_ch[CH_COUNT];

// ---------------------------------------------------------------- state

static CRITICAL_SECTION g_cs;
static Settings         g_settings = {};

// Bumped on every ae_Apply. The click loops compare it against their own copy so
// the hot path costs one atomic load instead of a lock plus a ~600 byte struct copy.
static std::atomic<unsigned> g_settingsGen(0);

// One guard per physical button. Only needed when a click has a hold duration:
// two channels share each button and their down/up pairs must not interleave.
static CRITICAL_SECTION g_csLmb, g_csRmb;

static std::atomic<bool> g_running(false);
static HANDLE            g_thInput = NULL;

static std::atomic<bool> g_leftToggled(false);
static std::atomic<bool> g_rightToggled(false);
static std::atomic<bool> g_blockPausedToggle(false);
static std::atomic<bool> g_physicalRmb(false);

static std::atomic<bool> g_highPriority(false);
static std::atomic<int>  g_throttling(THROTTLE_ACTIVE);

// ---------------------------------------------------------------- timing

static LARGE_INTEGER g_qpf = { 0 };

static inline LONGLONG qpcNow() {
    LARGE_INTEGER n;
    QueryPerformanceCounter(&n);
    return n.QuadPart;
}
static inline LONGLONG msToTicks(double ms) {
    return (LONGLONG)(ms * (double)g_qpf.QuadPart / 1000.0);
}
static inline double ticksToMs(LONGLONG t) {
    return (double)t * 1000.0 / (double)g_qpf.QuadPart;
}

// A waitable timer per thread. The high-resolution flag (Win10 1803+) makes the
// wait accurate to well under a millisecond regardless of the system-wide timer
// resolution, which is what stops the rate collapsing once we are in background.
struct HiResTimer {
    HANDLE h;
    HiResTimer() {
        h = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!h) h = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    }
    ~HiResTimer() { if (h) CloseHandle(h); }
};

static const double SPIN_MARGIN_MS = 0.4;
static const double HOP_MAX_MS     = 50.0;   // re-check g_running at least this often

static void sleepUntil(const HiResTimer& t, LONGLONG deadline) {
    for (;;) {
        if (!g_running.load(std::memory_order_relaxed)) return;
        LONGLONG now = qpcNow();
        if (now >= deadline) return;

        double left = ticksToMs(deadline - now);
        if (left <= SPIN_MARGIN_MS) break;

        double hop = left - SPIN_MARGIN_MS;
        if (hop > HOP_MAX_MS) hop = HOP_MAX_MS;

        if (t.h) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)(hop * 10000.0);   // 100ns units, negative = relative
            if (due.QuadPart == 0) break;
            if (SetWaitableTimer(t.h, &due, 0, NULL, NULL, FALSE))
                WaitForSingleObject(t.h, INFINITE);
            else
                Sleep(1);
        } else {
            DWORD coarse = (hop > 2.0) ? (DWORD)(hop - 1.0) : 0;
            if (coarse > 0) Sleep(coarse); else Sleep(0);
        }
    }
    while (g_running.load(std::memory_order_relaxed) && qpcNow() < deadline)
        YieldProcessor();
}

static void sleepFor(const HiResTimer& t, double ms) {
    if (ms <= 0.0) return;
    sleepUntil(t, qpcNow() + msToTicks(ms));
}

// ---------------------------------------------------------------- settings

static Settings snapshot() {
    EnterCriticalSection(&g_cs);
    Settings s = g_settings;
    LeaveCriticalSection(&g_cs);
    return s;
}

static double channelCps(const Settings& s, int id) {
    switch (id) {
        case CH_LEFT:    return s.leftCPS;
        case CH_RIGHT:   return s.rightCPS;
        case CH_HIGHCPS: return s.highCpsCPS;
        default:         return s.blockHitBPS;
    }
}

// Interval to the next click, in ms. Clicks are scheduled on an absolute
// cadence, so the long-run rate is 1 / mean(interval): a jitter range centred on
// 1.0 lands exactly on the configured CPS. The old Legit range (0.90..1.20)
// averaged 1.05 and therefore always ran ~5% under the number in the box.
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
        std::uniform_real_distribution<double> j(0.85, 1.15);
        ms = base * j(rng);
    }

    if (ms < 1.0) ms = 1.0;
    return ms;
}

static const double LIMITED_BURST_MS = 250.0;

static double effectiveCps(double cps, int pattern, double cpsLimit, double elapsedMs) {
    if (pattern != PATTERN_BLATANT)    return cps;
    if (cpsLimit <= 0.0)               return cps;
    if (cps <= cpsLimit)               return cps;
    if (elapsedMs < LIMITED_BURST_MS)  return cps;
    return cpsLimit;
}

// ---------------------------------------------------------------- clicking

static void emitClick(bool right, double holdMs, const HiResTimer& t) {
    INPUT in[2] = {};
    in[0].type = INPUT_MOUSE;
    in[1].type = INPUT_MOUSE;
    in[0].mi.dwFlags = right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    in[1].mi.dwFlags = right ? MOUSEEVENTF_RIGHTUP   : MOUSEEVENTF_LEFTUP;

    if (holdMs <= 0.0) {
        // A single batched call. SendInput guarantees a batch is not interspersed
        // with input from anywhere else, so the two channels that share this
        // button can no longer interleave into down,down,up,up -- which games
        // count as one click instead of two.
        SendInput(2, in, sizeof(INPUT));
        return;
    }

    CRITICAL_SECTION* guard = right ? &g_csRmb : &g_csLmb;
    EnterCriticalSection(guard);
    SendInput(1, &in[0], sizeof(INPUT));
    sleepFor(t, holdMs);
    SendInput(1, &in[1], sizeof(INPUT));
    LeaveCriticalSection(guard);
}

static DWORD WINAPI clickWorker(LPVOID param) {
    const int  id    = (int)(INT_PTR)param;
    const bool right = (id == CH_RIGHT || id == CH_BLOCKHIT);
    Channel&   ch    = g_ch[id];

    HiResTimer timer;
    Settings   s   = snapshot();
    unsigned   gen = g_settingsGen.load(std::memory_order_acquire);

    bool     firing  = false;
    LONGLONG next    = 0;
    LONGLONG burstAt = 0;

    while (g_running.load(std::memory_order_relaxed)) {
        unsigned g = g_settingsGen.load(std::memory_order_acquire);
        if (g != gen) { s = snapshot(); gen = g; firing = false; }

        double cps = channelCps(s, id);
        if (!ch.active.load(std::memory_order_relaxed) || cps <= 0.0) {
            firing = false;
            WaitForSingleObject(ch.wake, 50);   // parked: no idle spinning
            continue;
        }

        LONGLONG now = qpcNow();
        if (!firing) {
            firing  = true;
            burstAt = now;
            next    = now;                      // first click of a press goes out at once
        }

        double hold   = (s.pattern == PATTERN_CUSTOM) ? s.customDuration : 0.0;
        double eff    = effectiveCps(cps, s.pattern, s.limitedCps, ticksToMs(now - burstAt));
        double period = calcDelay(eff, s.pattern, s.customChance, s.customStrength);

        emitClick(right, hold, timer);
        ch.clicks.fetch_add(1, std::memory_order_relaxed);

        // Absolute cadence: the next click is one period after the previous
        // deadline, not after this click finished. SendInput cost and the Custom
        // hold duration therefore stop eating into the rate.
        next += msToTicks(period);
        LONGLONG after = qpcNow();
        if (next < after) next = after;         // fell behind -- resync, never burst to catch up

        sleepUntil(timer, next);
    }
    return 0;
}

// ---------------------------------------------------------------- input poll

static void setActive(int id, bool on) {
    bool was = g_ch[id].active.exchange(on, std::memory_order_relaxed);
    if (on && !was) SetEvent(g_ch[id].wake);
}

// Sole owner of all key/window polling. The click threads never touch
// GetAsyncKeyState or GetForegroundWindow, they just read their active flag.
static DWORD WINAPI inputThread(LPVOID) {
    bool prevLeftDown = false, prevRightDown = false, prevPauseDown = false;

    bool     winOk    = true;
    LONGLONG winOkAt  = 0;
    const LONGLONG winOkTtl = msToTicks(25.0);

    Settings s   = snapshot();
    unsigned gen = g_settingsGen.load(std::memory_order_acquire);

    while (g_running.load(std::memory_order_relaxed)) {
        unsigned g = g_settingsGen.load(std::memory_order_acquire);
        if (g != gen) {
            s = snapshot();
            gen = g;
            prevLeftDown = prevRightDown = prevPauseDown = false;
            winOkAt = 0;
        }

        LONGLONG now = qpcNow();
        if (s.allowAll) {
            winOk = true;
        } else if (winOkAt == 0 || now - winOkAt >= winOkTtl) {
            winOk   = ws_ForegroundInList(s.targets, s.targetCount);
            winOkAt = now;
        }

        bool lDown = (s.bindL != 0) && bm_IsDown(s.bindL);
        bool lOn;
        if (s.holdMode) {
            lOn = lDown;
        } else {
            if (lDown && !prevLeftDown) g_leftToggled.store(!g_leftToggled.load());
            lOn = g_leftToggled.load();
        }
        prevLeftDown = lDown;

        bool rDown = (s.bindR != 0) && bm_IsDown(s.bindR);
        bool rOn;
        if (s.holdMode) {
            rOn = rDown;
        } else {
            if (rDown && !prevRightDown) g_rightToggled.store(!g_rightToggled.load());
            rOn = g_rightToggled.load();
        }
        prevRightDown = rDown;

        bool hOn = (s.highCpsBind != 0) && bm_IsDown(s.highCpsBind);

        bool pDown = (s.blockHitPauseBind != 0) && bm_IsDown(s.blockHitPauseBind);
        if (!s.blockHitPauseHold && pDown && !prevPauseDown)
            g_blockPausedToggle.store(!g_blockPausedToggle.load());
        prevPauseDown = pDown;

        bool paused = (s.blockHitPauseBind != 0) &&
                      (s.blockHitPauseHold ? pDown : g_blockPausedToggle.load());

        setActive(CH_LEFT,     s.leftEnabled     && s.leftCPS     > 0.0 && lOn && winOk);
        setActive(CH_RIGHT,    s.rightEnabled    && s.rightCPS    > 0.0 && rOn && winOk);
        setActive(CH_HIGHCPS,  s.highCpsEnabled  && s.highCpsCPS  > 0.0 && hOn && winOk);
        setActive(CH_BLOCKHIT, s.blockHitEnabled && s.blockHitBPS > 0.0 &&
                               g_physicalRmb.load() && winOk && !paused);

        Sleep(1);
    }

    for (int i = 0; i < CH_COUNT; ++i) {
        g_ch[i].active.store(false, std::memory_order_relaxed);
        if (g_ch[i].wake) SetEvent(g_ch[i].wake);
    }
    return 0;
}

// ---------------------------------------------------------------- priority

// Machines with PowerThrottlingOff=1 have the feature switched off for every
// process; SetProcessInformation then fails with ERROR_INVALID_FUNCTION and there
// is genuinely nothing left to opt out of.
static bool throttlingDisabledSystemWide() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Power\\PowerThrottling",
                      0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return false;

    DWORD val = 0, sz = sizeof(val), type = 0;
    bool off = RegQueryValueExW(k, L"PowerThrottlingOff", NULL, &type,
                                (LPBYTE)&val, &sz) == ERROR_SUCCESS
               && type == REG_DWORD && val != 0;
    RegCloseKey(k);
    return off;
}

static void applyProcessPriority() {
    if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        g_highPriority.store(true);

    // Windows 10 1709+ parks background processes in EcoQoS and quietly ignores
    // their timeBeginPeriod request, which drops Sleep granularity to ~15.6 ms.
    // Opting out here is what keeps the rate steady once the window is minimised.
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    PFN_SetProcessInformation fn = k32
        ? (PFN_SetProcessInformation)GetProcAddress(k32, "SetProcessInformation")
        : NULL;

    if (fn) {
        AePowerThrottlingState ps = {};
        ps.Version     = AE_THROTTLING_VERSION;
        ps.ControlMask = AE_THROTTLING_EXECUTION_SPEED | AE_THROTTLING_IGNORE_TIMER_RESOLUTION;
        ps.StateMask   = 0;                        // 0 = throttling disabled

        if (fn(GetCurrentProcess(), AE_PROCESS_POWER_THROTTLING, &ps, sizeof(ps))) {
            g_throttling.store(THROTTLE_OFF_PROCESS);
            return;
        }
    }

    if (throttlingDisabledSystemWide())
        g_throttling.store(THROTTLE_OFF_SYSTEM);
}

// ---------------------------------------------------------------- lifecycle

void ae_Start() {
    QueryPerformanceFrequency(&g_qpf);
    InitializeCriticalSection(&g_cs);
    InitializeCriticalSection(&g_csLmb);
    InitializeCriticalSection(&g_csRmb);

    applyProcessPriority();

    g_running.store(true);

    for (int i = 0; i < CH_COUNT; ++i) {
        g_ch[i].wake = CreateEventW(NULL, FALSE, FALSE, NULL);
        g_ch[i].active.store(false);
        g_ch[i].clicks.store(0);
    }

    g_thInput = CreateThread(NULL, 0, inputThread, NULL, 0, NULL);
    if (g_thInput) SetThreadPriority(g_thInput, THREAD_PRIORITY_HIGHEST);

    for (int i = 0; i < CH_COUNT; ++i) {
        g_ch[i].thread = CreateThread(NULL, 0, clickWorker, (LPVOID)(INT_PTR)i, 0, NULL);
        if (g_ch[i].thread) SetThreadPriority(g_ch[i].thread, THREAD_PRIORITY_HIGHEST);
    }
}

void ae_Stop() {
    g_running.store(false);
    for (int i = 0; i < CH_COUNT; ++i)
        if (g_ch[i].wake) SetEvent(g_ch[i].wake);

    HANDLE valid[1 + CH_COUNT];
    int n = 0;
    if (g_thInput) valid[n++] = g_thInput;
    for (int i = 0; i < CH_COUNT; ++i)
        if (g_ch[i].thread) valid[n++] = g_ch[i].thread;

    DWORD wait = WAIT_OBJECT_0;
    if (n > 0) wait = WaitForMultipleObjects((DWORD)n, valid, TRUE, 2000);

    if (g_thInput) { CloseHandle(g_thInput); g_thInput = NULL; }
    for (int i = 0; i < CH_COUNT; ++i)
        if (g_ch[i].thread) { CloseHandle(g_ch[i].thread); g_ch[i].thread = NULL; }

    // Only tear down what threads could still be sitting inside once every one of
    // them is confirmed gone; on timeout the process is exiting anyway.
    if (wait != WAIT_TIMEOUT) {
        for (int i = 0; i < CH_COUNT; ++i)
            if (g_ch[i].wake) { CloseHandle(g_ch[i].wake); g_ch[i].wake = NULL; }
        DeleteCriticalSection(&g_cs);
        DeleteCriticalSection(&g_csLmb);
        DeleteCriticalSection(&g_csRmb);
    }
}

bool ae_IsActiveNow() {
    for (int i = 0; i < CH_COUNT; ++i)
        if (g_ch[i].active.load(std::memory_order_relaxed)) return true;
    return false;
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
    g_blockPausedToggle.store(false);
    for (int i = 0; i < CH_COUNT; ++i)
        g_ch[i].active.store(false, std::memory_order_relaxed);

    g_settingsGen.fetch_add(1, std::memory_order_release);
}

// ---------------------------------------------------------------- stats

static double queryTimerResMs() {
    static PFN_NtQueryTimerResolution fn = NULL;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE nt = GetModuleHandleW(L"ntdll.dll");
        if (nt) fn = (PFN_NtQueryTimerResolution)GetProcAddress(nt, "NtQueryTimerResolution");
    }
    if (!fn) return 0.0;

    ULONG mn = 0, mx = 0, cur = 0;
    if (fn(&mn, &mx, &cur) != 0) return 0.0;
    return (double)cur / 10000.0;                  // 100ns units -> ms
}

struct StatSample {
    unsigned long long clicks;
    LONGLONG           at;
    double             cps;
};
static StatSample g_stat[CH_COUNT] = {};

// Called from the UI thread only (the console's timer).
void ae_GetStats(EngineStats* out) {
    if (!out) return;

    LONGLONG now = qpcNow();
    double cps[CH_COUNT] = {};

    for (int i = 0; i < CH_COUNT; ++i) {
        unsigned long long c = g_ch[i].clicks.load(std::memory_order_relaxed);
        StatSample& p = g_stat[i];

        if (p.at == 0) { p.at = now; p.clicks = c; p.cps = 0.0; }

        double dtMs = ticksToMs(now - p.at);
        if (dtMs > 2000.0) {
            p.at = now; p.clicks = c; p.cps = 0.0;   // stale baseline (console was closed)
        } else if (dtMs >= 120.0) {
            double inst = (double)(c - p.clicks) * 1000.0 / dtMs;
            bool   idle = (c == p.clicks) && !g_ch[i].active.load(std::memory_order_relaxed);
            p.cps    = idle ? 0.0 : (p.cps * 0.4 + inst * 0.6);
            p.clicks = c;
            p.at     = now;
        }
        cps[i] = p.cps;
    }

    out->cpsLeft       = cps[CH_LEFT];
    out->cpsRight      = cps[CH_RIGHT];
    out->cpsHighCps    = cps[CH_HIGHCPS];
    out->cpsBlockHit   = cps[CH_BLOCKHIT];
    out->timerResMs   = queryTimerResMs();
    out->highPriority = g_highPriority.load();
    out->throttling   = g_throttling.load();
}
