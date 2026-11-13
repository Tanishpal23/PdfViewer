# PdfViewer — Lightweight Offline Windows PDF Viewer

A fast, fully offline, standalone desktop PDF viewer for **Windows 10 and Windows 11**.

Built using **native C++ (Win32)**, the high-performance **Google PDFium** rendering engine, and **SQLite** for local reading progress and settings persistence.

---

## ✨ Features

### 1. Multiple PDF Tabs
- **Independent Tabbed Interface**: Open multiple PDF files simultaneously in clean, modern tabs.
- **Reading Position Preservation**: Switch between tabs instantly without losing page number, scroll position, zoom, or rotation.
- **Drag-and-Drop**: Drag one or multiple PDF documents directly onto the viewer from Windows File Explorer.
- **Session Restoration**: Automatically remembers and restores all previously opened tabs on restart.
- **Reopen Closed Tabs**: Reopen recently closed documents via `Ctrl + Shift + T`.
- **Keyboard Navigation**: Cycle tabs using `Ctrl + Tab` (Next) and `Ctrl + Shift + Tab` (Previous), or close tabs with `Ctrl + W` or middle-click.

### 2. Automatic Reading Progress (Most Critical Feature)
- **Automatic & Instantaneous**: Remembers your exact reading progress whenever you scroll, navigate, zoom, or rotate.
- **Zero Loss**: Reading position is preserved even if the application exits, the window is closed, or Windows restarts.
- **Independent Tracking**: Every document has its own independent reading history entry stored in SQLite.
- **Exact Position Restoration**:
  - Last viewed page number.
  - Exact vertical & horizontal scroll coordinates (`scroll_x`, `scroll_y`).
  - Zoom factor and zoom mode (`Fit Width`, `Fit Page`, `Actual Size`, or custom zoom).
  - Page rotation (0°, 90°, 180°, 270°).
- **Relocation & Rename Resilience**: Uses content hashing (FNV-1a 64-bit of header + file size) and metadata to recognize moved or renamed documents and restore reading progress.

### 3. PDF Viewing & Inspection Features
- **Page Navigation**: Previous/next page buttons, direct jump page box (`[ 42 ] / 350`), Home/End for first/last page.
- **Zoom Controls**: Smooth zoom in/out (`Ctrl + Plus`, `Ctrl + Minus`), `Ctrl + MouseWheel` centered zoom, Fit Width (`Ctrl + 2`), Fit Page (`Ctrl + 0`), and Actual Size (`Ctrl + 1`).
- **Rotation**: 90° clockwise (`Ctrl + R`) and counter-clockwise (`Ctrl + Shift + R`).
- **Text Search**: In-document search (`Ctrl + F`) with match highlighting, "Find Next" / "Find Prev" navigation, match count, and case-sensitivity option.
- **Thumbnails Sidebar**: Collapsible, resizable sidebar with lazy thumbnail generation and click-to-jump navigation.
- **Select & Copy Text**: Left-click and drag across any text to select, and press `Ctrl + C` or right-click to copy to clipboard.
- **Fullscreen Mode**: Toggle borderless fullscreen reading via `F11`.

### 4. UI & Performance
- **Modern Dark & Light Themes**: Dark theme by default with a compact toolbar, minimal tabs, and distraction-free document canvas. Light theme available via a 1-click toggle.
- **Fast Startup & Low RAM**: Native Win32 GDI double-buffering. Only visible pages and immediate buffer pages are rendered in memory.
- **Large PDF Handling**: Easily opens and smoothly scrolls documents with hundreds or thousands of pages.
- **100% Offline & Private**: Zero network connections, zero telemetry, zero analytics, zero external API dependencies. All PDFs and history stay strictly on your local computer.

---

## ⌨️ Keyboard Shortcuts Reference

