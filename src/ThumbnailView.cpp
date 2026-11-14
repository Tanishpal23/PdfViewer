#include "ThumbnailView.h"

static const wchar_t* THUMBNAIL_VIEW_CLASS = L"PdfViewerThumbnailView";

ThumbnailView::ThumbnailView() {
}

ThumbnailView::~ThumbnailView() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
    }
}

bool ThumbnailView::Create(HWND hParent, HINSTANCE hInstance, int id) {
    m_hParent = hParent;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ThumbnailView::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = THUMBNAIL_VIEW_CLASS;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        0,
        THUMBNAIL_VIEW_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, 100,
        hParent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        hInstance,
        this
    );

    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -11;
    m_hFont = CreateFontIndirectW(&ncm.lfMessageFont);

    return m_hWnd != nullptr;
}

void ThumbnailView::SetTheme(const Theme* theme) {
    m_theme = theme;
    if (m_hWnd) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void ThumbnailView::SetBounds(int x, int y, int width, int height) {
    if (m_hWnd) {
        MoveWindow(m_hWnd, x, y, width, height, TRUE);
        UpdateLayout();
    }
}

void ThumbnailView::SetDocument(std::shared_ptr<Document> doc) {
    m_doc = doc;
    m_scrollY = 0;
    m_activePage = 0;
    UpdateLayout();
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void ThumbnailView::SetActivePage(int pageIndex) {
    if (m_activePage == pageIndex) return;
    m_activePage = pageIndex;

    // Ensure active thumbnail is scrolled into view if needed
    if (pageIndex >= 0 && pageIndex < static_cast<int>(m_items.size()) && m_hWnd) {
        RECT clientRect;
        GetClientRect(m_hWnd, &clientRect);
        int clientHeight = clientRect.bottom - clientRect.top;

        const auto& item = m_items[pageIndex];
        if (item.bounds.top - m_scrollY < 10) {
            m_scrollY = std::max(0, static_cast<int>(item.bounds.top - 10));
            UpdateScrollbars();
        } else if (item.bounds.bottom - m_scrollY > clientHeight - 10) {
            m_scrollY = static_cast<int>(item.bounds.bottom - clientHeight + 10);
            UpdateScrollbars();
        }
    }

    if (m_hWnd) InvalidateRect(m_hWnd, NULL, FALSE);
}

void ThumbnailView::UpdateLayout() {
    if (!m_hWnd) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;

    m_items.clear();
    m_totalContentHeight = 0;

    if (!m_doc || !m_doc->IsOpen()) {
        UpdateScrollbars();
        return;
    }

    int pageCount = m_doc->GetPageCount();
    m_items.resize(pageCount);

    int maxThumbW = std::max(50, clientWidth - 32);
    int maxThumbH = 160;
    int curY = 16;

    for (int i = 0; i < pageCount; ++i) {
        double ptW = 0, ptH = 0;
        m_doc->GetPageSize(i, &ptW, &ptH, 0);

        double scale = std::min(static_cast<double>(maxThumbW) / ptW, static_cast<double>(maxThumbH) / ptH);
        int tw = static_cast<int>(std::round(ptW * scale));
        int th = static_cast<int>(std::round(ptH * scale));

        m_items[i].pageIndex = i;
        m_items[i].imageBounds.left = (clientWidth - tw) / 2;
        m_items[i].imageBounds.right = m_items[i].imageBounds.left + tw;
        m_items[i].imageBounds.top = curY;
        m_items[i].imageBounds.bottom = curY + th;

        m_items[i].bounds.left = 8;
        m_items[i].bounds.right = clientWidth - 8;
        m_items[i].bounds.top = curY - 4;
        m_items[i].bounds.bottom = curY + th + 24;

        curY += th + 32;
    }

    m_totalContentHeight = curY;
    UpdateScrollbars();
}

void ThumbnailView::UpdateScrollbars() {
    if (!m_hWnd) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientHeight = clientRect.bottom - clientRect.top;

    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, m_totalContentHeight);
    si.nPage = clientHeight;
    si.nPos = m_scrollY;
    SetScrollInfo(m_hWnd, SB_VERT, &si, TRUE);
}

void ThumbnailView::OnPaint(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    HFONT oldFont = m_hFont ? (HFONT)SelectObject(memDC, m_hFont) : nullptr;
    SetBkMode(memDC, TRANSPARENT);

    // Background
    COLORREF bgSidebar = m_theme ? m_theme->bgSidebar : RGB(37, 37, 38);
    HBRUSH bgBrush = CreateSolidBrush(bgSidebar);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    if (!m_doc || !m_doc->IsOpen() || m_items.empty()) {
        SetTextColor(memDC, m_theme ? m_theme->textSecondary : RGB(140, 140, 140));
        RECT r = clientRect;
        DrawTextW(memDC, L"Thumbnails", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDC, 0, 0, SRCCOPY);
        if (oldFont) SelectObject(memDC, oldFont);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        return;
    }

    int viewTop = m_scrollY;
    int viewBottom = m_scrollY + clientHeight;

    HDC thumbDC = CreateCompatibleDC(hdc);

    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        if (item.bounds.bottom < viewTop || item.bounds.top > viewBottom) {
            continue;
        }

        int imgX = item.imageBounds.left;
        int imgY = item.imageBounds.top - m_scrollY;
        int imgW = item.imageBounds.right - item.imageBounds.left;
        int imgH = item.imageBounds.bottom - item.imageBounds.top;

        bool isActive = (static_cast<int>(i) == m_activePage);

        // Thumbnail shadow
        RECT shadow = { imgX + 2, imgY + 2, imgX + imgW + 2, imgY + imgH + 2 };
        HBRUSH shBrush = CreateSolidBrush(m_theme ? m_theme->pageShadow : RGB(15, 15, 15));
        FillRect(memDC, &shadow, shBrush);
        DeleteObject(shBrush);

        // Draw thumbnail
        HBITMAP hThumb = m_doc->GetThumbnailBitmap(static_cast<int>(i), imgW, imgH);
        if (hThumb) {
            HBITMAP oldThumbBmp = (HBITMAP)SelectObject(thumbDC, hThumb);
            BitBlt(memDC, imgX, imgY, imgW, imgH, thumbDC, 0, 0, SRCCOPY);
            SelectObject(thumbDC, oldThumbBmp);
        } else {
            RECT pr = { imgX, imgY, imgX + imgW, imgY + imgH };
            FillRect(memDC, &pr, (HBRUSH)GetStockObject(WHITE_BRUSH));
        }

        // Draw border (accent colored if active)
        COLORREF frameColor = isActive ? (m_theme ? m_theme->tabAccent : RGB(0, 122, 204)) :
                                         (m_theme ? m_theme->border : RGB(60, 60, 60));
        HPEN hpen = CreatePen(PS_SOLID, isActive ? 2 : 1, frameColor);
        HPEN oldPen = (HPEN)SelectObject(memDC, hpen);
        HBRUSH oldBr = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, imgX - 1, imgY - 1, imgX + imgW + 1, imgY + imgH + 1);
        SelectObject(memDC, oldBr);
        SelectObject(memDC, oldPen);
        DeleteObject(hpen);

        // Draw page label
        std::wstring label = std::to_wstring(i + 1);
        COLORREF txtColor = isActive ? (m_theme ? m_theme->tabAccent : RGB(0, 122, 204)) :
                                       (m_theme ? m_theme->textSecondary : RGB(160, 160, 160));
        SetTextColor(memDC, txtColor);

        RECT labelRect = { item.bounds.left, imgY + imgH + 4, item.bounds.right, imgY + imgH + 22 };
        DrawTextW(memDC, label.c_str(), -1, &labelRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    DeleteDC(thumbDC);

    // Right border line
    if (m_theme) {
        HPEN borderPen = CreatePen(PS_SOLID, 1, m_theme->border);
        HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
        MoveToEx(memDC, clientWidth - 1, 0, NULL);
        LineTo(memDC, clientWidth - 1, clientHeight);
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);
    }

    BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDC, 0, 0, SRCCOPY);

    if (oldFont) SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void ThumbnailView::OnVScroll(WORD scrollCode, short pos) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientHeight = clientRect.bottom - clientRect.top;
    int lineScroll = 50;

    int newY = m_scrollY;
    switch (scrollCode) {
        case SB_LINEUP: newY -= lineScroll; break;
        case SB_LINEDOWN: newY += lineScroll; break;
        case SB_PAGEUP: newY -= clientHeight; break;
        case SB_PAGEDOWN: newY += clientHeight; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            newY = pos;
            break;
    }

    int maxY = std::max(0, m_totalContentHeight - clientHeight);
    m_scrollY = std::clamp(newY, 0, maxY);
    UpdateScrollbars();
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void ThumbnailView::OnMouseWheel(int delta) {
    int scrollPixels = -delta / WHEEL_DELTA * 100;
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientHeight = clientRect.bottom - clientRect.top;
    int maxY = std::max(0, m_totalContentHeight - clientHeight);

    m_scrollY = std::clamp(m_scrollY + scrollPixels, 0, maxY);
    UpdateScrollbars();
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void ThumbnailView::OnLButtonDown(int x, int y) {
    SetFocus(m_hWnd);
    int docY = y + m_scrollY;

    for (size_t i = 0; i < m_items.size(); ++i) {
        if (docY >= m_items[i].bounds.top && docY <= m_items[i].bounds.bottom) {
            SetActivePage(static_cast<int>(i));
            if (onPageClicked) {
                onPageClicked(static_cast<int>(i));
            }
            return;
        }
    }
}

LRESULT CALLBACK ThumbnailView::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ThumbnailView* pThis = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<ThumbnailView*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<ThumbnailView*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT ThumbnailView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);
            OnPaint(hdc);
            EndPaint(m_hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_VSCROLL:
            OnVScroll(LOWORD(wParam), (short)HIWORD(wParam));
            return 0;
        case WM_MOUSEWHEEL:
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_SIZE:
            UpdateLayout();
            InvalidateRect(m_hWnd, NULL, FALSE);
            return 0;
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

