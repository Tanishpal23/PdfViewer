#include "TabControl.h"

static const wchar_t* TAB_CONTROL_CLASS = L"PdfViewerTabControl";

TabControl::TabControl() {
}

TabControl::~TabControl() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
    }
}

bool TabControl::Create(HWND hParent, HINSTANCE hInstance, int id) {
    m_hParent = hParent;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = TabControl::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // We paint everything in WM_PAINT
    wc.lpszClassName = TAB_CONTROL_CLASS;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        0,
        TAB_CONTROL_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, TAB_BAR_HEIGHT,
        hParent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        hInstance,
        this
    );

    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -12;
    ncm.lfMessageFont.lfWeight = FW_NORMAL;
    m_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

    return m_hWnd != nullptr;
}

void TabControl::SetTheme(const Theme* theme) {
    m_theme = theme;
    if (m_hWnd) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void TabControl::SetBounds(int x, int y, int width, int height) {
    if (m_hWnd) {
        MoveWindow(m_hWnd, x, y, width, height, TRUE);
        LayoutTabs();
    }
}

int TabControl::AddTab(const std::wstring& title, const std::wstring& filePath, bool makeActive) {
    TabItem tab;
    tab.title = title;
    tab.filePath = filePath;
    tab.tooltip = filePath;

    m_tabs.push_back(tab);
    int newIndex = static_cast<int>(m_tabs.size()) - 1;

    LayoutTabs();

    if (makeActive || m_activeTabIndex == -1) {
        SetActiveIndex(newIndex, true);
    } else {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }

    return newIndex;
}

void TabControl::RemoveTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        m_activeTabIndex = -1;
    } else if (m_activeTabIndex >= static_cast<int>(m_tabs.size())) {
        m_activeTabIndex = static_cast<int>(m_tabs.size()) - 1;
        if (onTabSelected) onTabSelected(m_activeTabIndex);
    } else if (m_activeTabIndex == index) {
        if (onTabSelected) onTabSelected(m_activeTabIndex);
    }

    LayoutTabs();
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void TabControl::ClearTabs() {
    m_tabs.clear();
    m_activeTabIndex = -1;
    LayoutTabs();
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void TabControl::SetActiveIndex(int index, bool notify) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    if (m_activeTabIndex == index) return;

    m_activeTabIndex = index;
    LayoutTabs();
    InvalidateRect(m_hWnd, NULL, FALSE);

    if (notify && onTabSelected) {
        onTabSelected(m_activeTabIndex);
    }
}

const TabItem* TabControl::GetTab(int index) const {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        return &m_tabs[index];
    }
    return nullptr;
}

