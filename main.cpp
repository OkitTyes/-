#include <Windows.h>
#include "ThunderStrike.h"
#include "BalatroGame.h"

LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"BUTTON", L"雷霆战机 ✈",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 120, 300, 50, hWnd, (HMENU)1, GetModuleHandleW(NULL), NULL);
        CreateWindowW(L"BUTTON", L"图形卡牌 🃏",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 200, 300, 50, hWnd, (HMENU)2, GetModuleHandleW(NULL), NULL);
        HFONT hFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SendDlgItemMessageW(hWnd, 1, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendDlgItemMessageW(hWnd, 2, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        TRIVERTEX vert[2] = {
            {0, 0, 0x20, 0x20, 0x60, 0xFF00},
            {rc.right, rc.bottom, 0x40, 0x10, 0x80, 0xFF00}
        };
        GRADIENT_RECT gRect = { 0, 1 };
        GradientFill(hdc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);
        SetBkMode(hdc, TRANSPARENT);
        HFONT hTitle = CreateFont(42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, hTitle);
        SetTextColor(hdc, RGB(255, 215, 0));
        TextOutW(hdc, rc.right / 2 - 120, 30, L"游戏合集", 6);
        SelectObject(hdc, oldFont);
        DeleteObject(hTitle);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 1) {
            ShowWindow(hWnd, SW_HIDE);
            RunThunderStrike(GetModuleHandleW(NULL));
            ShowWindow(hWnd, SW_SHOW);
        }
        else if (id == 2) {
            ShowWindow(hWnd, SW_HIDE);
            RunBalatroGame();
            ShowWindow(hWnd, SW_SHOW);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = MenuWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainMenuClass";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"MainMenuClass", L"游戏选择器",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 320,
        NULL, NULL, hInstance, NULL);
    if (!hWnd) return -1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}