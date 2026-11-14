#include "PdfView.h"

static const wchar_t* PDF_VIEW_CLASS = L"PdfViewerRenderView";
constexpr UINT_PTR TIMER_PROGRESS_SAVE = 1001;

PdfView::PdfView() {
}

PdfView::~PdfView() {
    if (m_saveTimerId && m_hWnd) {
        KillTimer(m_hWnd, m_saveTimerId);
    }
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
    }
}

bool PdfView::Create(HWND hParent, HINSTANCE hInstance, int id) {
    m_hParent = hParent;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = PdfView::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = PDF_VIEW_CLASS;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        0,
        PDF_VIEW_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, 100,
        hParent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        hInstance,
        this
    );

    return m_hWnd != nullptr;
}

void PdfView::SetTheme(const Theme* theme) {
    m_theme = theme;
    if (m_hWnd) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void PdfView::SetBounds(int x, int y, int width, int height) {
    if (m_hWnd) {
        MoveWindow(m_hWnd, x, y, width, height, TRUE);
        if (m_zoomMode == ZoomMode::FitWidth) {
            ZoomFitWidth();
        } else if (m_zoomMode == ZoomMode::FitPage) {
            ZoomFitPage();
        } else {
            UpdateLayout();
        }
    }
}

void PdfView::SetDocument(std::shared_ptr<Document> doc) {
    m_doc = doc;
    m_currentPage = 0;
    m_scrollX = 0;
    m_scrollY = 0;
    ClearSelection();
    m_searchMatches.clear();
    m_activeSearchIndex = -1;

    UpdateLayout();
    if (m_doc && m_doc->IsOpen()) {
        if (m_zoomMode == ZoomMode::FitWidth) {
            ZoomFitWidth();
        } else if (m_zoomMode == ZoomMode::FitPage) {
            ZoomFitPage();
        }
    }
    InvalidateRect(m_hWnd, NULL, FALSE);

    if (onPageChanged && m_doc) {
        onPageChanged(m_currentPage + 1, m_doc->GetPageCount());
    }
}

void PdfView::UpdateLayout() {
    if (!m_hWnd) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;

    m_pageLayouts.clear();
    m_totalContentWidth = 0;
    m_totalContentHeight = 0;

    if (!m_doc || !m_doc->IsOpen()) {
        UpdateScrollbars();
        return;
    }

    int pageCount = m_doc->GetPageCount();
    m_pageLayouts.resize(pageCount);

    int curY = PAGE_SPACING;
    int maxWidth = 0;

    for (int i = 0; i < pageCount; ++i) {
        double ptW = 0, ptH = 0;
        m_doc->GetPageSize(i, &ptW, &ptH, m_rotation);

        int pixW = static_cast<int>(std::round(ptW * m_zoom));
        int pixH = static_cast<int>(std::round(ptH * m_zoom));

        m_pageLayouts[i].pageIndex = i;
        m_pageLayouts[i].width = pixW;
        m_pageLayouts[i].height = pixH;
        m_pageLayouts[i].y = curY;

        if (pixW > maxWidth) {
            maxWidth = pixW;
        }

        curY += pixH + PAGE_SPACING;
    }

    m_totalContentHeight = curY;
    m_totalContentWidth = maxWidth + (PAGE_SPACING * 2);

    // Center pages horizontally if content width is larger or smaller than client
    for (int i = 0; i < pageCount; ++i) {
        int targetWidth = std::max(clientWidth, m_totalContentWidth);
        m_pageLayouts[i].x = (targetWidth - m_pageLayouts[i].width) / 2;
    }

    UpdateScrollbars();
    CalculateCurrentPage();
}

void PdfView::UpdateScrollbars() {
    if (!m_hWnd) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    // Vertical
    si.nMin = 0;
    si.nMax = std::max(0, m_totalContentHeight);
    si.nPage = clientHeight;
    si.nPos = m_scrollY;
    SetScrollInfo(m_hWnd, SB_VERT, &si, TRUE);

    // Horizontal
    si.nMin = 0;
    si.nMax = std::max(0, m_totalContentWidth);
    si.nPage = clientWidth;
    si.nPos = m_scrollX;
    SetScrollInfo(m_hWnd, SB_HORZ, &si, TRUE);
}

void PdfView::GoToPage(int pageIndex, bool alignTop) {
    if (!m_doc || !m_doc->IsOpen() || pageIndex < 0 || pageIndex >= static_cast<int>(m_pageLayouts.size())) return;

    m_currentPage = pageIndex;
    int targetY = m_pageLayouts[pageIndex].y - (PAGE_SPACING / 2);
    if (!alignTop) {
        RECT clientRect;
        GetClientRect(m_hWnd, &clientRect);
        int clientHeight = clientRect.bottom - clientRect.top;
        targetY = m_pageLayouts[pageIndex].y + (m_pageLayouts[pageIndex].height - clientHeight) / 2;
    }

    SetScrollPosition(m_scrollX, targetY);

    if (onPageChanged) {
        onPageChanged(m_currentPage + 1, m_doc->GetPageCount());
    }
}

void PdfView::NextPage() {
    if (m_doc && m_currentPage + 1 < m_doc->GetPageCount()) {
        GoToPage(m_currentPage + 1);
    }
}

void PdfView::PrevPage() {
    if (m_currentPage > 0) {
        GoToPage(m_currentPage - 1);
    }
}

void PdfView::FirstPage() {
    GoToPage(0);
}

void PdfView::LastPage() {
    if (m_doc && m_doc->GetPageCount() > 0) {
        GoToPage(m_doc->GetPageCount() - 1);
    }
}

void PdfView::SetScrollPosition(int scrollX, int scrollY) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    int maxScrollY = std::max(0, m_totalContentHeight - clientHeight);
    int maxScrollX = std::max(0, m_totalContentWidth - clientWidth);

    m_scrollY = std::clamp(scrollY, 0, maxScrollY);
    m_scrollX = std::clamp(scrollX, 0, maxScrollX);

    UpdateScrollbars();
    CalculateCurrentPage();
    InvalidateRect(m_hWnd, NULL, FALSE);
    ScheduleProgressNotification();
}