void TabControl::SetTabTitle(int index, const std::wstring& title) {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        m_tabs[index].title = title;
        LayoutTabs();
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void TabControl::LayoutTabs() {
    if (!m_hWnd) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    int curX = 4 - m_scrollOffset;
    int minTabWidth = 100;
    int maxTabWidth = 200;

    int totalAvailable = clientWidth - 40; // reserve space for '+' button
    int numTabs = static_cast<int>(m_tabs.size());

    int tabWidth = maxTabWidth;
    if (numTabs > 0 && numTabs * maxTabWidth > totalAvailable) {
        tabWidth = std::max(minTabWidth, totalAvailable / numTabs);
    }

    for (int i = 0; i < numTabs; ++i) {
        m_tabs[i].bounds.left = curX;
        m_tabs[i].bounds.right = curX + tabWidth;
        m_tabs[i].bounds.top = 4;
        m_tabs[i].bounds.bottom = clientHeight;

        // Close button at right side of tab
        int closeSize = 16;
        m_tabs[i].closeBtnBounds.right = m_tabs[i].bounds.right - 8;
        m_tabs[i].closeBtnBounds.left = m_tabs[i].closeBtnBounds.right - closeSize;
        m_tabs[i].closeBtnBounds.top = m_tabs[i].bounds.top + (clientHeight - 4 - closeSize) / 2;
        m_tabs[i].closeBtnBounds.bottom = m_tabs[i].closeBtnBounds.top + closeSize;

        curX += tabWidth + 2;
    }

    // New Tab '+' button
    int btnSize = 24;
    m_newBtnBounds.left = curX + 4;
    m_newBtnBounds.right = m_newBtnBounds.left + btnSize;
    m_newBtnBounds.top = (clientHeight - btnSize) / 2;
    m_newBtnBounds.bottom = m_newBtnBounds.top + btnSize;
}

void TabControl::OnPaint(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    HFONT oldFont = m_hFont ? (HFONT)SelectObject(memDC, m_hFont) : nullptr;
    SetBkMode(memDC, TRANSPARENT);

    // Fill background
    COLORREF bgTabBar = m_theme ? m_theme->bgTabBar : RGB(37, 37, 38);
    HBRUSH bgBrush = CreateSolidBrush(bgTabBar);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw tabs
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const auto& tab = m_tabs[i];
        bool isActive = (i == m_activeTabIndex);

        COLORREF tabBg = isActive ? (m_theme ? m_theme->tabActiveBg : RGB(30, 30, 30)) :
                         tab.isHovered ? (m_theme ? m_theme->tabHoverBg : RGB(55, 55, 58)) :
                         (m_theme ? m_theme->tabInactiveBg : RGB(45, 45, 48));

        HBRUSH hbrTab = CreateSolidBrush(tabBg);
        FillRect(memDC, &tab.bounds, hbrTab);
        DeleteObject(hbrTab);

        // Top accent line for active tab
        if (isActive && m_theme) {
            RECT accentRect = tab.bounds;
            accentRect.bottom = accentRect.top + 2;
            HBRUSH hbrAccent = CreateSolidBrush(m_theme->tabAccent);
            FillRect(memDC, &accentRect, hbrAccent);
            DeleteObject(hbrAccent);
        }

        // Draw tab title
        COLORREF textColor = isActive ? (m_theme ? m_theme->tabActiveText : RGB(240, 240, 240)) :
                                        (m_theme ? m_theme->tabInactiveText : RGB(170, 170, 170));
        SetTextColor(memDC, textColor);

        RECT textRect = tab.bounds;
        textRect.left += 10;
        textRect.right = tab.closeBtnBounds.left - 4;
        DrawTextW(memDC, tab.title.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        // Draw close button
        if (tab.isCloseHovered) {
            COLORREF closeHoverBg = m_theme ? m_theme->tabCloseHover : RGB(196, 43, 28);
            HBRUSH hbrClose = CreateSolidBrush(closeHoverBg);
            FillRect(memDC, &tab.closeBtnBounds, hbrClose);
            DeleteObject(hbrClose);
            SetTextColor(memDC, RGB(255, 255, 255));
        } else {
            SetTextColor(memDC, isActive ? textColor : RGB(140, 140, 140));
        }

        RECT xRect = tab.closeBtnBounds;
        DrawTextW(memDC, L"x", 1, &xRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // Draw '+' new tab button
    COLORREF newBtnBg = m_newBtnPressed ? (m_theme ? m_theme->btnPressBg : RGB(0, 122, 204)) :
                        m_newBtnHovered ? (m_theme ? m_theme->btnHoverBg : RGB(62, 62, 66)) :
                        (m_theme ? m_theme->bgTabBar : RGB(37, 37, 38));
    HBRUSH hbrNew = CreateSolidBrush(newBtnBg);
    FillRect(memDC, &m_newBtnBounds, hbrNew);
    DeleteObject(hbrNew);

    SetTextColor(memDC, m_theme ? m_theme->textPrimary : RGB(230, 230, 230));
    RECT plusRect = m_newBtnBounds;
    DrawTextW(memDC, L"+", 1, &plusRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Bottom border line separating tab bar from toolbar
    if (m_theme) {
        HPEN hpen = CreatePen(PS_SOLID, 1, m_theme->border);
        HPEN oldPen = (HPEN)SelectObject(memDC, hpen);
        MoveToEx(memDC, 0, height - 1, NULL);
        LineTo(memDC, width, height - 1);
        SelectObject(memDC, oldPen);
        DeleteObject(hpen);
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    if (oldFont) SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void TabControl::OnMouseMove(int x, int y) {
    TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
    TrackMouseEvent(&tme);

    bool needRepaint = false;
    POINT pt = { x, y };

    // Check tabs
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        bool inTab = PtInRect(&m_tabs[i].bounds, pt) != 0;
        bool inClose = PtInRect(&m_tabs[i].closeBtnBounds, pt) != 0;

        if (m_tabs[i].isHovered != inTab || m_tabs[i].isCloseHovered != inClose) {
            m_tabs[i].isHovered = inTab;
            m_tabs[i].isCloseHovered = inClose;
            needRepaint = true;
        }
    }

    // Check '+' button
    bool inNewBtn = PtInRect(&m_newBtnBounds, pt) != 0;
    if (m_newBtnHovered != inNewBtn) {
        m_newBtnHovered = inNewBtn;
        needRepaint = true;
    }

    if (needRepaint) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void TabControl::OnMouseLeave() {
    bool needRepaint = false;
    for (auto& tab : m_tabs) {
        if (tab.isHovered || tab.isCloseHovered) {
            tab.isHovered = false;
            tab.isCloseHovered = false;
            needRepaint = true;
        }
    }
    if (m_newBtnHovered) {
        m_newBtnHovered = false;
        needRepaint = true;
    }
    if (needRepaint) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void TabControl::OnLButtonDown(int x, int y) {
    POINT pt = { x, y };

    if (PtInRect(&m_newBtnBounds, pt)) {
        m_newBtnPressed = true;
        InvalidateRect(m_hWnd, NULL, FALSE);
        return;
    }

    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        if (PtInRect(&m_tabs[i].closeBtnBounds, pt)) {
            m_tabs[i].isClosePressed = true;
            InvalidateRect(m_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&m_tabs[i].bounds, pt)) {
            SetActiveIndex(i, true);
            return;
        }
    }
}

void TabControl::OnLButtonUp(int x, int y) {
    POINT pt = { x, y };

    if (m_newBtnPressed) {
        m_newBtnPressed = false;
        InvalidateRect(m_hWnd, NULL, FALSE);
        if (PtInRect(&m_newBtnBounds, pt)) {
            if (onNewTabClicked) onNewTabClicked();
        }
        return;
    }

    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        if (m_tabs[i].isClosePressed) {
            m_tabs[i].isClosePressed = false;
            InvalidateRect(m_hWnd, NULL, FALSE);
            if (PtInRect(&m_tabs[i].closeBtnBounds, pt)) {
                if (onTabClosed) onTabClosed(i);
            }
            return;
        }
    }
}

void TabControl::OnMButtonUp(int x, int y) {
    POINT pt = { x, y };
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        if (PtInRect(&m_tabs[i].bounds, pt)) {
            if (onTabClosed) onTabClosed(i);
            return;
        }
    }
}

void TabControl::OnMouseWheel(int delta) {
    if (m_tabs.empty()) return;
    int scrollAmount = 30;
    if (delta > 0) {
        m_scrollOffset = std::max(0, m_scrollOffset - scrollAmount);
    } else {
        m_scrollOffset += scrollAmount;
    }
    LayoutTabs();
    InvalidateRect(m_hWnd, NULL, FALSE);
}

LRESULT CALLBACK TabControl::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TabControl* pThis = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<TabControl*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<TabControl*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT TabControl::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);
            OnPaint(hdc);
            EndPaint(m_hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; // Double buffered in WM_PAINT
        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSELEAVE:
            OnMouseLeave();
            return 0;
        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MBUTTONUP:
            OnMButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_LBUTTONDBLCLK: {
            // Double click on empty space opens new file
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool inAny = false;
            for (const auto& tab : m_tabs) {
                if (PtInRect(&tab.bounds, pt)) { inAny = true; break; }
            }
            if (!inAny && onNewTabClicked) onNewTabClicked();
            return 0;
        }
        case WM_SIZE:
            LayoutTabs();
            InvalidateRect(m_hWnd, NULL, FALSE);
            return 0;
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

