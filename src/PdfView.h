#pragma once

#include "Common.h"
#include "Document.h"
#include <functional>
#include <vector>

struct PageLayoutInfo {
    int pageIndex = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

class PdfView {
public:
    PdfView();
    ~PdfView();

    bool Create(HWND hParent, HINSTANCE hInstance, int id);
    HWND GetHWND() const { return m_hWnd; }

    void SetTheme(const Theme* theme);
    void SetBounds(int x, int y, int width, int height);

    void SetDocument(std::shared_ptr<Document> doc);
    std::shared_ptr<Document> GetDocument() const { return m_doc; }

    // Navigation
    void GoToPage(int pageIndex, bool alignTop = true);
    void NextPage();
    void PrevPage();
    void FirstPage();
    void LastPage();
    int GetCurrentPage() const { return m_currentPage; }

    // Scroll Position
    int GetScrollX() const { return m_scrollX; }
    int GetScrollY() const { return m_scrollY; }
    void SetScrollPosition(int scrollX, int scrollY);

    // Zoom
    double GetZoom() const { return m_zoom; }
    ZoomMode GetZoomMode() const { return m_zoomMode; }
    void SetZoom(double zoom);
    void ZoomIn();
    void ZoomOut();
    void ZoomActual();
    void ZoomFitWidth();
    void ZoomFitPage();

    // Rotation
    int GetRotation() const { return m_rotation; }
    void SetRotation(int rotation);
    void RotateCW();
    void RotateCCW();

    // Text Selection & Clipboard
    void ClearSelection();
    bool HasSelection() const;
    std::wstring GetSelectedText() const;
    void CopySelectionToClipboard();
    void SelectAll();

    // Search
    void SetSearchMatches(const std::vector<SearchMatch>& matches, int activeIndex = -1);
    void SetActiveSearchMatch(int activeIndex);

    // State notification callbacks
    std::function<void(int currentPage, int totalPages)> onPageChanged;
    std::function<void(double zoom, ZoomMode mode)> onZoomChanged;
    std::function<void(int scrollX, int scrollY, int page, double zoom, int rotation)> onProgressChanged;

private:
    HWND m_hWnd = nullptr;
    HWND m_hParent = nullptr;
    const Theme* m_theme = nullptr;
    std::shared_ptr<Document> m_doc;

    double m_zoom = 1.0;
    ZoomMode m_zoomMode = ZoomMode::Custom;
    int m_rotation = 0; // 0, 90, 180, 270

    int m_scrollX = 0;
    int m_scrollY = 0;
    int m_totalContentWidth = 0;
    int m_totalContentHeight = 0;
    int m_currentPage = 0;

    std::vector<PageLayoutInfo> m_pageLayouts;
    constexpr static int PAGE_SPACING = 14;

    // Text Selection
    bool m_isSelecting = false;
    int m_selStartPage = -1;
    int m_selStartChar = -1;
    int m_selEndPage = -1;
    int m_selEndChar = -1;

    // Search matches
    std::vector<SearchMatch> m_searchMatches;
    int m_activeSearchIndex = -1;

    // Throttle progress save
    UINT_PTR m_saveTimerId = 0;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void UpdateLayout();
    void UpdateScrollbars();
    void OnPaint(HDC hdc);
    void OnVScroll(WORD scrollCode, short pos);
    void OnHScroll(WORD scrollCode, short pos);
    void OnMouseWheel(int delta, bool isCtrlDown);
    void OnLButtonDown(int x, int y);
    void OnMouseMove(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnKeyDown(WPARAM key, LPARAM lParam);

    void ScheduleProgressNotification();
    void NotifyProgress();
    void CalculateCurrentPage();
};

