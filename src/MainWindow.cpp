#include "MainWindow.h"
#include <commdlg.h>
#include <iostream>

static const wchar_t* MAIN_WINDOW_CLASS = L"PdfViewerMainWindow";

MainWindow::MainWindow() {
}

MainWindow::~MainWindow() {
    if (m_hUiFont) DeleteObject(m_hUiFont);
    if (m_hToolbarFont) DeleteObject(m_hToolbarFont);
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    // Open SQLite Database
    m_db.Open();
    m_settings = m_db.LoadAppSettings();
    m_theme.UpdateTheme(m_settings.isDarkMode);
    m_sidebarVisible = m_settings.sidebarVisible;
    m_sidebarWidth = m_settings.sidebarWidth;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = MAIN_WINDOW_CLASS;

    RegisterClassExW(&wc);

    // Setup fonts
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -13;
    m_hUiFont = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfHeight = -12;
    m_hToolbarFont = CreateFontIndirectW(&ncm.lfMessageFont);

    int winX = m_settings.windowX;
    int winY = m_settings.windowY;
    int winW = m_settings.windowWidth;
    int winH = m_settings.windowHeight;

    m_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        MAIN_WINDOW_CLASS,
        L"PdfViewer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        winX, winY, winW, winH,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hWnd) return false;

    // Set Window Icons
    HICON hIconBig = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (hIconBig) SendMessage(m_hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconBig));
    if (hIconSmall) SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));

    // Create Child Views
    m_tabControl.Create(m_hWnd, hInstance, 101);
    CreateToolbar();
    CreateSearchBar();
    m_thumbnailView.Create(m_hWnd, hInstance, 102);
    m_pdfView.Create(m_hWnd, hInstance, 103);

    ApplyTheme();

    // Setup Tab Control Callbacks
    m_tabControl.onTabSelected = [this](int index) {
        SwitchToTab(index);
    };
    m_tabControl.onTabClosed = [this](int index) {
        if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
            m_recentlyClosedFiles.push_back(m_tabs[index].progress.filePath);
            m_db.SaveProgress(m_tabs[index].progress);

            // Free document resources
            m_tabs[index].doc->Close();
            m_tabs.erase(m_tabs.begin() + index);
            m_tabControl.RemoveTab(index);

            if (m_tabs.empty()) {
                m_activeTabIndex = -1;
                m_pdfView.SetDocument(nullptr);
                m_thumbnailView.SetDocument(nullptr);
                UpdateToolbarState();
            }
        }
    };
    m_tabControl.onNewTabClicked = [this]() {
        ShowOpenFileDialog();
    };

    // Setup PdfView Callbacks
    m_pdfView.onPageChanged = [this](int currentPage, int totalPages) {
        m_thumbnailView.SetActivePage(currentPage - 1);
        if (m_hEditPage) {
            SetWindowTextW(m_hEditPage, std::to_wstring(currentPage).c_str());
        }
        if (m_hLblTotalPages) {
            std::wstring lbl = L"/ " + std::to_wstring(totalPages);
            SetWindowTextW(m_hLblTotalPages, lbl.c_str());
        }
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
            m_tabs[m_activeTabIndex].progress.lastPage = currentPage - 1;
        }
    };

    m_pdfView.onProgressChanged = [this](int scrollX, int scrollY, int page, double zoom, int rotation) {
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
            auto& prog = m_tabs[m_activeTabIndex].progress;
            prog.scrollX = scrollX;
            prog.scrollY = scrollY;
            prog.lastPage = page;
            prog.zoomLevel = zoom;
            prog.zoomMode = static_cast<int>(m_pdfView.GetZoomMode());
            prog.rotation = rotation;
            prog.totalPages = m_tabs[m_activeTabIndex].doc ? m_tabs[m_activeTabIndex].doc->GetPageCount() : 0;
            m_db.SaveProgress(prog);
        }
    };

    // Setup ThumbnailView Callback
    m_thumbnailView.onPageClicked = [this](int pageIndex) {
        m_pdfView.GoToPage(pageIndex);
    };

    if (m_settings.isMaximized) {
        ShowWindow(m_hWnd, SW_MAXIMIZE);
    } else {
        ShowWindow(m_hWnd, nCmdShow);
    }
    UpdateWindow(m_hWnd);

    // Drag and Drop
    DragAcceptFiles(m_hWnd, TRUE);

    return true;
}

