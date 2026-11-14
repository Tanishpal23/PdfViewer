#include "MainWindow.h"
#include <fpdfview.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&icex);

    // Initialize PDFium Library
    FPDF_LIBRARY_CONFIG config = {};
    config.version = 2;
    config.m_pUserFontPaths = nullptr;
    config.m_pIsolate = nullptr;
    config.m_v8EmbedderSlot = 0;
    FPDF_InitLibraryWithConfig(&config);

    int exitCode = 0;
    {
        // Create Main Window
        MainWindow mainWindow;
        if (!mainWindow.Create(hInstance, nCmdShow)) {
            FPDF_DestroyLibrary();
            return 1;
        }

        // Parse command line arguments
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::vector<std::wstring> initialFiles;
        if (argv) {
            for (int i = 1; i < argc; ++i) {
                std::wstring arg = argv[i];
                if (!arg.empty() && arg[0] != L'-' && arg[0] != L'/') {
                    initialFiles.push_back(arg);
                }
            }
            LocalFree(argv);
        }

        if (!initialFiles.empty()) {
            mainWindow.OpenFiles(initialFiles);
        } else {
            // Restore session tabs from previous run
            mainWindow.RestoreSession();
        }

        // Standard Message Loop with Global Shortcut Dispatching
        MSG msg = {};
        while (GetMessageW(&msg, NULL, 0, 0)) {
            // Handle global mouse wheel zoom with Ctrl
            if (msg.message == WM_MOUSEWHEEL) {
                bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (isCtrl) {
                    int delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
                    if (delta > 0) mainWindow.ZoomIn();
                    else mainWindow.ZoomOut();
                    continue;
                }
            }

            // Handle global keyboard shortcuts
            if (msg.message == WM_KEYDOWN) {
                bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool isEditFocused = mainWindow.IsEditControlFocused();

                if (isCtrl) {
                    switch (msg.wParam) {
                        case 'O':
                            SendMessage(mainWindow.GetHWND(), WM_COMMAND, ID_FILE_OPEN, 0);
                            continue;
                        case 'W':
                            mainWindow.CloseCurrentTab();
                            continue;
                        case 'T':
                            if (isShift) mainWindow.ReopenClosedTab();
                            continue;
                        case VK_TAB:
                            if (isShift) mainWindow.PrevTab();
                            else mainWindow.NextTab();
                            continue;
                        case 'F':
                            mainWindow.OpenSearch();
                            continue;
                        case 'B':
                            mainWindow.ToggleSidebar();
                            continue;
                        case 'R':
                            if (isShift) mainWindow.RotateCCW();
                            else mainWindow.RotateCW();
                            continue;
                        case VK_OEM_PLUS:
                        case VK_ADD:
                            mainWindow.ZoomIn();
                            continue;
                        case VK_OEM_MINUS:
                        case VK_SUBTRACT:
                            mainWindow.ZoomOut();
                            continue;
                        case '0':
                        case VK_NUMPAD0:
                            mainWindow.ZoomFitPage();
                            continue;
                        case '1':
                        case VK_NUMPAD1:
                            mainWindow.ZoomActual();
                            continue;
                        case '2':
                        case VK_NUMPAD2:
                            mainWindow.ZoomFitWidth();
                            continue;
                        case 'C':
                            mainWindow.CopySelection();
                            continue;
                        case 'A':
                            mainWindow.SelectAll();
                            continue;
                    }
                } else {
                    // Non-Ctrl shortcuts
                    switch (msg.wParam) {
                        case VK_F11:
                            mainWindow.ToggleFullscreen();
                            continue;
                        case VK_F4:
                            mainWindow.ToggleSidebar();
                            continue;
                        case VK_F3:
                            mainWindow.ExecuteSearch(!isShift);
                            continue;
                        case VK_ESCAPE:
                            if (mainWindow.IsFullscreen()) mainWindow.ToggleFullscreen();
                            else if (mainWindow.IsSearchOpen()) mainWindow.CloseSearch();
                            continue;
                    }

                    // Direct single-key shortcuts when NOT typing in an edit box
                    if (!isEditFocused) {
                        switch (msg.wParam) {
                            case VK_OEM_PLUS:
                            case VK_ADD:
                                mainWindow.ZoomIn();
                                continue;
                            case VK_OEM_MINUS:
                            case VK_SUBTRACT:
                                mainWindow.ZoomOut();
                                continue;
                            case 'R':
                                if (isShift) mainWindow.RotateCCW();
                                else mainWindow.RotateCW();
                                continue;
                            case VK_PRIOR: // Page Up
                            case VK_LEFT:
                            case VK_BACK:
                                mainWindow.PrevPage();
                                continue;
                            case VK_NEXT: // Page Down
                            case VK_RIGHT:
                            case VK_SPACE:
                                mainWindow.NextPage();
                                continue;
                            case VK_HOME:
                                mainWindow.FirstPage();
                                continue;
                            case VK_END:
                                mainWindow.LastPage();
                                continue;
                            case VK_UP:
                                mainWindow.ScrollUp(60);
                                continue;
                            case VK_DOWN:
                                mainWindow.ScrollDown(60);
                                continue;
                        }
                    }
                }
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        exitCode = static_cast<int>(msg.wParam);
    }

    // Clean up PDFium
    FPDF_DestroyLibrary();

    return exitCode;
}