void PdfView::SetZoom(double zoom) {
    zoom = std::clamp(zoom, 0.20, 5.0);
    if (std::abs(m_zoom - zoom) < 0.005) return;

    // Preserve relative scroll position
    double ratio = zoom / m_zoom;
    m_zoom = zoom;
    m_zoomMode = ZoomMode::Custom;

    UpdateLayout();

    SetScrollPosition(static_cast<int>(m_scrollX * ratio), static_cast<int>(m_scrollY * ratio));

    if (onZoomChanged) {
        onZoomChanged(m_zoom, m_zoomMode);
    }
}

void PdfView::ZoomIn() {
    SetZoom(m_zoom * 1.25);
}

void PdfView::ZoomOut() {
    SetZoom(m_zoom / 1.25);
}

void PdfView::ZoomActual() {
    m_zoomMode = ZoomMode::ActualSize;
    SetZoom(1.0);
    if (onZoomChanged) onZoomChanged(m_zoom, m_zoomMode);
}

void PdfView::ZoomFitWidth() {
    if (!m_doc || !m_doc->IsOpen() || m_doc->GetPageCount() == 0) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left - (PAGE_SPACING * 2);
    if (clientWidth <= 50) return;

    double ptW = 0, ptH = 0;
    m_doc->GetPageSize(m_currentPage, &ptW, &ptH, m_rotation);
    if (ptW <= 0) return;

    m_zoomMode = ZoomMode::FitWidth;
    m_zoom = static_cast<double>(clientWidth) / ptW;
    UpdateLayout();

    if (onZoomChanged) onZoomChanged(m_zoom, m_zoomMode);
    InvalidateRect(m_hWnd, NULL, FALSE);
    ScheduleProgressNotification();
}

void PdfView::ZoomFitPage() {
    if (!m_doc || !m_doc->IsOpen() || m_doc->GetPageCount() == 0) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left - (PAGE_SPACING * 2);
    int clientHeight = clientRect.bottom - clientRect.top - (PAGE_SPACING * 2);
    if (clientWidth <= 50 || clientHeight <= 50) return;

    double ptW = 0, ptH = 0;
    m_doc->GetPageSize(m_currentPage, &ptW, &ptH, m_rotation);
    if (ptW <= 0 || ptH <= 0) return;

    m_zoomMode = ZoomMode::FitPage;
    m_zoom = std::min(static_cast<double>(clientWidth) / ptW, static_cast<double>(clientHeight) / ptH);
    UpdateLayout();

    if (onZoomChanged) onZoomChanged(m_zoom, m_zoomMode);
    InvalidateRect(m_hWnd, NULL, FALSE);
    ScheduleProgressNotification();
}