void MainWindow::CreateToolbar() {
    m_hToolbar = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, TAB_BAR_HEIGHT, 100, TOOLBAR_HEIGHT,
        m_hWnd, NULL, m_hInstance, NULL
    );

    SetWindowSubclass(m_hToolbar, MainWindow::ToolbarSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

    auto makeBtn = [this](int id, const wchar_t* text, int w = 48) {
        HWND hBtn = CreateWindowExW(
            0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
            0, 0, w, 30,
            m_hToolbar, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            m_hInstance, NULL
        );
        SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);
        return hBtn;
    };

    m_hBtnOpen = makeBtn(ID_FILE_OPEN, L"Open", 56);
    m_hBtnSidebar = makeBtn(ID_VIEW_TOGGLE_SIDEBAR, L"Pages", 56);
    m_hBtnPrev = makeBtn(ID_NAV_PREV, L"<", 34);
    m_hBtnNext = makeBtn(ID_NAV_NEXT, L">", 34);

    m_hEditPage = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"1",
        WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER,
        0, 0, 42, 26,
        m_hToolbar, reinterpret_cast<HMENU>(ID_NAV_PAGE_BOX),
        m_hInstance, NULL
    );
    SendMessage(m_hEditPage, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);
    SetWindowSubclass(m_hEditPage, MainWindow::EditSubclassProc, 2, reinterpret_cast<DWORD_PTR>(this));

    m_hLblTotalPages = CreateWindowExW(
        0, L"STATIC", L"/ 0",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 50, 26,
        m_hToolbar, NULL, m_hInstance, NULL
    );
    SendMessage(m_hLblTotalPages, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);

    m_hBtnZoomOut = makeBtn(ID_VIEW_ZOOM_OUT, L"-", 34);
    m_hBtnZoomIn = makeBtn(ID_VIEW_ZOOM_IN, L"+", 34);
    m_hBtnFitWidth = makeBtn(ID_VIEW_ZOOM_FIT_WIDTH, L"Fit W", 48);
    m_hBtnFitPage = makeBtn(ID_VIEW_ZOOM_FIT_PAGE, L"Fit Page", 64);
    m_hBtnActual = makeBtn(ID_VIEW_ZOOM_ACTUAL, L"100%", 48);
    m_hBtnRotate = makeBtn(ID_VIEW_ROTATE_CW, L"Rotate", 56);
    m_hBtnSearch = makeBtn(ID_SEARCH_OPEN, L"Find", 52);
    m_hBtnTheme = makeBtn(ID_VIEW_TOGGLE_THEME, m_theme.isDark ? L"Light" : L"Dark", 56);
}

void MainWindow::CreateSearchBar() {
    m_hSearchBar = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, SEARCH_BAR_HEIGHT,
        m_hWnd, NULL, m_hInstance, NULL
    );
    SetWindowSubclass(m_hSearchBar, MainWindow::ToolbarSubclassProc, 3, reinterpret_cast<DWORD_PTR>(this));

    m_hEditSearch = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 180, 26,
        m_hSearchBar, reinterpret_cast<HMENU>(ID_SEARCH_BOX),
        m_hInstance, NULL
    );
    SendMessage(m_hEditSearch, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);
    SetWindowSubclass(m_hEditSearch, MainWindow::SearchEditSubclassProc, 4, reinterpret_cast<DWORD_PTR>(this));

    auto makeBtn = [this](int id, const wchar_t* text, int w = 48) {
        HWND hBtn = CreateWindowExW(
            0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
            0, 0, w, 26,
            m_hSearchBar, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            m_hInstance, NULL
        );
        SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);
        return hBtn;
    };

    m_hBtnSearchPrev = makeBtn(ID_SEARCH_PREV, L"Prev", 44);
    m_hBtnSearchNext = makeBtn(ID_SEARCH_NEXT, L"Next", 44);

    m_hLblSearchMatches = CreateWindowExW(
        0, L"STATIC", L"0 matches",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, 90, 26,
        m_hSearchBar, NULL, m_hInstance, NULL
    );
    SendMessage(m_hLblSearchMatches, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);

    m_hChkMatchCase = CreateWindowExW(
        0, L"BUTTON", L"Match Case",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        0, 0, 95, 26,
        m_hSearchBar, NULL, m_hInstance, NULL
    );
    SendMessage(m_hChkMatchCase, WM_SETFONT, reinterpret_cast<WPARAM>(m_hToolbarFont), TRUE);

    m_hBtnSearchClose = makeBtn(ID_SEARCH_CLOSE, L"X", 30);
}

