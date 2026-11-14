#pragma once

#include "Common.h"
#include "Document.h"
#include <functional>
#include <vector>

struct ThumbnailLayoutItem {
    int pageIndex = 0;
    RECT bounds = {};
    RECT imageBounds = {};
};

class ThumbnailView {
public:
    ThumbnailView();
    ~ThumbnailView();

    bool Create(HWND hParent, HINSTANCE hInstance, int id);
    HWND GetHWND() const { return m_hWnd; }

    void SetTheme(const Theme* theme);
    void SetBounds(int x, int y, int width, int height);

    void SetDocument(std::shared_ptr<Document> doc);
    void SetActivePage(int pageIndex);

    std::function<void(int pageIndex)> onPageClicked;

private:
    HWND m_hWnd = nullptr;
    HWND m_hParent = nullptr;
    const Theme* m_theme = nullptr;
    HFONT m_hFont = nullptr;
    std::shared_ptr<Document> m_doc;

    int m_activePage = 0;
    int m_scrollY = 0;
    int m_totalContentHeight = 0;
    std::vector<ThumbnailLayoutItem> m_items;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void UpdateLayout();
    void UpdateScrollbars();
    void OnPaint(HDC hdc);
    void OnVScroll(WORD scrollCode, short pos);
    void OnMouseWheel(int delta);
    void OnLButtonDown(int x, int y);
};

