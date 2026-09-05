#pragma once
#include <windows.h>

// Posted to the owner when the console is dismissed by its own close button, so
// the "Show Console" toggle in the gear panel can follow it back to off.
#define WM_LINEAC_CONSOLE_CLOSED (WM_APP + 17)

void con_Show(HWND owner);

void con_Hide();

bool con_IsVisible();

void con_Shutdown();