void MainWindow::LayoutChildren() {
    if (!m_hWnd) return;

    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;

    int curY = 0;

    // 1. Tab Bar
    m_tabControl.SetBounds(0, curY, clientWidth, TAB_BAR_HEIGHT);
    curY += TAB_BAR_HEIGHT;

    // 2. Toolbar
    if (m_hToolbar) {
        MoveWindow(m_hToolbar, 0, curY, clientWidth, TOOLBAR_HEIGHT, TRUE);

        // Layout Toolbar buttons
        int btnY = (TOOLBAR_HEIGHT - 28) / 2;
        int x = 8;
        auto place = [&](HWND h, int w) {
            if (h) {
                MoveWindow(h, x, btnY, w, 28, TRUE);
                x += w + 6;
            }
        };

        place(m_hBtnOpen, 56);
        place(m_hBtnSidebar, 56);
        x += 8; // separator
        place(m_hBtnPrev, 34);
        place(m_hBtnNext, 34);
        if (m_hEditPage) {
            MoveWindow(m_hEditPage, x, btnY + 1, 42, 26, TRUE);
            x += 46;
        }
        if (m_hLblTotalPages) {
            MoveWindow(m_hLblTotalPages, x, btnY + 1, 56, 26, TRUE);
            x += 60;
        }
        x += 8; // separator
        place(m_hBtnZoomOut, 34);
        place(m_hBtnZoomIn, 34);
        place(m_hBtnFitWidth, 54);
        place(m_hBtnFitPage, 66);
        place(m_hBtnActual, 50);
        x += 8; // separator
        place(m_hBtnRotate, 58);
        place(m_hBtnSearch, 52);

        // Theme toggle button aligned to right
        if (m_hBtnTheme) {
            MoveWindow(m_hBtnTheme, clientWidth - 68, btnY, 60, 28, TRUE);
        }
    }
    curY += TOOLBAR_HEIGHT;

    // 3. Search Bar (if visible)
    if (m_searchVisible && m_hSearchBar) {
        MoveWindow(m_hSearchBar, 0, curY, clientWidth, SEARCH_BAR_HEIGHT, TRUE);

        int sy = (SEARCH_BAR_HEIGHT - 26) / 2;
        int sx = 8;
        if (m_hEditSearch) {
            MoveWindow(m_hEditSearch, sx, sy, 220, 26, TRUE);
            sx += 226;
        }
        if (m_hBtnSearchPrev) {
            MoveWindow(m_hBtnSearchPrev, sx, sy, 48, 26, TRUE);
            sx += 52;
        }
        if (m_hBtnSearchNext) {
            MoveWindow(m_hBtnSearchNext, sx, sy, 48, 26, TRUE);
            sx += 52;
        }
        if (m_hLblSearchMatches) {
            MoveWindow(m_hLblSearchMatches, sx, sy, 100, 26, TRUE);
            sx += 106;
        }
        if (m_hChkMatchCase) {
            MoveWindow(m_hChkMatchCase, sx, sy, 100, 26, TRUE);
            sx += 106;
        }
        if (m_hBtnSearchClose) {
            MoveWindow(m_hBtnSearchClose, clientWidth - 38, sy, 30, 26, TRUE);
        }

        curY += SEARCH_BAR_HEIGHT;
    }

    // 4. Content Area: Sidebar + Splitter + PdfView
    int contentHeight = clientHeight - curY;
    if (contentHeight < 0) contentHeight = 0;

    int viewerX = 0;
    int viewerW = clientWidth;

    if (m_sidebarVisible) {
        int sbW = std::clamp(m_sidebarWidth, MIN_SIDEBAR_WIDTH, clientWidth - 200);
        m_thumbnailView.SetBounds(0, curY, sbW, contentHeight);
        ShowWindow(m_thumbnailView.GetHWND(), SW_SHOW);

        viewerX = sbW + SPLITTER_WIDTH;
        viewerW = clientWidth - viewerX;
    } else {
        ShowWindow(m_thumbnailView.GetHWND(), SW_HIDE);
    }

    m_pdfView.SetBounds(viewerX, curY, std::max(10, viewerW), contentHeight);
}

