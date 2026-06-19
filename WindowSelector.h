#pragma once
#include <windows.h>

int ws_SelectWindows(HWND owner, const HWND* preselect, int preCount,
                     HWND* outList, int maxOut);

bool ws_ForegroundInList(const HWND* list, int count);

void ws_GetTitle(HWND hwnd, wchar_t* out, int cch);
