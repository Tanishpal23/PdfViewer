#pragma once

#include "Common.h"
#include <string>
#include <vector>
#include <functional>

struct TabItem {
    std::wstring title;
    std::wstring tooltip;
    std::wstring filePath;
    RECT bounds = {};
    RECT closeBtnBounds = {};
    bool isHovered = false;
    bool isCloseHovered = false;
    bool isClosePressed = false;
};

class TabControl {
public:
    TabControl();
    ~TabControl();

    bool Create(HWND hParent, HINSTANCE hInstance, int id);
    HWND GetHWND() const { return m_hWnd; }

    void SetTheme(const Theme* theme);
    void SetBounds(int x, int y, int width, int height);

    int AddTab(const std::wstring& title, const std::wstring& filePath, bool makeActive = true);
    void RemoveTab(int index);
    void ClearTabs();

    int GetTabCount() const { return static_cast<int>(m_tabs.size()); }
    int GetActiveIndex() const { return m_activeTabIndex; }
    void SetActiveIndex(int index, bool notify = true);
    const TabItem* GetTab(int index) const;

    void SetTabTitle(int index, const std::wstring& title);

    // Callbacks
    std::function<void(int index)> onTabSelected;
    std::function<void(int index)> onTabClosed;
    std::function<void()> onNewTabClicked;

private:
    HWND m_hWnd = nullptr;
    HWND m_hParent = nullptr;
    const Theme* m_theme = nullptr;
    HFONT m_hFont = nullptr;

    std::vector<TabItem> m_tabs;
    int m_activeTabIndex = -1;
    int m_scrollOffset = 0; // horizontal scroll if tabs overflow
    int m_hoveredTabIndex = -1;
    bool m_newBtnHovered = false;
    bool m_newBtnPressed = false;
    RECT m_newBtnBounds = {};

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void LayoutTabs();
    void OnPaint(HDC hdc);
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMButtonUp(int x, int y);
    void OnMouseWheel(int delta);
};

