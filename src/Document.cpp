#include "Document.h"
#include <iostream>

Document::Document() {
}

Document::~Document() {
    Close();
}

bool Document::Open(const std::wstring& filePath, const std::string& password) {
    Close();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = filePath;

    std::string pathUtf8 = Utf16ToUtf8(filePath);
    m_doc = FPDF_LoadDocument(pathUtf8.c_str(), password.empty() ? nullptr : password.c_str());
    if (!m_doc) {
        return false;
    }

    m_pageCount = FPDF_GetPageCount(m_doc);
    m_pageSizes.resize(m_pageCount);

    // Pre-cache page sizes
    for (int i = 0; i < m_pageCount; ++i) {
        double w = 0, h = 0;
        if (FPDF_GetPageSizeByIndex(m_doc, i, &w, &h)) {
            m_pageSizes[i] = { w, h };
        } else {
            m_pageSizes[i] = { 612.0, 792.0 }; // Standard Letter fallback
        }
    }

    return true;
}

void Document::Close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    ClearCache();

    for (auto& pair : m_openTextPages) {
        if (pair.second) FPDFText_ClosePage(pair.second);
    }
    m_openTextPages.clear();

    for (auto& pair : m_openPages) {
        if (pair.second) FPDF_ClosePage(pair.second);
    }
    m_openPages.clear();

    if (m_doc) {
        FPDF_CloseDocument(m_doc);
        m_doc = nullptr;
    }

    m_pageCount = 0;
    m_pageSizes.clear();
    m_filePath.clear();
}

std::wstring Document::GetFileName() const {
    if (m_filePath.empty()) return L"";
    size_t lastSlash = m_filePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return m_filePath.substr(lastSlash + 1);
    }
    return m_filePath;
}

void Document::GetPageSize(int pageIndex, double* width, double* height, int rotation) {
    if (pageIndex < 0 || pageIndex >= m_pageCount) {
        if (width) *width = 612.0;
        if (height) *height = 792.0;
        return;
    }

    double w = m_pageSizes[pageIndex].first;
    double h = m_pageSizes[pageIndex].second;

    int rot = (rotation / 90) % 4;
    if (rot == 1 || rot == 3) {
        std::swap(w, h);
    }

    if (width) *width = w;
    if (height) *height = h;
}

int Document::GetPageRotation(int pageIndex) const {
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) return 0;
    // Query default document rotation if any
    return 0;
}

FPDF_PAGE Document::GetOrLoadPage(int pageIndex) {
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) return nullptr;

    auto it = m_openPages.find(pageIndex);
    if (it != m_openPages.end()) {
        return it->second;
    }

    FPDF_PAGE page = FPDF_LoadPage(m_doc, pageIndex);
    if (page) {
        m_openPages[pageIndex] = page;
    }
    return page;
}

FPDF_TEXTPAGE Document::GetOrLoadTextPage(int pageIndex) {
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) return nullptr;

    auto it = m_openTextPages.find(pageIndex);
    if (it != m_openTextPages.end()) {
        return it->second;
    }

    FPDF_PAGE page = GetOrLoadPage(pageIndex);
    if (!page) return nullptr;

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (textPage) {
        m_openTextPages[pageIndex] = textPage;
    }
    return textPage;
}

void Document::CloseUnusedPages() {
    // Keep max 10 open pages in memory
    if (m_openPages.size() > 10) {
        auto it = m_openPages.begin();
        int pageIndex = it->first;
        if (m_openTextPages.count(pageIndex)) {
            FPDFText_ClosePage(m_openTextPages[pageIndex]);
            m_openTextPages.erase(pageIndex);
        }
        FPDF_ClosePage(it->second);
        m_openPages.erase(it);
    }
}