void MainWindow::ApplyTheme() {
    m_tabControl.SetTheme(&m_theme);
    m_pdfView.SetTheme(&m_theme);
    m_thumbnailView.SetTheme(&m_theme);

    if (m_hBtnTheme) {
        SetWindowTextW(m_hBtnTheme, m_theme.isDark ? L"Light" : L"Dark");
    }

    if (m_hToolbar) InvalidateRect(m_hToolbar, NULL, TRUE);
    if (m_hSearchBar) InvalidateRect(m_hSearchBar, NULL, TRUE);
    if (m_hWnd) InvalidateRect(m_hWnd, NULL, TRUE);
}

void MainWindow::UpdateToolbarState() {
    bool hasDoc = (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()) &&
                   m_tabs[m_activeTabIndex].doc && m_tabs[m_activeTabIndex].doc->IsOpen());

    EnableWindow(m_hBtnPrev, hasDoc);
    EnableWindow(m_hBtnNext, hasDoc);
    EnableWindow(m_hEditPage, hasDoc);
    EnableWindow(m_hBtnZoomIn, hasDoc);
    EnableWindow(m_hBtnZoomOut, hasDoc);
    EnableWindow(m_hBtnFitWidth, hasDoc);
    EnableWindow(m_hBtnFitPage, hasDoc);
    EnableWindow(m_hBtnActual, hasDoc);
    EnableWindow(m_hBtnRotate, hasDoc);
    EnableWindow(m_hBtnSearch, hasDoc);

    if (!hasDoc) {
        SetWindowTextW(m_hEditPage, L"0");
        SetWindowTextW(m_hLblTotalPages, L"/ 0");
    }
}

void MainWindow::OpenFile(const std::wstring& filePath) {
    if (filePath.empty()) return;

    // Check if already open
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (_wcsicmp(m_tabs[i].progress.filePath.c_str(), filePath.c_str()) == 0) {
            m_tabControl.SetActiveIndex(static_cast<int>(i), true);
            return;
        }
    }

    // Check if file exists
    DWORD attribs = GetFileAttributesW(filePath.c_str());
    if (attribs == INVALID_FILE_ATTRIBUTES || (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
        ShowRelocateFileDialog(filePath);
        return;
    }

    // Compute metadata
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    int64_t fileSize = 0;
    int64_t mtime = 0;
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER li;
        GetFileSizeEx(hFile, &li);
        fileSize = li.QuadPart;
        FILETIME ft;
        GetFileTime(hFile, NULL, NULL, &ft);
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        mtime = uli.QuadPart;
        CloseHandle(hFile);
    }

    std::string fileHash = Database::ComputeFileHash(filePath);

    // Retrieve reading progress from SQLite
    DocumentProgress progress;
    auto optProg = m_db.GetProgressByPath(filePath);
    if (optProg) {
        progress = *optProg;
    } else {
        auto optByMeta = m_db.FindProgressByMetadata(fileSize, fileHash);
        if (optByMeta) {
            progress = *optByMeta;
            progress.filePath = filePath;
        } else {
            progress.filePath = filePath;
            progress.fileSize = fileSize;
            progress.lastModified = mtime;
            progress.fileHash = fileHash;
            progress.lastPage = 0;
            progress.scrollX = 0;
            progress.scrollY = 0;
            progress.zoomLevel = 1.0;
            progress.zoomMode = static_cast<int>(ZoomMode::FitWidth);
            progress.rotation = 0;
        }
    }

    // Open PDFium Document
    auto doc = std::make_shared<Document>();
    if (!doc->Open(filePath)) {
        MessageBoxW(m_hWnd, (L"Failed to open PDF document:\n" + filePath).c_str(),
                    L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    progress.totalPages = doc->GetPageCount();

    TabState tab;
    tab.doc = doc;
    tab.progress = progress;
    tab.isLoaded = true;

    m_tabs.push_back(tab);
    int newIndex = static_cast<int>(m_tabs.size()) - 1;

    m_tabControl.AddTab(doc->GetFileName(), filePath, true);
}

void MainWindow::OpenFiles(const std::vector<std::wstring>& filePaths) {
    for (const auto& path : filePaths) {
        OpenFile(path);
    }
}

void MainWindow::SwitchToTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    // Save previous active tab state
    SaveCurrentTabProgress();

    m_activeTabIndex = index;
    auto& tab = m_tabs[index];

    // Set document in viewers
    m_pdfView.SetDocument(tab.doc);
    m_thumbnailView.SetDocument(tab.doc);

    // Restore exact rotation, zoom, page and scroll position
    m_pdfView.SetRotation(tab.progress.rotation);

    if (tab.progress.zoomMode == static_cast<int>(ZoomMode::FitWidth)) {
        m_pdfView.ZoomFitWidth();
    } else if (tab.progress.zoomMode == static_cast<int>(ZoomMode::FitPage)) {
        m_pdfView.ZoomFitPage();
    } else if (tab.progress.zoomMode == static_cast<int>(ZoomMode::ActualSize)) {
        m_pdfView.ZoomActual();
    } else {
        m_pdfView.SetZoom(tab.progress.zoomLevel);
    }

    m_pdfView.SetScrollPosition(tab.progress.scrollX, tab.progress.scrollY);
    m_pdfView.GoToPage(tab.progress.lastPage, false);

    UpdateToolbarState();

    // Update window title
    std::wstring title = tab.doc->GetFileName() + L" - PdfViewer";
    SetWindowTextW(m_hWnd, title.c_str());

    FocusViewer();
}

