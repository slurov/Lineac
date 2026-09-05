#pragma once
#include <windows.h>

enum ClickPattern {
    PATTERN_LEGIT   = 0,
    PATTERN_BLATANT = 1,
    PATTERN_CUSTOM  = 2
};

#define MAX_TARGETS 64

struct Settings {
    bool   allowAll;

    HWND   targets[MAX_TARGETS];
    int    targetCount;

    bool   leftEnabled;
    bool   rightEnabled;
    double leftCPS;
    double rightCPS;

    int    pattern;
    bool   holdMode;

    int    bindL;
    int    bindR;

    bool   blockHitEnabled;

    double blockHitBPS;
    int    blockHitPauseBind;
    bool   blockHitPauseHold;

    bool   highCpsEnabled;
    double highCpsCPS;
    int    highCpsBind;

    double customDuration;
    double customChance;
    double customStrength;

    double limitedCps;
};

enum ThrottleStatus {
    THROTTLE_ACTIVE = 0,     // still exposed to background throttling
    THROTTLE_OFF_PROCESS,    // this process was opted out
    THROTTLE_OFF_SYSTEM      // disabled machine-wide, so there is nothing to opt out of
};

struct EngineStats {
    double cpsLeft;
    double cpsRight;
    double cpsHighCps;
    double cpsBlockHit;

    double timerResMs;
    bool   highPriority;
    int    throttling;       // ThrottleStatus
};

void ae_Start();

void ae_Stop();

void ae_Apply(const Settings& s);

bool ae_IsActiveNow();

void ae_SetRmbPhysical(bool down);

void ae_GetStats(EngineStats* out);