HBITMAP Document::GetPageBitmap(int pageIndex, double zoom, int rotation, int* outWidth, int* outHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) return nullptr;

    double ptWidth = 0, ptHeight = 0;
    GetPageSize(pageIndex, &ptWidth, &ptHeight, rotation);

    int pixelWidth = static_cast<int>(std::round(ptWidth * zoom));
    int pixelHeight = static_cast<int>(std::round(ptHeight * zoom));
    if (pixelWidth <= 0) pixelWidth = 1;
    if (pixelHeight <= 0) pixelHeight = 1;

    if (outWidth) *outWidth = pixelWidth;
    if (outHeight) *outHeight = pixelHeight;

    // Check cache
    for (auto& item : m_renderedCache) {
        if (item.pageIndex == pageIndex &&
            std::abs(item.zoom - zoom) < 0.001 &&
            item.rotation == rotation &&
            item.width == pixelWidth &&
            item.height == pixelHeight &&
            item.hBitmap != nullptr) {
            item.lastAccess = std::chrono::steady_clock::now();
            return item.hBitmap;
        }
    }

    // Need to render
    FPDF_PAGE page = GetOrLoadPage(pageIndex);
    if (!page) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pixelWidth;
    bmi.bmiHeader.biHeight = -pixelHeight; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBmp || !pBits) {
        return nullptr;
    }

    int stride = pixelWidth * 4;
    FPDF_BITMAP fpdfBmp = FPDFBitmap_CreateEx(pixelWidth, pixelHeight, FPDFBitmap_BGRA, pBits, stride);
    if (!fpdfBmp) {
        DeleteObject(hBmp);
        return nullptr;
    }

    // Fill background with white
    FPDFBitmap_FillRect(fpdfBmp, 0, 0, pixelWidth, pixelHeight, 0xFFFFFFFF);

    int rotateFlag = (rotation / 90) % 4;
    FPDF_RenderPageBitmap(fpdfBmp, page, 0, 0, pixelWidth, pixelHeight, rotateFlag, FPDF_ANNOT);
    FPDFBitmap_Destroy(fpdfBmp);

    // Cache entry
    RenderedPage entry;
    entry.pageIndex = pageIndex;
    entry.zoom = zoom;
    entry.rotation = rotation;
    entry.width = pixelWidth;
    entry.height = pixelHeight;
    entry.hBitmap = hBmp;
    entry.pixelData = pBits;
    entry.lastAccess = std::chrono::steady_clock::now();

    m_renderedCache.push_back(entry);
    PruneCache(8);
    CloseUnusedPages();

    return hBmp;
}

HBITMAP Document::GetThumbnailBitmap(int pageIndex, int maxWidth, int maxHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_doc || pageIndex < 0 || pageIndex >= m_pageCount) return nullptr;

    auto it = m_thumbnailCache.find(pageIndex);
    if (it != m_thumbnailCache.end()) {
        return it->second;
    }

    double ptWidth = 0, ptHeight = 0;
    GetPageSize(pageIndex, &ptWidth, &ptHeight, 0);

    double scale = std::min(static_cast<double>(maxWidth) / ptWidth, static_cast<double>(maxHeight) / ptHeight);
    int pixelWidth = static_cast<int>(std::round(ptWidth * scale));
    int pixelHeight = static_cast<int>(std::round(ptHeight * scale));
    if (pixelWidth <= 0) pixelWidth = 1;
    if (pixelHeight <= 0) pixelHeight = 1;

    FPDF_PAGE page = GetOrLoadPage(pageIndex);
    if (!page) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = pixelWidth;
    bmi.bmiHeader.biHeight = -pixelHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBmp) return nullptr;

    int stride = pixelWidth * 4;
    FPDF_BITMAP fpdfBmp = FPDFBitmap_CreateEx(pixelWidth, pixelHeight, FPDFBitmap_BGRA, pBits, stride);
    if (fpdfBmp) {
        FPDFBitmap_FillRect(fpdfBmp, 0, 0, pixelWidth, pixelHeight, 0xFFFFFFFF);
        FPDF_RenderPageBitmap(fpdfBmp, page, 0, 0, pixelWidth, pixelHeight, 0, FPDF_ANNOT);
        FPDFBitmap_Destroy(fpdfBmp);
    }

    m_thumbnailCache[pageIndex] = hBmp;
    return hBmp;
}