void MainWindow::SaveCurrentTabProgress() {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        auto& prog = m_tabs[m_activeTabIndex].progress;
        prog.scrollX = m_pdfView.GetScrollX();
        prog.scrollY = m_pdfView.GetScrollY();
        prog.lastPage = m_pdfView.GetCurrentPage();
        prog.zoomLevel = m_pdfView.GetZoom();
        prog.zoomMode = static_cast<int>(m_pdfView.GetZoomMode());
        prog.rotation = m_pdfView.GetRotation();
        m_db.SaveProgress(prog);
    }
}

void MainWindow::CloseCurrentTab() {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabControl.RemoveTab(m_activeTabIndex);
    }
}

void MainWindow::ReopenClosedTab() {
    if (!m_recentlyClosedFiles.empty()) {
        std::wstring last = m_recentlyClosedFiles.back();
        m_recentlyClosedFiles.pop_back();
        OpenFile(last);
    }
}

void MainWindow::NextTab() {
    int count = m_tabControl.GetTabCount();
    if (count > 1) {
        int next = (m_activeTabIndex + 1) % count;
        m_tabControl.SetActiveIndex(next, true);
    }
}

void MainWindow::PrevTab() {
    int count = m_tabControl.GetTabCount();
    if (count > 1) {
        int prev = (m_activeTabIndex - 1 + count) % count;
        m_tabControl.SetActiveIndex(prev, true);
    }
}

void MainWindow::ToggleSidebar() {
    m_sidebarVisible = !m_sidebarVisible;
    LayoutChildren();
}

void MainWindow::ToggleTheme() {
    m_theme.UpdateTheme(!m_theme.isDark);
    m_settings.isDarkMode = m_theme.isDark;
    ApplyTheme();
}

void MainWindow::ToggleFullscreen() {
    if (!m_isFullscreen) {
        GetWindowPlacement(m_hWnd, &m_prevPlacement);
        m_prevStyle = GetWindowLong(m_hWnd, GWL_STYLE);

        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(m_hWnd, GWL_STYLE, m_prevStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(m_hWnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            m_isFullscreen = true;
        }
    } else {
        SetWindowLong(m_hWnd, GWL_STYLE, m_prevStyle);
        SetWindowPlacement(m_hWnd, &m_prevPlacement);
        SetWindowPos(m_hWnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        m_isFullscreen = false;
    }
}

void MainWindow::OpenSearch() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= static_cast<int>(m_tabs.size())) return;

    m_searchVisible = true;
    ShowWindow(m_hSearchBar, SW_SHOW);
    LayoutChildren();
    SetFocus(m_hEditSearch);
    SendMessage(m_hEditSearch, EM_SETSEL, 0, -1);
}

void MainWindow::CloseSearch() {
    m_searchVisible = false;
    ShowWindow(m_hSearchBar, SW_HIDE);
    m_currentMatches.clear();
    m_currentMatchIndex = -1;
    m_pdfView.SetSearchMatches({}, -1);
    LayoutChildren();
    SetFocus(m_pdfView.GetHWND());
}