void PdfView::SetRotation(int rotation) {
    rotation = ((rotation % 360) + 360) % 360;
    rotation = (rotation / 90) * 90;
    if (m_rotation == rotation) return;

    m_rotation = rotation;
    UpdateLayout();
    InvalidateRect(m_hWnd, NULL, FALSE);
    ScheduleProgressNotification();
}

void PdfView::RotateCW() {
    SetRotation(m_rotation + 90);
}

void PdfView::RotateCCW() {
    SetRotation(m_rotation - 90);
}

void PdfView::ClearSelection() {
    m_isSelecting = false;
    m_selStartPage = -1;
    m_selStartChar = -1;
    m_selEndPage = -1;
    m_selEndChar = -1;
    if (m_hWnd) InvalidateRect(m_hWnd, NULL, FALSE);
}

bool PdfView::HasSelection() const {
    return m_selStartPage != -1 && m_selStartChar != -1 &&
           (m_selStartPage != m_selEndPage || m_selStartChar != m_selEndChar);
}

std::wstring PdfView::GetSelectedText() const {
    if (!m_doc || !HasSelection()) return L"";

    int pStart = m_selStartPage;
    int pEnd = m_selEndPage;
    int cStart = m_selStartChar;
    int cEnd = m_selEndChar;

    if (pStart > pEnd || (pStart == pEnd && cStart > cEnd)) {
        std::swap(pStart, pEnd);
        std::swap(cStart, cEnd);
    }

    std::wstring result;
    for (int p = pStart; p <= pEnd; ++p) {
        int firstChar = (p == pStart) ? cStart : 0;
        int lastChar = (p == pEnd) ? cEnd : m_doc->GetPageCharCount(p) - 1;
        int count = lastChar - firstChar + 1;
        if (count > 0) {
            std::wstring part = m_doc->GetSelectedText(p, firstChar, count);
            result += part;
            if (p < pEnd) result += L"\r\n";
        }
    }
    return result;
}

