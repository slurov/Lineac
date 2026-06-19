#pragma once
#include <windows.h>

int  bm_CaptureBind(HWND owner);

bool bm_IsDown(int vk);

void bm_DescribeKey(int vk, wchar_t* out, int cch);
