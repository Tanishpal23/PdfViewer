#pragma once

#include "Common.h"
#include <fpdfview.h>
#include <fpdf_doc.h>
#include <fpdf_text.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

struct PageRect {
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
};

struct SearchMatch {
    int pageIndex = 0;
    int charIndex = 0;
    int charCount = 0;
    std::vector<RECT> screenRects; // Transformed for current zoom & layout
};

struct RenderedPage {
    int pageIndex = -1;
    double zoom = 0;
    int rotation = 0;
    int width = 0;
    int height = 0;
    HBITMAP hBitmap = nullptr;
    void* pixelData = nullptr;
    std::chrono::steady_clock::time_point lastAccess;
};

class Document {
public:
    Document();
    ~Document();

    bool Open(const std::wstring& filePath, const std::string& password = "");
    void Close();
    bool IsOpen() const { return m_doc != nullptr; }

    const std::wstring& GetFilePath() const { return m_filePath; }
    std::wstring GetFileName() const;
    int GetPageCount() const { return m_pageCount; }

    // Page Geometry
    void GetPageSize(int pageIndex, double* width, double* height, int rotation = 0);
    int GetPageRotation(int pageIndex) const;

    // Rendering
    // Renders a page to an HBITMAP with caching.
    HBITMAP GetPageBitmap(int pageIndex, double zoom, int rotation, int* outWidth, int* outHeight);
    // Generates a thumbnail for sidebar
    HBITMAP GetThumbnailBitmap(int pageIndex, int maxWidth, int maxHeight);

    // Text & Selection
    int GetPageCharCount(int pageIndex);
    std::wstring GetPageText(int pageIndex);
    std::wstring GetSelectedText(int pageIndex, int startIndex, int count);
    int GetCharIndexAtPos(int pageIndex, double pageX, double pageY, double tolerance = 5.0);
    std::vector<RECT> GetTextSelectionRects(int pageIndex, int startIndex, int count, double zoom, int rotation, int offsetX, int offsetY);

    // Coordinate conversions
    void PageToDevice(int pageIndex, double pageX, double pageY, double zoom, int rotation, int offsetX, int offsetY, int* deviceX, int* deviceY);
    void DeviceToPage(int pageIndex, int deviceX, int deviceY, double zoom, int rotation, int offsetX, int offsetY, double* pageX, double* pageY);

    // Search
    std::vector<SearchMatch> Search(const std::wstring& query, bool matchCase = false, bool wholeWord = false);

    // Cache management
    void PruneCache(size_t maxCachedPages = 8);
    void ClearCache();

private:
    std::wstring m_filePath;
    FPDF_DOCUMENT m_doc = nullptr;
    int m_pageCount = 0;

    // Cached page sizes (in points)
    std::vector<std::pair<double, double>> m_pageSizes;

    // Cached open pages and text pages
    std::map<int, FPDF_PAGE> m_openPages;
    std::map<int, FPDF_TEXTPAGE> m_openTextPages;

    // LRU Cache for rendered full-size pages
    std::vector<RenderedPage> m_renderedCache;
    // Thumbnails cache
    std::map<int, HBITMAP> m_thumbnailCache;

    std::mutex m_mutex;

    FPDF_PAGE GetOrLoadPage(int pageIndex);
    FPDF_TEXTPAGE GetOrLoadTextPage(int pageIndex);
    void CloseUnusedPages();
};

