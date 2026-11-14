========================================================================
PdfViewer - Lightweight Fast Offline Windows PDF Viewer (Win32 Native)
========================================================================

PdfViewer is a high-performance, fully offline, standalone desktop PDF 
viewer designed natively for Windows 10 and Windows 11.

KEY HIGHLIGHTS:
* Zero Dependencies: Runs out of the box with no runtime installations.
  (NO Node.js, NO Python, NO Electron, NO Chromium browser wrappers).
* Blazing Fast PDFium Engine: Google Chrome's PDF engine for instant rendering.
* Reliable Reading Progress (SQLite): Automatically preserves your exact
  reading position (last page, scroll offset, zoom level, rotation) for
  every PDF independently.
* Multiple Independent Tabs: Open many documents at once, switch tabs without
  losing position, and restore previous sessions across app restarts.
* Fully Offline & Private: Zero telemetry, zero analytics, zero network requests.

------------------------------------------------------------------------
HOW TO RUN:
------------------------------------------------------------------------
Simply run `PdfViewer.exe` from this folder!
Make sure `pdfium.dll` remains in the same folder as `PdfViewer.exe`.

Drag & drop any PDF file directly into the application window or use Ctrl + O.

------------------------------------------------------------------------
KEYBOARD SHORTCUTS:
------------------------------------------------------------------------
File & Tab Management:
  Ctrl + O          : Open PDF document (multi-select supported)
  Ctrl + W          : Close current tab
  Ctrl + Shift + T  : Reopen recently closed tab
  Ctrl + Tab        : Switch to next tab
  Ctrl + Shift + Tab: Switch to previous tab

Viewing & Navigation:
  Page Up / Left    : Previous page
  Page Down / Right : Next page
  Home / End        : First page / Last page
  Ctrl + Plus / =   : Zoom in
  Ctrl + Minus / -  : Zoom out
  Ctrl + MouseWheel : Smooth zoom at cursor
  Ctrl + 0          : Fit page in window
  Ctrl + 1          : Actual size (100% zoom)
  Ctrl + 2          : Fit width to window
  Ctrl + R          : Rotate 90 degrees clockwise
  Ctrl + Shift + R  : Rotate 90 degrees counter-clockwise
  F11               : Fullscreen mode toggle
  F4 / Ctrl + B     : Toggle page thumbnails sidebar

Text & Search:
  Ctrl + F          : Open Search bar
  Enter / F3        : Find next match
  Shift+Enter / S+F3: Find previous match
  Esc               : Close search bar / Exit fullscreen
  Ctrl + C          : Copy selected text to clipboard
  Ctrl + A          : Select all text on current page

------------------------------------------------------------------------
STORAGE & METADATA:
------------------------------------------------------------------------
Reading progress, session history, and application settings are saved 
locally using high-performance SQLite in:
  %APPDATA%\PdfViewer\history.db

Enjoy lightning-fast, distraction-free document reading!