void MainWindow::ExecuteSearch(bool next) {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= static_cast<int>(m_tabs.size())) return;

    wchar_t query[256] = {};
    GetWindowTextW(m_hEditSearch, query, 255);
    std::wstring q(query);

    if (q.empty()) {
        m_currentMatches.clear();
        m_currentMatchIndex = -1;
        m_pdfView.SetSearchMatches({}, -1);
        SetWindowTextW(m_hLblSearchMatches, L"0 matches");
        return;
    }

    bool matchCase = (SendMessage(m_hChkMatchCase, BM_GETCHECK, 0, 0) == BST_CHECKED);

    m_currentMatches = m_tabs[m_activeTabIndex].doc->Search(q, matchCase);
    int total = static_cast<int>(m_currentMatches.size());

    if (total == 0) {
        m_currentMatchIndex = -1;
        m_pdfView.SetSearchMatches({}, -1);
        SetWindowTextW(m_hLblSearchMatches, L"0 matches");
        return;
    }

    if (next) {
        m_currentMatchIndex = (m_currentMatchIndex + 1) % total;
    } else {
        m_currentMatchIndex = (m_currentMatchIndex - 1 + total) % total;
    }

    std::wstring matchText = L"Match " + std::to_wstring(m_currentMatchIndex + 1) + L" of " + std::to_wstring(total);
    SetWindowTextW(m_hLblSearchMatches, matchText.c_str());

    m_pdfView.SetSearchMatches(m_currentMatches, m_currentMatchIndex);
    m_pdfView.SetActiveSearchMatch(m_currentMatchIndex);
}

void MainWindow::ShowOpenFileDialog() {
    WCHAR fileBuffer[4096] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"PDF Documents (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = sizeof(fileBuffer) / sizeof(WCHAR);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        // Handle single or multiple selected files
        WCHAR* p = fileBuffer;
        std::wstring dir = p;
        p += dir.length() + 1;

        if (*p == L'\0') {
            // Single file
            OpenFile(dir);
        } else {
            // Multiple files: first string is directory, followed by filenames
            std::vector<std::wstring> files;
            while (*p != L'\0') {
                std::wstring fullPath = dir + L"\\" + p;
                files.push_back(fullPath);
                p += wcslen(p) + 1;
            }
            OpenFiles(files);
        }
    }
}

void MainWindow::ShowRelocateFileDialog(const std::wstring& missingPath) {
    std::wstring msg = L"Document not found at original location:\n" + missingPath +
                       L"\n\nWould you like to locate this file?";
    if (MessageBoxW(m_hWnd, msg.c_str(), L"Locate File", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        WCHAR fileBuffer[MAX_PATH] = {};
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(OPENFILENAMEW);
        ofn.hwndOwner = m_hWnd;
        ofn.lpstrFilter = L"PDF Documents (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;

        if (GetOpenFileNameW(&ofn)) {
            std::wstring newPath(fileBuffer);
            m_db.UpdateDocumentPath(missingPath, newPath);
            OpenFile(newPath);
        }
    }
}

void MainWindow::RestoreSession() {
    auto sessionTabs = m_db.LoadSession();
    if (sessionTabs.empty()) return;

    int activeIdx = 0;
    for (size_t i = 0; i < sessionTabs.size(); ++i) {
        OpenFile(sessionTabs[i].filePath);
        if (sessionTabs[i].isActive) {
            activeIdx = static_cast<int>(i);
        }
    }

    if (!m_tabs.empty() && activeIdx >= 0 && activeIdx < static_cast<int>(m_tabs.size())) {
        m_tabControl.SetActiveIndex(activeIdx, true);
    }
}

void MainWindow::SaveSession() {
    SaveCurrentTabProgress();

    std::vector<TabSessionItem> sessionItems;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        TabSessionItem item;
        item.tabIndex = static_cast<int>(i);
        item.filePath = m_tabs[i].progress.filePath;
        item.isActive = (static_cast<int>(i) == m_activeTabIndex);
        sessionItems.push_back(item);
    }

    m_db.SaveSession(sessionItems);

    // Save app settings
    WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement(m_hWnd, &wp);
    m_settings.isMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
    m_settings.windowX = wp.rcNormalPosition.left;
    m_settings.windowY = wp.rcNormalPosition.top;
    m_settings.windowWidth = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    m_settings.windowHeight = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    m_settings.isDarkMode = m_theme.isDark;
    m_settings.sidebarVisible = m_sidebarVisible;
    m_settings.sidebarWidth = m_sidebarWidth;

    m_db.SaveAppSettings(m_settings);
}

