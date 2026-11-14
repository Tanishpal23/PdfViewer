#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <iomanip>

// Resource IDs
#define IDI_APP_ICON               101

// Command IDs
#define ID_FILE_OPEN               1001
#define ID_FILE_CLOSE_TAB          1002
#define ID_FILE_REOPEN_CLOSED      1003
#define ID_FILE_PREV_TAB           1004
#define ID_FILE_NEXT_TAB           1005
#define ID_FILE_EXIT               1006

#define ID_NAV_PREV                1010
#define ID_NAV_NEXT                1011
#define ID_NAV_FIRST               1012
#define ID_NAV_LAST                1013
#define ID_NAV_PAGE_BOX            1014

#define ID_VIEW_ZOOM_IN            1020
#define ID_VIEW_ZOOM_OUT           1021
#define ID_VIEW_ZOOM_ACTUAL        1022
#define ID_VIEW_ZOOM_FIT_WIDTH     1023
#define ID_VIEW_ZOOM_FIT_PAGE      1024
#define ID_VIEW_ROTATE_CW          1025
#define ID_VIEW_ROTATE_CCW         1026
#define ID_VIEW_FULLSCREEN         1027
#define ID_VIEW_TOGGLE_SIDEBAR     1028
#define ID_VIEW_TOGGLE_THEME       1029

#define ID_SEARCH_OPEN             1030
#define ID_SEARCH_PREV             1031
#define ID_SEARCH_NEXT             1032
#define ID_SEARCH_CLOSE            1033
#define ID_SEARCH_BOX              1034

#define ID_EDIT_COPY               1040

// Custom Windows Messages
#define WM_USER_DOCUMENT_LOADED    (WM_USER + 101)
#define WM_USER_THUMBNAIL_READY    (WM_USER + 102)
#define WM_USER_PAGE_CHANGED       (WM_USER + 103)
#define WM_USER_RELOCATE_FILE      (WM_USER + 104)

// UI Dimensions (standard 96 DPI baseline)
constexpr int TAB_BAR_HEIGHT = 36;
constexpr int TOOLBAR_HEIGHT = 42;
constexpr int SEARCH_BAR_HEIGHT = 38;
constexpr int DEFAULT_SIDEBAR_WIDTH = 220;
constexpr int MIN_SIDEBAR_WIDTH = 120;
constexpr int SPLITTER_WIDTH = 5;

// Zoom Fit Modes
enum class ZoomMode {
    Custom,
    FitWidth,
    FitPage,
    ActualSize
};

// UI Theme definition
struct Theme {
    bool isDark = true;

    // Window & background
    COLORREF bgMain;
    COLORREF bgToolbar;
    COLORREF bgTabBar;
    COLORREF bgSidebar;
    COLORREF bgViewer;
    COLORREF border;

    // Tabs
    COLORREF tabActiveBg;
    COLORREF tabInactiveBg;
    COLORREF tabHoverBg;
    COLORREF tabActiveText;
    COLORREF tabInactiveText;
    COLORREF tabAccent;
    COLORREF tabCloseHover;

    // Toolbar buttons
    COLORREF btnNormalBg;
    COLORREF btnHoverBg;
    COLORREF btnPressBg;
    COLORREF textPrimary;
    COLORREF textSecondary;
    COLORREF inputBg;
    COLORREF inputBorder;

    // Document view
    COLORREF pageShadow;
    COLORREF pageBorder;
    COLORREF highlightSearch;
    COLORREF highlightActiveSearch;
    COLORREF highlightSelection;

    // GDI Brushes & Pens
    HBRUSH hbrMain = nullptr;
    HBRUSH hbrToolbar = nullptr;
    HBRUSH hbrTabBar = nullptr;
    HBRUSH hbrSidebar = nullptr;
    HBRUSH hbrViewer = nullptr;
    HBRUSH hbrInput = nullptr;
    HPEN hpenBorder = nullptr;
    HPEN hpenInput = nullptr;