void PdfView::CopySelectionToClipboard() {
    std::wstring text = GetSelectedText();
    if (text.empty() || !m_hWnd) return;

    if (OpenClipboard(m_hWnd)) {
        EmptyClipboard();
        size_t bytes = (text.length() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, text.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }
}

void PdfView::SelectAll() {
    if (!m_doc || !m_doc->IsOpen() || m_doc->GetPageCount() == 0) return;
    m_selStartPage = 0;
    m_selStartChar = 0;
    m_selEndPage = m_doc->GetPageCount() - 1;
    m_selEndChar = m_doc->GetPageCharCount(m_selEndPage) - 1;
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void PdfView::SetSearchMatches(const std::vector<SearchMatch>& matches, int activeIndex) {
    m_searchMatches = matches;
    m_activeSearchIndex = activeIndex;
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void PdfView::SetActiveSearchMatch(int activeIndex) {
    if (activeIndex >= 0 && activeIndex < static_cast<int>(m_searchMatches.size())) {
        m_activeSearchIndex = activeIndex;
        const auto& match = m_searchMatches[activeIndex];

        // Jump to match
        GoToPage(match.pageIndex, false);
    } else {
        m_activeSearchIndex = -1;
    }
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void PdfView::CalculateCurrentPage() {
    if (m_pageLayouts.empty()) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int centerY = m_scrollY + (clientRect.bottom - clientRect.top) / 2;

    int newPage = m_currentPage;
    for (size_t i = 0; i < m_pageLayouts.size(); ++i) {
        if (centerY >= m_pageLayouts[i].y &&
            centerY < m_pageLayouts[i].y + m_pageLayouts[i].height + PAGE_SPACING) {
            newPage = static_cast<int>(i);
            break;
        }
    }

    if (newPage != m_currentPage) {
        m_currentPage = newPage;
        if (onPageChanged && m_doc) {
            onPageChanged(m_currentPage + 1, m_doc->GetPageCount());
        }
    }
}

void PdfView::ScheduleProgressNotification() {
    if (!m_hWnd) return;
    if (m_saveTimerId) {
        KillTimer(m_hWnd, m_saveTimerId);
    }
    m_saveTimerId = SetTimer(m_hWnd, TIMER_PROGRESS_SAVE, 250, NULL);
}

void PdfView::NotifyProgress() {
    if (onProgressChanged && m_doc && m_doc->IsOpen()) {
        onProgressChanged(m_scrollX, m_scrollY, m_currentPage, m_zoom, m_rotation);
    }
}

void PdfView::OnPaint(HDC hdc) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Fill viewer background
    COLORREF bgViewer = m_theme ? m_theme->bgViewer : RGB(28, 28, 28);
    HBRUSH bgBrush = CreateSolidBrush(bgViewer);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    if (!m_doc || !m_doc->IsOpen() || m_pageLayouts.empty()) {
        // Draw "No PDF Open" message
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, m_theme ? m_theme->textSecondary : RGB(140, 140, 140));
        RECT msgRect = clientRect;
        DrawTextW(memDC, L"Press Ctrl + O to open a PDF or Drag & Drop files here", -1, &msgRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        return;
    }

    int viewTop = m_scrollY;
    int viewBottom = m_scrollY + clientHeight;

    HDC pageDC = CreateCompatibleDC(hdc);

    for (size_t i = 0; i < m_pageLayouts.size(); ++i) {
        const auto& layout = m_pageLayouts[i];

        // Check if page intersects visible region (+1 page margin for smooth scrolling)
        if (layout.y + layout.height < viewTop - 100 || layout.y > viewBottom + 100) {
            continue;
        }

        int screenX = layout.x - m_scrollX;
        int screenY = layout.y - m_scrollY;

        // Draw page drop shadow
        RECT shadowRect = { screenX + 4, screenY + 4, screenX + layout.width + 4, screenY + layout.height + 4 };
        COLORREF shadowColor = m_theme ? m_theme->pageShadow : RGB(15, 15, 15);
        HBRUSH shadowBrush = CreateSolidBrush(shadowColor);
        FillRect(memDC, &shadowRect, shadowBrush);
        DeleteObject(shadowBrush);

        // Render PDF page bitmap
        int bmpW = 0, bmpH = 0;
        HBITMAP hPageBmp = m_doc->GetPageBitmap(static_cast<int>(i), m_zoom, m_rotation, &bmpW, &bmpH);
        if (hPageBmp) {
            HBITMAP oldPageBmp = (HBITMAP)SelectObject(pageDC, hPageBmp);
            BitBlt(memDC, screenX, screenY, layout.width, layout.height, pageDC, 0, 0, SRCCOPY);
            SelectObject(pageDC, oldPageBmp);
        } else {
            // Placeholder white page while rendering
            RECT pageRect = { screenX, screenY, screenX + layout.width, screenY + layout.height };
            FillRect(memDC, &pageRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        }

        // Page border
        RECT borderRect = { screenX - 1, screenY - 1, screenX + layout.width + 1, screenY + layout.height + 1 };
        COLORREF borderColor = m_theme ? m_theme->pageBorder : RGB(50, 50, 50);
        HPEN hpenBorder = CreatePen(PS_SOLID, 1, borderColor);
        HPEN oldPen = (HPEN)SelectObject(memDC, hpenBorder);
        HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, borderRect.left, borderRect.top, borderRect.right, borderRect.bottom);
        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(hpenBorder);

        // Draw Search Match Highlights
        for (size_t m = 0; m < m_searchMatches.size(); ++m) {
            const auto& match = m_searchMatches[m];
            if (match.pageIndex == static_cast<int>(i)) {
                bool isActive = (static_cast<int>(m) == m_activeSearchIndex);
                COLORREF hlColor = isActive ?
                    (m_theme ? m_theme->highlightActiveSearch : RGB(255, 152, 0)) :
                    (m_theme ? m_theme->highlightSearch : RGB(255, 235, 59));

                std::vector<RECT> rects = m_doc->GetTextSelectionRects(
                    match.pageIndex, match.charIndex, match.charCount,
                    m_zoom, m_rotation, screenX, screenY
                );

                HBRUSH hlBrush = CreateSolidBrush(hlColor);
                for (const auto& r : rects) {
                    // Draw highlight with InvertRect / blend
                    InvertRect(memDC, &r);
                }
                DeleteObject(hlBrush);
            }
        }

        // Draw Text Selection Highlights
        if (HasSelection()) {
            int pStart = m_selStartPage;
            int pEnd = m_selEndPage;
            int cStart = m_selStartChar;
            int cEnd = m_selEndChar;
            if (pStart > pEnd || (pStart == pEnd && cStart > cEnd)) {
                std::swap(pStart, pEnd);
                std::swap(cStart, cEnd);
            }

            int pIndex = static_cast<int>(i);
            if (pIndex >= pStart && pIndex <= pEnd) {
                int firstChar = (pIndex == pStart) ? cStart : 0;
                int lastChar = (pIndex == pEnd) ? cEnd : m_doc->GetPageCharCount(pIndex) - 1;
                int count = lastChar - firstChar + 1;
                if (count > 0) {
                    std::vector<RECT> selRects = m_doc->GetTextSelectionRects(
                        pIndex, firstChar, count, m_zoom, m_rotation, screenX, screenY
                    );
                    for (const auto& r : selRects) {
                        InvertRect(memDC, &r);
                    }
                }
            }
        }
    }

    DeleteDC(pageDC);

    // Blit to screen
    BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void PdfView::OnVScroll(WORD scrollCode, short pos) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientHeight = clientRect.bottom - clientRect.top;
    int lineScroll = 40;
    int pageScroll = clientHeight - 40;

    int newY = m_scrollY;
    switch (scrollCode) {
        case SB_LINEUP: newY -= lineScroll; break;
        case SB_LINEDOWN: newY += lineScroll; break;
        case SB_PAGEUP: newY -= pageScroll; break;
        case SB_PAGEDOWN: newY += pageScroll; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            newY = pos;
            break;
    }

    SetScrollPosition(m_scrollX, newY);
}

void PdfView::OnHScroll(WORD scrollCode, short pos) {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int lineScroll = 40;

    int newX = m_scrollX;
    switch (scrollCode) {
        case SB_LINELEFT: newX -= lineScroll; break;
        case SB_LINERIGHT: newX += lineScroll; break;
        case SB_PAGELEFT: newX -= clientWidth; break;
        case SB_PAGERIGHT: newX += clientWidth; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            newX = pos;
            break;
    }

    SetScrollPosition(newX, m_scrollY);
}

void PdfView::OnMouseWheel(int delta, bool isCtrlDown) {
    if (isCtrlDown) {
        // Zooming with mouse wheel
        if (delta > 0) {
            ZoomIn();
        } else {
            ZoomOut();
        }
    } else {
        // Normal scrolling
        int scrollLines = 3;
        int scrollPixels = -delta / WHEEL_DELTA * (40 * scrollLines);
        SetScrollPosition(m_scrollX, m_scrollY + scrollPixels);
    }
}

void PdfView::OnLButtonDown(int x, int y) {
    SetFocus(m_hWnd);
    if (!m_doc || !m_doc->IsOpen()) return;

    SetCapture(m_hWnd);
    m_isSelecting = true;

    int docX = x + m_scrollX;
    int docY = y + m_scrollY;

    for (const auto& layout : m_pageLayouts) {
        if (docX >= layout.x && docX <= layout.x + layout.width &&
            docY >= layout.y && docY <= layout.y + layout.height) {

            double pageX = 0, pageY = 0;
            m_doc->DeviceToPage(layout.pageIndex, docX, docY, m_zoom, m_rotation, layout.x, layout.y, &pageX, &pageY);

            int charIdx = m_doc->GetCharIndexAtPos(layout.pageIndex, pageX, pageY);
            if (charIdx >= 0) {
                m_selStartPage = layout.pageIndex;
                m_selStartChar = charIdx;
                m_selEndPage = layout.pageIndex;
                m_selEndChar = charIdx;
                InvalidateRect(m_hWnd, NULL, FALSE);
                return;
            }
        }
    }

    ClearSelection();
}

void PdfView::OnMouseMove(int x, int y) {
    if (!m_isSelecting || !m_doc || !m_doc->IsOpen()) return;

    int docX = x + m_scrollX;
    int docY = y + m_scrollY;

    for (const auto& layout : m_pageLayouts) {
        if (docX >= layout.x - 20 && docX <= layout.x + layout.width + 20 &&
            docY >= layout.y && docY <= layout.y + layout.height) {

            double pageX = 0, pageY = 0;
            m_doc->DeviceToPage(layout.pageIndex, docX, docY, m_zoom, m_rotation, layout.x, layout.y, &pageX, &pageY);

            int charIdx = m_doc->GetCharIndexAtPos(layout.pageIndex, pageX, pageY, 15.0);
            if (charIdx >= 0) {
                m_selEndPage = layout.pageIndex;
                m_selEndChar = charIdx;
                InvalidateRect(m_hWnd, NULL, FALSE);
                return;
            }
        }
    }
}

void PdfView::OnLButtonUp(int x, int y) {
    if (m_isSelecting) {
        m_isSelecting = false;
        ReleaseCapture();
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void PdfView::OnRButtonUp(int x, int y) {
    POINT pt = { x, y };
    ClientToScreen(m_hWnd, &pt);

    HMENU hMenu = CreatePopupMenu();
    bool hasSel = HasSelection();

    AppendMenuW(hMenu, hasSel ? MF_STRING : (MF_STRING | MF_GRAYED), ID_EDIT_COPY, L"Copy\tCtrl+C");
    AppendMenuW(hMenu, MF_STRING, 2001, L"Select All\tCtrl+A");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_VIEW_ZOOM_IN, L"Zoom In\tCtrl++");
    AppendMenuW(hMenu, MF_STRING, ID_VIEW_ZOOM_OUT, L"Zoom Out\tCtrl+-");
    AppendMenuW(hMenu, MF_STRING, ID_VIEW_ZOOM_ACTUAL, L"Actual Size\tCtrl+1");
    AppendMenuW(hMenu, MF_STRING, ID_VIEW_ZOOM_FIT_WIDTH, L"Fit Width\tCtrl+2");
    AppendMenuW(hMenu, MF_STRING, ID_VIEW_ZOOM_FIT_PAGE, L"Fit Page\tCtrl+0");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, ID_VIEW_ROTATE_CW, L"Rotate Clockwise\tCtrl+R");

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
    DestroyMenu(hMenu);

    if (cmd == ID_EDIT_COPY) {
        CopySelectionToClipboard();
    } else if (cmd == 2001) {
        SelectAll();
    } else if (cmd != 0) {
        SendMessage(m_hParent, WM_COMMAND, cmd, 0);
    }
}

void PdfView::OnKeyDown(WPARAM key, LPARAM lParam) {
    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    switch (key) {
        case 'C':
            if (isCtrl) CopySelectionToClipboard();
            break;
        case 'A':
            if (isCtrl) SelectAll();
            break;
        case VK_UP:
            SetScrollPosition(m_scrollX, m_scrollY - 40);
            break;
        case VK_DOWN:
            SetScrollPosition(m_scrollX, m_scrollY + 40);
            break;
        case VK_LEFT:
            PrevPage();
            break;
        case VK_RIGHT:
            NextPage();
            break;
        case VK_PRIOR: // Page Up
            PrevPage();
            break;
        case VK_NEXT: // Page Down
            NextPage();
            break;
        case VK_HOME:
            FirstPage();
            break;
        case VK_END:
            LastPage();
            break;
        case VK_OEM_PLUS:
        case VK_ADD:
            if (isCtrl) ZoomIn();
            break;
        case VK_OEM_MINUS:
        case VK_SUBTRACT:
            if (isCtrl) ZoomOut();
            break;
        case '0':
            if (isCtrl) ZoomFitPage();
            break;
        case '1':
            if (isCtrl) ZoomActual();
            break;
        case '2':
            if (isCtrl) ZoomFitWidth();
            break;
        case 'R':
            if (isCtrl) {
                if (isShift) RotateCCW();
                else RotateCW();
            }
            break;
    }
}

LRESULT CALLBACK PdfView::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PdfView* pThis = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<PdfView*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<PdfView*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT PdfView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        case WM_HSCROLL:
            OnHScroll(LOWORD(wParam), (short)HIWORD(wParam));
            return 0;
        case WM_MOUSEWHEEL: {
            bool isCtrl = (LOWORD(wParam) & MK_CONTROL) != 0;
            OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam), isCtrl);
            return 0;
        }
        case WM_LBUTTONDOWN:
            OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
            OnLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_RBUTTONUP:
            OnRButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_KEYDOWN:
            OnKeyDown(wParam, lParam);
            return 0;
        case WM_TIMER:
            if (wParam == TIMER_PROGRESS_SAVE) {
                KillTimer(m_hWnd, m_saveTimerId);
                m_saveTimerId = 0;
                NotifyProgress();
                return 0;
            }
            break;
        case WM_SIZE:
            if (m_zoomMode == ZoomMode::FitWidth) {
                ZoomFitWidth();
            } else if (m_zoomMode == ZoomMode::FitPage) {
                ZoomFitPage();
            } else {
                UpdateLayout();
            }
            InvalidateRect(m_hWnd, NULL, FALSE);
            return 0;
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