int Document::GetPageCharCount(int pageIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    FPDF_TEXTPAGE tp = GetOrLoadTextPage(pageIndex);
    return tp ? FPDFText_CountChars(tp) : 0;
}

std::wstring Document::GetPageText(int pageIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    FPDF_TEXTPAGE tp = GetOrLoadTextPage(pageIndex);
    if (!tp) return L"";

    int count = FPDFText_CountChars(tp);
    if (count <= 0) return L"";

    std::vector<unsigned short> buffer(count + 1, 0);
    int charsWritten = FPDFText_GetText(tp, 0, count, buffer.data());
    if (charsWritten <= 0) return L"";

    return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()), charsWritten);
}

std::wstring Document::GetSelectedText(int pageIndex, int startIndex, int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    FPDF_TEXTPAGE tp = GetOrLoadTextPage(pageIndex);
    if (!tp || count <= 0) return L"";

    int totalChars = FPDFText_CountChars(tp);
    if (startIndex < 0) startIndex = 0;
    if (startIndex >= totalChars) return L"";
    if (startIndex + count > totalChars) count = totalChars - startIndex;

    std::vector<unsigned short> buffer(count + 1, 0);
    int charsWritten = FPDFText_GetText(tp, startIndex, count, buffer.data());
    if (charsWritten <= 0) return L"";

    return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()), charsWritten);
}

int Document::GetCharIndexAtPos(int pageIndex, double pageX, double pageY, double tolerance) {
    std::lock_guard<std::mutex> lock(m_mutex);
    FPDF_TEXTPAGE tp = GetOrLoadTextPage(pageIndex);
    if (!tp) return -1;
    return FPDFText_GetCharIndexAtPos(tp, pageX, pageY, tolerance, tolerance);
}

void Document::PageToDevice(int pageIndex, double pageX, double pageY, double zoom, int rotation,
                            int offsetX, int offsetY, int* deviceX, int* deviceY) {
    double ptWidth = 0, ptHeight = 0;
    GetPageSize(pageIndex, &ptWidth, &ptHeight, rotation);

    int pixelWidth = static_cast<int>(std::round(ptWidth * zoom));
    int pixelHeight = static_cast<int>(std::round(ptHeight * zoom));
    int rotateFlag = (rotation / 90) % 4;

    FPDF_PAGE page = GetOrLoadPage(pageIndex);
    if (page) {
        int dx = 0, dy = 0;
        FPDF_PageToDevice(page, 0, 0, pixelWidth, pixelHeight, rotateFlag, pageX, pageY, &dx, &dy);
        if (deviceX) *deviceX = dx + offsetX;
        if (deviceY) *deviceY = dy + offsetY;
    } else {
        if (deviceX) *deviceX = static_cast<int>(std::round(pageX * zoom)) + offsetX;
        if (deviceY) *deviceY = static_cast<int>(std::round(pageY * zoom)) + offsetY;
    }
}

void Document::DeviceToPage(int pageIndex, int deviceX, int deviceY, double zoom, int rotation,
                            int offsetX, int offsetY, double* pageX, double* pageY) {
    double ptWidth = 0, ptHeight = 0;
    GetPageSize(pageIndex, &ptWidth, &ptHeight, rotation);

    int pixelWidth = static_cast<int>(std::round(ptWidth * zoom));
    int pixelHeight = static_cast<int>(std::round(ptHeight * zoom));
    int rotateFlag = (rotation / 90) % 4;

    FPDF_PAGE page = GetOrLoadPage(pageIndex);
    if (page) {
        double px = 0, py = 0;
        FPDF_DeviceToPage(page, 0, 0, pixelWidth, pixelHeight, rotateFlag, deviceX - offsetX, deviceY - offsetY, &px, &py);
        if (pageX) *pageX = px;
        if (pageY) *pageY = py;
    }
}

