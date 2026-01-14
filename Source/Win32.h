// Copyright Seong Woo Lee. All Rights Reserved.

struct Win32_State 
{
    HINSTANCE hinstance;
    WNDCLASSEXW window_class;
    HWND hwnd;
};

#define ASSERT_SUCCEEDED(hr) Assert(SUCCEEDED(hr))


static void w32Init(Win32_State *state, HINSTANCE hinst);
static LRESULT w32WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static HWND w32CreateWindow(Win32_State *state, const WCHAR *title);

namespace OS
{
    static u64 ReadTimer();
    static u64 GetTimerFrequency();
};
