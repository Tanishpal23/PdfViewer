# PdfViewer

Lightweight, fast, fully offline Windows PDF Viewer (Windows 10 & 11).

---

## 🚀 How to Run (Pre-built)

No installation required. `PdfViewer` is completely portable.

1. Go to the `dist\` folder.
2. Double-click **`PdfViewer.exe`** (ensure `pdfium.dll` stays in the same folder).
3. Open any PDF via **`Ctrl + O`** or drag and drop files into the window.

### Run with Sample Test PDFs
```powershell
.\dist\PdfViewer.exe sample_docs\1_Quick_Start_Guide.pdf sample_docs\5_Large_Encyclopedia.pdf
```

---

## 🛠️ How to Build from Source

### Prerequisites
* Windows 10 or 11
* MinGW-w64 (GCC / G++ with `windres`)

### Build Executable
Run the one-click build script:
```cmd
build.bat
```
The compiled output will be placed in `dist\PdfViewer.exe`.

---

## 🧪 Run Automated Tests

To verify multi-tab loading, lazy rendering, SQLite reading progress persistence, and text search:

```powershell
# 1. Generate test PDFs (if not already present)
python create_test_pdfs.py

# 2. Compile and run core test suite
g++ -O2 -std=c++17 -Isrc -Ithird_party\pdfium\include -Ithird_party\sqlite\sqlite-amalgamation-3460100 tests\test_core.cpp src\Database.cpp src\Document.cpp third_party\sqlite\sqlite-amalgamation-3460100\sqlite3.c third_party\pdfium\lib\pdfium.dll.lib -o build\test_core.exe
.\build\test_core.exe
```

---

## ⌨️ Shortcuts Reference

| Shortcut | Action |
|---|---|
| `Ctrl + O` | Open PDF file(s) |
| `Ctrl + W` | Close current tab |
| `Ctrl + Shift + T` | Reopen last closed tab |
| `Ctrl + Tab` / `Ctrl + Shift + Tab` | Next / Previous tab |
| `Ctrl + +` / `+` | Zoom In |
| `Ctrl + -` / `-` | Zoom Out |
| `Ctrl + MouseWheel` | Zoom In / Out |
| `Ctrl + 0` / `1` / `2` | Fit Page / Actual Size (100%) / Fit Width |
| `Ctrl + R` / `R` | Rotate Clockwise 90° |
| `Ctrl + Shift + R` | Rotate Counter-Clockwise 90° |
| `Page Down` / `Page Up` | Next / Previous Page |
| `Home` / `End` | First / Last Page |
| `Ctrl + F` | Search Text |
| `F11` | Fullscreen Mode |
| `F4` / `Ctrl + B` | Toggle Thumbnails Sidebar |