void MainWindow::ZoomIn() { m_pdfView.ZoomIn(); }
void MainWindow::ZoomOut() { m_pdfView.ZoomOut(); }
void MainWindow::ZoomActual() { m_pdfView.ZoomActual(); }
void MainWindow::ZoomFitWidth() { m_pdfView.ZoomFitWidth(); }
void MainWindow::ZoomFitPage() { m_pdfView.ZoomFitPage(); }
void MainWindow::RotateCW() { m_pdfView.RotateCW(); }
void MainWindow::RotateCCW() { m_pdfView.RotateCCW(); }
void MainWindow::NextPage() { m_pdfView.NextPage(); }
void MainWindow::PrevPage() { m_pdfView.PrevPage(); }
void MainWindow::FirstPage() { m_pdfView.FirstPage(); }
void MainWindow::LastPage() { m_pdfView.LastPage(); }
void MainWindow::ScrollUp(int amount) { m_pdfView.SetScrollPosition(m_pdfView.GetScrollX(), m_pdfView.GetScrollY() - amount); }
void MainWindow::ScrollDown(int amount) { m_pdfView.SetScrollPosition(m_pdfView.GetScrollX(), m_pdfView.GetScrollY() + amount); }
void MainWindow::CopySelection() { m_pdfView.CopySelectionToClipboard(); }
void MainWindow::SelectAll() { m_pdfView.SelectAll(); }

bool MainWindow::IsEditControlFocused() const {
    HWND hFocus = GetFocus();
    return (hFocus != NULL && (hFocus == m_hEditPage || hFocus == m_hEditSearch));
}

void MainWindow::FocusViewer() {
    if (m_pdfView.GetHWND() && !m_searchVisible) {
        SetFocus(m_pdfView.GetHWND());
    }
}