std::vector<RECT> Document::GetTextSelectionRects(int pageIndex, int startIndex, int count,
                                                 double zoom, int rotation, int offsetX, int offsetY) {
    std::vector<RECT> rects;
    std::lock_guard<std::mutex> lock(m_mutex);

    FPDF_TEXTPAGE tp = GetOrLoadTextPage(pageIndex);
    if (!tp || count <= 0) return rects;

    int totalChars = FPDFText_CountChars(tp);
    if (startIndex < 0) startIndex = 0;
    if (startIndex >= totalChars) return rects;
    if (startIndex + count > totalChars) count = totalChars - startIndex;

    int rectCount = FPDFText_CountRects(tp, startIndex, count);
    for (int i = 0; i < rectCount; ++i) {
        double left = 0, top = 0, right = 0, bottom = 0;
        if (FPDFText_GetRect(tp, i, &left, &top, &right, &bottom)) {
            int devLeft = 0, devTop = 0, devRight = 0, devBottom = 0;
            PageToDevice(pageIndex, left, top, zoom, rotation, offsetX, offsetY, &devLeft, &devTop);
            PageToDevice(pageIndex, right, bottom, zoom, rotation, offsetX, offsetY, &devRight, &devBottom);

            RECT r;
            r.left = std::min(devLeft, devRight);
            r.right = std::max(devLeft, devRight);
            r.top = std::min(devTop, devBottom);
            r.bottom = std::max(devTop, devBottom);
            rects.push_back(r);
        }
    }

    return rects;
}

std::vector<SearchMatch> Document::Search(const std::wstring& query, bool matchCase, bool wholeWord) {
    std::vector<SearchMatch> matches;
    if (query.empty() || !m_doc) return matches;

    std::lock_guard<std::mutex> lock(m_mutex);

    unsigned long flags = 0;
    if (matchCase) flags |= FPDF_MATCHCASE;
    if (wholeWord) flags |= FPDF_MATCHWHOLEWORD;

    for (int p = 0; p < m_pageCount; ++p) {
        FPDF_TEXTPAGE tp = GetOrLoadTextPage(p);
        if (!tp) continue;

        FPDF_SCHHANDLE sch = FPDFText_FindStart(tp, reinterpret_cast<const unsigned short*>(query.c_str()), flags, 0);
        if (!sch) continue;

        while (FPDFText_FindNext(sch)) {
            SearchMatch m;
            m.pageIndex = p;
            m.charIndex = FPDFText_GetSchResultIndex(sch);
            m.charCount = FPDFText_GetSchCount(sch);
            matches.push_back(m);
        }

        FPDFText_FindClose(sch);
    }

    return matches;
}

void Document::PruneCache(size_t maxCachedPages) {
    while (m_renderedCache.size() > maxCachedPages) {
        // Find oldest accessed
        auto oldestIt = m_renderedCache.begin();
        for (auto it = m_renderedCache.begin(); it != m_renderedCache.end(); ++it) {
            if (it->lastAccess < oldestIt->lastAccess) {
                oldestIt = it;
            }
        }
        if (oldestIt != m_renderedCache.end()) {
            if (oldestIt->hBitmap) {
                DeleteObject(oldestIt->hBitmap);
            }
            m_renderedCache.erase(oldestIt);
        }
    }
}

void Document::ClearCache() {
    for (auto& item : m_renderedCache) {
        if (item.hBitmap) {
            DeleteObject(item.hBitmap);
        }
    }
    m_renderedCache.clear();

    for (auto& pair : m_thumbnailCache) {
        if (pair.second) {
            DeleteObject(pair.second);
        }
    }
    m_thumbnailCache.clear();
}