    void UpdateTheme(bool dark) {
        isDark = dark;
        if (hbrMain) DeleteObject(hbrMain);
        if (hbrToolbar) DeleteObject(hbrToolbar);
        if (hbrTabBar) DeleteObject(hbrTabBar);
        if (hbrSidebar) DeleteObject(hbrSidebar);
        if (hbrViewer) DeleteObject(hbrViewer);
        if (hbrInput) DeleteObject(hbrInput);
        if (hpenBorder) DeleteObject(hpenBorder);
        if (hpenInput) DeleteObject(hpenInput);

        if (dark) {
            bgMain = RGB(30, 30, 30);
            bgToolbar = RGB(45, 45, 48);
            bgTabBar = RGB(37, 37, 38);
            bgSidebar = RGB(37, 37, 38);
            bgViewer = RGB(28, 28, 28);
            border = RGB(60, 60, 60);

            tabActiveBg = RGB(30, 30, 30);
            tabInactiveBg = RGB(45, 45, 48);
            tabHoverBg = RGB(55, 55, 58);
            tabActiveText = RGB(240, 240, 240);
            tabInactiveText = RGB(170, 170, 170);
            tabAccent = RGB(0, 122, 204);
            tabCloseHover = RGB(196, 43, 28);

            btnNormalBg = RGB(45, 45, 48);
            btnHoverBg = RGB(62, 62, 66);
            btnPressBg = RGB(0, 122, 204);
            textPrimary = RGB(230, 230, 230);
            textSecondary = RGB(160, 160, 160);
            inputBg = RGB(30, 30, 30);
            inputBorder = RGB(70, 70, 70);

            pageShadow = RGB(15, 15, 15);
            pageBorder = RGB(50, 50, 50);
            highlightSearch = RGB(255, 235, 59);
            highlightActiveSearch = RGB(255, 152, 0);
            highlightSelection = RGB(38, 79, 120);
        } else {
            bgMain = RGB(243, 243, 243);
            bgToolbar = RGB(249, 249, 249);
            bgTabBar = RGB(235, 235, 235);
            bgSidebar = RGB(245, 245, 245);
            bgViewer = RGB(225, 225, 225);
            border = RGB(218, 218, 218);

            tabActiveBg = RGB(255, 255, 255);
            tabInactiveBg = RGB(235, 235, 235);
            tabHoverBg = RGB(240, 240, 240);
            tabActiveText = RGB(30, 30, 30);
            tabInactiveText = RGB(100, 100, 100);
            tabAccent = RGB(0, 120, 215);
            tabCloseHover = RGB(232, 17, 35);

            btnNormalBg = RGB(249, 249, 249);
            btnHoverBg = RGB(230, 230, 230);
            btnPressBg = RGB(210, 210, 210);
            textPrimary = RGB(30, 30, 30);
            textSecondary = RGB(110, 110, 110);
            inputBg = RGB(255, 255, 255);
            inputBorder = RGB(180, 180, 180);

            pageShadow = RGB(180, 180, 180);
            pageBorder = RGB(200, 200, 200);
            highlightSearch = RGB(255, 235, 59);
            highlightActiveSearch = RGB(255, 152, 0);
            highlightSelection = RGB(173, 214, 255);
        }

        hbrMain = CreateSolidBrush(bgMain);
        hbrToolbar = CreateSolidBrush(bgToolbar);
        hbrTabBar = CreateSolidBrush(bgTabBar);
        hbrSidebar = CreateSolidBrush(bgSidebar);
        hbrViewer = CreateSolidBrush(bgViewer);
        hbrInput = CreateSolidBrush(inputBg);
        hpenBorder = CreatePen(PS_SOLID, 1, border);
        hpenInput = CreatePen(PS_SOLID, 1, inputBorder);
    }

    ~Theme() {
        if (hbrMain) DeleteObject(hbrMain);
        if (hbrToolbar) DeleteObject(hbrToolbar);
        if (hbrTabBar) DeleteObject(hbrTabBar);
        if (hbrSidebar) DeleteObject(hbrSidebar);
        if (hbrViewer) DeleteObject(hbrViewer);
        if (hbrInput) DeleteObject(hbrInput);
        if (hpenBorder) DeleteObject(hpenBorder);
        if (hpenInput) DeleteObject(hpenInput);
    }
};

// String conversion helpers
inline std::wstring Utf8ToUtf16(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], sizeNeeded);
    return wstr;
}

inline std::string Utf16ToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], sizeNeeded, NULL, NULL);
    return str;
}

// DPI scaling helper
inline int Scale(int val, UINT dpi = 96) {
    return MulDiv(val, dpi, 96);
}