LRESULT CALLBACK MainWindow::ToolbarSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (msg == WM_COMMAND) {
        LRESULT res = SendMessage(pThis->m_hWnd, WM_COMMAND, wParam, lParam);
        pThis->FocusViewer();
        return res;
    }
    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORBTN) {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, pThis->m_theme.textPrimary);
        return reinterpret_cast<LRESULT>(pThis->m_theme.hbrToolbar);
    }
    if (msg == WM_ERASEBKGND) {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, pThis->m_theme.hbrToolbar);
        return 1;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::EditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        wchar_t buf[32] = {};
        GetWindowTextW(hWnd, buf, 31);
        int page = _wtoi(buf);
        if (page > 0) {
            pThis->m_pdfView.GoToPage(page - 1);
        }
        return 0;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::SearchEditSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                   UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(dwRefData);
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            pThis->ExecuteSearch(!isShift);
            return 0;
        } else if (wParam == VK_ESCAPE) {
            pThis->CloseSearch();
            return 0;
        }
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            LayoutChildren();
            return 0;

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_FILE_OPEN:
                    ShowOpenFileDialog();
                    return 0;
                case ID_FILE_CLOSE_TAB:
                    CloseCurrentTab();
                    return 0;
                case ID_FILE_REOPEN_CLOSED:
                    ReopenClosedTab();
                    return 0;
                case ID_FILE_PREV_TAB:
                    PrevTab();
                    return 0;
                case ID_FILE_NEXT_TAB:
                    NextTab();
                    return 0;
                case ID_NAV_PREV:
                    m_pdfView.PrevPage();
                    return 0;
                case ID_NAV_NEXT:
                    m_pdfView.NextPage();
                    return 0;
                case ID_VIEW_ZOOM_IN:
                    m_pdfView.ZoomIn();
                    return 0;
                case ID_VIEW_ZOOM_OUT:
                    m_pdfView.ZoomOut();
                    return 0;
                case ID_VIEW_ZOOM_ACTUAL:
                    m_pdfView.ZoomActual();
                    return 0;
                case ID_VIEW_ZOOM_FIT_WIDTH:
                    m_pdfView.ZoomFitWidth();
                    return 0;
                case ID_VIEW_ZOOM_FIT_PAGE:
                    m_pdfView.ZoomFitPage();
                    return 0;
                case ID_VIEW_ROTATE_CW:
                    m_pdfView.RotateCW();
                    return 0;
                case ID_VIEW_ROTATE_CCW:
                    m_pdfView.RotateCCW();
                    return 0;
                case ID_VIEW_FULLSCREEN:
                    ToggleFullscreen();
                    return 0;
                case ID_VIEW_TOGGLE_SIDEBAR:
                    ToggleSidebar();
                    return 0;
                case ID_VIEW_TOGGLE_THEME:
                    ToggleTheme();
                    return 0;
                case ID_SEARCH_OPEN:
                    if (m_searchVisible) CloseSearch();
                    else OpenSearch();
                    return 0;
                case ID_SEARCH_PREV:
                    ExecuteSearch(false);
                    return 0;
                case ID_SEARCH_NEXT:
                    ExecuteSearch(true);
                    return 0;
                case ID_SEARCH_CLOSE:
                    CloseSearch();
                    return 0;
                case ID_EDIT_COPY:
                    m_pdfView.CopySelectionToClipboard();
                    return 0;
            }
            break;
        }

        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            std::vector<std::wstring> files;
            for (UINT i = 0; i < fileCount; ++i) {
                WCHAR path[MAX_PATH];
                if (DragQueryFileW(hDrop, i, path, MAX_PATH)) {
                    files.push_back(path);
                }
            }
            DragFinish(hDrop);
            OpenFiles(files);
            return 0;
        }

        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT && m_sidebarVisible) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(m_hWnd, &pt);
                int splitX = m_sidebarWidth;
                if (pt.x >= splitX && pt.x <= splitX + SPLITTER_WIDTH && pt.y >= TAB_BAR_HEIGHT + TOOLBAR_HEIGHT) {
                    SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                    return TRUE;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            if (m_sidebarVisible) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int splitX = m_sidebarWidth;
                if (x >= splitX && x <= splitX + SPLITTER_WIDTH && y >= TAB_BAR_HEIGHT + TOOLBAR_HEIGHT) {
                    m_isDraggingSplitter = true;
                    m_dragStartX = x;
                    m_dragStartWidth = m_sidebarWidth;
                    SetCapture(m_hWnd);
                    return 0;
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (m_isDraggingSplitter) {
                int x = GET_X_LPARAM(lParam);
                int delta = x - m_dragStartX;
                m_sidebarWidth = std::clamp(m_dragStartWidth + delta, MIN_SIDEBAR_WIDTH, 500);
                LayoutChildren();
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (m_isDraggingSplitter) {
                m_isDraggingSplitter = false;
                ReleaseCapture();
                return 0;
            }
            break;
        }

        case WM_KEYDOWN: {
            bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            if (isCtrl) {
                switch (wParam) {
                    case 'O':
                        ShowOpenFileDialog();
                        return 0;
                    case 'W':
                        CloseCurrentTab();
                        return 0;
                    case 'T':
                        if (isShift) ReopenClosedTab();
                        return 0;
                    case VK_TAB:
                        if (isShift) PrevTab();
                        else NextTab();
                        return 0;
                    case 'F':
                        OpenSearch();
                        return 0;
                    case 'B':
                        ToggleSidebar();
                        return 0;
                    case VK_OEM_PLUS:
                    case VK_ADD:
                        m_pdfView.ZoomIn();
                        return 0;
                    case VK_OEM_MINUS:
                    case VK_SUBTRACT:
                        m_pdfView.ZoomOut();
                        return 0;
                    case '0':
                        m_pdfView.ZoomFitPage();
                        return 0;
                    case '1':
                        m_pdfView.ZoomActual();
                        return 0;
                    case '2':
                        m_pdfView.ZoomFitWidth();
                        return 0;
                    case 'R':
                        if (isShift) m_pdfView.RotateCCW();
                        else m_pdfView.RotateCW();
                        return 0;
                }
            } else {
                switch (wParam) {
                    case VK_F11:
                        ToggleFullscreen();
                        return 0;
                    case VK_F4:
                        ToggleSidebar();
                        return 0;
                    case VK_F3:
                        if (m_searchVisible) ExecuteSearch(!isShift);
                        return 0;
                    case VK_ESCAPE:
                        if (m_isFullscreen) ToggleFullscreen();
                        else if (m_searchVisible) CloseSearch();
                        return 0;
                }
            }
            break;
        }

        case WM_CLOSE:
            SaveSession();
            DestroyWindow(m_hWnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(m_hWnd, msg, wParam, lParam);
}