| Shortcut | Description |
|---|---|
| `Ctrl + O` | Open PDF file(s) dialog |
| `Ctrl + W` | Close current tab |
| `Ctrl + Shift + T` | Reopen recently closed tab |
| `Ctrl + Tab` | Next tab |
| `Ctrl + Shift + Tab` | Previous tab |
| `Ctrl + F` | Open in-document text search |
| `Enter` / `F3` | Find next search match |
| `Shift + Enter` / `Shift + F3` | Find previous search match |
| `Ctrl + Plus` / `Ctrl + =` | Zoom In |
| `Ctrl + Minus` / `Ctrl + -` | Zoom Out |
| `Ctrl + MouseWheel` | Zoom In / Out at mouse pointer |
| `Ctrl + 0` | Fit Page to window |
| `Ctrl + 1` | Actual Size (100% scale) |
| `Ctrl + 2` | Fit Width to window |
| `Ctrl + R` | Rotate 90° Clockwise |
| `Ctrl + Shift + R` | Rotate 90° Counter-Clockwise |
| `Page Up` / `Left Arrow` | Previous Page |
| `Page Down` / `Right Arrow` | Next Page |
| `Home` / `End` | First Page / Last Page |
| `Ctrl + C` | Copy selected text to clipboard |
| `Ctrl + A` | Select all text on page |
| `F11` | Toggle Fullscreen mode |
| `F4` / `Ctrl + B` | Toggle Thumbnails Sidebar |
| `Esc` | Close search bar or exit fullscreen |

---

## 📁 Project Architecture

```
PdfViewer/
├── build/                 # Intermediate build objects (*.o, *.res)
├── dist/                  # Standalone release distribution
│   ├── PdfViewer.exe      # Standalone native 64-bit Windows executable
│   ├── pdfium.dll         # Fast Google PDFium rendering engine
│   └── README.txt         # Quick user guide
├── resources/
│   ├── app.manifest       # DPI-aware v2, Common Controls v6 manifest
│   └── app.rc             # Windows version info & manifest resource
├── sample_docs/           # Generated test PDFs (3 to 200 pages)
├── src/
│   ├── Common.h           # DPI helpers, theme colors, command IDs
│   ├── Database.h / .cpp  # SQLite persistent reading progress & session manager
│   ├── Document.h / .cpp  # PDFium wrapper: geometry, rendering, text, search
│   ├── TabControl.h / .cpp# Custom dark/light tab bar with close buttons & scroll
│   ├── PdfView.h / .cpp   # Continuous scrolling view, zoom, selection, search
│   ├── ThumbnailView.h/.cpp# Sidebar thumbnail previews with lazy generation
│   ├── MainWindow.h / .cpp# Win32 main window, toolbar, shortcuts, drag & drop
│   └── main.cpp           # WinMain entry point, command line parser, message loop
├── tests/
│   └── test_core.cpp      # Automated test suite (8 tests, 100% pass)
├── third_party/
│   ├── pdfium/            # PDFium C headers, DLL, and import library
│   └── sqlite/            # SQLite amalgamation (sqlite3.c, sqlite3.h)
├── build.bat              # One-click Windows build script
├── create_test_pdfs.py    # Script to generate sample PDFs for testing
├── PdfViewer-Portable-win-x64.zip # Ready-to-use portable zip archive
└── README.md              # Project documentation
```

---

## 🛠️ Build Instructions

### Prerequisites
- Windows 10 or Windows 11 (x64)
- MinGW-w64 (GCC/G++ with UCRT64 or MSYS2)

### Building
Run the one-click build script:
```cmd
build.bat
```
The compiled standalone executable and runtime will be placed in `dist/`:
- `dist\PdfViewer.exe`
- `dist\pdfium.dll`

### Running
Double click `dist\PdfViewer.exe` or run:
```cmd
dist\PdfViewer.exe [optional_path_to_pdf]
```

### Running the Test Suite
To verify the core engine and reading progress persistence:
```cmd
g++ -O2 -std=c++17 -Isrc -Ithird_party/pdfium/include -Ithird_party/sqlite/sqlite-amalgamation-3460100 tests/test_core.cpp build/Database.o build/Document.o build/sqlite3.o third_party/pdfium/lib/pdfium.dll.lib -lgdi32 -luser32 -lshell32 -lole32 -static-libgcc -static-libstdc++ -o build/test_core.exe
.\build\test_core.exe
```

---

## 💾 Local Storage & Database Schema

Application metadata and reading positions are stored in SQLite at:
`%APPDATA%\PdfViewer\history.db` (or adjacent portable database).

### SQLite Tables:
1. `documents`: Tracks `file_path`, `file_size`, `last_modified`, `file_hash`, `last_page`, `scroll_x`, `scroll_y`, `zoom_level`, `zoom_mode`, `rotation`, `total_pages`, `last_opened`.
2. `session_tabs`: Tracks all open tabs (`tab_index`, `file_path`, `is_active`) for seamless restart restoration.
3. `settings`: Tracks UI settings including theme mode (`dark`/`light`), window position, size, maximized state, and sidebar layout.

# PdfViewer
