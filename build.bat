@echo off
setlocal enabledelayedexpansion

echo ============================================
echo Building PdfViewer Standalone Windows Application
echo ============================================

if not exist build mkdir build
if not exist dist mkdir dist

REM Copy PDFium runtime DLL
copy /Y third_party\pdfium\bin\pdfium.dll dist\pdfium.dll >nul

REM Compile SQLite if needed
if not exist build\sqlite3.o (
    echo [1/7] Compiling SQLite amalgamation...
    gcc -O2 -c third_party\sqlite\sqlite-amalgamation-3460100\sqlite3.c -o build\sqlite3.o
) else (
    echo [1/7] SQLite already compiled.
)

REM Compile Resources
echo [2/7] Compiling Windows Resources...
cd resources
windres app.rc -O coff -o ..\build\app.res
cd ..

REM Compile C++ Modules
set CXXFLAGS=-O2 -std=c++17 -municode -mwindows -Isrc -Ithird_party\pdfium\include -Ithird_party\sqlite\sqlite-amalgamation-3460100

echo [3/7] Compiling Database.cpp...
g++ %CXXFLAGS% -c src\Database.cpp -o build\Database.o

echo [4/7] Compiling Document.cpp...
g++ %CXXFLAGS% -c src\Document.cpp -o build\Document.o

echo [5/7] Compiling UI Components...
g++ %CXXFLAGS% -c src\TabControl.cpp -o build\TabControl.o
g++ %CXXFLAGS% -c src\PdfView.cpp -o build\PdfView.o
g++ %CXXFLAGS% -c src\ThumbnailView.cpp -o build\ThumbnailView.o
g++ %CXXFLAGS% -c src\MainWindow.cpp -o build\MainWindow.o

echo [6/7] Compiling main.cpp...
g++ %CXXFLAGS% -c src\main.cpp -o build\main.o

echo [7/7] Linking PdfViewer.exe...
g++ -O2 -municode -mwindows ^
    build\main.o ^
    build\MainWindow.o ^
    build\PdfView.o ^
    build\ThumbnailView.o ^
    build\TabControl.o ^
    build\Document.o ^
    build\Database.o ^
    build\sqlite3.o ^
    build\app.res ^
    third_party\pdfium\lib\pdfium.dll.lib ^
    -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -lshell32 -lole32 -luxtheme -lmsimg32 ^
    -static-libgcc -static-libstdc++ ^
    -o dist\PdfViewer.exe

if %ERRORLEVEL% equ 0 (
    echo ============================================
    echo SUCCESS: dist\PdfViewer.exe built successfully!
    echo ============================================
) else (
    echo ============================================
    echo FAILED: Compilation or linking failed!
    echo ============================================
    exit /b 1
)

