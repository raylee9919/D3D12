// Copyright (c) Seong Woo Lee
// Licensed under the MIT license (https://opensource.org/license/mit/)

struct Win32_State {
    HINSTANCE hinstance;
    WNDCLASSEXW window_class;
    HWND hwnd;
};

#define ASSERT_SUCCEEDED(hr) Assert(SUCCEEDED(hr))


Internal void w32Init(Win32_State *state, HINSTANCE hinst);
Internal LRESULT w32WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
Internal HWND w32CreateWindow(Win32_State *state, WCHAR *title);
