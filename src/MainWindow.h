#pragma once

#include "Common.h"
#include "Database.h"
#include "Document.h"
#include "TabControl.h"
#include "PdfView.h"
#include "ThumbnailView.h"

struct TabState {
    std::shared_ptr<Document> doc;
    DocumentProgress progress;
    bool isLoaded = false;
};

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    HWND GetHWND() const { return m_hWnd; }

    void OpenFile(const std::wstring& filePath);
    void OpenFiles(const std::vector<std::wstring>& filePaths);
    void CloseCurrentTab();
    void ReopenClosedTab();
    void NextTab();
    void PrevTab();

    void ToggleSidebar();
    void ToggleTheme();
    void ToggleFullscreen();

    void OpenSearch();
    void CloseSearch();
    void ExecuteSearch(bool next = true);

    void ZoomIn();
    void ZoomOut();
    void ZoomActual();
    void ZoomFitWidth();
    void ZoomFitPage();
    void RotateCW();
    void RotateCCW();
    void NextPage();
    void PrevPage();
    void FirstPage();
    void LastPage();
    void ScrollUp(int amount = 60);
    void ScrollDown(int amount = 60);
    void CopySelection();
    void SelectAll();

    bool IsFullscreen() const { return m_isFullscreen; }
    bool IsSearchOpen() const { return m_searchVisible; }
    bool IsEditControlFocused() const;
    void FocusViewer();

    void RestoreSession();
    void SaveSession();

private:
    HWND m_hWnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Theme m_theme;
    Database m_db;
    AppSettings m_settings;

    // Child Controls
    TabControl m_tabControl;
    PdfView m_pdfView;
    ThumbnailView m_thumbnailView;

    // Toolbar Controls
    HWND m_hToolbar = nullptr;
    HWND m_hBtnOpen = nullptr;
    HWND m_hBtnPrev = nullptr;
    HWND m_hBtnNext = nullptr;
    HWND m_hEditPage = nullptr;
    HWND m_hLblTotalPages = nullptr;
    HWND m_hBtnZoomOut = nullptr;
    HWND m_hBtnZoomIn = nullptr;
    HWND m_hBtnFitWidth = nullptr;
    HWND m_hBtnFitPage = nullptr;
    HWND m_hBtnActual = nullptr;
    HWND m_hBtnRotate = nullptr;
    HWND m_hBtnSearch = nullptr;
    HWND m_hBtnSidebar = nullptr;
    HWND m_hBtnTheme = nullptr;

    // Search Bar Controls
    HWND m_hSearchBar = nullptr;
    HWND m_hEditSearch = nullptr;
    HWND m_hBtnSearchPrev = nullptr;
    HWND m_hBtnSearchNext = nullptr;
    HWND m_hLblSearchMatches = nullptr;
    HWND m_hChkMatchCase = nullptr;
    HWND m_hBtnSearchClose = nullptr;

    HFONT m_hUiFont = nullptr;
    HFONT m_hToolbarFont = nullptr;

    // State
    std::vector<TabState> m_tabs;
    int m_activeTabIndex = -1;
    std::vector<std::wstring> m_recentlyClosedFiles;

    // Sidebar & Splitter
    bool m_sidebarVisible = true;
    int m_sidebarWidth = DEFAULT_SIDEBAR_WIDTH;
    bool m_isDraggingSplitter = false;
    int m_dragStartX = 0;
    int m_dragStartWidth = 0;

    // Fullscreen state
    bool m_isFullscreen = false;
    WINDOWPLACEMENT m_prevPlacement = { sizeof(WINDOWPLACEMENT) };
    DWORD m_prevStyle = 0;

    // Search state
    bool m_searchVisible = false;
    std::vector<SearchMatch> m_currentMatches;
    int m_currentMatchIndex = -1;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ToolbarSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK SearchEditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateToolbar();
    void CreateSearchBar();
    void LayoutChildren();
    void ApplyTheme();
    void UpdateToolbarState();

    void SwitchToTab(int index);
    void SaveCurrentTabProgress();

    void ShowOpenFileDialog();
    void ShowRelocateFileDialog(const std::wstring& missingPath);
};

